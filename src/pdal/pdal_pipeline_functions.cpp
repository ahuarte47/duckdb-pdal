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
		std::vector<column_t> column_ids;
		pdal::PointId point_idx;
		explicit GlobalState(ClientContext &context, const std::vector<column_t> &column_ids)
		    : column_ids(std::move(column_ids)), point_idx(0) {
		}
	};

	static unique_ptr<GlobalTableFunctionState> InitGlobal(ClientContext &context, TableFunctionInitInput &input) {
		std::vector<column_t> column_ids;
		std::copy(input.column_ids.begin(), input.column_ids.end(), std::back_inserter(column_ids));

		auto result = make_uniq<GlobalState>(context, column_ids);
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
		PdalUtils::ExtractDataChunk(view, point_start, output_size, gstate.column_ids, output);

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

		// Enable projection pushdown - allows DuckDB to tell us which columns are needed
		// The column_ids will be passed to InitGlobal via TableFunctionInitInput
		func.projection_pushdown = true;

		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);
	}
};

//======================================================================================================================
// PDAL_PipelineTable
//======================================================================================================================

struct PDAL_PipelineTable {
	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData : public TableFunctionData {
		vector<string> field_names;
		vector<LogicalType> field_types;
		std::vector<idx_t> field_indexes;

		std::unique_ptr<pdal::PipelineManager> pipeline;
		std::unique_ptr<pdal::BufferReader> reader;
		std::unique_ptr<pdal::PointTable> table;
		std::shared_ptr<pdal::PointView> view;

		BindData(vector<string> field_names, vector<LogicalType> field_types)
		    : field_names(std::move(field_names)), field_types(std::move(field_types)) {
		}
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {
		auto input_table = input.inputs[0];
		auto the_pipeline = StringValue::Get(input.inputs[1]);

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

		// Create the PDAL reader and prepare the target table.

		std::unique_ptr<pdal::BufferReader> reader = std::make_unique<pdal::BufferReader>();
		if (!reader) {
			throw InvalidInputException("Driver 'readers.buffer' was not found in PDAL installation");
		}

		std::unique_ptr<pdal::PointTable> table = std::make_unique<pdal::PointTable>();
		std::shared_ptr<pdal::PointView> view = std::make_shared<pdal::PointView>(*table);

		reader->addView(view);
		roots[0]->setInput(*reader);

		// Fill the layout by mapping SQL types to PDAL types.

		pdal::PointLayoutPtr layout = table->layout();
		auto &logger = Logger::Get(context);

		std::vector<idx_t> field_indexes =
		    PdalUtils::FillLayout(layout, input.input_table_names, input.input_table_types, logger);

		pdal::PointLayoutPtr pipeline_layout = pipeline->pointTable().layout();
		PdalUtils::CopyLayout(layout, pipeline_layout);

		for (auto it = field_indexes.begin(); it != field_indexes.end(); it++) {
			return_types.push_back(input.input_table_types[*it]);
			names.emplace_back(input.input_table_names[*it]);
		}

		// Return bind data.

		auto bind_data = make_uniq<BindData>(names, return_types);
		bind_data->pipeline = std::move(pipeline);
		bind_data->reader = std::move(reader);
		bind_data->table = std::move(table);
		bind_data->view = std::move(view);
		bind_data->field_indexes = std::move(field_indexes);

		return std::move(bind_data);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Global
	//------------------------------------------------------------------------------------------------------------------

	struct GlobalState final : GlobalTableFunctionState {
		std::vector<column_t> column_ids;
		pdal::PointId point_idx;
		uint64_t input_point_count = 0;
		uint64_t point_count = 0;
		bool initialized = false;

		explicit GlobalState(ClientContext &context, const std::vector<column_t> &column_ids)
		    : column_ids(std::move(column_ids)), point_idx(0) {
		}
	};

	static unique_ptr<GlobalTableFunctionState> InitGlobal(ClientContext &context, TableFunctionInitInput &input) {
		std::vector<column_t> column_ids;
		std::copy(input.column_ids.begin(), input.column_ids.end(), std::back_inserter(column_ids));

		auto result = make_uniq<GlobalState>(context, column_ids);
		return std::move(result);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Local
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Execute Function
	//------------------------------------------------------------------------------------------------------------------

	static OperatorResultType Function(ExecutionContext &context, TableFunctionInput &data_p, DataChunk &input,
	                                   DataChunk &output) {
		auto &bind_data = data_p.bind_data->Cast<BindData>();
		auto &gstate = data_p.global_state->Cast<GlobalState>();

		pdal::PointView *view = bind_data.view.get();
		const std::vector<idx_t> &field_indexes = bind_data.field_indexes;

		// Write the points into the output
		input.Flatten();
		PdalUtils::FillDataChunk(view, input, field_indexes);

		output.SetCardinality(0);
		gstate.input_point_count += input.size();

		return OperatorResultType::NEED_MORE_INPUT;
	}

	//------------------------------------------------------------------------------------------------------------------
	// Finalize
	//------------------------------------------------------------------------------------------------------------------

	static OperatorFinalizeResultType Finalize(ExecutionContext &context, TableFunctionInput &data_p,
	                                           DataChunk &output) {
		auto &bind_data = data_p.bind_data->Cast<BindData>();
		auto &gstate = data_p.global_state->Cast<GlobalState>();

		pdal::PipelineManager *pipeline = bind_data.pipeline.get();

		// Execute the PDAL pipeline?
		if (!gstate.initialized) {
			gstate.point_count = pipeline->execute();
			gstate.initialized = true;
		}

		// Calculate how many record we can fit in the output
		const auto output_size = std::min<idx_t>(STANDARD_VECTOR_SIZE, gstate.point_count - gstate.point_idx);
		const auto point_start = gstate.point_idx;

		// Set the cardinality of the output
		if (output_size == 0) {
			output.SetCardinality(0);
			return OperatorFinalizeResultType::FINISHED;
		}
		output.SetCardinality(output_size);

		// Load current subset of points into the output.
		pdal::PointViewPtr view = *(bind_data.pipeline->views().begin());
		PdalUtils::ExtractDataChunk(view, point_start, output_size, gstate.column_ids, output);

		// Update the point index
		gstate.point_idx += output_size;

		return OperatorFinalizeResultType::HAVE_MORE_OUTPUT;
	}

	//------------------------------------------------------------------------------------------------------------------
	// Documentation
	//------------------------------------------------------------------------------------------------------------------

	static constexpr auto DESCRIPTION = R"(
		Apply a custom processing pipeline to the input table. It is supposed that the input table contains columns
		compatible with PDAL point clouds.

		The pipeline can be provided either as a JSON file or as an inline JSON string. If the second parameter value
		starts with "[" and ends with "]", it represents an inline JSON, otherwise it is a file path.
	)";

	static constexpr auto EXAMPLE = R"(
		SELECT * FROM PDAL_PipelineTable((SELECT * FROM PDAL_Read('path/to/your/filename.las')), 'path/to/your/pipeline.json');
		SELECT * FROM PDAL_PipelineTable((SELECT * FROM PDAL_Read('path/to/your/filename.las')), '[ {"type": "filters.tail", "count": 100} ]');
	)";

	//------------------------------------------------------------------------------------------------------------------
	// Register
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {
		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "pdal");
		tags.insert("category", "table");

		TableFunction func("PDAL_PipelineTable", {LogicalType::TABLE, LogicalType::VARCHAR}, nullptr, Bind, InitGlobal);

		func.in_out_function = Function;
		func.in_out_function_final = Finalize;

		// Enable projection pushdown - allows DuckDB to tell us which columns are needed
		// The column_ids will be passed to InitGlobal via TableFunctionInitInput
		func.projection_pushdown = true;

		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);
	}
};

} // namespace

// #####################################################################################################################
// Register Pipeline Functions
// #####################################################################################################################

void PdalPipelineFunctions::Register(ExtensionLoader &loader) {
	PDAL_Pipeline::Register(loader);
	PDAL_PipelineTable::Register(loader);
}

} // namespace duckdb
