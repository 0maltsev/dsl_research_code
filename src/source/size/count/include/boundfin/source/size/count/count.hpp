#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"

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
// `shape_context` supplies C-Len-E/C-Len-U's own per-binding data
// (AM-034, 2026-10-07): a `src/source/size/shape::ShapeContext`
// (`BindingId -> ShapePtr`), the first dependency this module takes on
// `src/source/size/shape` (previously sibling modules with no
// dependency either direction; `docs/architecture.md`'s module table
// does not distinguish between them, both being part of one planned
// `src/source/size` row). Omit it (or pass `{}`) when no `len(e)`
// subexpression needs a count refinement; `infer_len` (count.cpp) is
// scoped to a `VarExpr` operand with a recorded `shape_context` entry
// only, mirroring C-Idx's own narrow "via a caller-supplied context,
// not full live `ast::Expr` traversal" precedent (AM-026) -- any other
// `len(e)` operand (not a `VarExpr`, or a `VarExpr` absent from
// `shape_context`) falls through to `SIZ003`, deferred to a later
// live-wiring slice (a fully general operand needs the still-undeferred
// `infer_shape` dispatcher to compute its own shape first).
//
// Phase 3.4 implements 8 of Table 6's 10 rules so far -- C-Const,
// C-If-E, C-If-U, C-Add, C-Sub, C-Idx, C-Len-E, C-Len-U (AM-023, AM-024,
// AM-025, AM-026, AM-034) -- see STATUS.md's Phase 3.4 entry for why the
// other 2 (C-ABI, C-Call) are deferred rather than guessed (AM-034: both
// are tied up with the still-undesigned Sigma/K_f/Q_f-construction
// machinery, a materially larger, separately-scoped future milestone).
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
[[nodiscard]] CountOutcome infer_count(const ast::Expr &expr, const IndexContext &index_context = {},
                                        const shape::ShapeContext &shape_context = {});

struct AdmissibilityOutcome {
  bool ok = false;
  std::optional<IndexBinding> binding; // the (symbol, u_n) pair for the index binder, on success
  std::optional<CountDiagnostic> diagnostic;
};

// Checks the count-admissibility premise T-Fold and T-Build share beyond
// ordinary typing (main.pdf Sec. 4.2, p.11): "Sigma;Delta;Gamma |-cnt en
// => (lambda_n,u_n;nu_n)" then "Delta |-cert u_n<=N". Infers a count
// refinement for `count_expr` (the fold/build's own count subexpression;
// `outer_context` lets `count_expr` itself reference an *enclosing*
// fold/build's index binder, e.g. a nested fold whose count is a bare
// reference to an outer index), certifies the result's upper bound
// against `capacity` (the declared N) via ClosedEval -- conservative,
// not unsound, when the upper bound involves a symbol inherited from
// `outer_context` that cannot be reduced to a closed value (main.pdf
// p.10: "A true formula for which no accepted certificate is supplied
// is conservatively rejected") -- and, on success, mints a symbol for
// `index_binding_id` named deterministically from the id itself (never a
// fixed/reused literal, per the third slice's documented requirement)
// for the caller to add to the `IndexContext` it threads into the body.
//
// Does not itself mint nu_n or construct Delta_n (main.pdf's
// Delta_n = Delta u {0<=nu_n<=u_n<=N}): this function only computes the
// IndexContext extension C-Idx needs, not a general certificate-
// hypothesis set for the body's own nested certificates (AM-026 defers
// that). Nor does it walk `count_expr`'s enclosing module looking for
// further fold/build nodes to check -- it checks exactly one given
// count expression/capacity/index-binding triple, directly testable on
// an AST node extracted from a parsed-resolved-typechecked module. Not
// yet invoked from `typecheck` or any module-wide traversal.
//
// Trust boundary: `index_binding_id` is taken as given, not verified
// against `count_expr`/`capacity` -- nothing here checks it is actually
// *the* fold/build these belong to (mirrors `check_certificate` trusting
// its caller-supplied `delta`, already an accepted pattern). The future
// slice that wires this into live `FoldExpr`/`BuildExpr` traversal must
// always pass the matching triple from the same AST node.
[[nodiscard]] AdmissibilityOutcome check_count_admissibility(const ast::Expr &count_expr, std::uint32_t capacity,
                                                              ast::BindingId index_binding_id,
                                                              const IndexContext &outer_context = {});

struct ModuleAdmissibilityOutcome {
  bool ok = false;
  std::optional<CountDiagnostic> diagnostic; // the first-encountered failure, module-traversal order
};

