#include "boundfin/source/parse/parser.hpp"

#include "boundfin/source/lex/lexer.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace boundfin::source::parse {

using boundfin::source::ast::ArrayLiteralExpr;
using boundfin::source::ast::ArrayType;
using boundfin::source::ast::BinaryPrimitiveExpr;
using boundfin::source::ast::BinaryPrimitiveOp;
using boundfin::source::ast::BuildExpr;
using boundfin::source::ast::CallExpr;
using boundfin::source::ast::Expr;
using boundfin::source::ast::ExprKind;
using boundfin::source::ast::ExprPtr;
using boundfin::source::ast::ExportDecl;
using boundfin::source::ast::FoldExpr;
using boundfin::source::ast::FunctionDecl;
using boundfin::source::ast::IfExpr;
using boundfin::source::ast::IndexExpr;
using boundfin::source::ast::LenExpr;
using boundfin::source::ast::LetExpr;
using boundfin::source::ast::LiteralExpr;
using boundfin::source::ast::LiteralKind;
using boundfin::source::ast::Module;
using boundfin::source::ast::Parameter;
using boundfin::source::ast::ProductExpr;
using boundfin::source::ast::ProductType;
using boundfin::source::ast::ProjExpr;
using boundfin::source::ast::Type;
using boundfin::source::ast::TypeKind;
using boundfin::source::ast::TypePtr;
using boundfin::source::ast::UnaryPrimitiveExpr;
using boundfin::source::ast::UnaryPrimitiveOp;
using boundfin::source::ast::VarExpr;
using boundfin::source::lex::Token;
using boundfin::source::lex::TokenKind;

namespace {

// Internal control-flow exception, caught once at parse_module()'s top
// level: deep recursive descent is far simpler with exceptions than
// threading std::optional/error codes through every call.
struct ParseFailure {
  ParseDiagnostic diagnostic;
};

[[noreturn]] void fail(std::string code, SourceSpan span, std::string message) {
  throw ParseFailure{ParseDiagnostic{std::move(code), span, std::move(message)}};
}

bool is_keyword(TokenKind kind) {
  switch (kind) {
  case TokenKind::KwFn:
  case TokenKind::KwExport:
  case TokenKind::KwBool:
  case TokenKind::KwI32:
  case TokenKind::KwI64:
  case TokenKind::KwF64:
  case TokenKind::KwArr:
  case TokenKind::KwProd:
  case TokenKind::KwLet:
  case TokenKind::KwIn:
  case TokenKind::KwIf:
  case TokenKind::KwThen:
  case TokenKind::KwElse:
  case TokenKind::KwFold:
  case TokenKind::KwBuild:
  case TokenKind::KwArray:
  case TokenKind::KwLen:
  case TokenKind::KwProj:
  case TokenKind::KwAbs:
  case TokenKind::KwTrue:
  case TokenKind::KwFalse:
  case TokenKind::KwI32Bits:
  case TokenKind::KwI64Bits:
  case TokenKind::KwF64Bits:
    return true;
  default:
    return false;
  }
}

bool is_equality_or_relational(TokenKind kind) {
  switch (kind) {
  case TokenKind::EqualEqual:
  case TokenKind::BangEqual:
  case TokenKind::Less:
  case TokenKind::LessEqual:
  case TokenKind::Greater:
  case TokenKind::GreaterEqual:
    return true;
  default:
    return false;
  }
}

class Parser {
public:
  Parser(std::string_view source, std::vector<Token> tokens) : source_(source), tokens_(std::move(tokens)) {}

  Module parse() {
    Module module;
    const std::size_t start = current().span.start;
    while (check(TokenKind::KwFn)) {
      module.functions.push_back(parse_function_declaration());
    }
    module.export_decl = parse_export_declaration();
    if (!check(TokenKind::EndOfFile)) {
      fail("SYN007", current().span, "no source may follow the module's export declaration");
    }
    module.span = SourceSpan{start, current().span.end};
    return module;
  }

private:
  std::string_view source_;
  std::vector<Token> tokens_;
  std::size_t pos_ = 0;

