#include "boundfin/toolchain_manifest/manifest.hpp"

#include "boundfin/support/sha256.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <stdexcept>

namespace boundfin::toolchain_manifest {

namespace {

bool is_valid_tool_name(const std::string &name) {
  static const std::regex pattern(R"(^[A-Za-z][A-Za-z0-9_.-]*$)");
  return std::regex_match(name, pattern);
}

std::string trim_trailing_whitespace(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

} // namespace

std::optional<std::filesystem::path> resolve_on_path(const std::string &name) {
  if (name.find('/') != std::string::npos) {
    throw std::invalid_argument("boundfin::toolchain_manifest::resolve_on_path: name must not contain '/': " +
                                 name);
  }

  const char *path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return std::nullopt;
  }

  std::string path_value(path_env);
  std::size_t start = 0;
  while (start <= path_value.size()) {
    std::size_t end = path_value.find(':', start);
    if (end == std::string::npos) {
      end = path_value.size();
    }
    std::string dir = path_value.substr(start, end - start);
    if (dir.empty()) {
      dir = ".";
    }
    const std::filesystem::path candidate = std::filesystem::path(dir) / name;
    if (access(candidate.c_str(), X_OK) == 0) {
      std::error_code ec;
      std::filesystem::path canonical = std::filesystem::canonical(candidate, ec);
      return ec ? candidate : canonical;
    }
    start = end + 1;
  }
  return std::nullopt;
}

std::string run_and_capture(const std::filesystem::path &executable,
                             const std::vector<std::string> &version_args) {
  std::array<int, 2> pipe_fds{-1, -1};
  if (pipe(pipe_fds.data()) != 0) {
    throw std::runtime_error("boundfin::toolchain_manifest: pipe() failed for " + executable.string());
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    throw std::runtime_error("boundfin::toolchain_manifest: fork() failed for " + executable.string());
  }

  if (pid == 0) {
    close(pipe_fds[0]);
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);

    std::string exe_str = executable.string();
    std::vector<std::string> args_storage = version_args;
    std::vector<char *> argv;
    argv.push_back(exe_str.data());
    for (auto &arg : args_storage) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    execv(exe_str.c_str(), argv.data());
    _exit(127);
  }

