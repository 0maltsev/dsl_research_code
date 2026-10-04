#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace boundfin::toolchain_manifest {

// One entry of a `toolchain_versions` record
// (compiler-certificate.schema.json#/properties/toolchain_versions): a free
// version string plus the SHA-256 of the exact file that identity was taken
// from.
struct ToolIdentity {
  std::string version;
  std::string sha256;
};

// Resolves `name` on PATH the way a shell would (first match, in PATH
// order). `name` must not itself contain a path separator.
[[nodiscard]] std::optional<std::filesystem::path> resolve_on_path(const std::string &name);

// Runs `executable version_args...` with no shell involved, with stderr
// merged into stdout, and returns the captured output with trailing
// whitespace trimmed. Throws std::runtime_error if the process cannot be
// started, is terminated by a signal, or exits nonzero.
[[nodiscard]] std::string run_and_capture(const std::filesystem::path &executable,
                                           const std::vector<std::string> &version_args);

// Extracts the first dotted-decimal version token (two or more
// period-separated numeral groups, e.g. "22.1.6") found in free-form
// `--version` output. Throws std::invalid_argument if none is found.
[[nodiscard]] std::string extract_first_version_token(const std::string &version_output);

// Runs `executable version_args...`, extracts its version token, and hashes
// `executable`'s own bytes: the identity of a build-toolchain binary.
[[nodiscard]] ToolIdentity identity_from_subprocess(const std::filesystem::path &executable,
                                                     const std::vector<std::string> &version_args);

// Reads `package_name`'s resolved entry from an npm `package-lock.json`
// (its `node_modules/<package_name>` object: `version`, `resolved`,
// `integrity`) and hashes the compact JSON serialization of
// {"integrity": ..., "resolved": ...}: the identity of a pinned, lockfile-
// addressed dev-only npm package (used for the Draft 2020-12 meta-schema
// checker in tools/schema-validate/, which has no reliable `--version` CLI
// surface). Hashing `resolved`+`integrity` rather than just the installed
// package's own `package.json` detects a tarball/content change that a
// `package.json` version bump would not catch. nlohmann::json's default
// object type sorts keys, so this hash does not depend on field order.
// Throws std::runtime_error if the file cannot be read or has no matching,
// complete entry.
[[nodiscard]] ToolIdentity identity_from_npm_lockfile_entry(const std::filesystem::path &package_lock_json_path,
                                                             const std::string &package_name);

// Pairs a version string already known by the caller (e.g. from a CMake
// `find_package` result, such as `OPENSSL_VERSION`) with the SHA-256 of
// `identity_file`'s bytes (e.g. the resolved library `find_package` linked
// against): the identity of a build-time dependency that has no `--version`
// CLI surface of its own. Throws std::filesystem::filesystem_error if
// `identity_file` cannot be read.
[[nodiscard]] ToolIdentity identity_from_versioned_file(const std::string &version,
                                                         const std::filesystem::path &identity_file);

// Serializes `identities` (keyed by tool name) as a `toolchain_versions`
// object matching compiler-certificate.schema.json. nlohmann::json's default
// object type sorts keys, so the result's dump() does not depend on
// insertion order. Throws std::invalid_argument if a name does not match the
// schema's propertyNames pattern "^[A-Za-z][A-Za-z0-9_.-]*$".
[[nodiscard]] nlohmann::json to_toolchain_versions_json(const std::map<std::string, ToolIdentity> &identities);

// Builds this host's full Phase-1 toolchain_versions record: cmake, ninja,
// the configured C++ compiler, the OpenSSL/libcrypto build dependency, node
// and npm, and the dev-only schema meta-validation packages pinned under
// tools/schema-validate/. `cxx_compiler` is the compiler CMake configured
// the build with (CMAKE_CXX_COMPILER), not rediscovered from PATH, since the
// two can differ. `openssl_version`/`openssl_crypto_library` are CMake's
// `find_package(OpenSSL)` results (OPENSSL_VERSION/OPENSSL_CRYPTO_LIBRARY),
// baked in at configure time rather than rediscovered, for the same reason.
// `repo_root` is this repository's root, used to find
// tools/schema-validate/{package-lock.json,node_modules/}. Throws
// std::runtime_error naming the tool if any entry cannot be resolved, run,
// or read.
[[nodiscard]] nlohmann::json build_default_manifest(const std::filesystem::path &cxx_compiler,
                                                      const std::string &openssl_version,
                                                      const std::filesystem::path &openssl_crypto_library,
                                                      const std::filesystem::path &repo_root);

enum class DriftStatus { kMatch, kDrift };

struct DriftReport {
  DriftStatus status = DriftStatus::kMatch;
  std::vector<std::string> diagnostic_codes; // "RUN014" once, iff status == kDrift
  std::vector<std::string> drifted_tools;    // names present in the drift, sorted
};

// Compares a previously recorded `toolchain_versions` object against a
// freshly built one of the same shape. A tool whose version or sha256
// differs, or that is present in exactly one of the two maps, counts as
// drift: RUN014, "checksum_failure", "Artifact/input/output checksum or
// identity drift" (diagnostics-and-status.md).
[[nodiscard]] DriftReport compare_toolchain_versions(const nlohmann::json &baseline,
                                                      const nlohmann::json &current);

} // namespace boundfin::toolchain_manifest
