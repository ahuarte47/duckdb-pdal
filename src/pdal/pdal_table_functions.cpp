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
#include <pdal/PipelineManager.hpp>
#include <pdal/PluginManager.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/BufferReader.hpp>
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
	static void ExtractLayout(const pdal::PointLayoutPtr layout, vector<LogicalType> &return_types,
	                          vector<string> &names) {

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

	// Fill a PDAL PointLayout by mapping DuckDB SQL types to PDAL types.
	static std::vector<idx_t> FillLayout(pdal::PointLayoutPtr layout, const vector<LogicalType> &sql_types,
	                                     const vector<string> &names, Logger &logger) {

		if (sql_types.size() != names.size()) {
			throw InvalidInputException("SQL types and names size mismatch");
		}

		std::vector<idx_t> field_indexes;

		for (idx_t i = 0; i < sql_types.size(); i++) {
			const auto &sql_type = sql_types[i];
			const auto &name = names[i];

			switch (sql_type.id()) {
			case LogicalTypeId::FLOAT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Float);
				break;
			case LogicalTypeId::DOUBLE:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Double);
				break;

			case LogicalTypeId::TINYINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed8);
				break;
			case LogicalTypeId::SMALLINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed16);
				break;
			case LogicalTypeId::INTEGER:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed32);
				break;
			case LogicalTypeId::BIGINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed64);
				break;

			case LogicalTypeId::UTINYINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned8);
				break;
			case LogicalTypeId::USMALLINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned16);
				break;
			case LogicalTypeId::UINTEGER:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned32);
				break;
			case LogicalTypeId::UBIGINT:
				layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned64);
				break;

			default:
				logger.WriteLog("pdal", LogLevel::LOG_WARN, "Field type '%s' not supported, skipping dimension '%s'.",
				                sql_type.ToString().c_str(), name.c_str());
				continue;
			}
			field_indexes.push_back(i);
		}
		return field_indexes;
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
		names.emplace_back("category");
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
			output.data[1].SetValue(count, StringUtil::Replace(description, "\n", ""));

			std::size_t pos = name.find('.');

			if (pos == std::string::npos) {
				output.data[2].SetValue(count, "unknown");
			} else {
				output.data[2].SetValue(count, name.substr(0, pos));
			}
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
		SELECT name, description FROM PDAL_Drivers();

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
// PDAL_Info
//======================================================================================================================

