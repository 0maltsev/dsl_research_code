add_library(boundfin_toolchain_manifest STATIC
  src/manifest.cpp
)

target_include_directories(boundfin_toolchain_manifest PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_toolchain_manifest
  PUBLIC
    nlohmann_json::nlohmann_json
  PRIVATE
    boundfin_support
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)

configure_file(
  tool/config.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/boundfin/toolchain_manifest/config.hpp
  @ONLY
)

add_executable(boundfin-toolchain-manifest tool/main.cpp)
target_include_directories(boundfin-toolchain-manifest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(boundfin-toolchain-manifest PRIVATE
  boundfin_toolchain_manifest
  boundfin_warnings
  boundfin_sanitizers
)
