# Source-built public suite harness. Runtime output intentionally shares the
# configured tests directory used by the check preset.
add_executable(lynx-test-suites ${CMAKE_CURRENT_LIST_DIR}/test_public_suites.c)
target_include_directories(lynx-test-suites PRIVATE ${PROJECT_SOURCE_DIR}/core)
target_link_libraries(lynx-test-suites PRIVATE lynx_core)
target_compile_definitions(lynx-test-suites PRIVATE _POSIX_C_SOURCE=200809L)
lynx_warnings(lynx-test-suites)
lynx_sanitize(lynx-test-suites)
set_target_properties(lynx-test-suites PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

find_program(LYNX_SUITES_ENV_EXECUTABLE env REQUIRED)
function(lynx_suite_test name mode timeout)
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DMODE=${mode}
      -DROOT=${PROJECT_SOURCE_DIR}
      -DWORK=${LYNX_SCRATCH_DIR}/suites/${mode}
      -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}/suites
      -DTEST_BINARY=$<TARGET_FILE:lynx-test-suites>
      -DENV_EXECUTABLE=${LYNX_SUITES_ENV_EXECUTABLE}
      -P ${CMAKE_CURRENT_LIST_DIR}/run.cmake
    LABELS suite TIMEOUT ${timeout})
endfunction()

lynx_suite_test(suite.cpu.lynx1 cpu 60)
lynx_suite_test(suite.oracle_tracer tracer 60)
lynx_suite_test(suite.oracle_controls oracle_controls 60)
lynx_suite_test(suite.symbol_controls symbol_controls 60)
