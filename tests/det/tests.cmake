# Determinism tests: the runner is driven as separate fresh processes.
# Workload: fixtures/cpu/churn-loop (a never-ending loop of opcodes both Lynx
# profiles define identically, reading seeded RAM) under profile none.
set(LYNX_CPU_LOOP_CONTENT ${PROJECT_SOURCE_DIR}/fixtures/cpu/churn-loop.hex)

# Workload values (set in plan 02-09 for the CPU loop).
# LYNX_DET_CONTENT_SHA256: SHA-256 of the accepted image bytes (det.two_runs).
# LYNX_DET_FLIP_*: the one place a byte of the image is changed for the content
#   sensitivity check; FROM must occur exactly once in the image text. The flip
#   changes the LDY data byte, so ADC ($40),Y reads another seeded byte (not an
#   opcode byte, so the digest changes for the intended reason).
# LYNX_DET_TRAP_*: the image with a stopping opcode planted (det.manifest_fields:
#   FROM must occur exactly once), and the ticks consumed and stop reason a run of
#   it must report. LYNX_DET_CLI_TRAP_*: the one-instruction image runner.cli
#   runs with the largest budget, and its expected ticks and stop reason.
set(LYNX_DET_CONTENT_SHA256 dcee7216a88ff2ffaf3e4a2794c3369676fc47493d710f26cd8bbadd19745b77)
set(LYNX_DET_FLIP_FROM "a0 08")
set(LYNX_DET_FLIP_TO "a0 09")
# NOP then the bit-op slot $07: one 2-cycle instruction (10 ticks at flat5),
# then profile none stops before the $07.
set(LYNX_DET_TRAP_FROM "a2 00 a0 08")
set(LYNX_DET_TRAP_TO "ea 07 a0 08")
set(LYNX_DET_TRAP_TICKS 10)
set(LYNX_DET_TRAP_STOP undefined)
set(LYNX_DET_CLI_TRAP_HEX 07)
set(LYNX_DET_CLI_TRAP_TICKS 0)
set(LYNX_DET_CLI_TRAP_STOP undefined)

lynx_add_test(NAME det.two_runs
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
    -DWORK=${LYNX_SCRATCH_DIR}/det/two_runs
    -P ${CMAKE_CURRENT_LIST_DIR}/two_runs.cmake
  LABELS det TIMEOUT 30)

lynx_add_test(NAME runner.cli
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
    -DTRAP_HEX=${LYNX_DET_CLI_TRAP_HEX}
    -DTRAP_TICKS=${LYNX_DET_CLI_TRAP_TICKS}
    -DTRAP_STOP=${LYNX_DET_CLI_TRAP_STOP}
    -DC_COMPILER=${CMAKE_C_COMPILER}
    -DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
    -DCORE_LIBRARY=$<TARGET_FILE:lynx_core_plain>
    -DWORK=${LYNX_SCRATCH_DIR}/det/runner_cli
    -P ${CMAKE_CURRENT_LIST_DIR}/runner_cli.cmake
  LABELS runner TIMEOUT 30)

# In-process determinism: two instances in one process, whole bundles compared,
# and the committed cross-compiler golden. Host side (stdio, mkdir), so the test
# executable defines _POSIX_C_SOURCE itself in its source file.
set(LYNX_CPU_LOOP_GOLDEN ${PROJECT_SOURCE_DIR}/tests/golden/skeleton.digest)

