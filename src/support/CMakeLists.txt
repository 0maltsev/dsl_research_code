add_library(boundfin_support STATIC
  src/sha256.cpp
  src/record_schema.cpp
)

target_include_directories(boundfin_support PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_support
  PUBLIC
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator::validator
  PRIVATE
    OpenSSL::Crypto
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
