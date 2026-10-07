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
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::infer::compute_function_summary;
using boundfin::source::size::infer::infer_call;
using boundfin::source::size::infer::SigmaContext;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::shape_of_abi_array_param;
using boundfin::source::size::shape::ShapeContext;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::typecheck::typecheck_module;

// AM-035 step 5 (re-sequenced per AM-037): Table 8's own "call" row,
// composed from T-Call's own Sigma(f) lookup premise and substitute_shape
// (step 4a).

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

SourceSpan call_span(const Module &module) { return module.functions.back().body->span; }

const CallExpr &call_expr_of(const Module &module) { return std::get<CallExpr>(module.functions.back().body->data); }

// helper's own K_f is array(n_h,n_h,8;scalar), n_h="abi#<helper's xs
// binding>". caller's own single argument is a bare reference to its own
// array parameter ys, whose own shape is array(n_c,n_c,8;scalar), a
// DIFFERENT symbol n_c="abi#<caller's ys binding>". Substituting n_h->n_c
// in both exact and upper positions of K_f must give exactly ys's own
// shape -- confirming the call row's own composition, not a coincidence
// (the two symbols are genuinely distinct bindings).
void infer_call_substitutes_an_array_parameter_through_an_identity_function() {
  const auto module = parse_and_typecheck("fn helper(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn caller(ys: arr<i32, 8>): arr<i32, 8> = helper(ys);\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{0, *helper_summary.result}};

  const auto &caller_param = module.functions[1].parameters[0];
  const auto ys_shape_outcome = shape_of_abi_array_param(caller_param.type, *caller_param.binding, caller_param.span);
  BOUNDFIN_CHECK(ys_shape_outcome.ok);
  ShapeContext caller_shape_context{{*caller_param.binding, *ys_shape_outcome.result}};

  const auto outcome =
      infer_call(call_expr_of(module), call_span(module), module, sigma, caller_shape_context, {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result_array = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result_array.exact.has_value());
  const auto &ys_array = std::get<ArrayShape>((*ys_shape_outcome.result)->data);
  BOUNDFIN_CHECK(terms_equal(**result_array.exact, **ys_array.exact));
  BOUNDFIN_CHECK(terms_equal(*result_array.upper, *ys_array.upper));
  BOUNDFIN_CHECK(!terms_equal(**result_array.exact, *make_literal(0))); // sanity: genuinely a symbol, not coincidentally 0
  BOUNDFIN_CHECK_EQ(result_array.capacity, static_cast<std::uint32_t>(8));
}

// A concrete array-literal actual: substitution replaces the formal
// symbol with a LITERAL term, not another symbol -- K_f's own result
// becomes fully concrete, matching main.pdf's own "instantiated
// (lambda,u;nu)" result column exactly.
void infer_call_substitutes_a_concrete_literal_actual() {
  const auto module = parse_and_typecheck("fn helper(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn caller(): arr<i32, 8> = "
                                           "helper(array<8>[i32bits(0x00000001), i32bits(0x00000002), "
                                           "i32bits(0x00000003)]);\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{0, *helper_summary.result}};

  const auto outcome = infer_call(call_expr_of(module), call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result_array = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result_array.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result_array.exact, *make_literal(3)));
  BOUNDFIN_CHECK(terms_equal(*result_array.upper, *make_literal(3)));
}

// A scalar-only callee needs no substitution at all (K_f is already
// scalar, with no formal symbol anywhere) -- confirms the empty-map case.
void infer_call_passes_through_a_scalar_callee_unchanged() {
  const auto module = parse_and_typecheck("fn helper(a: i32): i32 = a;\n"
                                           "fn caller(b: i32): i32 = helper(b);\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{0, *helper_summary.result}};

  const auto &caller_param = module.functions[1].parameters[0];
  ShapeContext caller_shape_context{{*caller_param.binding, boundfin::source::size::shape::scalar_shape()}};

  const auto outcome =
      infer_call(call_expr_of(module), call_span(module), module, sigma, caller_shape_context, {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// A mixed scalar+array parameter list: only the array argument's own
// position contributes a substitution-map entry; argument/parameter
// POSITIONS are confirmed correctly aligned (not just "first array
// argument found"), by making the scalar parameter come FIRST.
void infer_call_only_substitutes_at_array_typed_parameter_positions() {
  const auto module = parse_and_typecheck("fn helper(a: i32, xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn caller(b: i32, ys: arr<i32, 8>): arr<i32, 8> = helper(b, ys);\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{0, *helper_summary.result}};

  const auto &caller_params = module.functions[1].parameters;
  const auto ys_shape_outcome =
      shape_of_abi_array_param(caller_params[1].type, *caller_params[1].binding, caller_params[1].span);
  BOUNDFIN_CHECK(ys_shape_outcome.ok);
  ShapeContext caller_shape_context{{*caller_params[0].binding, boundfin::source::size::shape::scalar_shape()},
                                     {*caller_params[1].binding, *ys_shape_outcome.result}};

  const auto outcome =
      infer_call(call_expr_of(module), call_span(module), module, sigma, caller_shape_context, {});
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result_array = std::get<ArrayShape>((*outcome.result)->data);
  const auto &ys_array = std::get<ArrayShape>((*ys_shape_outcome.result)->data);
  BOUNDFIN_CHECK(terms_equal(*result_array.upper, *ys_array.upper));
}

// Propagates a failure encountered while computing an actual argument's
// own shape AT AN ARRAY-TYPED PARAMETER POSITION -- genuinely
// load-bearing for the substitution, unlike a scalar/product position
// (see the next test). The argument itself is a nested call expression;
// since the live-wiring slice (below) threads `module`/`sigma` into
// `infer_call`'s own internal argument-shape computation, this nested
// call is now genuinely resolved against `sigma`, not unconditionally
// rejected -- and `sigma` deliberately has no entry for `inner` (rank 0)
// here, so the nested call correctly fails with `INT001` ("Sigma has no
// entry"), which is what this test now exercises propagating. (Before
// the live-wiring slice, `infer_call`'s own internal recursion never had
// `module`/`sigma` at all, so a nested call was unconditionally
// unreachable -- SIZ013 regardless of `sigma`'s own content; this test's
// own expectation is updated to match the now-live, fully-wired
// dispatcher, not a regression.)
void infer_call_propagates_a_failure_from_a_load_bearing_actual_argument() {
  const auto module = parse_and_typecheck("fn inner(): arr<i32, 8> = array<8>[i32bits(0x00000000)];\n"
                                           "fn helper(xs: arr<i32, 8>): arr<i32, 8> = xs;\n"
                                           "fn caller(): arr<i32, 8> = helper(inner());\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[1]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{1, *helper_summary.result}}; // deliberately no entry for inner (rank 0)

  const auto outcome = infer_call(call_expr_of(module), call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// Confirms the fix's own positive case directly: a failing argument at a
// NON-array (here scalar) parameter position is never visited at all --
// the call still succeeds, since that argument's own shape is not
// load-bearing for K_f's own (empty) substitution.
void infer_call_does_not_visit_a_failing_argument_at_a_non_array_position() {
  const auto module = parse_and_typecheck("fn inner(): i32 = i32bits(0x00000000);\n"
                                           "fn helper(a: i32): i32 = a;\n"
                                           "fn caller(): i32 = helper(inner());\n"
                                           "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[1]);
  BOUNDFIN_CHECK(helper_summary.ok);
  // sigma has no entry for inner (rank 0) -- irrelevant here too (see the
  // identical note on the preceding test): inner's own call is never
  // even visited by infer_call in this scenario (a's own position is
  // scalar), so whether sigma has an entry for it is moot either way.
  const SigmaContext sigma{{1, *helper_summary.result}};

  const auto outcome = infer_call(call_expr_of(module), call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

void infer_call_rejects_with_INT001_for_a_missing_sigma_entry() {
  const auto module = parse_and_typecheck("fn helper(a: i32): i32 = a;\n"
                                           "fn caller(b: i32): i32 = helper(b);\n"
                                           "export caller;\n");
  const SigmaContext sigma; // deliberately empty
  const auto &caller_param = module.functions[1].parameters[0];
  ShapeContext caller_shape_context{{*caller_param.binding, boundfin::source::size::shape::scalar_shape()}};
  const auto outcome =
      infer_call(call_expr_of(module), call_span(module), module, sigma, caller_shape_context, {});
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void infer_call_rejects_with_INT001_for_an_unresolved_callee_rank() {
  auto module = parse_and_typecheck("fn helper(a: i32): i32 = a;\n"
                                     "fn caller(b: i32): i32 = helper(b);\n"
                                     "export caller;\n");
  std::get<CallExpr>(module.functions[1].body->data).resolved_callee_rank = std::nullopt;
  const SigmaContext sigma;
  const auto outcome = infer_call(call_expr_of(module), call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// A spec-auditor review flagged this and the next trust-boundary branch
// as untested, and specifically worth covering before a future
// module-wide Sigma-builder milestone (most likely to trip an
// off-by-one bug exactly here). A second, independent re-audit then
// found this test's own first version vacuous: with `sigma` left empty,
// the dedicated "rank out of range" check and the very next "Sigma has
// no entry" check both return the identical INT001 code (differing only
// in message), so removing the dedicated check entirely would still
// pass this test via the neighboring branch. Fixed by also asserting on
// `.message`, which the two branches do NOT share, so only the dedicated
// check's own removal can make this test fail.
void infer_call_rejects_with_INT001_for_an_out_of_range_callee_rank() {
  auto module = parse_and_typecheck("fn helper(a: i32): i32 = a;\n"
                                     "fn caller(b: i32): i32 = helper(b);\n"
                                     "export caller;\n");
  std::get<CallExpr>(module.functions[1].body->data).resolved_callee_rank = 999;
  const SigmaContext sigma;
  const auto outcome = infer_call(call_expr_of(module), call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->message, std::string("internal: resolved callee rank is out of range"));
}

void infer_call_rejects_with_INT001_for_a_mismatched_argument_count() {
  auto module = parse_and_typecheck("fn helper(a: i32, c: i32): i32 = a;\n"
                                     "fn caller(b: i32): i32 = helper(b, b);\n"
                                     "export caller;\n");
  const auto helper_summary = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(helper_summary.ok);
  const SigmaContext sigma{{0, *helper_summary.result}};
  auto &call = std::get<CallExpr>(module.functions[1].body->data);
  call.arguments.pop_back(); // now 1 argument; helper still declares 2 parameters
  const auto outcome = infer_call(call, call_span(module), module, sigma, {}, {});
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_infer_call() {
  infer_call_substitutes_an_array_parameter_through_an_identity_function();
  infer_call_substitutes_a_concrete_literal_actual();
  infer_call_passes_through_a_scalar_callee_unchanged();
  infer_call_only_substitutes_at_array_typed_parameter_positions();
  infer_call_propagates_a_failure_from_a_load_bearing_actual_argument();
  infer_call_does_not_visit_a_failing_argument_at_a_non_array_position();
  infer_call_rejects_with_INT001_for_a_missing_sigma_entry();
  infer_call_rejects_with_INT001_for_an_unresolved_callee_rank();
  infer_call_rejects_with_INT001_for_an_out_of_range_callee_rank();
  infer_call_rejects_with_INT001_for_a_mismatched_argument_count();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_infer_call)
