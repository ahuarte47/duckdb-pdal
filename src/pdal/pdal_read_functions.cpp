#include "pdal_read_functions.hpp"
#include "function_builder.hpp"
#include "filter_encoder.hpp"

// DuckDB
#include "duckdb/main/database.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/parsed_data/create_copy_function_info.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/operator/logical_limit.hpp"
#include "duckdb/planner/operator/logical_order.hpp"

// PDAL
#include <pdal/PipelineManager.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/LasHeader.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/util/FileUtils.hpp>
#include "pdal_utils.hpp"

namespace duckdb {

namespace {

//======================================================================================================================
// PDAL_Info
//======================================================================================================================

// Define PDAL dimension type for the "dimensions" field.
inline LogicalType PDAL_DIMENSION_TYPE() {
	LogicalType varchar_type = LogicalType(LogicalTypeId::VARCHAR);
	return LogicalType::STRUCT({{"name", varchar_type}, {"type", varchar_type}});
}

// Get QuickInfo of a LAZ/LAS file.
pdal::QuickInfo GetQuickInfo(pdal::PointTableRef table, pdal::LasReader *reader) {
	pdal::QuickInfo qi;

	// NOTE:
	// Do not call LasReader->preview(), it reopens the file again. If it is a remote file then it
	// can be downloaded twice. Instead, use the header to fill the QuickInfo data.

	pdal::PointLayoutPtr layout = table.layout();
	pdal::LasHeader header = reader->header();

	for (const auto &dimId : layout->dims()) {
		qi.m_dimNames.push_back(layout->dimName(dimId));
	}

	if (!pdal::Utils::numericCast(header.pointCount(), qi.m_pointCount)) {
		qi.m_pointCount = (std::numeric_limits<pdal::point_count_t>::max)();
	}

	qi.m_bounds = header.getBounds();
	qi.m_srs = header.srs();
	qi.m_metadata = reader->getMetadata();
	qi.m_valid = true;

	return qi;
}

struct PDAL_Info {
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

		names.emplace_back("dimensions");
		return_types.push_back(LogicalType::LIST(PDAL_DIMENSION_TYPE()));

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
			auto file_ext = StringUtil::GetFileExtension(lower_path);

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

