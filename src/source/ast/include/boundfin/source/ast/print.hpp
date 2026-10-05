#pragma once

#include "boundfin/source/ast/ast.hpp"

#include <string>

namespace boundfin::source::ast {

// Renders `module` as valid grammar.ebnf concrete syntax that reparses to a
// structurally equivalent AST (PLAN.md Phase 2 test item: "generated
// parse/print/parse cases"). Deliberately always parenthesizes every
// operator subexpression rather than computing minimal precedence-aware
// parentheses: this is a round-trip-testing printer, not a user-facing
// pretty-printer, and always-parenthesize is trivially correct regardless
// of precedence-table subtleties.
[[nodiscard]] std::string print(const Module &module);

} // namespace boundfin::source::ast
