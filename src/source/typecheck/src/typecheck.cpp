#include "boundfin/source/typecheck/typecheck.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace boundfin::source::typecheck {

namespace {

using namespace boundfin::source::ast;

struct TypecheckFailure {
  TypeDiagnostic diagnostic;
};

[[noreturn]] void fail(std::string code, SourceSpan span, std::string message) {
  throw TypecheckFailure{TypeDiagnostic{std::move(code), span, std::move(message)}};
}

TypePtr make_type(TypeKind kind, SourceSpan span) {
  auto type = std::make_shared<Type>();
  type->kind = kind;
  type->span = span;
  return type;
}

TypePtr make_array_type(TypePtr element, std::uint32_t capacity, SourceSpan span) {
  auto type = std::make_shared<Type>();
  type->kind = TypeKind::Array;
  type->span = span;
  type->data = ArrayType{std::move(element), capacity};
  return type;
}

TypePtr make_product_type(std::vector<TypePtr> components, SourceSpan span) {
  auto type = std::make_shared<Type>();
  type->kind = TypeKind::Product;
  type->span = span;
  type->data = ProductType{std::move(components)};
  return type;
}

bool types_equal(const Type &a, const Type &b) {
  if (a.kind != b.kind) {
    return false;
  }
  bool result = false;
  switch (a.kind) {
  case TypeKind::Bool:
  case TypeKind::I32:
  case TypeKind::I64:
  case TypeKind::F64:
    result = true;
    break;
  case TypeKind::Array: {
    const auto &array_a = std::get<ArrayType>(a.data);
    const auto &array_b = std::get<ArrayType>(b.data);
    result = array_a.capacity == array_b.capacity && types_equal(*array_a.element, *array_b.element);
    break;
  }
  case TypeKind::Product: {
    const auto &product_a = std::get<ProductType>(a.data);
    const auto &product_b = std::get<ProductType>(b.data);
    result = product_a.components.size() == product_b.components.size();
    for (std::size_t i = 0; result && i < product_a.components.size(); ++i) {
      result = types_equal(*product_a.components[i], *product_b.components[i]);
    }
    break;
  }
  }
  return result;
}

std::string describe_type(const Type &type) {
  std::string result;
  switch (type.kind) {
  case TypeKind::Bool:
    result = "bool";
    break;
  case TypeKind::I32:
    result = "i32";
    break;
  case TypeKind::I64:
    result = "i64";
    break;
  case TypeKind::F64:
    result = "f64";
    break;
  case TypeKind::Array: {
    const auto &array_type = std::get<ArrayType>(type.data);
    result = "arr<" + describe_type(*array_type.element) + ", " + std::to_string(array_type.capacity) + ">";
    break;
  }
  case TypeKind::Product: {
    const auto &product_type = std::get<ProductType>(type.data);
    result = "prod<";
    for (std::size_t i = 0; i < product_type.components.size(); ++i) {
      if (i != 0) {
        result += ", ";
      }
      result += describe_type(*product_type.components[i]);
    }
    result += ">";
    break;
  }
  }
  return result;
}

// Table 5 arithmetic primitives (add/sub/mul/div_w, fadd/fsub/fmul/fdiv).
// `kind` must be I32, I64, or F64: the caller has already confirmed both
// operands share this kind.
MonomorphicPrimitive arith_primitive(BinaryPrimitiveOp op, TypeKind kind) {
  if (op == BinaryPrimitiveOp::Add) {
    if (kind == TypeKind::I32) return MonomorphicPrimitive::Add32;
    if (kind == TypeKind::I64) return MonomorphicPrimitive::Add64;
    return MonomorphicPrimitive::FAdd;
  }
  if (op == BinaryPrimitiveOp::Sub) {
    if (kind == TypeKind::I32) return MonomorphicPrimitive::Sub32;
    if (kind == TypeKind::I64) return MonomorphicPrimitive::Sub64;
    return MonomorphicPrimitive::FSub;
  }
  if (op == BinaryPrimitiveOp::Mul) {
    if (kind == TypeKind::I32) return MonomorphicPrimitive::Mul32;
    if (kind == TypeKind::I64) return MonomorphicPrimitive::Mul64;
    return MonomorphicPrimitive::FMul;
  }
  // op == Div
  if (kind == TypeKind::I32) return MonomorphicPrimitive::DivS32;
  if (kind == TypeKind::I64) return MonomorphicPrimitive::DivS64;
  return MonomorphicPrimitive::FDiv;
}

// Table 5 equality primitives (eq/ne on Boolean, eq/ne_w, feq/fne). `kind`
// must be Bool, I32, I64, or F64: the caller has already confirmed both
// operands share this kind.
MonomorphicPrimitive eq_primitive(BinaryPrimitiveOp op, TypeKind kind) {
  const bool is_eq = op == BinaryPrimitiveOp::Eq;
  if (kind == TypeKind::Bool) return is_eq ? MonomorphicPrimitive::BoolEq : MonomorphicPrimitive::BoolNe;
  if (kind == TypeKind::I32) return is_eq ? MonomorphicPrimitive::Eq32 : MonomorphicPrimitive::Ne32;
  if (kind == TypeKind::I64) return is_eq ? MonomorphicPrimitive::Eq64 : MonomorphicPrimitive::Ne64;
  return is_eq ? MonomorphicPrimitive::FEq : MonomorphicPrimitive::FNe;
}

// Table 5 ordering primitives (lt/le/gt/ge_s_w, flt/fle/fgt/fge). `kind`
// must be I32, I64, or F64: the caller has already confirmed both operands
// share this kind and neither is bool (TYP012 "Boolean ordering").
MonomorphicPrimitive cmp_primitive(BinaryPrimitiveOp op, TypeKind kind) {
  if (kind == TypeKind::I32) {
    switch (op) {
    case BinaryPrimitiveOp::Lt: return MonomorphicPrimitive::LtS32;
    case BinaryPrimitiveOp::Le: return MonomorphicPrimitive::LeS32;
    case BinaryPrimitiveOp::Gt: return MonomorphicPrimitive::GtS32;
    default: return MonomorphicPrimitive::GeS32;
    }
  }
  if (kind == TypeKind::I64) {
    switch (op) {
    case BinaryPrimitiveOp::Lt: return MonomorphicPrimitive::LtS64;
    case BinaryPrimitiveOp::Le: return MonomorphicPrimitive::LeS64;
    case BinaryPrimitiveOp::Gt: return MonomorphicPrimitive::GtS64;
    default: return MonomorphicPrimitive::GeS64;
    }
  }
  switch (op) {
  case BinaryPrimitiveOp::Lt: return MonomorphicPrimitive::FLt;
  case BinaryPrimitiveOp::Le: return MonomorphicPrimitive::FLe;
  case BinaryPrimitiveOp::Gt: return MonomorphicPrimitive::FGt;
  default: return MonomorphicPrimitive::FGe;
  }
}

class Typechecker {
public:
  explicit Typechecker(Module &module) : module_(module) {}

