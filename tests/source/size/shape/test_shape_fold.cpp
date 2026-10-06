#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::shape_of_fold;
using boundfin::source::size::shape::shape_of_product;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::ShapePtr;

SourceSpan test_span() { return SourceSpan{}; }

ShapePtr array_shape(std::optional<std::uint64_t> exact_value, std::uint64_t upper_value, std::uint32_t capacity,
                      ShapePtr element) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = exact_value ? std::optional{make_literal(*exact_value)} : std::nullopt;
  data.upper = make_literal(upper_value);
  data.capacity = capacity;
  data.element = std::move(element);
  shape->data = data;
  return shape;
}

// A present-but-null exact term (distinct from exact_value=std::nullopt,
// which legitimately means "star"/inexact) -- a malformed ArrayShape only
// reachable by direct, hand-built construction like this, matching
// test_shape_builder.cpp's identical AM-032-driven distinction.
ShapePtr array_shape_with_null_exact_term(std::uint64_t upper_value, std::uint32_t capacity, ShapePtr element) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = std::optional<boundfin::source::size::certificate::TermPtr>{nullptr};
  data.upper = make_literal(upper_value);
  data.capacity = capacity;
  data.element = std::move(element);
  shape->data = data;
  return shape;
}

// Table 8's "fold" row (main.pdf p.40): "kappa_0, kappa_i+1=Kb(i,kappa_i)"
// -> "kappa_s for exact s; otherwise squnion_{0<=j<=u} kappa_j". AM-033:
// a scalar accumulator is always a trivial fixed point (Kb(kappa)=kappa
// for the only possible ShapeKind::Scalar value), so the result is the
// initial shape itself.
void shape_of_fold_with_matching_scalars_returns_the_initial_shape() {
  const auto initial = scalar_shape();
  const auto outcome = shape_of_fold(initial, scalar_shape(), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
  // The result is the initial shape by pointer identity, not a freshly
  // built equal-but-different object or the step shape -- confirms
  // AM-033's "the result is kappa_0 itself" design.
  BOUNDFIN_CHECK(outcome.result->get() == initial.get());
}

// A structurally-identical array accumulator (same exact/upper/capacity
// and element shape) is a genuine fixed point: F(kappa_0)=kappa_0.
void shape_of_fold_with_structurally_equal_arrays_returns_the_initial_shape() {
  const auto initial = array_shape(5, 5, 8, scalar_shape());
  const auto step = array_shape(5, 5, 8, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->get() == initial.get());
}

// An inexact array accumulator (no exact component) that stays inexact
// with the same upper/capacity/element is likewise a fixed point --
// soundly covers a symbolic (not just literal) upper bound, since no
// literal value is compared here at all beyond terms_equal.
void shape_of_fold_with_structurally_equal_inexact_arrays_returns_the_initial_shape() {
  const auto initial = array_shape(std::nullopt, 16, 16, scalar_shape());
  const auto step = array_shape(std::nullopt, 16, 16, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(!std::get<ArrayShape>((*outcome.result)->data).exact.has_value());
  BOUNDFIN_CHECK(outcome.result->get() == initial.get());
}

// Products are a fixed point when every component, recursively, is.
void shape_of_fold_with_matching_products_returns_the_initial_shape() {
  const auto initial = shape_of_product({scalar_shape(), array_shape(2, 2, 4, scalar_shape())});
  const auto step = shape_of_product({scalar_shape(), array_shape(2, 2, 4, scalar_shape())});
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->get() == initial.get());
}

// Nested array-of-array accumulators are a fixed point only when the
// inner element shape also matches -- exercises compare_shapes's own
// recursion into ArrayShape::element, not just the outer term.
void shape_of_fold_with_structurally_equal_nested_arrays_returns_the_initial_shape() {
  const auto initial = array_shape(1, 1, 4, array_shape(2, 2, 4, scalar_shape()));
  const auto step = array_shape(1, 1, 4, array_shape(2, 2, 4, scalar_shape()));
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->get() == initial.get());
}

