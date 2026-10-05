#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;

const Expr &function_body(const Module &module) { return *module.functions[0].body; }

void let_expression_binds_name_and_evaluates_body() {
  const auto result =
      parse_module("fn f(x: i32): i32 = let y = x + i32bits(0x00000001) in y;\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &let_expr = std::get<LetExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(let_expr.name, std::string("y"));
  BOUNDFIN_CHECK(let_expr.bound->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(let_expr.body->kind == ExprKind::Var);
}

void if_expression_has_condition_then_else() {
  const auto result = parse_module("fn f(a: i32, b: i32): i32 = if a < b then a else b;\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &if_expr = std::get<IfExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK(if_expr.condition->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(if_expr.then_branch->kind == ExprKind::Var);
  BOUNDFIN_CHECK(if_expr.else_branch->kind == ExprKind::Var);
}

// cost-trace-semantics.md "Fold": count-bounded, second binder is a 0-based
// loop index, not an array element.
void fold_expression_fields_in_grammar_order() {
  const auto result =
      parse_module("fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); acc + idx);\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &fold_expr = std::get<FoldExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(fold_expr.capacity, std::uint32_t{16});
  BOUNDFIN_CHECK(fold_expr.count->kind == ExprKind::Var);
  BOUNDFIN_CHECK_EQ(fold_expr.accumulator_name, std::string("acc"));
  BOUNDFIN_CHECK_EQ(fold_expr.index_name, std::string("idx"));
  BOUNDFIN_CHECK(fold_expr.initial->kind == ExprKind::Literal);
  BOUNDFIN_CHECK(fold_expr.body->kind == ExprKind::BinaryPrimitive);
}

void build_expression_fields_in_grammar_order() {
  const auto result = parse_module("fn f(n: i32): arr<i32, 4> = build<4>(n; idx; idx * n);\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &build_expr = std::get<BuildExpr>(function_body(*result.module).data);
  BOUNDFIN_CHECK_EQ(build_expr.capacity, std::uint32_t{4});
  BOUNDFIN_CHECK(build_expr.count->kind == ExprKind::Var);
  BOUNDFIN_CHECK_EQ(build_expr.index_name, std::string("idx"));
  BOUNDFIN_CHECK(build_expr.body->kind == ExprKind::BinaryPrimitive);
}

void SYN006_malformed_fold_and_build_punctuation() {
  // Missing "," between the two fold binders.
  {
    const auto result =
        parse_module("fn f(n: i32): i32 = fold<16>(n; acc idx; i32bits(0x00000000); acc);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN006"));
  }
  // Missing "<" before the fold capacity (note the required space: maximal
  // munch would otherwise lex "fold16" as one identifier, not the keyword
  // "fold" followed by "16").
  {
    const auto result = parse_module("fn f(n: i32): i32 = fold 16>(n; acc, idx; i32bits(0x00000000); acc);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN006"));
  }
  // Missing ";" separating build's count from its index binder.
  {
    const auto result = parse_module("fn f(n: i32): arr<i32, 4> = build<4>(n idx; idx);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN006"));
  }
}

void let_if_fold_build_require_parens_as_operator_operands() {
  // grammar.ebnf: operator operands are primary-expression-rooted, which
  // does not include let/if/fold/build directly.
  const auto result = parse_module("fn f(x: i32): i32 = x + let y = x in y;\nexport f;\n");
  BOUNDFIN_CHECK(!result.ok);
  // "let" is a reserved word appearing where a primary expression (an
  // additive-expression operand) is required.
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));

  const auto parenthesized = parse_module("fn f(x: i32): i32 = x + (let y = x in y);\nexport f;\n");
  BOUNDFIN_CHECK(parenthesized.ok);
}

// SYN002 ("expected token missing") is the generic diagnostic Parser::expect()
// raises at roughly 15 call sites outside the productions SYN005/SYN006/SYN007
// own specifically (let's "="/"in", if's "then"/"else", postfix "]", call/
// len/proj/abs/bit-literal parens, proj's "<"/">"). A spec-auditor review
// found this code had no test anywhere in the suite despite STATUS.md
// claiming coverage; these cover it at a representative sample of those
// sites, not just one.
void SYN002_missing_required_token() {
  // let missing "in".
  {
    const auto result = parse_module("fn f(x: i32): i32 = let y = x then y;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN002"));
  }
  // if missing "then".
  {
    const auto result = parse_module("fn f(a: i32, b: i32): i32 = if a < b a else b;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN002"));
  }
  // if missing "else".
  {
    const auto result = parse_module("fn f(a: i32, b: i32): i32 = if a < b then a b;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN002"));
  }
  // len missing the closing ")".
  {
    const auto result = parse_module("fn f(xs: arr<i32, 4>): i32 = len(xs;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN002"));
  }
  // postfix index missing the closing "]".
  {
    const auto result = parse_module("fn f(xs: arr<i32, 4>): i32 = xs[i32bits(0x00000000);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN002"));
  }
}

void boundfin_parse_let_if_fold_build() {
  let_expression_binds_name_and_evaluates_body();
  if_expression_has_condition_then_else();
  fold_expression_fields_in_grammar_order();
  build_expression_fields_in_grammar_order();
  SYN006_malformed_fold_and_build_punctuation();
  let_if_fold_build_require_parens_as_operator_operands();
  SYN002_missing_required_token();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_let_if_fold_build)
