#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::count::check_module_count_admissibility;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

// AM-027: check_module_count_admissibility is the first caller of
// check_count_admissibility reachable from outside this module's own
// tests, so these tests exercise the module-wide walker itself (finding
// FoldExpr/BuildExpr in live AST structure), not check_count_admissibility
// 's own per-node semantics (already covered by test_count_admissibility.cpp).

// A module with no fold/build at all (only Let/If/arithmetic/Var/
// Literal) is accepted: the walker must terminate cleanly on every
// non-fold/build ExprKind without ever calling check_count_admissibility.
void module_accepts_a_function_with_no_fold_or_build() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = let x = n in if true then x else i32bits(0x00000000);\n"
                                     "export f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(outcome.ok);
}

// A single admissible fold, reached through the walker's own Fold case
// (not a caller-built IndexContext as in test_count_admissibility.cpp).
void module_accepts_a_single_admissible_fold() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc);\nexport f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(outcome.ok);
}

// A single admissible build, reached through the walker's own Build
// case.
void module_accepts_a_single_admissible_build() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = len(build<16>(i32bits(0x0000000a); idx; idx));\nexport f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(outcome.ok);
}

// SIZ005 (count exceeds capacity) found by the walker inside a fold
// nested two levels deep (inside a Let's bound expression), confirming
// the walker recurses into Let, not just top-level Fold/Build.
void SIZ005_propagates_from_a_fold_found_deep_inside_a_let() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = let x = fold<5>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc) in x;\n"
      "export f;\n"); // count=10, capacity=5
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// SIZ003 (no accepted count-refinement rule) found by the walker inside
// a build's body position (the build's own `idx` body is fine, but its
// count expression here is an unrefinable bare parameter).
void SIZ003_propagates_from_a_build_whose_count_is_unrefinable() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = len(build<16>(n; idx; idx));\nexport f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// A nested fold's own count may reference the *enclosing* fold's index
// binder -- this time via the walker's own IndexContext threading, not a
// manually-constructed outer_context (test_count_admissibility.cpp's
// admissibility_lets_a_nested_count_reference_an_outer_index_binder
// covers the latter).
void module_accepts_a_nested_fold_whose_count_references_the_enclosing_index() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); outer_acc, outer_idx; i32bits(0x00000000); "
      "fold<16>(outer_idx; inner_acc, inner_idx; i32bits(0x00000000); inner_acc));\nexport f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(outcome.ok);
}

// A fold nested inside another fold's `initial` (not `body`) is still
// found and admissibility-checked -- confirms the walker visits
// `initial`, not just `body`.
void SIZ005_propagates_from_a_fold_nested_inside_anothers_initial_expression() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); outer_acc, outer_idx; "
      "fold<5>(i32bits(0x0000000a); inner_acc, inner_idx; i32bits(0x00000000); inner_acc); outer_acc);\n"
      "export f;\n"); // inner fold: count=10, capacity=5
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// Regression for a spec-auditor FAIL (2026-10-06): a Fold/Build hidden
// inside an If's *condition*, where that If is itself part of an
// enclosing fold's own `count` expression, was previously invisible to
// the walker -- infer_if (count.cpp) never inspects if_expr.condition at
// all (it only recurses into then_branch/else_branch), so neither
// infer_count_impl's own traversal nor the walker's original
// "count is already fold/build-free by construction" argument ever
// reached it. The inner fold<5> below has count=10 > capacity=5 and
// must be found and rejected even though it sits inside the *condition*
// of an `if` that is itself the outer fold's own count expression (the
// outer fold's own count judgment succeeds regardless -- both branches
// are literals 5 and 3, C-If-U gives upper bound 5, well within capacity
// 16 -- so before the fix this whole module was reported ok=true).
void SIZ005_propagates_from_a_fold_hidden_inside_an_enclosing_folds_count_condition() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(if fold<5>(i32bits(0x0000000a); acc, idx; true; acc) then "
      "i32bits(0x00000005) else i32bits(0x00000003); outer_acc, outer_idx; i32bits(0x00000000); "
      "outer_acc);\nexport f;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// Two independent functions: the first (in declaration order) has the
// inadmissible fold, the second is fine. The module is rejected with the
// first function's diagnostic -- confirms module.functions order, not
// just within-one-function order, is first-error.
void module_rejects_the_first_function_in_declaration_order_before_checking_the_second() {
  auto module = parse_and_typecheck(
      "fn bad(n: i32): i32 = fold<5>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc);\n" // count=10 > capacity=5
      "fn good(n: i32): i32 = i32bits(0x00000000);\n"
      "export good;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// The mirror of the previous test: the FIRST function is fine and the
// SECOND has the inadmissible fold -- confirms every function in
// `module.functions` is actually checked, not just the first.
void module_rejects_a_later_function_even_when_earlier_ones_are_admissible() {
  auto module = parse_and_typecheck(
      "fn good(n: i32): i32 = i32bits(0x00000000);\n"
      "fn bad(n: i32): i32 = fold<5>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc);\n" // count=10 > capacity=5
      "export good;\n");
  const auto outcome = check_module_count_admissibility(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

void boundfin_count_module_admissibility() {
  module_accepts_a_function_with_no_fold_or_build();
  module_accepts_a_single_admissible_fold();
  module_accepts_a_single_admissible_build();
  SIZ005_propagates_from_a_fold_found_deep_inside_a_let();
  SIZ003_propagates_from_a_build_whose_count_is_unrefinable();
  module_accepts_a_nested_fold_whose_count_references_the_enclosing_index();
  SIZ005_propagates_from_a_fold_nested_inside_anothers_initial_expression();
  SIZ005_propagates_from_a_fold_hidden_inside_an_enclosing_folds_count_condition();
  module_rejects_the_first_function_in_declaration_order_before_checking_the_second();
  module_rejects_a_later_function_even_when_earlier_ones_are_admissible();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_module_admissibility)
