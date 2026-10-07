#include "boundfin/source/size/infer/infer.hpp"

namespace boundfin::source::size::infer {

namespace {

using namespace boundfin::source::ast;
using boundfin::source::size::shape::ShapeContext;
using boundfin::source::size::shape::ShapeDiagnostic;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::ShapeOutcome;
using boundfin::source::size::shape::ShapePtr;

ShapeOutcome shape_fail(std::string code, SourceSpan span, std::string message) {
  return ShapeOutcome{false, std::nullopt, ShapeDiagnostic{std::move(code), span, std::move(message)}};
}

ShapeOutcome shape_ok(ShapePtr shape) { return ShapeOutcome{true, std::move(shape), std::nullopt}; }

// Converts a count-module failure into a ShapeOutcome failure, carrying
// the original SIZ code/span/message through unchanged -- count's own
// diagnostics (SIZ005, SIZ006, SIZ003, INT001, ...) are already correctly
// coded for whatever actually went wrong; this function does not
// reinterpret them.
ShapeOutcome propagate_count_failure(const count::CountDiagnostic &diagnostic) {
  return shape_fail(diagnostic.code, diagnostic.span, diagnostic.message);
}

ShapeOutcome infer_shape_impl(const Expr &expr, const ShapeContext &shape_context,
                               const count::IndexContext &index_context);

ShapeOutcome infer_product(const ProductExpr &product_expr, const ShapeContext &shape_context,
                            const count::IndexContext &index_context) {
  std::vector<ShapePtr> component_shapes;
  component_shapes.reserve(product_expr.components.size());
  for (const auto &component : product_expr.components) {
    const auto outcome = infer_shape_impl(*component, shape_context, index_context);
    if (!outcome.ok) {
      return outcome;
    }
    component_shapes.push_back(*outcome.result);
  }
  return shape_ok(shape::shape_of_product(std::move(component_shapes)));
}

ShapeOutcome infer_proj(const ProjExpr &proj_expr, SourceSpan span, const ShapeContext &shape_context,
                         const count::IndexContext &index_context) {
  const auto operand_outcome = infer_shape_impl(*proj_expr.operand, shape_context, index_context);
  if (!operand_outcome.ok) {
    return operand_outcome;
  }
  return shape::shape_of_proj(*operand_outcome.result, proj_expr.index, span);
}

ShapeOutcome infer_array_literal(const ArrayLiteralExpr &array_literal, SourceSpan span,
                                  const ShapeContext &shape_context, const count::IndexContext &index_context) {
  std::vector<ShapePtr> element_shapes;
  element_shapes.reserve(array_literal.elements.size());
  for (const auto &element : array_literal.elements) {
    const auto outcome = infer_shape_impl(*element, shape_context, index_context);
    if (!outcome.ok) {
      return outcome;
    }
    element_shapes.push_back(*outcome.result);
  }
  return shape::shape_of_literal(array_literal.capacity, std::move(element_shapes), span);
}

ShapeOutcome infer_if(const IfExpr &if_expr, SourceSpan span, const ShapeContext &shape_context,
                       const count::IndexContext &index_context) {
  // Table 8's "conditional" row needs only the branch shapes; the guard's
  // own shape has no role in the result (shape_of_conditional's own doc
  // comment, fourth slice), so the condition subexpression is not
  // visited here.
  const auto then_outcome = infer_shape_impl(*if_expr.then_branch, shape_context, index_context);
  if (!then_outcome.ok) {
    return then_outcome;
  }
  const auto else_outcome = infer_shape_impl(*if_expr.else_branch, shape_context, index_context);
  if (!else_outcome.ok) {
    return else_outcome;
  }
  return shape::shape_of_conditional(*then_outcome.result, *else_outcome.result, span);
}

ShapeOutcome infer_let(const LetExpr &let_expr, const ShapeContext &shape_context,
                        const count::IndexContext &index_context) {
  if (!let_expr.binding) {
    // Unreachable for any module that went through Phase 3.1's resolver;
    // matches every sibling module's identical defensive branch.
    return shape_fail("INT001", let_expr.name_span, "internal: let has no resolved binding");
  }
  const auto bound_outcome = infer_shape_impl(*let_expr.bound, shape_context, index_context);
  if (!bound_outcome.ok) {
    return bound_outcome;
  }
  auto extended = shape_context;
  extended[*let_expr.binding] = *bound_outcome.result;
  return infer_shape_impl(*let_expr.body, extended, index_context);
}

ShapeOutcome infer_len(const LenExpr &len_expr, SourceSpan span, const ShapeContext &shape_context,
                        const count::IndexContext &index_context) {
  const auto array_outcome = infer_shape_impl(*len_expr.array, shape_context, index_context);
  if (!array_outcome.ok) {
    return array_outcome;
  }
  return shape::shape_of_len(*array_outcome.result, span);
}

ShapeOutcome infer_index(const IndexExpr &index_expr, SourceSpan span, const ShapeContext &shape_context,
                          const count::IndexContext &index_context) {
  // shape_of_index does not take the index value at all (the result does
  // not depend on it), so index_expr.index is not visited here.
  const auto array_outcome = infer_shape_impl(*index_expr.array, shape_context, index_context);
  if (!array_outcome.ok) {
    return array_outcome;
  }
  return shape::shape_of_index(*array_outcome.result, span);
}

ShapeOutcome infer_fold(const FoldExpr &fold_expr, const ShapeContext &shape_context,
                         const count::IndexContext &index_context) {
  if (!fold_expr.index_binding || !fold_expr.accumulator_binding) {
    // Unreachable for a resolver-processed module; mirrors count.cpp's
    // walk_for_admissibility's identical defensive branch.
    return shape_fail("INT001", fold_expr.count->span, "internal: fold has no resolved binding");
  }
  // AM-033: shape_of_fold itself needs no count data at all (the
  // fixed-point design sidesteps it entirely) -- but check_count_
  // admissibility is still called here, for its own SIZ005 validation
  // and to mint the index binder's symbol, since a *nested* fold/build
  // inside this body may need it via index_context (mirrors
  // check_module_count_admissibility's own established pattern exactly).
  // Checked BEFORE `initial`, matching E-Fold's own left-to-right
  // sequential evaluation (main.pdf p.42: count is evaluated before the
  // initial accumulator) and count.cpp's walk_for_admissibility's own
  // already-shipped, already-audited order for the identical node (count
  // before initial before body) -- a spec-auditor review of this slice's
  // first version found the order reversed here specifically, the one
  // place in this dispatcher diverging from that established sequence,
  // reproduced concretely: fold<2>(i32bits(9); acc,idx; g(); acc) with
  // g() an (unimplemented) call wrongly surfaced SIZ013 (from `initial`)
  // instead of SIZ005 (from the over-capacity count), violating CLAUDE.md
  // Sec. 6's left-to-right/first-error preservation requirement.
  //
  // Known limitation, disclosed not hidden: check_count_admissibility's
  // own signature has no shape_context parameter at all (only
  // IndexContext), so a fold whose own `count` expression needs
  // C-Len-E/C-Len-U (e.g. `len(x)` for an already-shaped `x`) cannot
  // resolve even when `shape_context` already has `x`'s own entry here --
  // conservatively falls through to SIZ003 instead. Fixing this needs
  // extending check_count_admissibility's own already-shipped signature
  // (Phase 3.4 fourth slice), out of this slice's own scope; left for a
  // future slice.
  const auto admissibility =
      count::check_count_admissibility(*fold_expr.count, fold_expr.capacity, *fold_expr.index_binding, index_context);
  if (!admissibility.ok) {
    return propagate_count_failure(*admissibility.diagnostic);
  }
  const auto initial_outcome = infer_shape_impl(*fold_expr.initial, shape_context, index_context);
  if (!initial_outcome.ok) {
    return initial_outcome;
  }
  auto extended_shape_context = shape_context;
  extended_shape_context[*fold_expr.accumulator_binding] = *initial_outcome.result;
  extended_shape_context[*fold_expr.index_binding] = shape::scalar_shape();
  auto extended_index_context = index_context;
  extended_index_context[*fold_expr.index_binding] = *admissibility.binding;
  const auto step_outcome = infer_shape_impl(*fold_expr.body, extended_shape_context, extended_index_context);
  if (!step_outcome.ok) {
    return step_outcome;
  }
  return shape::shape_of_fold(*initial_outcome.result, *step_outcome.result, fold_expr.count->span);
}

ShapeOutcome infer_build(const BuildExpr &build_expr, const ShapeContext &shape_context,
                          const count::IndexContext &index_context) {
  if (!build_expr.index_binding) {
    return shape_fail("INT001", build_expr.count->span, "internal: build has no resolved binding");
  }
  // Known limitation, disclosed not hidden (same as infer_fold's
  // identical call): check_count_admissibility's own signature has no
  // shape_context parameter, so a build whose own `count` expression
  // needs C-Len-E/C-Len-U cannot resolve here even when shape_context
  // already has the needed entry -- conservatively SIZ003 instead.
  // Fixing this needs extending check_count_admissibility's own
  // already-shipped signature (Phase 3.4 fourth slice), out of this
  // slice's own scope.
  const auto admissibility = count::check_count_admissibility(*build_expr.count, build_expr.capacity,
                                                                *build_expr.index_binding, index_context);
  if (!admissibility.ok) {
    return propagate_count_failure(*admissibility.diagnostic);
  }
  // check_count_admissibility's own AdmissibilityOutcome discards the
  // count's exact component by design (an already-disclosed integration
  // gap, STATUS.md) -- shape_of_builder needs the full (exact,upper)
  // pair, so infer_count is called directly here too, a second time,
  // rather than changing AdmissibilityOutcome's own shape. Unlike
  // check_count_admissibility, infer_count's own signature already has a
  // shape_context parameter (added for C-Len-E/C-Len-U, AM-034) -- passed
  // through here for forward consistency, though in practice
  // check_count_admissibility's own call above (which lacks this
  // parameter) already rejects a C-Len-E/C-Len-U-dependent count before
  // this line is ever reached -- not independently exercised by any test
  // in this slice for that reason (confirmed: a mutation dropping this
  // argument here is not currently caught by any test), but still the
  // correct argument to pass once check_count_admissibility's own
  // signature is eventually extended.
  const auto count_outcome = count::infer_count(*build_expr.count, index_context, shape_context);
  if (!count_outcome.ok) {
    return propagate_count_failure(*count_outcome.diagnostic);
  }
  auto extended_shape_context = shape_context;
  extended_shape_context[*build_expr.index_binding] = shape::scalar_shape();
  auto extended_index_context = index_context;
  extended_index_context[*build_expr.index_binding] = *admissibility.binding;
  const auto body_outcome = infer_shape_impl(*build_expr.body, extended_shape_context, extended_index_context);
  if (!body_outcome.ok) {
    return body_outcome;
  }
  return shape::shape_of_builder(count_outcome.result->exact, count_outcome.result->upper, build_expr.capacity,
                                  *body_outcome.result, build_expr.count->span);
}

ShapeOutcome infer_shape_impl(const Expr &expr, const ShapeContext &shape_context,
                               const count::IndexContext &index_context) {
  switch (expr.kind) {
  case ExprKind::Literal:
  case ExprKind::UnaryPrimitive:
  case ExprKind::BinaryPrimitive:
    // Table 8's "scalar/variable" row, first half: "scalar literal/
    // primitive" -> "scalar", unconditionally, for every Table 5
    // primitive and every literal -- the row's own premise states no
    // dependency on operand shapes at all, unlike e.g. Table 6's C-Add/
    // C-Sub, which do have certificate obligations over their operands.
    // A primitive's own operands are accordingly NOT recursed into here
    // (their own shapes, whatever they are, cannot change this result).
    // This does not skip validating anything a nested fold/build inside
    // an operand might need: count::check_module_count_admissibility
    // (Table 6, already shipped) independently walks every subexpression
    // of every function body, including primitive operands, admissibility
    // -checking every fold/build at any nesting depth on its own --
    // infer_shape's own job is computing shapes for expressions that need
    // them, not re-deriving validation Table 6's own traversal already
    // performs completely.
    return shape_ok(shape::scalar_shape());
  case ExprKind::Var:
    return shape::shape_of_var(std::get<VarExpr>(expr.data), expr.span, shape_context);
  case ExprKind::Product:
    return infer_product(std::get<ProductExpr>(expr.data), shape_context, index_context);
  case ExprKind::Proj:
    return infer_proj(std::get<ProjExpr>(expr.data), expr.span, shape_context, index_context);
  case ExprKind::ArrayLiteral:
    return infer_array_literal(std::get<ArrayLiteralExpr>(expr.data), expr.span, shape_context, index_context);
  case ExprKind::If:
    return infer_if(std::get<IfExpr>(expr.data), expr.span, shape_context, index_context);
  case ExprKind::Let:
    return infer_let(std::get<LetExpr>(expr.data), shape_context, index_context);
  case ExprKind::Len:
    return infer_len(std::get<LenExpr>(expr.data), expr.span, shape_context, index_context);
  case ExprKind::Index:
    return infer_index(std::get<IndexExpr>(expr.data), expr.span, shape_context, index_context);
  case ExprKind::Fold:
    return infer_fold(std::get<FoldExpr>(expr.data), shape_context, index_context);
  case ExprKind::Build:
    return infer_build(std::get<BuildExpr>(expr.data), shape_context, index_context);
  case ExprKind::Call:
    // AM-036: a real Table 8 row (not SIZ003's "no rule exists"), just
    // not yet implemented by this codebase -- step 5 of AM-035's own
    // declaration-rank induction ladder.
    return shape_fail("SIZ013", expr.span, "call expression's Table 8 shape judgment has no implemented dispatch rule yet");
  }
  // Unreachable (exhaustive switch over every ExprKind); INT001, not a
  // SIZ/user-facing code, matching every sibling module's own defensive
  // branches.
  return shape_fail("INT001", expr.span, "internal: unreachable expression kind");
}

} // namespace

ShapeOutcome infer_shape(const ast::Expr &expr, const shape::ShapeContext &shape_context,
                          const count::IndexContext &index_context) {
  return infer_shape_impl(expr, shape_context, index_context);
}

namespace {

FunctionSummaryOutcome summary_fail(std::string code, SourceSpan span, std::string message) {
  return FunctionSummaryOutcome{false, std::nullopt, ShapeDiagnostic{std::move(code), span, std::move(message)}};
}

FunctionSummaryOutcome summary_ok(FunctionSummary summary) {
  return FunctionSummaryOutcome{true, std::move(summary), std::nullopt};
}

} // namespace

FunctionSummaryOutcome compute_function_summary(const ast::FunctionDecl &function) {
  ShapeContext shape_context;
  for (const auto &parameter : function.parameters) {
    if (!parameter.binding) {
      // Unreachable for a resolver-processed module; mirrors every
      // sibling module's identical defensive branch.
      return summary_fail("INT001", parameter.span, "internal: parameter has no resolved binding");
    }
    if (!parameter.type) {
      return summary_fail("INT001", parameter.span, "internal: parameter has no declared type");
    }
    switch (parameter.type->kind) {
    case TypeKind::Array: {
      // The top-level ABI-array-parameter rule (main.pdf p.11 main text)
      // applies only to a parameter whose OWN declared type is directly
      // arr<tau,N> -- mints a fresh, tight formal length n_x from this
      // parameter's own binding.
      const auto outcome = shape::shape_of_abi_array_param(parameter.type, *parameter.binding, parameter.span);
      if (!outcome.ok) {
        return FunctionSummaryOutcome{false, std::nullopt, outcome.diagnostic};
      }
      shape_context[*parameter.binding] = *outcome.result;
      break;
    }
    case TypeKind::Bool:
    case TypeKind::I32:
    case TypeKind::I64:
    case TypeKind::F64:
    case TypeKind::Product: {
      // A spec-auditor review of this slice's first version found the
      // Product case wrongly rejected with SIZ013 (not yet supported) --
      // factually wrong for an all-scalar product (shape_of_product
      // composed with scalar_shape() for every field has zero remaining
      // ambiguity) and, even for a product containing a nested array
      // field, the rejection was unnecessary: shape_of_abi_array_param's
      // own doc comment (step 1) already disclosed and anticipated
      // exactly this gap -- "whether such a nested array independently
      // needs its own fresh formal length symbol, or falls back to
      // capshape's own star-shaped conservative treatment" -- naming
      // capshape as an available, sound (if non-tightest) fallback, not
      // an unresolved blocker. capshape (shape.hpp) is already general
      // over the whole Type grammar: scalar gives scalar_shape()
      // directly (identical to the dedicated case above); product
      // recurses component-wise via shape_of_product, applying this same
      // treatment to each field; a nested array field gets the
      // capacity-fallback row's own array(star,N,N;capshape(tau))
      // (conservative, not unsound -- no fresh formal length symbol is
      // minted for it, since a nested field has no independent binding
      // to mint one from; whether a future slice should mint one anyway,
      // e.g. via some synthetic per-field symbol, remains the one
      // genuinely open question here, left deferred and disclosed, not
      // silently resolved). Scalar parameters route through capshape
      // too here (not the dedicated scalar_shape() call) purely to avoid
      // a redundant case split -- capshape's own scalar case returns the
      // identical shared instance.
      const auto outcome = shape::capshape(parameter.type, parameter.span);
      if (!outcome.ok) {
        return FunctionSummaryOutcome{false, std::nullopt, outcome.diagnostic};
      }
      shape_context[*parameter.binding] = *outcome.result;
      break;
    }
    }
  }

  const auto k_f_outcome = infer_shape_impl(*function.body, shape_context, {});
  if (!k_f_outcome.ok) {
    return FunctionSummaryOutcome{false, std::nullopt, k_f_outcome.diagnostic};
  }

  FunctionSummary summary;
  summary.k_f = *k_f_outcome.result;

  // AM-003: "Q_f is present only when the declared result is i32 AND the
  // body has an accepted count-refinement derivation... If Q_f is
  // absent, the call is legal as an ordinary expression but prohibited
  // in the count fragment." A failed derivation is accordingly NOT a
  // failure of this function as a whole -- unlike K_f (unconditionally
  // required, main.pdf p.12), Q_f's own absence is an ordinary, expected
  // outcome for most i32-result bodies (e.g. a + b for ordinary scalar
  // parameters: Table 6 has no C-Var rule at all, so this fails SIZ003
  // every time, correctly, with no implication the function itself is
  // malformed). A SIZ-coded infer_count failure -- SIZ003 ("no rule,"
  // the common case), SIZ006 (e.g. an overflow/no-wrap certificate
  // Table 8's own shape judgment never checks for ordinary scalar
  // arithmetic, so this can fail here even when k_f already succeeded
  // cleanly; main.pdf p.10's own "may enter the count fragment only
  // when its certificate proves..." confirms this IS exactly a failure
  // to derive, the same "no accepted derivation" condition AM-003 names,
  // not a distinct program-level rejection), or any other SIZ code -- is
  // silently absorbed into q_f=std::nullopt, not propagated as an error.
  //
  // A spec-auditor review of this slice's first version found this
  // absorption had no exception for INT0xx codes, even though an INT
  // code (unlike a SIZ code) never means "no accepted derivation" --
  // diagnostics-and-status.md's own INT catalogue defines it as a
  // genuine internal-invariant violation, "never presented as user
  // source rejection." Because infer_shape's own scalar/primitive
  // dispatch deliberately does not recurse into primitive operands
  // (unlike count::infer_count's own Table 6 traversal, which must, for
  // C-Add/C-Sub), the two traversals over the same body do not visit an
  // identical node set -- so a latent invariant violation reachable only
  // from inside a primitive operand could in principle surface as INT001
  // solely through this q_f path and be silently swallowed rather than
  // surfaced as a bug. Fixed: an INT-coded failure here propagates as a
  // genuine compute_function_summary failure, the same as k_f's own.
  if (function.result_type && function.result_type->kind == TypeKind::I32) {
    const auto q_f_outcome = count::infer_count(*function.body, {}, shape_context);
    if (q_f_outcome.ok) {
      summary.q_f = *q_f_outcome.result;
    } else if (q_f_outcome.diagnostic->code.rfind("INT", 0) == 0) {
      return summary_fail(q_f_outcome.diagnostic->code, q_f_outcome.diagnostic->span, q_f_outcome.diagnostic->message);
    }
  }

  return summary_ok(std::move(summary));
}

} // namespace boundfin::source::size::infer
