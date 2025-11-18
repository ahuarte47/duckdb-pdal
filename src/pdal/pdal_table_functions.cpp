#include "pdal_table_functions.hpp"
#include "function_builder.hpp"

// DuckDB
#include "duckdb/main/database.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/parsed_data/create_copy_function_info.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

// PDAL
#include <pdal/PluginManager.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/LasHeader.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/util/FileUtils.hpp>

namespace duckdb {

namespace {

//======================================================================================================================
// PDAL Types & Utils
//======================================================================================================================

struct PDAL_Utils {

	// Parse a DuckDB struct array of key-value pairs into a PDAL Options object.
	static void ParseOptions(const std::vector<duckdb::Value> &input, pdal::Options &options) {

		for (const auto &kv_child : input) {
			auto kv_pair = StructValue::GetChildren(kv_child);
			if (kv_pair.size() != 2) {
				throw InvalidInputException("Invalid input passed to options parameter");
			}
			auto key = StringValue::Get(kv_pair[0]);
			auto val = StringValue::Get(kv_pair[1]);
			options.add(key, val);
		}
	}

	// Extract the PDAL PointLayout into DuckDB return types and names.
	static void ExtractLayout(pdal::PointLayoutPtr layout, vector<LogicalType> &return_types, vector<string> &names) {

		for (const auto &dimId : layout->dims()) {
			std::string name = layout->dimName(dimId);
			const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
			pdal::Dimension::Type t = detail->type();

			switch (t) {
			case pdal::Dimension::Type::Float:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::FLOAT);
				break;
			case pdal::Dimension::Type::Double:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::DOUBLE);
				break;

			case pdal::Dimension::Type::Signed8:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::TINYINT);
				break;
			case pdal::Dimension::Type::Signed16:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::SMALLINT);
				break;
			case pdal::Dimension::Type::Signed32:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::INTEGER);
				break;
			case pdal::Dimension::Type::Signed64:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::BIGINT);
				break;

			case pdal::Dimension::Type::Unsigned8:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::UTINYINT);
				break;
			case pdal::Dimension::Type::Unsigned16:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::USMALLINT);
				break;
			case pdal::Dimension::Type::Unsigned32:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::UINTEGER);
				break;
			case pdal::Dimension::Type::Unsigned64:
				names.emplace_back(name);
				return_types.emplace_back(LogicalTypeId::UBIGINT);
				break;

			default:
				throw InvalidInputException("Field type %d not supported", t);
			}
		}
	}

	// Write a chunk of points from a PDAL PointView into a DuckDB DataChunk.
	static void WriteOutputChunk(pdal::PointViewPtr view, idx_t record_start, std::size_t output_size,
	                             DataChunk &output) {

		pdal::PointLayoutPtr layout = view->layout();
		pdal::PointRef point(*view, record_start);

		for (idx_t row_idx = 0, point_idx = record_start; row_idx < output_size; row_idx++, point_idx++) {

			point.setPointId(point_idx);
			idx_t col_idx = 0;

			for (const auto &dimId : layout->dims()) {
				const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
				pdal::Dimension::Type t = detail->type();

				switch (t) {
				case pdal::Dimension::Type::Float: {
					float value = point.getFieldAs<float>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::FLOAT(value));
					break;
				}
				case pdal::Dimension::Type::Double: {
					double value = point.getFieldAs<double>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::DOUBLE(value));
					break;
				}
				case pdal::Dimension::Type::Signed8: {
					int8_t value = point.getFieldAs<int8_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::TINYINT(value));
					break;
				}
				case pdal::Dimension::Type::Signed16: {
					int16_t value = point.getFieldAs<int16_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::SMALLINT(value));
					break;
				}
				case pdal::Dimension::Type::Signed32: {
					int32_t value = point.getFieldAs<int32_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::INTEGER(value));
					break;
				}
				case pdal::Dimension::Type::Signed64: {
					int64_t value = point.getFieldAs<int64_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::BIGINT(value));
					break;
				}
				case pdal::Dimension::Type::Unsigned8: {
					uint8_t value = point.getFieldAs<uint8_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::UTINYINT(value));
					break;
				}
				case pdal::Dimension::Type::Unsigned16: {
					uint16_t value = point.getFieldAs<uint16_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::USMALLINT(value));
					break;
				}
				case pdal::Dimension::Type::Unsigned32: {
					uint32_t value = point.getFieldAs<uint32_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::UINTEGER(value));
					break;
				}
				case pdal::Dimension::Type::Unsigned64: {
					uint64_t value = point.getFieldAs<uint64_t>(dimId);
					output.SetValue(col_idx, row_idx, duckdb::Value::UBIGINT(value));
					break;
				}
				default:
					throw InvalidInputException("Field type %d not supported", t);
				}
				col_idx++;
			}
		}
	}
};

