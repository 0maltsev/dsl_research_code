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
using boundfin::source::size::count::check_count_admissibility;
using boundfin::source::size::count::IndexBinding;
using boundfin::source::size::count::IndexContext;
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

// T-Fold/T-Build's shared count-admissibility premise (main.pdf p.11):
// "Sigma;Delta;Gamma |-cnt en => (lambda_n,u_n;nu_n)" then
// "Delta |-cert u_n<=N".

void admissibility_succeeds_when_the_count_is_within_capacity() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc);\nexport f;\n"); // count=10, capacity=16
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto outcome =
      check_count_admissibility(*fold_expr.count, fold_expr.capacity, *fold_expr.index_binding);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.binding.has_value());
  BOUNDFIN_CHECK(terms_equal(*outcome.binding->upper, *make_literal(10)));
}

// The returned symbol is deterministically named from the index binder's
// own BindingId (not a fixed/reused literal), per the prior slice's
// documented requirement -- confirmed directly, and confirmed usable
// as an IndexContext entry for infer_count's C-Idx.
void admissibility_mints_a_symbol_usable_as_an_index_context_entry() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); idx);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto admissibility =
      check_count_admissibility(*fold_expr.count, fold_expr.capacity, *fold_expr.index_binding);
  BOUNDFIN_CHECK(admissibility.ok);

  IndexContext context;
  context[*fold_expr.index_binding] = *admissibility.binding;
  const auto body_outcome = infer_count(*fold_expr.body, context); // body is just "idx"
  BOUNDFIN_CHECK(body_outcome.ok);
  BOUNDFIN_CHECK(body_outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**body_outcome.result->exact, *admissibility.binding->symbol));
}

// Two *sibling* folds within the SAME resolved module get genuinely
// distinct BindingIds for their index binders (Phase 3.1's resolver uses
// one counter spanning the whole module, confirmed in resolver.cpp) --
// AM-026's third-slice audit required symbol names to be minted
// deterministically from BindingId specifically so this case can never
// collide; confirmed directly here, not merely asserted.
void admissibility_mints_distinct_symbols_for_distinct_sibling_index_binders() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = (fold<16>(i32bits(0x0000000a); acc1, idx1; i32bits(0x00000000); idx1)) + "
      "(fold<16>(i32bits(0x0000000a); acc2, idx2; i32bits(0x00000000); idx2));\nexport f;\n");
  const auto &binary = std::get<BinaryPrimitiveExpr>(module.functions[0].body->data);
  const auto &first_fold = std::get<FoldExpr>(binary.lhs->data);
  const auto &second_fold = std::get<FoldExpr>(binary.rhs->data);
  BOUNDFIN_CHECK(first_fold.index_binding.has_value());
  BOUNDFIN_CHECK(second_fold.index_binding.has_value());
  BOUNDFIN_CHECK(*first_fold.index_binding != *second_fold.index_binding); // distinct ids, same module

  const auto first_admissibility =
      check_count_admissibility(*first_fold.count, first_fold.capacity, *first_fold.index_binding);
  const auto second_admissibility =
      check_count_admissibility(*second_fold.count, second_fold.capacity, *second_fold.index_binding);
  BOUNDFIN_CHECK(first_admissibility.ok);
  BOUNDFIN_CHECK(second_admissibility.ok);
  BOUNDFIN_CHECK(!terms_equal(*first_admissibility.binding->symbol, *second_admissibility.binding->symbol));
}