  void run() {
    for (auto &decl : module_.functions) {
      typecheck_function(decl);
    }
  }

private:
  Module &module_;
  std::unordered_map<BindingId, TypePtr> env_;

  TypePtr lookup_binding(BindingId id, SourceSpan span) {
    const auto it = env_.find(id);
    if (it == env_.end()) {
      fail("INT001", span, "internal: no recorded type for binding " + std::to_string(id));
    }
    return it->second;
  }

  // AM-021: a function's body must synthesize exactly its declared result
  // type; Table 7 scopes "every source expression form," not declarations,
  // so this check (and TYP003 for it) sits here rather than in any single
  // T-rule.
  void typecheck_function(FunctionDecl &decl) {
    for (auto &param : decl.parameters) {
      env_[*param.binding] = param.type;
    }
    const TypePtr body_type = typecheck_expr(*decl.body);
    if (!types_equal(*body_type, *decl.result_type)) {
      fail("TYP003", decl.body->span,
           "function \"" + decl.name + "\" body has type " + describe_type(*body_type) +
               ", but its declared result type is " + describe_type(*decl.result_type));
    }
    for (auto &param : decl.parameters) {
      env_.erase(*param.binding);
    }
  }

  TypePtr typecheck_expr(Expr &expr) {
    TypePtr result;
    switch (expr.kind) {
    case ExprKind::Literal:
      result = typecheck_literal(std::get<LiteralExpr>(expr.data), expr.span);
      break;
    case ExprKind::Var: {
      auto &var_expr = std::get<VarExpr>(expr.data);
      result = lookup_binding(*var_expr.resolved_binding, expr.span);
      break;
    }
    case ExprKind::Let: {
      auto &let_expr = std::get<LetExpr>(expr.data);
      const TypePtr bound_type = typecheck_expr(*let_expr.bound);
      env_[*let_expr.binding] = bound_type;
      result = typecheck_expr(*let_expr.body);
      env_.erase(*let_expr.binding);
      break;
    }
    case ExprKind::If:
      result = typecheck_if(std::get<IfExpr>(expr.data), expr.span);
      break;
    case ExprKind::UnaryPrimitive: {
      auto &unary_expr = std::get<UnaryPrimitiveExpr>(expr.data);
      const TypePtr operand_type = typecheck_expr(*unary_expr.operand);
      result = typecheck_unary_primitive(unary_expr, *operand_type, expr.span);
      break;
    }
    case ExprKind::BinaryPrimitive: {
      auto &binary_expr = std::get<BinaryPrimitiveExpr>(expr.data);
      const TypePtr lhs_type = typecheck_expr(*binary_expr.lhs);
      const TypePtr rhs_type = typecheck_expr(*binary_expr.rhs);
      result = typecheck_binary_primitive(binary_expr, *lhs_type, *rhs_type, expr.span);
      break;
    }
    case ExprKind::Call:
      result = typecheck_call(std::get<CallExpr>(expr.data), expr.span);
      break;
    case ExprKind::ArrayLiteral:
      result = typecheck_array_literal(std::get<ArrayLiteralExpr>(expr.data), expr.span);
      break;
    case ExprKind::Product: {
      auto &product_expr = std::get<ProductExpr>(expr.data);
      std::vector<TypePtr> component_types;
      component_types.reserve(product_expr.components.size());
      for (auto &component : product_expr.components) {
        component_types.push_back(typecheck_expr(*component));
      }
      result = make_product_type(std::move(component_types), expr.span);
      break;
    }
    case ExprKind::Proj:
      result = typecheck_proj(std::get<ProjExpr>(expr.data), expr.span);
      break;
    case ExprKind::Len: {
      auto &len_expr = std::get<LenExpr>(expr.data);
      const TypePtr array_type = typecheck_expr(*len_expr.array);
      if (array_type->kind != TypeKind::Array) {
        fail("TYP009", len_expr.array->span, "len applied to non-array type " + describe_type(*array_type));
      }
      result = make_type(TypeKind::I32, expr.span);
      break;
    }
    case ExprKind::Index:
      result = typecheck_index(std::get<IndexExpr>(expr.data), expr.span);
      break;
    case ExprKind::Fold:
      result = typecheck_fold(std::get<FoldExpr>(expr.data));
      break;
    case ExprKind::Build:
      result = typecheck_build(std::get<BuildExpr>(expr.data), expr.span);
      break;
    }
    expr.inferred_type = result;
    return result;
  }

