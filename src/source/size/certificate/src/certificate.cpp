#include "boundfin/source/size/certificate/certificate.hpp"

#include <limits>

namespace boundfin::source::size::certificate {

// --- Term construction and equality ---------------------------------------

TermPtr make_literal(std::uint64_t value) {
  auto term = std::make_shared<Term>();
  term->kind = TermKind::Literal;
  term->data = LiteralTerm{value};
  return term;
}

TermPtr make_symbol(std::string name) {
  auto term = std::make_shared<Term>();
  term->kind = TermKind::Symbol;
  term->data = SymbolTerm{std::move(name)};
  return term;
}

TermPtr make_add(TermPtr lhs, TermPtr rhs) {
  auto term = std::make_shared<Term>();
  term->kind = TermKind::Add;
  term->data = AddTerm{std::move(lhs), std::move(rhs)};
  return term;
}

TermPtr make_scale(std::uint64_t factor, TermPtr inner) {
  auto term = std::make_shared<Term>();
  term->kind = TermKind::Scale;
  term->data = ScaleTerm{factor, std::move(inner)};
  return term;
}

TermPtr make_max(TermPtr lhs, TermPtr rhs) {
  auto term = std::make_shared<Term>();
  term->kind = TermKind::Max;
  term->data = MaxTerm{std::move(lhs), std::move(rhs)};
  return term;
}

namespace {
// Null-tolerant: a null child anywhere in a Term tree is malformed and
// matches nothing, not even another null (conservative, not a crash --
// mirrors "a true formula for which no accepted certificate is supplied is
// conservatively rejected", main.pdf p.10, extended to malformed/null
// structure rather than just unprovable formulas).
bool terms_equal_ptr(const TermPtr &a, const TermPtr &b) {
  if (!a || !b) {
    return false;
  }
  return terms_equal(*a, *b);
}
} // namespace

bool terms_equal(const Term &a, const Term &b) {
  if (a.kind != b.kind) {
    return false;
  }
  bool result = false;
  switch (a.kind) {
  case TermKind::Literal:
    result = std::get<LiteralTerm>(a.data).value == std::get<LiteralTerm>(b.data).value;
    break;
  case TermKind::Symbol:
    result = std::get<SymbolTerm>(a.data).name == std::get<SymbolTerm>(b.data).name;
    break;
  case TermKind::Add: {
    const auto &add_a = std::get<AddTerm>(a.data);
    const auto &add_b = std::get<AddTerm>(b.data);
    result = terms_equal_ptr(add_a.lhs, add_b.lhs) && terms_equal_ptr(add_a.rhs, add_b.rhs);
    break;
  }
  case TermKind::Scale: {
    const auto &scale_a = std::get<ScaleTerm>(a.data);
    const auto &scale_b = std::get<ScaleTerm>(b.data);
    result = scale_a.factor == scale_b.factor && terms_equal_ptr(scale_a.term, scale_b.term);
    break;
  }
  case TermKind::Max: {
    const auto &max_a = std::get<MaxTerm>(a.data);
    const auto &max_b = std::get<MaxTerm>(b.data);
    result = terms_equal_ptr(max_a.lhs, max_b.lhs) && terms_equal_ptr(max_a.rhs, max_b.rhs);
    break;
  }
  }
  return result;
}

bool constraints_equal(const Constraint &a, const Constraint &b) {
  if (a.kind != b.kind) {
    return false;
  }
  if (!a.lhs || !a.rhs || !b.lhs || !b.rhs) {
    return false; // malformed (null term): conservatively not equal, never crash
  }
  return terms_equal(*a.lhs, *b.lhs) && terms_equal(*a.rhs, *b.rhs);
}

// --- Certificate construction ----------------------------------------------

CertificatePtr cert_assume(Constraint claimed) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::Assume;
  certificate->data = AssumeCert{std::move(claimed)};
  return certificate;
}

CertificatePtr cert_refl(TermPtr term) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::Refl;
  certificate->data = ReflCert{std::move(term)};
  return certificate;
}

CertificatePtr cert_trans(CertificatePtr le_u1_u2, CertificatePtr le_u2_u3) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::Trans;
  certificate->data = TransCert{std::move(le_u1_u2), std::move(le_u2_u3)};
  return certificate;
}

