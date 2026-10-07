#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/size/shape/shape.hpp"

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

} // namespace boundfin::source::size::infer
