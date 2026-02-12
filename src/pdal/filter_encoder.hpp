#pragma once

#include "duckdb.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/filter/in_filter.hpp"

namespace duckdb {

/**
 * Result of encoding a single expression or filter.
 */
struct ExpressionEncodeResult {
	std::string sql; // T-SQL fragment (empty if not supported)
	bool supported;  // True if expression was fully encoded
};

/**
 * Result of encoding an entire filter set.
 */
struct FilterEncoderResult {
	std::string where_clause; // Complete WHERE clause (without "WHERE" keyword)
	bool supported;           // True if expression was fully encoded
};

/**
 * Context for expression encoding, passed through recursive calls.
 */
struct ExpressionEncodeContext {
	const std::vector<column_t> &column_ids;      // Projection mapping
	const std::vector<std::string> &column_names; // All table column names
	const std::vector<LogicalType> &column_types; // All table column types

	ExpressionEncodeContext(const std::vector<column_t> &col_ids, const std::vector<std::string> &col_names,
	                        const std::vector<LogicalType> &col_types)
	    : column_ids(col_ids), column_names(col_names), column_types(col_types) {
	}
};

/**
 * Main filter encoder class.
 * Converts DuckDB filter expressions to T-SQL WHERE clauses.
 */
class FilterEncoder {
public:
	/**
	 * Encode a set of DuckDB Expressions to T-SQL.
	 */
	static FilterEncoderResult EncodeExpressions(const std::vector<unique_ptr<Expression>> &expressions,
	                                             const ExpressionEncodeContext &ctx);

private:
	/**
	 * Get T-SQL comparison operator for DuckDB ExpressionType.
	 */
	static bool GetComparisonOperator(ExpressionType type, std::string &out_operator);

	/**
	 * Convert DuckDB Value to T-SQL literal.
	 */
	static std::string ValueToSQLLiteral(const Value &value, const LogicalType &type);

	/**
	 * Encode a column reference.
	 */
	static ExpressionEncodeResult EncodeColumnRef(const BoundColumnRefExpression &expr,
	                                              const ExpressionEncodeContext &ctx);

	/**
	 * Encode a constant value.
	 */
	static ExpressionEncodeResult EncodeConstant(const BoundConstantExpression &expr);

	/**
	 * Encode a conjunction (AND/OR) expression.
	 */
	static ExpressionEncodeResult EncodeConjunctionExpression(const BoundConjunctionExpression &expr,
	                                                          const ExpressionEncodeContext &ctx);

	/**
	 * Encode a comparison expression (left OP right).
	 */
	static ExpressionEncodeResult EncodeComparisonExpression(const BoundComparisonExpression &expr,
	                                                         const ExpressionEncodeContext &ctx);

	/**
	 * Encode an operator expression (+, -, *, /, etc.).
	 */
	static ExpressionEncodeResult EncodeOperatorExpression(const BoundOperatorExpression &expr,
	                                                       const ExpressionEncodeContext &ctx);

	/**
	 * Encode a BETWEEN expression (input BETWEEN lower AND upper).
	 */
	static ExpressionEncodeResult EncodeBetweenExpression(const BoundBetweenExpression &expr,
	                                                      const ExpressionEncodeContext &ctx);

	/**
	 * Encode a DuckDB Expression to T-SQL.
	 */
	static ExpressionEncodeResult EncodeExpression(const Expression &expr, const ExpressionEncodeContext &ctx);
};

} // namespace duckdb
