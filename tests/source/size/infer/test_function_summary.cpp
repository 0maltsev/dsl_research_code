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
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

// AM-003: Sigma(f)=(signature, K_f, Q_f?, Phi_f, rank). Step 3 of
// AM-035's ladder: a call-free function's own K_f/Q_f, computed via
// infer_shape/infer_count directly, no substitution needed yet.
//
// A literal body: K_f is scalar (main.pdf p.11's "scalar literal" rule);
// Q_f is present (C-Const: (5,5;nu=5)), since the body has a genuine
// accepted count-refinement derivation.
void compute_function_summary_of_a_literal_body() {
  const auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x00000005);\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(outcome.result->q_f.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->q_f->exact, *make_literal(5)));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->q_f->upper, *make_literal(5)));
}

// An ordinary scalar parameter is seeded with scalar_shape() (K_f
// succeeds), but Table 6 has no C-Var rule for an arbitrary,
// non-index-binder variable -- Q_f's own derivation fails (SIZ003),
// which AM-003 says must NOT fail the whole summary: Q_f is simply
// absent, not an error.
void compute_function_summary_seeds_scalar_parameters_but_Qf_is_absent_for_a_bare_reference() {
  const auto module = parse_and_typecheck("fn f(a: i32): i32 = a;\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(!outcome.result->q_f.has_value());
}

// An ABI array parameter is seeded via shape_of_abi_array_param
// (K_f = scalar, since len's own Table 8 shape is always scalar); Q_f
// IS derivable here, via C-Len-E reading the same seeded shape_context
// (an n_x-parameterized, but genuinely present, count refinement).
void compute_function_summary_seeds_abi_array_parameters_and_Qf_resolves_via_C_Len_E() {
  const auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(outcome.result->q_f.has_value());
  BOUNDFIN_CHECK(outcome.result->q_f->exact.has_value());
}

// A non-i32 result: K_f is the ABI array's own n_x-parameterized shape
// (array(n_x,n_x,N;scalar), since the body is just the parameter
// itself); Q_f is absent (not applicable to a non-i32 result at all,
// per AM-003).
void compute_function_summary_of_an_array_result_has_no_qf() {
  const auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): arr<i32, 8> = xs;\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Array);
  const auto &array_shape = std::get<ArrayShape>(outcome.result->k_f->data);
  BOUNDFIN_CHECK(array_shape.exact.has_value()); // n_x, parameterized, not a literal
  BOUNDFIN_CHECK(!outcome.result->q_f.has_value());
}

