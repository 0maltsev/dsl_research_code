#pragma once

#include "boundfin/source/lex/token.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace boundfin::source::lex {

// One of diagnostics-and-status.md's "Lexical diagnostics" (LEX001-LEX007),
// except LEX005 ("Reserved word used where an identifier is required"),
// which requires syntactic context (what production is being matched) that
// this standalone tokenizer does not have; it is raised by the parser.
struct LexDiagnostic {
  std::string code; // e.g. "LEX001"
  SourceSpan span;
  std::string message;
};

// This tokenizer stops at the first diagnostic-worthy event (first-error
// behavior, consistent with this repository's general left-to-right
// first-error convention) rather than collecting every lexical error in one
// pass. `ok == true` iff the entire source tokenized cleanly, in which case
// `tokens` is complete and ends with exactly one `EndOfFile` token whose
// span is `[source.size(), source.size())`; `diagnostic` is empty.
// `ok == false` iff a diagnostic-worthy event was found, in which case
// `diagnostic` is set and, for every code except LEX001/LEX002, `tokens`
// holds every token successfully recognized strictly before it (for
// partial-recovery tooling). LEX001 (malformed UTF-8 or a leading BOM) and
// LEX002 (a non-ASCII character) are checked as whole-buffer preconditions
// before any token is attempted, so for those two codes specifically
// `tokens` is always empty, regardless of how many valid tokens precede the
// offending byte.
struct LexResult {
  bool ok = false;
  std::vector<Token> tokens;
  std::optional<LexDiagnostic> diagnostic;
};

// Tokenizes `source` per grammar.ebnf's lexical rules. `source` is treated
// as raw bytes, not assumed to already be valid UTF-8 or ASCII -- that is
// exactly what LEX001/LEX002 check.
[[nodiscard]] LexResult tokenize(std::string_view source);

} // namespace boundfin::source::lex
