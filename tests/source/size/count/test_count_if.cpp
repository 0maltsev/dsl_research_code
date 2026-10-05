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

// C-If-E: "guard Boolean; both branches (s,u_t;nu_t) and (s,u_f;nu_f)" ->
// (s,max(u_t,u_f);nu) with nu=s -- applies when both branches are exact
// *with the same exact term*.
void CIfE_same_exact_branches_stay_exact() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = if true then i32bits(0x0000002a) else i32bits(0x0000002a);\nexport f;\n"); // 42 both sides
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(42)));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_max(make_literal(42), make_literal(42))));
}

// C-If-U: "branch exact terms differ or either is star" ->
// (star,max(u_t,u_f);nu) with 0<=nu<=max(u_t,u_f).
void CIfU_differing_exact_branches_fall_back_to_inexact() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = if true then i32bits(0x0000002a) else i32bits(0x0000002b);\nexport f;\n"); // 42 vs 43
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(!outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_max(make_literal(42), make_literal(43))));
}

// C-If-U's premise is a disjunction ("branch exact terms differ *or*
// either is star"); the case above exercises only the "differ" half.
// This exercises the "either is star" half specifically: the outer
// then-branch is itself an inexact (C-If-U) nested if, so there is no
// exact term on that side to even compare against the else-branch's.
void CIfU_either_branch_inexact_falls_back_to_inexact() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = if true then (if true then i32bits(0x00000001) else i32bits(0x00000002)) "
      "else i32bits(0x00000063);\nexport f;\n"); // inner: 1 vs 2 (inexact); outer else: 99
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(!outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(
      *outcome.result->upper, *make_max(make_max(make_literal(1), make_literal(2)), make_literal(99))));
}

// Either branch being inexact (here, via a nested rejected sub-expression
// that still itself fails -- using a bare variable, which has no rule at
// all) means the whole if-expression is uninferrable here, not merely
// "inexact": a branch must *successfully* produce some (lambda,u), even
// an inexact one, for C-If-U's max(u_t,u_f) to be constructible at all.
void nested_if_combines_through_CIfE_and_CIfU() {
  // Outer branches: "if true then 1 else 1" (exact, both sides equal) vs "2".
  auto module = parse_and_typecheck(
      "fn f(): i32 = if true then (if true then i32bits(0x00000001) else i32bits(0x00000001)) "
      "else i32bits(0x00000002);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(outcome.ok);
  // Inner if is exact (1) with its own upper bound max(1,1) (terms are
  // never arithmetically simplified); outer then-branch is therefore
  // exact(1) with upper max(1,1), but differs from the else-branch's
  // exact(2), so the outer if is C-If-U with upper max(max(1,1), 2).
  BOUNDFIN_CHECK(!outcome.result->exact.has_value());
  BOUNDFIN_CHECK(
      terms_equal(*outcome.result->upper, *make_max(make_max(make_literal(1), make_literal(1)), make_literal(2))));
}

void SIZ003_propagates_from_an_unrefinable_branch() {
  auto module = parse_and_typecheck("fn f(x: i32): i32 = if true then x else i32bits(0x00000001);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

void boundfin_count_if() {
  CIfE_same_exact_branches_stay_exact();
  CIfU_differing_exact_branches_fall_back_to_inexact();
  CIfU_either_branch_inexact_falls_back_to_inexact();
  nested_if_combines_through_CIfE_and_CIfU();
  SIZ003_propagates_from_an_unrefinable_branch();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_if)
