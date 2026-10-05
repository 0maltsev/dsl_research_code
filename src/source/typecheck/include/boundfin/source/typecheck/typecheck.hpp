#pragma once

#include "boundfin/source/ast/ast.hpp"

#include <optional>
#include <string>

namespace boundfin::source::typecheck {

using boundfin::source::ast::SourceSpan;

// A "TYPxxx" (type) diagnostic (paper Table 7, `app:typing-size`;
// diagnostics-and-status.md's "Type diagnostics" TYP001-TYP012). This
// module's first-error design is consistent with
// boundfin::source::{lex,parse,resolve}. It never raises a NAM/EFF/SIZ code:
// every EFF00x effect-rejection case this frozen grammar can syntactically
// reach is already handled earlier (EFF004 by src/source/resolve per
// AM-019; every other EFF00x has no reachable AST shape under this grammar,
// paper Sec. 4.1's closed construct list); count/size admissibility
// (paper's separate `Sigma;Delta;Gamma |-cnt e => (lambda,u;nu)` judgment,
// Table 6, SIZ00x) is src/source/size's responsibility (Phase 3.3+, not yet
// implemented), not this module's -- see AM-018's precedent and this
// module's own design note on FoldExpr/BuildExpr below.
struct TypeDiagnostic {
  std::string code;
  SourceSpan span;
  std::string message;
};

struct TypecheckResult {
  bool ok = false;
  std::optional<TypeDiagnostic> diagnostic;
};

// Typechecks `module` in place (paper Sec. 4.2 and App. A.5, Table 7
// "Typing rules for every source expression form"; Table 5 "Scalar
// primitive signatures"). Must run after a successful
// `boundfin::source::resolve::resolve_module` (this module reads
// `resolved_binding`/`resolved_callee_rank`/every `binding` field set by
// resolve, and performs no name resolution of its own).
//
// On success, every `ast::Expr` reached by the ordinary `Sigma;Delta;Gamma
// |- e : tau` judgment gets `inferred_type` set (every kind except the
// `count` subexpression of FoldExpr/BuildExpr -- see below), every
// `ast::UnaryPrimitiveExpr`/`ast::BinaryPrimitiveExpr` gets
// `resolved_primitive` set to its exact Table 5 primitive, and each
// function's body type is checked against its declared result type
// (AM-021: TYP003).
//
// FoldExpr/BuildExpr's `count` subexpression is deliberately left
// untouched (`inferred_type` stays `std::nullopt`): the paper's main text
// (Sec. 4.2, before Table 7's abbreviation) states the count is typed by a
// *separate* auxiliary judgment, `Sigma;Delta;Gamma |-cnt e => (lambda,u;nu)`,
// which "types e as i32" as part of establishing its admissibility
// (0 <= nu <= u <= N) -- not by this module's ordinary `|- e : tau`
// judgment. That auxiliary judgment, and therefore the count's own typing,
// belongs to src/source/size (Phase 3.3+, SIZ003-SIZ010), consistent with
// docs/architecture.md's module table and AM-018's precedent of deferring
// every count/capacity-shaped check to that module rather than letting an
// earlier, information-sufficient phase anticipate it.
//
// First-error: on failure, `module` may be partially annotated (everything
// typed strictly before the failure point, in declaration and left-to-right
// evaluation order) and `diagnostic` names the first problem found.
[[nodiscard]] TypecheckResult typecheck_module(ast::Module &module);

} // namespace boundfin::source::typecheck
