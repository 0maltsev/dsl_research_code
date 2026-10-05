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

// C-Idx: "body index i under 0<=i<nu_n<=u_n" -> "(i,u_n;i)". AM-026:
// exercised standalone via a caller-supplied IndexContext, not yet wired
// to live FoldExpr/BuildExpr traversal.

void CIdx_index_variable_resolves_via_the_context() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); idx);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto enclosing_upper = make_literal(16);
  IndexContext context;
  context[*fold_expr.index_binding] = IndexBinding{make_symbol("idx"), enclosing_upper};

  const auto outcome = infer_count(*fold_expr.body, context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_symbol("idx")));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *enclosing_upper));
}

// C-Idx composes with already-implemented rules (here, C-Add): the index
// binder's own (Symbol, u_n) combines through infer_add exactly like any
// other operand.
void CIdx_index_variable_combines_with_CAdd() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); acc + idx);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());

  const auto index_symbol = make_symbol("idx");
  const auto enclosing_upper = make_literal(16);
  IndexContext context;
  context[*fold_expr.index_binding] = IndexBinding{index_symbol, enclosing_upper};

  // "acc + idx": acc is an ordinary variable (the accumulator), which has
  // no accepted count-refinement rule at all (not in `context`, and
  // Table 6 has no C-Var) -- so the whole expression is unrefinable, but
  // only because of `acc`, not `idx`. Confirmed separately below by
  // testing `idx` alone (already done) and by testing `idx + idx`.
  const auto acc_plus_idx_outcome = infer_count(*fold_expr.body, context);
  BOUNDFIN_CHECK(!acc_plus_idx_outcome.ok);
  BOUNDFIN_CHECK_EQ(acc_plus_idx_outcome.diagnostic->code, std::string("SIZ003"));

  // Build a fresh "idx + idx" expression shape via a second module so
  // C-Idx's result is exercised on *both* sides of a C-Add.
  auto idx_plus_idx_module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); idx + idx);\nexport f;\n");
  const auto &idx_plus_idx_fold = std::get<FoldExpr>(idx_plus_idx_module.functions[0].body->data);
  BOUNDFIN_CHECK(idx_plus_idx_fold.index_binding.has_value());
  IndexContext idx_plus_idx_context;
  idx_plus_idx_context[*idx_plus_idx_fold.index_binding] = IndexBinding{index_symbol, enclosing_upper};
  const auto idx_plus_idx_outcome = infer_count(*idx_plus_idx_fold.body, idx_plus_idx_context);
  BOUNDFIN_CHECK(idx_plus_idx_outcome.ok);
  BOUNDFIN_CHECK(idx_plus_idx_outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**idx_plus_idx_outcome.result->exact, *make_add(index_symbol, index_symbol)));
  BOUNDFIN_CHECK(terms_equal(*idx_plus_idx_outcome.result->upper, *make_add(enclosing_upper, enclosing_upper)));
}

// Without a supplied context (the default, empty one), a reference to
// the very same index binder has no accepted count-refinement rule: the
// "0<=i<nu_n<=u_n" premise is established only by the context a caller
// supplies, never re-derived from the AST itself in this slice.
void SIZ003_index_variable_without_a_supplied_context_is_rejected() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); idx);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  const auto outcome = infer_count(*fold_expr.body); // no context argument: defaults to empty
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// A binding genuinely absent from the context (here, the accumulator,
// which Table 6 never gives a rule to regardless of context) stays
// SIZ003 even while a *different* binding (the index) is present in the
// same context -- confirms the lookup is keyed per-binding, not a blanket
// "inside a fold body" allowance.
void SIZ003_a_binding_absent_from_the_context_is_rejected_even_when_others_are_present() {
  auto module = parse_and_typecheck(
      "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); acc);\nexport f;\n");
  const auto &fold_expr = std::get<FoldExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(fold_expr.index_binding.has_value());
  BOUNDFIN_CHECK(fold_expr.accumulator_binding.has_value());

  IndexContext context;
  context[*fold_expr.index_binding] = IndexBinding{make_symbol("idx"), make_literal(16)};
  // Note: fold_expr.accumulator_binding is deliberately NOT added.

  const auto outcome = infer_count(*fold_expr.body, context); // body is just "acc"
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

void boundfin_count_idx() {
  CIdx_index_variable_resolves_via_the_context();
  CIdx_index_variable_combines_with_CAdd();
  SIZ003_index_variable_without_a_supplied_context_is_rejected();
  SIZ003_a_binding_absent_from_the_context_is_rejected_even_when_others_are_present();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_idx)
