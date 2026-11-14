# Source-built public suite harness. Runtime output intentionally shares the
# configured tests directory used by the check preset.
add_executable(lynx-test-suites ${CMAKE_CURRENT_LIST_DIR}/test_public_suites.c)
target_include_directories(lynx-test-suites PRIVATE ${PROJECT_SOURCE_DIR}/core)
target_link_libraries(lynx-test-suites PRIVATE lynx_core)
target_compile_definitions(lynx-test-suites PRIVATE
  _POSIX_C_SOURCE=200809L
  _XOPEN_SOURCE=700
  LYNX_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
  LYNX_SCRATCH_ROOT="${LYNX_SCRATCH_DIR}/suites")
lynx_warnings(lynx-test-suites)
lynx_sanitize(lynx-test-suites)
set_target_properties(lynx-test-suites PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

find_program(LYNX_SUITES_ENV_EXECUTABLE env REQUIRED)
function(lynx_suite_test name mode timeout)
  set(profile "")
  set(suite "")
  if(ARGC GREATER 3)
    set(profile "${ARGV3}")
  endif()
  if(ARGC GREATER 4)
    set(suite "${ARGV4}")
  endif()
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DMODE=${mode}
      -DPROFILE=${profile}
      -DSUITE=${suite}
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
lynx_suite_test(suite.cpu.no_run cpu_no_run 240)
lynx_suite_test(suite.completion_controls completion_controls 300)
lynx_suite_test(suite.selection_controls selection_controls 30)
lynx_suite_test(suite.oracle_tracer tracer 60)
lynx_suite_test(suite.oracle_controls oracle_controls 60)
lynx_suite_test(suite.symbol_controls symbol_controls 60)
lynx_suite_test(suite.audio.observation.lynx1 audio_observation 180 lynx1 audio)
lynx_suite_test(suite.audio.observation.lynx2 audio_observation 180 lynx2 audio)
lynx_suite_test(suite.audio2.observation.lynx1 audio_observation 180 lynx1 audio2)
lynx_suite_test(suite.audio2.observation.lynx2 audio_observation 180 lynx2 audio2)
lynx_suite_test(suite.audio.controls audio_controls 240 "" audio)
lynx_suite_test(suite.audio.source_controls audio_source_controls 240)
lynx_suite_test(suite.audio2.controls audio2_controls 240 "" audio2)
lynx_suite_test(suite.math.observation.lynx1 math_memio_observation 180 lynx1 math)
lynx_suite_test(suite.page_mode.observation.lynx1 page_mode_observation 300 lynx1)
lynx_suite_test(suite.page_mode.observation.lynx2 page_mode_observation 300 lynx2)
lynx_suite_test(suite.page_mode.controls page_mode_controls 300)
set_tests_properties(suite.page_mode.observation.lynx1 suite.page_mode.observation.lynx2 PROPERTIES SKIP_REGULAR_EXPRESSION "^LYNX-SKIP: page-mode source requires unavailable SERCTL write at")
lynx_suite_test(suite.timers.observation.lynx1 timer_observation 60 lynx1)
lynx_suite_test(suite.timers.observation.lynx2 timer_observation 60 lynx2)
lynx_suite_test(suite.timers.strict_comparison_controls timer_strict_controls 150)
lynx_suite_test(suite.timers.cascade_model timer_cascade_model 150)
lynx_suite_test(suite.timers.model_controls timer_model_controls 90)
lynx_suite_test(suite.timers.pending_controls timer_pending_controls 300)
lynx_suite_test(suite.timers.result_controls timer_result_controls 300)
lynx_suite_test(suite.timers.video_revisit timer_video_revisit 300)
lynx_suite_test(suite.timers.video_revisit_controls timer_video_revisit_controls 300)
lynx_suite_test(suite.timers.no_run timers_no_run 300)

# Privately built source timer package evidence (separate from the public suite
# driver and its existing inventory).
add_executable(lynx-test-timer-evidence
  ${PROJECT_SOURCE_DIR}/tests/timer-evidence/test_timer_evidence.c)
set(timer_identity_file "${PROJECT_SOURCE_DIR}/fixtures/timer-evidence/images.sha256")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${timer_identity_file}")
file(SHA256 "${timer_identity_file}" timer_case_set_sha256)
file(STRINGS "${timer_identity_file}" timer_identity_lines REGEX "^[^#]")
set(timer_identity_header "#define TIMER_CASE_SET_SHA256 \"${timer_case_set_sha256}\"\nstatic const struct { const char *name; const char *sha256; } timer_approved_images[] = {\n")
set(timer_identity_names)
foreach(identity_line IN LISTS timer_identity_lines)
  if(NOT identity_line MATCHES "^([0-9a-f]+)  ((hardware|software)-dma[01]-page[01])\\.bll$")
    message(FATAL_ERROR "timer evidence: invalid approved image identity")
  endif()
  string(LENGTH "${CMAKE_MATCH_1}" identity_sha_length)
  if(NOT identity_sha_length EQUAL 64)
    message(FATAL_ERROR "timer evidence: invalid image digest width")
  endif()
  list(APPEND timer_identity_names "${CMAKE_MATCH_2}")
  string(APPEND timer_identity_header "{\"${CMAKE_MATCH_2}\", \"${CMAKE_MATCH_1}\"},\n")
endforeach()
list(REMOVE_DUPLICATES timer_identity_names)
list(LENGTH timer_identity_names timer_identity_count)
list(LENGTH timer_identity_lines timer_identity_line_count)
if(NOT timer_identity_count EQUAL 8 OR NOT timer_identity_line_count EQUAL 8)
  message(FATAL_ERROR "timer evidence: expected eight unique approved images")
endif()
string(APPEND timer_identity_header "};\n")
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/timer_evidence_ids.h" CONTENT "${timer_identity_header}" @ONLY)
target_include_directories(lynx-test-timer-evidence PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(lynx-test-timer-evidence PRIVATE lynx_core)
target_compile_definitions(lynx-test-timer-evidence PRIVATE
  _POSIX_C_SOURCE=200809L
  _XOPEN_SOURCE=700
  LYNX_PROJECT_ROOT="${PROJECT_SOURCE_DIR}")
lynx_warnings(lynx-test-timer-evidence)
lynx_sanitize(lynx-test-timer-evidence)
set_target_properties(lynx-test-timer-evidence PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

foreach(evidence_mode tracer matrix controls)
lynx_add_test(NAME timer_evidence.${evidence_mode}
  COMMAND ${CMAKE_COMMAND}
    -DMODE=${evidence_mode}
    -DROOT=${PROJECT_SOURCE_DIR}
    -DWORK=${LYNX_SCRATCH_DIR}/timer-evidence/${evidence_mode}
    -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}
    -DTEST_BINARY=$<TARGET_FILE:lynx-test-timer-evidence>
    -P ${PROJECT_SOURCE_DIR}/tests/timer-evidence/run.cmake
  LABELS timer TIMEOUT 60)
endforeach()