  TypePtr typecheck_literal(const LiteralExpr &literal, SourceSpan span) {
    switch (literal.kind) {
    case LiteralKind::Bool:
      return make_type(TypeKind::Bool, span);
    case LiteralKind::I32Bits:
      return make_type(TypeKind::I32, span);
    case LiteralKind::I64Bits:
      return make_type(TypeKind::I64, span);
    case LiteralKind::F64Bits:
      return make_type(TypeKind::F64, span);
    }
    fail("INT001", span, "internal: unreachable literal kind");
  }

  TypePtr typecheck_if(IfExpr &if_expr, SourceSpan span) {
    const TypePtr cond_type = typecheck_expr(*if_expr.condition);
    if (cond_type->kind != TypeKind::Bool) {
      fail("TYP004", if_expr.condition->span, "conditional guard has type " + describe_type(*cond_type) +
                                                   ", expected bool");
    }
    const TypePtr then_type = typecheck_expr(*if_expr.then_branch);
    const TypePtr else_type = typecheck_expr(*if_expr.else_branch);
    if (!types_equal(*then_type, *else_type)) {
      fail("TYP005", span, "conditional branches have differing types " + describe_type(*then_type) + " and " +
                                describe_type(*else_type));
    }
    return then_type;
  }

  TypePtr typecheck_unary_primitive(UnaryPrimitiveExpr &unary_expr, const Type &operand_type, SourceSpan span) {
    switch (unary_expr.op) {
    case UnaryPrimitiveOp::Not:
      if (operand_type.kind != TypeKind::Bool) {
        fail("TYP001", span, "\"!\" requires a bool operand, found " + describe_type(operand_type));
      }
      unary_expr.resolved_primitive = MonomorphicPrimitive::BoolNot;
      return make_type(TypeKind::Bool, span);
    case UnaryPrimitiveOp::Neg:
      switch (operand_type.kind) {
      case TypeKind::I32:
        unary_expr.resolved_primitive = MonomorphicPrimitive::Neg32;
        return make_type(TypeKind::I32, span);
      case TypeKind::I64:
        unary_expr.resolved_primitive = MonomorphicPrimitive::Neg64;
        return make_type(TypeKind::I64, span);
      case TypeKind::F64:
        unary_expr.resolved_primitive = MonomorphicPrimitive::FNeg;
        return make_type(TypeKind::F64, span);
      case TypeKind::Bool:
      case TypeKind::Array:
      case TypeKind::Product:
        fail("TYP001", span, "unary \"-\" requires an i32/i64/f64 operand, found " + describe_type(operand_type));
      }
      fail("INT001", span, "internal: unreachable operand kind");
    case UnaryPrimitiveOp::Abs:
      // Table 5 lists only `fabs: f64 -> f64`; there is no integer abs.
      if (operand_type.kind != TypeKind::F64) {
        fail("TYP001", span, "\"abs\" requires an f64 operand, found " + describe_type(operand_type));
      }
      unary_expr.resolved_primitive = MonomorphicPrimitive::FAbs;
      return make_type(TypeKind::F64, span);
    }
    fail("INT001", span, "internal: unreachable unary primitive op");
  }

