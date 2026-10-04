add_library(boundfin_source_lex STATIC
  src/lexer.cpp
)

target_include_directories(boundfin_source_lex PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_lex
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