  close(pipe_fds[1]);
  std::string output;
  std::array<char, 4096> buffer{};
  ssize_t bytes_read = 0;
  while ((bytes_read = read(pipe_fds[0], buffer.data(), buffer.size())) > 0) {
    output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
  }
  close(pipe_fds[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    throw std::runtime_error("boundfin::toolchain_manifest: waitpid() failed for " + executable.string());
  }
  if (WIFSIGNALED(status)) {
    throw std::runtime_error("boundfin::toolchain_manifest: " + executable.string() +
                              " terminated by signal " + std::to_string(WTERMSIG(status)));
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error("boundfin::toolchain_manifest: " + executable.string() +
                              " exited with nonzero status " + std::to_string(WEXITSTATUS(status)));
  }

  return trim_trailing_whitespace(std::move(output));
}

std::string extract_first_version_token(const std::string &version_output) {
  static const std::regex pattern(R"(\d+(?:\.\d+)+)");
  std::smatch match;
  if (!std::regex_search(version_output, match, pattern)) {
    throw std::invalid_argument(
        "boundfin::toolchain_manifest::extract_first_version_token: no dotted-decimal version "
        "token found in: " +
        version_output);
  }
  return match.str();
}

ToolIdentity identity_from_subprocess(const std::filesystem::path &executable,
                                       const std::vector<std::string> &version_args) {
  const std::string output = run_and_capture(executable, version_args);
  return ToolIdentity{extract_first_version_token(output), support::sha256_hex_of_file(executable)};
}

ToolIdentity identity_from_npm_lockfile_entry(const std::filesystem::path &package_lock_json_path,
                                               const std::string &package_name) {
  std::ifstream file(package_lock_json_path);
  if (!file) {
    throw std::runtime_error("boundfin::toolchain_manifest: cannot open package-lock.json: " +
                              package_lock_json_path.string());
  }
  nlohmann::json document;
  file >> document;

  const auto packages_it = document.find("packages");
  if (packages_it == document.end()) {
    throw std::runtime_error("boundfin::toolchain_manifest: package-lock.json has no \"packages\": " +
                              package_lock_json_path.string());
  }
  const auto entry_it = packages_it->find("node_modules/" + package_name);
  if (entry_it == packages_it->end()) {
    throw std::runtime_error("boundfin::toolchain_manifest: package-lock.json has no entry for " + package_name +
                              ": " + package_lock_json_path.string());
  }

  const auto version_it = entry_it->find("version");
  const auto resolved_it = entry_it->find("resolved");
  const auto integrity_it = entry_it->find("integrity");
  if (version_it == entry_it->end() || !version_it->is_string() || resolved_it == entry_it->end() ||
      !resolved_it->is_string() || integrity_it == entry_it->end() || !integrity_it->is_string()) {
    throw std::runtime_error(
        "boundfin::toolchain_manifest: package-lock.json entry for " + package_name +
        " is missing a string \"version\"/\"resolved\"/\"integrity\": " + package_lock_json_path.string());
  }

  const nlohmann::json identity_fragment = {{"integrity", *integrity_it}, {"resolved", *resolved_it}};
  return ToolIdentity{version_it->get<std::string>(), support::sha256_hex(identity_fragment.dump())};
}

ToolIdentity identity_from_versioned_file(const std::string &version, const std::filesystem::path &identity_file) {
  return ToolIdentity{version, support::sha256_hex_of_file(identity_file)};
}

nlohmann::json to_toolchain_versions_json(const std::map<std::string, ToolIdentity> &identities) {
  if (identities.empty()) {
    throw std::invalid_argument(
        "boundfin::toolchain_manifest::to_toolchain_versions_json: at least one tool is required");
  }

  nlohmann::json versions = nlohmann::json::object();
  for (const auto &[name, identity] : identities) {
    if (!is_valid_tool_name(name)) {
      throw std::invalid_argument(
          "boundfin::toolchain_manifest::to_toolchain_versions_json: tool name does not match "
          "schema propertyNames pattern: " +
          name);
    }
    versions[name] = {{"version", identity.version}, {"sha256", identity.sha256}};
  }
  return versions;
}

nlohmann::json build_default_manifest(const std::filesystem::path &cxx_compiler, const std::string &openssl_version,
                                       const std::filesystem::path &openssl_crypto_library,
                                       const std::filesystem::path &repo_root) {
  std::map<std::string, ToolIdentity> identities;

  const auto probe_subprocess = [&identities](const std::string &name, const std::filesystem::path &executable,
                                               const std::vector<std::string> &version_args) {
    identities.emplace(name, identity_from_subprocess(executable, version_args));
  };

  const auto require_on_path = [](const std::string &name) {
    auto resolved = resolve_on_path(name);
    if (!resolved.has_value()) {
      throw std::runtime_error("boundfin::toolchain_manifest: required tool not found on PATH: " + name);
    }
    return *resolved;
  };

  probe_subprocess("cmake", require_on_path("cmake"), {"--version"});
  probe_subprocess("ninja", require_on_path("ninja"), {"--version"});
  probe_subprocess("cxx_compiler", cxx_compiler, {"--version"});
  probe_subprocess("node", require_on_path("node"), {"--version"});
  probe_subprocess("npm", require_on_path("npm"), {"--version"});

  identities.emplace("openssl_crypto", identity_from_versioned_file(openssl_version, openssl_crypto_library));

  const std::filesystem::path schema_validate_dir = repo_root / "tools" / "schema-validate";
  const std::filesystem::path package_lock_json = schema_validate_dir / "package-lock.json";
  for (const char *package : {"ajv-cli", "ajv", "ajv-formats"}) {
    identities.emplace(package, identity_from_npm_lockfile_entry(package_lock_json, package));
  }

  return to_toolchain_versions_json(identities);
}

DriftReport compare_toolchain_versions(const nlohmann::json &baseline, const nlohmann::json &current) {
  DriftReport report;

  std::set<std::string> all_names;
  for (const auto &entry : baseline.items()) {
    all_names.insert(entry.key());
  }
  for (const auto &entry : current.items()) {
    all_names.insert(entry.key());
  }

  std::vector<std::string> drifted;
  for (const auto &name : all_names) {
    const auto baseline_it = baseline.find(name);
    const auto current_it = current.find(name);
    if (baseline_it == baseline.end() || current_it == current.end() || *baseline_it != *current_it) {
      drifted.push_back(name);
    }
  }

  if (drifted.empty()) {
    return report;
  }

  report.status = DriftStatus::kDrift;
  report.diagnostic_codes = {"RUN014"};
  report.drifted_tools = std::move(drifted);
  return report;
}

} // namespace boundfin::toolchain_manifest
