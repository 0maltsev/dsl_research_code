#pragma once

#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/lex/token.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace boundfin::source::parse {

using boundfin::source::lex::SourceSpan;

// A lexical (propagated verbatim from boundfin::source::lex, codes
// "LEX001"-"LEX007") or syntax ("SYNxxx") diagnostic. A distinct type from
// boundfin::source::lex::LexDiagnostic (structurally identical) so that
// Phase 2.1's already-audited lexer module needs no change for this module
// to exist.
struct ParseDiagnostic {
  std::string code;
  SourceSpan span;
  std::string message;
};

struct ParseResult {
  bool ok = false;
  std::optional<boundfin::source::ast::Module> module;
  std::optional<ParseDiagnostic> diagnostic;
};

// Tokenizes then parses `source` into a module per grammar.ebnf's complete
// concrete syntax, applying mandatory operator elaboration (grammar.ebnf
// "Mandatory elaboration before typing/size/cost analysis": surface
// operators become UnaryPrimitiveExpr/BinaryPrimitiveExpr AST nodes,
// type-generic since this AST is untyped) and rejecting a reserved word
// used where an identifier is required (LEX005; this is where it is
// implemented, not in boundfin::source::lex, since it needs the
// grammatical-position context only a parser has). First-error: returns
// `ok == false` with exactly one diagnostic at the first lexical or
// syntactic problem, and no module.
[[nodiscard]] ParseResult parse_module(std::string_view source);

} // namespace boundfin::source::parse
