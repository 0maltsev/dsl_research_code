add_library(boundfin_source_size_count STATIC
  src/count.cpp
)

target_include_directories(boundfin_source_size_count PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_size_count
  PUBLIC
    boundfin_source_ast
    boundfin_source_size_certificate
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