// Independently resolved modules do not share a BindingId counter (each
// resolver run restarts at 0), so the SAME source text produces the SAME
// index-binder id, and therefore the same minted symbol, in two separate
// resolutions -- confirms the minting scheme is a deterministic function
// of BindingId (not, e.g., incorporating anything nondeterministic like
// an address or a process-wide counter), the property AM-026's audit
// actually required; distinctness-within-one-module is covered above.
void admissibility_minting_is_deterministic_across_independent_resolutions() {
  auto first_module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); idx);\nexport f;\n");
  auto second_module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); acc, idx; i32bits(0x00000000); idx);\nexport f;\n");
  const auto &first_fold = std::get<FoldExpr>(first_module.functions[0].body->data);
  const auto &second_fold = std::get<FoldExpr>(second_module.functions[0].body->data);
  BOUNDFIN_CHECK(first_fold.index_binding.has_value());
  BOUNDFIN_CHECK(second_fold.index_binding.has_value());
  BOUNDFIN_CHECK_EQ(*first_fold.index_binding, *second_fold.index_binding); // same text, same id, by construction

  const auto first_admissibility =
      check_count_admissibility(*first_fold.count, first_fold.capacity, *first_fold.index_binding);
  const auto second_admissibility =
      check_count_admissibility(*second_fold.count, second_fold.capacity, *second_fold.index_binding);
  BOUNDFIN_CHECK(first_admissibility.ok);
  BOUNDFIN_CHECK(second_admissibility.ok);
  BOUNDFIN_CHECK(terms_equal(*first_admissibility.binding->symbol, *second_admissibility.binding->symbol));
}

// A nested fold's own count expression may itself reference an
// *enclosing* fold's index binder (via outer_context) -- e.g. an inner
// fold counting up to the outer loop's own current index.
void admissibility_lets_a_nested_count_reference_an_outer_index_binder() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(i32bits(0x0000000a); outer_acc, outer_idx; i32bits(0x00000000); "
      "fold<16>(outer_idx; inner_acc, inner_idx; i32bits(0x00000000); inner_acc));\nexport f;\n");
  const auto &outer_fold = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(outer_fold.index_binding.has_value());
  const auto outer_admissibility =
      check_count_admissibility(*outer_fold.count, outer_fold.capacity, *outer_fold.index_binding);
  BOUNDFIN_CHECK(outer_admissibility.ok);

  IndexContext outer_context;
  outer_context[*outer_fold.index_binding] = *outer_admissibility.binding;

  const auto &inner_fold = std::get<FoldExpr>(outer_fold.body->data);
  BOUNDFIN_CHECK(inner_fold.index_binding.has_value());
  // The inner fold's own count is "outer_idx": resolving it needs
  // outer_context supplied, not the default-empty one.
  const auto inner_admissibility =
      check_count_admissibility(*inner_fold.count, inner_fold.capacity, *inner_fold.index_binding, outer_context);
  BOUNDFIN_CHECK(inner_admissibility.ok);
  // outer_idx's own upper bound (10, from the outer admissibility check)
  // becomes the inner fold's count's upper bound too (C-Idx inherits u_n).
  BOUNDFIN_CHECK(terms_equal(*inner_admissibility.binding->upper, *make_literal(10)));
}

// SIZ005: the count's upper bound is not proved within the declared
// capacity -- here the count itself (via two C-Const operands combined
// by C-Add) certifiably exceeds the fold's declared capacity of 5.
void SIZ005_rejects_a_count_whose_upper_bound_exceeds_capacity() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<5>(i32bits(0x0000000a) + i32bits(0x0000000a); acc, idx; i32bits(0x00000000); acc);\n"
      "export f;\n"); // count = 10+10 = 20, capacity = 5
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto outcome =
      check_count_admissibility(*fold_expr.count, fold_expr.capacity, *fold_expr.index_binding);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// A count expression with no accepted count-refinement rule at all (here,
// a bare non-index parameter reference) propagates infer_count's own
// SIZ003, not a SIZ005.
void SIZ003_propagates_from_an_unrefinable_count_expression() {
  auto module =
      parse_and_typecheck("fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); acc);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto outcome =
      check_count_admissibility(*fold_expr.count, fold_expr.capacity, *fold_expr.index_binding);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

void boundfin_count_admissibility() {
  admissibility_succeeds_when_the_count_is_within_capacity();
  admissibility_mints_a_symbol_usable_as_an_index_context_entry();
  admissibility_mints_distinct_symbols_for_distinct_sibling_index_binders();
  admissibility_minting_is_deterministic_across_independent_resolutions();
  admissibility_lets_a_nested_count_reference_an_outer_index_binder();
  SIZ005_rejects_a_count_whose_upper_bound_exceeds_capacity();
  SIZ003_propagates_from_an_unrefinable_count_expression();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_admissibility)
