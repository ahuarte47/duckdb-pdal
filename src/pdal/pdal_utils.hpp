#pragma once

// DuckDB
#include "duckdb.hpp"
// PDAL
#include "pdal/Options.hpp"
#include "pdal/PointLayout.hpp"
#include "pdal/PointView.hpp"

namespace duckdb {

struct PdalUtils {
public:
	// Parse a DuckDB struct array of key-value pairs into a PDAL Options object.
	static void ParseOptions(const std::vector<duckdb::Value> &input, pdal::Options &options);

	// Copy schema from one PDAL PointLayout to another.
	static void CopyLayout(const pdal::PointLayoutPtr input, const pdal::PointLayoutPtr output);

	// Extract DuckDB names and types from a PDAL PointLayout.
	static void ExtractLayout(const pdal::PointLayoutPtr layout, vector<string> &names, vector<LogicalType> &types);

	// Fill a PDAL PointLayout from a set of DuckDB names and types.
	static std::vector<idx_t> FillLayout(pdal::PointLayoutPtr layout, const vector<string> &names,
	                                     const vector<LogicalType> &types, Logger &logger);

	// Extract a chunk of points from a PDAL PointView into a DuckDB DataChunk.
	static void ExtractDataChunk(const pdal::PointViewPtr view, idx_t point_start, std::size_t point_count,
	                             DataChunk &output);

	// Fill a PDAL PointView from a DuckDB DataChunk.
	static void FillDataChunk(pdal::PointView *view, const DataChunk &input, const std::vector<idx_t> &field_indexes);
};

} // namespace duckdb
