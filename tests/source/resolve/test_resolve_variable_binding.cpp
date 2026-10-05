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

void parameter_reference_resolves_to_its_binding() {
  auto module = parse_ok("fn f(x: i32): i32 = x;\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());
  const auto &var = std::get<VarExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(var.resolved_binding.has_value());
  BOUNDFIN_CHECK_EQ(*var.resolved_binding, *param.binding);
}

void NAM002_duplicate_parameter_name() {
  auto module = parse_ok("fn f(x: i32, x: i32): i32 = x;\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM002"));
}

void NAM003_unbound_variable() {
  auto module = parse_ok("fn f(): i32 = y;\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM003"));
}

// diagnostics-and-status.md: "Lexical shadowing by let/fold/build is
// accepted and alpha-renamed; it is not NAM002 unless the same binding list
// duplicates a name." A let shadowing an outer parameter of the same name
// must succeed, and the two bindings must get *different* BindingIds, with
// each variable reference resolving to the lexically nearest one.
void let_shadowing_an_outer_binding_is_accepted_and_alpha_renamed() {
  // fn f(x: i32): i32 = let x = x in x;
  // The bound expression's "x" is the parameter; the body's "x" is the let.
  auto module = parse_ok("fn f(x: i32): i32 = let x = x in x;\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);

  const auto &param = module.functions[0].parameters[0];
  const auto &let_expr = std::get<LetExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(let_expr.binding.has_value());
  BOUNDFIN_CHECK(*let_expr.binding != *param.binding); // alpha-renamed: distinct identity

  const auto &bound_var = std::get<VarExpr>(let_expr.bound->data); // "x" in "= x in"
  BOUNDFIN_CHECK_EQ(*bound_var.resolved_binding, *param.binding);

  const auto &body_var = std::get<VarExpr>(let_expr.body->data); // "x" in "in x"
  BOUNDFIN_CHECK_EQ(*body_var.resolved_binding, *let_expr.binding);
}

void nested_let_shadowing_resolves_innermost_first() {
  // let x = i32bits(0) in let x = i32bits(1) in x  -- the body's "x" is the
  // *inner* let, not the outer one or the parameter.
  auto module =
      parse_ok("fn f(x: i32): i32 = let x = i32bits(0x00000000) in let x = i32bits(0x00000001) in x;\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);

  const auto &outer_let = std::get<LetExpr>(module.functions[0].body->data);
  const auto &inner_let = std::get<LetExpr>(outer_let.body->data);
  const auto &body_var = std::get<VarExpr>(inner_let.body->data);
  BOUNDFIN_CHECK_EQ(*body_var.resolved_binding, *inner_let.binding);
  BOUNDFIN_CHECK(*body_var.resolved_binding != *outer_let.binding);
}

void let_scope_does_not_leak_past_its_body() {
  // fn f(): i32 = let x = i32bits(0x00000001) in x;  -- "x" must not be
  // visible to a *sibling* function.
  auto module = parse_ok("fn f(): i32 = let x = i32bits(0x00000001) in x;\n"
                          "fn g(): i32 = x;\n"
                          "export f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM003"));
}

void boundfin_resolve_variable_binding() {
  parameter_reference_resolves_to_its_binding();
  NAM002_duplicate_parameter_name();
  NAM003_unbound_variable();
  let_shadowing_an_outer_binding_is_accepted_and_alpha_renamed();
  nested_let_shadowing_resolves_innermost_first();
  let_scope_does_not_leak_past_its_body();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_variable_binding)
