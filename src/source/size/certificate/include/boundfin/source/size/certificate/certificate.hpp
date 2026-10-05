#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace boundfin::source::size::certificate {

// --- Size/upper terms (main.pdf Sec. 4.2: size terms "t ::= n | xi | t+t |
// kt"; upper terms "u ::= t | max(u,u)", n,k in N, xi ranging over ABI
// lengths, count results, and bound indices). This module does not give
// upper terms a separate type from size terms: Appendix A.4's inference
// rules (TRANS/ADD/SCALE/MAXL/MAXR/MAXU) are stated uniformly over "u",
// so one recursive Term type covers both; a caller that needs an "exact"
// (max-free) term enforces that by how it builds the tree, not by a
// distinct type. A Symbol is an opaque named atom at this layer -- its
// connection to an actual ABI length/count result/bound index is
// src/source/size/count's concern (Phase 3.4), not this checker's.

enum class TermKind { Literal, Symbol, Add, Scale, Max };

struct Term;
using TermPtr = std::shared_ptr<Term>;

struct LiteralTerm {
  std::uint64_t value = 0;
};
struct SymbolTerm {
  std::string name;
};
struct AddTerm {
  TermPtr lhs;
  TermPtr rhs;
};
struct ScaleTerm {
  std::uint64_t factor = 0;
  TermPtr term;
};
struct MaxTerm {
  TermPtr lhs;
  TermPtr rhs;
};

struct Term {
  TermKind kind = TermKind::Literal;
  std::variant<LiteralTerm, SymbolTerm, AddTerm, ScaleTerm, MaxTerm> data;
};

TermPtr make_literal(std::uint64_t value);
TermPtr make_symbol(std::string name);
TermPtr make_add(TermPtr lhs, TermPtr rhs);
TermPtr make_scale(std::uint64_t factor, TermPtr term);
TermPtr make_max(TermPtr lhs, TermPtr rhs);

[[nodiscard]] bool terms_equal(const Term &a, const Term &b);

// --- Constraints (main.pdf Sec. 4.2: "A constraint is t = t, t <= t, or a
// finite conjunction of constraints; Delta is a finite set of them.") A
// finite conjunction is represented at the call site as several separate
// Constraint values (the set Delta itself already is that conjunction);
// this type represents one conjunct.

enum class ConstraintKind { Le, Eq };

struct Constraint {
  ConstraintKind kind = ConstraintKind::Le;
  TermPtr lhs;
  TermPtr rhs;
};

[[nodiscard]] bool constraints_equal(const Constraint &a, const Constraint &b);

// --- Certificates (main.pdf Appendix A.4, "Count expressions,
// certificates, and refinement rules") ---------------------------------
//
// "Certificate derivations are generated only by [ASSUME, REFL, TRANS] and
// the rules below ... [ADD, SCALE, MAXL, MAXR, MAXU]." Two more mechanisms
// are then stated in prose, not boxed as inference rules, but are exactly
// as precisely specified as the eight boxed ones and are therefore
// implemented as two further node kinds rather than left out:
//   - "A closed arithmetic leaf is accepted exactly when direct
//     natural-number evaluation makes it true" -> ClosedEval: without it,
//     no closed numeric fact with two distinct literal terms (e.g. 3<=5)
//     could ever be proved by the eight boxed rules alone (REFL only gives
//     u<=u for one term on both sides; ASSUME needs the fact already in
//     Delta), so this is a necessary, fully-determined mechanism, just not
//     formally boxed.
//   - "Equality is checked as two inequalities" -> EqSandwich: a direct,
//     mechanical reading (prove t1<=t2 and t2<=t1, conclude t1=t2), adding
//     no proving power the eight rules didn't already have for each half.
//
// A third prose sentence -- "a substitution node replaces equals in a
// previously checked formula" -- is deliberately *not* implemented here:
// unlike the two above, the paper gives it no premise shape (which
// occurrences of a term get replaced; within Delta only, or within an
// already-derived fact), so inventing one would be a judgment call this
// module's narrow scope doesn't need to make yet. Table 6's C-Call premise
// ("simultaneous actual substitution is certified") is the first place
// this fragment is actually needed; it is deferred to that milestone
// (Phase 3.4, src/source/size/count), where it can be grounded against
// C-Call's and Q_f/K_f's exact formal shape instead of guessed here.
enum class CertificateKind {
  Assume,
  Refl,
  Trans,
  Add,
  Scale,
  MaxL,
  MaxR,
  MaxU,
  ClosedEval,
  EqSandwich,
};

