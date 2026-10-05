#include "boundfin/source/ast/serialize.hpp"

namespace boundfin::source::ast {

namespace {

struct Serializer {
  bool include_spans;

  [[nodiscard]] nlohmann::json span_or_omit(const SourceSpan &span) const {
    if (!include_spans) {
      return nullptr;
    }
    return nlohmann::json{{"start", span.start}, {"end", span.end}};
  }

  [[nodiscard]] nlohmann::json operator()(const Type &type) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(type.span);
    }
    switch (type.kind) {
    case TypeKind::Bool:
      out["kind"] = "bool";
      return out;
    case TypeKind::I32:
      out["kind"] = "i32";
      return out;
    case TypeKind::I64:
      out["kind"] = "i64";
      return out;
    case TypeKind::F64:
      out["kind"] = "f64";
      return out;
    case TypeKind::Array: {
      out["kind"] = "array";
      const auto &array_type = std::get<ArrayType>(type.data);
      out["element"] = (*this)(*array_type.element);
      out["capacity"] = array_type.capacity;
      return out;
    }
    case TypeKind::Product: {
      out["kind"] = "product";
      const auto &product_type = std::get<ProductType>(type.data);
      nlohmann::json components = nlohmann::json::array();
      for (const auto &component : product_type.components) {
        components.push_back((*this)(*component));
      }
      out["components"] = std::move(components);
      return out;
    }
    }
    return out;
  }

  [[nodiscard]] static const char *unary_op_name(UnaryPrimitiveOp op) {
    switch (op) {
    case UnaryPrimitiveOp::Not:
      return "not";
    case UnaryPrimitiveOp::Neg:
      return "neg";
    case UnaryPrimitiveOp::Abs:
      return "abs";
    }
    return "unknown";
  }

  [[nodiscard]] static const char *binary_op_name(BinaryPrimitiveOp op) {
    switch (op) {
    case BinaryPrimitiveOp::And:
      return "and";
    case BinaryPrimitiveOp::Or:
      return "or";
    case BinaryPrimitiveOp::Add:
      return "add";
    case BinaryPrimitiveOp::Sub:
      return "sub";
    case BinaryPrimitiveOp::Mul:
      return "mul";
    case BinaryPrimitiveOp::Div:
      return "div";
    case BinaryPrimitiveOp::Rem:
      return "rem";
    case BinaryPrimitiveOp::Eq:
      return "eq";
    case BinaryPrimitiveOp::Ne:
      return "ne";
    case BinaryPrimitiveOp::Lt:
      return "lt";
    case BinaryPrimitiveOp::Le:
      return "le";
    case BinaryPrimitiveOp::Gt:
      return "gt";
    case BinaryPrimitiveOp::Ge:
      return "ge";
    }
    return "unknown";
  }

  [[nodiscard]] static const char *literal_kind_name(LiteralKind kind) {
    switch (kind) {
    case LiteralKind::Bool:
      return "bool";
    case LiteralKind::I32Bits:
      return "i32bits";
    case LiteralKind::I64Bits:
      return "i64bits";
    case LiteralKind::F64Bits:
      return "f64bits";
    }
    return "unknown";
  }

