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
// refinement, forever), Table 8 (main.pdf p.11, Sec. 4.3 main text: "The
// judgment Sigma;Delta;Gamma|-sz e=>kappa does not allocate or evaluate
// e; it summarises result sizes" -- not App. A.5, which restates this
// judgment's exhaustive equations on p.39 but does not itself repeat
// this sentence) is a *total* judgment -- every well-typed
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
// are fully precluded by TYP006/TYP007 already. AM-032, logged when the
// "builder" row was later implemented: that row's own shape_of_builder
// also does not raise SIZ007 for a lambda<=u<=N violation, despite this
// comment's own anticipation -- see shape_of_builder's doc comment for
// why: the invariant is already guaranteed upstream, before any
// legitimate caller could reach that function with a violating triple,
// so SIZ007 remains unreserved by any row implemented so far.) `index`'s
// low bound
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

// Table 8's "capacity fallback" row: "type `arr(tau,N)`" -> "`array(star,
// N,N;capshape(tau))`" (main.pdf p.40). `capshape(tau)` itself (main.pdf
// p.11: "recursively replaces every array length in a type by its
// capacity and supplies a finite fallback for nested aggregate
// elements") is a *general* recursive function over the whole Type
// grammar (`tau::=b | tau1 x ... x tauk | arr(tau,N)`), not specific to
// array-typed inputs: applied to a scalar type it gives `scalar`;
// applied to a product it recurses component-wise (reusing
// shape_of_product); applied to an array `arr(tau,N)` it gives exactly
// `array(star,N,N;capshape(tau))` -- which *is* this row's own result.
// The "capacity fallback" row is therefore not a separate rule from
// `capshape` itself, just `capshape` restated for its array-input case.
//
// No premise is stated anywhere for this function beyond `type` itself
// being well-formed (unlike every other row in this module, which
// checks a caller-supplied `Shape`'s own structure). Returns `INT001`
// for a null `type` at any recursion depth (checked before every
// dereference, at the top level and for `array_type.element`/each
// `product_type.components[i]` before recursing into them): a spec-
// auditor review of this slice's first version found this function had
// no defensive null check at all, justified by an analogy to
// `shape_of_product` that did not actually hold (`shape_of_product`
// never dereferences its own `component_shapes` elements, so a null
// entry there is merely stored, not immediately crashed on; `capshape`
// does dereference `type->kind` immediately, at every recursion level,
// making a null input a crash, not a latent bad value) -- and found
// `capshape`'s *only actual caller today* is this module's own tests
// (no `infer_shape` dispatcher exists yet to supply a real, Phase-3.2-
// guaranteed-non-null `ast::TypePtr`), i.e. exactly the "constructed by
// hand, can get wrong" risk category the original doc comment claimed
// was unique to `Shape`, not `Type`. Null-checking here instead matches
// this project's established house convention for exactly this role (a
// function that is "the trust boundary" over a possibly-malformed
// pointer-bearing tree it dereferences): `src/source/size/certificate`'s
// own public builders are documented as deliberately unvalidated
// specifically because the certificate *checker* is the trust boundary,
// reproduced under ASan/UBSan (`test_certificate_null_safety.cpp`).
//
// NOT the same as the paper's separate ABI-array-parameter rule
// (main.pdf p.11, main text, not a named Table 8 row): "ABI array
// argument `x:arr(tau,N)` receives a fresh formal length `n_x`... Its
// shape is `array(n_x,n_x,N;capshape(tau))`" -- an EXACT shape bound to
// a fresh symbol, used specifically to seed `ShapeContext` for a real
// ABI array parameter, versus this row's inexact (`star`) fallback.
// This slice's first version claimed that rule was "likely ABI/lowering
// -phase wiring, not this phase's own scope," without logging or asking
// -- a spec-auditor review found this unsupported: the rule's own
// paragraph is textually inside main.pdf Sec. 4.3, which CLAUDE.md's own
// phase-reading table assigns to Phase 3 (not Phase 8's abi-layout.md);
// Sec. 4.2's term grammar explicitly names "ABI lengths" (the `n_x`
// symbol this rule mints) as a Phase-3 term-grammar concept; and `n_x`
// seeds Sigma/K_f construction, Phase 3's own apparatus (AM-003 already
// ties formal array-length symbols to Sigma). AM-030 (approved
// 2026-10-06) corrects the rule's *categorization* to Phase 3's own
// scope, but it is still not *implemented* in this already-complete
// slice -- it needs its own future slice (an EXACT-shaped variant
// minting a fresh symbol, tied to live ShapeContext-population wiring
// this module does not have yet for any row), not a trivial addition to
// `capshape` itself, which this function already provides in full for
// the "capacity fallback" row's own scope.
[[nodiscard]] ShapeOutcome capshape(const ast::TypePtr &type, SourceSpan span);

