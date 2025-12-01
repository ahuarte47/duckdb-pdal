#pragma once

namespace duckdb {

class ExtensionLoader;

class PdalStaticRegistry {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
