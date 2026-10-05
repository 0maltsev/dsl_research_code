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

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

// C-Add: "operands (lambda_i,u_i;nu_i); certificate NW_+" ->
// (lambda1+lambda2,u1+u2;nu) if both exact, else (star,u1+u2;nu).

void CAdd_two_exact_operands_combine_to_an_exact_sum() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = i32bits(0x0000002a) + i32bits(0x00000001);\nexport f;\n"); // 42 + 1
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_add(make_literal(42), make_literal(1))));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_add(make_literal(42), make_literal(1))));
}

// SIZ006 when the required no-wrap certificate (u1+u2 <= 2^31-1) fails:
// two i32-representable literals whose closed sum still exceeds 2^31-1
// (2^31-2 + 2^31-2 = 2^32-4, far over the count-fragment's bound).
void SIZ006_add_rejects_when_the_sum_would_exceed_the_i32_bound() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = i32bits(0x7ffffffe) + i32bits(0x7ffffffe);\nexport f;\n"); // (2^31-2) + (2^31-2)
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ006"));
}

void SIZ003_propagates_from_an_unrefinable_add_operand() {
  auto module = parse_and_typecheck("fn f(x: i32): i32 = x + i32bits(0x00000001);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// A BinaryPrimitiveExpr whose operator is neither Add nor Sub (Table 6
// has no rule for any other binary primitive) falls through to the
// generic SIZ003, not infer_add/infer_sub's own operand-level checks.
void SIZ003_other_binary_operators_have_no_accepted_rule() {
  auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x00000002) * i32bits(0x00000003);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// C-Sub: "operands (lambda_i,u_i;nu_i); certificate NW_-" ->
// (lambda1-lambda2,u1;nu) if both exact, else (star,u1;nu). AM-024: the
// exact case only applies when both operands are closed; here both are
// (plain literals), so the subtraction reduces to a fresh Literal.

void CSub_two_closed_exact_operands_reduce_to_a_literal_difference() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = i32bits(0x0000002a) - i32bits(0x00000001);\nexport f;\n"); // 42 - 1
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(41)));
  // C-Sub's upper bound is simply u1 (not a combined term).
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(42)));
}

// AM-025: the required no-wrap certificate is "u2 <=cert lambda1" (the
// minuend's *exact* component, not its upper bound -- see the AM-025
// regression test below for why u1 alone is unsound). Here lambda1=1,
// u2=42: 42<=1 fails, so this is rejected regardless of u1's own value.
void SIZ006_sub_rejects_when_the_subtrahends_upper_bound_exceeds_the_minuends_exact_value() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = i32bits(0x00000001) - i32bits(0x0000002a);\nexport f;\n"); // 1 - 42
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ006"));
}

// AM-025: C-Sub requires the minuend's exact component (lambda1) to be
// present at all -- if the minuend is itself inexact (star), there is no
// sound lower bound on its real value to certify the subtrahend's upper
// bound against, so C-Sub is rejected outright (not merely falling back
// to an inexact result, which the pre-AM-025 implementation wrongly did).
void SIZ006_sub_rejects_when_the_minuend_is_not_exact() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = (if true then i32bits(0x00000001) else i32bits(0x00000002)) - i32bits(0x00000001);\n"
      "export f;\n"); // (1 or 2, inexact) - 1
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ006"));
}

// The subtrahend may still be inexact (star) as long as the minuend is
// exact and the required certificate holds: Table 6's C-Sub "else" branch
// (operands not both exact) remains reachable under AM-025, it just needs
// lambda1 present -- unlike the subtrahend, which this case leaves
// inexact via C-If-U's "branches differ" disjunct.
void CSub_with_an_inexact_subtrahend_still_succeeds_when_the_minuend_is_exact() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = i32bits(0x0000002a) - (if true then i32bits(0x00000001) else i32bits(0x00000002));\n"
      "export f;\n"); // 42 - (1 or 2, inexact)
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(!outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(42)));
}

// AM-025 regression test: the exact counterexample a spec-auditor review
// found and this session independently reproduced against the pre-fix
// build ((10-9)-2 = 1-2 = -1, a deterministic underflow that the pre-fix
// "u2<=u1" certificate wrongly accepted, since u1 stayed loose at 10 even
// though the inner subtraction's real value was 1). Must now be rejected.
void SIZ006_nested_sub_rejects_a_deterministic_underflow() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = (i32bits(0x0000000a) - i32bits(0x00000009)) - i32bits(0x00000002);\nexport f;\n"); // (10-9)-2
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ006"));
}

void boundfin_count_add_sub() {
  CAdd_two_exact_operands_combine_to_an_exact_sum();
  SIZ006_add_rejects_when_the_sum_would_exceed_the_i32_bound();
  SIZ003_propagates_from_an_unrefinable_add_operand();
  SIZ003_other_binary_operators_have_no_accepted_rule();
  CSub_two_closed_exact_operands_reduce_to_a_literal_difference();
  SIZ006_sub_rejects_when_the_subtrahends_upper_bound_exceeds_the_minuends_exact_value();
  SIZ006_sub_rejects_when_the_minuend_is_not_exact();
  CSub_with_an_inexact_subtrahend_still_succeeds_when_the_minuend_is_exact();
  SIZ006_nested_sub_rejects_a_deterministic_underflow();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_add_sub)
