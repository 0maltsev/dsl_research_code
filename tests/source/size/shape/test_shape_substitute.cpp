#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>
#include <unordered_map>

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::make_symbol;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::certificate::TermPtr;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::ProductShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::ShapePtr;
using boundfin::source::size::shape::shape_of_product;
using boundfin::source::size::shape::substitute_shape;

SourceSpan test_span() { return SourceSpan{}; }

// AM-037 step 4a: the Shape-level composition of
// certificate::substitute_term, implementing main.pdf p.12's call-site
// substitution over a whole Shape tree -- the mechanism Table 8's own
// "call" row needs.

ShapePtr array_shape(std::optional<TermPtr> exact, TermPtr upper, std::uint32_t capacity, ShapePtr element) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  shape->data = ArrayShape{std::move(exact), std::move(upper), capacity, std::move(element)};
  return shape;
}

void substitute_shape_leaves_a_scalar_unchanged() {
  const auto outcome = substitute_shape(scalar_shape(), {}, {}, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// The same formal symbol name ("n_x") resolves to a DIFFERENT actual
// term depending on whether it is found in the exact or the upper
// position -- confirms the two substitution maps are genuinely separate,
// not one map reused for both positions.
void substitute_shape_resolves_exact_and_upper_independently_for_the_same_symbol_name() {
  const auto n_x = make_symbol("n_x");
  const auto shape = array_shape(n_x, n_x, 10, scalar_shape());
  const std::unordered_map<std::string, TermPtr> exact_substitution{{"n_x", make_literal(3)}};
  const std::unordered_map<std::string, TermPtr> upper_substitution{{"n_x", make_literal(10)}};
  const auto outcome = substitute_shape(shape, exact_substitution, upper_substitution, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(3)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(10)));
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(10)); // capacity carried over unchanged
}

// An already-absent exact (star) stays absent -- nothing to substitute.
void substitute_shape_leaves_an_absent_exact_as_star() {
  const auto shape = array_shape(std::nullopt, make_symbol("u"), 10, scalar_shape());
  const std::unordered_map<std::string, TermPtr> upper_substitution{{"u", make_literal(10)}};
  const auto outcome = substitute_shape(shape, {}, upper_substitution, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!result.exact.has_value());
}

// main.pdf p.12: "if an exact actual is unavailable, upper substitution
// yields a conservative star result" -- a PRESENT exact term whose own
// substitution fails (the symbol it mentions has no entry in
// exact_substitution) is this named, legitimate case: the result's own
// exact becomes absent (star), and the function still reports ok=true,
// not a failure.
void substitute_shape_falls_back_to_star_when_an_exact_substitution_is_unavailable() {
  const auto shape = array_shape(make_symbol("n_x"), make_symbol("n_x"), 10, scalar_shape());
  const std::unordered_map<std::string, TermPtr> upper_substitution{{"n_x", make_literal(10)}};
  // exact_substitution deliberately has no entry for "n_x".
  const auto outcome = substitute_shape(shape, {}, upper_substitution, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(!result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(10))); // upper still resolved
}

// T-Call's "ordered actual types agree" premise guarantees every formal
// symbol in an upper position resolves; a failed upper substitution is
// therefore a genuinely incomplete substitution map -- INT001, not a
// star fallback (which is exact-only).
void substitute_shape_rejects_with_INT001_when_an_upper_substitution_is_unavailable() {
  const auto shape = array_shape(std::nullopt, make_symbol("unmapped"), 10, scalar_shape());
  const auto outcome = substitute_shape(shape, {}, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// Recurses into ArrayShape::element.
void substitute_shape_recurses_into_the_element_shape() {
  const auto inner = array_shape(make_symbol("a"), make_symbol("a"), 5, scalar_shape());
  const auto outer = array_shape(std::nullopt, make_literal(1), 1, inner);
  const std::unordered_map<std::string, TermPtr> exact_substitution{{"a", make_literal(2)}};
  const std::unordered_map<std::string, TermPtr> upper_substitution{{"a", make_literal(2)}};
  const auto outcome = substitute_shape(outer, exact_substitution, upper_substitution, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &outer_result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(outer_result.element->kind == ShapeKind::Array);
  const auto &inner_result = std::get<ArrayShape>(outer_result.element->data);
  BOUNDFIN_CHECK(terms_equal(**inner_result.exact, *make_literal(2)));
}

// Recurses into every ProductShape::components entry, in order.
void substitute_shape_recurses_into_every_product_component() {
  const auto first = array_shape(make_symbol("a"), make_symbol("a"), 5, scalar_shape());
  const auto second = array_shape(make_symbol("b"), make_symbol("b"), 7, scalar_shape());
  const auto product = shape_of_product({first, second});
  const std::unordered_map<std::string, TermPtr> exact_substitution{{"a", make_literal(1)}, {"b", make_literal(2)}};
  const std::unordered_map<std::string, TermPtr> upper_substitution{{"a", make_literal(1)}, {"b", make_literal(2)}};
  const auto outcome = substitute_shape(product, exact_substitution, upper_substitution, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &components = std::get<ProductShape>((*outcome.result)->data).components;
  BOUNDFIN_CHECK_EQ(components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(terms_equal(**std::get<ArrayShape>(components[0]->data).exact, *make_literal(1)));
  BOUNDFIN_CHECK(terms_equal(**std::get<ArrayShape>(components[1]->data).exact, *make_literal(2)));
}

// A failure anywhere in a nested structure propagates to the top.
void substitute_shape_propagates_a_nested_failure() {
  const auto inner = array_shape(std::nullopt, make_symbol("unmapped"), 5, scalar_shape());
  const auto outer = array_shape(std::nullopt, make_literal(1), 1, inner);
  const auto outcome = substitute_shape(outer, {}, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void substitute_shape_rejects_with_INT001_for_a_null_shape() {
  const auto outcome = substitute_shape(nullptr, {}, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void substitute_shape_rejects_with_INT001_for_a_null_upper_term() {
  const auto shape = array_shape(std::nullopt, nullptr, 10, scalar_shape());
  const auto outcome = substitute_shape(shape, {}, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void substitute_shape_rejects_with_INT001_for_a_null_exact_term() {
  const auto shape = array_shape(TermPtr{nullptr}, make_literal(10), 10, scalar_shape());
  const auto outcome = substitute_shape(shape, {}, {}, test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_substitute() {
  substitute_shape_leaves_a_scalar_unchanged();
  substitute_shape_resolves_exact_and_upper_independently_for_the_same_symbol_name();
  substitute_shape_leaves_an_absent_exact_as_star();
  substitute_shape_falls_back_to_star_when_an_exact_substitution_is_unavailable();
  substitute_shape_rejects_with_INT001_when_an_upper_substitution_is_unavailable();
  substitute_shape_recurses_into_the_element_shape();
  substitute_shape_recurses_into_every_product_component();
  substitute_shape_propagates_a_nested_failure();
  substitute_shape_rejects_with_INT001_for_a_null_shape();
  substitute_shape_rejects_with_INT001_for_a_null_upper_term();
  substitute_shape_rejects_with_INT001_for_a_null_exact_term();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_substitute)
