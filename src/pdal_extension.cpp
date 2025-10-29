#define DUCKDB_EXTENSION_MAIN

#include "pdal_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

inline void PdalScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "Pdal " + name.GetString() + " 🐥");
	});
}

inline void PdalOpenSSLVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "Pdal " + name.GetString() + ", my linked OpenSSL version is " +
		                                           OPENSSL_VERSION_TEXT);
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	// Register a scalar function
	auto pdal_scalar_function = ScalarFunction("pdal", {LogicalType::VARCHAR}, LogicalType::VARCHAR, PdalScalarFun);
	loader.RegisterFunction(pdal_scalar_function);

	// Register another scalar function
	auto pdal_openssl_version_scalar_function = ScalarFunction("pdal_openssl_version", {LogicalType::VARCHAR},
	                                                            LogicalType::VARCHAR, PdalOpenSSLVersionScalarFun);
	loader.RegisterFunction(pdal_openssl_version_scalar_function);
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
