#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;

ExprPtr parse_body(const std::string &expr_text) {
  const std::string source = "fn f(a: i32, b: i32, c: i32): i32 = " + expr_text + ";\nexport f;\n";
  const auto result = parse_module(source);
  BOUNDFIN_CHECK(result.ok);
  auto body = std::make_unique<Expr>(std::move(*result.module->functions[0].body));
  return body;
}

// grammar.ebnf "Mandatory elaboration": every surface operator maps
// one-for-one to its BinaryPrimitiveOp/UnaryPrimitiveOp tag.
void every_operator_elaborates_to_its_primitive_tag() {
  const std::array<std::pair<std::string, BinaryPrimitiveOp>, 13> binary_cases = {{
      {"a && b", BinaryPrimitiveOp::And}, {"a || b", BinaryPrimitiveOp::Or}, {"a + b", BinaryPrimitiveOp::Add},
      {"a - b", BinaryPrimitiveOp::Sub},  {"a * b", BinaryPrimitiveOp::Mul}, {"a / b", BinaryPrimitiveOp::Div},
      {"a % b", BinaryPrimitiveOp::Rem},  {"a == b", BinaryPrimitiveOp::Eq}, {"a != b", BinaryPrimitiveOp::Ne},
      {"a < b", BinaryPrimitiveOp::Lt},   {"a <= b", BinaryPrimitiveOp::Le}, {"a > b", BinaryPrimitiveOp::Gt},
      {"a >= b", BinaryPrimitiveOp::Ge},
  }};
  for (const auto &[text, op] : binary_cases) {
    const auto expr = parse_body(text);
    BOUNDFIN_CHECK(expr->kind == ExprKind::BinaryPrimitive);
    BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(expr->data).op == op);
  }

  const auto not_expr = parse_body("!(a == b)");
  BOUNDFIN_CHECK(not_expr->kind == ExprKind::UnaryPrimitive);
  BOUNDFIN_CHECK(std::get<UnaryPrimitiveExpr>(not_expr->data).op == UnaryPrimitiveOp::Not);

  const auto neg_expr = parse_body("-a");
  BOUNDFIN_CHECK(neg_expr->kind == ExprKind::UnaryPrimitive);
  BOUNDFIN_CHECK(std::get<UnaryPrimitiveExpr>(neg_expr->data).op == UnaryPrimitiveOp::Neg);
}

// Precedence, highest to lowest: postfix, unary, * / %, + -, relational,
// equality, &&, ||.
void arithmetic_precedence_and_left_associativity() {
  // a + b * c -> Add(a, Mul(b, c))
  const auto expr = parse_body("a + b * c");
  BOUNDFIN_CHECK(expr->kind == ExprKind::BinaryPrimitive);
  const auto &add = std::get<BinaryPrimitiveExpr>(expr->data);
  BOUNDFIN_CHECK(add.op == BinaryPrimitiveOp::Add);
  BOUNDFIN_CHECK(add.lhs->kind == ExprKind::Var);
  BOUNDFIN_CHECK(add.rhs->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(add.rhs->data).op == BinaryPrimitiveOp::Mul);

  // a - b - c -> Sub(Sub(a, b), c): left-associative.
  const auto sub_expr = parse_body("a - b - c");
  const auto &outer_sub = std::get<BinaryPrimitiveExpr>(sub_expr->data);
  BOUNDFIN_CHECK(outer_sub.op == BinaryPrimitiveOp::Sub);
  BOUNDFIN_CHECK(outer_sub.lhs->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(outer_sub.rhs->kind == ExprKind::Var);
  BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(outer_sub.lhs->data).op == BinaryPrimitiveOp::Sub);
}

void relational_binds_tighter_than_equality_which_binds_tighter_than_and_or() {
  // a == b && c -> And(Eq(a, b), c)
  const auto expr = parse_body("a == b && c");
  BOUNDFIN_CHECK(expr->kind == ExprKind::BinaryPrimitive);
  const auto &top = std::get<BinaryPrimitiveExpr>(expr->data);
  BOUNDFIN_CHECK(top.op == BinaryPrimitiveOp::And);
  BOUNDFIN_CHECK(top.lhs->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(top.lhs->data).op == BinaryPrimitiveOp::Eq);

  // a && b || c -> Or(And(a, b), c): || binds loosest.
  const auto or_expr = parse_body("a && b || c");
  const auto &or_top = std::get<BinaryPrimitiveExpr>(or_expr->data);
  BOUNDFIN_CHECK(or_top.op == BinaryPrimitiveOp::Or);
  BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(or_top.lhs->data).op == BinaryPrimitiveOp::And);
}

