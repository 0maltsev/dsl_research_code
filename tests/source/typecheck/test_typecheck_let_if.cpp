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

// T-Let: "e1:tau1; under x:tau1, e2:tau2" -> "let x = e1 in e2 : tau2".
void TLet_result_type_is_the_body_type() {
  auto module = parse_and_resolve("fn f(): i64 = let x = i32bits(0x00000001) in i64bits(0x0000000000000002);\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &let_expr = std::get<LetExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK((*let_expr.bound->inferred_type)->kind == TypeKind::I32);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I64);
}

// T-If: "guard:bool; both branches:tau" -> "conditional:tau".
void TIf_result_type_is_the_shared_branch_type() {
  auto module = parse_and_resolve("fn f(): i32 = if true then i32bits(0x00000001) else i32bits(0x00000002);\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

void TYP004_if_guard_must_be_bool() {
  auto module =
      parse_and_resolve("fn f(): i32 = if i32bits(0x00000001) then i32bits(0x00000001) else i32bits(0x00000002);\n"
                         "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP004"));
}

void TYP005_if_branches_must_share_a_type() {
  auto module = parse_and_resolve(
      "fn f(): i32 = if true then i32bits(0x00000001) else i64bits(0x0000000000000002);\n"
      "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP005"));
}

void boundfin_typecheck_let_if() {
  TLet_result_type_is_the_body_type();
  TIf_result_type_is_the_shared_branch_type();
  TYP004_if_guard_must_be_bool();
  TYP005_if_branches_must_share_a_type();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_let_if)
