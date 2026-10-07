add_library(boundfin_source_eval STATIC
  src/eval.cpp
)

target_include_directories(boundfin_source_eval PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_eval
  PUBLIC
    boundfin_source_ast
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
