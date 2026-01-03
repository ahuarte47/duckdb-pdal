#pragma once

namespace duckdb {

class ExtensionLoader;

struct PdalDrivers {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