  [[nodiscard]] const Token &current() const { return tokens_[pos_]; }
  [[nodiscard]] bool check(TokenKind kind) const { return current().kind == kind; }
  [[nodiscard]] std::string text_of(SourceSpan span) const {
    return std::string(source_.substr(span.start, span.length()));
  }

  const Token &advance() {
    const Token &token = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) {
      pos_ += 1;
    }
    return token;
  }

  // Consumes `kind` or raises `code`/`what` naming the missing token. Used
  // for structural punctuation outside the productions SYN005/SYN006 name
  // specifically (those use expect_in(...) below instead).
  const Token &expect(TokenKind kind, const std::string &what) {
    if (!check(kind)) {
      fail("SYN002", current().span, "expected " + what + ", found " + to_string(current().kind));
    }
    return advance();
  }

  // Same as expect(), but raises `code` (SYN005 inside function
  // declarations/parameters/result types, SYN006 inside array
  // literals/folds/builders) instead of the generic SYN002.
  const Token &expect_in(TokenKind kind, const std::string &what, const std::string &code) {
    if (!check(kind)) {
      fail(code, current().span, "expected " + what + ", found " + to_string(current().kind));
    }
    return advance();
  }

  // An `identifier` production match. Raises LEX005 if the current token is
  // a reserved word (this is where LEX005 is implemented: it needs this
  // grammatical-position context, which boundfin::source::lex does not
  // have), or `code` (SYN002/SYN005/SYN006 depending on caller) if it is
  // neither an identifier nor a keyword.
  std::pair<std::string, SourceSpan> expect_identifier(const std::string &code) {
    if (check(TokenKind::Identifier)) {
      const Token &token = advance();
      return {text_of(token.span), token.span};
    }
    if (is_keyword(current().kind)) {
      fail("LEX005", current().span, "reserved word \"" + to_string(current().kind) + "\" used where an identifier is required");
    }
    fail(code, current().span, "expected an identifier, found " + to_string(current().kind));
  }

  // --- Declarations and module ---------------------------------------------

  FunctionDecl parse_function_declaration() {
    const std::size_t start = current().span.start;
    expect_in(TokenKind::KwFn, "\"fn\"", "SYN005");
    auto [name, name_span] = expect_identifier("SYN005");

    expect_in(TokenKind::LParen, "\"(\"", "SYN005");
    std::vector<Parameter> parameters;
    if (!check(TokenKind::RParen)) {
      parameters.push_back(parse_parameter());
      while (check(TokenKind::Comma)) {
        advance();
        parameters.push_back(parse_parameter());
      }
    }
    expect_in(TokenKind::RParen, "\")\"", "SYN005");

    expect_in(TokenKind::Colon, "\":\"", "SYN005");
    TypePtr result_type = parse_type();
    expect_in(TokenKind::Equal, "\"=\"", "SYN005");
    ExprPtr body = parse_expression();
    expect_in(TokenKind::Semicolon, "\";\"", "SYN005");

    FunctionDecl decl;
    decl.name = std::move(name);
    decl.name_span = name_span;
    decl.parameters = std::move(parameters);
    decl.result_type = std::move(result_type);
    decl.body = std::move(body);
    decl.span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    return decl;
  }

  Parameter parse_parameter() {
    const std::size_t start = current().span.start;
    auto [name, name_span] = expect_identifier("SYN005");
    expect_in(TokenKind::Colon, "\":\"", "SYN005");
    TypePtr type = parse_type();
    Parameter parameter;
    parameter.name = std::move(name);
    parameter.name_span = name_span;
    parameter.type = std::move(type);
    parameter.span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    return parameter;
  }

  ExportDecl parse_export_declaration() {
    const std::size_t start = current().span.start;
    expect_in(TokenKind::KwExport, "\"export\"", "SYN007");
    auto [name, name_span] = expect_identifier("SYN007");
    expect_in(TokenKind::Semicolon, "\";\"", "SYN007");
    return ExportDecl{std::move(name), name_span, SourceSpan{start, tokens_[pos_ - 1].span.end}};
  }

  // --- Types -----------------------------------------------------------

