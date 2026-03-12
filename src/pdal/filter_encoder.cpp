#include "filter_encoder.hpp"
#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "duckdb/planner/filter/in_filter.hpp"
#include "duckdb/planner/filter/optional_filter.hpp"
#include "duckdb/planner/expression/bound_between_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_columnref_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"

// Debug logging controlled by PDAL_DEBUG environment variable
static int GetDebugLevel() {
	static int level = -1;
	if (level == -1) {
		const char *env = std::getenv("PDAL_DEBUG");
		level = env ? std::atoi(env) : 0;
	}
	return level;
}

#define PDAL_FILTER_DEBUG_LOG(level, fmt, ...)                                                                         \
	do {                                                                                                               \
		if (GetDebugLevel() >= level) {                                                                                \
			fprintf(stderr, "PDAL: " fmt "\n", ##__VA_ARGS__);                                                         \
		}                                                                                                              \
	} while (0)

namespace duckdb {

//======================================================================================================================
// Expression Encoding
//======================================================================================================================

bool FilterEncoder::GetComparisonOperator(ExpressionType type, std::string &out_operator) {
	switch (type) {
	case ExpressionType::COMPARE_EQUAL:
		out_operator = " == ";
		return true;
	case ExpressionType::COMPARE_NOTEQUAL:
		out_operator = " != ";
		return true;
	case ExpressionType::COMPARE_LESSTHAN:
		out_operator = " < ";
		return true;
	case ExpressionType::COMPARE_GREATERTHAN:
		out_operator = " > ";
		return true;
	case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		out_operator = " <= ";
		return true;
	case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
		out_operator = " >= ";
		return true;
	default:
		return false;
	}
}

std::string FilterEncoder::ValueToSQLLiteral(const Value &value, const LogicalType &type) {
	if (value.IsNull()) {
		return "NULL";
	}

	// PDAL only manages numeric types, we rely on the default string conversion which should produce a valid literal.
	switch (type.id()) {
	case LogicalTypeId::BOOLEAN:
		return value.GetValue<bool>() ? "1" : "0";
	default:
		return value.ToString();
	}
}

ExpressionEncodeResult FilterEncoder::EncodeColumnRef(const BoundColumnRefExpression &expr,
                                                      const ExpressionEncodeContext &ctx) {
	// Get the column binding - this contains the table index and column index
	const auto &binding = expr.binding;
	const auto &column_ids = ctx.column_ids;
	const auto &column_names = ctx.column_names;

	// Virtual/special column identifiers start at 2^63
	constexpr column_t VIRTUAL_COL_START = UINT64_C(9223372036854775808);

	// The column_index from binding refers to the projected column index
	// We need to map it through column_ids to get the actual table column index
	column_t projected_col_idx = binding.column_index;
	column_t table_col_idx;

	// Map from projected column index to actual table column index.
	if (column_ids.empty()) {
		// No projection - use filter index directly as table column index.
		table_col_idx = projected_col_idx;
	} else if (projected_col_idx >= column_ids.size()) {
		// Something's wrong, filter column index out of projected range.
		return {"", false};
	} else {
		// Map through column_ids to get actual table column index.
		table_col_idx = column_ids[projected_col_idx];
	}

	// Skip virtual/special columns.
	if (table_col_idx >= VIRTUAL_COL_START) {
		return {"", false};
	}
	// Filter column index out of projected range.
	if (table_col_idx >= column_names.size()) {
		return {"", false};
	}

	const std::string &col_name = column_names[table_col_idx];
	return {col_name, true};
}

ExpressionEncodeResult FilterEncoder::EncodeConstant(const BoundConstantExpression &expr) {
	std::string sql = ValueToSQLLiteral(expr.value, expr.return_type);
	return {sql, true};
}

ExpressionEncodeResult FilterEncoder::EncodeComparisonExpression(const BoundComparisonExpression &expr,
                                                                 const ExpressionEncodeContext &ctx) {
	// Get the comparison operator
	std::string op;
	if (!GetComparisonOperator(expr.type, op)) {
		return {"", false};
	}

	// Encode left and right sides
	auto left_result = EncodeExpression(*expr.left, ctx);
	if (!left_result.supported) {
		return {"", false};
	}

	auto right_result = EncodeExpression(*expr.right, ctx);
	if (!right_result.supported) {
		return {"", false};
	}

	std::string sql = left_result.sql + op + right_result.sql;
	return {sql, true};
}

ExpressionEncodeResult FilterEncoder::EncodeOperatorExpression(const BoundOperatorExpression &expr,
                                                               const ExpressionEncodeContext &ctx) {
	// Handle NOT operator
	if (expr.type == ExpressionType::OPERATOR_NOT) {
		if (expr.children.size() != 1) {
			return {"", false};
		}
		auto child_result = EncodeExpression(*expr.children[0], ctx);
		if (!child_result.supported) {
			return {"", false};
		}
		return {"!(" + child_result.sql + ")", true};
	}

	// Handle IN operator
	if (expr.type == ExpressionType::COMPARE_IN) {
		if (expr.children.size() < 2) {
			return {"", false};
		}
		auto left_result = EncodeExpression(*expr.children[0], ctx);
		if (!left_result.supported) {
			return {"", false};
		}

		std::string sql = "(";

		for (size_t i = 1; i < expr.children.size(); i++) {
			auto child_result = EncodeExpression(*expr.children[i], ctx);
			if (!child_result.supported) {
				return {"", false};
			}
			if (i > 1) {
				sql += " || ";
			}
			sql += "(" + left_result.sql + " == " + child_result.sql + ")";
		}
		sql += ")";

		return {sql, true};
	}

	// For other operators, we don't support them yet
	return {"", false};
}

ExpressionEncodeResult FilterEncoder::EncodeConjunctionExpression(const BoundConjunctionExpression &expr,
                                                                  const ExpressionEncodeContext &ctx) {
	if (expr.children.empty()) {
		return {"", false};
	}

	bool is_and = (expr.type == ExpressionType::CONJUNCTION_AND);
	std::string conj_op = is_and ? " && " : " || ";
	std::string sql;

	for (const auto &child : expr.children) {
		auto result = EncodeExpression(*child, ctx);

		if (!result.supported) {
			return {"", false};
		}
		sql += sql.empty() ? "(" + result.sql + ")" : conj_op + "(" + result.sql + ")";
	}

	if (sql.empty()) {
		return {"", false};
	}
	return {sql, true};
}

ExpressionEncodeResult FilterEncoder::EncodeBetweenExpression(const BoundBetweenExpression &expr,
                                                              const ExpressionEncodeContext &ctx) {
	if (expr.input->GetExpressionClass() != ExpressionClass::BOUND_COLUMN_REF ||
	    expr.lower->GetExpressionClass() != ExpressionClass::BOUND_CONSTANT ||
	    expr.upper->GetExpressionClass() != ExpressionClass::BOUND_CONSTANT) {
		return {"", false};
	}

	const auto &col_ref = EncodeColumnRef(expr.input->Cast<BoundColumnRefExpression>(), ctx);

	if (!col_ref.supported) {
		return {"", false};
	}

	const auto &lower_const = expr.lower->Cast<BoundConstantExpression>();
	const auto &upper_const = expr.upper->Cast<BoundConstantExpression>();

	std::string sql = "(" + col_ref.sql + " >= " + lower_const.value.ToString() + " && " + col_ref.sql +
	                  " <= " + upper_const.value.ToString() + ")";
	return {sql, true};
}

ExpressionEncodeResult FilterEncoder::EncodeExpression(const Expression &expr, const ExpressionEncodeContext &ctx) {
	switch (expr.GetExpressionClass()) {
	case ExpressionClass::BOUND_COLUMN_REF:
		return EncodeColumnRef(expr.Cast<BoundColumnRefExpression>(), ctx);

	case ExpressionClass::BOUND_CONSTANT:
		return EncodeConstant(expr.Cast<BoundConstantExpression>());

	case ExpressionClass::BOUND_COMPARISON:
		return EncodeComparisonExpression(expr.Cast<BoundComparisonExpression>(), ctx);

	case ExpressionClass::BOUND_OPERATOR:
		return EncodeOperatorExpression(expr.Cast<BoundOperatorExpression>(), ctx);

	case ExpressionClass::BOUND_CONJUNCTION:
		return EncodeConjunctionExpression(expr.Cast<BoundConjunctionExpression>(), ctx);

	case ExpressionClass::BOUND_BETWEEN:
		return EncodeBetweenExpression(expr.Cast<BoundBetweenExpression>(), ctx);

	default:
		PDAL_FILTER_DEBUG_LOG(1, "EncodeExpression: unsupported expression class %d", (int)expr.GetExpressionClass());
		return {"", false};
	}
}

FilterEncoderResult FilterEncoder::EncodeExpressions(const std::vector<unique_ptr<Expression>> &expressions,
                                                     const ExpressionEncodeContext &ctx) {
	FilterEncoderResult result;
	result.supported = true;

	// No expressions to encode.
	if (expressions.empty()) {
		result.supported = false;
		return result;
	}

	// Encode each expression.
	for (const auto &expr : expressions) {
		ExpressionEncodeResult expr_result = EncodeExpression(*expr, ctx);

		if (!expr_result.supported) {
			result.where_clause = "";
			result.supported = false;
			return result;
		}
		if (!expr_result.sql.empty()) {
			// Combine with existing where clause if needed.
			if (result.where_clause.empty()) {
				result.where_clause = "(" + expr_result.sql + ")";
			} else {
				result.where_clause += " && (" + expr_result.sql + ")";
			}
		}
	}

	PDAL_FILTER_DEBUG_LOG(1, "EncodeExpression: '%s'", result.where_clause.c_str());
	return result;
}

} // namespace duckdb
