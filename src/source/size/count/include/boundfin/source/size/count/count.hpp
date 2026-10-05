#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace boundfin::source::size::count {

using boundfin::source::ast::SourceSpan;

// The result of the count judgment (main.pdf Sec. 4.2, p.10: "The
// auxiliary judgment Sigma;Delta;Gamma |-cnt e => (lambda,u;nu) types e as
// i32, names its nonnegative signed interpretation nu, and establishes
// 0 <= nu <= u <= 2^31-1"). `exact` is the judgment's lambda component:
// present (a certificate::Term, per Sec. 4.2's "Its exact component
// lambda is either a term s, with a checked derivation of nu = s, or
// star") when the expression's value is known exactly as a term, absent
// (the paper's "star") otherwise. `upper` is always present (u).
//
// The judgment's third component, nu (the expression's own real
// nonnegative signed runtime value), is deliberately not represented
// here: none of the rules currently implemented (C-Const/C-If-E/C-If-U/
// C-Add/C-Sub/C-Idx) ever needs nu as an explicit term in CountResult
// itself -- C-Add's certificate obligation is phrased over u (AM-023),
// C-Sub's corrected obligation (AM-025) is phrased over `exact`
// (lambda1), and C-Idx's own result (AM-026) only needs u_n and a fresh
// symbol for the index, supplied via IndexContext below, not nu_n. This
// does *not* mean nu is never needed by Table 6 as a whole: the full
// formal T-Fold/T-Build rules (main.pdf Sec. 4.2, p.11) mint nu_n as a
// fresh symbol and add it to the body's own certificate hypothesis set
// (Delta_n = Delta u {0<=nu_n<=u_n<=N}, plus "nu_n=s_n when lambda_n=s_n")
// -- AM-026 deliberately defers building that *admissibility* machinery
// (which would also require threading a nonempty Delta through every
// rule, not just C-Idx) to a later slice, scoping this one to C-Idx used
// standalone via a caller-supplied index context instead.
struct CountResult {
  std::optional<certificate::TermPtr> exact; // lambda; std::nullopt means "star"
  certificate::TermPtr upper;                // u
};

// C-Idx's per-index-binder context (AM-026): the enclosing fold/build's
// own established upper bound (u_n) and a fresh symbol standing for the
// index binder's own value, keyed by the binder's BindingId (set by
// Phase 3.1's resolver; globally unique even under lexical shadowing, so
// the *map key* never collides between nested fold/build index binders).
// Supplied by the caller -- this module does not yet derive it from live
// `ast::FoldExpr`/`BuildExpr` traversal (AM-026 defers that wiring).
//
// BindingId uniqueness does NOT by itself make `symbol`'s own name
// collision-free: `certificate::SymbolTerm` carries only a `std::string`,
// compared structurally by `terms_equal` with no link back to BindingId.
// A future caller that mints two nested index binders' symbols with the
// same literal string (e.g. both "idx") would make every downstream
// structural-equality check (e.g. C-If-E's "both branches' exact terms
// are equal") unable to distinguish two semantically different index
// variables -- the same *shape* of defect as AM-023's original C-Sub
// certificate (a looser tracked quantity silently standing in for a
// distinct real one). Not reachable from this slice's own code (every
// test builds one isolated IndexContext per infer_count call, no
// cross-call reuse), but the live `FoldExpr`/`BuildExpr`-wiring slice
// that populates this map from real nested AST structure MUST mint
// symbol names deterministically from BindingId (e.g. "idx#42", never a
// fixed/reused literal) to stay sound.
struct IndexBinding {
  certificate::TermPtr symbol; // a fresh Symbol term naming this index binder
  certificate::TermPtr upper;  // u_n, the enclosing count's own upper bound
};
using IndexContext = std::unordered_map<ast::BindingId, IndexBinding>;

// A "SIZxxx" (size/count) diagnostic (diagnostics-and-status.md's
// "Size/count diagnostics" SIZ001-SIZ012).
struct CountDiagnostic {
  std::string code;
  SourceSpan span;
  std::string message;
};

struct CountOutcome {
  bool ok = false;
  std::optional<CountResult> result;
  std::optional<CountDiagnostic> diagnostic;
};

// Infers a count refinement for `expr` per main.pdf Table 6 ("Accepted
// count-refinement fragment", Appendix A.4, p.38). `expr` is expected to
// already be typechecked (Phase 3.2); this function does not re-derive
// or re-check typing (e.g. C-If's "guard Boolean" premise is trusted
// from Phase 3.2's own T-If check, not re-verified here).
//
// `index_context` supplies C-Idx's per-binder data (AM-026); omit it (or
// pass `{}`) outside a fold/build body, where it is always empty and a
// bare variable reference accordingly always fails (Table 6 has no
// generic C-Var rule for an arbitrary binding). This module does not
// itself derive `index_context` from live `ast::FoldExpr`/`BuildExpr`
// traversal (deferred to a later slice, AM-026) -- a caller exercising
// C-Idx builds and supplies it directly.
//
// This is Phase 3.4's third slice: it implements 6 of Table 6's 10
// rules -- C-Const, C-If-E, C-If-U, C-Add, C-Sub, C-Idx (AM-023, AM-024,
// AM-025, AM-026) -- see STATUS.md's Phase 3.4 entry for why the other 4
// (C-ABI, C-Len-E, C-Len-U, C-Call) are deferred rather than guessed.
// Every expression kind/operator not covered by an implemented rule
// raises `SIZ003` ("Expression has no accepted count-refinement rule"),
// matching the paper's own closing statement for Table 6 (p.37): "No
// other expression derives a count refinement." `C-Add`/`C-Sub`
// specifically can instead raise `SIZ006` ("Count add/sub lacks the
// required no-wrap/nonnegative certificate") when their own premise's
// certificate obligation fails -- AM-023 for C-Add; AM-025 for C-Sub,
// correcting AM-023's original C-Sub reading after a spec-auditor review
// found and this session independently reproduced a real soundness bug
// (certifying only "u2<=u1" let a deterministically-underflowing nested
// subtraction, (10-9)-2, through as ok=true).
[[nodiscard]] CountOutcome infer_count(const ast::Expr &expr, const IndexContext &index_context = {});

} // namespace boundfin::source::size::count