CertificatePtr cert_add(CertificatePtr le_a_b, CertificatePtr le_c_d) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::Add;
  certificate->data = AddCert{std::move(le_a_b), std::move(le_c_d)};
  return certificate;
}

CertificatePtr cert_scale(std::uint64_t factor, CertificatePtr le_a_b) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::Scale;
  certificate->data = ScaleCert{factor, std::move(le_a_b)};
  return certificate;
}

CertificatePtr cert_max_l(TermPtr a, TermPtr b) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::MaxL;
  certificate->data = MaxLCert{std::move(a), std::move(b)};
  return certificate;
}

CertificatePtr cert_max_r(TermPtr a, TermPtr b) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::MaxR;
  certificate->data = MaxRCert{std::move(a), std::move(b)};
  return certificate;
}

CertificatePtr cert_max_u(CertificatePtr le_a_c, CertificatePtr le_b_c) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::MaxU;
  certificate->data = MaxUCert{std::move(le_a_c), std::move(le_b_c)};
  return certificate;
}

CertificatePtr cert_closed_eval(Constraint claimed) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::ClosedEval;
  certificate->data = ClosedEvalCert{std::move(claimed)};
  return certificate;
}

CertificatePtr cert_eq_sandwich(CertificatePtr le_forward, CertificatePtr le_backward) {
  auto certificate = std::make_shared<Certificate>();
  certificate->kind = CertificateKind::EqSandwich;
  certificate->data = EqSandwichCert{std::move(le_forward), std::move(le_backward)};
  return certificate;
}

namespace {

// Null-tolerant: a null child is malformed and cannot be evaluated (never
// crashes; returns std::nullopt like any other "not closed" case).
std::optional<std::uint64_t> evaluate_closed_ptr(const TermPtr &term) {
  if (!term) {
    return std::nullopt;
  }
  return evaluate_closed(*term);
}

CheckOutcome fail(std::string reason) { return CheckOutcome{false, std::nullopt, std::move(reason)}; }

CheckOutcome ok_with(Constraint proved) { return CheckOutcome{true, std::move(proved), std::nullopt}; }

} // namespace

// Returns std::nullopt if the term contains a Symbol (or a null/malformed
// child) anywhere, or if exact evaluation would overflow std::uint64_t
// (rejected, not wrapped: this is mathematical-integer evaluation per
// main.pdf Sec. 4.2's "eta |= Delta has its usual mathematical-integer
// meaning", not machine modular arithmetic). main.pdf's own N is an
// unbounded mathematical domain; representing it with std::uint64_t here
// is this module's own implementation choice, not a paper-specified bound.
// It is conservative (a true closed fact can only be rejected as
// "overflow", never wrongly accepted), and ample for this module's actual
// domain -- capacities and ABI lengths are themselves bounded by
// 2^31-1 -- but a hypothetical future caller composing very large Scale/Add
// terms outside that domain could see a spurious rejection with no
// deeper cause than this 64-bit choice.
std::optional<std::uint64_t> evaluate_closed(const Term &term) {
  switch (term.kind) {
  case TermKind::Literal:
    return std::get<LiteralTerm>(term.data).value;
  case TermKind::Symbol:
    return std::nullopt;
  case TermKind::Add: {
    const auto &add = std::get<AddTerm>(term.data);
    const auto lhs = evaluate_closed_ptr(add.lhs);
    const auto rhs = evaluate_closed_ptr(add.rhs);
    if (!lhs || !rhs) {
      return std::nullopt;
    }
    if (*lhs > std::numeric_limits<std::uint64_t>::max() - *rhs) {
      return std::nullopt; // would overflow
    }
    return *lhs + *rhs;
  }
  case TermKind::Scale: {
    const auto &scale = std::get<ScaleTerm>(term.data);
    const auto inner = evaluate_closed_ptr(scale.term);
    if (!inner) {
      return std::nullopt;
    }
    if (scale.factor != 0 && *inner > std::numeric_limits<std::uint64_t>::max() / scale.factor) {
      return std::nullopt; // would overflow
    }
    return scale.factor * (*inner);
  }
  case TermKind::Max: {
    const auto &max_term = std::get<MaxTerm>(term.data);
    const auto lhs = evaluate_closed_ptr(max_term.lhs);
    const auto rhs = evaluate_closed_ptr(max_term.rhs);
    if (!lhs || !rhs) {
      return std::nullopt;
    }
    return *lhs > *rhs ? *lhs : *rhs;
  }
  }
  return std::nullopt;
}