  TypePtr parse_type() {
    const std::size_t start = current().span.start;
    if (check(TokenKind::KwBool)) {
      advance();
      return make_scalar_type(TypeKind::Bool, start);
    }
    if (check(TokenKind::KwI32)) {
      advance();
      return make_scalar_type(TypeKind::I32, start);
    }
    if (check(TokenKind::KwI64)) {
      advance();
      return make_scalar_type(TypeKind::I64, start);
    }
    if (check(TokenKind::KwF64)) {
      advance();
      return make_scalar_type(TypeKind::F64, start);
    }
    if (check(TokenKind::KwArr)) {
      advance();
      expect_in(TokenKind::Less, "\"<\"", "SYN005");
      TypePtr element = parse_type();
      expect_in(TokenKind::Comma, "\",\"", "SYN005");
      const std::uint32_t capacity = parse_capacity("SYN005");
      expect_in(TokenKind::Greater, "\">\"", "SYN005");
      auto type = std::make_unique<Type>();
      type->kind = TypeKind::Array;
      type->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
      type->data = ArrayType{std::move(element), capacity};
      return type;
    }
    if (check(TokenKind::KwProd)) {
      advance();
      expect_in(TokenKind::Less, "\"<\"", "SYN005");
      std::vector<TypePtr> components;
      components.push_back(parse_type());
      expect_in(TokenKind::Comma, "\",\"", "SYN005");
      components.push_back(parse_type());
      while (check(TokenKind::Comma)) {
        advance();
        components.push_back(parse_type());
      }
      expect_in(TokenKind::Greater, "\">\"", "SYN005");
      if (components.size() < 2) {
        fail("SYN003", SourceSpan{start, tokens_[pos_ - 1].span.end}, "product type has arity below two");
      }
      auto type = std::make_unique<Type>();
      type->kind = TypeKind::Product;
      type->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
      type->data = ProductType{std::move(components)};
      return type;
    }
    fail("SYN001", current().span, "expected a type, found " + to_string(current().kind));
  }

  TypePtr make_scalar_type(TypeKind kind, std::size_t start) {
    auto type = std::make_unique<Type>();
    type->kind = kind;
    type->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    return type;
  }

  // --- Capacities and indices -------------------------------------------

  // grammar.ebnf `capacity = decimal`: the lexer already enforces
  // [0, 2^31-1] for every bare decimal numeral (AM-017(a), author-approved);
  // no additional range check is needed here.
  std::uint32_t parse_capacity(const std::string &code) {
    if (!check(TokenKind::DecimalNumeral)) {
      fail(code, current().span, "expected a capacity (decimal literal), found " + to_string(current().kind));
    }
    const Token &token = advance();
    return static_cast<std::uint32_t>(token.numeral_value);
  }

  // grammar.ebnf `positive-decimal = nonzero-digit, { decimal-digit }`: the
  // same lexical numeral shape as `capacity`, but syntactically excluding a
  // standalone "0". The lexer cannot make this distinction (it does not
  // know which nonterminal a numeral will fill); the parser does, here.
  // Whether a nonzero index is also within an operand's actual arity is
  // TYP007, a later (Phase 3) semantic check this parser cannot perform.
  std::uint32_t parse_positive_decimal(const std::string &code) {
    if (!check(TokenKind::DecimalNumeral) || current().numeral_value == 0) {
      fail(code, current().span, "expected a positive decimal literal (nonzero), found " + to_string(current().kind));
    }
    const Token &token = advance();
    return static_cast<std::uint32_t>(token.numeral_value);
  }

  // --- Expressions -------------------------------------------------------

  ExprPtr parse_expression() {
    if (check(TokenKind::KwLet)) {
      return parse_let_expression();
    }
    if (check(TokenKind::KwIf)) {
      return parse_conditional_expression();
    }
    if (check(TokenKind::KwFold)) {
      return parse_fold_expression();
    }
    if (check(TokenKind::KwBuild)) {
      return parse_build_expression();
    }
    return parse_logical_or_expression();
  }

  ExprPtr parse_let_expression() {
    const std::size_t start = current().span.start;
    advance(); // "let"
    auto [name, name_span] = expect_identifier("SYN001");
    expect(TokenKind::Equal, "\"=\"");
    ExprPtr bound = parse_expression();
    expect(TokenKind::KwIn, "\"in\"");
    ExprPtr body = parse_expression();
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Let;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = LetExpr{std::move(name), name_span, std::move(bound), std::move(body)};
    return expr;
  }

