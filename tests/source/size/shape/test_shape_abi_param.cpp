#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <memory>
#include <string>

namespace {

using boundfin::source::ast::ArrayType;
using boundfin::source::ast::BindingId;
using boundfin::source::ast::SourceSpan;
using boundfin::source::ast::Type;
using boundfin::source::ast::TypeKind;
using boundfin::source::ast::TypePtr;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::make_symbol;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::shape_of_abi_array_param;
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

// main.pdf p.11 main text: "ABI array argument x:arr<tau,N> receives a
// fresh formal length n_x under 0<=n_x<=N. Its shape is
// array(n_x,n_x,N;capshape(tau))." AM-035 (step 1 of the Sigma/K_f/Q_f
// induction ladder): n_x is minted deterministically as
// "abi#" + to_string(param_binding).
void shape_of_abi_array_param_mints_an_exact_shape_with_the_declared_capacity() {
  const auto type = array_type(scalar_type(TypeKind::I32), 10);
  const auto outcome = shape_of_abi_array_param(type, BindingId{42}, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  const auto expected_symbol = make_symbol("abi#42");
  BOUNDFIN_CHECK(terms_equal(**result.exact, *expected_symbol));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *expected_symbol)); // exact == upper, both n_x
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(10));
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Scalar);
}

// Two distinct ABI array parameters (distinct BindingId) mint two
// distinct, structurally-unequal symbols -- confirms the minting is
// genuinely per-binding, not a fixed/reused literal.
void shape_of_abi_array_param_mints_distinct_symbols_for_distinct_bindings() {
  const auto type = array_type(scalar_type(TypeKind::I32), 10);
  const auto first = shape_of_abi_array_param(type, BindingId{1}, test_span());
  const auto second = shape_of_abi_array_param(type, BindingId{2}, test_span());
  BOUNDFIN_CHECK(first.ok);
  BOUNDFIN_CHECK(second.ok);
  const auto &first_result = std::get<ArrayShape>((*first.result)->data);
  const auto &second_result = std::get<ArrayShape>((*second.result)->data);
  BOUNDFIN_CHECK(!terms_equal(**first_result.exact, **second_result.exact));
}

// The element shape is capshape(tau) exactly, reusing capshape rather
// than reimplementing its recursion -- confirmed here with a nested
// array-of-array element type, where capshape's own star-shaped,
// capacity-only recursion is directly observable.
void shape_of_abi_array_param_recurses_into_the_element_type_via_capshape() {
  const auto inner_array = array_type(scalar_type(TypeKind::I32), 4);
  const auto type = array_type(inner_array, 10);
  const auto outcome = shape_of_abi_array_param(type, BindingId{5}, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Array);
  const auto &inner = std::get<ArrayShape>(result.element->data);
  BOUNDFIN_CHECK(!inner.exact.has_value()); // capshape's own "star" for a nested array
  BOUNDFIN_CHECK(terms_equal(*inner.upper, *make_literal(4)));
  BOUNDFIN_CHECK_EQ(inner.capacity, static_cast<std::uint32_t>(4));
}

// INT001: a non-Array type is a caller precondition violation -- this
// function implements main.pdf's own sentence, specific to "ABI array
// argument x:arr<tau,N>", not a general rule over every ABI parameter
// type (AM-035's own documented scope).
void shape_of_abi_array_param_rejects_with_INT001_for_a_non_array_type() {
  const auto outcome = shape_of_abi_array_param(scalar_type(TypeKind::I32), BindingId{1}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null type is a caller precondition violation, mirroring
// capshape's identical null-safety convention.
void shape_of_abi_array_param_rejects_with_INT001_for_a_null_type() {
  const auto outcome = shape_of_abi_array_param(nullptr, BindingId{1}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_abi_param() {
  shape_of_abi_array_param_mints_an_exact_shape_with_the_declared_capacity();
  shape_of_abi_array_param_mints_distinct_symbols_for_distinct_bindings();
  shape_of_abi_array_param_recurses_into_the_element_type_via_capshape();
  shape_of_abi_array_param_rejects_with_INT001_for_a_non_array_type();
  shape_of_abi_array_param_rejects_with_INT001_for_a_null_type();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_abi_param)