				if ((file_ext == "las" || file_ext == "laz") && !StringUtil::EndsWith(lower_path, ".copc.laz")) {
					pdal::LasReader reader;
					reader.setOptions(read_options);

					reader.prepare(table);
					info = GetQuickInfo(table, &reader);
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
		pdal::Options reader_options;
		std::string where_clause;
		std::vector<std::string> column_names;
		std::vector<LogicalType> column_types;
		uint64_t point_count = 0;
		uint64_t point_limit = 0;

		// Variables used when WHERE clause is not pushed down.
		std::unique_ptr<pdal::PointTable> table;
		pdal::PointViewSet views;
		// Variables used when WHERE clause is pushed down.
		std::unique_ptr<pdal::PipelineManager> pipeline;
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names) {
		auto file_name = StringValue::Get(input.inputs[0]);
		auto &fs = FileSystem::GetFileSystem(context);

		if (!fs.IsRemoteFile(file_name) && !pdal::FileUtils::fileExists(file_name)) {
			throw InvalidInputException("File not found: %s", file_name);
		}

		std::string driver = pdal::StageFactory::inferReaderDriver(file_name);
		if (driver.length() == 0) {
			throw InvalidInputException("File format not supported: %s", file_name);
		}

		pdal::Options reader_options;
		reader_options.add("filename", file_name);

		auto options_param = input.named_parameters.find("options");
		if (options_param != input.named_parameters.end()) {
			const std::vector<duckdb::Value> &children = MapValue::GetChildren(options_param->second);
			PdalUtils::ParseOptions(children, reader_options);
		}

		// Create the PDAL reader based on file extension to Make the PDAL PointTable where layout is stored,
		// and set the output schema.

		auto lower_path = StringUtil::Lower(file_name);
		auto file_ext = StringUtil::GetFileExtension(lower_path);

		pdal::point_count_t point_count = 0;

		if ((file_ext == "las" || file_ext == "laz") && !StringUtil::EndsWith(lower_path, ".copc.laz")) {
			pdal::LasReader reader;
			reader.setOptions(reader_options);

			pdal::FixedPointTable table(5);
			reader.prepare(table);

			pdal::PointLayoutPtr layout = table.layout();
			PdalUtils::ExtractLayout(layout, names, return_types);

			const pdal::LasHeader &header = reader.header();
			point_count = header.pointCount();
		} else {
			pdal::StageFactory stage_factory;
			pdal::Stage *reader = stage_factory.createStage(driver);

			if (!reader) {
				throw InvalidInputException("Driver not found for file: %s", file_name);
			}

			reader->setOptions(reader_options);

			std::unique_ptr<pdal::PointTable> table = std::make_unique<pdal::PointTable>();
			reader->prepare(*table);

			pdal::PointLayoutPtr layout = table->layout();
			PdalUtils::ExtractLayout(layout, names, return_types);

			point_count = reader->preview().m_pointCount;
			stage_factory.destroyStage(reader);
		}

		// Create and return bind data.

		auto bind_data = make_uniq<BindData>();
		bind_data->file_name = file_name;
		bind_data->reader_options = reader_options;
		bind_data->column_names = names;
		bind_data->column_types = return_types;
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
		auto &bind_data = (BindData &)*input.bind_data;

		const auto &file_name = bind_data.file_name;
		const auto &reader_options = bind_data.reader_options;

		std::string driver = pdal::StageFactory::inferReaderDriver(file_name);
		if (driver.length() == 0) {
			throw InvalidInputException("File format not supported: %s", file_name);
		}

		// Depending on whether or not there is a where clause that can be pushed down...
		if (bind_data.where_clause.empty()) {
			// Just read the whole file and keep it in memory for execution.
			pdal::StageFactory stage_factory;
			pdal::Stage *reader = stage_factory.createStage(driver);

			if (bind_data.point_limit > 0) {
				pdal::Options temp_options(reader_options);
				temp_options.add("count", bind_data.point_limit);
				reader->setOptions(temp_options);
			} else {
				reader->setOptions(reader_options);
			}
			std::unique_ptr<pdal::PointTable> table = std::make_unique<pdal::PointTable>();
			reader->prepare(*table);

			pdal::PointViewSet views = reader->execute(*table);
			bind_data.table = std::move(table);
			bind_data.views = std::move(views);
		} else {
			// Encode the where clause into a PDAL pipeline and push down the filter to PDAL.

			std::string the_pipeline = StringUtil::Format(
			    "{\"type\": \"filters.expression\", \"expression\": \"(%s)\"}", bind_data.where_clause);

			if (bind_data.point_limit > 0) {
				the_pipeline +=
				    StringUtil::Format(", {\"type\": \"filters.head\", \"count\": %zu}", bind_data.point_limit);
			}

			std::unique_ptr<pdal::PipelineManager> pipeline = std::make_unique<pdal::PipelineManager>();
			std::stringstream ssin("[" + the_pipeline + "]");
			pipeline->readPipeline(ssin);

			std::vector<pdal::Stage *> roots = pipeline->roots();
			if (roots.size() != 1) {
				throw InvalidInputException("Can't process pipelines without an unique root.");
			}

			pdal::Stage *reader = &pipeline->makeReader(file_name, driver, reader_options);
			roots[0]->setInput(*reader);
			pdal::point_count_t point_count = pipeline->execute();

			bind_data.pipeline = std::move(pipeline);
			bind_data.point_count = point_count;
		}

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

		const uint64_t point_count = bind_data.point_limit > 0
		                                 ? MinValue<uint64_t>(bind_data.point_limit, bind_data.point_count)
		                                 : bind_data.point_count;

		// Calculate how many record we can fit in the output
		const auto output_size = MinValue<idx_t>(STANDARD_VECTOR_SIZE, point_count - gstate.point_idx);
		const auto point_start = gstate.point_idx;

		if (output_size == 0) {
			output.SetCardinality(0);
			return;
		}

		// Load current subset of points into the output.
		if (bind_data.where_clause.empty()) {
			pdal::PointViewPtr view = *(bind_data.views.begin());
			PdalUtils::ExtractDataChunk(view, point_start, output_size, gstate.column_ids, output);
		} else {
			pdal::PointViewPtr view = *(bind_data.pipeline->views().begin());
			PdalUtils::ExtractDataChunk(view, point_start, output_size, gstate.column_ids, output);
		}

		// Update the point index
		gstate.point_idx += output_size;

		// Set the cardinality of the output
		output.SetCardinality(output_size);
	};

