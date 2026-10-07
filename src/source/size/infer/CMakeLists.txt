add_library(boundfin_source_size_infer STATIC
  src/infer.cpp
)

target_include_directories(boundfin_source_size_infer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_size_infer
  PUBLIC
    boundfin_source_ast
    boundfin_source_size_certificate
    boundfin_source_size_count
    boundfin_source_size_shape
  PRIVATE
    boundfin_warnings
    boundfin_sanitizers
)
