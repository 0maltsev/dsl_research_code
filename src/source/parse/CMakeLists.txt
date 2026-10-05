add_library(boundfin_source_parse STATIC
  src/parser.cpp
)

target_include_directories(boundfin_source_parse PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_parse
  PUBLIC
    boundfin_source_ast
    boundfin_source_lex
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
