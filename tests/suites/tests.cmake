# Source-built public suite harness. Runtime output intentionally shares the
# configured tests directory used by the check preset.
add_executable(lynx-test-suites ${CMAKE_CURRENT_LIST_DIR}/test_public_suites.c)
target_include_directories(lynx-test-suites PRIVATE ${PROJECT_SOURCE_DIR}/core)
target_link_libraries(lynx-test-suites PRIVATE lynx_core)
target_compile_definitions(lynx-test-suites PRIVATE
  _POSIX_C_SOURCE=200809L
  LYNX_PROJECT_ROOT="${PROJECT_SOURCE_DIR}")
lynx_warnings(lynx-test-suites)
lynx_sanitize(lynx-test-suites)
set_target_properties(lynx-test-suites PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

find_program(LYNX_SUITES_ENV_EXECUTABLE env REQUIRED)
function(lynx_suite_test name mode timeout)
  set(profile "")
  if(ARGC GREATER 3)
    set(profile "${ARGV3}")
  endif()
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DMODE=${mode}
      -DPROFILE=${profile}
      -DROOT=${PROJECT_SOURCE_DIR}
      -DWORK=${LYNX_SCRATCH_DIR}/suites/${name}
      -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}/suites
      -DTEST_BINARY=$<TARGET_FILE:lynx-test-suites>
      -DENV_EXECUTABLE=${LYNX_SUITES_ENV_EXECUTABLE}
      -P ${CMAKE_CURRENT_LIST_DIR}/run.cmake
    LABELS suite TIMEOUT ${timeout})
endfunction()

lynx_suite_test(suite.cpu.lynx1 cpu 60 lynx1)
lynx_suite_test(suite.cpu.lynx2 cpu 60 lynx2)
lynx_suite_test(suite.cpu.model_controls cpu_model_controls 60)
lynx_suite_test(suite.cpu.bound_controls cpu_bound_controls 60)
lynx_suite_test(suite.oracle_tracer tracer 60)
lynx_suite_test(suite.oracle_controls oracle_controls 60)
lynx_suite_test(suite.symbol_controls symbol_controls 60)
lynx_suite_test(suite.timers.observation.lynx1 timer_observation 60 lynx1)
lynx_suite_test(suite.timers.observation.lynx2 timer_observation 60 lynx2)
lynx_suite_test(suite.timers.strict_comparison_controls timer_strict_controls 150)
lynx_suite_test(suite.timers.cascade_model timer_cascade_model 150)