// AM-027 (approved 2026-10-06, "separate pass" option): walks `module` --
// expected to already be resolved (Phase 3.1) and typechecked (Phase
// 3.2); this function does not re-derive or re-check either -- to find
// every FoldExpr/BuildExpr at any nesting depth, in any subexpression
// position, and admissibility-checks each via check_count_admissibility
// above, threading the resulting IndexContext into nested bodies so an
// inner fold/build's own count or body may reference an *enclosing*
// fold/build's index binder (check_count_admissibility's own
// outer_context parameter already supports this; this function is what
// finally populates it from live AST structure instead of a caller-built
// map). Does not modify `src/source/typecheck` -- this is the first
// caller of check_count_admissibility reachable from outside this
// module's own tests, closing the "FoldExpr/BuildExpr's count
// subexpression is untyped" gap left open since Phase 3.2.
//
// Each function body starts its own walk with a fresh, empty
// IndexContext: an index binder is local to its own fold/build and that
// fold/build's lexical descendants; nothing in the grammar lets one
// function's index binder leak into another function's body.
//
// First-error, in module-traversal order: `module.functions` order, and
// within one function body, the same left-to-right, outer-before-inner
// order already used throughout this module and by Phase 3.1's resolver.
//
// A FoldExpr/BuildExpr's own `count` subexpression is itself walked by
// the same general traversal (not left to infer_count's own narrower
// recursion): a spec-auditor review of this slice's first version found
// and independently reproduced a counterexample where a Fold/Build
// hidden inside the *condition* of an `if` nested in an enclosing
// fold/build's `count` expression went unchecked (see count.cpp's
// walk_for_admissibility doc comment for the concrete program and the
// fix). "Any subexpression position" above is therefore accurate as
// implemented, not merely as designed.
[[nodiscard]] ModuleAdmissibilityOutcome check_module_count_admissibility(const ast::Module &module);

// Table 6's C-ABI: "ABI length symbol n_x, declaration capacity N" ->
// "(n_x,N;n_x) and 0<=n_x<=N" (main.pdf p.38). AM-034 found this row
// names no source expression e at all (unlike every sibling row), so it
// is not reached via infer_count's own ExprKind dispatch; AM-035 (step 1
// of a five-step declaration-rank induction ladder for the Sigma/K_f/
// Q_f/substitution initiative) confirms its real role: seeding the
// count-refinement context for a function declaration's own formal ABI
// array parameters, consumed when constructing Sigma(f)'s K_f/Q_f
// (main.pdf p.12: "n_x seeds Gamma_sz so K_f(n_vec) is checked with the
// body and stored in Sigma"; AM-003: Q_f is "derived under exactly the
// same formal parameters, formal array-length symbols... as K_f").
//
// Mints n_x deterministically as "abi#" + to_string(param_binding) --
// disjoint from check_count_admissibility's own "idx#" prefix above,
// closing the namespace-collision gap that function's own doc comment
// has flagged since Phase 3.4's fourth slice -- and identical to
// src/source/size/shape's shape_of_abi_array_param, which mints the same
// n_x for the same ABI array parameter via the same convention, so both
// modules' independently-minted symbols agree structurally
// (certificate::terms_equal compares Symbol names, not object identity)
// without sharing a context object. No certificate obligation is raised
// here: "0<=n_x<=N" is axiomatically true by construction -- n_x IS the
// fresh formal length, under that very bound, by definition (main.pdf
// p.11 main text) -- the same "no certificate obligation at this call
// site" reasoning AM-026 already established for C-Idx. A spec-auditor
// review of this slice located the actual external mechanism that makes
// this true before any static-analysis code (including this function)
// ever runs: main.pdf p.12, Sec. 4.4 ("Well-formedness and rejection"):
// "Rejected modules have no core-language evaluation. Invocation
// rejection yields InvalidABI before the entry body begins" -- an
// out-of-bounds actual length is an ABI-validity failure (InvalidABI,
// status 4, diagnostics-and-status.md), checked before invocation, not
// an in-language certificate derivation this function would need to
// produce; Table 6's own C-ABI row (p.38) has no "certificate ..."
// phrase in its Premises column either, unlike C-Add/C-Sub's NW_+/NW_-.
// `exact=upper=n_x` matches the row's own result tuple exactly: the
// formal length is its own exact value, and its only a priori known
// upper bound (before any actual is substituted in at a call site, step
// 4/5 of AM-035's ladder) is the declared capacity N itself.
//
// Infallible (no SourceSpan/CountOutcome): `capacity` and `param_binding`
// are plain values, not pointers, so there is nothing to null-check --
// mirrors shape_of_product's identical infallible-constructor precedent.
[[nodiscard]] CountResult count_of_abi_param(std::uint32_t capacity, ast::BindingId param_binding);

} // namespace boundfin::source::size::count
