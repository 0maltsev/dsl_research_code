#include "boundfin/source/size/shape/shape.hpp"

namespace boundfin::source::size::shape {

ShapePtr scalar_shape() {
  static const ShapePtr instance = [] {
    auto shape = std::make_shared<Shape>();
    shape->kind = ShapeKind::Scalar;
    shape->data = std::monostate{};
    return shape;
  }();
  return instance;
}

ShapeOutcome shape_of_var(const ast::VarExpr &var_expr, SourceSpan span, const ShapeContext &context) {
  if (!var_expr.resolved_binding) {
    // Unreachable for any module that went through Phase 3.1's resolver
    // (which always sets resolved_binding on every VarExpr); matches
    // src/source/size/count's infer_var's identical defensive branch.
    return ShapeOutcome{false, std::nullopt,
                         ShapeDiagnostic{"INT001", span, "internal: variable reference has no resolved binding"}};
  }
  const auto it = context.find(*var_expr.resolved_binding);
  if (it == context.end()) {
    // See shape.hpp's doc comment on shape_of_var: unlike Table 6's
    // IndexContext, a resolved, in-scope variable always has a Table 8
    // shape once `context` is correctly populated -- a missing entry is
    // a caller precondition violation, not a true fact about this
    // program.
    return ShapeOutcome{false, std::nullopt,
                         ShapeDiagnostic{"INT001", span, "internal: variable reference has no recorded shape"}};
  }
  return ShapeOutcome{true, it->second, std::nullopt};
}

ShapePtr shape_of_product(std::vector<ShapePtr> component_shapes) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Product;
  shape->data = ProductShape{std::move(component_shapes)};
  return shape;
}

ShapeOutcome shape_of_proj(const ShapePtr &operand_shape, std::uint32_t index, SourceSpan span) {
  if (!operand_shape || operand_shape->kind != ShapeKind::Product) {
    // See shape.hpp's doc comment on shape_of_proj: Phase 3.2's TYP006
    // already rejects a non-product projection operand before shape
    // derivation runs.
    return ShapeOutcome{false, std::nullopt,
                         ShapeDiagnostic{"INT001", span, "internal: projection operand is not a product shape"}};
  }
  const auto &components = std::get<ProductShape>(operand_shape->data).components;
  if (index < 1 || index > components.size()) {
    // Phase 3.2's TYP007 already rejects an out-of-arity index.
    return ShapeOutcome{false, std::nullopt,
                         ShapeDiagnostic{"INT001", span, "internal: projection index out of recorded component range"}};
  }
  return ShapeOutcome{true, components[index - 1], std::nullopt};
}

} // namespace boundfin::source::size::shape
