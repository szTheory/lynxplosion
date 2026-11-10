# Independent artifact parser. 03-09 registers its named integration and
# refusal controls after the consumer/public API contract is complete.
add_executable(lynx-check-artifacts ${CMAKE_CURRENT_LIST_DIR}/check_artifacts.c)
target_compile_definitions(lynx-check-artifacts PRIVATE _POSIX_C_SOURCE=200809L)
lynx_warnings(lynx-check-artifacts)
lynx_sanitize(lynx-check-artifacts)
set_target_properties(lynx-check-artifacts PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

# Phase 03-09 registers the exact planned IDs by calling this helper; keeping
# registration separate here preserves the Phase 03-08 inventory boundary.
function(lynx_consumer_test name scenario profile timeout)
  set(work "${LYNX_SCRATCH_DIR}/consumer/${name}")
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DMODE=consumer
      -DCASE=${scenario}
      -DPROFILE=${profile}
      -DROOT=${PROJECT_SOURCE_DIR}
      -DWORK=${work}
      -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}/consumer
      -DTEST_BINARY=$<TARGET_FILE:lynx-test-diag>
      -DRUNNER=$<TARGET_FILE:lynx-run>
      -DENV_EXECUTABLE=${LYNX_ENV_EXECUTABLE}
      -DCONSUMER=$<TARGET_FILE:lynx-consume>
      -DARTIFACT_CHECKER=$<TARGET_FILE:lynx-check-artifacts>
      -P "${LYNX_DIAG_DRIVER}"
    LABELS consumer TIMEOUT ${timeout})
endfunction()