  ExprPtr parse_conditional_expression() {
    const std::size_t start = current().span.start;
    advance(); // "if"
    ExprPtr condition = parse_expression();
    expect(TokenKind::KwThen, "\"then\"");
    ExprPtr then_branch = parse_expression();
    expect(TokenKind::KwElse, "\"else\"");
    ExprPtr else_branch = parse_expression();
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::If;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = IfExpr{std::move(condition), std::move(then_branch), std::move(else_branch)};
    return expr;
  }

  ExprPtr parse_fold_expression() {
    const std::size_t start = current().span.start;
    advance(); // "fold"
    expect_in(TokenKind::Less, "\"<\"", "SYN006");
    const std::uint32_t capacity = parse_capacity("SYN006");
    expect_in(TokenKind::Greater, "\">\"", "SYN006");
    expect_in(TokenKind::LParen, "\"(\"", "SYN006");
    ExprPtr count = parse_expression();
    expect_in(TokenKind::Semicolon, "\";\"", "SYN006");
    auto [accumulator_name, accumulator_span] = expect_identifier("SYN006");
    expect_in(TokenKind::Comma, "\",\"", "SYN006");
    auto [index_name, index_span] = expect_identifier("SYN006");
    expect_in(TokenKind::Semicolon, "\";\"", "SYN006");
    ExprPtr initial = parse_expression();
    expect_in(TokenKind::Semicolon, "\";\"", "SYN006");
    ExprPtr body = parse_expression();
    expect_in(TokenKind::RParen, "\")\"", "SYN006");

    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Fold;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = FoldExpr{capacity,           std::move(count),      std::move(accumulator_name),
                           accumulator_span,   std::move(index_name), index_span,
                           std::move(initial), std::move(body)};
    return expr;
  }

  ExprPtr parse_build_expression() {
    const std::size_t start = current().span.start;
    advance(); // "build"
    expect_in(TokenKind::Less, "\"<\"", "SYN006");
    const std::uint32_t capacity = parse_capacity("SYN006");
    expect_in(TokenKind::Greater, "\">\"", "SYN006");
    expect_in(TokenKind::LParen, "\"(\"", "SYN006");
    ExprPtr count = parse_expression();
    expect_in(TokenKind::Semicolon, "\";\"", "SYN006");
    auto [index_name, index_span] = expect_identifier("SYN006");
    expect_in(TokenKind::Semicolon, "\";\"", "SYN006");
    ExprPtr body = parse_expression();
    expect_in(TokenKind::RParen, "\")\"", "SYN006");

    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Build;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = BuildExpr{capacity, std::move(count), std::move(index_name), index_span, std::move(body)};
    return expr;
  }

  ExprPtr parse_logical_or_expression() {
    ExprPtr lhs = parse_logical_and_expression();
    while (check(TokenKind::PipePipe)) {
      const std::size_t start = lhs->span.start;
      advance();
      ExprPtr rhs = parse_logical_and_expression();
      lhs = make_binary(BinaryPrimitiveOp::Or, std::move(lhs), std::move(rhs), start);
    }
    return lhs;
  }

  ExprPtr parse_logical_and_expression() {
    ExprPtr lhs = parse_equality_expression();
    while (check(TokenKind::AmpAmp)) {
      const std::size_t start = lhs->span.start;
      advance();
      ExprPtr rhs = parse_equality_expression();
      lhs = make_binary(BinaryPrimitiveOp::And, std::move(lhs), std::move(rhs), start);
    }
    return lhs;
  }

  ExprPtr parse_equality_expression() {
    ExprPtr lhs = parse_relational_expression();
    if (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) {
      const std::size_t start = lhs->span.start;
      const BinaryPrimitiveOp op = check(TokenKind::EqualEqual) ? BinaryPrimitiveOp::Eq : BinaryPrimitiveOp::Ne;
      advance();
      ExprPtr rhs = parse_relational_expression();
      lhs = make_binary(op, std::move(lhs), std::move(rhs), start);
    }
    reject_chained_comparison();
    return lhs;
  }

