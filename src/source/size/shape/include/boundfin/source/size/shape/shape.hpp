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

// Table 8's "product/proj" row, first half: "component shapes kappa_i" ->
// "prod(kappa-bar)" -- collects already-computed component shapes into a
// Product shape, in declared order. Takes the component shapes directly
// (not an ast::ProductExpr) because the row's own premise is phrased over
// already-known shapes, not subexpressions; recursively computing each
// component's own shape from source needs the general infer_shape
// dispatcher this module does not expose yet (see shape.hpp's top
// comment). Infallible: Table 8's own row text (main.pdf p.40) states a
// premise only for the projection half ("valid field j"), none for
// construction; Phase 3.2's typecheck already rejects a malformed
// product (wrong arity, etc., AM-002) before any shape derivation runs;
// and prod() imposes no further cross-component constraint to violate.
// This mirrors src/source/typecheck's own make_product_type, the exact
// existing precedent for "trust the caller, no arity re-check" (it too
// relies entirely on the parser's SYN003 arity-at-least-two guarantee,
// never re-verified downstream).
[[nodiscard]] ShapePtr shape_of_product(std::vector<ShapePtr> component_shapes);

// Table 8's "product/proj" row, second half: "valid field j" ->
// "kappa_j" -- selects the j-th component shape (1-based, matching
// ast::ProjExpr::index and AM-002's "proj<j>(e) uses one-based j") from
// an already-Product-shaped operand.
//
// Trust boundary: `operand_shape` is taken as given, not derived here
// (same pattern as shape_of_var's `context`). Phase 3.2's typecheck
// already enforces "valid field j" (TYP006 non-product operand, TYP007
// out-of-arity index) before shape derivation ever runs, so a caller
// presenting a non-Product `operand_shape` or an out-of-range `index`
// here violates an already-checked invariant -- INT001, not a SIZ code,
// matching shape_of_var's identical reasoning. (SIZ007, "Exact/upper/
// capacity shape is malformed or constraint-inconsistent," is reserved
// for a later Table 8 row where the *shape itself* can be inconsistent
// in a way Table 7 never checks, e.g. a builder/array's lambda<=u<=N --
// neither of this function's own conditions is such a case, since both
// are fully precluded by TYP006/TYP007 already.) `index`'s low bound
// (index<1, i.e. index==0 since index is unsigned) is in fact
// unreachable one gate earlier than TYP007: grammar.ebnf's
// positive-decimal production forbids a literal 0 at the parser
// (SYN001), so a real parsed-and-typechecked program can never reach
// this function with index==0 at all -- the check (and its test) stay
// defensive, not merely redundant with TYP007, since they exercise this
// function's own direct API rather than a full parse pipeline.
[[nodiscard]] ShapeOutcome shape_of_proj(const ShapePtr &operand_shape, std::uint32_t index, SourceSpan span);

