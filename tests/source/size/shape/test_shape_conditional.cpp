#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

namespace {

using boundfin::source::ast::SourceSpan;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::join_shapes;
using boundfin::source::size::shape::ProductShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::shape_of_conditional;
using boundfin::source::size::shape::shape_of_product;
using boundfin::source::size::shape::ShapePtr;

SourceSpan test_span() { return SourceSpan{}; }

ShapePtr array_shape(std::optional<std::uint64_t> exact_value, std::uint64_t upper_value, std::uint32_t capacity) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = exact_value ? std::optional{make_literal(*exact_value)} : std::nullopt;
  data.upper = make_literal(upper_value);
  data.capacity = capacity;
  data.element = scalar_shape();
  shape->data = data;
  return shape;
}

// Table 8's "conditional" row (main.pdf p.40): "branch shapes
// kappa_t,kappa_f" -> "J(kappa_t,kappa_f); exact length retained iff
// equal". shape_of_conditional is a deliberately thin wrapper over
// join_shapes (already implemented and spec-auditor-reviewed in the
// third slice); this confirms it actually delegates rather than
// diverging, across all three shape kinds.
void shape_of_conditional_matches_join_shapes_for_scalar_branches() {
  const auto via_conditional = shape_of_conditional(scalar_shape(), scalar_shape(), test_span());
  const auto via_join = join_shapes(scalar_shape(), scalar_shape(), test_span());
  BOUNDFIN_CHECK(via_conditional.ok);
  BOUNDFIN_CHECK(via_join.ok);
  BOUNDFIN_CHECK((*via_conditional.result)->kind == (*via_join.result)->kind);
}

// "exact length retained iff equal": matching exact lengths on both
// branches are preserved.
void shape_of_conditional_preserves_exact_length_when_branches_agree() {
  const auto then_branch = array_shape(5, 5, 8);
  const auto else_branch = array_shape(5, 5, 8);
  const auto outcome = shape_of_conditional(then_branch, else_branch, test_span());
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(5)));
}

// Differing branch exact lengths fall back to inexact (star), matching
// join_shapes's own already-tested behavior for the identical scenario.
void shape_of_conditional_is_inexact_when_branches_differ() {
  const auto then_branch = array_shape(3, 3, 8);
  const auto else_branch = array_shape(5, 5, 8);
  const auto via_conditional = shape_of_conditional(then_branch, else_branch, test_span());
  const auto via_join = join_shapes(then_branch, else_branch, test_span());
  BOUNDFIN_CHECK(via_conditional.ok);
  const auto &conditional_result = std::get<ArrayShape>((*via_conditional.result)->data);
  BOUNDFIN_CHECK(!conditional_result.exact.has_value());
  // Identical outcome either way (same underlying operation).
  const auto &join_result = std::get<ArrayShape>((*via_join.result)->data);
  BOUNDFIN_CHECK(terms_equal(*conditional_result.upper, *join_result.upper));
}

// Product branches: a spec-auditor review of this slice flagged that
// no test exercised ShapeKind::Product through shape_of_conditional
// (only Scalar and Array), the one join_shapes case whose "component-
// wise" behavior is a structural reading of "J is recursive join"
// rather than being directly spelled out in the paper's own prose --
// confirms the wrapper delegates correctly for this case too, not just
// the two already-covered kinds.
void shape_of_conditional_matches_join_shapes_for_product_branches() {
  const auto then_branch = shape_of_product({array_shape(1, 1, 4), scalar_shape()});
  const auto else_branch = shape_of_product({array_shape(2, 2, 4), scalar_shape()});
  const auto via_conditional = shape_of_conditional(then_branch, else_branch, test_span());
  const auto via_join = join_shapes(then_branch, else_branch, test_span());
  BOUNDFIN_CHECK(via_conditional.ok);
  BOUNDFIN_CHECK(via_join.ok);
  const auto &conditional_components = std::get<ProductShape>((*via_conditional.result)->data).components;
  const auto &join_components = std::get<ProductShape>((*via_join.result)->data).components;
  BOUNDFIN_CHECK_EQ(conditional_components.size(), join_components.size());
  const auto &conditional_first = std::get<ArrayShape>(conditional_components[0]->data);
  // Differing exact lengths (1 vs 2) in the first component fold to
  // inexact, confirming the wrapper actually recurses through the
  // product's own components rather than stopping at the top level.
  BOUNDFIN_CHECK(!conditional_first.exact.has_value());
}

// INT001 propagates through the wrapper exactly as join_shapes itself
// would raise it (a kind mismatch between two branches, which Phase
// 3.2's TYP005 already precludes for a real typechecked conditional).
void shape_of_conditional_propagates_INT001_for_a_kind_mismatch() {
  const auto outcome = shape_of_conditional(scalar_shape(), array_shape(1, 1, 4), test_span());
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_conditional() {
  shape_of_conditional_matches_join_shapes_for_scalar_branches();
  shape_of_conditional_preserves_exact_length_when_branches_agree();
  shape_of_conditional_is_inexact_when_branches_differ();
  shape_of_conditional_matches_join_shapes_for_product_branches();
  shape_of_conditional_propagates_INT001_for_a_kind_mismatch();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_conditional)
