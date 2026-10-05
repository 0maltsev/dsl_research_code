#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;

Module parse_ok(const std::string &source) {
  auto result = parse_module(source);
  BOUNDFIN_CHECK(result.ok);
  return std::move(*result.module);
}

void fold_accumulator_and_index_resolve_distinctly() {
  auto module =
      parse_ok("fn f(n: i32): i32 = fold<8>(n; acc, i; i32bits(0x00000000); acc + i);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);

  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.accumulator_binding.has_value());
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());
  BOUNDFIN_CHECK(*fold_expr.accumulator_binding != *fold_expr.index_binding);

  const auto &add = std::get<BinaryPrimitiveExpr>(fold_expr.body->data);
  BOUNDFIN_CHECK_EQ(*std::get<VarExpr>(add.lhs->data).resolved_binding, *fold_expr.accumulator_binding);
  BOUNDFIN_CHECK_EQ(*std::get<VarExpr>(add.rhs->data).resolved_binding, *fold_expr.index_binding);
}

void fold_binders_are_not_visible_in_count_or_initial() {
  // The count and initial-accumulator expressions are evaluated before the
  // accumulator/index are bound (cost-trace-semantics.md "Fold": "the count
  // and initial accumulator evaluate left-to-right"), so neither may refer
  // to them.
  {
    auto result_module =
        parse_module("fn f(n: i32): i32 = fold<8>(acc; acc, i; i32bits(0x00000000); acc);\nexport f;\n");
    BOUNDFIN_CHECK(result_module.ok);
    auto module = std::move(*result_module.module);
    const auto result = resolve_module(module);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM003"));
  }
  {
    auto module = parse_ok("fn f(n: i32): i32 = fold<8>(n; acc, i; i; acc);\nexport f;\n");
    const auto result = resolve_module(module);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM003"));
  }
}

void NAM002_fold_duplicate_binder_name() {
  auto module = parse_ok("fn f(n: i32): i32 = fold<8>(n; x, x; i32bits(0x00000000); x);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM002"));
}

void build_index_resolves() {
  auto module = parse_ok("fn f(n: i32): arr<i32, 4> = build<4>(n; i; i * n);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);

  const auto &build_expr = std::get<BuildExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(build_expr.index_binding.has_value());
  const auto &mul = std::get<BinaryPrimitiveExpr>(build_expr.body->data);
  BOUNDFIN_CHECK_EQ(*std::get<VarExpr>(mul.lhs->data).resolved_binding, *build_expr.index_binding);
}

void build_index_not_visible_in_count() {
  auto module_result = parse_module("fn f(n: i32): arr<i32, 4> = build<4>(i; i; i);\nexport f;\n");
  BOUNDFIN_CHECK(module_result.ok);
  auto module = std::move(*module_result.module);
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM003"));
}

void boundfin_resolve_binders() {
  fold_accumulator_and_index_resolve_distinctly();
  fold_binders_are_not_visible_in_count_or_initial();
  NAM002_fold_duplicate_binder_name();
  build_index_resolves();
  build_index_not_visible_in_count();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_binders)