//======================================================================================================================
// PDAL_Drivers
//======================================================================================================================

struct PDAL_Drivers {

	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData final : TableFunctionData {
		idx_t driver_count;
		explicit BindData(const idx_t driver_count_p) : driver_count(driver_count_p) {
		}
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {

		names.emplace_back("name");
		return_types.push_back(LogicalType::VARCHAR);
		names.emplace_back("description");
		return_types.push_back(LogicalType::VARCHAR);

		pdal::PluginManager<pdal::Stage>::loadAll();
		std::vector<std::string> pdal_stages = pdal::PluginManager<pdal::Stage>::names();

		return make_uniq_base<FunctionData, BindData>(pdal_stages.size());
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init
	//------------------------------------------------------------------------------------------------------------------

	struct State final : GlobalTableFunctionState {
		idx_t current_idx;
		explicit State() : current_idx(0) {
		}
	};

	static unique_ptr<GlobalTableFunctionState> Init(ClientContext &context, TableFunctionInitInput &input) {
		return make_uniq_base<GlobalTableFunctionState, State>();
	}

	//------------------------------------------------------------------------------------------------------------------
	// Execute
	//------------------------------------------------------------------------------------------------------------------

	static void Execute(ClientContext &context, TableFunctionInput &input, DataChunk &output) {
		auto &state = input.global_state->Cast<State>();
		auto &bind_data = input.bind_data->Cast<BindData>();

		idx_t count = 0;
		auto next_idx = MinValue<idx_t>(state.current_idx + STANDARD_VECTOR_SIZE, bind_data.driver_count);

		std::vector<std::string> pdal_stages = pdal::PluginManager<pdal::Stage>::names();

		for (; state.current_idx < next_idx; state.current_idx++) {
			std::string name = pdal_stages[state.current_idx];
			std::string description = pdal::PluginManager<pdal::Stage>::description(name);

			output.data[0].SetValue(count, name);
			output.data[1].SetValue(count, description);
			count++;
		}
		output.SetCardinality(count);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Documentation
	//------------------------------------------------------------------------------------------------------------------

	static constexpr auto DESCRIPTION = R"(
		Returns the list of supported stage types of a PDAL Pipeline.

		The stages of a PDAL Pipeline are divided into Readers, Filters and Writers: https://pdal.io/en/stable/stages/stages.html
	)";

	static constexpr auto EXAMPLE = R"(
		SELECT * FROM PDAL_Drivers();

		┌─────────────────────────────┬─────────────────────────────────────────────────────────────────────────────────┐
		│            name             │                                     description                                 │
		│           varchar           │                                       varchar                                   │
		├─────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
		│ filters.approximatecoplanar │ Estimates the planarity of a neighborhood of points using eigenvalues.          │
		│ filters.assign              │ Assign values for a dimension range to a specified value.                       │
		│ filters.chipper             │ Organize points into spatially contiguous, squarish, and non-overlapping chips. │
		│ filters.cluster             │ Extract and label clusters using Euclidean distance.                            │
		│      ·                      │      ·                                                                          │
		│      ·                      │      ·                                                                          │
		│      ·                      │      ·                                                                          │
		│ readers.slpk                │ SLPK Reader                                                                     │
		│ readers.smrmsg              │ SBET smrmsg Reader                                                              │
		│ readers.stac                │ STAC Reader                                                                     │
		│ readers.terrasolid          │ TerraSolid Reader                                                               │
		│ writers.copc                │ COPC Writer                                                                     │
		│ writers.gdal                │ Write a point cloud as a GDAL raster.                                           │
		│ writers.las                 │ ASPRS LAS 1.0 - 1.4 writer                                                      │
		│ writers.text                │ Text Writer                                                                     │
		├─────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────┤
		│ 119 rows                                                                                            2 columns │
		└───────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
	)";

	//------------------------------------------------------------------------------------------------------------------
	// Register
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "pdal");
		tags.insert("category", "table");

		const TableFunction func("PDAL_Drivers", {}, Execute, Bind, Init);
		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);
	}
};

//======================================================================================================================
// PDAL_Read
//======================================================================================================================

