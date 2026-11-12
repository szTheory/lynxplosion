# Independent artifact parser and publication-control executable. The latter
# is compiled from the real consumer source with test-only fault injection; the
# shipped lynx-consume target has no test controls.
add_executable(lynx-check-artifacts ${CMAKE_CURRENT_LIST_DIR}/check_artifacts.c)
target_compile_definitions(lynx-check-artifacts PRIVATE _POSIX_C_SOURCE=200809L)
lynx_warnings(lynx-check-artifacts)
lynx_sanitize(lynx-check-artifacts)
set_target_properties(lynx-check-artifacts PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

add_executable(lynx-consume-test ${PROJECT_SOURCE_DIR}/consumer/main.c)
target_link_libraries(lynx-consume-test PRIVATE lynx_core)
target_compile_definitions(lynx-consume-test PRIVATE
  _POSIX_C_SOURCE=200809L LYNX_CONSUMER_TESTING=1)
lynx_warnings(lynx-consume-test)
lynx_sanitize(lynx-consume-test)
set_target_properties(lynx-consume-test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

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
      -DTEST_CONSUMER=$<TARGET_FILE:lynx-consume-test>
      -DARTIFACT_CHECKER=$<TARGET_FILE:lynx-check-artifacts>
      -DC_COMPILER=${CMAKE_C_COMPILER}
      -DPUBLIC_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
      -P "${LYNX_DIAG_DRIVER}"
    LABELS consumer TIMEOUT ${timeout})
endfunction()

lynx_consumer_test(consumer.public_api public_api none 60)
function(lynx_frame_test name scenario timeout)
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${scenario}
      -DROOT=${PROJECT_SOURCE_DIR}
      -DWORK=${LYNX_SCRATCH_DIR}/consumer/${name}
      -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}/consumer
      -DCONSUMER=$<TARGET_FILE:lynx-consume>
      -DARTIFACT_CHECKER=$<TARGET_FILE:lynx-check-artifacts>
      -DENV_EXECUTABLE=${LYNX_ENV_EXECUTABLE}
      -P ${CMAKE_CURRENT_LIST_DIR}/frame_driver.cmake
    LABELS consumer TIMEOUT ${timeout})
endfunction()
lynx_frame_test(consumer.frame_replay frame_replay 240)
lynx_frame_test(consumer.frame_tracer frame_tracer 120)
lynx_frame_test(consumer.replay_edges replay_edges 120)
lynx_frame_test(consumer.replay_restore replay_restore 120)
lynx_consumer_test(consumer.snapshot_ppm snapshot_ppm none 60)
lynx_consumer_test(consumer.silence_wav silence_wav none 60)
lynx_consumer_test(consumer.refusal_controls refusal_controls none 90)
