#include "boundfin/source/lex/lexer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace boundfin::source::lex {

std::string to_string(TokenKind kind) {
  switch (kind) {
  case TokenKind::KwFn:
    return "fn";
  case TokenKind::KwExport:
    return "export";
  case TokenKind::KwBool:
    return "bool";
  case TokenKind::KwI32:
    return "i32";
  case TokenKind::KwI64:
    return "i64";
  case TokenKind::KwF64:
    return "f64";
  case TokenKind::KwArr:
    return "arr";
  case TokenKind::KwProd:
    return "prod";
  case TokenKind::KwLet:
    return "let";
  case TokenKind::KwIn:
    return "in";
  case TokenKind::KwIf:
    return "if";
  case TokenKind::KwThen:
    return "then";
  case TokenKind::KwElse:
    return "else";
  case TokenKind::KwFold:
    return "fold";
  case TokenKind::KwBuild:
    return "build";
  case TokenKind::KwArray:
    return "array";
  case TokenKind::KwLen:
    return "len";
  case TokenKind::KwProj:
    return "proj";
  case TokenKind::KwAbs:
    return "abs";
  case TokenKind::KwTrue:
    return "true";
  case TokenKind::KwFalse:
    return "false";
  case TokenKind::KwI32Bits:
    return "i32bits";
  case TokenKind::KwI64Bits:
    return "i64bits";
  case TokenKind::KwF64Bits:
    return "f64bits";
  case TokenKind::Identifier:
    return "identifier";
  case TokenKind::DecimalNumeral:
    return "decimal numeral";
  case TokenKind::HexNumeral:
    return "hex numeral";
  case TokenKind::LParen:
    return "(";
  case TokenKind::RParen:
    return ")";
  case TokenKind::LBracket:
    return "[";
  case TokenKind::RBracket:
    return "]";
  case TokenKind::Less:
    return "<";
  case TokenKind::Greater:
    return ">";
  case TokenKind::Comma:
    return ",";
  case TokenKind::Colon:
    return ":";
  case TokenKind::Semicolon:
    return ";";
  case TokenKind::Equal:
    return "=";
  case TokenKind::EqualEqual:
    return "==";
  case TokenKind::BangEqual:
    return "!=";
  case TokenKind::LessEqual:
    return "<=";
  case TokenKind::GreaterEqual:
    return ">=";
  case TokenKind::AmpAmp:
    return "&&";
  case TokenKind::PipePipe:
    return "||";
  case TokenKind::Plus:
    return "+";
  case TokenKind::Minus:
    return "-";
  case TokenKind::Star:
    return "*";
  case TokenKind::Slash:
    return "/";
  case TokenKind::Percent:
    return "%";
  case TokenKind::Bang:
    return "!";
  case TokenKind::EndOfFile:
    return "end of file";
  }
  return "unknown token";
}

namespace {

constexpr std::uint64_t kMaxCapacityValue = 2147483647ULL; // 2^31 - 1, SPEC_FREEZE.md

[[nodiscard]] bool is_ascii_letter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

[[nodiscard]] bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] bool is_hex_digit(char c) {
  return is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] bool is_identifier_start(char c) { return is_ascii_letter(c) || c == '_'; }

[[nodiscard]] bool is_identifier_continue(char c) { return is_ascii_letter(c) || is_ascii_digit(c) || c == '_'; }

[[nodiscard]] bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

[[nodiscard]] unsigned hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<unsigned>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<unsigned>(c - 'a') + 10U;
  }
  return static_cast<unsigned>(c - 'A') + 10U;
}

const std::unordered_map<std::string_view, TokenKind> &keyword_table() {
  static const std::unordered_map<std::string_view, TokenKind> table = {
      {"fn", TokenKind::KwFn},         {"export", TokenKind::KwExport}, {"bool", TokenKind::KwBool},
      {"i32", TokenKind::KwI32},       {"i64", TokenKind::KwI64},       {"f64", TokenKind::KwF64},
      {"arr", TokenKind::KwArr},       {"prod", TokenKind::KwProd},     {"let", TokenKind::KwLet},
      {"in", TokenKind::KwIn},         {"if", TokenKind::KwIf},         {"then", TokenKind::KwThen},
      {"else", TokenKind::KwElse},     {"fold", TokenKind::KwFold},     {"build", TokenKind::KwBuild},
      {"array", TokenKind::KwArray},   {"len", TokenKind::KwLen},       {"proj", TokenKind::KwProj},
      {"abs", TokenKind::KwAbs},       {"true", TokenKind::KwTrue},     {"false", TokenKind::KwFalse},
      {"i32bits", TokenKind::KwI32Bits}, {"i64bits", TokenKind::KwI64Bits}, {"f64bits", TokenKind::KwF64Bits},
  };
  return table;
}

