#pragma once

#include "boundfin/source/ast/ast.hpp"

#include <nlohmann/json.hpp>

namespace boundfin::source::ast {

// Deterministic structural serialization of a parsed module (PLAN.md Phase 2
// "Deliverables: ... deterministic serialization"). nlohmann::json's default
// object type sorts keys, so dump() does not depend on field insertion
// order. When `include_spans` is false, source byte offsets are omitted, so
// two ASTs that differ only in surface formatting -- in particular a fresh
// `source text -> parse -> AST -> print -> source text' -> parse -> AST'`
// round trip, where AST and AST' were never parsed from the same bytes --
// serialize identically whenever they are structurally equivalent. That is
// the AST-equality mechanism this module's parse/print/parse tests use.
[[nodiscard]] nlohmann::json serialize(const Module &module, bool include_spans = true);

} // namespace boundfin::source::ast
