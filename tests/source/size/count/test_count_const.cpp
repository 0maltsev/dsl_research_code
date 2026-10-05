#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using namespace boundfin::source::size::certificate;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::count::infer_count;
using boundfin::source::typecheck::typecheck_module;

// Parses, resolves, and typechecks `source`, returning the module. The
// count module's contract expects an already-typechecked AST, even
// though this milestone's two implemented rules (C-Const, C-If) don't
// themselves read `inferred_type`.
Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

// C-Const: "literal signed value n in [0, 2^31-1]" -> (n,n;nu) with nu=n.

void CConst_zero_is_exact() {
  auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x00000000);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(0)));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(0)));
}

void CConst_max_i32_is_exact() {
  // 2^31-1 = 0x7fffffff, the largest nonnegative signed i32.
  auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x7fffffff);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(2147483647)));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(2147483647)));
}

void CConst_ordinary_value_is_exact() {
  auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x0000002a);\nexport f;\n"); // 42
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(42)));
}

// A negative i32 literal (two's-complement sign bit set) fails C-Const's
// "n in [0, 2^31-1]" premise; no other Table 6 rule applies to a bare
// literal, so this is SIZ003.
void SIZ003_negative_i32_literal_is_rejected() {
  // 0x80000000 = INT32_MIN, 0xffffffff = -1.
  auto module = parse_and_typecheck("fn f(): i32 = i32bits(0xffffffff);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// Non-i32 literals (bool/i64/f64) are not covered by any Table 6 rule.
void SIZ003_non_i32_literal_is_rejected() {
  auto module = parse_and_typecheck("fn f(): bool = true;\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// Table 6 has no rule for a bare variable reference (no C-Var/C-Let): "No
// other expression derives a count refinement" (main.pdf p.37).
void SIZ003_bare_variable_reference_is_rejected() {
  auto module = parse_and_typecheck("fn f(x: i32): i32 = x;\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

void boundfin_count_const() {
  CConst_zero_is_exact();
  CConst_max_i32_is_exact();
  CConst_ordinary_value_is_exact();
  SIZ003_negative_i32_literal_is_rejected();
  SIZ003_non_i32_literal_is_rejected();
  SIZ003_bare_variable_reference_is_rejected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_const)
