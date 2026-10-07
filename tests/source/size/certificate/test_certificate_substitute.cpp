#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <unordered_map>

namespace {

using namespace boundfin::source::size::certificate;

// AM-037 step 4a: the pure structural half of main.pdf p.12's call-site
// substitution ("exact or upper actual length expressions are
// substituted simultaneously into K_f"), with no certificate check.

void substitute_term_replaces_a_matching_symbol_leaf() {
  const std::unordered_map<std::string, TermPtr> substitution{{"n_x", make_literal(5)}};
  const auto result = substitute_term(make_symbol("n_x"), substitution);
  BOUNDFIN_CHECK(result.has_value());
  BOUNDFIN_CHECK(terms_equal(**result, *make_literal(5)));
}

// A substituted symbol's own replacement is used as-is, not itself
// re-substituted even if it happens to also be a key in the map --
// single-pass substitution, not a fixed-point/iterated rewrite.
void substitute_term_does_not_re_substitute_the_replacement() {
  const std::unordered_map<std::string, TermPtr> substitution{{"a", make_symbol("b")}, {"b", make_literal(99)}};
  const auto result = substitute_term(make_symbol("a"), substitution);
  BOUNDFIN_CHECK(result.has_value());
  BOUNDFIN_CHECK(terms_equal(**result, *make_symbol("b"))); // not literal(99)
}

void substitute_term_leaves_a_literal_unchanged() {
  const auto literal = make_literal(3);
  const auto result = substitute_term(literal, {});
  BOUNDFIN_CHECK(result.has_value());
  BOUNDFIN_CHECK(result->get() == literal.get()); // pointer identity: nothing needed replacing
}

void substitute_term_recurses_through_add() {
  const std::unordered_map<std::string, TermPtr> substitution{{"a", make_literal(2)}, {"b", make_literal(3)}};
  const auto term = make_add(make_symbol("a"), make_symbol("b"));
  const auto result = substitute_term(term, substitution);
  BOUNDFIN_CHECK(result.has_value());
  // Structurally add(2,3), never arithmetically reduced to 5 -- matches
  // this project's own established non-reducing convention for every
  // term-building operation.
  BOUNDFIN_CHECK(terms_equal(**result, *make_add(make_literal(2), make_literal(3))));
  BOUNDFIN_CHECK(!terms_equal(**result, *make_literal(5)));
}

void substitute_term_recurses_through_scale() {
  const std::unordered_map<std::string, TermPtr> substitution{{"x", make_literal(2)}};
  const auto term = make_scale(4, make_symbol("x"));
  const auto result = substitute_term(term, substitution);
  BOUNDFIN_CHECK(result.has_value());
  BOUNDFIN_CHECK(terms_equal(**result, *make_scale(4, make_literal(2))));
}

void substitute_term_recurses_through_max() {
  const std::unordered_map<std::string, TermPtr> substitution{{"a", make_literal(1)}, {"b", make_literal(2)}};
  const auto term = make_max(make_symbol("a"), make_symbol("b"));
  const auto result = substitute_term(term, substitution);
  BOUNDFIN_CHECK(result.has_value());
  BOUNDFIN_CHECK(terms_equal(**result, *make_max(make_literal(1), make_literal(2))));
}

// main.pdf p.12: "If an exact actual is unavailable, upper substitution
// yields a conservative star result" -- at this Term-level primitive,
// that case is exactly an unmapped Symbol, returning std::nullopt (a
// plain optional, no diagnostic code, mirroring evaluate_closed's own
// established precedent).
void substitute_term_returns_nullopt_for_an_unmapped_symbol() {
  const auto result = substitute_term(make_symbol("unmapped"), {});
  BOUNDFIN_CHECK(!result.has_value());
}

// Propagates through recursion: one unmapped symbol anywhere in the
// tree makes the whole substitution fail, even when a sibling resolves.
void substitute_term_returns_nullopt_when_a_nested_symbol_is_unmapped() {
  const std::unordered_map<std::string, TermPtr> substitution{{"a", make_literal(1)}};
  const auto term = make_add(make_symbol("a"), make_symbol("unmapped"));
  const auto result = substitute_term(term, substitution);
  BOUNDFIN_CHECK(!result.has_value());
}

void substitute_term_returns_nullopt_for_a_null_term() {
  const auto result = substitute_term(nullptr, {});
  BOUNDFIN_CHECK(!result.has_value());
}

void boundfin_certificate_substitute() {
  substitute_term_replaces_a_matching_symbol_leaf();
  substitute_term_does_not_re_substitute_the_replacement();
  substitute_term_leaves_a_literal_unchanged();
  substitute_term_recurses_through_add();
  substitute_term_recurses_through_scale();
  substitute_term_recurses_through_max();
  substitute_term_returns_nullopt_for_an_unmapped_symbol();
  substitute_term_returns_nullopt_when_a_nested_symbol_is_unmapped();
  substitute_term_returns_nullopt_for_a_null_term();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_substitute)
