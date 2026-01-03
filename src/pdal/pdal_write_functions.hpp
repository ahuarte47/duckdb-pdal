#pragma once

namespace duckdb {

class ExtensionLoader;

struct PdalWriteFunctions {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
