#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::make_max;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::join_shapes;
using boundfin::source::size::shape::ProductShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::shape_of_product;
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

// scalar join scalar = scalar (main.pdf p.11's conditional-row prose,
// reused verbatim for every Table 8 row that needs this join).
void join_shapes_of_two_scalars_is_scalar() {
  const auto outcome = join_shapes(scalar_shape(), scalar_shape(), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// "A conditional preserves an exact length only when both branches
// establish the same expression": two array shapes with the *same*
// exact term join to a result retaining that exact term, with upper =
// max(u1,u2).
void join_shapes_of_two_arrays_with_the_same_exact_term_preserves_it() {
  const auto a = array_shape(5, 5, 8, scalar_shape());
  const auto b = array_shape(5, 7, 8, scalar_shape()); // same exact (5), different upper
  const auto outcome = join_shapes(a, b, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &joined = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(joined.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**joined.exact, *make_literal(5)));
  // Terms are never arithmetically simplified (count.cpp's own
  // established precedent): upper is literally max(5,7), not reduced
  // to the literal 7.
  BOUNDFIN_CHECK(terms_equal(*joined.upper, *make_max(make_literal(5), make_literal(7))));
}

// Differing exact terms: "otherwise it has exact component star."
void join_shapes_of_two_arrays_with_differing_exact_terms_is_inexact() {
  const auto a = array_shape(3, 3, 8, scalar_shape());
  const auto b = array_shape(5, 5, 8, scalar_shape());
  const auto outcome = join_shapes(a, b, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &joined = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!joined.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*joined.upper, *make_max(make_literal(3), make_literal(5))));
}

// Either operand already inexact (star): the join is inexact too, even
// though no two *numbers* actually differ.
void join_shapes_of_an_exact_and_an_inexact_array_is_inexact() {
  const auto a = array_shape(std::nullopt, 6, 8, scalar_shape());
  const auto b = array_shape(4, 4, 8, scalar_shape());
  const auto outcome = join_shapes(a, b, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &joined = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!joined.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*joined.upper, *make_max(make_literal(6), make_literal(4))));
}

// "it joins element summaries recursively": a join of array-of-array
// shapes recurses into the inner element shapes too, not just the
// outer lambda/u.
void join_shapes_of_nested_arrays_joins_elements_recursively() {
  const auto inner_a = array_shape(2, 2, 4, scalar_shape());
  const auto inner_b = array_shape(3, 3, 4, scalar_shape()); // differs -> inner join is inexact
  const auto outer_a = array_shape(1, 1, 4, inner_a);
  const auto outer_b = array_shape(1, 1, 4, inner_b);
  const auto outcome = join_shapes(outer_a, outer_b, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &outer_joined = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(outer_joined.exact.has_value()); // outer exacts (1,1) match
  const auto &inner_joined = std::get<ArrayShape>(outer_joined.element->data);
  BOUNDFIN_CHECK(!inner_joined.exact.has_value()); // inner exacts (2,3) differ
  BOUNDFIN_CHECK(terms_equal(*inner_joined.upper, *make_max(make_literal(2), make_literal(3))));
}

// Products join component-wise, preserving order.
void join_shapes_of_two_products_joins_componentwise() {
  const auto first = shape_of_product({array_shape(1, 1, 4, scalar_shape()), scalar_shape()});
  const auto second = shape_of_product({array_shape(2, 2, 4, scalar_shape()), scalar_shape()});
  const auto outcome = join_shapes(first, second, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &joined = std::get<ProductShape>((*outcome.result)->data).components;
  BOUNDFIN_CHECK_EQ(joined.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(joined[0]->kind == ShapeKind::Array);
  const auto &first_component = std::get<ArrayShape>(joined[0]->data);
  BOUNDFIN_CHECK(!first_component.exact.has_value()); // (1,...) vs (2,...) differ
  BOUNDFIN_CHECK(joined[1]->kind == ShapeKind::Scalar);
}

// INT001: a kind mismatch (scalar vs. array) is a caller precondition
// violation -- every real call site joins two shapes with the same
// static type, so this never happens on a genuinely typechecked module.
void join_shapes_rejects_with_INT001_for_a_kind_mismatch() {
  const auto outcome = join_shapes(scalar_shape(), array_shape(1, 1, 4, scalar_shape()), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: an array capacity mismatch is likewise a caller precondition
// violation (two same-typed arrays always share a declared capacity).
void join_shapes_rejects_with_INT001_for_an_array_capacity_mismatch() {
  const auto a = array_shape(1, 1, 4, scalar_shape());
  const auto b = array_shape(1, 1, 8, scalar_shape());
  const auto outcome = join_shapes(a, b, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a product arity mismatch is likewise a caller precondition
// violation (two same-typed products always share an arity).
void join_shapes_rejects_with_INT001_for_a_product_arity_mismatch() {
  const auto first = shape_of_product({scalar_shape(), scalar_shape()});
  const auto second = shape_of_product({scalar_shape(), scalar_shape(), scalar_shape()});
  const auto outcome = join_shapes(first, second, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null operand (either position) is a caller precondition
// violation, mirroring shape_of_proj's own null-operand coverage.
void join_shapes_rejects_with_INT001_for_a_null_operand() {
  const auto first_null = join_shapes(nullptr, scalar_shape(), test_span());
  BOUNDFIN_CHECK(!first_null.ok);
  BOUNDFIN_CHECK_EQ(first_null.diagnostic->code, std::string("INT001"));

  const auto second_null = join_shapes(scalar_shape(), nullptr, test_span());
  BOUNDFIN_CHECK(!second_null.ok);
  BOUNDFIN_CHECK_EQ(second_null.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_join() {
  join_shapes_of_two_scalars_is_scalar();
  join_shapes_of_two_arrays_with_the_same_exact_term_preserves_it();
  join_shapes_of_two_arrays_with_differing_exact_terms_is_inexact();
  join_shapes_of_an_exact_and_an_inexact_array_is_inexact();
  join_shapes_of_nested_arrays_joins_elements_recursively();
  join_shapes_of_two_products_joins_componentwise();
  join_shapes_rejects_with_INT001_for_a_kind_mismatch();
  join_shapes_rejects_with_INT001_for_an_array_capacity_mismatch();
  join_shapes_rejects_with_INT001_for_a_product_arity_mismatch();
  join_shapes_rejects_with_INT001_for_a_null_operand();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_join)
