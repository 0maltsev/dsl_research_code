#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;

const Expr &function_body(const Module &module) { return *module.functions[0].body; }

void array_literal_elements_and_capacity() {
  const auto result =
      parse_module("fn f(): arr<i32, 3> = array<3>[i32bits(0x00000001), i32bits(0x00000002), i32bits(0x00000003)];\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &array_literal = std::get<ArrayLiteralExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(array_literal.capacity, std::uint32_t{3});
  BOUNDFIN_CHECK_EQ(array_literal.elements.size(), std::size_t{3});
}

void empty_array_literal_is_valid() {
  const auto result = parse_module("fn f(): arr<i32, 4> = array<4>[];\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(std::get<ArrayLiteralExpr>(function_body(*result.module).data).elements.empty());
}

// AM-018 (author-approved): whether an array literal's length exceeds its
// declared capacity (grammar.ebnf "Static concrete-syntax constraints" item
// 6; diagnostics-and-status.md SIZ002) is deferred entirely to
// src/source/size (Phase 3), not checked by this parser, since
// docs/architecture.md names src/source/size as owning size/count
// decisions. This parser accepts the literal's AST shape regardless of
// whether its element count exceeds its capacity.
void array_literal_length_exceeding_capacity_is_not_checked_by_the_parser() {
  const auto result =
      parse_module("fn f(): arr<i32, 2> = array<2>[i32bits(0x00000001), i32bits(0x00000002), i32bits(0x00000003)];\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &array_literal = std::get<ArrayLiteralExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(array_literal.capacity, std::uint32_t{2});
  BOUNDFIN_CHECK_EQ(array_literal.elements.size(), std::size_t{3});
}

void SYN006_malformed_array_literal() {
  // Missing "]".
  const auto result = parse_module("fn f(): arr<i32, 1> = array<1>[i32bits(0x00000001);\nexport f;\n");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN006"));
}

void product_expression_arity_at_least_two() {
  const auto result = parse_module("fn f(): prod<i32, i32, i32> = "
                                    "(i32bits(0x00000001), i32bits(0x00000002), i32bits(0x00000003));\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &product = std::get<ProductExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(product.components.size(), std::size_t{3});
}

// AM-002: "grouping parentheses are not products" -- a single parenthesized
// expression (no comma) is transparent, not a Product AST node.
void single_parenthesized_expression_is_transparent_grouping() {
  const auto result = parse_module("fn f(): i32 = (i32bits(0x00000001));\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(function_body(*result.module).kind == ExprKind::Literal);
}

void projection_one_based_index() {
  const auto result =
      parse_module("fn f(p: prod<i32, i32, i32>): i32 = proj<3>(p);\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &proj_expr = std::get<ProjExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(proj_expr.index, std::uint32_t{3});
  BOUNDFIN_CHECK(proj_expr.operand->kind == ExprKind::Var);
}

// grammar.ebnf `positive-decimal` excludes a standalone "0" syntactically;
// whether a nonzero index is within an operand's actual arity is TYP007, a
// later (Phase 3) semantic check this parser does not and cannot perform.
void zero_projection_index_is_rejected_syntactically() {
  const auto result = parse_module("fn f(p: prod<i32, i32>): i32 = proj<0>(p);\nexport f;\n");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN001"));
}

void index_expression_postfix_and_chainable() {
  const auto result =
      parse_module("fn f(xs: arr<arr<i32, 4>, 4>): i32 = xs[i32bits(0x00000000)][i32bits(0x00000001)];\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &outer_index = std::get<IndexExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK(outer_index.array->kind == ExprKind::Index);
}

void length_expression() {
  const auto result = parse_module("fn f(xs: arr<i32, 4>): i32 = len(xs);\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &len_expr = std::get<LenExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK(len_expr.array->kind == ExprKind::Var);
}

void boundfin_parse_arrays_products_proj_index_len() {
  array_literal_elements_and_capacity();
  empty_array_literal_is_valid();
  array_literal_length_exceeding_capacity_is_not_checked_by_the_parser();
  SYN006_malformed_array_literal();
  product_expression_arity_at_least_two();
  single_parenthesized_expression_is_transparent_grouping();
  projection_one_based_index();
  zero_projection_index_is_rejected_syntactically();
  index_expression_postfix_and_chainable();
  length_expression();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_arrays_products_proj_index_len)
