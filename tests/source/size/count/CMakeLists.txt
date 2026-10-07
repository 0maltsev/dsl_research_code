function(boundfin_add_count_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_size_count boundfin_source_size_certificate
                                         boundfin_source_size_shape boundfin_source_typecheck
                                         boundfin_source_resolve boundfin_source_parse boundfin_source_ast)
endfunction()

boundfin_add_count_test(boundfin_count_const test_count_const.cpp)
boundfin_add_count_test(boundfin_count_if test_count_if.cpp)
boundfin_add_count_test(boundfin_count_add_sub test_count_add_sub.cpp)
boundfin_add_count_test(boundfin_count_idx test_count_idx.cpp)
boundfin_add_count_test(boundfin_count_admissibility test_count_admissibility.cpp)
boundfin_add_count_test(boundfin_count_module_admissibility test_count_module_admissibility.cpp)
boundfin_add_count_test(boundfin_count_len test_count_len.cpp)
boundfin_add_count_test(boundfin_count_abi_param test_count_abi_param.cpp)
