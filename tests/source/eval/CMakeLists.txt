function(boundfin_add_eval_test name)
  boundfin_add_test(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE boundfin_source_eval boundfin_source_ast)
endfunction()

boundfin_add_eval_test(boundfin_eval_store_reachability test_eval_store_reachability.cpp)
boundfin_add_eval_test(boundfin_eval_var_const test_eval_var_const.cpp)