// Q_f's own derivation failure is absorbed silently even when the
// failure code is NOT SIZ003: a genuine SIZ006 (no-wrap certificate
// failure, an overflowing add) still leaves Q_f simply absent, not a
// hard failure of the whole summary -- K_f (Table 8) never cares about
// overflow for ordinary scalar arithmetic, so it still succeeds cleanly.
void compute_function_summary_absorbs_a_SIZ006_Qf_failure_too() {
  const auto module =
      parse_and_typecheck("fn f(): i32 = i32bits(0x7FFFFFFF) + i32bits(0x7FFFFFFF);\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(!outcome.result->q_f.has_value());
}

// Unlike a SIZ-coded Qf failure (absorbed above), an INT-coded failure
// during Qf's own infer_count traversal propagates as a genuine
// compute_function_summary failure -- a spec-auditor re-audit of this
// slice's fix confirmed the code is correct but suggested this is
// cheaply, directly testable (not requiring a parser bypass): resolve a
// normal module, then corrupt one already-resolved VarExpr's own
// resolved_binding in place. infer_shape's own BinaryPrimitive dispatch
// never recurses into operands (k_f still succeeds, scalar), but
// count::infer_count's own C-Add recursion does reach the corrupted
// operand and fails INT001 -- exercising the actual new propagation
// logic this fix added, not just reading it.
void compute_function_summary_propagates_an_INT001_Qf_failure_rather_than_absorbing_it() {
  auto module = parse_and_typecheck("fn f(a: i32): i32 = a + a;\nexport f;\n");
  auto &body = *module.functions[0].body;
  auto &binary = std::get<BinaryPrimitiveExpr>(body.data);
  auto &lhs_var = std::get<VarExpr>(binary.lhs->data);
  BOUNDFIN_CHECK(lhs_var.resolved_binding.has_value()); // genuinely resolved before corruption
  lhs_var.resolved_binding = std::nullopt;

  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// An all-scalar product parameter is seeded via capshape (zero remaining
// ambiguity: capshape's own Product case recurses into scalar_shape()
// for each field). A spec-auditor review of this slice's first version
// found EVERY product-typed parameter wrongly rejected with SIZ013,
// including this fully-resolvable case -- fixed.
void compute_function_summary_seeds_an_all_scalar_product_parameter() {
  const auto module = parse_and_typecheck("fn f(p: prod<i32, i32>): prod<i32, i32> = p;\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->k_f->kind == ShapeKind::Product);
  const auto &components = std::get<boundfin::source::size::shape::ProductShape>(outcome.result->k_f->data).components;
  BOUNDFIN_CHECK_EQ(components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(components[0]->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(components[1]->kind == ShapeKind::Scalar);
}

// A product parameter containing a nested array field gets capshape's
// own conservative "capacity fallback" treatment for that field (no
// fresh formal-length symbol minted -- a nested field has no
// independent binding to mint one from): array(star,N,N;capshape(tau)).
void compute_function_summary_seeds_a_product_parameter_with_a_nested_array_via_capshape() {
  const auto module =
      parse_and_typecheck("fn f(p: prod<i32, arr<i32, 4>>): prod<i32, arr<i32, 4>> = p;\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[0]);
  BOUNDFIN_CHECK(outcome.ok);
  const auto &components = std::get<boundfin::source::size::shape::ProductShape>(outcome.result->k_f->data).components;
  BOUNDFIN_CHECK_EQ(components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(components[0]->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(components[1]->kind == ShapeKind::Array);
  const auto &nested_array = std::get<ArrayShape>(components[1]->data);
  BOUNDFIN_CHECK(!nested_array.exact.has_value()); // capshape's own star, not a fresh formal-length symbol
  BOUNDFIN_CHECK(terms_equal(*nested_array.upper, *make_literal(4)));
  BOUNDFIN_CHECK_EQ(nested_array.capacity, static_cast<std::uint32_t>(4));
}

// Unlike Q_f, K_f is unconditionally required (main.pdf p.12) -- a body
// containing a call (not yet dispatchable, SIZ013) fails
// compute_function_summary as a whole, not just leaves k_f absent
// (there is no "k_f absent" case at all; K_f failure IS summary failure).
void compute_function_summary_propagates_a_Kf_failure_as_a_whole_summary_failure() {
  const auto module =
      parse_and_typecheck("fn g(): i32 = i32bits(0x00000001);\nfn f(): i32 = g();\nexport f;\n");
  const auto outcome = compute_function_summary(module.functions[1]);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ013"));
}

void boundfin_function_summary() {
  compute_function_summary_of_a_literal_body();
  compute_function_summary_seeds_scalar_parameters_but_Qf_is_absent_for_a_bare_reference();
  compute_function_summary_seeds_abi_array_parameters_and_Qf_resolves_via_C_Len_E();
  compute_function_summary_of_an_array_result_has_no_qf();
  compute_function_summary_absorbs_a_SIZ006_Qf_failure_too();
  compute_function_summary_propagates_an_INT001_Qf_failure_rather_than_absorbing_it();
  compute_function_summary_seeds_an_all_scalar_product_parameter();
  compute_function_summary_seeds_a_product_parameter_with_a_nested_array_via_capshape();
  compute_function_summary_propagates_a_Kf_failure_as_a_whole_summary_failure();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_function_summary)
