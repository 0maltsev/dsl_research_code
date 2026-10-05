#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"

#include <optional>
#include <string>

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
// here: none of the 3 rules this slice implements (C-Const/C-If-E/
// C-If-U) ever needs nu as an explicit term in its own premises or
// result construction. This is *not* true of Table 6 as a whole --
// C-Idx's premise ("body index i under 0<=i<nu_n<=u_n", main.pdf p.38)
// explicitly references nu_n, so whether CountResult needs a nu-bearing
// field (vs. e.g. a fresh symbol synthesized at the T-Fold/T-Build call
// site that uses it) is an open representational question, not yet
// decided, to resolve when C-Idx itself is scoped -- flagged here so it
// is not silently reopened as a "nothing ever needs nu" assumption at
// that point (a spec-auditor review of this slice, 2026-10-05, caught an
// earlier draft of this comment overclaiming exactly that).
struct CountResult {
  std::optional<certificate::TermPtr> exact; // lambda; std::nullopt means "star"
  certificate::TermPtr upper;                // u
};

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
// This is one slice of Phase 3.4: it implements exactly 3 of Table 6's
// 10 rules -- C-Const, C-If-E, C-If-U -- see STATUS.md's Phase 3.4 entry
// for why the other 7 are deferred rather than guessed. Every
// expression kind not covered by an implemented rule raises `SIZ003`
// ("Expression has no accepted count-refinement rule"), matching the
// paper's own closing statement for Table 6 (p.37): "No other
// expression derives a count refinement."
[[nodiscard]] CountOutcome infer_count(const ast::Expr &expr);

} // namespace boundfin::source::size::count
