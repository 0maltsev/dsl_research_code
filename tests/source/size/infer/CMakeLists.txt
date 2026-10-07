function(boundfin_add_infer_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_size_infer boundfin_source_size_count
                                         boundfin_source_size_shape boundfin_source_size_certificate
                                         boundfin_source_typecheck boundfin_source_resolve boundfin_source_parse
                                         boundfin_source_ast)
endfunction()

boundfin_add_infer_test(boundfin_infer_shape test_infer_shape.cpp)
