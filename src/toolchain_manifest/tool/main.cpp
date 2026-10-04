#include "boundfin/toolchain_manifest/config.hpp"
#include "boundfin/toolchain_manifest/manifest.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage() {
  std::cerr << "usage:\n"
            << "  boundfin-toolchain-manifest emit [--out <file>]\n"
            << "  boundfin-toolchain-manifest check --baseline <file>\n";
}

nlohmann::json build_manifest() {
  return boundfin::toolchain_manifest::build_default_manifest(
      boundfin::toolchain_manifest::config::kCxxCompiler, boundfin::toolchain_manifest::config::kOpenSslVersion,
      boundfin::toolchain_manifest::config::kOpenSslCryptoLibrary, boundfin::toolchain_manifest::config::kRepoRoot);
}

int run_emit(const std::vector<std::string> &args) {
  std::string out_path;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--out" && i + 1 < args.size()) {
      out_path = args[++i];
    } else {
      print_usage();
      return 2;
    }
  }

  const std::string serialized = build_manifest().dump(2);

  if (out_path.empty()) {
    std::cout << serialized << "\n";
  } else {
    std::ofstream out(out_path);
    out << serialized << "\n";
  }
  return 0;
}

int run_check(const std::vector<std::string> &args) {
  std::string baseline_path;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--baseline" && i + 1 < args.size()) {
      baseline_path = args[++i];
    } else {
      print_usage();
      return 2;
    }
  }
  if (baseline_path.empty()) {
    print_usage();
    return 2;
  }

  std::ifstream baseline_file(baseline_path);
  if (!baseline_file) {
    std::cerr << "error: cannot open baseline: " << baseline_path << "\n";
    return 2;
  }
  nlohmann::json baseline;
  baseline_file >> baseline;

  const auto report = boundfin::toolchain_manifest::compare_toolchain_versions(baseline, build_manifest());

  if (report.status == boundfin::toolchain_manifest::DriftStatus::kMatch) {
    std::cout << "toolchain identity matches baseline\n";
    return 0;
  }

  std::cerr << "RUN014 checksum_failure: toolchain identity drift in:";
  for (const auto &name : report.drifted_tools) {
    std::cerr << " " << name;
  }
  std::cerr << "\n";
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    print_usage();
    return 2;
  }

  const std::string &command = args.front();
  const std::vector<std::string> rest(args.begin() + 1, args.end());

  try {
    if (command == "emit") {
      return run_emit(rest);
    }
    if (command == "check") {
      return run_check(rest);
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }

  print_usage();
  return 2;
}
