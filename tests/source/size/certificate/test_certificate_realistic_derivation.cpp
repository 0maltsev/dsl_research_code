#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <vector>

namespace {

using namespace boundfin::source::size::certificate;

// A realistic multi-step derivation combining Assume, Refl, Add, MaxL, and
// Trans: from "n_x <= N" (an ABI-length-vs-capacity fact, the shape
// Table 6's C-ABI premise has), proves "n_x + 1 <= max(N + 1, M)" for an
// unrelated symbol M -- the kind of bound a later count-fragment rule
// (e.g. combining C-ABI with C-If-U's max) would need to assemble.
CertificatePtr build_realistic_derivation(const Constraint &n_x_le_N) {
  const auto one_le_one = cert_refl(make_literal(1));             // 1 <= 1
  const auto sum = cert_add(cert_assume(n_x_le_N), one_le_one);   // n_x+1 <= N+1
  const auto n_plus_one = make_add(n_x_le_N.rhs, make_literal(1)); // N+1
  const auto widen = cert_max_l(n_plus_one, make_symbol("M"));    // N+1 <= max(N+1,M)
  return cert_trans(sum, widen);                                            // n_x+1 <= max(N+1,M)
}

void realistic_multi_step_derivation_succeeds() {
  const Constraint n_x_le_N{ConstraintKind::Le, make_symbol("n_x"), make_symbol("N")};
  const std::vector<Constraint> delta{n_x_le_N};
  const auto certificate = build_realistic_derivation(n_x_le_N);
  const auto outcome = check_certificate(certificate, delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.proved->kind == ConstraintKind::Le);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_add(make_symbol("n_x"), make_literal(1))));
  BOUNDFIN_CHECK(
      terms_equal(*outcome.proved->rhs, *make_max(make_add(make_symbol("N"), make_literal(1)), make_symbol("M"))));
}

// The same derivation with its one load-bearing hypothesis removed from
// Delta must fail: a mutation-shaped functional test of the checker
// itself, confirming it does not accept an unjustified ASSUME leaf deep
// inside an otherwise well-formed tree.
void realistic_derivation_fails_when_its_hypothesis_is_missing() {
  const Constraint n_x_le_N{ConstraintKind::Le, make_symbol("n_x"), make_symbol("N")};
  const auto certificate = build_realistic_derivation(n_x_le_N);
  const auto outcome = check_certificate(certificate, {}); // empty Delta this time
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK(outcome.failure_reason.has_value());
}

void null_certificate_is_rejected() {
  const auto outcome = check_certificate(nullptr, {});
  BOUNDFIN_CHECK(!outcome.ok);
}

void boundfin_certificate_realistic_derivation() {
  realistic_multi_step_derivation_succeeds();
  realistic_derivation_fails_when_its_hypothesis_is_missing();
  null_certificate_is_rejected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_realistic_derivation)