struct PDAL_Info {

// Define PDAL dimension type for the "dimensions" field.
#ifndef DEBUG
	static LogicalType PDAL_DIMENSION_TYPE() {
		return LogicalType::STRUCT({{"name", LogicalType::VARCHAR}, {"type", LogicalType::VARCHAR}});
	}
#endif

	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData final : TableFunctionData {
		vector<OpenFileInfo> files;
		explicit BindData(vector<OpenFileInfo> files_p) : files(std::move(files_p)) {
		}
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {

		names.emplace_back("file_name");
		return_types.push_back(LogicalType::VARCHAR);

		// General Point Cloud fields (QuickInfo)

		names.emplace_back("point_count");
		return_types.push_back(LogicalType::UBIGINT);

		names.emplace_back("min_x");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("min_y");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("min_z");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("max_x");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("max_y");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("max_z");
		return_types.push_back(LogicalType::DOUBLE);

		names.emplace_back("srs_wkt");
		return_types.push_back(LogicalType::VARCHAR);

		// LAS/LAZ Header fields

		names.emplace_back("extra_header");
		return_types.push_back(LogicalType::BOOLEAN);
		names.emplace_back("compressed");
		return_types.push_back(LogicalType::BOOLEAN);

		names.emplace_back("file_signature");
		return_types.push_back(LogicalType::VARCHAR);
		names.emplace_back("file_source_id");
		return_types.push_back(LogicalType::USMALLINT);
		names.emplace_back("global_encoding");
		return_types.push_back(LogicalType::USMALLINT);
		names.emplace_back("project_id");
		return_types.push_back(LogicalType::UUID);
		names.emplace_back("version_major");
		return_types.push_back(LogicalType::UTINYINT);
		names.emplace_back("version_minor");
		return_types.push_back(LogicalType::UTINYINT);
		names.emplace_back("system_id");
		return_types.push_back(LogicalType::VARCHAR);
		names.emplace_back("software_id");
		return_types.push_back(LogicalType::VARCHAR);
		names.emplace_back("creation_doy");
		return_types.push_back(LogicalType::USMALLINT);
		names.emplace_back("creation_year");
		return_types.push_back(LogicalType::USMALLINT);

		names.emplace_back("point_format");
		return_types.push_back(LogicalType::UTINYINT);
		names.emplace_back("point_offset");
		return_types.push_back(LogicalType::UINTEGER);
		names.emplace_back("point_len");
		return_types.push_back(LogicalType::USMALLINT);

		// Scale & Offset

		names.emplace_back("scale_x");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("scale_y");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("scale_z");
		return_types.push_back(LogicalType::DOUBLE);

		names.emplace_back("offset_x");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("offset_y");
		return_types.push_back(LogicalType::DOUBLE);
		names.emplace_back("offset_z");
		return_types.push_back(LogicalType::DOUBLE);

		// Returns info

		names.emplace_back("number_of_point_records");
		return_types.push_back(LogicalType::UINTEGER);
		names.emplace_back("number_of_points_by_return");
		return_types.push_back(LogicalType::LIST(LogicalType::UBIGINT));

		// Dimensions info

#ifndef DEBUG
		names.emplace_back("dimensions");
		return_types.push_back(LogicalType::LIST(PDAL_DIMENSION_TYPE()));
#endif

		// Get the filename list
		const auto mfreader = MultiFileReader::Create(input.table_function);
		const auto mflist = mfreader->CreateFileList(context, input.inputs[0], FileGlobOptions::ALLOW_EMPTY);
		return make_uniq_base<FunctionData, BindData>(mflist->GetAllFiles());
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Global
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
	// Init Local
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Execute
	//------------------------------------------------------------------------------------------------------------------

	static void Execute(ClientContext &context, TableFunctionInput &input, DataChunk &output) {

		auto &bind_data = input.bind_data->Cast<BindData>();
		auto &state = input.global_state->Cast<State>();

		// Calculate how many record we can fit in the output
		auto output_size = MinValue<idx_t>(STANDARD_VECTOR_SIZE, bind_data.files.size() - state.current_idx);

		if (output_size == 0) {
			output.SetCardinality(0);
			return;
		}

		pdal::StageFactory stage_factory;

		for (idx_t out_idx = 0; out_idx < output_size; out_idx++, state.current_idx++) {
			auto file = bind_data.files[state.current_idx];
			auto lower_path = StringUtil::Lower(file.path);

			try {
				pdal::Options read_options;
				read_options.add("filename", file.path);

				// Default LAS/LAZ Header fields values for the output

				pdal::FixedPointTable table(5);
				pdal::QuickInfo info = pdal::QuickInfo();
				bool extra_header = false;
				bool compressed = false;

				std::string file_signature;
				uint16_t file_source_id = 0;
				uint16_t global_encoding = 0;
				std::string project_id = "00000000-0000-0000-0000-000000000000";
				uint8_t version_major = 0;
				uint8_t version_minor = 0;
				std::string system_id;
				std::string software_id;
				uint16_t creation_doy = 0;
				uint16_t creation_year = 0;

				uint8_t point_format = 0;
				uint32_t point_offset = 0;
				uint16_t point_len = 0;

				double scale_x = 0.01;
				double scale_y = 0.01;
				double scale_z = 0.01;
				double offset_x = 0.0;
				double offset_y = 0.0;
				double offset_z = 0.0;

				uint32_t number_of_point_records = 0;
				std::vector<uint64_t> number_of_points_by_return;

				// Get the header data from the file

				if (StringUtil::EndsWith(lower_path, ".las") || StringUtil::EndsWith(lower_path, ".laz")) {

					pdal::LasReader reader;
					reader.setOptions(read_options);

					reader.prepare(table);
					info = reader.preview();
					const pdal::LasHeader &header = reader.header();

					extra_header = true;
					compressed = header.compressed();

					file_signature = header.fileSignature();
					file_source_id = header.fileSourceId();
					global_encoding = header.globalEncoding();
					project_id = header.projectId().toString();
					version_major = header.versionMajor();
					version_minor = header.versionMinor();
					system_id = header.systemId();
					software_id = header.softwareId();
					creation_doy = header.creationDOY();
					creation_year = header.creationYear();

					point_format = header.pointFormat();
					point_offset = header.pointOffset();
					point_len = header.pointLen();

					scale_x = header.scaleX();
					scale_y = header.scaleY();
					scale_z = header.scaleZ();
					offset_x = header.offsetX();
					offset_y = header.offsetY();
					offset_z = header.offsetZ();

					number_of_point_records = header.maxReturnCount();

					for (size_t i = 0; i < number_of_point_records; i++) {
						number_of_points_by_return.push_back(header.pointCountByReturn(i));
					}
				} else {
					std::string driver = pdal::StageFactory::inferReaderDriver(file.path);
					if (driver.length() == 0) {
						throw InvalidInputException("File format not supported: %s", file.path);
					}

					pdal::Stage *reader = stage_factory.createStage(driver);
					if (!reader) {
						throw InvalidInputException("Driver not found for file: %s", file.path);
					}
					reader->setOptions(read_options);

					reader->prepare(table);
					info = reader->preview();

					stage_factory.destroyStage(reader);
				}

				// Finally fill the output values

				int attr_idx = 0;
				output.data[attr_idx++].SetValue(out_idx, file.path);

				// General Point Cloud fields

				output.data[attr_idx++].SetValue(out_idx, Value::UBIGINT(info.m_pointCount));
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.minx);
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.miny);
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.minz);
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.maxx);
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.maxy);
				output.data[attr_idx++].SetValue(out_idx, info.m_bounds.maxz);
				output.data[attr_idx++].SetValue(out_idx, info.m_srs.getWKT());

