function(boundfin_add_typecheck_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_typecheck boundfin_source_resolve boundfin_source_parse
                                         boundfin_source_ast)
endfunction()

boundfin_add_typecheck_test(boundfin_typecheck_literals_and_vars test_typecheck_literals_and_vars.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_primitives test_typecheck_primitives.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_let_if test_typecheck_let_if.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_products test_typecheck_products.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_arrays test_typecheck_arrays.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_fold_build test_typecheck_fold_build.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_call_and_result_type test_typecheck_call_and_result_type.cpp)
boundfin_add_typecheck_test(boundfin_typecheck_full_module test_typecheck_full_module.cpp)
