#include "boundfin/source/size/shape/shape.hpp"

namespace boundfin::source::size::shape {

namespace {

ShapeOutcome shape_fail(std::string code, SourceSpan span, std::string message) {
  return ShapeOutcome{false, std::nullopt, ShapeDiagnostic{std::move(code), span, std::move(message)}};
}

ShapeOutcome shape_ok(ShapePtr shape) { return ShapeOutcome{true, std::move(shape), std::nullopt}; }

// Structural shape comparison, used only by shape_of_fold's own
// fixed-point check (see shape.hpp's doc comment on shape_of_fold and
// AM-033). Three-valued, not boolean, to distinguish two different
// reasons two shapes can fail to coincide: `Incompatible` (different
// kind, array capacity, or product arity at some level -- a structural
// mismatch that T-Fold/TYP010 already rules out for any real,
// legitimately-typechecked fold, since the body's synthesized type is
// required to match the accumulator's own declared type at every
// iteration; this mirrors join_shapes's own AM-028-settled "a kind/
// capacity/arity mismatch is a caller precondition violation, not a
// program property" reasoning, applied here to the identical situation)
// versus `Differs` (same structural shape, but an exact/upper term or a
// nested element/component genuinely differs -- the real "fold
// recurrence cannot be formed" case SIZ008 was reserved for). A null
// child anywhere is `Incompatible`, matching every other null-safety
// check in this module.
enum class ShapeComparison { Equal, Differs, Incompatible };

ShapeComparison compare_shapes(const ShapePtr &first, const ShapePtr &second) {
  if (!first || !second) {
    return ShapeComparison::Incompatible;
  }
  if (first->kind != second->kind) {
    return ShapeComparison::Incompatible;
  }
  switch (first->kind) {
  case ShapeKind::Scalar:
    return ShapeComparison::Equal;
  case ShapeKind::Array: {
    const auto &first_array = std::get<ArrayShape>(first->data);
    const auto &second_array = std::get<ArrayShape>(second->data);
    if (first_array.capacity != second_array.capacity) {
      return ShapeComparison::Incompatible;
    }
    const auto element_comparison = compare_shapes(first_array.element, second_array.element);
    if (element_comparison == ShapeComparison::Incompatible) {
      return ShapeComparison::Incompatible;
    }
    bool differs = element_comparison == ShapeComparison::Differs;
    if (first_array.exact.has_value() != second_array.exact.has_value()) {
      differs = true;
    } else if (first_array.exact) {
      if (!*first_array.exact || !*second_array.exact) {
        return ShapeComparison::Incompatible;
      }
      if (!certificate::terms_equal(**first_array.exact, **second_array.exact)) {
        differs = true;
      }
    }
    if (!first_array.upper || !second_array.upper) {
      return ShapeComparison::Incompatible;
    }
    if (!certificate::terms_equal(*first_array.upper, *second_array.upper)) {
      differs = true;
    }
    return differs ? ShapeComparison::Differs : ShapeComparison::Equal;
  }
  case ShapeKind::Product: {
    const auto &first_components = std::get<ProductShape>(first->data).components;
    const auto &second_components = std::get<ProductShape>(second->data).components;
    if (first_components.size() != second_components.size()) {
      return ShapeComparison::Incompatible;
    }
    bool differs = false;
    for (std::size_t i = 0; i < first_components.size(); ++i) {
      const auto component_comparison = compare_shapes(first_components[i], second_components[i]);
      if (component_comparison == ShapeComparison::Incompatible) {
        return ShapeComparison::Incompatible;
      }
      if (component_comparison == ShapeComparison::Differs) {
        differs = true;
      }
    }
    return differs ? ShapeComparison::Differs : ShapeComparison::Equal;
  }
  }
  // Unreachable (exhaustive switch over every ShapeKind).
  return ShapeComparison::Incompatible;
}

} // namespace

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

ShapeOutcome join_shapes(const ShapePtr &first, const ShapePtr &second, SourceSpan span) {
  if (!first || !second) {
    return shape_fail("INT001", span, "internal: join operand is null");
  }
  if (first->kind != second->kind) {
    // See shape.hpp's doc comment on join_shapes: every call site joins
    // two shapes that provably share a static type, so a kind mismatch
    // here is a caller precondition violation, not a program property.
    return shape_fail("INT001", span, "internal: join operands have different shape kinds");
  }
  switch (first->kind) {
  case ShapeKind::Scalar:
    return shape_ok(scalar_shape());
  case ShapeKind::Array: {
    const auto &first_array = std::get<ArrayShape>(first->data);
    const auto &second_array = std::get<ArrayShape>(second->data);
    if (first_array.capacity != second_array.capacity) {
      return shape_fail("INT001", span, "internal: join operands have different array capacities");
    }
    const auto element_join = join_shapes(first_array.element, second_array.element, span);
    if (!element_join.ok) {
      return element_join;
    }
    ArrayShape joined;
    joined.capacity = first_array.capacity;
    joined.upper = certificate::make_max(first_array.upper, second_array.upper);
    if (first_array.exact && second_array.exact &&
        certificate::terms_equal(**first_array.exact, **second_array.exact)) {
      joined.exact = *first_array.exact;
    } else {
      joined.exact = std::nullopt;
    }
    joined.element = *element_join.result;
    auto shape = std::make_shared<Shape>();
    shape->kind = ShapeKind::Array;
    shape->data = joined;
    return shape_ok(shape);
  }
  case ShapeKind::Product: {
    const auto &first_components = std::get<ProductShape>(first->data).components;
    const auto &second_components = std::get<ProductShape>(second->data).components;
    if (first_components.size() != second_components.size()) {
      return shape_fail("INT001", span, "internal: join operands have different product arities");
    }
    std::vector<ShapePtr> joined_components;
    joined_components.reserve(first_components.size());
    for (std::size_t i = 0; i < first_components.size(); ++i) {
      const auto component_join = join_shapes(first_components[i], second_components[i], span);
      if (!component_join.ok) {
        return component_join;
      }
      joined_components.push_back(*component_join.result);
    }
    return shape_ok(shape_of_product(std::move(joined_components)));
  }
  }
  // Unreachable (exhaustive switch over every ShapeKind); INT001, not a
  // SIZ/user-facing code, matching this module's other defensive
  // branches.
  return shape_fail("INT001", span, "internal: unreachable shape kind");
}

ShapeOutcome shape_of_literal(std::uint32_t capacity, std::vector<ShapePtr> element_shapes, SourceSpan span) {
  if (element_shapes.empty()) {
    // AM-020: typecheck's TYP008 already rejects an empty array literal
    // (m=0) before shape derivation ever runs.
    return shape_fail("INT001", span, "internal: array literal has no element shapes");
  }
  for (const auto &element_shape : element_shapes) {
    if (!element_shape) {
      // A null element shape (e.g. a single-element literal, m=1, whose
      // only element is null) would otherwise bypass every null check
      // in this file: with m=1 the fold loop below never runs, so
      // join_shapes's own "!first || !second" guard never sees it, and
      // an internally-inconsistent Shape (ShapeKind::Array with a null
      // `element`) would silently escape with ok=true. Checked up front
      // for every element, not just the first, for the same reason
      // join_shapes checks both of its own operands.
      return shape_fail("INT001", span, "internal: array literal element shape is null");
    }
  }
  const auto m = element_shapes.size();
  // AM-018: "m<=N" is deferred to src/source/size specifically; this is
  // the first point in the pipeline that checks it.
  if (m > capacity) {
    return shape_fail("SIZ002", span, "array literal length exceeds capacity");
  }
  ShapePtr joined_element = element_shapes[0];
  for (std::size_t i = 1; i < element_shapes.size(); ++i) {
    const auto outcome = join_shapes(joined_element, element_shapes[i], span);
    if (!outcome.ok) {
      return outcome;
    }
    joined_element = *outcome.result;
  }
  const auto count_term = certificate::make_literal(static_cast<std::uint64_t>(m));
  ArrayShape result;
  result.exact = count_term;
  result.upper = count_term;
  result.capacity = capacity;
  result.element = joined_element;
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  shape->data = result;
  return shape_ok(shape);
}

ShapeOutcome shape_of_conditional(const ShapePtr &then_shape, const ShapePtr &else_shape, SourceSpan span) {
  return join_shapes(then_shape, else_shape, span);
}

ShapeOutcome shape_of_len(const ShapePtr &array_shape, SourceSpan span) {
  if (!array_shape || array_shape->kind != ShapeKind::Array) {
    // See shape.hpp's doc comment on shape_of_len: Phase 3.2's TYP009
    // already rejects a non-array len operand before shape derivation
    // runs.
    return shape_fail("INT001", span, "internal: len operand is not an array shape");
  }
  return shape_ok(scalar_shape());
}

ShapeOutcome shape_of_index(const ShapePtr &array_shape, SourceSpan span) {
  if (!array_shape || array_shape->kind != ShapeKind::Array) {
    // See shape.hpp's doc comment on shape_of_index: Phase 3.2's TYP009
    // already rejects a non-array index operand before shape derivation
    // runs.
    return shape_fail("INT001", span, "internal: index operand is not an array shape");
  }
  return shape_ok(std::get<ArrayShape>(array_shape->data).element);
}

ShapeOutcome capshape(const ast::TypePtr &type, SourceSpan span) {
  if (!type) {
    // See shape.hpp's doc comment: capshape's only actual caller today
    // is this module's own tests, not a guaranteed-non-null Phase 3.2
    // pipeline, so this is checked the same as every other function in
    // this module.
    return shape_fail("INT001", span, "internal: capshape type is null");
  }
  switch (type->kind) {
  case ast::TypeKind::Bool:
  case ast::TypeKind::I32:
  case ast::TypeKind::I64:
  case ast::TypeKind::F64:
    return shape_ok(scalar_shape());
  case ast::TypeKind::Array: {
    const auto &array_type = std::get<ast::ArrayType>(type->data);
    const auto element_outcome = capshape(array_type.element, span);
    if (!element_outcome.ok) {
      return element_outcome;
    }
    ArrayShape result;
    result.exact = std::nullopt;
    result.upper = certificate::make_literal(array_type.capacity);
    result.capacity = array_type.capacity;
    result.element = *element_outcome.result;
    auto shape = std::make_shared<Shape>();
    shape->kind = ShapeKind::Array;
    shape->data = result;
    return shape_ok(shape);
  }
  case ast::TypeKind::Product: {
    const auto &product_type = std::get<ast::ProductType>(type->data);
    std::vector<ShapePtr> component_shapes;
    component_shapes.reserve(product_type.components.size());
    for (const auto &component : product_type.components) {
      const auto component_outcome = capshape(component, span);
      if (!component_outcome.ok) {
        return component_outcome;
      }
      component_shapes.push_back(*component_outcome.result);
    }
    return shape_ok(shape_of_product(std::move(component_shapes)));
  }
  }
  // Unreachable (exhaustive switch over every ast::TypeKind); INT001,
  // not a SIZ/user-facing code, matching this module's other defensive
  // branches (e.g. join_shapes's identical fallback).
  return shape_fail("INT001", span, "internal: unreachable type kind");
}

ShapeOutcome shape_of_builder(std::optional<certificate::TermPtr> exact, certificate::TermPtr upper,
                               std::uint32_t capacity, const ShapePtr &body_shape, SourceSpan span) {
  if (!upper || (exact && !*exact) || !body_shape) {
    // See shape.hpp's doc comment on shape_of_builder: this function's
    // only real caller today is this module's own tests, not a
    // guaranteed-valid pipeline.
    return shape_fail("INT001", span, "internal: builder count term or body shape is null");
  }
  ArrayShape result;
  result.exact = std::move(exact);
  result.upper = std::move(upper);
  result.capacity = capacity;
  result.element = body_shape;
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  shape->data = result;
  return shape_ok(shape);
}

ShapeOutcome shape_of_fold(const ShapePtr &initial_shape, const ShapePtr &step_shape, SourceSpan span) {
  if (!initial_shape || !step_shape) {
    // See shape.hpp's doc comment on shape_of_fold: this function's only
    // real caller today is this module's own tests, not a guaranteed-valid
    // pipeline.
    return shape_fail("INT001", span, "internal: fold initial or step shape is null");
  }
  switch (compare_shapes(initial_shape, step_shape)) {
  case ShapeComparison::Equal:
    // AM-033: a genuine fixed point (F(kappa_0)=kappa_0) holds for every
    // iteration by induction, regardless of whether the fold's count is
    // exact or inexact, literal or symbolic.
    return shape_ok(initial_shape);
  case ShapeComparison::Differs:
    // AM-033: a genuinely shape-varying accumulator is conservatively
    // rejected rather than guessed at -- this project has not built
    // bounded literal-count unrolling or a general fixed-point/widening
    // solver for this case. SIZ008 was reserved by AM-028 for exactly
    // this problem.
    return shape_fail("SIZ008", span, "fold recurrence cannot be formed: accumulator shape varies across iterations");
  case ShapeComparison::Incompatible:
    // T-Fold/TYP010 already guarantees the fold body's synthesized type
    // matches the accumulator's own declared type at every iteration, so
    // a structural (kind/capacity/arity) mismatch here is a caller
    // precondition violation, not a genuine shape-variation fact --
    // mirrors join_shapes's own AM-028-settled trust-boundary reasoning,
    // applied to this function's own two inputs.
    return shape_fail("INT001", span, "internal: fold initial and step shapes are structurally incompatible");
  }
  // Unreachable (exhaustive switch over every ShapeComparison value).
  return shape_fail("INT001", span, "internal: unreachable shape comparison");
}

} // namespace boundfin::source::size::shape
