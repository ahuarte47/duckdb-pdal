#pragma once

namespace duckdb {

class ExtensionLoader;

struct PdalReadFunctions {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
