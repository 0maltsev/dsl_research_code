function(boundfin_add_shape_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_size_shape boundfin_source_size_certificate
                                         boundfin_source_typecheck boundfin_source_resolve boundfin_source_parse
                                         boundfin_source_ast)
endfunction()

boundfin_add_shape_test(boundfin_shape_scalar_var test_shape_scalar_var.cpp)
boundfin_add_shape_test(boundfin_shape_product_proj test_shape_product_proj.cpp)
boundfin_add_shape_test(boundfin_shape_join test_shape_join.cpp)
boundfin_add_shape_test(boundfin_shape_literal test_shape_literal.cpp)
boundfin_add_shape_test(boundfin_shape_conditional test_shape_conditional.cpp)
boundfin_add_shape_test(boundfin_shape_len_index test_shape_len_index.cpp)
boundfin_add_shape_test(boundfin_shape_capshape test_shape_capshape.cpp)
boundfin_add_shape_test(boundfin_shape_builder test_shape_builder.cpp)
boundfin_add_shape_test(boundfin_shape_fold test_shape_fold.cpp)
boundfin_add_shape_test(boundfin_shape_abi_param test_shape_abi_param.cpp)
