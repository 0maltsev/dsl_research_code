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

void call_to_an_earlier_function_resolves_to_its_rank() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\nfn g(): i32 = f();\nexport g;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &call = std::get<CallExpr>(module.functions[1].body->data); // g's body
  BOUNDFIN_CHECK(call.resolved_callee_rank.has_value());
  BOUNDFIN_CHECK_EQ(*call.resolved_callee_rank, std::size_t{0}); // f's rank
}

void NAM001_duplicate_function_declaration() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\nfn f(): i32 = i32bits(0x00000002);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM001"));
}

void NAM004_unknown_function() {
  auto module = parse_ok("fn f(): i32 = g();\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM004"));
}

// paper Sec. 4.1: "The body of g may call only f with f declared strictly
// before g; hence the call graph is acyclic."
void NAM006_forward_reference_to_a_later_function() {
  auto module = parse_ok("fn f(): i32 = g();\nfn g(): i32 = i32bits(0x00000001);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM006"));
}

// EFF004 ("Recursion or cyclic call graph") is reachable, in this frozen
// grammar, by exactly one shape: a function calling itself -- genuine
// multi-function cycles are structurally impossible once every call must
// target a strictly-earlier declaration (see test above).
void EFF004_direct_self_recursion() {
  auto module = parse_ok("fn f(x: i32): i32 = f(x);\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("EFF004"));
}

void chain_of_calls_to_strictly_earlier_functions_is_accepted() {
  auto module = parse_ok("fn a(): i32 = i32bits(0x00000001);\n"
                          "fn b(): i32 = a();\n"
                          "fn c(): i32 = b();\n"
                          "export c;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(*std::get<CallExpr>(module.functions[1].body->data).resolved_callee_rank, std::size_t{0});
  BOUNDFIN_CHECK_EQ(*std::get<CallExpr>(module.functions[2].body->data).resolved_callee_rank, std::size_t{1});
}

void boundfin_resolve_function_table() {
  call_to_an_earlier_function_resolves_to_its_rank();
  NAM001_duplicate_function_declaration();
  NAM004_unknown_function();
  NAM006_forward_reference_to_a_later_function();
  EFF004_direct_self_recursion();
  chain_of_calls_to_strictly_earlier_functions_is_accepted();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_function_table)
