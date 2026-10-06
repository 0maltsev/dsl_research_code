#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::shape_of_literal;
using boundfin::source::size::shape::ShapePtr;

SourceSpan test_span() { return SourceSpan{}; }

ShapePtr array_shape(std::uint64_t exact_value, std::uint32_t capacity) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(exact_value);
  data.upper = make_literal(exact_value);
  data.capacity = capacity;
  data.element = scalar_shape();
  shape->data = data;
  return shape;
}

// Table 8's "literal" row (main.pdf p.40): "m<=N, element shapes
// kappa_i" -> "array(m,m,N;squnion_{i<m} kappa_i)". A single-element
// literal needs no actual join step -- the element shape passes through
// unchanged -- while lambda/u are both the literal's own exactly-known
// element count m.
void shape_of_literal_with_one_element_uses_that_elements_shape_unchanged() {
  const auto outcome = shape_of_literal(4, {scalar_shape()}, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(1)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(1)));
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(4));
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Scalar);
}

// Several elements with the *same* element shape: the folded join
// preserves that shared shape, and the outer array's own lambda=u=m
// (the element count), not anything derived from the elements'
// own exact/upper terms.
void shape_of_literal_with_several_identically_shaped_elements_preserves_the_shared_shape() {
  const std::vector<ShapePtr> elements = {array_shape(2, 4), array_shape(2, 4), array_shape(2, 4)};
  const auto outcome = shape_of_literal(8, elements, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(3))); // m=3, not the elements' own exact (2)
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(3)));
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Array);
  const auto &joined_element = std::get<ArrayShape>(result.element->data);
  BOUNDFIN_CHECK(joined_element.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**joined_element.exact, *make_literal(2)));
}

// Elements with *differing* exact shapes: the folded join falls back to
// inexact for the element summary (kappa-bar), confirming
// shape_of_literal actually threads join_shapes across every element,
// not just the first two.
void shape_of_literal_with_differently_shaped_elements_joins_to_inexact() {
  const std::vector<ShapePtr> elements = {array_shape(2, 4), array_shape(2, 4), array_shape(3, 4)};
  const auto outcome = shape_of_literal(8, elements, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  const auto &joined_element = std::get<ArrayShape>(result.element->data);
  BOUNDFIN_CHECK(!joined_element.exact.has_value());
}

// SIZ002: the literal's own element count exceeds its declared
// capacity -- AM-018's long-deferred "m<=N" check, finally enforced
// here (Table 8's "literal" row is the specific rule naming this
// premise; neither the parser nor typecheck checks it anywhere else).
void SIZ002_rejects_a_literal_whose_element_count_exceeds_capacity() {
  const std::vector<ShapePtr> elements = {scalar_shape(), scalar_shape(), scalar_shape()}; // m=3
  const auto outcome = shape_of_literal(2, elements, test_span()); // N=2
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ002"));
}

// The boundary m==N is accepted (the premise is "m<=N", not "m<N").
void shape_of_literal_accepts_element_count_exactly_equal_to_capacity() {
  const std::vector<ShapePtr> elements = {scalar_shape(), scalar_shape()}; // m=2
  const auto outcome = shape_of_literal(2, elements, test_span()); // N=2
  BOUNDFIN_CHECK(outcome.ok);
}

// INT001: an empty element-shape vector is a caller precondition
// violation -- AM-020 already confirmed typecheck's TYP008 rejects an
// empty array literal (m=0) before shape derivation ever runs, so this
// is never reachable via a real pipeline.
void shape_of_literal_rejects_with_INT001_for_an_empty_element_list() {
  const auto outcome = shape_of_literal(4, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// Regression for a spec-auditor FAIL (2026-10-06): a single-element
// literal (m=1) whose sole element shape is null previously bypassed
// every null check in this file -- the fold loop never runs for m=1, so
// join_shapes's own null guard never sees it, and an internally-
// inconsistent Shape (Array kind, null element) silently escaped with
// ok=true. Every element is now checked up front, independent of m.
void shape_of_literal_rejects_with_INT001_for_a_null_single_element() {
  const auto outcome = shape_of_literal(4, {nullptr}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// The same null check applies to a null element at any position, not
// just a lone element -- confirmed with a null second element among
// otherwise-valid ones.
void shape_of_literal_rejects_with_INT001_for_a_null_element_among_several() {
  const std::vector<ShapePtr> elements = {scalar_shape(), nullptr, scalar_shape()};
  const auto outcome = shape_of_literal(4, elements, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_literal() {
  shape_of_literal_with_one_element_uses_that_elements_shape_unchanged();
  shape_of_literal_with_several_identically_shaped_elements_preserves_the_shared_shape();
  shape_of_literal_with_differently_shaped_elements_joins_to_inexact();
  SIZ002_rejects_a_literal_whose_element_count_exceeds_capacity();
  shape_of_literal_accepts_element_count_exactly_equal_to_capacity();
  shape_of_literal_rejects_with_INT001_for_an_empty_element_list();
  shape_of_literal_rejects_with_INT001_for_a_null_single_element();
  shape_of_literal_rejects_with_INT001_for_a_null_element_among_several();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_literal)
