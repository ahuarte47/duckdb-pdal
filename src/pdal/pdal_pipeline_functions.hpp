#pragma once

namespace duckdb {

class ExtensionLoader;

struct PdalPipelineFunctions {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
