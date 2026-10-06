add_library(boundfin_source_size_shape STATIC
  src/shape.cpp
)

target_include_directories(boundfin_source_size_shape PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_size_shape
  PUBLIC
    boundfin_source_ast
    boundfin_source_size_certificate
  PRIVATE
    boundfin_warnings
    boundfin_sanitizers
)
