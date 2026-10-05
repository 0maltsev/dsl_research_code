#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin_test.hpp"

#include <string>
#include <unordered_set>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;

// A realistic multi-function module exercising every binding form together:
// parameters, let, fold, build, nested calls to strictly-earlier functions,
// and every expression kind that can carry a resolved reference.
void a_realistic_module_resolves_cleanly() {
  const std::string source = "fn clamp(x: i32, lo: i32, hi: i32): i32 =\n"
                              "  if x < lo then lo else if x > hi then hi else x;\n"
                              "\n"
                              "fn sum_to(n: i32): i32 =\n"
                              "  fold<8>(n; acc, idx; i32bits(0x00000000); acc + idx);\n"
                              "\n"
                              "fn make(n: i32): arr<i32, 4> =\n"
                              "  build<4>(n; idx; idx * n);\n"
                              "\n"
                              "fn combine(n: i32): i32 =\n"
                              "  let total = sum_to(n) in\n"
                              "  clamp(total, i32bits(0x00000000), i32bits(0x00000064));\n"
                              "\n"
                              "export combine;\n";
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  const auto result = resolve_module(module);
  if (!result.ok) {
    throw boundfin::testing::CheckFailure("expected resolution to succeed, got " + result.diagnostic->code + " " +
                                           result.diagnostic->message);
  }

  BOUNDFIN_CHECK_EQ(module.functions.size(), std::size_t{4});
  BOUNDFIN_CHECK(module.export_decl.resolved_target_rank.has_value());
  BOUNDFIN_CHECK_EQ(*module.export_decl.resolved_target_rank, std::size_t{3}); // combine's rank

  // combine (rank 3) calls sum_to (rank 1) and clamp (rank 0): both
  // strictly earlier.
  const auto &combine_body = *module.functions[3].body;
  const auto &let_expr = std::get<LetExpr>(combine_body.data);
  const auto &sum_to_call = std::get<CallExpr>(let_expr.bound->data);
  BOUNDFIN_CHECK_EQ(*sum_to_call.resolved_callee_rank, std::size_t{1});
  const auto &clamp_call = std::get<CallExpr>(let_expr.body->data);
  BOUNDFIN_CHECK_EQ(*clamp_call.resolved_callee_rank, std::size_t{0});

  // Every BindingId assigned across the whole module is distinct (alpha-
  // renaming never accidentally reuses an id across different bindings,
  // even though several bindings share the surface name "n"/"x"/"idx"
  // across different functions).
  std::unordered_set<BindingId> seen_ids;
  const auto &clamp_params = module.functions[0].parameters;
  BOUNDFIN_CHECK_EQ(clamp_params.size(), std::size_t{3});
  for (const auto &parameter : clamp_params) {
    BOUNDFIN_CHECK(parameter.binding.has_value());
    BOUNDFIN_CHECK(seen_ids.insert(*parameter.binding).second); // true iff newly inserted
  }
  const auto &sum_to_param = module.functions[1].parameters[0];
  BOUNDFIN_CHECK(seen_ids.insert(*sum_to_param.binding).second);
  const auto &fold_expr = std::get<FoldExpr>(module.functions[1].body->data);
  BOUNDFIN_CHECK(seen_ids.insert(*fold_expr.accumulator_binding).second);
  BOUNDFIN_CHECK(seen_ids.insert(*fold_expr.index_binding).second);
  const auto &make_param = module.functions[2].parameters[0];
  BOUNDFIN_CHECK(seen_ids.insert(*make_param.binding).second);
  const auto &build_expr = std::get<BuildExpr>(module.functions[2].body->data);
  BOUNDFIN_CHECK(seen_ids.insert(*build_expr.index_binding).second);
  const auto &combine_param = module.functions[3].parameters[0];
  BOUNDFIN_CHECK(seen_ids.insert(*combine_param.binding).second);
  BOUNDFIN_CHECK(seen_ids.insert(*let_expr.binding).second);
}

void boundfin_resolve_full_module() { a_realistic_module_resolves_cleanly(); }

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_full_module)
