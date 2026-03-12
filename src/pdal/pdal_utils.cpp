#include "pdal_utils.hpp"

namespace duckdb {

// Parse a DuckDB struct array of key-value pairs into a PDAL Options object.
void PdalUtils::ParseOptions(const std::vector<duckdb::Value> &input, pdal::Options &options) {
	for (const auto &kv_child : input) {
		auto kv_pair = StructValue::GetChildren(kv_child);
		if (kv_pair.size() != 2) {
			throw InvalidInputException("Invalid input passed to options parameter");
		}
		auto key = StringValue::Get(kv_pair[0]);
		auto val = StringValue::Get(kv_pair[1]);
		options.add(key, val);
	}
}

// Copy schema from one PDAL PointLayout to another.
void PdalUtils::CopyLayout(const pdal::PointLayoutPtr input, const pdal::PointLayoutPtr output) {
	for (const auto &dimId : input->dims()) {
		std::string name = input->dimName(dimId);
		const pdal::Dimension::Detail *detail = input->dimDetail(dimId);
		pdal::Dimension::Type t = detail->type();

		output->registerOrAssignDim(name, t);
	}
}

// Extract DuckDB names and types from a PDAL PointLayout.
void PdalUtils::ExtractLayout(const pdal::PointLayoutPtr layout, vector<string> &names, vector<LogicalType> &types) {
	for (const auto &dimId : layout->dims()) {
		std::string name = layout->dimName(dimId);
		const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
		pdal::Dimension::Type t = detail->type();

		switch (t) {
		case pdal::Dimension::Type::Float:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::FLOAT);
			break;
		case pdal::Dimension::Type::Double:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::DOUBLE);
			break;

		case pdal::Dimension::Type::Signed8:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::TINYINT);
			break;
		case pdal::Dimension::Type::Signed16:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::SMALLINT);
			break;
		case pdal::Dimension::Type::Signed32:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::INTEGER);
			break;
		case pdal::Dimension::Type::Signed64:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::BIGINT);
			break;

		case pdal::Dimension::Type::Unsigned8:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::UTINYINT);
			break;
		case pdal::Dimension::Type::Unsigned16:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::USMALLINT);
			break;
		case pdal::Dimension::Type::Unsigned32:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::UINTEGER);
			break;
		case pdal::Dimension::Type::Unsigned64:
			names.emplace_back(name);
			types.emplace_back(LogicalTypeId::UBIGINT);
			break;

		default:
			throw InvalidInputException("Field type %d not supported", t);
		}
	}
}

// Fill a PDAL PointLayout from a set of DuckDB names and types.
std::vector<idx_t> PdalUtils::FillLayout(pdal::PointLayoutPtr layout, const vector<string> &names,
                                         const vector<LogicalType> &types, Logger &logger) {
	if (names.size() != types.size()) {
		throw InvalidInputException("SQL types and names size mismatch");
	}

	std::vector<idx_t> field_indexes;

	for (idx_t i = 0; i < names.size(); i++) {
		const auto &name = names[i];
		const auto &type = types[i];

		switch (type.id()) {
		case LogicalTypeId::FLOAT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Float);
			break;
		case LogicalTypeId::DOUBLE:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Double);
			break;

		case LogicalTypeId::TINYINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed8);
			break;
		case LogicalTypeId::SMALLINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed16);
			break;
		case LogicalTypeId::INTEGER:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed32);
			break;
		case LogicalTypeId::BIGINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Signed64);
			break;

		case LogicalTypeId::UTINYINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned8);
			break;
		case LogicalTypeId::USMALLINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned16);
			break;
		case LogicalTypeId::UINTEGER:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned32);
			break;
		case LogicalTypeId::UBIGINT:
			layout->registerOrAssignDim(name, pdal::Dimension::Type::Unsigned64);
			break;

		default:
			logger.WriteLog("pdal", LogLevel::LOG_WARNING, "Field type '%s' not supported, skipping dimension '%s'.",
			                type.ToString().c_str(), name.c_str());
			continue;
		}
		field_indexes.push_back(i);
	}
	return field_indexes;
}