add_executable(lynx-test-det ${CMAKE_CURRENT_LIST_DIR}/test_det.c)
target_link_libraries(lynx-test-det PRIVATE lynx_bundle)
# check.h lives in tests/; core/internal.h is white-box use by state_sensitivity only.
target_include_directories(lynx-test-det PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-det)
lynx_sanitize(lynx-test-det)
set_target_properties(lynx-test-det PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

# Build the existing source-authored A/V guest with the same pinned recipe as
# consumer.av_tracer. The deterministic path tests consume its BLL and linked
# probe_result symbol directly rather than maintaining a second guest image.
set(LYNX_AV_PROBE_SOURCE ${PROJECT_SOURCE_DIR}/fixtures/diagnostic/av_probe.s)
set(LYNX_AV_PROBE_CONFIG ${PROJECT_SOURCE_DIR}/fixtures/diagnostic/av_probe.cfg)
set(LYNX_AV_PROBE_MANIFEST ${PROJECT_SOURCE_DIR}/fixtures/manifests/av-probe.manifest)
set(LYNX_CC65_HOME ${PROJECT_SOURCE_DIR}/build/pins/cc65)
set(LYNX_CA65 ${LYNX_CC65_HOME}/bin/ca65)
set(LYNX_LD65 ${LYNX_CC65_HOME}/bin/ld65)
if(NOT EXISTS ${LYNX_CA65} OR NOT EXISTS ${LYNX_LD65} OR
   NOT EXISTS ${LYNX_CC65_HOME}/.lynx-build-files)
  message(FATAL_ERROR "det.audio_paths requires the pinned ca65/ld65 build used by consumer.av_tracer")
endif()
set(LYNX_AV_PROBE_DIR ${PROJECT_BINARY_DIR}/generated/av-probe)
set(LYNX_AV_PROBE_OBJECT ${LYNX_AV_PROBE_DIR}/av_probe.o)
set(LYNX_AV_PROBE_BLL ${LYNX_AV_PROBE_DIR}/av_probe.bll)
set(LYNX_AV_PROBE_LABELS ${LYNX_AV_PROBE_DIR}/av_probe.lbl)
add_custom_command(OUTPUT ${LYNX_AV_PROBE_BLL} ${LYNX_AV_PROBE_LABELS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${LYNX_AV_PROBE_DIR}
  COMMAND ${CMAKE_COMMAND} -E env "PATH=/usr/bin:/bin" "CC65_HOME=${LYNX_CC65_HOME}"
    ${LYNX_CA65} --cpu 65SC02 -o ${LYNX_AV_PROBE_OBJECT} ${LYNX_AV_PROBE_SOURCE}
  COMMAND ${CMAKE_COMMAND} -E env "PATH=/usr/bin:/bin" "CC65_HOME=${LYNX_CC65_HOME}"
    ${LYNX_LD65} -C ${LYNX_AV_PROBE_CONFIG} -Ln ${LYNX_AV_PROBE_LABELS}
    -o ${LYNX_AV_PROBE_BLL} ${LYNX_AV_PROBE_OBJECT}
  COMMAND ${CMAKE_COMMAND} -E rm -f ${LYNX_AV_PROBE_OBJECT}
  DEPENDS ${LYNX_AV_PROBE_SOURCE} ${LYNX_AV_PROBE_CONFIG} ${LYNX_AV_PROBE_MANIFEST}
    ${PROJECT_SOURCE_DIR}/pins.txt ${LYNX_CC65_HOME}/.lynx-build-files
  VERBATIM)
add_custom_target(lynx-det-av-probe-fixture DEPENDS ${LYNX_AV_PROBE_BLL} ${LYNX_AV_PROBE_LABELS})
add_dependencies(lynx-test-det lynx-det-av-probe-fixture)

lynx_add_test(NAME det.golden
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
    -DGOLDEN=${LYNX_CPU_LOOP_GOLDEN}
    -DWORK=${LYNX_SCRATCH_DIR}/det/golden
    -P ${CMAKE_CURRENT_LIST_DIR}/golden.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.golden.self_test
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
    -DGOLDEN=${LYNX_CPU_LOOP_GOLDEN}
    -DWORK=${LYNX_SCRATCH_DIR}/det/golden_self_test
    -DSCRIPTS=${PROJECT_SOURCE_DIR}/tests/cmake
    -P ${CMAKE_CURRENT_LIST_DIR}/golden_self_test.cmake
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.two_instances
  COMMAND lynx-test-det two_instances ${LYNX_CPU_LOOP_CONTENT} ${LYNX_CPU_LOOP_GOLDEN}
    ${LYNX_SCRATCH_DIR}/det/two_instances
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.sensitivity
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
    "-DFLIP_FROM=${LYNX_DET_FLIP_FROM}"
    "-DFLIP_TO=${LYNX_DET_FLIP_TO}"
    -DWORK=${LYNX_SCRATCH_DIR}/det/sensitivity
    -P ${CMAKE_CURRENT_LIST_DIR}/sensitivity.cmake
  LABELS det TIMEOUT 60)

# The golden workload is profile-independent: none, lynx1 and lynx2 give the same
# registers, RAM, guest time and observation digest (the committed golden).
lynx_add_test(NAME det.profile_agreement
  COMMAND lynx-test-det profile_agreement ${LYNX_CPU_LOOP_CONTENT} ${LYNX_CPU_LOOP_GOLDEN}
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.sensitivity_state
  COMMAND lynx-test-det state_sensitivity ${LYNX_CPU_LOOP_CONTENT}
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.timer_paths
  COMMAND lynx-test-det timer_paths
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.video_paths
  COMMAND lynx-test-det video_paths
  LABELS det TIMEOUT 120)

lynx_add_test(NAME det.video_sensitivity
  COMMAND lynx-test-det video_sensitivity
  LABELS det TIMEOUT 60)

lynx_add_test(NAME det.audio_paths
  COMMAND lynx-test-det audio_paths ${LYNX_AV_PROBE_BLL} ${LYNX_AV_PROBE_LABELS}
    ${LYNX_AV_PROBE_MANIFEST}
  LABELS det TIMEOUT 300)

lynx_add_test(NAME det.audio_sensitivity
  COMMAND lynx-test-det audio_sensitivity ${LYNX_AV_PROBE_BLL} ${LYNX_AV_PROBE_LABELS}
    ${LYNX_AV_PROBE_MANIFEST}
  LABELS det TIMEOUT 120)

# The strict key = value validator, over real runner manifests and over built
# faulty ones. Both tests run under /bin/sh through the CMake-found shell.
find_program(LYNX_SH sh REQUIRED)
lynx_add_test(NAME det.manifest_fields
  COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:lynx-run>
    -DCONTENT=${LYNX_CPU_LOOP_CONTENT}
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