// The recursive join operation App. A.5's intro names "J" (main.pdf
// p.11: "J is recursive join"), stated once by the paper and reused by
// multiple Table 8 rows under two different notations: "literal" and
// "builder" write it folded over several operands as kappa-bar=
// (squnion)_{i<n} kappa_i; "conditional" writes it applied to exactly
// two operands as J(kappa_t,kappa_f). This function is the binary case
// both reduce to (folding is the caller's job, e.g. shape_of_literal
// below).
//
// Per-kind behavior: the scalar and array cases are stated directly in
// main.pdf p.11-12's conditional-row prose (Table 8 itself just names
// "J"/"squnion" without re-deriving it): scalar join scalar = scalar
// (no data to compare). array join array: exact component is the
// shared term when both inputs have the *same* exact component
// (compared structurally via certificate::terms_equal, matching
// "preserves an exact length only when both branches establish the
// same expression"), else star; upper component is always max(u1,u2);
// element component is the two inputs' own elements joined recursively.
// product join product (component-wise, matching arity) is *not*
// separately spelled out in prose anywhere in the paper -- it is the
// forced structural reading of "J is recursive join" (p.39) applied to
// Sec. 4.3's own prod(kappa_1,...,kappa_k) grammar, the only coherent
// way to recursively join two product shapes given that grammar (a
// spec-auditor review of the fourth slice confirmed no sentence in
// pp.10-12 or App. A.5 spells this case out the way the array case is
// spelled out, correcting an earlier overclaim in this comment that it
// was). A capacity mismatch between two array operands, an arity
// mismatch between two product operands, or a kind mismatch between the
// two operands at all is INT001, not a SIZ code: every call site in
// this module joins two shapes that provably share a static type (two
// elements of the same array literal; two branches of one conditional;
// two iterations of one fold/builder body), so Phase 3.2's own
// type-equality checks already preclude a real structural mismatch here
// -- mirrors shape_of_var's/shape_of_proj's identical trust-boundary
// reasoning. (AM-028, approved 2026-10-06: SIZ008, "Recursive
// element-shape join or fold recurrence cannot be formed," is reserved
// for the deferred builder/fold rows' own distinct problem -- folding
// squnion over a *symbolic*, not-necessarily-concrete iteration count
// (0<=i<u_n) cannot be mechanically formed by direct enumeration the
// way shape_of_literal's fold over a literal's always-concrete m can,
// needing some other proof technique instead. join_shapes's own
// algorithm is total for any two structurally compatible shapes; its
// mismatch branches guard a caller precondition violation, not a
// limitation of the join itself, so SIZ008 does not apply here.)
[[nodiscard]] ShapeOutcome join_shapes(const ShapePtr &first, const ShapePtr &second, SourceSpan span);

// Table 8's "literal" row: "m<=N, element shapes kappa_i" ->
// "array(m,m,N;squnion_{i<m} kappa_i)" (main.pdf p.40). `element_shapes`
// is the array literal's own per-element shapes, already computed (same
// "takes shapes directly, not ast::Expr" pattern as shape_of_product --
// recursing into each element's own subexpression needs the still-
// deferred infer_shape dispatcher). Folds join_shapes left-to-right over
// `element_shapes` (AM-020 already confirmed an empty array literal
// (m=0) is rejected by typecheck's TYP008 before shape derivation ever
// runs, so `element_shapes` is expected non-empty; an empty vector here
// is INT001, a caller precondition violation, not SIZ002/SIZ003 -- the
// program that would reach this with m=0 was already rejected one phase
// earlier). Both lambda and u in the result are the same literal term
// `m` (the element count is exactly, statically known for any literal,
// not merely bounded): per the result column, "array(m,m,N;...)" writes
// the identical symbol `m` in both the exact and upper positions.
//
// This is where AM-018's already-approved "m<=N is deferred to
// src/source/size" decision is finally carried out: `capacity <
// element_shapes.size()` raises SIZ002 ("Array literal length exceeds
// capacity") here -- a real, SIZ-coded rejection of the program (unlike
// every INT001 case in this module so far), since neither the parser
// (AM-018) nor typecheck (AM-029: Table 7's own T-Array rule states
// "m<=N" too, but typecheck deliberately omits it, the same split
// already applied to T-Fold/T-Build's own bundled size premise) checks
// this anywhere else. Not yet reachable by actually compiling a real
// program, though: like every Table 8 row in this module so far, this
// function is callable directly on caller-supplied shapes but not yet
// wired into any module-wide traversal (no infer_shape dispatcher
// exists yet -- see this header's top comment) -- an over-length array
// literal in a real program will not be rejected by any currently
// reachable code path until that wiring lands.
[[nodiscard]] ShapeOutcome shape_of_literal(std::uint32_t capacity, std::vector<ShapePtr> element_shapes,
                                             SourceSpan span);