// Returns the byte offset of the first byte that is not part of a
// well-formed shortest-form UTF-8 sequence, or std::nullopt if the whole
// buffer is well-formed (grammar.ebnf lexical constraint 1; does not check
// for a leading BOM, which is a separate, otherwise-well-formed sequence).
[[nodiscard]] std::optional<std::size_t> find_first_utf8_error(std::string_view source) {
  const auto n = source.size();
  std::size_t i = 0;
  while (i < n) {
    const auto b0 = static_cast<unsigned char>(source[i]);
    const std::size_t start = i;

    if (b0 <= 0x7FU) {
      i += 1;
      continue;
    }

    std::size_t len = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t min_codepoint = 0;
    if ((b0 & 0xE0U) == 0xC0U) {
      len = 2;
      codepoint = b0 & 0x1FU;
      min_codepoint = 0x80;
    } else if ((b0 & 0xF0U) == 0xE0U) {
      len = 3;
      codepoint = b0 & 0x0FU;
      min_codepoint = 0x800;
    } else if ((b0 & 0xF8U) == 0xF0U) {
      len = 4;
      codepoint = b0 & 0x07U;
      min_codepoint = 0x10000;
    } else {
      return start; // stray continuation byte, or an invalid lead byte
    }

    if (start + len > n) {
      return start; // truncated at end of input
    }
    for (std::size_t k = 1; k < len; ++k) {
      const auto bk = static_cast<unsigned char>(source[start + k]);
      if ((bk & 0xC0U) != 0x80U) {
        return start; // expected continuation byte
      }
      codepoint = (codepoint << 6) | (bk & 0x3FU);
    }

    if (codepoint < min_codepoint) {
      return start; // overlong (non-shortest-form) encoding
    }
    if (codepoint > 0x10FFFFU) {
      return start; // beyond the Unicode range
    }
    if (codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
      return start; // surrogate half: never valid UTF-8
    }

    i = start + len;
  }
  return std::nullopt;
}

// Returns the byte offset of the first byte of the first non-ASCII
// codepoint, or std::nullopt if every codepoint is ASCII. Only meaningful
// once `find_first_utf8_error` has already returned std::nullopt for the
// same buffer: a buffer with no non-ASCII bytes present is trivially also
// one where every multi-byte sequence (if any) is well-formed.
[[nodiscard]] std::optional<std::size_t> find_first_non_ascii_byte(std::string_view source) {
  for (std::size_t i = 0; i < source.size(); ++i) {
    if (static_cast<unsigned char>(source[i]) > 0x7FU) {
      return i;
    }
  }
  return std::nullopt;
}

struct Scanner {
  std::string_view source;
  std::size_t pos = 0;
  std::vector<Token> tokens;

  [[nodiscard]] bool at_end() const { return pos >= source.size(); }
  [[nodiscard]] char current() const { return source[pos]; }
  [[nodiscard]] std::optional<char> peek(std::size_t ahead = 1) const {
    const std::size_t index = pos + ahead;
    if (index >= source.size()) {
      return std::nullopt;
    }
    return source[index];
  }

  [[nodiscard]] LexResult fail(std::string code, SourceSpan span, std::string message) {
    LexResult result;
    result.ok = false;
    result.tokens = std::move(tokens);
    result.diagnostic = LexDiagnostic{std::move(code), span, std::move(message)};
    return result;
  }

  void push(TokenKind kind, SourceSpan span, std::uint64_t numeral_value = 0, int hex_digit_width = 0) {
    tokens.push_back(Token{kind, span, numeral_value, hex_digit_width});
  }
};

} // namespace

