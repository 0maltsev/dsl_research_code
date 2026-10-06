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
using boundfin::source::size::shape::shape_of_index;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::shape_of_len;
using boundfin::source::size::shape::ShapePtr;

SourceSpan test_span() { return SourceSpan{}; }

ShapePtr array_shape_with_element(std::uint64_t exact_value, std::uint64_t upper_value, std::uint32_t capacity,
                                   ShapePtr element) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(exact_value);
  data.upper = make_literal(upper_value);
  data.capacity = capacity;
  data.element = std::move(element);
  shape->data = data;
  return shape;
}

// Table 8's "length/index" row, first half (main.pdf p.40): "array
// array(lambda,u,N;kappa-bar)" -> "scalar/count (lambda,u)" -- len(e)
// always has shape scalar (it returns i32), regardless of the array's
// own lambda/u/N/element.
void shape_of_len_is_always_scalar() {
  const auto array_like = array_shape_with_element(3, 3, 4, scalar_shape());
  const auto outcome = shape_of_len(array_like, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// Demonstrates, rather than only asserting in documentation, the
// pre-existing ArrayShape fact shape_of_len's own doc comment relies on:
// the "count (lambda,u)" half of the row's result is already directly
// available as the array's own ArrayShape.exact/.upper fields (public
// since the third slice, not new behavior introduced by shape_of_len or
// shape_of_index themselves) -- this is what will let Table 6's still-
// deferred C-Len-E/C-Len-U eventually pull (lambda,u) straight from an
// already-computed Table 8 shape, without calling shape_of_len at all.
void array_shapes_own_exact_and_upper_are_directly_the_len_count_refinement() {
  const auto array_like = array_shape_with_element(7, 9, 10, scalar_shape());
  const auto &data = std::get<ArrayShape>(array_like->data);
  BOUNDFIN_CHECK(data.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**data.exact, *make_literal(7)));
  BOUNDFIN_CHECK(terms_equal(*data.upper, *make_literal(9)));
}

// INT001: a non-Array operand is a caller precondition violation
// (Phase 3.2's TYP009 already rejects a non-array len operand before
// shape derivation runs).
void shape_of_len_rejects_with_INT001_for_a_non_array_operand() {
  const auto outcome = shape_of_len(scalar_shape(), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null operand is likewise a caller precondition violation.
void shape_of_len_rejects_with_INT001_for_a_null_operand() {
  const auto outcome = shape_of_len(nullptr, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// Table 8's "length/index" row, second half: "array array(lambda,u,N;
// kappa-bar)" -> "kappa-bar" -- indexing returns the array's own
// element summary, unchanged. Uses a distinguishable (Array-kind)
// element shape, not scalar, so the result can be confirmed to be
// exactly that element shape rather than some other trivial default.
void shape_of_index_returns_the_arrays_own_element_shape() {
  const auto element = array_shape_with_element(1, 1, 2, scalar_shape()); // a distinguishable, non-scalar element
  const auto array_like = array_shape_with_element(5, 5, 8, element);
  const auto outcome = shape_of_index(array_like, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result).get() == element.get());
}

// INT001: a non-Array operand is a caller precondition violation
// (Phase 3.2's TYP009 already rejects a non-array index operand before
// shape derivation runs).
void shape_of_index_rejects_with_INT001_for_a_non_array_operand() {
  const auto outcome = shape_of_index(scalar_shape(), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null operand is likewise a caller precondition violation.
void shape_of_index_rejects_with_INT001_for_a_null_operand() {
  const auto outcome = shape_of_index(nullptr, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_len_index() {
  shape_of_len_is_always_scalar();
  array_shapes_own_exact_and_upper_are_directly_the_len_count_refinement();
  shape_of_len_rejects_with_INT001_for_a_non_array_operand();
  shape_of_len_rejects_with_INT001_for_a_null_operand();
  shape_of_index_returns_the_arrays_own_element_shape();
  shape_of_index_rejects_with_INT001_for_a_non_array_operand();
  shape_of_index_rejects_with_INT001_for_a_null_operand();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_len_index)
