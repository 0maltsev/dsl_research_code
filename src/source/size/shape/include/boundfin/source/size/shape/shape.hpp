#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace boundfin::source::size::shape {

using boundfin::source::ast::SourceSpan;

// Result-size shapes (main.pdf Sec. 4.3, p.11): "A capacity alone is
// insufficient for a parametric cost. Size shapes are therefore
// kappa ::= scalar | prod(kappa_1,...,kappa_k) | array(lambda,u,N;kappa-bar)
// where lambda is either an exact symbolic length s or star (not known
// exactly), u is a proved symbolic upper length, N is the declared
// capacity, and kappa-bar conservatively joins the shapes of every
// logical element." Unlike src/source/size/count's Table 6 (an explicitly
// bounded *fragment*: most expression forms correctly have no count
// refinement, forever), Table 8 (App. A.5, p.40: "The judgment
// Sigma;Delta;Gamma|-sz e=>kappa does not allocate or evaluate e; it
// summarises result sizes") is a *total* judgment -- every well-typed
// expression has a Table 8 shape. This module is built incrementally,
// one Table 8 row's dedicated function at a time (mirroring Table 6's own
// bottom-up build order), rather than via one exhaustive dispatcher from
// the start: an exhaustive switch cannot honestly fall back to a
// SIZ-coded "no rule" the way Table 6's genuinely-fragmentary switch
// could, since a not-yet-implemented row is a real gap in this
// implementation, not a true fact about the program. The unifying
// `infer_shape(const ast::Expr&, const ShapeContext&)` dispatcher is
// deferred to the slice that completes every non-`call` row (`call`
// itself stays deferred for the same reason Table 6's C-Call does: it
// needs K_f/Q_f and the certificate module's still-deferred "substitution"
// mechanism, AM-022).
enum class ShapeKind { Scalar, Array, Product };

struct Shape;
using ShapePtr = std::shared_ptr<Shape>;

// array(lambda,u,N;kappa-bar). `exact` is lambda (std::nullopt means
// "star", i.e. not known exactly); `upper` is u, always present;
// `capacity` is the declared N; `element` is kappa-bar, the single
// shape produced by conservatively joining every logical element's own
// shape (not a per-index vector -- main.pdf p.11: "kappa-bar
// conservatively joins the shapes of every logical element").
struct ArrayShape {
  std::optional<certificate::TermPtr> exact;
  certificate::TermPtr upper;
  std::uint32_t capacity = 0;
  ShapePtr element;
};

// prod(kappa_1,...,kappa_k): one component shape per product field, in
// declared order (AM-002: arity at least two, though this type does not
// itself enforce that -- Phase 3.2's typecheck already rejects a
// malformed product before any shape derivation runs).
struct ProductShape {
  std::vector<ShapePtr> components;
};

struct Shape {
  ShapeKind kind = ShapeKind::Scalar;
  // Empty (std::monostate) for Scalar: main.pdf's grammar gives `scalar`
  // no parameters at all.
  std::variant<std::monostate, ArrayShape, ProductShape> data;
};

// `scalar` is a single shared instance (it carries no data, so there is
// nothing to distinguish between two calls); every expression kind whose
// Table 8 row's result is "scalar" with no further structure uses this.
[[nodiscard]] ShapePtr scalar_shape();

// Gamma_sz, the shape/size environment Table 8's "let" and "variable"
// rows read and extend, keyed by BindingId (set by Phase 3.1's resolver;
// globally unique within one resolved module, mirroring
// src/source/size/count's IndexContext). Caller-supplied here -- this
// module does not yet derive it from live module traversal (deferred,
// same incremental pattern src/source/size/count followed before its own
// `check_module_count_admissibility`).
using ShapeContext = std::unordered_map<ast::BindingId, ShapePtr>;

struct ShapeDiagnostic {
  std::string code;
  SourceSpan span;
  std::string message;
};

struct ShapeOutcome {
  bool ok = false;
  std::optional<ShapePtr> result;
  std::optional<ShapeDiagnostic> diagnostic;
};

// Table 8's "scalar/variable" row, second half: "Gamma_sz(x)=kappa" --
// a variable's shape is whatever `context` already has recorded for its
// resolved binding.
//
// Trust boundary: `context` is taken as given, not derived here. Every
// binding in scope at `var_expr` is expected to already have a `context`
// entry (every binding site -- parameter, let, fold/build binder -- gets
// one when Table 8's own rule for that site runs; `main.pdf` p.11's own
// text has no case where a resolved, in-scope variable legitimately has
// no shape, unlike Table 6's IndexContext, where "not an index binder"
// is a true, permanent rejection). A missing entry is accordingly
// INT001, not a SIZ code, matching infer_var's identical reasoning in
// src/source/size/count for its own (different) trust boundary: an
// internal-invariant failure after a validated preceding stage must
// never be presented as user source rejection
// (diagnostics-and-status.md). The future slice that wires this into
// live module traversal must populate `context` for every binding
// before this function ever sees a reference to it.
[[nodiscard]] ShapeOutcome shape_of_var(const ast::VarExpr &var_expr, SourceSpan span, const ShapeContext &context);

} // namespace boundfin::source::size::shape