struct Certificate;
using CertificatePtr = std::shared_ptr<Certificate>;

// phi in Delta => Delta |-cert phi
struct AssumeCert {
  Constraint claimed;
};
// => Delta |- u <= u
struct ReflCert {
  TermPtr term;
};
// Delta |- u1<=u2   Delta |- u2<=u3 => Delta |- u1<=u3
struct TransCert {
  CertificatePtr le_u1_u2;
  CertificatePtr le_u2_u3;
};
// a<=b   c<=d => a+c<=b+d
struct AddCert {
  CertificatePtr le_a_b;
  CertificatePtr le_c_d;
};
// a<=b => ka<=kb, k in N
struct ScaleCert {
  std::uint64_t factor = 0;
  CertificatePtr le_a_b;
};
// => a<=max(a,b)
struct MaxLCert {
  TermPtr a;
  TermPtr b;
};
// => b<=max(a,b)
struct MaxRCert {
  TermPtr a;
  TermPtr b;
};
// a<=c   b<=c => max(a,b)<=c
struct MaxUCert {
  CertificatePtr le_a_c;
  CertificatePtr le_b_c;
};
// A closed (symbol-free) claimed constraint, accepted exactly when direct
// natural-number evaluation makes it true.
struct ClosedEvalCert {
  Constraint claimed;
};
// Delta |- t1<=t2   Delta |- t2<=t1 => Delta |- t1=t2
struct EqSandwichCert {
  CertificatePtr le_forward;
  CertificatePtr le_backward;
};

struct Certificate {
  CertificateKind kind = CertificateKind::Assume;
  std::variant<AssumeCert, ReflCert, TransCert, AddCert, ScaleCert, MaxLCert, MaxRCert, MaxUCert, ClosedEvalCert,
               EqSandwichCert>
      data;
};

CertificatePtr cert_assume(Constraint claimed);
CertificatePtr cert_refl(TermPtr term);
CertificatePtr cert_trans(CertificatePtr le_u1_u2, CertificatePtr le_u2_u3);
CertificatePtr cert_add(CertificatePtr le_a_b, CertificatePtr le_c_d);
CertificatePtr cert_scale(std::uint64_t factor, CertificatePtr le_a_b);
CertificatePtr cert_max_l(TermPtr a, TermPtr b);
CertificatePtr cert_max_r(TermPtr a, TermPtr b);
CertificatePtr cert_max_u(CertificatePtr le_a_c, CertificatePtr le_b_c);
CertificatePtr cert_closed_eval(Constraint claimed);
CertificatePtr cert_eq_sandwich(CertificatePtr le_forward, CertificatePtr le_backward);

struct CheckOutcome {
  bool ok = false;
  std::optional<Constraint> proved;          // set iff ok
  std::optional<std::string> failure_reason; // set iff !ok
};

// Checks that `certificate` is a valid finite derivation under hypothesis
// set `delta`, independently recomputing/verifying every node's conclusion
// from its kind, its local parameters (a claimed constraint, a term, a
// scale factor), and its premises' *already-checked* conclusions -- never
// trusting a conclusion a node merely asserts for itself except at an
// Assume/ClosedEval leaf, where the check is against `delta` or direct
// evaluation respectively (main.pdf Sec. 3.3, p.8, "Memory and trust
// boundaries": "Certificate generation may use an untrusted solver, but
// module acceptance depends on checking a finite derivation in the rules
// of section A.4."). First-error: recursion stops at the first invalid
// node and `failure_reason` names it.
[[nodiscard]] CheckOutcome check_certificate(const CertificatePtr &certificate, const std::vector<Constraint> &delta);

} // namespace boundfin::source::size::certificate
