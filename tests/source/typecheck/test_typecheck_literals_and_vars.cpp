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

// T-Var/T-Const: "Gamma(x) = tau; literal has declared kind".

void TConst_bool_literal_types_as_bool() {
  auto module = parse_and_resolve("fn f(): bool = true;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &body_type = *module.functions[0].body->inferred_type;
  BOUNDFIN_CHECK(body_type->kind == TypeKind::Bool);
}

void TConst_i32_literal_types_as_i32() {
  auto module = parse_and_resolve("fn f(): i32 = i32bits(0x00000007);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

void TConst_i64_literal_types_as_i64() {
  auto module = parse_and_resolve("fn f(): i64 = i64bits(0x0000000000000007);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I64);
}

void TConst_f64_literal_types_as_f64() {
  auto module = parse_and_resolve("fn f(): f64 = f64bits(0x3ff0000000000000);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::F64);
}

// T-Var: a parameter reference types as the parameter's declared type.
void TVar_parameter_reference_types_as_its_declared_type() {
  auto module = parse_and_resolve("fn f(x: i32): i32 = x;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

// T-Var: a let-bound variable types as its bound expression's type.
void TVar_let_bound_reference_types_as_bound_expression_type() {
  auto module = parse_and_resolve("fn f(): f64 = let x = f64bits(0x0000000000000000) in x;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::F64);
}

void boundfin_typecheck_literals_and_vars() {
  TConst_bool_literal_types_as_bool();
  TConst_i32_literal_types_as_i32();
  TConst_i64_literal_types_as_i64();
  TConst_f64_literal_types_as_f64();
  TVar_parameter_reference_types_as_its_declared_type();
  TVar_let_bound_reference_types_as_bound_expression_type();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_literals_and_vars)
