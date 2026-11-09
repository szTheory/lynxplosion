# Determinism tests: the runner is driven as separate fresh processes.
set(LYNX_SKELETON_CONTENT ${PROJECT_SOURCE_DIR}/fixtures/skeleton/skeleton-image.hex)

lynx_add_test(NAME det.two_runs
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DWORK=${LYNX_SCRATCH_DIR}/det/two_runs
    -P ${CMAKE_CURRENT_LIST_DIR}/two_runs.cmake
  LABELS det TIMEOUT 30)
