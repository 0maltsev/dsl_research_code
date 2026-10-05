#pragma once

#include "boundfin/source/ast/ast.hpp"

#include <optional>
#include <string>

namespace boundfin::source::resolve {

using boundfin::source::ast::SourceSpan;

// A "NAMxxx" (name resolution) or "EFF004" (recursion) diagnostic.
// diagnostics-and-status.md's Record rule: on failure, exactly one stable
// code is present (this module's first-error design, consistent with
// boundfin::source::lex and boundfin::source::parse).
struct ResolveDiagnostic {
  std::string code;
  SourceSpan span;
  std::string message;
};

struct ResolveResult {
  bool ok = false;
  std::optional<ResolveDiagnostic> diagnostic;
};

// Resolves and alpha-renames `module` in place (paper Sec. 4.1's module/call
// -graph definition; grammar.ebnf's static concrete-syntax constraint 4;
// diagnostics-and-status.md's "Name-resolution diagnostics" NAM001-NAM007,
// plus EFF004 for direct self-recursion specifically). On success, every
// binding-introducing site (ast::Parameter, ast::LetExpr, ast::FoldExpr's
// two binders, ast::BuildExpr's index) gets a fresh ast::BindingId, every
// ast::VarExpr gets `resolved_binding` set to the ast::BindingId it refers
// to, every ast::CallExpr gets `resolved_callee_rank` set to its callee's
// 0-based declaration rank, and `module.export_decl.resolved_target_rank`
// is set. `src/source/resolve` performs no type inference or implicit
// conversion (docs/architecture.md); the result is still an untyped AST,
// now with name/call-graph ambiguity eliminated.
//
// First-error: on failure, `module` may be partially annotated (everything
// resolved strictly before the failure point) and `diagnostic` names the
// first problem found, left to right.
[[nodiscard]] ResolveResult resolve_module(ast::Module &module);

} // namespace boundfin::source::resolve