  ExprPtr parse_relational_expression() {
    ExprPtr lhs = parse_additive_expression();
    if (is_equality_or_relational(current().kind) && !check(TokenKind::EqualEqual) && !check(TokenKind::BangEqual)) {
      const std::size_t start = lhs->span.start;
      BinaryPrimitiveOp op = BinaryPrimitiveOp::Lt;
      switch (current().kind) {
      case TokenKind::Less:
        op = BinaryPrimitiveOp::Lt;
        break;
      case TokenKind::LessEqual:
        op = BinaryPrimitiveOp::Le;
        break;
      case TokenKind::Greater:
        op = BinaryPrimitiveOp::Gt;
        break;
      case TokenKind::GreaterEqual:
        op = BinaryPrimitiveOp::Ge;
        break;
      default:
        break;
      }
      advance();
      ExprPtr rhs = parse_additive_expression();
      lhs = make_binary(op, std::move(lhs), std::move(rhs), start);
    }
    return lhs;
  }

  // grammar.ebnf precedence note: "Chained comparisons/equalities are
  // syntax errors unless parentheses make a Boolean operand explicit."
  // parse_equality_expression()/parse_relational_expression() each consume
  // at most one comparison (the grammar's `[...]` is optional-not-repeated);
  // if another equality/relational operator immediately follows, that is a
  // chained comparison attempt, SYN004 rather than a generic SYN001.
  void reject_chained_comparison() {
    if (is_equality_or_relational(current().kind)) {
      fail("SYN004", current().span, "chained equality/comparison operators are not associative; use parentheses");
    }
  }

