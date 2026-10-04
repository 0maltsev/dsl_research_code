#include "boundfin/toolchain_manifest/manifest.hpp"
#include "boundfin_test.hpp"

#include <cstddef>
#include <string>

namespace {

// RUN014 ("checksum_failure": "Artifact/input/output checksum or identity
// drift", diagnostics-and-status.md) is the code a toolchain-manifest
// drift check must report.
void RUN014_toolchain_manifest_drift_rejected() {
  using boundfin::toolchain_manifest::DriftStatus;
  using boundfin::toolchain_manifest::compare_toolchain_versions;

  const nlohmann::json baseline = {
      {"cmake", {{"version", "4.3.3"}, {"sha256", std::string(64, 'a')}}},
      {"ninja", {{"version", "1.13.2"}, {"sha256", std::string(64, 'b')}}},
  };

  {
    const auto report = compare_toolchain_versions(baseline, baseline);
    BOUNDFIN_CHECK(report.status == DriftStatus::kMatch);
    BOUNDFIN_CHECK(report.diagnostic_codes.empty());
    BOUNDFIN_CHECK(report.drifted_tools.empty());
  }

  {
    // Same declared version, tampered hash: still drift.
    nlohmann::json tampered = baseline;
    tampered["cmake"]["sha256"] = std::string(64, 'c');
    const auto report = compare_toolchain_versions(baseline, tampered);
    BOUNDFIN_CHECK(report.status == DriftStatus::kDrift);
    BOUNDFIN_CHECK_EQ(report.diagnostic_codes.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.diagnostic_codes.front(), std::string("RUN014"));
    BOUNDFIN_CHECK_EQ(report.drifted_tools.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.drifted_tools.front(), std::string("cmake"));
  }

  {
    // A tool recorded in the baseline but absent from the current host.
    nlohmann::json shrunk = baseline;
    shrunk.erase("ninja");
    const auto report = compare_toolchain_versions(baseline, shrunk);
    BOUNDFIN_CHECK(report.status == DriftStatus::kDrift);
    BOUNDFIN_CHECK_EQ(report.drifted_tools.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.drifted_tools.front(), std::string("ninja"));
  }

  {
    // A tool present now but absent from the recorded baseline.
    nlohmann::json grown = baseline;
    grown["clang"] = {{"version", "22.1.6"}, {"sha256", std::string(64, 'd')}};
    const auto report = compare_toolchain_versions(baseline, grown);
    BOUNDFIN_CHECK(report.status == DriftStatus::kDrift);
    BOUNDFIN_CHECK_EQ(report.drifted_tools.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.drifted_tools.front(), std::string("clang"));
  }

  {
    // PLAN.md Phase 1's named "tool version rejection" test: a tool
    // upgraded to a newer declared version (with the correspondingly
    // different binary hash a real upgrade would have) is drift, not
    // silently accepted because the tool is still "present."
    nlohmann::json upgraded = baseline;
    upgraded["ninja"] = {{"version", "1.14.0"}, {"sha256", std::string(64, 'e')}};
    const auto report = compare_toolchain_versions(baseline, upgraded);
    BOUNDFIN_CHECK(report.status == DriftStatus::kDrift);
    BOUNDFIN_CHECK_EQ(report.diagnostic_codes.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.diagnostic_codes.front(), std::string("RUN014"));
    BOUNDFIN_CHECK_EQ(report.drifted_tools.size(), std::size_t{1});
    BOUNDFIN_CHECK_EQ(report.drifted_tools.front(), std::string("ninja"));
  }
}

} // namespace

BOUNDFIN_TEST_MAIN(RUN014_toolchain_manifest_drift_rejected)
