#pragma once

namespace duckdb {

class ExtensionLoader;

struct PdalTableFunctions {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
