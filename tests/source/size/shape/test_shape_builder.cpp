#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::make_symbol;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::shape_of_builder;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::ShapePtr;

SourceSpan test_span() { return SourceSpan{}; }

ShapePtr array_body_shape() {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(2);
  data.upper = make_literal(2);
  data.capacity = 2;
  data.element = scalar_shape();
  shape->data = data;
  return shape;
}

// Table 8's "builder" row (main.pdf p.40): "count (lambda,u;nu), body
// kappa_b(i)" -> "array(lambda,u,N;squnion_{i<u} kappa_b(i))". AM-031:
// the join degenerates to one computation of kappa_b -- with an exact
// count, the result is array(lambda,u,N;body_shape), using body_shape
// directly as the element (not some derived/rejoined value).
void shape_of_builder_with_an_exact_count_wraps_the_body_shape_directly() {
  const auto body = array_body_shape();
  const auto outcome = shape_of_builder(make_literal(5), make_literal(5), 8, body, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(5)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(5)));
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(8));
  // The element is the body shape itself, not a join result -- confirms
  // AM-031's "degenerates to one computation" design, not a hidden
  // self-join that happens to produce an equal-but-different object.
  BOUNDFIN_CHECK(result.element.get() == body.get());
}

// An inexact count (upper-only) carries through as inexact, matching
// Table 6's own C-Idx result shape for the index binder.
void shape_of_builder_with_an_inexact_count_has_no_exact_component() {
  const auto body = scalar_shape();
  const auto outcome = shape_of_builder(std::nullopt, make_literal(16), 16, body, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(16)));
}

// INT001: a null upper bound is a caller precondition violation.
void shape_of_builder_rejects_with_INT001_for_a_null_upper() {
  const auto outcome = shape_of_builder(std::nullopt, nullptr, 4, scalar_shape(), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a *present but null* exact term is a caller precondition
// violation, distinct from the legitimately-inexact "absent" case
// (std::nullopt) tested above.
void shape_of_builder_rejects_with_INT001_for_a_present_but_null_exact_term() {
  const std::optional<boundfin::source::size::certificate::TermPtr> null_exact{nullptr};
  const auto outcome = shape_of_builder(null_exact, make_literal(4), 4, scalar_shape(), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null body shape is a caller precondition violation.
void shape_of_builder_rejects_with_INT001_for_a_null_body_shape() {
  const auto outcome = shape_of_builder(std::nullopt, make_literal(4), 4, nullptr, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// AM-032: shape_of_builder deliberately adds no "lambda<=u<=N" consistency
// check, because that invariant is already guaranteed upstream (by
// check_count_admissibility's SIZ005, and by infer_count's own per-rule
// lambda<=u soundness) before any legitimate caller could reach this
// function with a violating triple. This test documents the resulting,
// deliberate trust-boundary behavior: a directly-constructed, internally
// inconsistent triple (here, exact > upper) is accepted silently (ok ==
// true), not rejected with SIZ007 -- SIZ007 remains unreserved by this row.
void shape_of_builder_silently_accepts_an_inconsistent_exact_above_upper_triple() {
  const auto outcome = shape_of_builder(make_literal(9), make_literal(5), 5, scalar_shape(), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(9)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(5)));
}

// Same trust-boundary behavior for the other half of the invariant: an
// upper bound exceeding the declared capacity N is likewise accepted
// silently rather than rejected with SIZ007.
void shape_of_builder_silently_accepts_an_inconsistent_upper_above_capacity_triple() {
  const auto outcome = shape_of_builder(std::nullopt, make_literal(7), 4, scalar_shape(), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(4));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(7)));
}

// A symbolic (non-literal) count term, matching the shape Table 6's C-Idx
// actually produces for an index binder's own upper bound (a symbolic `N`
// or `u`, not always a concrete literal). shape_of_builder must carry a
// symbolic term through exactly as given, since join_shapes-style structural
// comparison (not arithmetic reduction) is this project's established
// convention for every TermPtr in this module.
void shape_of_builder_carries_a_symbolic_count_term_through_unmodified() {
  const auto body = scalar_shape();
  const auto symbolic_upper = make_symbol("N");
  const auto outcome = shape_of_builder(std::nullopt, symbolic_upper, 16, body, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*result.upper, *symbolic_upper));
  BOUNDFIN_CHECK(result.element.get() == body.get());
}

void boundfin_shape_builder() {
  shape_of_builder_with_an_exact_count_wraps_the_body_shape_directly();
  shape_of_builder_with_an_inexact_count_has_no_exact_component();
  shape_of_builder_rejects_with_INT001_for_a_null_upper();
  shape_of_builder_rejects_with_INT001_for_a_present_but_null_exact_term();
  shape_of_builder_rejects_with_INT001_for_a_null_body_shape();
  shape_of_builder_silently_accepts_an_inconsistent_exact_above_upper_triple();
  shape_of_builder_silently_accepts_an_inconsistent_upper_above_capacity_triple();
  shape_of_builder_carries_a_symbolic_count_term_through_unmodified();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_builder)