std::optional<TermPtr> substitute_term(const TermPtr &term, const std::unordered_map<std::string, TermPtr> &substitution) {
  if (!term) {
    return std::nullopt;
  }
  switch (term->kind) {
  case TermKind::Literal:
    return term;
  case TermKind::Symbol: {
    const auto it = substitution.find(std::get<SymbolTerm>(term->data).name);
    if (it == substitution.end()) {
      return std::nullopt;
    }
    return it->second;
  }
  case TermKind::Add: {
    const auto &add = std::get<AddTerm>(term->data);
    const auto lhs = substitute_term(add.lhs, substitution);
    const auto rhs = substitute_term(add.rhs, substitution);
    if (!lhs || !rhs) {
      return std::nullopt;
    }
    return make_add(*lhs, *rhs);
  }
  case TermKind::Scale: {
    const auto &scale = std::get<ScaleTerm>(term->data);
    const auto inner = substitute_term(scale.term, substitution);
    if (!inner) {
      return std::nullopt;
    }
    return make_scale(scale.factor, *inner);
  }
  case TermKind::Max: {
    const auto &max_term = std::get<MaxTerm>(term->data);
    const auto lhs = substitute_term(max_term.lhs, substitution);
    const auto rhs = substitute_term(max_term.rhs, substitution);
    if (!lhs || !rhs) {
      return std::nullopt;
    }
    return make_max(*lhs, *rhs);
  }
  }
  return std::nullopt;
}

