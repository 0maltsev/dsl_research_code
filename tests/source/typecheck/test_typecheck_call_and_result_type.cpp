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

// T-Call: "earlier f:tau_vec -> tau; ordered actual types agree" -> "f(e_vec):tau".
void TCall_result_type_is_the_callees_declared_result_type() {
  auto module = parse_and_resolve("fn g(x: i32): i64 = i64bits(0x0000000000000001);\n"
                                   "fn f(): i64 = g(i32bits(0x00000001));\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[1].body->inferred_type)->kind == TypeKind::I64);
}

void TYP002_call_arity_mismatch_is_rejected() {
  auto module = parse_and_resolve("fn g(x: i32): i32 = x;\n"
                                   "fn f(): i32 = g();\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP002"));
}

void TYP002_call_argument_type_mismatch_is_rejected() {
  auto module = parse_and_resolve("fn g(x: i32): i32 = x;\n"
                                   "fn f(): i32 = g(true);\n"
                                   "export f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP002"));
}

// AM-021: a function body's synthesized type must equal its declared result
// type; Table 7 does not name this as one of "every source expression
// form," so this module checks it directly and raises TYP003 (chosen over
// TYP002, which covers call-site arity/type per T-Call above).
void TYP003_function_body_type_must_match_declared_result_type() {
  auto module = parse_and_resolve("fn f(): i32 = true;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP003"));
}

void function_body_matching_declared_result_type_is_accepted() {
  auto module = parse_and_resolve("fn f(): bool = true;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
}

void boundfin_typecheck_call_and_result_type() {
  TCall_result_type_is_the_callees_declared_result_type();
  TYP002_call_arity_mismatch_is_rejected();
  TYP002_call_argument_type_mismatch_is_rejected();
  TYP003_function_body_type_must_match_declared_result_type();
  function_body_matching_declared_result_type_is_accepted();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_call_and_result_type)
