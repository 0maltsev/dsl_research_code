#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_resolve(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  const auto resolved = resolve_module(module);
  BOUNDFIN_CHECK(resolved.ok);
  return module;
}

// T-Fold: "admissible count for N; initial/body:tau_x under i:idx(N),x:tau_x"
// -> "fold:tau_x". The index binder (i) types as i32 ("acc + i" requires it).
void TFold_result_type_is_the_accumulator_type() {
  auto module = parse_and_resolve(
      "fn f(): i32 = fold<3>(i32bits(0x00000003); acc, i; i32bits(0x00000000); acc + i);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

void TYP010_fold_body_type_must_match_accumulator_type() {
  auto module = parse_and_resolve(
      "fn f(): i32 = fold<3>(i32bits(0x00000003); acc, i; i32bits(0x00000000); true);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP010"));
}

// AM-018 / this module's header comment: `count` is typed by the separate
// |-cnt judgment owned by src/source/size (Phase 3.3+), not by this
// module's |- e : tau judgment. A count expression of the "wrong" type
// (here, bool instead of i32) must not block typecheck, and must be left
// with no inferred_type of its own.
void Fold_count_expression_is_not_typechecked_here() {
  auto module =
      parse_and_resolve("fn f(): i32 = fold<3>(true; acc, i; i32bits(0x00000000); acc);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(!fold_expr.count->inferred_type.has_value());
}

// T-Build: "admissible count for N; body:tau under i:idx(N)" -> "builder: arr<tau, N>".
void TBuild_result_type_is_arr_of_body_type_and_declared_capacity() {
  auto module = parse_and_resolve("fn f(): arr<i32, 3> = build<3>(i32bits(0x00000003); i; i);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &type = *module.functions[0].body->inferred_type;
  BOUNDFIN_CHECK(type->kind == TypeKind::Array);
  const auto &array_type = std::get<ArrayType>(type->data);
  BOUNDFIN_CHECK(array_type.element->kind == TypeKind::I32);
  BOUNDFIN_CHECK_EQ(array_type.capacity, static_cast<std::uint32_t>(3));
}

void Build_count_expression_is_not_typechecked_here() {
  auto module = parse_and_resolve("fn f(): arr<i32, 3> = build<3>(true; i; i);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &build_expr = std::get<BuildExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(!build_expr.count->inferred_type.has_value());
}

void boundfin_typecheck_fold_build() {
  TFold_result_type_is_the_accumulator_type();
  TYP010_fold_body_type_must_match_accumulator_type();
  Fold_count_expression_is_not_typechecked_here();
  TBuild_result_type_is_arr_of_body_type_and_declared_capacity();
  Build_count_expression_is_not_typechecked_here();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_fold_build)