// Table 8's "builder" row: "count `(lambda,u;nu)`, body `kappa_b(i)`" ->
// "`array(lambda,u,N;squnion_{i<u} kappa_b(i))`" (main.pdf p.40).
// `exact`/`upper` are the already-computed `(lambda,u)` pair from Table
// 6's count judgment for the builder's own count expression (the same
// "already-computed, not ast::Expr" pattern every function in this
// module follows); `capacity` is the builder's own declared `N`;
// `body_shape` is the already-computed shape of the builder's body
// expression (eventually produced by this module's still-deferred
// `infer_shape` dispatcher, under a `ShapeContext` extended with an
// entry for the builder's own index binder -- not to be confused with
// `src/source/size/count`'s own, differently-shaped `IndexContext`
// (symbol + upper bound), which is a separate map this module's
// `ShapeContext` does not carry).
//
// AM-031 (approved 2026-10-06): the join `squnion_{i<u} kappa_b(i)`
// degenerates to one computation of `kappa_b` itself -- Table 8's shape
// judgment is explicitly static (main.pdf p.11, Sec. 4.3 main text:
// "does not allocate or evaluate `e`; it summarises result sizes" --
// not App. A.5, p.39, which a spec-auditor review of the eighth slice
// found this citation had been misattributed to), so a body's shape cannot
// actually depend on which concrete runtime value the index binder `i`
// takes, only on `i`'s symbolic count-refinement identity, which is the
// same across every conceptual "iteration"; Table 7's T-Build further
// confirms a builder body has no accumulator, so there is no other free
// variable whose shape could evolve per iteration either. The result is
// accordingly exactly `array(lambda,u,N;body_shape)`, with `body_shape`
// used directly as `kappa_b` -- no actual per-index iteration or join
// mechanism is built (nor is one needed: `u` may be symbolic, and this
// design sidesteps ever needing to "iterate" over it at all). A
// spec-auditor review independently re-derived this from main.pdf pp.9-12
// directly and could not construct a counterexample where `kappa_b(i)`
// actually varies with `i` under this language's current grammar.
//
// Null trust boundary: `upper`/`body_shape` (and any `TermPtr` inside a
// present `exact`) are checked non-null -- `INT001`, not a SIZ code --
// mirroring every sibling function in this module: this function's only
// real caller today is this module's own tests, not a guaranteed-valid
// pipeline (no `infer_shape` dispatcher exists yet to supply real,
// already-validated inputs).
//
// AM-032 (approved 2026-10-06): unlike the null check above, this
// function deliberately does NOT check "lambda<=u<=N" (main.pdf p.11:
// "A valid shape satisfies Delta|=0<=s<=u<=N when lambda=s"), even
// though shape_of_proj's own doc comment (second slice) specifically
// anticipated the builder row would need exactly this check, raising
// SIZ007 on violation. A spec-auditor review flagged this omission as
// unreconciled; AM-032 resolves it: "u<=N" is already certified by
// check_count_admissibility's own SIZ005 before any caller could
// legitimately obtain a builder's own (lambda,u) pair at all; "lambda<=u"
// is separately maintained as an invariant throughout infer_count's own
// already-audited rule set (C-Const: lambda=u; C-If-E: the shared exact
// is each branch's own lambda, already <= that branch's own u; C-Add/
// C-Sub: each follows from the same invariant on their operands,
// AM-025; C-Idx: i<u_n is the index binder's own externally-established
// invariant). By the time check_count_admissibility returns ok=true,
// "lambda<=u<=N" already holds for its own result, so this function's
// only legitimate caller always supplies an already-valid triple --
// consistent with every other function in this module trusting its
// caller-supplied inputs. SIZ007 remains reserved for a row where this
// invariant is NOT already guaranteed upstream by the time that row's
// own function runs; a malformed triple reaching this function directly
// (bypassing that upstream guarantee, e.g. in a test) is accordingly
// accepted silently, not rejected -- a deliberate trust-boundary choice,
// not an oversight, exercised directly by this header's own test suite.
[[nodiscard]] ShapeOutcome shape_of_builder(std::optional<certificate::TermPtr> exact, certificate::TermPtr upper,
                                             std::uint32_t capacity, const ShapePtr &body_shape, SourceSpan span);