				// LAS/LAZ Header fields

				output.data[attr_idx++].SetValue(out_idx, Value::BOOLEAN(extra_header));
				output.data[attr_idx++].SetValue(out_idx, Value::BOOLEAN(compressed));

				output.data[attr_idx++].SetValue(out_idx, file_signature);
				output.data[attr_idx++].SetValue(out_idx, Value::USMALLINT(file_source_id));
				output.data[attr_idx++].SetValue(out_idx, Value::USMALLINT(global_encoding));
				output.data[attr_idx++].SetValue(out_idx, Value::UUID(project_id));
				output.data[attr_idx++].SetValue(out_idx, Value::UTINYINT(version_major));
				output.data[attr_idx++].SetValue(out_idx, Value::UTINYINT(version_minor));
				output.data[attr_idx++].SetValue(out_idx, system_id);
				output.data[attr_idx++].SetValue(out_idx, software_id);
				output.data[attr_idx++].SetValue(out_idx, Value::USMALLINT(creation_doy));
				output.data[attr_idx++].SetValue(out_idx, Value::USMALLINT(creation_year));

				output.data[attr_idx++].SetValue(out_idx, Value::UTINYINT(point_format));
				output.data[attr_idx++].SetValue(out_idx, Value::UINTEGER(point_offset));
				output.data[attr_idx++].SetValue(out_idx, Value::USMALLINT(point_len));

				// Scale & Offset

				output.data[attr_idx++].SetValue(out_idx, scale_x);
				output.data[attr_idx++].SetValue(out_idx, scale_y);
				output.data[attr_idx++].SetValue(out_idx, scale_z);
				output.data[attr_idx++].SetValue(out_idx, offset_x);
				output.data[attr_idx++].SetValue(out_idx, offset_y);
				output.data[attr_idx++].SetValue(out_idx, offset_z);

				// Returns info

				output.data[attr_idx++].SetValue(out_idx, Value::UINTEGER(number_of_point_records));

				if (number_of_point_records > 0) {
					auto total_count = ListVector::GetListSize(output.data[attr_idx]);
					ListVector::Reserve(output.data[attr_idx], total_count + number_of_point_records);
					ListVector::SetListSize(output.data[attr_idx], total_count + number_of_point_records);

					auto &ref_entry = ListVector::GetData(output.data[attr_idx])[out_idx];
					auto &ref_vector = ListVector::GetEntry(output.data[attr_idx]);
					ref_entry.offset = total_count;
					ref_entry.length = number_of_point_records;

					auto ref_data = FlatVector::GetData<uint64_t>(ref_vector);

					for (size_t i = 0; i < number_of_point_records; i++) {
						ref_data[total_count + i] = number_of_points_by_return[i];
					}
					attr_idx++;
				} else {
					FlatVector::SetNull(output.data[attr_idx], out_idx, true);
					attr_idx++;
				}