  [[nodiscard]] nlohmann::json operator()(const Expr &expr) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(expr.span);
    }

    switch (expr.kind) {
    case ExprKind::Let: {
      const auto &let_expr = std::get<LetExpr>(expr.data);
      out["kind"] = "let";
      out["name"] = let_expr.name;
      out["bound"] = (*this)(*let_expr.bound);
      out["body"] = (*this)(*let_expr.body);
      return out;
    }
    case ExprKind::If: {
      const auto &if_expr = std::get<IfExpr>(expr.data);
      out["kind"] = "if";
      out["condition"] = (*this)(*if_expr.condition);
      out["then"] = (*this)(*if_expr.then_branch);
      out["else"] = (*this)(*if_expr.else_branch);
      return out;
    }
    case ExprKind::Fold: {
      const auto &fold_expr = std::get<FoldExpr>(expr.data);
      out["kind"] = "fold";
      out["capacity"] = fold_expr.capacity;
      out["count"] = (*this)(*fold_expr.count);
      out["accumulator_name"] = fold_expr.accumulator_name;
      out["index_name"] = fold_expr.index_name;
      out["initial"] = (*this)(*fold_expr.initial);
      out["body"] = (*this)(*fold_expr.body);
      return out;
    }
    case ExprKind::Build: {
      const auto &build_expr = std::get<BuildExpr>(expr.data);
      out["kind"] = "build";
      out["capacity"] = build_expr.capacity;
      out["count"] = (*this)(*build_expr.count);
      out["index_name"] = build_expr.index_name;
      out["body"] = (*this)(*build_expr.body);
      return out;
    }
    case ExprKind::UnaryPrimitive: {
      const auto &unary_expr = std::get<UnaryPrimitiveExpr>(expr.data);
      out["kind"] = "unary_primitive";
      out["op"] = unary_op_name(unary_expr.op);
      out["operand"] = (*this)(*unary_expr.operand);
      return out;
    }
    case ExprKind::BinaryPrimitive: {
      const auto &binary_expr = std::get<BinaryPrimitiveExpr>(expr.data);
      out["kind"] = "binary_primitive";
      out["op"] = binary_op_name(binary_expr.op);
      out["lhs"] = (*this)(*binary_expr.lhs);
      out["rhs"] = (*this)(*binary_expr.rhs);
      return out;
    }
    case ExprKind::Var: {
      const auto &var_expr = std::get<VarExpr>(expr.data);
      out["kind"] = "var";
      out["name"] = var_expr.name;
      return out;
    }
    case ExprKind::Call: {
      const auto &call_expr = std::get<CallExpr>(expr.data);
      out["kind"] = "call";
      out["callee"] = call_expr.callee;
      nlohmann::json arguments = nlohmann::json::array();
      for (const auto &argument : call_expr.arguments) {
        arguments.push_back((*this)(*argument));
      }
      out["arguments"] = std::move(arguments);
      return out;
    }
    case ExprKind::ArrayLiteral: {
      const auto &array_literal = std::get<ArrayLiteralExpr>(expr.data);
      out["kind"] = "array_literal";
      out["capacity"] = array_literal.capacity;
      nlohmann::json elements = nlohmann::json::array();
      for (const auto &element : array_literal.elements) {
        elements.push_back((*this)(*element));
      }
      out["elements"] = std::move(elements);
      return out;
    }
    case ExprKind::Product: {
      const auto &product_expr = std::get<ProductExpr>(expr.data);
      out["kind"] = "product";
      nlohmann::json components = nlohmann::json::array();
      for (const auto &component : product_expr.components) {
        components.push_back((*this)(*component));
      }
      out["components"] = std::move(components);
      return out;
    }
    case ExprKind::Len: {
      const auto &len_expr = std::get<LenExpr>(expr.data);
      out["kind"] = "len";
      out["array"] = (*this)(*len_expr.array);
      return out;
    }
    case ExprKind::Proj: {
      const auto &proj_expr = std::get<ProjExpr>(expr.data);
      out["kind"] = "proj";
      out["index"] = proj_expr.index;
      out["operand"] = (*this)(*proj_expr.operand);
      return out;
    }
    case ExprKind::Index: {
      const auto &index_expr = std::get<IndexExpr>(expr.data);
      out["kind"] = "index";
      out["array"] = (*this)(*index_expr.array);
      out["index"] = (*this)(*index_expr.index);
      return out;
    }
    case ExprKind::Literal: {
      const auto &literal_expr = std::get<LiteralExpr>(expr.data);
      out["kind"] = "literal";
      out["literal_kind"] = literal_kind_name(literal_expr.kind);
      out["value"] = literal_expr.value;
      return out;
    }
    }
    return out;
  }

  [[nodiscard]] nlohmann::json operator()(const Parameter &parameter) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(parameter.span);
    }
    out["name"] = parameter.name;
    out["type"] = (*this)(*parameter.type);
    return out;
  }

  [[nodiscard]] nlohmann::json operator()(const FunctionDecl &function) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(function.span);
    }
    out["name"] = function.name;
    nlohmann::json parameters = nlohmann::json::array();
    for (const auto &parameter : function.parameters) {
      parameters.push_back((*this)(parameter));
    }
    out["parameters"] = std::move(parameters);
    out["result_type"] = (*this)(*function.result_type);
    out["body"] = (*this)(*function.body);
    return out;
  }

  [[nodiscard]] nlohmann::json operator()(const ExportDecl &export_decl) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(export_decl.span);
    }
    out["name"] = export_decl.name;
    return out;
  }

  [[nodiscard]] nlohmann::json operator()(const Module &module) const {
    nlohmann::json out;
    if (include_spans) {
      out["span"] = span_or_omit(module.span);
    }
    nlohmann::json functions = nlohmann::json::array();
    for (const auto &function : module.functions) {
      functions.push_back((*this)(function));
    }
    out["functions"] = std::move(functions);
    out["export"] = (*this)(module.export_decl);
    return out;
  }
};

} // namespace

nlohmann::json serialize(const Module &module, bool include_spans) {
  const Serializer serializer{include_spans};
  return serializer(module);
}

} // namespace boundfin::source::ast
