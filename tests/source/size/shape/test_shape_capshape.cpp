#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <memory>
#include <string>

namespace {

using boundfin::source::ast::ArrayType;
using boundfin::source::ast::ProductType;
using boundfin::source::ast::SourceSpan;
using boundfin::source::ast::Type;
using boundfin::source::ast::TypeKind;
using boundfin::source::ast::TypePtr;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::capshape;
using boundfin::source::size::shape::ProductShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::ShapeKind;

SourceSpan test_span() { return SourceSpan{}; }

TypePtr scalar_type(TypeKind kind) {
  auto type = std::make_shared<Type>();
  type->kind = kind;
  return type;
}

TypePtr array_type(TypePtr element, std::uint32_t capacity) {
  auto type = std::make_shared<Type>();
  type->kind = TypeKind::Array;
  type->data = ArrayType{std::move(element), capacity};
  return type;
}

TypePtr product_type(std::vector<TypePtr> components) {
  auto type = std::make_shared<Type>();
  type->kind = TypeKind::Product;
  type->data = ProductType{std::move(components)};
  return type;
}

// Table 8's "capacity fallback" row (main.pdf p.40): "type arr(tau,N)"
// -> "array(star,N,N;capshape(tau))". capshape itself (p.11) is a
// general recursive function over the whole Type grammar; every scalar
// TypeKind (Bool/I32/I64/F64) gives `scalar`, matching kappa's own
// grammar having no scalar sub-kind to distinguish.
void capshape_of_every_scalar_kind_is_scalar() {
  for (const auto kind : {TypeKind::Bool, TypeKind::I32, TypeKind::I64, TypeKind::F64}) {
    const auto outcome = capshape(scalar_type(kind), test_span());
    BOUNDFIN_CHECK(outcome.ok);
    BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
  }
}

// An array type's own capshape is exactly the "capacity fallback" row's
// own result: array(star,N,N;capshape(element)) -- exact absent, upper
// and capacity both the declared N.
void capshape_of_an_array_type_is_the_capacity_fallback_row_result() {
  const auto outcome = capshape(array_type(scalar_type(TypeKind::I32), 8), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &data = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!data.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*data.upper, *make_literal(8)));
  BOUNDFIN_CHECK_EQ(data.capacity, static_cast<std::uint32_t>(8));
  BOUNDFIN_CHECK(data.element->kind == ShapeKind::Scalar);
}

// "recursively replaces every array length in a type by its capacity":
// a nested array-of-arrays recurses into the *element* type too, not
// just the outer array.
void capshape_recurses_into_nested_array_element_types() {
  const auto inner = array_type(scalar_type(TypeKind::I32), 4);
  const auto outer = array_type(inner, 8);
  const auto outcome = capshape(outer, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &outer_data = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK_EQ(outer_data.capacity, static_cast<std::uint32_t>(8));
  BOUNDFIN_CHECK(outer_data.element->kind == ShapeKind::Array);
  const auto &inner_data = std::get<ArrayShape>(outer_data.element->data);
  BOUNDFIN_CHECK(!inner_data.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*inner_data.upper, *make_literal(4)));
  BOUNDFIN_CHECK_EQ(inner_data.capacity, static_cast<std::uint32_t>(4));
}

// A product type recurses component-wise, in declared order, reusing
// shape_of_product.
void capshape_of_a_product_type_recurses_componentwise() {
  const auto outcome =
      capshape(product_type({scalar_type(TypeKind::Bool), array_type(scalar_type(TypeKind::I32), 4)}), test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Product);
  const auto &components = std::get<ProductShape>((*outcome.result)->data).components;
  BOUNDFIN_CHECK_EQ(components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(components[0]->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(components[1]->kind == ShapeKind::Array);
  const auto &second_data = std::get<ArrayShape>(components[1]->data);
  BOUNDFIN_CHECK_EQ(second_data.capacity, static_cast<std::uint32_t>(4));
}

// A product containing an array containing a product confirms the
// recursion genuinely threads through every nesting combination, not
// just one level of either kind.
void capshape_recurses_through_mixed_nested_aggregates() {
  const auto innermost_product = product_type({scalar_type(TypeKind::F64), scalar_type(TypeKind::I64)});
  const auto array_of_products = array_type(innermost_product, 3);
  const auto outer_product = product_type({scalar_type(TypeKind::Bool), array_of_products});
  const auto outcome = capshape(outer_product, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &outer_components = std::get<ProductShape>((*outcome.result)->data).components;
  BOUNDFIN_CHECK(outer_components[1]->kind == ShapeKind::Array);
  const auto &array_data = std::get<ArrayShape>(outer_components[1]->data);
  BOUNDFIN_CHECK_EQ(array_data.capacity, static_cast<std::uint32_t>(3));
  BOUNDFIN_CHECK(array_data.element->kind == ShapeKind::Product);
  const auto &innermost_components = std::get<ProductShape>(array_data.element->data).components;
  BOUNDFIN_CHECK_EQ(innermost_components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(innermost_components[0]->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(innermost_components[1]->kind == ShapeKind::Scalar);
}

// Regression for a spec-auditor FAIL (2026-10-06): capshape's first
// version had no defensive null check at all, unlike every sibling
// function in this module, justified by an analogy to shape_of_product
// that did not actually hold (shape_of_product never dereferences its
// own elements; capshape does, immediately, at every recursion level,
// and its only real caller today is this module's own hand-built test
// fixtures, not a guaranteed-non-null pipeline). A null top-level type
// is INT001.
void capshape_rejects_with_INT001_for_a_null_top_level_type() {
  const auto outcome = capshape(nullptr, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// The same null check applies at any recursion depth, not just the top
// level: a null array element type.
void capshape_rejects_with_INT001_for_a_null_array_element_type() {
  const auto outcome = capshape(array_type(nullptr, 4), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// ...and a null product component type.
void capshape_rejects_with_INT001_for_a_null_product_component_type() {
  const auto outcome = capshape(product_type({scalar_type(TypeKind::Bool), nullptr}), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_capshape() {
  capshape_of_every_scalar_kind_is_scalar();
  capshape_of_an_array_type_is_the_capacity_fallback_row_result();
  capshape_recurses_into_nested_array_element_types();
  capshape_of_a_product_type_recurses_componentwise();
  capshape_recurses_through_mixed_nested_aggregates();
  capshape_rejects_with_INT001_for_a_null_top_level_type();
  capshape_rejects_with_INT001_for_a_null_array_element_type();
  capshape_rejects_with_INT001_for_a_null_product_component_type();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_capshape)
