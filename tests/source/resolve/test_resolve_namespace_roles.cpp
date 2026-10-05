#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::Module;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;

Module parse_ok(const std::string &source) {
  auto result = parse_module(source);
  BOUNDFIN_CHECK(result.ok);
  return std::move(*result.module);
}

// NAM007 ("Variable/function namespace used in the wrong syntactic role"):
// a bare identifier must be a variable, and a call target must be a
// function -- the two namespaces are disjoint, and grammar.ebnf's
// name-expression syntax (identifier, with call parens optional) means the
// same spelling can plausibly be either, so this must be checked.
void NAM007_calling_a_variable() {
  auto module = parse_ok("fn f(x: i32): i32 = x();\nexport f;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM007"));
}

void NAM007_using_a_function_name_as_a_bare_variable() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\nfn g(): i32 = f;\nexport g;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM007"));
}

// A let-bound name happens to share a function's spelling: the bare
// reference still means the *variable* (the nearer, lexically-scoped
// binding), not NAM007 -- namespace-role confusion only arises when there
// is no variable binding to find at all.
void shadowing_a_function_name_with_a_variable_is_not_a_namespace_error() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\n"
                          "fn g(): i32 = let f = i32bits(0x00000002) in f;\n"
                          "export g;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);
}

void boundfin_resolve_namespace_roles() {
  NAM007_calling_a_variable();
  NAM007_using_a_function_name_as_a_bare_variable();
  shadowing_a_function_name_with_a_variable_is_not_a_namespace_error();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_namespace_roles)
