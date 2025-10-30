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

namespace duckdb {

namespace {

//======================================================================================================================
// PDAL Types & Utils
//======================================================================================================================

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

} // namespace

// ######################################################################################################################
//  Register Table Functions
// ######################################################################################################################

void PdalTableFunctions::Register(ExtensionLoader &loader) {

	PDAL_Drivers::Register(loader);
}

} // namespace duckdb