CheckOutcome check_certificate(const CertificatePtr &certificate, const std::vector<Constraint> &delta) {
  if (!certificate) {
    return fail("null certificate");
  }
  switch (certificate->kind) {
  case CertificateKind::Assume: {
    const auto &node = std::get<AssumeCert>(certificate->data);
    for (const auto &phi : delta) {
      if (constraints_equal(phi, node.claimed)) {
        return ok_with(node.claimed);
      }
    }
    return fail("ASSUME: claimed constraint is not a member of Delta");
  }
  case CertificateKind::Refl: {
    const auto &node = std::get<ReflCert>(certificate->data);
    if (!node.term) {
      return fail("REFL: term is null");
    }
    return ok_with(Constraint{ConstraintKind::Le, node.term, node.term});
  }
  case CertificateKind::Trans: {
    const auto &node = std::get<TransCert>(certificate->data);
    const auto left = check_certificate(node.le_u1_u2, delta);
    if (!left.ok) {
      return fail("TRANS: first premise invalid: " + *left.failure_reason);
    }
    if (left.proved->kind != ConstraintKind::Le) {
      return fail("TRANS: first premise is not a <= fact");
    }
    const auto right = check_certificate(node.le_u2_u3, delta);
    if (!right.ok) {
      return fail("TRANS: second premise invalid: " + *right.failure_reason);
    }
    if (right.proved->kind != ConstraintKind::Le) {
      return fail("TRANS: second premise is not a <= fact");
    }
    if (!terms_equal(*left.proved->rhs, *right.proved->lhs)) {
      return fail("TRANS: chain mismatch (u2 differs between premises)");
    }
    return ok_with(Constraint{ConstraintKind::Le, left.proved->lhs, right.proved->rhs});
  }
  case CertificateKind::Add: {
    const auto &node = std::get<AddCert>(certificate->data);
    const auto first = check_certificate(node.le_a_b, delta);
    if (!first.ok || first.proved->kind != ConstraintKind::Le) {
      return fail("ADD: first premise is not a valid <= fact");
    }
    const auto second = check_certificate(node.le_c_d, delta);
    if (!second.ok || second.proved->kind != ConstraintKind::Le) {
      return fail("ADD: second premise is not a valid <= fact");
    }
    return ok_with(Constraint{ConstraintKind::Le, make_add(first.proved->lhs, second.proved->lhs),
                               make_add(first.proved->rhs, second.proved->rhs)});
  }
  case CertificateKind::Scale: {
    const auto &node = std::get<ScaleCert>(certificate->data);
    const auto premise = check_certificate(node.le_a_b, delta);
    if (!premise.ok || premise.proved->kind != ConstraintKind::Le) {
      return fail("SCALE: premise is not a valid <= fact");
    }
    return ok_with(Constraint{ConstraintKind::Le, make_scale(node.factor, premise.proved->lhs),
                               make_scale(node.factor, premise.proved->rhs)});
  }
  case CertificateKind::MaxL: {
    const auto &node = std::get<MaxLCert>(certificate->data);
    if (!node.a || !node.b) {
      return fail("MAXL: a or b is null");
    }
    return ok_with(Constraint{ConstraintKind::Le, node.a, make_max(node.a, node.b)});
  }
  case CertificateKind::MaxR: {
    const auto &node = std::get<MaxRCert>(certificate->data);
    if (!node.a || !node.b) {
      return fail("MAXR: a or b is null");
    }
    return ok_with(Constraint{ConstraintKind::Le, node.b, make_max(node.a, node.b)});
  }
  case CertificateKind::MaxU: {
    const auto &node = std::get<MaxUCert>(certificate->data);
    const auto first = check_certificate(node.le_a_c, delta);
    if (!first.ok || first.proved->kind != ConstraintKind::Le) {
      return fail("MAXU: first premise is not a valid <= fact");
    }
    const auto second = check_certificate(node.le_b_c, delta);
    if (!second.ok || second.proved->kind != ConstraintKind::Le) {
      return fail("MAXU: second premise is not a valid <= fact");
    }
    if (!terms_equal(*first.proved->rhs, *second.proved->rhs)) {
      return fail("MAXU: premises do not share the same upper bound c");
    }
    return ok_with(
        Constraint{ConstraintKind::Le, make_max(first.proved->lhs, second.proved->lhs), first.proved->rhs});
  }
  case CertificateKind::ClosedEval: {
    const auto &node = std::get<ClosedEvalCert>(certificate->data);
    if (!node.claimed.lhs || !node.claimed.rhs) {
      return fail("CLOSED_EVAL: claimed constraint has a null term");
    }
    const auto lhs_value = evaluate_closed_ptr(node.claimed.lhs);
    const auto rhs_value = evaluate_closed_ptr(node.claimed.rhs);
    if (!lhs_value || !rhs_value) {
      return fail("CLOSED_EVAL: claimed constraint is not closed (contains a free symbol) or overflows");
    }
    const bool holds = node.claimed.kind == ConstraintKind::Le ? (*lhs_value <= *rhs_value) : (*lhs_value == *rhs_value);
    if (!holds) {
      return fail("CLOSED_EVAL: evaluated arithmetic does not satisfy the claimed relation");
    }
    return ok_with(node.claimed);
  }
  case CertificateKind::EqSandwich: {
    const auto &node = std::get<EqSandwichCert>(certificate->data);
    const auto forward = check_certificate(node.le_forward, delta);
    if (!forward.ok || forward.proved->kind != ConstraintKind::Le) {
      return fail("EQ_SANDWICH: forward premise is not a valid <= fact");
    }
    const auto backward = check_certificate(node.le_backward, delta);
    if (!backward.ok || backward.proved->kind != ConstraintKind::Le) {
      return fail("EQ_SANDWICH: backward premise is not a valid <= fact");
    }
    if (!terms_equal(*forward.proved->lhs, *backward.proved->rhs) ||
        !terms_equal(*forward.proved->rhs, *backward.proved->lhs)) {
      return fail("EQ_SANDWICH: forward/backward premises are not over the same two terms");
    }
    return ok_with(Constraint{ConstraintKind::Eq, forward.proved->lhs, forward.proved->rhs});
  }
  }
  return fail("internal: unreachable certificate kind");
}

} // namespace boundfin::source::size::certificate
