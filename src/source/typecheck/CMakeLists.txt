add_library(boundfin_source_typecheck STATIC
  src/typecheck.cpp
)

target_include_directories(boundfin_source_typecheck PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_typecheck
  PUBLIC
    boundfin_source_ast
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