void unary_binds_tighter_than_multiplicative() {
  // -a * b -> Mul(Neg(a), b)
  const auto expr = parse_body("-a * b");
  BOUNDFIN_CHECK(expr->kind == ExprKind::BinaryPrimitive);
  const auto &mul = std::get<BinaryPrimitiveExpr>(expr->data);
  BOUNDFIN_CHECK(mul.op == BinaryPrimitiveOp::Mul);
  BOUNDFIN_CHECK(mul.lhs->kind == ExprKind::UnaryPrimitive);
}

void parentheses_override_precedence() {
  // (a + b) * c -> Mul(Add(a,b), c)
  const auto expr = parse_body("(a + b) * c");
  const auto &mul = std::get<BinaryPrimitiveExpr>(expr->data);
  BOUNDFIN_CHECK(mul.op == BinaryPrimitiveOp::Mul);
  BOUNDFIN_CHECK(mul.lhs->kind == ExprKind::BinaryPrimitive);
  BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(mul.lhs->data).op == BinaryPrimitiveOp::Add);
}

void SYN004_chained_equality_and_relational() {
  // "Chained" means two operators from the *same* non-associative tier
  // (relational: < <= > >=; equality: == !=) in immediate succession.
  // grammar.ebnf's bracketed-optional structure (`equality-expression =
  // relational-expression, [equality-operator, relational-expression]`,
  // itself `relational-expression = additive-expression, [relational-operator,
  // additive-expression]`) only allows *one* operator per tier per operand
  // slot, but that is not "chaining" in the prohibited sense when the two
  // operators are from *different* tiers (relational nested inside
  // equality): see mixed_relational_and_equality_tiers_are_not_chaining().
  for (const std::string text : {"a < b < c", "a == b == c", "a <= b <= c", "a > b > c", "a >= b >= c",
                                  "a != b != c"}) {
    const auto source = "fn f(a: i32, b: i32, c: i32): bool = " + text + ";\nexport f;\n";
    const auto result = parse_module(source);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN004"));
  }
  // Parentheses make the chain legal again (grammar.ebnf precedence note).
  {
    const auto result = parse_module("fn f(a: i32, b: i32, c: i32): bool = (a < b) == (b < c);\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
  }
}

// One relational comparison nested inside one equality comparison is not
// "chaining": it is ordinary precedence nesting (equality is a strictly
// lower tier than relational, so an equality operand may itself already be
// a complete relational-expression). Confirmed against the grammar's
// literal bracketed-optional structure, not just asserted.
void mixed_relational_and_equality_tiers_are_not_chaining() {
  {
    // "a < b == c" parses as "(a < b) == c": the left equality operand
    // consumes the relational comparison "a < b" as one complete
    // relational-expression; "==" then starts equality-expression's
    // optional second half.
    const auto result = parse_module("fn f(a: i32, b: i32, c: i32): bool = a < b == c;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &eq = std::get<BinaryPrimitiveExpr>(result.module->functions[0].body->data);
    BOUNDFIN_CHECK(eq.op == BinaryPrimitiveOp::Eq);
    BOUNDFIN_CHECK(eq.lhs->kind == ExprKind::BinaryPrimitive);
    BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(eq.lhs->data).op == BinaryPrimitiveOp::Lt);
    BOUNDFIN_CHECK(eq.rhs->kind == ExprKind::Var);
  }
  {
    // "a == b < c" parses as "a == (b < c)": equality's left operand "a" is
    // a bare relational-expression with its optional relational part
    // absent; "==" starts the optional second half, whose relational
    // operand "b < c" has the relational part present.
    const auto result = parse_module("fn f(a: i32, b: i32, c: i32): bool = a == b < c;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &eq = std::get<BinaryPrimitiveExpr>(result.module->functions[0].body->data);
    BOUNDFIN_CHECK(eq.op == BinaryPrimitiveOp::Eq);
    BOUNDFIN_CHECK(eq.lhs->kind == ExprKind::Var);
    BOUNDFIN_CHECK(eq.rhs->kind == ExprKind::BinaryPrimitive);
    BOUNDFIN_CHECK(std::get<BinaryPrimitiveExpr>(eq.rhs->data).op == BinaryPrimitiveOp::Lt);
  }
}

void boundfin_parse_operators_and_precedence() {
  every_operator_elaborates_to_its_primitive_tag();
  arithmetic_precedence_and_left_associativity();
  relational_binds_tighter_than_equality_which_binds_tighter_than_and_or();
  unary_binds_tighter_than_multiplicative();
  parentheses_override_precedence();
  SYN004_chained_equality_and_relational();
  mixed_relational_and_equality_tiers_are_not_chaining();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_operators_and_precedence)
