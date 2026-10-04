#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <array>
#include <string>

namespace {

using boundfin::source::lex::TokenKind;
using boundfin::source::lex::tokenize;

void every_reserved_word_is_its_own_keyword_kind() {
  const std::array<std::pair<std::string, TokenKind>, 24> cases = {{
      {"fn", TokenKind::KwFn},
      {"export", TokenKind::KwExport},
      {"bool", TokenKind::KwBool},
      {"i32", TokenKind::KwI32},
      {"i64", TokenKind::KwI64},
      {"f64", TokenKind::KwF64},
      {"arr", TokenKind::KwArr},
      {"prod", TokenKind::KwProd},
      {"let", TokenKind::KwLet},
      {"in", TokenKind::KwIn},
      {"if", TokenKind::KwIf},
      {"then", TokenKind::KwThen},
      {"else", TokenKind::KwElse},
      {"fold", TokenKind::KwFold},
      {"build", TokenKind::KwBuild},
      {"array", TokenKind::KwArray},
      {"len", TokenKind::KwLen},
      {"proj", TokenKind::KwProj},
      {"abs", TokenKind::KwAbs},
      {"true", TokenKind::KwTrue},
      {"false", TokenKind::KwFalse},
      {"i32bits", TokenKind::KwI32Bits},
      {"i64bits", TokenKind::KwI64Bits},
      {"f64bits", TokenKind::KwF64Bits},
  }};

  for (const auto &[text, kind] : cases) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2}); // keyword + EndOfFile
    BOUNDFIN_CHECK(result.tokens[0].kind == kind);
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, text.size());
  }
}

void identifiers_are_maximal_munch_and_case_sensitive() {
  // "Case is significant" (grammar.ebnf lexical constraint 5): "Fn" is an
  // ordinary identifier, not the "fn" keyword.
  for (const std::string text : {"x", "_x", "x_1", "camelCase", "snake_case_1", "Fn", "LET", "ifx"}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2});
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::Identifier);
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, text.size());
  }
}

void keyword_prefix_does_not_shadow_a_longer_identifier() {
  // Maximal munch: "ifx" is one Identifier token, never "if" + "x".
  const auto result = tokenize("ifx");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2});
  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::Identifier);
  BOUNDFIN_CHECK_EQ(result.tokens[0].span.start, std::size_t{0});
  BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, std::size_t{3});
}

void boundfin_lex_keywords_and_identifiers() {
  every_reserved_word_is_its_own_keyword_kind();
  identifiers_are_maximal_munch_and_case_sensitive();
  keyword_prefix_does_not_shadow_a_longer_identifier();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_keywords_and_identifiers)
