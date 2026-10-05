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

// A small multi-function module exercising T-Call, T-Let, T-If, and T-Prim
// (Add/Gt) together, confirming every reached Expr node ends up with an
// inferred_type and every primitive ends up monomorphized.
void full_module_with_call_let_if_and_primitives_typechecks_end_to_end() {
  auto module = parse_and_resolve("fn double(x: i32): i32 = x + x;\n"
                                   "fn f(n: i32): i32 =\n"
                                   "  let y = double(n) in\n"
                                   "  if y > i32bits(0x00000000) then y else i32bits(0x00000000);\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);

  const auto &double_body = *module.functions[0].body;
  BOUNDFIN_CHECK(double_body.inferred_type.has_value());
  BOUNDFIN_CHECK((*double_body.inferred_type)->kind == TypeKind::I32);
  const auto &add_expr = std::get<BinaryPrimitiveExpr>(double_body.data);
  BOUNDFIN_CHECK(add_expr.resolved_primitive.has_value());
  BOUNDFIN_CHECK(*add_expr.resolved_primitive == MonomorphicPrimitive::Add32);

  const auto &f_body = *module.functions[1].body;
  BOUNDFIN_CHECK((*f_body.inferred_type)->kind == TypeKind::I32);
  const auto &let_expr = std::get<LetExpr>(f_body.data);
  BOUNDFIN_CHECK((*let_expr.bound->inferred_type)->kind == TypeKind::I32);
  const auto &if_expr = std::get<IfExpr>(let_expr.body->data);
  const auto &gt_expr = std::get<BinaryPrimitiveExpr>(if_expr.condition->data);
  BOUNDFIN_CHECK(*gt_expr.resolved_primitive == MonomorphicPrimitive::GtS32);
}

// Lexical shadowing with a *different* type: resolve's alpha-renaming gives
// the let-bound "x" a distinct BindingId from the parameter "x", so
// typecheck's binding-id-keyed context must not confuse the two types.
void shadowing_with_a_different_type_typechecks_correctly() {
  auto module = parse_and_resolve("fn f(x: i32): bool = let x = true in x;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::Bool);
}

void boundfin_typecheck_full_module() {
  full_module_with_call_let_if_and_primitives_typechecks_end_to_end();
  shadowing_with_a_different_type_typechecks_correctly();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_full_module)
