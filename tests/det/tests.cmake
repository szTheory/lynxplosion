# Determinism tests: the runner is driven as separate fresh processes.
set(LYNX_SKELETON_CONTENT ${PROJECT_SOURCE_DIR}/fixtures/skeleton/skeleton-image.hex)

# workload values; plan 02-09 changes only these.
# LYNX_DET_CONTENT_SHA256: SHA-256 of the accepted image bytes (det.two_runs).
# LYNX_DET_FLIP_*: the one place a byte of the image is changed for the content
#   sensitivity check; FROM must occur exactly once in the image text.
# LYNX_DET_TRAP_*: the image with a stopping opcode planted (det.manifest_fields:
#   FROM must occur exactly once), and the ticks consumed and stop reason a run of
#   it must report. LYNX_DET_CLI_TRAP_*: the one-instruction image runner.cli
#   runs with the largest budget, and its expected ticks and stop reason.
set(LYNX_DET_CONTENT_SHA256 ab5f8b5cb9435354c7b58603592d5faf081e17ceb05f7a7c67f4b666f12ca457)
set(LYNX_DET_FLIP_FROM "03 0a")
set(LYNX_DET_FLIP_TO "04 0a")
set(LYNX_DET_TRAP_FROM "03 0a 11 18")
set(LYNX_DET_TRAP_TO "03 0a 11 db")
set(LYNX_DET_TRAP_TICKS 12)
set(LYNX_DET_TRAP_STOP halt)
set(LYNX_DET_CLI_TRAP_HEX db)
set(LYNX_DET_CLI_TRAP_TICKS 0)
set(LYNX_DET_CLI_TRAP_STOP halt)

lynx_add_test(NAME det.two_runs
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DCONTENT_SHA256=${LYNX_DET_CONTENT_SHA256}
    -DWORK=${LYNX_SCRATCH_DIR}/det/two_runs
    -P ${CMAKE_CURRENT_LIST_DIR}/two_runs.cmake
  LABELS det TIMEOUT 30)

lynx_add_test(NAME runner.cli
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    -DTRAP_HEX=${LYNX_DET_CLI_TRAP_HEX}
    -DTRAP_TICKS=${LYNX_DET_CLI_TRAP_TICKS}
    -DTRAP_STOP=${LYNX_DET_CLI_TRAP_STOP}
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
    "-DFLIP_FROM=${LYNX_DET_FLIP_FROM}"
    "-DFLIP_TO=${LYNX_DET_FLIP_TO}"
    -DWORK=${LYNX_SCRATCH_DIR}/det/sensitivity
    -P ${CMAKE_CURRENT_LIST_DIR}/sensitivity.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.sensitivity_state
  COMMAND lynx-test-det state_sensitivity ${LYNX_SKELETON_CONTENT}
  LABELS det TIMEOUT 60)

# The strict key = value validator, over real runner manifests and over built
# faulty ones. Both tests run under /bin/sh through the CMake-found shell.
find_program(LYNX_SH sh REQUIRED)
lynx_add_test(NAME det.manifest_fields
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_SKELETON_CONTENT}
    "-DTRAP_FROM=${LYNX_DET_TRAP_FROM}"
    "-DTRAP_TO=${LYNX_DET_TRAP_TO}"
    -DTRAP_TICKS=${LYNX_DET_TRAP_TICKS}
    -DTRAP_STOP=${LYNX_DET_TRAP_STOP}
    -DWORK=${LYNX_SCRATCH_DIR}/det/manifest_fields
    -DSH=${LYNX_SH}
    -DVALIDATE=${PROJECT_SOURCE_DIR}/scripts/validate-kv.sh
    -P ${CMAKE_CURRENT_LIST_DIR}/manifest_fields.cmake
  LABELS det TIMEOUT 30)

lynx_add_test(NAME det.manifest_fields.self_test
  COMMAND ${LYNX_SH} ${CMAKE_CURRENT_LIST_DIR}/manifest_controls.sh
    ${LYNX_SCRATCH_DIR}/det/manifest_controls
  LABELS det TIMEOUT 30)