// Extract a chunk of points from a PDAL PointView into a DuckDB DataChunk.
void PdalUtils::ExtractDataChunk(const pdal::PointViewPtr view, idx_t point_start, std::size_t point_count,
                                 const std::vector<column_t> &column_ids, DataChunk &output) {
	pdal::PointLayoutPtr layout = view->layout();
	pdal::PointRef point(*view, point_start);

	const pdal::Dimension::IdList &dims = layout->dims();

	for (idx_t row_idx = 0, point_idx = point_start; row_idx < point_count; row_idx++, point_idx++) {
		point.setPointId(point_idx);

		for (idx_t col_idx = 0; col_idx < column_ids.size(); col_idx++) {
			const auto &dim_index = column_ids[col_idx];

			const auto &dimId = dims[dim_index];
			const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
			pdal::Dimension::Type t = detail->type();

			switch (t) {
			case pdal::Dimension::Type::Float: {
				float value = point.getFieldAs<float>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::FLOAT(value));
				break;
			}
			case pdal::Dimension::Type::Double: {
				double value = point.getFieldAs<double>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::DOUBLE(value));
				break;
			}
			case pdal::Dimension::Type::Signed8: {
				int8_t value = point.getFieldAs<int8_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::TINYINT(value));
				break;
			}
			case pdal::Dimension::Type::Signed16: {
				int16_t value = point.getFieldAs<int16_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::SMALLINT(value));
				break;
			}
			case pdal::Dimension::Type::Signed32: {
				int32_t value = point.getFieldAs<int32_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::INTEGER(value));
				break;
			}
			case pdal::Dimension::Type::Signed64: {
				int64_t value = point.getFieldAs<int64_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::BIGINT(value));
				break;
			}
			case pdal::Dimension::Type::Unsigned8: {
				uint8_t value = point.getFieldAs<uint8_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::UTINYINT(value));
				break;
			}
			case pdal::Dimension::Type::Unsigned16: {
				uint16_t value = point.getFieldAs<uint16_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::USMALLINT(value));
				break;
			}
			case pdal::Dimension::Type::Unsigned32: {
				uint32_t value = point.getFieldAs<uint32_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::UINTEGER(value));
				break;
			}
			case pdal::Dimension::Type::Unsigned64: {
				uint64_t value = point.getFieldAs<uint64_t>(dimId);
				output.SetValue(col_idx, row_idx, duckdb::Value::UBIGINT(value));
				break;
			}
			default:
				throw InvalidInputException("Field type %d not supported", t);
			}
		}
	}
}

// Fill a PDAL PointView from a DuckDB DataChunk.
void PdalUtils::FillDataChunk(pdal::PointView *view, const DataChunk &input, const std::vector<idx_t> &field_indexes) {
	pdal::PointLayoutPtr layout = view->layout();
	pdal::PointId point_start = view->size();

	// Write the points into the output
	for (idx_t row_idx = 0, col_idx = 0, field_idx = 0; row_idx < input.size(); row_idx++) {
		field_idx = 0;

		for (const auto &dimId : layout->dims()) {
			const pdal::Dimension::Detail *detail = layout->dimDetail(dimId);
			pdal::Dimension::Type t = detail->type();

			col_idx = field_indexes[field_idx];
			duckdb::Value value = input.GetValue(col_idx, row_idx);

			switch (t) {
			case pdal::Dimension::Type::Float:
				view->setField<float>(dimId, point_start + row_idx, value.GetValue<float>());
				break;
			case pdal::Dimension::Type::Double:
				view->setField<double>(dimId, point_start + row_idx, value.GetValue<double>());
				break;
			case pdal::Dimension::Type::Signed8:
				view->setField<int8_t>(dimId, point_start + row_idx, value.GetValue<int8_t>());
				break;
			case pdal::Dimension::Type::Signed16:
				view->setField<int16_t>(dimId, point_start + row_idx, value.GetValue<int16_t>());
				break;
			case pdal::Dimension::Type::Signed32:
				view->setField<int32_t>(dimId, point_start + row_idx, value.GetValue<int32_t>());
				break;
			case pdal::Dimension::Type::Signed64:
				view->setField<int64_t>(dimId, point_start + row_idx, value.GetValue<int64_t>());
				break;
			case pdal::Dimension::Type::Unsigned8:
				view->setField<uint8_t>(dimId, point_start + row_idx, value.GetValue<uint8_t>());
				break;
			case pdal::Dimension::Type::Unsigned16:
				view->setField<uint16_t>(dimId, point_start + row_idx, value.GetValue<uint16_t>());
				break;
			case pdal::Dimension::Type::Unsigned32:
				view->setField<uint32_t>(dimId, point_start + row_idx, value.GetValue<uint32_t>());
				break;
			case pdal::Dimension::Type::Unsigned64:
				view->setField<uint64_t>(dimId, point_start + row_idx, value.GetValue<uint64_t>());
				break;
			default:
				throw InvalidInputException("Unsupported PDAL dimension type in write: %d.", static_cast<int>(t));
			}
			field_idx++;
		}
	}
}

} // namespace duckdb
