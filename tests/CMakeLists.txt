add_library(boundfin_test_harness INTERFACE)
target_include_directories(boundfin_test_harness INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Registers a standalone BoundFin test binary (see boundfin_test.hpp) and
# adds it to ctest under its own name. Every test links the support library
# so it can hash/validate without repeating that boilerplate per call site.
function(boundfin_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE
    boundfin_support
    boundfin_test_harness
    boundfin_warnings
    boundfin_sanitizers
  )
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_subdirectory(support)
add_subdirectory(toolchain_manifest)
add_subdirectory(source/lex)
add_subdirectory(source/parse)
add_subdirectory(source/resolve)
add_subdirectory(source/typecheck)
add_subdirectory(source/size/certificate)

# Dev-only Draft 2020-12 meta-schema conformance check for schemas/*.schema.json.
# Runs Node + ajv (pinned in tools/schema-validate/); it is not part of the
# production C++ pipeline. See tools/schema-validate/check-meta-schema.cjs
# and docs/dependency-policy.md for why a JS tool is used for this one check.
find_program(BOUNDFIN_NODE_EXECUTABLE node REQUIRED)
add_test(
  NAME schema_meta_validation_draft_2020_12
  COMMAND ${BOUNDFIN_NODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/schema-validate/check-meta-schema.cjs
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools/schema-validate
)