LexResult tokenize(std::string_view source) {
  if (const auto utf8_error = find_first_utf8_error(source); utf8_error.has_value()) {
    return LexResult{false, {}, LexDiagnostic{"LEX001", SourceSpan{*utf8_error, *utf8_error + 1},
                                               "invalid or non-shortest-form UTF-8"}};
  }
  if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEFU &&
      static_cast<unsigned char>(source[1]) == 0xBBU && static_cast<unsigned char>(source[2]) == 0xBFU) {
    return LexResult{false, {}, LexDiagnostic{"LEX001", SourceSpan{0, 3}, "source begins with a byte-order mark"}};
  }
  if (const auto non_ascii = find_first_non_ascii_byte(source); non_ascii.has_value()) {
    // The offending codepoint may itself span multiple bytes; find its end
    // by re-running the (now known-valid) UTF-8 decode from this position.
    std::size_t end = *non_ascii + 1;
    const auto rest = source.substr(*non_ascii);
    const auto b0 = static_cast<unsigned char>(rest[0]);
    if ((b0 & 0xE0U) == 0xC0U) {
      end = *non_ascii + 2;
    } else if ((b0 & 0xF0U) == 0xE0U) {
      end = *non_ascii + 3;
    } else if ((b0 & 0xF8U) == 0xF0U) {
      end = *non_ascii + 4;
    }
    return LexResult{
        false, {}, LexDiagnostic{"LEX002", SourceSpan{*non_ascii, end}, "non-ASCII character outside the permitted token/whitespace set"}};
  }

  Scanner scanner{source, 0, {}};

  while (!scanner.at_end()) {
    const char c = scanner.current();

    if (is_whitespace(c)) {
      scanner.pos += 1;
      continue;
    }

    const std::size_t start = scanner.pos;

    if (is_identifier_start(c)) {
      std::size_t j = start;
      while (j < source.size() && is_identifier_continue(source[j])) {
        j += 1;
      }
      const std::string_view text = source.substr(start, j - start);
      const auto &keywords = keyword_table();
      const auto it = keywords.find(text);
      const TokenKind kind = (it != keywords.end()) ? it->second : TokenKind::Identifier;
      scanner.push(kind, SourceSpan{start, j});
      scanner.pos = j;
      continue;
    }

    // "0x" is the only grammar.ebnf terminal beginning with "0" (line 78,
    // used only inside i32/i64/f64-bit-literal). Any other letter after "0"
    // (e.g. "0X" uppercase, "0z") has no textual basis as a bit-literal
    // near-miss: it is an ordinary DecimalNumeral("0") followed by its own
    // Identifier token, exactly like "42x" lexes as DecimalNumeral(42) then
    // Identifier(x). Only the exact lowercase "0x" prefix is special-cased
    // here; everything else falls through to the plain decimal-numeral path
    // below.
    if (c == '0' && scanner.peek() == 'x') {
      std::size_t j = start + 2;
      while (j < source.size() && is_hex_digit(source[j])) {
        j += 1;
      }
      const std::size_t digit_count = j - (start + 2);
      if (digit_count != 8 && digit_count != 16) {
        return scanner.fail("LEX004", SourceSpan{start, j},
                             "bit literal hex digits must be exactly 8 (i32) or 16 (i64/f64) hex digits");
      }
      std::uint64_t value = 0;
      for (std::size_t k = start + 2; k < j; ++k) {
        value = (value << 4) | hex_value(source[k]);
      }
      scanner.push(TokenKind::HexNumeral, SourceSpan{start, j}, value, static_cast<int>(digit_count));
      scanner.pos = j;
      continue;
    }

    if (is_ascii_digit(c)) {
      std::size_t j = start;
      while (j < source.size() && is_ascii_digit(source[j])) {
        j += 1;
      }
      if (j < source.size() && source[j] == '.') {
        std::size_t k = j + 1;
        while (k < source.size() && is_ascii_digit(source[k])) {
          k += 1;
        }
        return scanner.fail("LEX007", SourceSpan{start, k}, "decimal floating literals are unsupported in language 0.1.0");
      }
      const std::string_view digit_text = source.substr(start, j - start);
      const bool leading_zero_violation = digit_text.size() > 1 && digit_text.front() == '0';
      std::uint64_t value = 0;
      bool overflow = false;
      for (const char d : digit_text) {
        if (!overflow) {
          value = (value * 10) + static_cast<unsigned>(d - '0');
          if (value > kMaxCapacityValue) {
            overflow = true;
          }
        }
      }
      if (leading_zero_violation || overflow) {
        return scanner.fail("LEX006", SourceSpan{start, j},
                             leading_zero_violation ? "decimal numeral has a forbidden leading zero"
                                                     : "decimal numeral exceeds the admissible natural range [0, 2^31-1]");
      }
      scanner.push(TokenKind::DecimalNumeral, SourceSpan{start, j}, value);
      scanner.pos = j;
      continue;
    }

    if (c == '.') {
      if (scanner.peek().has_value() && is_ascii_digit(*scanner.peek())) {
        std::size_t k = start + 1;
        while (k < source.size() && is_ascii_digit(source[k])) {
          k += 1;
        }
        return scanner.fail("LEX007", SourceSpan{start, k}, "decimal floating literals are unsupported in language 0.1.0");
      }
      return scanner.fail("LEX002", SourceSpan{start, start + 1}, "'.' is outside the permitted ASCII token/whitespace set");
    }

    switch (c) {
    case '(':
      scanner.push(TokenKind::LParen, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case ')':
      scanner.push(TokenKind::RParen, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '[':
      scanner.push(TokenKind::LBracket, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case ']':
      scanner.push(TokenKind::RBracket, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case ',':
      scanner.push(TokenKind::Comma, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case ':':
      scanner.push(TokenKind::Colon, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case ';':
      scanner.push(TokenKind::Semicolon, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '+':
      scanner.push(TokenKind::Plus, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '-':
      scanner.push(TokenKind::Minus, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '*':
      scanner.push(TokenKind::Star, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '%':
      scanner.push(TokenKind::Percent, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    case '=':
      if (scanner.peek() == '=') {
        scanner.push(TokenKind::EqualEqual, SourceSpan{start, start + 2});
        scanner.pos += 2;
      } else {
        scanner.push(TokenKind::Equal, SourceSpan{start, start + 1});
        scanner.pos += 1;
      }
      continue;
    case '!':
      if (scanner.peek() == '=') {
        scanner.push(TokenKind::BangEqual, SourceSpan{start, start + 2});
        scanner.pos += 2;
      } else {
        scanner.push(TokenKind::Bang, SourceSpan{start, start + 1});
        scanner.pos += 1;
      }
      continue;
    case '<':
      if (scanner.peek() == '=') {
        scanner.push(TokenKind::LessEqual, SourceSpan{start, start + 2});
        scanner.pos += 2;
      } else {
        scanner.push(TokenKind::Less, SourceSpan{start, start + 1});
        scanner.pos += 1;
      }
      continue;
    case '>':
      if (scanner.peek() == '=') {
        scanner.push(TokenKind::GreaterEqual, SourceSpan{start, start + 2});
        scanner.pos += 2;
      } else {
        scanner.push(TokenKind::Greater, SourceSpan{start, start + 1});
        scanner.pos += 1;
      }
      continue;
    case '&':
      if (scanner.peek() == '&') {
        scanner.push(TokenKind::AmpAmp, SourceSpan{start, start + 2});
        scanner.pos += 2;
        continue;
      }
      return scanner.fail("LEX003", SourceSpan{start, start + 1}, "'&' is only valid as '&&'");
    case '|':
      if (scanner.peek() == '|') {
        scanner.push(TokenKind::PipePipe, SourceSpan{start, start + 2});
        scanner.pos += 2;
        continue;
      }
      return scanner.fail("LEX003", SourceSpan{start, start + 1}, "'|' is only valid as '||'");
    case '/':
      if (scanner.peek() == '/' || scanner.peek() == '*') {
        return scanner.fail("LEX007", SourceSpan{start, start + 2}, "source comments are unsupported in language 0.1.0");
      }
      scanner.push(TokenKind::Slash, SourceSpan{start, start + 1});
      scanner.pos += 1;
      continue;
    default:
      return scanner.fail("LEX002", SourceSpan{start, start + 1}, "character is outside the permitted ASCII token/whitespace set");
    }
  }

  scanner.push(TokenKind::EndOfFile, SourceSpan{source.size(), source.size()});

  LexResult result;
  result.ok = true;
  result.tokens = std::move(scanner.tokens);
  return result;
}

} // namespace boundfin::source::lex
