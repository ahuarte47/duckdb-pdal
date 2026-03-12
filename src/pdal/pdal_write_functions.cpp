#include "pdal_write_functions.hpp"
#include "function_builder.hpp"

// DuckDB
#include "duckdb/main/database.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/parsed_data/create_copy_function_info.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

// PDAL
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/io/BufferReader.hpp>
#include "pdal_utils.hpp"

namespace duckdb {

namespace {

//======================================================================================================================
// PDAL_Write
//======================================================================================================================

struct PDAL_Write {
	//------------------------------------------------------------------------------------------------------------------
	// Bind
	//------------------------------------------------------------------------------------------------------------------

	struct BindData : public TableFunctionData {
		string file_name;

		vector<string> field_names;
		vector<LogicalType> field_types;
		std::vector<idx_t> field_indexes;

		std::unique_ptr<pdal::StageFactory> stage_factory;
		std::unique_ptr<pdal::BufferReader> reader;
		pdal::Stage *writer = nullptr;
		std::unique_ptr<pdal::PointTable> table;
		std::shared_ptr<pdal::PointView> view;

		BindData(string file_name, vector<string> field_names, vector<LogicalType> field_types)
		    : file_name(std::move(file_name)), field_names(std::move(field_names)),
		      field_types(std::move(field_types)) {
		}
	};

	static unique_ptr<FunctionData> Bind(ClientContext &context, CopyFunctionBindInput &input,
	                                     const vector<string> &names, const vector<LogicalType> &types) {
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

		std::vector<idx_t> field_indexes = PdalUtils::FillLayout(layout, names, types, logger);

		// Return bind data.

		auto bind_data = make_uniq<BindData>(input.info.file_path, names, types);
		bind_data->stage_factory = std::move(stage_factory);
		bind_data->reader = std::move(reader);
		bind_data->writer = writer;
		bind_data->table = std::move(table);
		bind_data->view = std::move(view);
		bind_data->field_indexes = std::move(field_indexes);

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

		pdal::PointView *view = bind_data.view.get();
		const std::vector<idx_t> &field_indexes = bind_data.field_indexes;

		// Write the points into the output
		input.Flatten();
		PdalUtils::FillDataChunk(view, input, field_indexes);
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

// #####################################################################################################################
// Register Write Functions
// #####################################################################################################################

void PdalWriteFunctions::Register(ExtensionLoader &loader) {
	PDAL_Write::Register(loader);
}

} // namespace duckdb
