add_library(boundfin_source_size_certificate STATIC
  src/certificate.cpp
)

target_include_directories(boundfin_source_size_certificate PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(boundfin_source_size_certificate
  PRIVATE
    boundfin_warnings
    boundfin_numerics
    boundfin_sanitizers
)
