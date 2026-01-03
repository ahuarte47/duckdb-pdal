#define DUCKDB_EXTENSION_MAIN

#include "pdal_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// PDAL
#include "pdal/pdal_static_registry.hpp"
#include "pdal/pdal_drivers.hpp"
#include "pdal/pdal_read_functions.hpp"
#include "pdal/pdal_pipeline_functions.hpp"
#include "pdal/pdal_write_functions.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	// Register static PDAL plugins first
	PdalStaticRegistry::Register(loader);

	// Register functions
	PdalDrivers::Register(loader);
	PdalReadFunctions::Register(loader);
	PdalPipelineFunctions::Register(loader);
	PdalWriteFunctions::Register(loader);
}

void PdalExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string PdalExtension::Name() {
	return "pdal";
}

std::string PdalExtension::Version() const {
#ifdef EXT_VERSION_PDAL
	return EXT_VERSION_PDAL;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(pdal, loader) {
	duckdb::LoadInternal(loader);
}
}