	//------------------------------------------------------------------------------------------------------------------
	// Optimize (Only LIMIT pushdown is implemented)
	//------------------------------------------------------------------------------------------------------------------

	static void Optimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &op) {
		// Apply optimizations on the LogicalPlan

		if (op->type == LogicalOperatorType::LOGICAL_LIMIT) {
			auto &limit = op->Cast<LogicalLimit>();

			// Only push down simple LIMIT without OFFSET, ORDER BY or GROUP BY, and with a constant value,
			// as it would change the result of the query.
			if (limit.limit_val.Type() != LimitNodeType::CONSTANT_VALUE) {
				return;
			}
			if (limit.offset_val.Type() != LimitNodeType::UNSET) {
				return;
			}
			for (const auto &child : op->children) {
				if (child->type == LogicalOperatorType::LOGICAL_ORDER_BY) {
					return;
				}
				if (child->type == LogicalOperatorType::LOGICAL_AGGREGATE_AND_GROUP_BY) {
					return;
				}
				if (child->type == LogicalOperatorType::LOGICAL_GET) {
					auto &get = child->Cast<LogicalGet>();

					if (StringUtil::Lower(get.function.name) == "pdal_read") {
						auto &bind_data = get.bind_data->Cast<BindData>();
						bind_data.point_limit = limit.limit_val.GetConstantValue();
						return;
					}
				}
			}
		}

		// Recurse into children
		for (auto &child : op->children) {
			Optimize(input, child);
		}
	}

	//------------------------------------------------------------------------------------------------------------------
	// Complex Filter Pushdown
	//------------------------------------------------------------------------------------------------------------------

	static void PushdownComplexFilter(ClientContext &context, LogicalGet &get, FunctionData *bind_data_p,
	                                  vector<unique_ptr<Expression>> &expressions) {
		auto &bind_data = bind_data_p->Cast<BindData>();

		// Get column_ids from LogicalGet to map expression column indices to table columns.

		const auto &get_column_ids = get.GetColumnIds();

		std::vector<column_t> column_ids;
		for (const auto &col_idx : get_column_ids) {
			column_ids.push_back(col_idx.IsVirtualColumn() ? COLUMN_IDENTIFIER_ROW_ID : col_idx.GetPrimaryIndex());
		}

		// Encode the expressions into a PDAL-readable where clause.

		ExpressionEncodeContext ctx(column_ids, bind_data.column_names, bind_data.column_types);

		FilterEncoderResult result = FilterEncoder::EncodeExpressions(expressions, ctx);
		if (result.supported && !result.where_clause.empty()) {
			bind_data.where_clause = result.where_clause;
			expressions.clear();
		}
	}

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
		auto lower_path = StringUtil::Lower(table_name);

		// Check if the file extension is a common LiDAR file extension
		if (StringUtil::EndsWith(lower_path, ".las") || StringUtil::EndsWith(lower_path, ".laz")) {
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

		// Enable projection pushdown - allows DuckDB to tell us which columns are needed
		// The column_ids will be passed to InitGlobal via TableFunctionInitInput
		func.projection_pushdown = true;

		// Enable complex filter pushdown - handles expressions like (A AND B) OR (C AND D)
		// that cannot be represented as simple TableFilter objects
		func.pushdown_complex_filter = PushdownComplexFilter;

		RegisterFunction<TableFunction>(loader, func, CatalogType::TABLE_FUNCTION_ENTRY, DESCRIPTION, EXAMPLE, tags);

		// Replacement scan
		auto &db = loader.GetDatabaseInstance();
		auto &config = DBConfig::GetConfig(db);
		config.replacement_scans.emplace_back(ReplacementScan);

		// Register optimizer extension for LIMIT pushdown
		OptimizerExtension pdal_optimizer;
		pdal_optimizer.optimize_function = PDAL_Read::Optimize;
		OptimizerExtension::Register(config, std::move(pdal_optimizer));
	}
};

} // namespace

// #####################################################################################################################
// Register Read Functions
// #####################################################################################################################

void PdalReadFunctions::Register(ExtensionLoader &loader) {
	PDAL_Info::Register(loader);
	PDAL_Read::Register(loader);
}

} // namespace duckdb
