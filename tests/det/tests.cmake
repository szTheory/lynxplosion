# Determinism tests: the runner is driven as separate fresh processes.
set(LYNX_SKELETON_CONTENT ${PROJECT_SOURCE_DIR}/fixtures/skeleton/skeleton-image.hex)

lynx_add_test(NAME det.two_runs
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DWORK=${LYNX_SCRATCH_DIR}/det/two_runs
    -P ${CMAKE_CURRENT_LIST_DIR}/two_runs.cmake
  LABELS det TIMEOUT 30)

lynx_add_test(NAME runner.cli
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DWORK=${LYNX_SCRATCH_DIR}/det/runner_cli
    -P ${CMAKE_CURRENT_LIST_DIR}/runner_cli.cmake
  LABELS runner TIMEOUT 30)

# In-process determinism: two instances in one process, whole bundles compared,
# and the committed cross-compiler golden. Host side (stdio, mkdir), so the test
# executable defines _POSIX_C_SOURCE itself in its source file.
set(LYNX_SKELETON_GOLDEN ${PROJECT_SOURCE_DIR}/tests/golden/skeleton.digest)

add_executable(lynx-test-det ${CMAKE_CURRENT_LIST_DIR}/test_det.c)
target_link_libraries(lynx-test-det PRIVATE lynx_bundle)
# check.h lives in tests/; core/internal.h is white-box use by state_sensitivity only.
target_include_directories(lynx-test-det PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-det)
lynx_sanitize(lynx-test-det)
set_target_properties(lynx-test-det PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

lynx_add_test(NAME det.golden
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DGOLDEN=${LYNX_SKELETON_GOLDEN}
    -DWORK=${LYNX_SCRATCH_DIR}/det/golden
    -P ${CMAKE_CURRENT_LIST_DIR}/golden.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.golden.self_test
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DGOLDEN=${LYNX_SKELETON_GOLDEN}
    -DWORK=${LYNX_SCRATCH_DIR}/det/golden_self_test
    -DSCRIPTS=${PROJECT_SOURCE_DIR}/tests/cmake
    -P ${CMAKE_CURRENT_LIST_DIR}/golden_self_test.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.two_instances
  COMMAND lynx-test-det two_instances ${LYNX_SKELETON_CONTENT} ${LYNX_SKELETON_GOLDEN}
    ${LYNX_SCRATCH_DIR}/det/two_instances
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.sensitivity
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DWORK=${LYNX_SCRATCH_DIR}/det/sensitivity
    -P ${CMAKE_CURRENT_LIST_DIR}/sensitivity.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.sensitivity_state
  COMMAND lynx-test-det state_sensitivity ${LYNX_SKELETON_CONTENT}
  LABELS det TIMEOUT 60)
