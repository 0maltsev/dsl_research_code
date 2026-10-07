#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/infer/infer.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::infer::compute_module_summaries;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::typecheck::typecheck_module;

// The live-wiring slice (AM-035 step 5's own disclosed follow-up,
// confirmed via AskUserQuestion before implementation, 2026-10-07):
// compute_module_summaries actually builds a real, whole-module Sigma by
// calling compute_function_summary for every function in declaration-
// rank order, threading the Sigma accumulated so far into each call --
// the concrete mechanism that makes declaration-rank induction sound in
// this codebase, not just an argument about it on paper.

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

void compute_module_summaries_builds_sigma_for_a_single_call_free_function() {
  const auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x00000005);\nexport f;\n");
  const auto outcome = compute_module_summaries(module);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.result->size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(outcome.result->count(0) == 1);
  BOUNDFIN_CHECK(outcome.result->at(0).k_f->kind == ShapeKind::Scalar);
}

// Two-function call chain: confirms BOTH ranks land in Sigma, and that
// the caller's own k_f is genuinely the substituted result (not merely
// present) -- the same assertion style infer_call's own tests use,
// reached here via the full live pipeline instead of a hand-built Sigma.
void compute_module_summaries_resolves_a_two_function_call_chain() {
  const auto module = parse_and_typecheck("fn helper(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn caller(ys: arr<i32, 8>): arr<i32, 8> = helper(ys);\n"
                                           "export caller;\n");
  const auto outcome = compute_module_summaries(module);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.result->size(), static_cast<std::size_t>(2));

  const auto &caller_k_f = outcome.result->at(1).k_f;
  BOUNDFIN_CHECK(caller_k_f->kind == ShapeKind::Array);
  const auto &caller_array = std::get<ArrayShape>(caller_k_f->data);
  BOUNDFIN_CHECK(caller_array.exact.has_value());
  const auto &ys_binding = *module.functions[1].parameters[0].binding;
  BOUNDFIN_CHECK(terms_equal(**caller_array.exact,
                              *boundfin::source::size::certificate::make_symbol("abi#" + std::to_string(ys_binding))));
}

// Three-function chain (A <- B <- C): confirms multi-hop declaration-rank
// induction genuinely works, not just one hop -- C's own k_f resolves
// through B's own already-substituted k_f, which itself was substituted
// from A's.
void compute_module_summaries_resolves_a_three_function_call_chain() {
  const auto module = parse_and_typecheck("fn a(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn b(ys: arr<i32, 8>): arr<i32, 8> = a(ys);\n"
                                           "fn c(zs: arr<i32, 8>): arr<i32, 8> = b(zs);\n"
                                           "export c;\n");
  const auto outcome = compute_module_summaries(module);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.result->size(), static_cast<std::size_t>(3));

  const auto &c_k_f = outcome.result->at(2).k_f;
  BOUNDFIN_CHECK(c_k_f->kind == ShapeKind::Array);
  const auto &c_array = std::get<ArrayShape>(c_k_f->data);
  BOUNDFIN_CHECK(c_array.exact.has_value());
  const auto &zs_binding = *module.functions[2].parameters[0].binding;
  BOUNDFIN_CHECK(terms_equal(**c_array.exact,
                              *boundfin::source::size::certificate::make_symbol("abi#" + std::to_string(zs_binding))));
}

// First-error: the FIRST function whose own summary fails blocks the
// whole module's Sigma construction, matching check_module_count_
// admissibility's own established pattern and main.pdf p.12 Sec. 4.4's
// own "size transformer for every declaration" |-_M wf requirement.
// Forced via the established in-place-corruption technique (no parser
// bypass needed): corrupt an already-resolved call's own
// resolved_callee_rank after a normal parse/resolve/typecheck pass.
void compute_module_summaries_propagates_a_good_first_then_bad_second_function_failure() {
  auto module = parse_and_typecheck("fn helper(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                     "fn caller(ys: arr<i32, 8>): arr<i32, 8> = helper(ys);\n"
                                     "export caller;\n");
  std::get<CallExpr>(module.functions[1].body->data).resolved_callee_rank = std::nullopt;

  const auto outcome = compute_module_summaries(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// A spec-auditor review found the preceding test only exercised "good
// function before bad function" -- this project's own established
// precedent for an identical module-wide first-error property
// (check_module_count_admissibility, Phase 3.4 fifth slice) explicitly
// tests both directions, confirming the loop checks every function, not
// just the first. Mirrors that: the BAD function (rank 0, a corrupted
// let-binding inside its own body -- not a call, since a rank-0 function
// can never contain one, by declaration-rank induction) comes first; a
// perfectly good, independent, call-free function (rank 1) follows and
// would succeed on its own, confirming the first function's own failure
// blocks the whole module rather than being skipped in favor of a later
// success.
void compute_module_summaries_propagates_a_bad_first_then_good_second_function_failure() {
  auto module = parse_and_typecheck("fn bad(): i32 = let x = i32bits(0x00000001) in x;\n"
                                     "fn good(): i32 = i32bits(0x00000002);\n"
                                     "export good;\n");
  auto &bad_let = std::get<LetExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(bad_let.binding.has_value()); // genuinely resolved before corruption
  bad_let.binding = std::nullopt;

  const auto outcome = compute_module_summaries(module);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_module_summaries() {
  compute_module_summaries_builds_sigma_for_a_single_call_free_function();
  compute_module_summaries_resolves_a_two_function_call_chain();
  compute_module_summaries_resolves_a_three_function_call_chain();
  compute_module_summaries_propagates_a_good_first_then_bad_second_function_failure();
  compute_module_summaries_propagates_a_bad_first_then_good_second_function_failure();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_module_summaries)