// Table 8's "fold" row: "`kappa_0, kappa_i+1 = Kb(i,kappa_i)`" ->
// "`kappa_s` for exact `s`; otherwise `squnion_{0<=j<=u} kappa_j`"
// (main.pdf p.40). Unlike every row above, this one is a genuine
// recurrence over shapes, not a single already-computed-inputs-in,
// single-shape-out function -- `Kb` (App. A.5, p.39: "the body shape
// transformer checked under its index/accumulator binders") takes both
// the index binder `i` and the accumulator's own current shape
// `kappa_i`, unlike the "builder" row's `kappa_b(i)` (no accumulator
// exists for a builder, T-Build). AM-033 (approved 2026-10-07) scopes
// this slice narrowly, via a re-derivation of AM-031's own "static
// judgment, no value-dependence on the index" argument (already
// independently spec-auditor-confirmed for builder's kappa_b(i)):
// since the shape judgment "does not allocate or evaluate e; it
// summarises result sizes" (main.pdf p.11, Sec. 4.3 main text -- not
// App. A.5, p.39, a misattribution a spec-auditor review of this slice
// found carried forward from AM-031's own citation, corrected here and
// in shape_of_builder's doc comment above), the fold body's shape
// derivation cannot observe `i`'s concrete runtime value at any
// iteration -- only its fixed symbolic count-refinement identity, the
// same at every conceptual iteration. `Kb`'s explicit `i`-argument is
// accordingly read as existing only because the body is typed/shaped
// *under* both binders textually (T-Fold: `Gamma,i:idx(N),x:tau_x`),
// not because the transformer's *output* shape can depend on `i`'s
// value -- so `Kb` reduces to a single pure function `F(kappa)` of the
// accumulator's own shape alone.
//
// Under that reading, checking *one* application, `F(kappa_0)` against
// `kappa_0`, soundly decides the entire (possibly symbolic-length)
// recurrence by induction: if `F(kappa_0)=kappa_0`, then `F` applied
// again to `kappa_0` still yields `kappa_0`, so `kappa_j=kappa_0` for
// *every* iteration index `j>=0` -- for any exact `s` (even a
// non-literal symbol) or upper bound `u` (even non-literal), with no
// unrolling and no new "symbolic iteration" machinery. `initial_shape`
// is the already-computed `kappa_0` (the fold's initial-accumulator
// expression's own shape); `step_shape` is the already-computed
// `kappa_1=F(kappa_0)` (the fold body's own shape, evaluated under a
// ShapeContext where the accumulator binder is bound to `kappa_0` --
// the same "already-computed, not ast::Expr" pattern every function in
// this module follows; this function does not itself iterate or call
// any dispatcher). Comparing the two distinguishes two different
// reasons they can fail to coincide, mirroring join_shapes's own
// AM-028-settled distinction between a trust-boundary mismatch and a
// real result: a *structural* mismatch (different kind, array capacity,
// or product arity at any level) is a caller precondition violation --
// `INT001`, not a SIZ code -- since T-Fold/TYP010 already guarantees
// the body's synthesized type matches the accumulator's own declared
// type at every iteration, so `initial_shape` and `step_shape` provably
// share the same static type for any real, legitimately-typechecked
// fold, the identical reasoning join_shapes's own mismatch branches
// already rely on. Within a shared structure, if every exact/upper
// term (certificate::terms_equal) and every nested element/component
// shape also matches, the result is `initial_shape` itself (returned by
// pointer identity, mirroring shape_of_builder's own test discipline)
// -- sound for any count, exact or inexact, literal or symbolic.
//
// If the structure matches but some exact/upper term or nested
// element/component genuinely differs, this function raises `SIZ008`
// ("Recursive element-shape join or fold recurrence cannot be formed")
// rather than guess: a genuinely shape-varying accumulator (e.g. a fold
// that reconstructs a logically-growing array accumulator each
// iteration) needs either bounded literal-count unrolling (only
// possible when the count is a known compile-time literal) or a
// general fixed-point/widening solver -- neither built anywhere in this
// project -- so it is conservatively rejected, spending SIZ008 exactly
// where AM-028 (2026-10-06) reserved it ("folding over a symbolic
// iteration count that cannot be mechanically enumerated... some other
// proof technique (e.g. an inductive/fixed-point argument over Kb)
// would be needed instead, and failing to produce one is what SIZ008
// names"), and matching main.pdf p.10's general fallback principle ("a
// true formula for which no accepted certificate is supplied is
// conservatively rejected") generalized here to shape derivation. This
// is sound but disclosed-incomplete: a fold whose accumulator shape
// genuinely stabilizes only after *more than one* step, one a bounded
// literal-count unrolling could otherwise accept, or one that
// oscillates without ever converging to a single fixed shape (a
// spec-auditor review of this slice confirmed such a case is real and
// grammar-expressible -- a product accumulator whose body swaps two
// differently-shaped components each iteration has period exactly two,
// kappa_2=kappa_0 -- and confirmed this function still rejects it
// conservatively via SIZ008 rather than wrongly accepting it; see
// test_shape_fold.cpp's own period-two regression test), is still
// rejected by this slice -- left to a future, separately-scoped slice,
// not hidden (AM-033's own "Scientific effect" entry).
//
// Null trust boundary: `initial_shape`/`step_shape` are checked
// non-null -- `INT001`, not a SIZ code -- mirroring every sibling
// function in this module: this function's only real caller today is
// this module's own tests, not a guaranteed-valid pipeline (no
// `infer_shape` dispatcher exists yet to supply real, already-validated
// inputs).
[[nodiscard]] ShapeOutcome shape_of_fold(const ShapePtr &initial_shape, const ShapePtr &step_shape, SourceSpan span);

} // namespace boundfin::source::size::shape
