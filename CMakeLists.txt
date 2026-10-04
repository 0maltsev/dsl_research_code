cmake_minimum_required(VERSION 3.25)

project(boundfin
  VERSION 0.1.0
  DESCRIPTION "BoundFin research artifact: Track C support code"
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-tree builds are not permitted; configure into an out-of-tree build directory.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BOUNDFIN_BUILD_TESTS "Build the BoundFin test suite" ON)
option(BOUNDFIN_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)
option(BOUNDFIN_WARNINGS_AS_ERRORS "Treat warnings in BoundFin's own code as errors" ON)

# No DSL behaviour exists yet: this tree only builds the Track C support
# library (hashing, schema validation) and the toolchain-manifest emitter.

add_library(boundfin_warnings INTERFACE)
target_compile_options(boundfin_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  $<$<BOOL:${BOUNDFIN_WARNINGS_AS_ERRORS}>:-Werror>
)

add_library(boundfin_numerics INTERFACE)
target_compile_options(boundfin_numerics INTERFACE
  -ffp-contract=off
)

if(BOUNDFIN_ENABLE_SANITIZERS)
  add_library(boundfin_sanitizers INTERFACE)
  target_compile_options(boundfin_sanitizers INTERFACE
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
  )
  target_link_options(boundfin_sanitizers INTERFACE
    -fsanitize=address,undefined
  )
else()
  add_library(boundfin_sanitizers INTERFACE)
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(BOUNDFIN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
