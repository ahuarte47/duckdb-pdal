#include "pdal_pipeline_functions.hpp"
#include "function_builder.hpp"

// DuckDB
#include "duckdb/main/database.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/parsed_data/create_copy_function_info.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

// PDAL
#include <pdal/PipelineManager.hpp>
#include <pdal/PluginManager.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/BufferReader.hpp>
#include <pdal/util/FileUtils.hpp>
#include "pdal_utils.hpp"

namespace duckdb {

namespace {

//======================================================================================================================
// PDAL_Pipeline
//======================================================================================================================

struct PDAL_Pipeline {

	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData final : TableFunctionData {
		string file_name;
		std::unique_ptr<pdal::PipelineManager> pipeline;
		uint64_t point_count = 0;
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {

		auto file_name = StringValue::Get(input.inputs[0]);
		auto the_pipeline = StringValue::Get(input.inputs[1]);
		auto &fs = FileSystem::GetFileSystem(context);

		if (!fs.IsRemoteFile(file_name) && !pdal::FileUtils::fileExists(file_name)) {
			throw InvalidInputException("File not found: %s", file_name);
		}

		std::string driver = pdal::StageFactory::inferReaderDriver(file_name);
		if (driver.length() == 0) {
			throw InvalidInputException("File format not supported: %s", file_name);
		}

		// Create the PDAL Pipeline Manager and read the pipeline definition (inline JSON or file).

		std::unique_ptr<pdal::PipelineManager> pipeline = std::make_unique<pdal::PipelineManager>();

		if (StringUtil::StartsWith(the_pipeline, "[") && StringUtil::EndsWith(the_pipeline, "]")) {
			std::stringstream ssin(the_pipeline);
			pipeline->readPipeline(ssin);
		} else {
			if (!pdal::FileUtils::fileExists(the_pipeline)) {
				throw InvalidInputException("Pipeline file not found: %s", the_pipeline);
			}
			pipeline->readPipeline(the_pipeline);
		}

		std::vector<pdal::Stage *> roots = pipeline->roots();
		if (roots.size() != 1) {
			throw InvalidInputException("Can't process pipelines without an unique root.");
		}

		// Create the PDAL reader based on file extension and set reader options.

		pdal::Options reader_options;
		reader_options.add("filename", file_name);

		auto options_param = input.named_parameters.find("options");
		if (options_param != input.named_parameters.end()) {
			const std::vector<duckdb::Value> &children = MapValue::GetChildren(options_param->second);
			PdalUtils::ParseOptions(children, reader_options);
		}

		pdal::Stage *reader = &pipeline->makeReader(file_name, driver, reader_options);
		roots[0]->setInput(*reader);

		// Run the PDAL pipeline from the JSON file.

		pdal::point_count_t point_count = pipeline->execute();
		pdal::PointViewPtr view = *(pipeline->views().begin());

		pdal::PointLayoutPtr layout = view->layout();
		PdalUtils::ExtractLayout(layout, names, return_types);

		// Create and return bind data.

		auto bind_data = make_uniq<BindData>();
		bind_data->file_name = file_name;
		bind_data->pipeline = std::move(pipeline);
		bind_data->point_count = point_count;

		return std::move(bind_data);
	};

	//------------------------------------------------------------------------------------------------------------------
	// Init Global
	//------------------------------------------------------------------------------------------------------------------

	struct GlobalState final : GlobalTableFunctionState {
		pdal::PointId point_idx;
		explicit GlobalState(ClientContext &context) : point_idx(0) {
		}
	};

	static unique_ptr<GlobalTableFunctionState> InitGlobal(ClientContext &context, TableFunctionInitInput &input) {
		auto result = make_uniq<GlobalState>(context);
		return std::move(result);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Local
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Execute
	//------------------------------------------------------------------------------------------------------------------

	static void Execute(ClientContext &context, TableFunctionInput &input, DataChunk &output) {
		auto &bind_data = (BindData &)*input.bind_data;
		auto &gstate = input.global_state->Cast<GlobalState>();

		// Calculate how many record we can fit in the output
		const auto output_size = std::min<idx_t>(STANDARD_VECTOR_SIZE, bind_data.point_count - gstate.point_idx);
		const auto point_start = gstate.point_idx;

		// Set the cardinality of the output
		if (output_size == 0) {
			output.SetCardinality(0);
			return;
		}
		output.SetCardinality(output_size);

		// Load current subset of points into the output
		pdal::PointViewPtr view = *(bind_data.pipeline->views().begin());
		PdalUtils::ExtractDataChunk(view, point_start, output_size, output);

		// Update the point index
		gstate.point_idx += output_size;
	};

	//------------------------------------------------------------------------------------------------------------------
	// Cardinality
	//------------------------------------------------------------------------------------------------------------------

	static unique_ptr<NodeStatistics> Cardinality(ClientContext &context, const FunctionData *data) {

		auto &bind_data = data->Cast<BindData>();
		auto result = make_uniq<NodeStatistics>();

		// This is the maximum number of points in a single file
		result->has_max_cardinality = true;
		result->max_cardinality = bind_data.point_count;

		return result;
	}

	//------------------------------------------------------------------------------------------------------------------
	// Replacement Scan
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Documentation
	//------------------------------------------------------------------------------------------------------------------

	static constexpr auto DESCRIPTION = R"(
		Read and import a variety of point cloud data file formats using the PDAL library,
		applying also a custom processing pipeline to the data.

		The pipeline can be provided either as a JSON file or as an inline JSON string. If the second parameter value
		starts with "[" and ends with "]", it represents an inline JSON, otherwise it is a file path.
	)";

	static constexpr auto EXAMPLE = R"(
		SELECT * FROM PDAL_Pipeline('path/to/your/filename.las', 'path/to/your/pipeline.json');
		SELECT * FROM PDAL_Pipeline('path/to/your/filename.las', '[ {"type": "filters.tail", "count": 100} ]');
	)";

	//------------------------------------------------------------------------------------------------------------------
	// Register
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "pdal");
		tags.insert("category", "table");

		TableFunction func("PDAL_Pipeline", {LogicalType::VARCHAR, LogicalType::VARCHAR}, Execute, Bind, InitGlobal);

		func.cardinality = Cardinality;
		func.named_parameters["options"] = LogicalType::MAP(LogicalType::VARCHAR, LogicalType::VARCHAR);

		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);
	}
};

} // namespace

// #####################################################################################################################
// Register Pipeline Functions
// #####################################################################################################################

void PdalPipelineFunctions::Register(ExtensionLoader &loader) {

	PDAL_Pipeline::Register(loader);
}

} // namespace duckdb