				// Dimensions info

#ifndef DEBUG

				std::vector<duckdb::Value> dimensions;
				pdal::PointLayoutPtr layout = table.layout();

				for (const auto &dimId : layout->dims()) {
					std::string name = layout->dimName(dimId);
					const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
					pdal::Dimension::Type t = detail->type();

					child_list_t<Value> struct_entry;
					struct_entry.emplace_back("name", Value(name));
					struct_entry.emplace_back("type", Value(pdal::Dimension::interpretationName(t)));
					dimensions.push_back(Value::STRUCT(struct_entry));
				}
				output.data[attr_idx++].SetValue(out_idx, Value::LIST(PDAL_DIMENSION_TYPE(), std::move(dimensions)));

#endif

			} catch (...) {
				// Just skip anything we cant open
				out_idx--;
				output_size--;
				continue;
			}
		}
		output.SetCardinality(output_size);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Cardinality
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Replacement Scan
	//------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------------------------
	// Documentation
	//------------------------------------------------------------------------------------------------------------------

	static constexpr auto DESCRIPTION = R"(
		Read the metadata from a point cloud file.

		The `PDAL_Info` table function accompanies the `PDAL_Read` table function, but instead of reading the contents of a file, this function scans the metadata instead.
	)";

	static constexpr auto EXAMPLE = R"(
		SELECT * FROM PDAL_Info('./test/data/autzen_trim.laz');
	)";

	//------------------------------------------------------------------------------------------------------------------
	// Register
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "pdal");
		tags.insert("category", "table");

		const TableFunction func("PDAL_Info", {LogicalType::VARCHAR}, Execute, Bind, Init);

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
		pdal::point_count_t point_count = reader->preview().m_pointCount;

		// Create and return bind data.

		auto result = make_uniq<BindData>();
		result->file_name = file_name;
		result->stage_factory = std::move(stage_factory);
		result->table = std::move(table);
		result->views = std::move(views);
		result->point_count = point_count;

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

		if (!pdal::FileUtils::fileExists(file_name)) {
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
		if (roots.size() > 1) {
			throw InvalidInputException("Can't process pipeline with more than one root.");
		}
		if (roots.size() == 0) {
			throw InvalidInputException("Pipeline has no root stage.");
		}

		// Create the PDAL reader based on file extension and set reader options.

		pdal::Options reader_options;
		reader_options.add("filename", file_name);

		auto options_param = input.named_parameters.find("options");
		if (options_param != input.named_parameters.end()) {
			const std::vector<duckdb::Value> &children = MapValue::GetChildren(options_param->second);
			PDAL_Utils::ParseOptions(children, reader_options);
		}

		pdal::Stage *reader = &pipeline->makeReader(file_name, driver, reader_options);
		roots[0]->setInput(*reader);

		// Run the PDAL pipeline from the JSON file.

		pdal::point_count_t point_count = pipeline->execute();
		pdal::PointViewPtr view = *(pipeline->views().begin());

		pdal::PointLayoutPtr layout = view->layout();
		PDAL_Utils::ExtractLayout(layout, return_types, names);

		// Create and return bind data.

		auto result = make_uniq<BindData>();
		result->file_name = file_name;
		result->pipeline = std::move(pipeline);
		result->point_count = point_count;

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
		pdal::PointViewPtr view = *(bind_data.pipeline->views().begin());
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

//======================================================================================================================
// PDAL_Write
//======================================================================================================================

struct PDAL_Write {

	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData : public TableFunctionData {

		string file_name;
		vector<LogicalType> field_sql_types;
		vector<string> field_names;
		std::vector<idx_t> field_indexes;

		std::unique_ptr<pdal::StageFactory> stage_factory;
		std::unique_ptr<pdal::BufferReader> reader;
		pdal::Stage *writer = nullptr;
		std::unique_ptr<pdal::PointTable> table;
		std::shared_ptr<pdal::PointView> view;

		BindData(string file_name, vector<LogicalType> field_sql_types, vector<string> field_names)
		    : file_name(std::move(file_name)), field_sql_types(std::move(field_sql_types)),
		      field_names(std::move(field_names)) {
		}
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, CopyFunctionBindInput &input,
	                                     const vector<string> &names, const vector<LogicalType> &sql_types) {

		auto bind_data = make_uniq<BindData>(input.info.file_path, sql_types, names);

		std::string file_name = input.info.file_path;
		std::string driver_name;

		pdal::Options writer_options;
		writer_options.add("filename", file_name);

		// Check all the options in the copy info and set.

		for (auto &option : input.info.options) {
			if (StringUtil::Upper(option.first) == "DRIVER") {
				auto set = option.second.front();
				if (set.type().id() == LogicalTypeId::VARCHAR) {
					driver_name = StringUtil::Lower(set.GetValue<string>());

					if (!StringUtil::StartsWith(driver_name, "writers.")) {
						driver_name = "writers." + driver_name;
					}
				} else {
					throw BinderException("Driver name must be a string");
				}
			} else if (StringUtil::Upper(option.first) == "CREATION_OPTIONS") {
				auto set = option.second;
				for (auto &s : set) {
					if (s.type().id() != LogicalTypeId::VARCHAR) {
						throw BinderException("Creation options must be strings");
					}
					auto kv_pair = StringUtil::Split(s.GetValue<string>(), '=');
					if (kv_pair.size() != 2) {
						throw InvalidInputException("Invalid input passed to options parameter");
					}
					writer_options.add(StringUtil::Lower(kv_pair[0]), kv_pair[1]);
				}
			} else {
				throw BinderException("Unknown option '%s'", option.first);
			}
		}

		if (driver_name.empty()) {
			driver_name = pdal::StageFactory::inferWriterDriver(file_name);
		}
		if (driver_name.empty()) {
			throw BinderException("Driver name must be specified");
		}

		// Create the PDAL reader & writer and prepare the target table.

		std::unique_ptr<pdal::StageFactory> stage_factory = std::make_unique<pdal::StageFactory>();

		std::unique_ptr<pdal::BufferReader> reader = std::make_unique<pdal::BufferReader>();
		if (!reader) {
			throw InvalidInputException("Driver 'readers.buffer' was not found in PDAL installation");
		}

		pdal::Stage *writer = stage_factory->createStage(driver_name);
		if (!writer) {
			throw InvalidInputException("Driver not found for file: %s", file_name);
		}

		std::unique_ptr<pdal::PointTable> table = std::make_unique<pdal::PointTable>();
		std::shared_ptr<pdal::PointView> view = std::make_shared<pdal::PointView>(*table);

		reader->addView(view);
		writer->setInput(*reader);
		writer->setOptions(writer_options);
		writer->prepare(*table);

		// Fill the layout by mapping SQL types to PDAL types.

		pdal::PointLayoutPtr layout = table->layout();
		auto &logger = Logger::Get(context);

		std::vector<idx_t> field_indexes = PDAL_Utils::FillLayout(layout, sql_types, names, logger);
		bind_data->field_indexes = std::move(field_indexes);

		// Return bind data.

		bind_data->stage_factory = std::move(stage_factory);
		bind_data->reader = std::move(reader);
		bind_data->writer = writer;
		bind_data->table = std::move(table);
		bind_data->view = std::move(view);

		return std::move(bind_data);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Global
	//------------------------------------------------------------------------------------------------------------------

	struct GlobalState final : GlobalFunctionData {
		explicit GlobalState(ClientContext &context) {
		}
	};

	static unique_ptr<GlobalFunctionData> InitGlobal(ClientContext &context, FunctionData &fdata,
	                                                 const string &file_path) {
		auto global_data = make_uniq<GlobalState>(context);
		return std::move(global_data);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Init Local
	//------------------------------------------------------------------------------------------------------------------

	struct LocalState : public LocalFunctionData {
		explicit LocalState(ClientContext &context) {
		}
	};

	static unique_ptr<LocalFunctionData> InitLocal(ExecutionContext &context, FunctionData &fdata) {
		auto local_data = make_uniq<LocalState>(context.client);
		return std::move(local_data);
	}

	//------------------------------------------------------------------------------------------------------------------
	// Sink
	//------------------------------------------------------------------------------------------------------------------

	static void Sink(ExecutionContext &context, FunctionData &fdata, GlobalFunctionData &gstate,
	                 LocalFunctionData &lstate, DataChunk &input) {

		auto &bind_data = fdata.Cast<BindData>();

		pdal::PointLayoutPtr layout = bind_data.view->layout();
		pdal::PointView *view = bind_data.view.get();

		std::vector<idx_t> &field_indexes = bind_data.field_indexes;
		pdal::PointId record_start = view->size();

		// Write the points into the output
		input.Flatten();
		for (idx_t row_idx = 0, col_idx = 0, field_idx = 0; row_idx < input.size(); row_idx++) {
			field_idx = 0;

			for (const auto &dimId : layout->dims()) {
				const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
				pdal::Dimension::Type t = detail->type();

				col_idx = field_indexes[field_idx];
				duckdb::Value value = input.GetValue(col_idx, row_idx);

				switch (t) {
				case pdal::Dimension::Type::Float:
					view->setField<float>(dimId, record_start + row_idx, value.GetValue<float>());
					break;
				case pdal::Dimension::Type::Double:
					view->setField<double>(dimId, record_start + row_idx, value.GetValue<double>());
					break;
				case pdal::Dimension::Type::Signed8:
					view->setField<int8_t>(dimId, record_start + row_idx, value.GetValue<int8_t>());
					break;
				case pdal::Dimension::Type::Signed16:
					view->setField<int16_t>(dimId, record_start + row_idx, value.GetValue<int16_t>());
					break;
				case pdal::Dimension::Type::Signed32:
					view->setField<int32_t>(dimId, record_start + row_idx, value.GetValue<int32_t>());
					break;
				case pdal::Dimension::Type::Signed64:
					view->setField<int64_t>(dimId, record_start + row_idx, value.GetValue<int64_t>());
					break;
				case pdal::Dimension::Type::Unsigned8:
					view->setField<uint8_t>(dimId, record_start + row_idx, value.GetValue<uint8_t>());
					break;
				case pdal::Dimension::Type::Unsigned16:
					view->setField<uint16_t>(dimId, record_start + row_idx, value.GetValue<uint16_t>());
					break;
				case pdal::Dimension::Type::Unsigned32:
					view->setField<uint32_t>(dimId, record_start + row_idx, value.GetValue<uint32_t>());
					break;
				case pdal::Dimension::Type::Unsigned64:
					view->setField<uint64_t>(dimId, record_start + row_idx, value.GetValue<uint64_t>());
					break;
				default:
					throw InvalidInputException("Unsupported PDAL dimension type in write: %d.", static_cast<int>(t));
				}
				field_idx++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------------------------
	// Combine
	//------------------------------------------------------------------------------------------------------------------

	static void Combine(ExecutionContext &context, FunctionData &fdata, GlobalFunctionData &gstate,
	                    LocalFunctionData &lstate) {
	}

	//------------------------------------------------------------------------------------------------------------------
	// Finalize
	//------------------------------------------------------------------------------------------------------------------

	static void Finalize(ClientContext &context, FunctionData &fdata, GlobalFunctionData &gstate) {
		auto &bind_data = fdata.Cast<BindData>();

		// Flush writer
		pdal::PointTable *table = bind_data.table.get();
		pdal::Stage *writer = bind_data.writer;
		writer->execute(*table);
	}

	//------------------------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------------------------

	static void Register(ExtensionLoader &loader) {
		// register the copy function
		CopyFunction info("PDAL");
		info.copy_to_bind = Bind;
		info.copy_to_initialize_local = InitLocal;
		info.copy_to_initialize_global = InitGlobal;
		info.copy_to_sink = Sink;
		info.copy_to_combine = Combine;
		info.copy_to_finalize = Finalize;
		info.extension = "pdal";

		loader.RegisterFunction(info);
	}
};

} // namespace

// ######################################################################################################################
//  Register Table Functions
// ######################################################################################################################

void PdalTableFunctions::Register(ExtensionLoader &loader) {

	PDAL_Drivers::Register(loader);
	PDAL_Info::Register(loader);
	PDAL_Read::Register(loader);
	PDAL_Pipeline::Register(loader);
	PDAL_Write::Register(loader);
}

} // namespace duckdb