// Table 8's "conditional" row: "branch shapes kappa_t,kappa_f" ->
// "J(kappa_t,kappa_f); exact length retained iff equal" (main.pdf p.40).
// `main.pdf` p.39's App. A.5 intro already states "J is recursive join"
// as one operation named once and reused across rows; `join_shapes`
// above already implements it, built in the third slice directly from
// this same row's own prose (main.pdf p.11-12), which states the
// scalar and array cases in full; the product case is the forced
// structural reading of "recursive join" applied to Sec. 4.3's own
// prod(kappa_1,...,kappa_k) grammar, not separately spelled out in
// prose anywhere in the paper (confirmed by a spec-auditor review of
// this slice, which also found and corrected an earlier version of
// this comment that over-attributed this confirmation to AM-028 --
// AM-028 itself is narrowly about join_shapes's error/mismatch
// branches' diagnostic code, not its per-kind join algorithm). This
// function is therefore a thin, deliberately non-reimplementing
// wrapper: it exists
// so the "conditional" row has its own discoverable, independently-
// traceable entry point, matching every other row's one-function-per-
// row style in this module, not because the join behaves any
// differently for two conditional branches than for two array-literal
// elements. Phase 3.2's T-If (`TYP005`) already requires both branches
// to share a type, the identical "provably same static type"
// precondition `join_shapes`'s own `INT001` trust boundary already
// relies on.
[[nodiscard]] ShapeOutcome shape_of_conditional(const ShapePtr &then_shape, const ShapePtr &else_shape,
                                                 SourceSpan span);

// Table 8's "length/index" row, first half: "array `array(lambda,u,N;
// kappa-bar)`" -> "scalar/count `(lambda,u)`" (main.pdf p.40). Table 8's
// own caption disambiguates this cell directly: "'Scalar/count' means
// scalar shape plus the separate refinement in table 6" -- so `len(e)`'s
// Table 8 shape (kappa) is unconditionally `scalar` (also confirmed by
// Table 7's T-Len: "length: i32" unconditionally, and by kappa's own
// grammar, p.11, having no variant that could carry `(lambda,u)`
// alongside `scalar`); "`(lambda,u)`" is a pointer to Table 6's separate
// `|-cnt` judgment, not part of kappa at all, and this function
// implements only the `scalar`/kappa half.
//
// `(lambda,u)` itself is not obtained from this function's own return
// value (`ShapeOutcome` only ever carries a scalar `ShapePtr` on
// success) -- it is already directly available on the `array_shape`
// *parameter itself* (`ArrayShape::exact`/`::upper`, public since the
// third slice), which a future caller retains from the operand's own
// already-computed shape before calling this function, not something
// this function hands back. This is what will let Table 6's still-
// deferred `C-Len-E`/`C-Len-U` pull `(lambda,u)` from an already-
// computed Table 8 shape in a later, separately-scoped slice (threading
// a `Shape` into `src/source/size/count`'s `infer_count` is a
// cross-module design question not undertaken here) -- a fact about
// `ArrayShape`'s own pre-existing public fields, not new behavior this
// function provides.
//
// Trust boundary: `array_shape` is taken as given, not derived here
// (same pattern as every other row in this module). Phase 3.2's
// typecheck already rejects a non-array `len` operand via `TYP009`
// before shape derivation ever runs, so a caller presenting a
// non-`Array` `array_shape` here violates an already-checked invariant
// -- `INT001`, not a SIZ code.
[[nodiscard]] ShapeOutcome shape_of_len(const ShapePtr &array_shape, SourceSpan span);

// Table 8's "length/index" row, second half: "array `array(lambda,u,N;
// kappa-bar)`" -> "`kappa-bar`" -- indexing an array returns its
// element summary, the *same* conservative join every logical element
// already shares (main.pdf p.11-12: "indexing returns the array's
// element summary"). The index value itself is irrelevant to the
// result (consistent with why `kappa-bar` is one joined shape, not a
// per-index vector, in the first place) -- this function accordingly
// does not take the index expression/value at all.
//
// Trust boundary: identical to shape_of_len's -- Phase 3.2's typecheck
// already rejects a non-array index operand via `TYP009` before shape
// derivation ever runs, so `INT001`, not a SIZ code, for a non-`Array`
// `array_shape`.
[[nodiscard]] ShapeOutcome shape_of_index(const ShapePtr &array_shape, SourceSpan span);

} // namespace boundfin::source::size::shape