// AM-033: a genuinely shape-varying accumulator (here, a logically
// growing array -- the exact term differs between kappa_0 and kappa_1)
// is conservatively rejected with SIZ008 rather than guessed at.
void shape_of_fold_rejects_with_SIZ008_when_the_accumulator_exact_term_grows() {
  const auto initial = array_shape(0, 10, 10, scalar_shape());
  const auto step = array_shape(1, 10, 10, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ008"));
}

// SIZ008 also fires when only a *nested* element shape differs, even if
// the outer array's own exact/upper/capacity match.
void shape_of_fold_rejects_with_SIZ008_when_a_nested_element_shape_differs() {
  const auto initial = array_shape(1, 1, 4, array_shape(2, 2, 4, scalar_shape()));
  const auto step = array_shape(1, 1, 4, array_shape(3, 3, 4, scalar_shape()));
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ008"));
}

// A spec-auditor review of this slice independently constructed a
// counterexample probing whether a per-iteration shape could cycle with
// period >1 (kappa_1!=kappa_0 but kappa_2=kappa_0), which this slice's
// single-step check could not detect as a genuine, sound, bounded
// recurrence: a product accumulator of type arr<i32,4>x arr<i32,4> whose
// body swaps its two components each iteration has exactly this shape
// (kappa_0=(A,B), kappa_1=(B,A), kappa_2=(A,B)=kappa_0, for A!=B). This
// type-checks under T-Fold/TYP010 (a component swap preserves the
// declared product type), so compare_shapes correctly reports `Differs`
// (same ShapeKind::Product, same arity, same per-component ShapeKind::
// Array/capacity, only the component exact/upper terms differ) -- never
// `Incompatible` -- and shape_of_fold accordingly, conservatively,
// rejects with SIZ008 rather than wrongly accepting an unsound bound.
// This documents AM-033's own disclosed "sound but incomplete" cost
// concretely: a genuine, bounded, oscillating recurrence is rejected,
// not just a never-converging one.
void shape_of_fold_rejects_with_SIZ008_for_a_sound_period_two_product_swap_recurrence() {
  const auto shape_a = array_shape(2, 2, 4, scalar_shape());
  const auto shape_b = array_shape(3, 3, 4, scalar_shape());
  const auto initial = shape_of_product({shape_a, shape_b});   // kappa_0 = (A,B)
  const auto step = shape_of_product({shape_b, shape_a});      // kappa_1 = (B,A)
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ008"));
}

// SIZ008 also fires for an exact-presence mismatch alone (one shape
// exact, the other inexact/star), with everything else equal.
void shape_of_fold_rejects_with_SIZ008_for_an_exact_presence_mismatch() {
  const auto initial = array_shape(5, 10, 10, scalar_shape());
  const auto step = array_shape(std::nullopt, 10, 10, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ008"));
}

// SIZ008 also fires when only the upper bound differs, with both shapes
// equally inexact (no exact component on either side).
void shape_of_fold_rejects_with_SIZ008_when_only_the_upper_bound_differs() {
  const auto initial = array_shape(std::nullopt, 10, 12, scalar_shape());
  const auto step = array_shape(std::nullopt, 11, 12, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ008"));
}

// INT001: a present-but-null exact term inside an otherwise-structurally
// -compatible array is a caller precondition violation (compare_shapes's
// own defensive null check), distinct from the legitimate "both inexact"
// SIZ008 case above -- mirrors test_shape_builder.cpp's identical
// AM-032-driven distinction for shape_of_builder's own null checks.
void shape_of_fold_rejects_with_INT001_for_a_present_but_null_exact_term() {
  const auto initial = array_shape_with_null_exact_term(10, 10, scalar_shape());
  const auto step = array_shape(5, 10, 10, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a shape-kind mismatch (scalar vs. array) is a caller
// precondition violation, not a genuine fold-recurrence fact --
// T-Fold/TYP010 already guarantees the body's synthesized type matches
// the accumulator's own declared type, so this never happens for a
// real, legitimately-typechecked fold.
void shape_of_fold_rejects_with_INT001_for_a_kind_mismatch() {
  const auto outcome = shape_of_fold(scalar_shape(), array_shape(1, 1, 4, scalar_shape()), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: an array capacity mismatch is likewise a structural (not
// shape-content) mismatch, mirroring join_shapes's AM-028-settled
// reasoning.
void shape_of_fold_rejects_with_INT001_for_an_array_capacity_mismatch() {
  const auto initial = array_shape(1, 1, 4, scalar_shape());
  const auto step = array_shape(1, 1, 8, scalar_shape());
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a product arity mismatch is likewise structural.
void shape_of_fold_rejects_with_INT001_for_a_product_arity_mismatch() {
  const auto initial = shape_of_product({scalar_shape(), scalar_shape()});
  const auto step = shape_of_product({scalar_shape(), scalar_shape(), scalar_shape()});
  const auto outcome = shape_of_fold(initial, step, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null initial shape is a caller precondition violation.
void shape_of_fold_rejects_with_INT001_for_a_null_initial_shape() {
  const auto outcome = shape_of_fold(nullptr, scalar_shape(), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null step shape is a caller precondition violation.
void shape_of_fold_rejects_with_INT001_for_a_null_step_shape() {
  const auto outcome = shape_of_fold(scalar_shape(), nullptr, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_fold() {
  shape_of_fold_with_matching_scalars_returns_the_initial_shape();
  shape_of_fold_with_structurally_equal_arrays_returns_the_initial_shape();
  shape_of_fold_with_structurally_equal_inexact_arrays_returns_the_initial_shape();
  shape_of_fold_with_matching_products_returns_the_initial_shape();
  shape_of_fold_with_structurally_equal_nested_arrays_returns_the_initial_shape();
  shape_of_fold_rejects_with_SIZ008_when_the_accumulator_exact_term_grows();
  shape_of_fold_rejects_with_SIZ008_when_a_nested_element_shape_differs();
  shape_of_fold_rejects_with_SIZ008_for_a_sound_period_two_product_swap_recurrence();
  shape_of_fold_rejects_with_SIZ008_for_an_exact_presence_mismatch();
  shape_of_fold_rejects_with_SIZ008_when_only_the_upper_bound_differs();
  shape_of_fold_rejects_with_INT001_for_a_present_but_null_exact_term();
  shape_of_fold_rejects_with_INT001_for_a_kind_mismatch();
  shape_of_fold_rejects_with_INT001_for_an_array_capacity_mismatch();
  shape_of_fold_rejects_with_INT001_for_a_product_arity_mismatch();
  shape_of_fold_rejects_with_INT001_for_a_null_initial_shape();
  shape_of_fold_rejects_with_INT001_for_a_null_step_shape();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_fold)