  ExprPtr parse_additive_expression() {
    ExprPtr lhs = parse_multiplicative_expression();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
      const std::size_t start = lhs->span.start;
      const BinaryPrimitiveOp op = check(TokenKind::Plus) ? BinaryPrimitiveOp::Add : BinaryPrimitiveOp::Sub;
      advance();
      ExprPtr rhs = parse_multiplicative_expression();
      lhs = make_binary(op, std::move(lhs), std::move(rhs), start);
    }
    return lhs;
  }

  ExprPtr parse_multiplicative_expression() {
    ExprPtr lhs = parse_unary_expression();
    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
      const std::size_t start = lhs->span.start;
      BinaryPrimitiveOp op = BinaryPrimitiveOp::Mul;
      if (check(TokenKind::Slash)) {
        op = BinaryPrimitiveOp::Div;
      } else if (check(TokenKind::Percent)) {
        op = BinaryPrimitiveOp::Rem;
      }
      advance();
      ExprPtr rhs = parse_unary_expression();
      lhs = make_binary(op, std::move(lhs), std::move(rhs), start);
    }
    return lhs;
  }

  ExprPtr parse_unary_expression() {
    if (check(TokenKind::Bang) || check(TokenKind::Minus)) {
      const std::size_t start = current().span.start;
      const UnaryPrimitiveOp op = check(TokenKind::Bang) ? UnaryPrimitiveOp::Not : UnaryPrimitiveOp::Neg;
      advance();
      ExprPtr operand = parse_unary_expression();
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::UnaryPrimitive;
      expr->span = SourceSpan{start, operand->span.end};
      expr->data = UnaryPrimitiveExpr{op, std::move(operand)};
      return expr;
    }
    return parse_postfix_expression();
  }

  ExprPtr parse_postfix_expression() {
    ExprPtr expr = parse_primary_expression();
    while (check(TokenKind::LBracket)) {
      const std::size_t start = expr->span.start;
      advance();
      ExprPtr index = parse_expression();
      expect(TokenKind::RBracket, "\"]\"");
      auto index_expr = std::make_unique<Expr>();
      index_expr->kind = ExprKind::Index;
      index_expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
      index_expr->data = IndexExpr{std::move(expr), std::move(index)};
      expr = std::move(index_expr);
    }
    return expr;
  }

  ExprPtr parse_primary_expression() {
    switch (current().kind) {
    case TokenKind::KwTrue:
    case TokenKind::KwFalse:
    case TokenKind::HexNumeral:
      return parse_literal();
    case TokenKind::KwI32Bits:
      return parse_bit_literal(LiteralKind::I32Bits, 8);
    case TokenKind::KwI64Bits:
      return parse_bit_literal(LiteralKind::I64Bits, 16);
    case TokenKind::KwF64Bits:
      return parse_bit_literal(LiteralKind::F64Bits, 16);
    case TokenKind::Identifier:
      return parse_name_expression();
    case TokenKind::KwArray:
      return parse_array_literal();
    case TokenKind::LParen:
      return parse_parenthesized_expression();
    case TokenKind::KwLen:
      return parse_length_expression();
    case TokenKind::KwProj:
      return parse_projection_expression();
    case TokenKind::KwAbs:
      return parse_absolute_expression();
    default:
      if (is_keyword(current().kind)) {
        fail("LEX005", current().span,
             "reserved word \"" + to_string(current().kind) + "\" used where an expression is required");
      }
      fail("SYN001", current().span, "expected an expression, found " + to_string(current().kind));
    }
  }

  ExprPtr parse_literal() {
    const std::size_t start = current().span.start;
    if (check(TokenKind::KwTrue) || check(TokenKind::KwFalse)) {
      const bool value = check(TokenKind::KwTrue);
      advance();
      return make_literal(LiteralKind::Bool, value ? 1 : 0, start);
    }
    // HexNumeral: width (8 or 16 hex digits) distinguishes i32bits from
    // i64bits/f64bits textually, but grammar.ebnf requires the matching
    // keyword wrapper ("i32bits(0x" hex8 ")" etc.); the lexer validates
    // only the numeral's own shape (AM-017 context), so the keyword/width
    // correspondence is checked here.
    const Token &numeral = current();
    if (check(TokenKind::HexNumeral)) {
      fail("SYN001", numeral.span, "a bit literal's hex digits must be wrapped in i32bits(0x...)/i64bits(0x...)/f64bits(0x...)");
    }
    fail("SYN001", current().span, "expected a literal, found " + to_string(current().kind));
  }

  ExprPtr parse_bit_literal(LiteralKind literal_kind, int required_width) {
    const std::size_t start = current().span.start;
    advance(); // i32bits / i64bits / f64bits
    expect(TokenKind::LParen, "\"(\"");
    if (!check(TokenKind::HexNumeral)) {
      fail("SYN001", current().span, "expected a hex bit pattern, found " + to_string(current().kind));
    }
    const Token &numeral = advance();
    if (numeral.hex_digit_width != required_width) {
      fail("SYN001", numeral.span,
           std::string("expected a ") + std::to_string(required_width) + "-hex-digit bit pattern here, found " +
               std::to_string(numeral.hex_digit_width) + " digits");
    }
    expect(TokenKind::RParen, "\")\"");
    return make_literal(literal_kind, numeral.numeral_value, start);
  }

  ExprPtr make_literal(LiteralKind kind, std::uint64_t value, std::size_t start) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Literal;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = LiteralExpr{kind, value};
    return expr;
  }

  ExprPtr parse_name_expression() {
    const std::size_t start = current().span.start;
    const Token &identifier = advance();
    const std::string name = text_of(identifier.span);
    if (!check(TokenKind::LParen)) {
      auto expr = std::make_unique<Expr>();
      expr->kind = ExprKind::Var;
      expr->span = identifier.span;
      expr->data = VarExpr{name, identifier.span};
      return expr;
    }
    advance(); // "("
    std::vector<ExprPtr> arguments;
    if (!check(TokenKind::RParen)) {
      arguments.push_back(parse_expression());
      while (check(TokenKind::Comma)) {
        advance();
        arguments.push_back(parse_expression());
      }
    }
    expect(TokenKind::RParen, "\")\"");
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Call;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = CallExpr{name, identifier.span, std::move(arguments)};
    return expr;
  }

  ExprPtr parse_array_literal() {
    const std::size_t start = current().span.start;
    advance(); // "array"
    expect_in(TokenKind::Less, "\"<\"", "SYN006");
    const std::uint32_t capacity = parse_capacity("SYN006");
    expect_in(TokenKind::Greater, "\">\"", "SYN006");
    expect_in(TokenKind::LBracket, "\"[\"", "SYN006");
    std::vector<ExprPtr> elements;
    if (!check(TokenKind::RBracket)) {
      elements.push_back(parse_expression());
      while (check(TokenKind::Comma)) {
        advance();
        elements.push_back(parse_expression());
      }
    }
    expect_in(TokenKind::RBracket, "\"]\"", "SYN006");
    // Whether `elements.size() <= capacity` (grammar.ebnf "Static
    // concrete-syntax constraints" item 6, diagnostics-and-status.md
    // SIZ002) is deliberately not checked here: AM-018 (author-approved)
    // defers this to src/source/size (Phase 3), since docs/architecture.md
    // names src/source/size, not src/source/parse, as owning size/count
    // decisions. An over-length array literal parses successfully into the
    // untyped AST for now.
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::ArrayLiteral;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = ArrayLiteralExpr{capacity, std::move(elements)};
    return expr;
  }

  ExprPtr parse_parenthesized_expression() {
    const std::size_t start = current().span.start;
    advance(); // "("
    ExprPtr first = parse_expression();
    if (!check(TokenKind::Comma)) {
      expect(TokenKind::RParen, "\")\"");
      // Transparent grouping: no AST node of its own (AM-002: "grouping
      // parentheses are not products").
      first->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
      return first;
    }
    std::vector<ExprPtr> components;
    components.push_back(std::move(first));
    while (check(TokenKind::Comma)) {
      advance();
      components.push_back(parse_expression());
    }
    expect(TokenKind::RParen, "\")\"");
    if (components.size() < 2) {
      fail("SYN003", SourceSpan{start, tokens_[pos_ - 1].span.end}, "product expression has arity below two");
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Product;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = ProductExpr{std::move(components)};
    return expr;
  }

  ExprPtr parse_length_expression() {
    const std::size_t start = current().span.start;
    advance(); // "len"
    expect(TokenKind::LParen, "\"(\"");
    ExprPtr array = parse_expression();
    expect(TokenKind::RParen, "\")\"");
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Len;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = LenExpr{std::move(array)};
    return expr;
  }

  ExprPtr parse_projection_expression() {
    const std::size_t start = current().span.start;
    advance(); // "proj"
    expect(TokenKind::Less, "\"<\"");
    const std::uint32_t index = parse_positive_decimal("SYN001");
    expect(TokenKind::Greater, "\">\"");
    expect(TokenKind::LParen, "\"(\"");
    ExprPtr operand = parse_expression();
    expect(TokenKind::RParen, "\")\"");
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::Proj;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = ProjExpr{index, std::move(operand)};
    return expr;
  }

  ExprPtr parse_absolute_expression() {
    const std::size_t start = current().span.start;
    advance(); // "abs"
    expect(TokenKind::LParen, "\"(\"");
    ExprPtr operand = parse_expression();
    expect(TokenKind::RParen, "\")\"");
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::UnaryPrimitive;
    expr->span = SourceSpan{start, tokens_[pos_ - 1].span.end};
    expr->data = UnaryPrimitiveExpr{UnaryPrimitiveOp::Abs, std::move(operand)};
    return expr;
  }

  ExprPtr make_binary(BinaryPrimitiveOp op, ExprPtr lhs, ExprPtr rhs, std::size_t start) {
    auto expr = std::make_unique<Expr>();
    expr->kind = ExprKind::BinaryPrimitive;
    expr->span = SourceSpan{start, rhs->span.end};
    expr->data = BinaryPrimitiveExpr{op, std::move(lhs), std::move(rhs)};
    return expr;
  }
};

} // namespace

ParseResult parse_module(std::string_view source) {
  auto lex_result = boundfin::source::lex::tokenize(source);
  if (!lex_result.ok) {
    ParseResult result;
    result.ok = false;
    result.diagnostic =
        ParseDiagnostic{lex_result.diagnostic->code, lex_result.diagnostic->span, lex_result.diagnostic->message};
    return result;
  }

  try {
    Parser parser(source, std::move(lex_result.tokens));
    Module module = parser.parse();
    ParseResult result;
    result.ok = true;
    result.module = std::move(module);
    return result;
  } catch (const ParseFailure &failure) {
    ParseResult result;
    result.ok = false;
    result.diagnostic = failure.diagnostic;
    return result;
  }
}

} // namespace boundfin::source::parse
