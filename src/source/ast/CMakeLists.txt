add_library(boundfin_source_ast STATIC
  src/serialize.cpp
  src/print.cpp
)

target_include_directories(boundfin_source_ast PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_ast
  PUBLIC
    boundfin_source_lex
    nlohmann_json::nlohmann_json
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
