# Pinned third-party dependencies for BoundFin Track C support code.
#
# Each is fetched as the GitHub source archive for one exact upstream commit
# (not a floating tag, branch, or a full git clone) and checked against a
# recorded SHA-256 of that archive. This is both faster than a git clone
# (no history, no other branches/tags) and a strictly stronger identity
# record than a bare commit hash, consistent with this repository's
# SHA-256-based provenance convention. See docs/dependency-policy.md.

include(FetchContent)

# nlohmann/json 3.12.0 (tag v3.12.0, commit 65ee68451d8eb2b5f3a30b410476ab83deb3289b)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/archive/65ee68451d8eb2b5f3a30b410476ab83deb3289b.tar.gz
  URL_HASH SHA256=13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f
  SYSTEM
)

# pboettch/json-schema-validator 2.4.0 (tag 2.4.0, commit 55b49c221b41c8369342d4d23e52d9f31119c848)
set(JSON_VALIDATOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_INSTALL OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  nlohmann_json_schema_validator
  URL https://github.com/pboettch/json-schema-validator/archive/55b49c221b41c8369342d4d23e52d9f31119c848.tar.gz
  URL_HASH SHA256=3d611d7d829197a9083688e0399f75f562a547cc7859ceb6dd4f51bfeaa18d63
  SYSTEM
)

FetchContent_MakeAvailable(nlohmann_json nlohmann_json_schema_validator)
