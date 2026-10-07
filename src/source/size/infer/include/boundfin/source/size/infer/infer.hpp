#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/size/shape/shape.hpp"

#include <cstddef>
#include <unordered_map>

namespace boundfin::source::size::infer {

using boundfin::source::ast::SourceSpan;

// The unifying Table 8 shape dispatcher (main.pdf Sec. 4.3/App. A.5,
// `Sigma;Delta;Gamma|-sz e=>kappa`), finally recursing into live
// `ast::Expr` structure rather than taking already-computed shapes
// directly the way every row function in `src/source/size/shape` does.
// `src/source/size/shape/shape.hpp`'s own top-of-file comment (first
// slice) named this dispatcher's deferral condition exactly: "deferred
// to the slice that completes every non-`call` row." AM-036 (2026-10-07)
// found that condition could not actually be met from *inside*
// `src/source/size/shape` itself: the "builder" row's own
// `shape_of_builder` needs an `(exact,upper)` pair from Table 6's count
// judgment for the builder's own count expression (`count::infer_count`),
// but `src/source/size/count` already depends on `src/source/size/shape`
// (added for `C-Len-E`/`C-Len-U`, AM-034) -- so `shape` depending back on
// `count` would form a build-level cycle. This module, `infer`, resolves
// it by sitting *above* both (a natural third/fourth sub-component of
// `docs/architecture.md`'s single planned `src/source/size` row, which
// does not distinguish finer ownership between `certificate`/`count`/
// `shape`/`infer`), depending on both without forcing either to depend
// on the other -- `count`'s own one-directional dependency on `shape`
// stays exactly as shipped.
//
// Covers every non-`call` Table 8 row with a real `ExprKind`: scalar/
// variable, product/proj, literal, conditional, let, fold, length/index,
// and builder (8 of Table 8's 9 non-`call` rows; "capacity fallback" is
// the 9th but, like `C-ABI`, names no source expression in its own
// premise -- "type `arr<tau,N>`," not an `e` -- so it is never a live
// dispatch target here, the same reasoning AM-036 applied). `CallExpr`
// raises `SIZ013` ("Expression's Table 8 shape judgment has no
// implemented dispatch rule yet," AM-036, `boundfin-diagnostics-0.1.1`)
// -- deliberately NOT `SIZ003` (Table 6's own "no rule exists at all,"
// a true permanent fact, false here: Table 8's own `call` row exists,
// just isn't implemented by this codebase yet) and NOT `INT001` (a
// genuine consistency-bug code, misleading for an honest, disclosed
// implementation gap).
//
// `shape_context`/`index_context` default to empty, matching every
// sibling module's own "default-empty, every existing call site
// unaffected" precedent. For `FoldExpr`/`BuildExpr`, this function calls
// `count::check_count_admissibility` (validates the count's own
// admissibility, e.g. `SIZ005`, and mints the index binder's symbol for
// `index_context`'s own extension into the body -- exactly mirroring
// `check_module_count_admissibility`'s own established pattern) and, for
// `BuildExpr` only, a separate direct `count::infer_count` call for the
// full `(exact,upper)` pair `shape_of_builder` needs (`check_count_
// admissibility`'s own `AdmissibilityOutcome` discards `exact`, by
// design -- an already-disclosed integration gap this function works
// around by calling `infer_count` a second time, not by changing
// `AdmissibilityOutcome`'s own shape). `FoldExpr` itself needs no count
// data (`AM-033`'s fixed-point design for `shape_of_fold` sidesteps it
// entirely), but still calls `check_count_admissibility` for its own
// `SIZ005` validation and index-binder-symbol minting, since a *nested*
// fold/build inside its body may need that symbol via `index_context`.
// For both `FoldExpr` and `BuildExpr`, `check_count_admissibility` is
// called BEFORE recursing into `initial`/`body` (matching E-Fold/E-Build's
// own left-to-right sequential evaluation, main.pdf p.42, and
// `count.cpp`'s already-shipped `walk_for_admissibility`'s identical
// order) -- a spec-auditor review of this slice's first version found
// `infer_fold` had this reversed (computing `initial`'s own shape before
// checking the count's admissibility), a genuine first-error-order
// defect (`CLAUDE.md` Sec. 6), fixed and reproduced/re-verified.
//
// Known limitation, disclosed not hidden: `check_count_admissibility`'s
// own signature (`src/source/size/count`, Phase 3.4 fourth slice) has no
// `shape_context` parameter at all, unlike `infer_count`'s own signature
// (extended for `C-Len-E`/`C-Len-U`, `AM-034`) -- so a fold/build whose
// own `count` expression itself needs `C-Len-E`/`C-Len-U` (e.g. `len(x)`
// for an already-shaped `x`) cannot resolve via `check_count_
// admissibility`'s own internal `infer_count` call, even when this
// function's own `shape_context` already has the needed entry --
// conservatively falls through to `SIZ003` instead of resolving. This
// function's own *separate*, direct `infer_count` call for `BuildExpr`'s
// `(exact,upper)` pair does NOT have this limitation (it is passed
// `shape_context` directly) -- only `check_count_admissibility`'s own
// internal call does. Fixing this needs extending `check_count_
// admissibility`'s own already-shipped signature, out of this slice's
// own scope; left for a future slice.
[[nodiscard]] shape::ShapeOutcome infer_shape(const ast::Expr &expr, const shape::ShapeContext &shape_context = {},
                                               const count::IndexContext &index_context = {});

// Step 3 of `AM-035`'s five-step declaration-rank induction ladder:
// `Sigma(f)=(signature, K_f, Q_f?, Phi_f, rank)` construction (`AM-003`)
// for a single, *call-free* function declaration -- the first point this
// project can exercise "`K_f(n-vec)` is checked with the body and stored
// in `Sigma`" (main.pdf p.12) end to end, still without needing a
// substitution mechanism (step 4), since the call graph is acyclic
// (`f precedes_M g`, Phase 3.1's resolver) and a function with no
// earlier declarations to call cannot contain a `CallExpr` at all.
//
// This struct holds only the two NEWLY-COMPUTED parts of `Sigma(f)`:
// `signature` (the function's own declared parameter/result types) and
// `rank` (its own 0-based position in `ast::Module::functions`, source
// order) are already directly available on the `ast::FunctionDecl`/its
// enclosing `ast::Module` without needing separate computation here, so
// this struct does not duplicate them; `Phi_f` (the cost/footprint
// transformer, main.pdf Sec. 6) is Phase 5's own, entirely separate
// concern, out of this module's scope.
//
// `k_f` is literally the function body's own already-computed Table 8
// shape (`infer_shape` on the body) -- not a separately-represented
// "transformer object." It is *parameterized* over the function's own
// ABI formal-length symbols (minted by `shape::shape_of_abi_array_param`
// while seeding the initial `ShapeContext` below) because those `n_x`
// symbols appear directly inside the computed shape's own `exact`/
// `upper` terms wherever the body's own arithmetic/joins propagate them
// -- main.pdf p.12's own "At a call, exact or upper actual length
// expressions are substituted simultaneously into `K_f`" describes
// exactly this: substituting a concrete actual term for each `n_x`
// symbol inside an already-computed `k_f` value (deferred to step 4/5).
//
// `q_f` is present only when the function's declared result type is
// `i32` AND the body has an accepted count-refinement derivation
// (`AM-003`'s own exact wording), computed via `count::infer_count` on
// the body under the identical seeded `ShapeContext` (needed for any
// `C-Len-E`/`C-Len-U` use within the body). Absent (`std::nullopt`), NOT
// an error, both for any other declared result type and for an i32
// -result body whose own count-refinement derivation fails with a
// SIZ-coded diagnostic, for ANY such code -- `AM-003`'s own text is
// explicit on this: "If Q_f is absent, the call is legal as an ordinary
// expression but prohibited in the count fragment," meaning a failed
// derivation never rejects the function itself, only narrows how it may
// later be called. Unlike `k_f` (unconditionally required, main.pdf
// p.12 -- its own failure DOES fail `compute_function_summary` as a
// whole), a missing `q_f` is the ordinary, expected outcome for most
// i32-result bodies: e.g. `a+b` for two ordinary scalar parameters
// always fails with `SIZ003` (Table 6 has no `C-Var` rule for an
// arbitrary, non-index-binder variable at all), correctly, with no
// implication the function itself is malformed. An `INT0xx`-coded
// failure is the one exception -- it genuinely IS propagated as a
// `compute_function_summary` failure, not absorbed, since an internal-
// invariant violation (diagnostics-and-status.md's own INT catalogue:
// "never presented as user source rejection") is categorically not "no
// accepted derivation" the way a SIZ code is (a spec-auditor review of
// this slice's first version found the absorption had no such exception
// and fixed it).
struct FunctionSummary {
  shape::ShapePtr k_f;
  std::optional<count::CountResult> q_f;
};

// Reuses `shape::ShapeDiagnostic`'s own `{code,span,message}` shape
// rather than inventing a fourth near-identical diagnostic struct
// (`shape`/`count`/`infer_shape`'s own internal helper already have
// two) -- `compute_function_summary`'s own failures are always either a
// `shape::ShapeOutcome` or `count::CountOutcome` failure propagated
// through unchanged, or an `INT001`/`SIZ013` this function raises
// itself in exactly the same shape.
struct FunctionSummaryOutcome {
  bool ok = false;
  std::optional<FunctionSummary> result;
  std::optional<shape::ShapeDiagnostic> diagnostic;
};

// Seeds a `ShapeContext` for every one of `function`'s own parameters
// (required before `infer_shape`/`infer_count` can process the body at
// all -- `shape_of_var`'s own trust boundary requires a context entry
// for *any* variable reference, including an ordinary scalar parameter,
// not just ABI array ones): an array-typed parameter whose OWN top-level
// declared type is `arr<tau,N>` gets `shape_of_abi_array_param`'s own
// result (a fresh, tight formal length `n_x` minted from this
// parameter's own binding, main.pdf p.11 main text). Every other
// parameter type (scalar, or product, at any nesting depth) gets
// `capshape(parameter.type, span)` directly -- `capshape` is already
// general over the whole Type grammar: for a scalar type this is
// identical to `scalar_shape()` (no case split needed); for a product it
// recurses component-wise via `shape_of_product`, applying this same
// treatment to each field in turn, so an all-scalar product has a fully
// -determined shape with zero remaining ambiguity, and a product field
// that is itself an array gets the "capacity fallback" row's own
// conservative `array(star,N,N;capshape(tau))` (sound, not unsound --
// no fresh formal-length symbol is minted for it, since a nested field
// has no independent binding to mint one from).
//
// A spec-auditor review of this slice's first version found it instead
// rejected EVERY product-typed parameter outright with `SIZ013`,
// justified by a citation to `shape_of_abi_array_param`'s own doc
// comment (step 1) that, on direct re-reading, did not actually support
// blanket rejection: that comment's own disclosed gap is narrowly
// "whether such a nested array independently needs its own fresh formal
// length symbol, or falls back to `capshape`'s own star-shaped
// conservative treatment" -- naming `capshape` as an available, sound
// fallback, not an unresolved blocker, and saying nothing at all about
// an all-scalar product, which has no array anywhere in it and
// therefore no ambiguity whatsoever. This was an unescalated,
// inaccurately-justified scope decision (the kind `CLAUDE.md` Sec. 5
// requires a stop for), not a forced consequence of prior precedent --
// fixed by using `capshape` directly, which the prior disclosure had
// already anticipated and approved as the right fallback. The one
// genuinely open question that remains, deferred and disclosed (not
// silently resolved): whether a future slice should mint an array field
// nested inside a product parameter its own fresh formal-length symbol
// (e.g. via some synthetic per-field naming scheme) rather than always
// falling back to `capshape`'s own conservative, non-tightest treatment.
//
// Then computes `k_f` via `infer_shape` and, when `function.result_type`
// is `i32`, `q_f` via `count::infer_count`, both under the seeded
// context. Does **not** itself check that `function.body` is call-free:
// `infer_shape`'s own `SIZ013` (for `CallExpr`, see above) and
// `count::infer_count`'s own `SIZ003` (Table 6 genuinely has no `C-Call`
// implemented either, the pre-existing, already-shipped fallback every
// unimplemented `ExprKind` shares in that module) already propagate
// naturally through whichever recursive dispatch reaches a call anywhere
// in the body, so no separate up-front precondition check is needed.
[[nodiscard]] FunctionSummaryOutcome compute_function_summary(const ast::FunctionDecl &function);

// A rank-keyed map from an already-summarized function's own 0-based
// declaration rank (`ast::CallExpr::resolved_callee_rank`'s own type;
// `ast::Module::functions`'s own index) to its `FunctionSummary` -- the
// concrete, in-codebase form main.pdf p.11's own `Sigma(f)=(tau_vec->tau,
// K_f,Phi_f)` takes (`Sigma` itself, not a new concept). Caller-supplied,
// not derived here, mirroring `shape::ShapeContext`/`count::IndexContext`'s
// own established "caller-supplied, trusted" pattern -- a module-wide
// pass that builds one of these for real, by calling
// `compute_function_summary` for every function in increasing
// declaration-rank order (the actual mechanism that makes declaration-
// rank induction sound: `f precedes_M g` guarantees `f`'s own entry
// already exists in `Sigma` by the time `g`'s body is processed), is
// deferred to a later, live-wiring slice -- the same "narrow function
// first" pattern this module's own `infer_shape`/`compute_function_
// summary` already followed before this one.
using SigmaContext = std::unordered_map<std::size_t, FunctionSummary>;

// Step 5 of `AM-035`'s ladder, re-sequenced per `AM-037`: reachable once
// step 4a (`certificate::substitute_term`/`shape::substitute_shape`)
// plus this module-wide `Sigma` lookup exist, without waiting for step
// 4b (the certificate-level `Subst` mechanism Table 6's `C-Call` still
// needs). Implements Table 8's own "call" row (main.pdf p.40: "earlier
// transformer `K_f`" -> "simultaneous actual substitution, losing
// exactness to star as needed"), composed with `T-Call`'s own `Sigma(f)`
// lookup premise (main.pdf p.11: "`Sigma(f)=(tau_vec->tau,K_f,Phi_f)
// Sigma;Delta;Gamma|-e_vec:tau_vec f precedes_M g`" over
// "`Sigma,g;Delta;Gamma|-f(e_vec):tau`") and main.pdf p.12's own
// call-site substitution sentence, which `substitute_shape` (`AM-037`
// step 4a) already implements in full.
//
// Looks up the callee's own already-computed `Sigma(f)` entry in `sigma`
// by `call_expr.resolved_callee_rank` (`INT001` if absent -- by
// declaration-rank induction this rank's own entry is guaranteed already
// present in a correctly-built, rank-ordered `Sigma`, so a miss here is
// a caller precondition violation, not a program property, mirroring
// every sibling function's identical trust-boundary reasoning), then
// looks up the callee's own `ast::FunctionDecl` in `module` by the
// identical rank (for its own parameter types/bindings -- deliberately
// NOT duplicated into `FunctionSummary`/`SigmaContext`, per
// `FunctionSummary`'s own already-established "signature already
// available on `ast::FunctionDecl`, don't duplicate" convention).
//
// Computes a Table 8 shape (via `infer_shape`) ONLY for an actual
// argument at an array-typed formal-parameter position -- Table 8's own
// "call" row (main.pdf p.40) premises only on "earlier transformer
// `K_f`", naming no argument-shape premise at all, unlike every other
// composite row (literal, product/proj, conditional, fold, builder); an
// argument at a scalar/product position is never visited here, mirroring
// this same module's own `infer_shape_impl` scalar/primitive dispatch
// case (`infer.cpp`), which deliberately does not recurse into a
// primitive's own operands either -- validating what a non-load-bearing
// child contains is `count::check_module_count_admissibility`'s own,
// separate, already-shipped traversal's job (it visits every
// subexpression of every function body independently), not this
// function's. A spec-auditor review of this slice's first version found
// it instead computed every argument's own shape unconditionally,
// rejecting the whole call if a non-load-bearing one failed -- an
// unforced scope decision, resting on a citation (main.pdf p.11's "does
// not allocate or evaluate `e`" sentence) that does not actually support
// it (that sentence describes the `sz`-judgment's static character, not
// an obligation to recurse into children whose shape the row's own
// result does not use) -- the same class of unescalated,
// inaccurately-cited scope decision `AM-035` step 3's own audit and
// `AM-037`'s own audit each already found and fixed once in this exact
// ladder. Fixed: restricted to array-typed positions only, matching this
// module's own established precedent.
//
// Only an array-typed formal parameter's own position contributes to the
// substitution maps (`AM-035` step 1's own scope: only a top-level
// `arr<tau,N>` parameter mints a fresh formal length `n_x` at all; a
// scalar or product parameter -- including one with a nested array field
// -- mints no symbol whatsoever, so there is nothing in `K_f` for its
// own actual argument to replace -- `compute_function_summary`'s own
// parameter-seeding loop mints none for such a field either, via
// `capshape`'s conservative fallback). For such a parameter, the
// corresponding actual argument's own already-computed shape is expected
// to itself be `Array`-kind (`INT001` otherwise -- `T-Call`'s own
// "ordered actual types agree" premise, already enforced by typecheck's
// `TYP002`, precludes a real type mismatch here) -- its own `upper` term
// is mapped into the upper substitution map under the formal's own
// `"abi#"+to_string(binding)` symbol name (the identical deterministic
// minting convention `shape_of_abi_array_param`/`count_of_abi_param`
// already use, `AM-035` step 1); its own `exact` term, if present, is
// mapped into the exact substitution map under the same name -- if the
// actual's own exact component is absent, nothing is added for that
// symbol to the exact map at all (NOT an error here -- `substitute_
// shape`'s own star-fallback, `AM-037` step 4a, already handles a
// resulting unresolved exact-position symbol as main.pdf p.12's own
// named "star" case).
//
// Finally substitutes both maps into the callee's own `k_f` via
// `substitute_shape` (`AM-037` step 4a) -- the call expression's own
// Table 8 shape is exactly the result. `q_f`/`Phi_f` are not read at all
// (Table 8's "call" row needs only `K_f`; `Q_f` is Table 6's `C-Call`'s
// own concern, step 4b, deliberately out of this function's scope, per
// `AM-037`'s own finding that the two rows need different mechanisms).
[[nodiscard]] shape::ShapeOutcome infer_call(const ast::CallExpr &call_expr, SourceSpan span, const ast::Module &module,
                                              const SigmaContext &sigma, const shape::ShapeContext &shape_context,
                                              const count::IndexContext &index_context);

} // namespace boundfin::source::size::infer
