function(boundfin_add_shape_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_size_shape boundfin_source_size_certificate
                                         boundfin_source_typecheck boundfin_source_resolve boundfin_source_parse
                                         boundfin_source_ast)
endfunction()

boundfin_add_shape_test(boundfin_shape_scalar_var test_shape_scalar_var.cpp)