  TypePtr typecheck_binary_primitive(BinaryPrimitiveExpr &binary_expr, const Type &lhs_type, const Type &rhs_type,
                                      SourceSpan span) {
    switch (binary_expr.op) {
    case BinaryPrimitiveOp::And:
    case BinaryPrimitiveOp::Or: {
      if (lhs_type.kind != TypeKind::Bool || rhs_type.kind != TypeKind::Bool) {
        fail("TYP001", span,
             "\"&&\"/\"||\" requires bool operands, found " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type));
      }
      binary_expr.resolved_primitive =
          binary_expr.op == BinaryPrimitiveOp::And ? MonomorphicPrimitive::BoolAnd : MonomorphicPrimitive::BoolOr;
      return make_type(TypeKind::Bool, span);
    }
    case BinaryPrimitiveOp::Add:
    case BinaryPrimitiveOp::Sub:
    case BinaryPrimitiveOp::Mul:
    case BinaryPrimitiveOp::Div: {
      const bool lhs_numeric = is_numeric(lhs_type.kind);
      const bool rhs_numeric = is_numeric(rhs_type.kind);
      if (!lhs_numeric || !rhs_numeric) {
        fail("TYP001", span,
             "arithmetic operator has no signature for operand types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type));
      }
      if (lhs_type.kind != rhs_type.kind) {
        fail("TYP012", span,
             "arithmetic operands have differing types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type) + "; implicit conversion is not supported");
      }
      binary_expr.resolved_primitive = arith_primitive(binary_expr.op, lhs_type.kind);
      return make_type(lhs_type.kind, span);
    }
    case BinaryPrimitiveOp::Rem: {
      if (lhs_type.kind == TypeKind::F64 || rhs_type.kind == TypeKind::F64) {
        // TYP012 explicitly names "floating remainder": Table 5 has no
        // frem signature.
        fail("TYP012", span, "\"%\" does not accept f64 operands (floating remainder has no Table 5 primitive)");
      }
      const bool lhs_int = lhs_type.kind == TypeKind::I32 || lhs_type.kind == TypeKind::I64;
      const bool rhs_int = rhs_type.kind == TypeKind::I32 || rhs_type.kind == TypeKind::I64;
      if (!lhs_int || !rhs_int) {
        fail("TYP001", span,
             "\"%\" has no signature for operand types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type));
      }
      if (lhs_type.kind != rhs_type.kind) {
        fail("TYP012", span,
             "\"%\" operands have differing widths " + describe_type(lhs_type) + " and " + describe_type(rhs_type) +
                 "; implicit conversion is not supported");
      }
      binary_expr.resolved_primitive =
          lhs_type.kind == TypeKind::I32 ? MonomorphicPrimitive::RemS32 : MonomorphicPrimitive::RemS64;
      return make_type(lhs_type.kind, span);
    }
    case BinaryPrimitiveOp::Eq:
    case BinaryPrimitiveOp::Ne: {
      const bool lhs_scalar = is_scalar(lhs_type.kind);
      const bool rhs_scalar = is_scalar(rhs_type.kind);
      if (!lhs_scalar || !rhs_scalar) {
        fail("TYP001", span,
             "\"==\"/\"!=\" has no signature for operand types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type));
      }
      if (lhs_type.kind != rhs_type.kind) {
        fail("TYP012", span,
             "\"==\"/\"!=\" operands have differing types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type) + "; implicit conversion is not supported");
      }
      binary_expr.resolved_primitive = eq_primitive(binary_expr.op, lhs_type.kind);
      return make_type(TypeKind::Bool, span);
    }
    case BinaryPrimitiveOp::Lt:
    case BinaryPrimitiveOp::Le:
    case BinaryPrimitiveOp::Gt:
    case BinaryPrimitiveOp::Ge: {
      const bool lhs_numeric = is_numeric(lhs_type.kind);
      const bool rhs_numeric = is_numeric(rhs_type.kind);
      if (!lhs_numeric || !rhs_numeric) {
        // TYP012 explicitly names "Boolean ordering": Table 5 has no
        // lt/le/gt/ge signature for bool. Any other non-numeric operand
        // (array/product) has no signature at all (TYP001).
        if (lhs_type.kind == TypeKind::Bool || rhs_type.kind == TypeKind::Bool) {
          fail("TYP012", span, "relational/ordering operators do not accept bool operands (Boolean ordering)");
        }
        fail("TYP001", span,
             "relational/ordering operator has no signature for operand types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type));
      }
      if (lhs_type.kind != rhs_type.kind) {
        fail("TYP012", span,
             "relational/ordering operands have differing types " + describe_type(lhs_type) + " and " +
                 describe_type(rhs_type) + "; implicit conversion is not supported");
      }
      binary_expr.resolved_primitive = cmp_primitive(binary_expr.op, lhs_type.kind);
      return make_type(TypeKind::Bool, span);
    }
    }
    fail("INT001", span, "internal: unreachable binary primitive op");
  }

  static bool is_numeric(TypeKind kind) { return kind == TypeKind::I32 || kind == TypeKind::I64 || kind == TypeKind::F64; }
  static bool is_scalar(TypeKind kind) { return kind == TypeKind::Bool || is_numeric(kind); }

  TypePtr typecheck_call(CallExpr &call_expr, SourceSpan span) {
    const auto &callee = module_.functions[*call_expr.resolved_callee_rank];
    if (call_expr.arguments.size() != callee.parameters.size()) {
      fail("TYP002", span,
           "call to \"" + call_expr.callee + "\" passes " + std::to_string(call_expr.arguments.size()) +
               " argument(s), expected " + std::to_string(callee.parameters.size()));
    }
    for (std::size_t i = 0; i < call_expr.arguments.size(); ++i) {
      const TypePtr arg_type = typecheck_expr(*call_expr.arguments[i]);
      if (!types_equal(*arg_type, *callee.parameters[i].type)) {
        fail("TYP002", call_expr.arguments[i]->span,
             "argument " + std::to_string(i + 1) + " to \"" + call_expr.callee + "\" has type " +
                 describe_type(*arg_type) + ", expected " + describe_type(*callee.parameters[i].type));
      }
    }
    return callee.result_type;
  }

  TypePtr typecheck_array_literal(ArrayLiteralExpr &array_expr, SourceSpan span) {
    if (array_expr.elements.empty()) {
      // AM-020: T-Array synthesizes tau from "every e_i : tau"; with zero
      // elements there is nothing to synthesize tau from. Reuses TYP008
      // rather than minting a new code.
      fail("TYP008", span, "empty array literal has no determinable element type");
    }
    const TypePtr element_type = typecheck_expr(*array_expr.elements[0]);
    for (std::size_t i = 1; i < array_expr.elements.size(); ++i) {
      const TypePtr this_type = typecheck_expr(*array_expr.elements[i]);
      if (!types_equal(*element_type, *this_type)) {
        fail("TYP008", array_expr.elements[i]->span,
             "array literal element " + std::to_string(i + 1) + " has type " + describe_type(*this_type) +
                 ", expected " + describe_type(*element_type));
      }
    }
    // AM-018: m <= N (this literal's own declared capacity) is deliberately
    // not checked here; it is src/source/size's SIZ002 (Phase 3.3+).
    return make_array_type(element_type, array_expr.capacity, span);
  }

  TypePtr typecheck_proj(ProjExpr &proj_expr, SourceSpan span) {
    const TypePtr operand_type = typecheck_expr(*proj_expr.operand);
    if (operand_type->kind != TypeKind::Product) {
      fail("TYP006", proj_expr.operand->span,
           "projection operand has type " + describe_type(*operand_type) + ", expected a product");
    }
    const auto &product_type = std::get<ProductType>(operand_type->data);
    if (proj_expr.index < 1 || proj_expr.index > product_type.components.size()) {
      fail("TYP007", span,
           "projection index " + std::to_string(proj_expr.index) + " is outside 1.." +
               std::to_string(product_type.components.size()));
    }
    return product_type.components[proj_expr.index - 1];
  }

  TypePtr typecheck_index(IndexExpr &index_expr, SourceSpan span) {
    const TypePtr array_type = typecheck_expr(*index_expr.array);
    if (array_type->kind != TypeKind::Array) {
      fail("TYP009", index_expr.array->span, "index operand has non-array type " + describe_type(*array_type));
    }
    const TypePtr idx_type = typecheck_expr(*index_expr.index);
    if (idx_type->kind != TypeKind::I32) {
      fail("TYP009", index_expr.index->span, "array index has type " + describe_type(*idx_type) + ", expected i32");
    }
    (void)span;
    return std::get<ArrayType>(array_type->data).element;
  }

  // Table 7 T-Fold: "admissible count for N; initial/body: tau_x under
  // i:idx(N), x:tau_x" -> "fold: tau_x". `count` is deliberately not
  // visited: per main.pdf Sec. 4.2, it is typed and bounded by the separate
  // auxiliary judgment `|-cnt`, owned by src/source/size (Phase 3.3+), not
  // by this module's `|- e : tau` judgment. See this module's header
  // comment and AM-018.
  TypePtr typecheck_fold(FoldExpr &fold_expr) {
    const TypePtr initial_type = typecheck_expr(*fold_expr.initial);
    env_[*fold_expr.accumulator_binding] = initial_type;
    env_[*fold_expr.index_binding] = make_type(TypeKind::I32, fold_expr.index_name_span);
    const TypePtr body_type = typecheck_expr(*fold_expr.body);
    env_.erase(*fold_expr.accumulator_binding);
    env_.erase(*fold_expr.index_binding);
    if (!types_equal(*initial_type, *body_type)) {
      fail("TYP010", fold_expr.body->span,
           "fold body has type " + describe_type(*body_type) + ", but the accumulator/initial type is " +
               describe_type(*initial_type));
    }
    return initial_type;
  }

  // Table 7 T-Build: "admissible count for N; body: tau under i:idx(N)" ->
  // "builder: arr<tau, N>". `count` is deliberately not visited; see
  // typecheck_fold above. TYP011 ("Builder body type does not match result
  // element type") is not raised here: T-Build defines the builder's
  // element type to *be* the body's synthesized type, with no independent
  // expected-element-type premise for it to mismatch against -- recorded
  // in docs/traceability.md for this milestone, parallel to SYN003's
  // already-accepted unreachability (AM-019's precedent).
  TypePtr typecheck_build(BuildExpr &build_expr, SourceSpan span) {
    env_[*build_expr.index_binding] = make_type(TypeKind::I32, build_expr.index_name_span);
    const TypePtr body_type = typecheck_expr(*build_expr.body);
    env_.erase(*build_expr.index_binding);
    return make_array_type(body_type, build_expr.capacity, span);
  }
};

} // namespace

TypecheckResult typecheck_module(ast::Module &module) {
  try {
    Typechecker typechecker(module);
    typechecker.run();
    return TypecheckResult{true, std::nullopt};
  } catch (const TypecheckFailure &failure) {
    return TypecheckResult{false, failure.diagnostic};
  }
}

} // namespace boundfin::source::typecheck