struct PDAL_Read {

	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData final : TableFunctionData {
		string file_name;
		std::unique_ptr<pdal::StageFactory> stage_factory;
		std::unique_ptr<pdal::PointTable> table;
		pdal::PointViewSet views;
		uint64_t point_count = 0;
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {

		auto file_name = StringValue::Get(input.inputs[0]);

		if (!pdal::FileUtils::fileExists(file_name)) {
			throw InvalidInputException("File not found: %s", file_name);
		}

		std::string driver = pdal::StageFactory::inferReaderDriver(file_name);
		if (driver.length() == 0) {
			throw InvalidInputException("File format not supported: %s", file_name);
		}

		// Create the PDAL reader based on file extension and set reader options.

		std::unique_ptr<pdal::StageFactory> stage_factory = std::make_unique<pdal::StageFactory>();

		pdal::Stage *reader = stage_factory->createStage(driver);
		if (!reader) {
			throw InvalidInputException("Driver not found for file: %s", file_name);
		}

		pdal::Options reader_options;
		reader_options.add("filename", file_name);

		auto options_param = input.named_parameters.find("options");
		if (options_param != input.named_parameters.end()) {
			const std::vector<duckdb::Value> &children = MapValue::GetChildren(options_param->second);
			PDAL_Utils::ParseOptions(children, reader_options);
		}

		reader->setOptions(reader_options);

		// Make the PDAL PointTable where layout is stored, and set the output schema.

		std::unique_ptr<pdal::PointTable> table = std::make_unique<pdal::PointTable>();
		reader->prepare(*table);

		pdal::PointLayoutPtr layout = table->layout();
		PDAL_Utils::ExtractLayout(layout, return_types, names);

		// Load the point data into a PointViewSet.

		pdal::PointViewSet views = reader->execute(*table);

		// Create and return bind data.

		auto result = make_uniq<BindData>();
		result->file_name = file_name;
		result->stage_factory = std::move(stage_factory);
		result->table = std::move(table);
		result->views = std::move(views);
		result->point_count = reader->preview().m_pointCount;

		return std::move(result);
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
		const auto record_start = gstate.point_idx;

		if (output_size == 0) {
			output.SetCardinality(0);
			return;
		}

		// Load current subset of points into the output.
		pdal::PointViewPtr view = *(bind_data.views.begin());
		PDAL_Utils::WriteOutputChunk(view, record_start, output_size, output);

		// Update the point index
		gstate.point_idx += output_size;

		// Set the cardinality of the output
		output.SetCardinality(output_size);
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

	static unique_ptr<TableRef> ReplacementScan(ClientContext &, ReplacementScanInput &input,
	                                            optional_ptr<ReplacementScanData>) {
		auto &table_name = input.table_name;
		auto lower_name = StringUtil::Lower(table_name);

		// Check if the file name ends with some common LiDAR file extensions
		if (StringUtil::EndsWith(lower_name, ".las") || StringUtil::EndsWith(lower_name, ".laz")) {

			auto table_function = make_uniq<TableFunctionRef>();
			vector<unique_ptr<ParsedExpression>> children;
			children.push_back(make_uniq<ConstantExpression>(Value(table_name)));
			table_function->function = make_uniq<FunctionExpression>("PDAL_Read", std::move(children));
			return std::move(table_function);
		}
		// else not something we can replace
		return nullptr;
	}

	//------------------------------------------------------------------------------------------------------------------
	// Documentation
	//------------------------------------------------------------------------------------------------------------------

	static constexpr auto DESCRIPTION = R"(
		Read and import a variety of point cloud data file formats using the PDAL library.
	)";

	static constexpr auto EXAMPLE = R"(
		SELECT * FROM PDAL_Read('path/to/your/filename.las') LIMIT 10;

		┌───────────┬───────────┬────────┐
		│     X     │     Y     │   Z    │
		│   double  │   double  │ double │
		├───────────┼───────────┼────────┤
		│ 637177.98 │ 849393.95 │ 411.19 │
		│ 637177.30 │ 849396.95 │ 411.25 │
		│ 637176.34 │ 849400.84 │ 411.01 │
		│ 637175.45 │ 849404.62 │ 410.99 │
		│ 637174.33 │ 849407.37 │ 411.38 │
		└───────────┴───────────┴────────┘

		SELECT * FROM PDAL_Read('path/to/your/filename.las', options => MAP {'start': 10});

		Optional Options parameter can be used to pass reader-specific options as key-value pairs.
		For example, for the LAS/LAZ reader, the options are documented at https://pdal.io/en/stable/stages/readers.las.html#options
	)";

	//------------------------------------------------------------------------------------------------------------------
	// Register
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "pdal");
		tags.insert("category", "table");

		TableFunction func("PDAL_Read", {LogicalType::VARCHAR}, Execute, Bind, InitGlobal);

		func.cardinality = Cardinality;
		func.named_parameters["options"] = LogicalType::MAP(LogicalType::VARCHAR, LogicalType::VARCHAR);

		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);

		// Replacement scan
		auto &db = loader.GetDatabaseInstance();
		auto &config = DBConfig::GetConfig(db);
		config.replacement_scans.emplace_back(ReplacementScan);
	}
};

} // namespace

// ######################################################################################################################
//  Register Table Functions
// ######################################################################################################################

void PdalTableFunctions::Register(ExtensionLoader &loader) {

	PDAL_Drivers::Register(loader);
	PDAL_Read::Register(loader);
}

} // namespace duckdb
