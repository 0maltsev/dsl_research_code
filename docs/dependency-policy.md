# Dependency policy

This is the Phase 1 "dependency policy" deliverable (`PLAN.md`, Phase 1). It
records every third-party dependency the Track C support code uses, why each
one exists, and the exact identity pinned for it. "Exact" means a specific
upstream commit or an exact package version with a lockfile -- never a
floating tag, branch, or version range -- because version drift is Phase 1's
named scientific risk (`PLAN.md`, Phase 1, "Scientific risks").

## C++ production dependencies (fetched by `cmake/Dependencies.cmake`)

These are built from source via CMake `FetchContent`, fetched as the GitHub
source archive for one exact upstream commit (never a floating tag, branch,
or a full git clone) and checked against a recorded SHA-256 of that archive.
An archive-by-commit plus a content hash is both faster than a git clone
(nlohmann/json's full repository history is hundreds of MB; its
one-commit archive is under 10 MB) and a strictly stronger identity record
than a bare commit hash, consistent with this repository's SHA-256-based
provenance convention elsewhere. Each is linked privately where possible;
both are used from `src/support/record_schema.cpp` to validate generated
records (compiler certificates, correctness results, benchmark samples,
experiment manifests) against `schemas/*.schema.json`.

| Dependency | Pinned tag | Pinned commit | Archive SHA-256 | Role |
|---|---|---|---|---|
| [`nlohmann/json`](https://github.com/nlohmann/json) | `v3.12.0` | `65ee68451d8eb2b5f3a30b410476ab83deb3289b` | `13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f` | JSON value type used throughout the support and toolchain-manifest libraries. |
| [`pboettch/json-schema-validator`](https://github.com/pboettch/json-schema-validator) | `2.4.0` | `55b49c221b41c8369342d4d23e52d9f31119c848` | `3d611d7d829197a9083688e0399f75f562a547cc7859ceb6dd4f51bfeaa18d63` | Validates a generated record (a JSON instance) against one of this repository's own schemas. Supports `$ref`/`$defs`/`propertyNames`/`pattern`/`oneOf`/`const`/etc., which is everything our schemas use. It does **not** implement `$dynamicRef`/`$dynamicAnchor`, so it must not be used to check whether a schema *document* conforms to the Draft 2020-12 meta-schema (see below). |

Both archive SHA-256 values were computed locally (`sha256sum`) immediately
after download, from `https://github.com/<repo>/archive/<commit>.tar.gz`,
and match what `cmake/Dependencies.cmake`'s `URL_HASH` enforces: a mismatch
fails the configure step rather than silently using different bytes.

## System library dependency

| Dependency | Observed version (this host) | Role |
|---|---|---|
| OpenSSL (`libcrypto`, found via `find_package(OpenSSL REQUIRED COMPONENTS Crypto)`) | `3.6.2` (Arch package `openssl`) | SHA-256 digests (`src/support/sha256.cpp`), via the EVP digest interface rather than a from-scratch implementation, to avoid the correctness risk of hand-rolled cryptographic code. `find_package(OpenSSL)` runs at the top-level `CMakeLists.txt`, before `add_subdirectory(src)`, so `OPENSSL_VERSION`/`OPENSSL_CRYPTO_LIBRARY` are baked into `src/toolchain_manifest/tool/config.hpp.in` and recorded as every manifest's `openssl_crypto` entry: `{"version": OPENSSL_VERSION, "sha256": sha256_hex_of_file(OPENSSL_CRYPTO_LIBRARY)}`, the same `{version, sha256}` shape as every other tool, not folded into any other entry. |

## Dev-only, non-production tool: Draft 2020-12 meta-schema conformance

`AGENTS.md`/`CLAUDE.md` restrict Python to `analysis/`, and no maintained
C++ JSON Schema library available here correctly implements the Draft
2020-12 meta-schema (which is self-referential via `$dynamicRef`/
`$dynamicAnchor`; `pboettch/json-schema-validator` 2.4.0 does not implement
either keyword). The author approved (2026-10-04, in response to the
recommended option) using an external, non-Python CLI tool for this one
narrow, infrequent check, on the same precedent as WABT's `wasm-validate`
(an external, non-C++, non-Python tool recorded in the toolchain manifest
and used later for Wasm validation).

`tools/schema-validate/` pins:

| Package | Exact version | Role |
|---|---|---|
| `ajv` | `8.20.0` (resolved transitively; see `package-lock.json`) | The only locally available implementation with real `$dynamicRef`/`$dynamicAnchor` support (`ajv/dist/2020`, the `Ajv2020` class), and it ships the official 2020-12 meta-schema built in. |
| `ajv-cli` | `5.0.0` | Pinned entry point for the package; its CLI does not expose the 2020-12 dialect (`--spec=` only lists `draft7`/`draft2019`), so the actual check is `tools/schema-validate/check-meta-schema.cjs`, which uses the `ajv` library's `Ajv2020` class directly instead of the `ajv` CLI binary. |
| `ajv-formats` | `3.0.1` | Registers the `format` validators (e.g. `date-time`, used by `created_at`/`timestamp` fields) that `ajv` does not ship by default; without it, Ajv's strict mode reports every used format as unrecognized. |

`npm audit` reports a high-severity prototype-pollution advisory in
`fast-json-patch` (an `ajv-cli` dependency used for its patch/merge
subcommands, which `check-meta-schema.cjs` does not call). Recorded here
rather than silently fixed with `--force`, which would pull a breaking
`ajv-cli@0.6.0` downgrade: this tool runs locally, offline, only against
this repository's own static, trusted schema files, so the exposure from
that advisory is not applicable to how it is used here.

`node_modules/` is not committed (`.gitignore`); `package.json` and
`package-lock.json` are, so `npm ci` reproduces the exact pinned tree. The
toolchain manifest's `ajv`/`ajv-cli`/`ajv-formats` entries hash the compact
JSON serialization of each package's `package-lock.json` `resolved`+
`integrity` fields (via `identity_from_npm_lockfile_entry`), not the
installed package's own `package.json`: a `package.json` alone only proves
the declared version string, while `resolved`+`integrity` is npm's own
content-addressed identity for the exact tarball that was installed, so a
tarball/content change is caught as drift even without a version bump.

## Verification-only tool, not a repository dependency: `actionlint`

`.github/workflows/ci.yml` (Phase 1.2, "CI smoke checks") was checked with
[`actionlint`](https://github.com/rhysd/actionlint) `v1.7.12`
(`actionlint_1.7.12_linux_amd64.tar.gz`, SHA-256
`8aca8db96f1b94770f1b0d72b6dddcb1ebb8123cb3712530b08cc387b349a3d8`, matching
the release's own published digest) before that workflow was committed.
`actionlint` was downloaded to a scratch directory and run once; it is not
installed system-wide, not added as a repository dependency, and not part of
any committed toolchain manifest, since it verifies a workflow file's syntax
and does not run as part of the build, test, or CI pipeline itself, and
gates nothing retained as evidence. It is therefore not a scientific choice
under `AGENTS.md`'s decision-log rule, unlike `DEC-014`/`DEC-015` or the
author-approved choice of `ajv` above; it is recorded here only for the same
install/version transparency `CLAUDE.md` §6 asks for every development tool.
No container runtime (Docker/Podman) was available on this host to actually
execute the workflow locally (e.g. via `act`); `actionlint`'s static check,
plus running the workflow's own command sequence directly (`npm ci` from a
clean `node_modules/`, `cmake --preset dev`, `cmake --build --preset dev`,
`ctest --preset dev`, `.claude/hooks/check-snapshot.sh session`), is the
verification evidence for this milestone instead.

## Rule

Every dependency added to this repository, in any language, is pinned to an
exact version (and for git-fetched C++ dependencies, an exact commit), is
recorded in this file with its role, and is reflected in the relevant
toolchain manifest or lockfile. A floating version range or branch
reference is not permitted.
