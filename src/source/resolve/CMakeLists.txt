add_library(boundfin_source_resolve STATIC
  src/resolver.cpp
)

target_include_directories(boundfin_source_resolve PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_resolve
  PUBLIC
    boundfin_source_ast
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
