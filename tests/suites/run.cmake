cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
if(DEFINED TIMER_SUBCASE_MANIFEST_CHECK)
  foreach(required_manifest EXPECTED_MANIFEST ACTUAL_MANIFEST)
    if(NOT DEFINED ${required_manifest} OR "${${required_manifest}}" STREQUAL "")
      message(FATAL_ERROR "suite_driver: ${required_manifest} is required for manifest validation")
    endif()
  endforeach()
  file(STRINGS "${EXPECTED_MANIFEST}" expected_names)
  file(STRINGS "${ACTUAL_MANIFEST}" actual_names)
  set(seen_names "")
  foreach(name IN LISTS actual_names)
    list(FIND seen_names "${name}" duplicate_index)
    if(NOT duplicate_index EQUAL -1)
      message(FATAL_ERROR "suite_driver: timer subcase manifest duplicate name: ${name}")
    endif()
    list(APPEND seen_names "${name}")
  endforeach()
  foreach(name IN LISTS expected_names)
    list(FIND actual_names "${name}" actual_index)
    if(actual_index EQUAL -1)
      message(FATAL_ERROR "suite_driver: timer subcase manifest missing name: ${name}")
    endif()
  endforeach()
  if(NOT actual_names STREQUAL expected_names)
    message(FATAL_ERROR "suite_driver: timer subcase manifest has unexpected names or order")
  endif()
  message(STATUS "timer subcase manifest: ${actual_names} unique names valid")
  return()
endif()
function(assert_no_symlink_ancestors path_name path_value)
  if(NOT IS_ABSOLUTE "${path_value}")
    message(FATAL_ERROR "suite_driver: ${path_name} must be absolute")
  endif()
  set(cursor "${path_value}")
  while(TRUE)
    if(IS_SYMLINK "${cursor}")
      message(FATAL_ERROR "suite_driver: refusing symlink ancestor in ${path_name}")
    endif()
    get_filename_component(parent "${cursor}" DIRECTORY)
    if(parent STREQUAL cursor)
      break()
    endif()
    set(cursor "${parent}")
  endwhile()
endfunction()

function(cleanup_suite_run directory root nonce)
  assert_no_symlink_ancestors("cleanup root" "${root}")
  assert_no_symlink_ancestors("cleanup directory" "${directory}")
  file(REAL_PATH "${root}" real_root)
  file(REAL_PATH "${directory}" real_directory)
  if(NOT IS_DIRECTORY "${directory}" OR NOT real_root STREQUAL root OR
     NOT real_directory STREQUAL directory)
    message(FATAL_ERROR "suite_driver: cleanup path is not a canonical directory")
  endif()
  cmake_path(IS_PREFIX real_root "${real_directory}" NORMALIZE cleanup_owned)
  get_filename_component(run_name "${real_directory}" NAME)
  string(LENGTH "${nonce}" nonce_length)
  if(NOT cleanup_owned OR real_directory STREQUAL real_root OR
     NOT nonce_length EQUAL 48 OR NOT nonce MATCHES "^[0-9a-f]+$" OR
     NOT run_name STREQUAL "run-${nonce}")
    message(FATAL_ERROR "suite_driver: cleanup is outside the exact invocation run")
  endif()
  set(marker "${real_directory}/.lynx-suite-owner")
  if(NOT EXISTS "${marker}" OR IS_DIRECTORY "${marker}" OR IS_SYMLINK "${marker}")
    message(FATAL_ERROR "suite_driver: cleanup owner marker missing or invalid")
  endif()
  file(SIZE "${marker}" marker_size)
  if(marker_size GREATER 8192)
    message(FATAL_ERROR "suite_driver: cleanup owner marker mismatch")
  endif()
  file(READ "${marker}" owner)
  if(NOT owner STREQUAL "lynx-suite-output-v1\n${real_directory}\n${nonce}\n")
    message(FATAL_ERROR "suite_driver: cleanup owner marker mismatch")
  endif()
  # Only this freshly revalidated, nonce-named invocation directory is removed.
  # Symlink children are unlinked; the negative control preserves their targets.
  file(REMOVE_RECURSE "${real_directory}")
  if(EXISTS "${real_directory}" OR IS_SYMLINK "${real_directory}")
    message(FATAL_ERROR "suite_driver: cannot clean owned invocation directory")
  endif()
endfunction()

if(DEFINED SUITE_CLEANUP_CHECK)
  foreach(required_cleanup SCRATCH_ROOT CLEANUP_DIRECTORY CLEANUP_NONCE)
    if(NOT DEFINED ${required_cleanup} OR "${${required_cleanup}}" STREQUAL "")
      message(FATAL_ERROR "suite_driver: ${required_cleanup} required for cleanup control")
    endif()
  endforeach()
  cleanup_suite_run("${CLEANUP_DIRECTORY}" "${SCRATCH_ROOT}" "${CLEANUP_NONCE}")
  message(STATUS "suite cleanup: exact owned invocation removed")
  return()
endif()

foreach(required MODE ROOT WORK SCRATCH_ROOT TEST_BINARY ENV_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "suite_driver: -D${required} is required")
  endif()
endforeach()
if(NOT MODE MATCHES "^(cpu|cpu_no_run|timers_no_run|completion_controls|selection_controls|cpu_model_controls|cpu_bound_controls|tracer|oracle_controls|symbol_controls|timer_observation|timer_video_revisit|timer_video_revisit_controls|timer_strict_controls|timer_cascade_model|timer_model_controls|timer_result_controls|timer_pending_controls|page_mode_observation|page_mode_controls|audio_observation|audio_controls|audio2_controls|audio_source_controls)$")
  message(FATAL_ERROR "suite_driver: unknown mode")
endif()
if(MODE STREQUAL "cpu" AND NOT PROFILE MATCHES "^(lynx1|lynx2)$")
  message(FATAL_ERROR "suite_driver: explicit CPU PROFILE is required")
endif()
if(MODE STREQUAL "page_mode_observation" AND NOT PROFILE MATCHES "^(lynx1|lynx2)$")
  message(FATAL_ERROR "suite_driver: explicit page-mode PROFILE is required")
endif()
if(MODE STREQUAL "audio_observation" AND (NOT PROFILE MATCHES "^(lynx1|lynx2)$" OR NOT SUITE MATCHES "^(audio|audio2)$"))
  message(FATAL_ERROR "suite_driver: explicit audio SUITE and PROFILE are required")
endif()
if((MODE STREQUAL "audio_controls" AND NOT SUITE STREQUAL "audio") OR
   (MODE STREQUAL "audio2_controls" AND NOT SUITE STREQUAL "audio2"))
  message(FATAL_ERROR "suite_driver: audio control mode and SUITE do not match")
endif()

function(write_suite_owner_marker directory)
  assert_no_symlink_ancestors("output directory" "${directory}")
  file(REAL_PATH "${directory}" canonical_directory)
  if(NOT canonical_directory STREQUAL "${directory}")
    message(FATAL_ERROR "suite_driver: output directory is not canonical")
  endif()
  cmake_path(IS_PREFIX SCRATCH_ROOT "${canonical_directory}" NORMALIZE is_owned)
  if(NOT is_owned OR canonical_directory STREQUAL "${SCRATCH_ROOT}")
    message(FATAL_ERROR "suite_driver: output directory is outside invocation-owned scratch")
  endif()
  set(marker "${directory}/.lynx-suite-owner")
  if(IS_SYMLINK "${marker}")
    message(FATAL_ERROR "suite_driver: refusing symlink output owner marker")
  endif()
  if(EXISTS "${marker}")
    file(REMOVE "${marker}")
  endif()
  file(WRITE "${marker}" "lynx-suite-output-v1\n${canonical_directory}\n${SUITE_RUN_NONCE}\n")
endfunction()

if(NOT IS_ABSOLUTE "${ROOT}" OR NOT IS_ABSOLUTE "${SCRATCH_ROOT}" OR NOT IS_ABSOLUTE "${WORK}")
  message(FATAL_ERROR "suite_driver: ROOT, SCRATCH_ROOT and WORK must be absolute")
endif()
assert_no_symlink_ancestors("ROOT" "${ROOT}")
assert_no_symlink_ancestors("SCRATCH_ROOT" "${SCRATCH_ROOT}")
assert_no_symlink_ancestors("WORK" "${WORK}")
file(REAL_PATH "${ROOT}" ROOT_CANONICAL)
if(NOT ROOT_CANONICAL STREQUAL "${ROOT}")
  message(FATAL_ERROR "suite_driver: ROOT is not canonical")
endif()
file(MAKE_DIRECTORY "${SCRATCH_ROOT}")
file(REAL_PATH "${SCRATCH_ROOT}" SCRATCH_CANONICAL)
if(NOT SCRATCH_CANONICAL STREQUAL "${SCRATCH_ROOT}")
  message(FATAL_ERROR "suite_driver: SCRATCH_ROOT is not canonical")
endif()
set(SCRATCH_ROOT "${SCRATCH_CANONICAL}")
get_filename_component(WORK_PARENT "${WORK}" DIRECTORY)
get_filename_component(WORK_BASE_NAME "${WORK}" NAME)
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK_PARENT}" NORMALIZE work_parent_owned)
if(NOT work_parent_owned AND NOT WORK_PARENT STREQUAL SCRATCH_ROOT)
  message(FATAL_ERROR "suite_driver: WORK parent is outside invocation-owned scratch")
endif()
file(MAKE_DIRECTORY "${WORK_PARENT}")
file(REAL_PATH "${WORK_PARENT}" WORK_PARENT_CANONICAL)
if(NOT WORK_PARENT_CANONICAL STREQUAL "${WORK_PARENT}")
  message(FATAL_ERROR "suite_driver: WORK parent is not canonical")
endif()
set(WORK_BASE "${WORK_PARENT_CANONICAL}/${WORK_BASE_NAME}")
if(WORK_BASE STREQUAL SCRATCH_ROOT)
  message(FATAL_ERROR "suite_driver: refusing scratch root as WORK")
endif()
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK_BASE}" NORMALIZE work_is_owned)
if(NOT work_is_owned)
  message(FATAL_ERROR "suite_driver: refusing output outside invocation-owned scratch")
endif()
if(IS_SYMLINK "${WORK_BASE}" OR (EXISTS "${WORK_BASE}" AND NOT IS_DIRECTORY "${WORK_BASE}"))
  message(FATAL_ERROR "suite_driver: WORK base must be a real directory")
endif()
file(MAKE_DIRECTORY "${WORK_BASE}")
string(RANDOM LENGTH 48 ALPHABET 0123456789abcdef SUITE_RUN_NONCE)
set(WORK "${WORK_BASE}/run-${SUITE_RUN_NONCE}")
if(EXISTS "${WORK}" OR IS_SYMLINK "${WORK}")
  message(FATAL_ERROR "suite_driver: unique run output path already exists")
endif()
file(MAKE_DIRECTORY "${WORK}")
write_suite_owner_marker("${WORK}")
file(MAKE_DIRECTORY "${WORK}/tmp" "${WORK}/build")
write_suite_owner_marker("${WORK}/build")
if(MODE MATCHES "^page_mode")
  set(SUITE_BUILD "${WORK}/page-mode-build")
  set(ORACLE "${WORK}/page-mode-oracle.tsv")
else()
  set(SUITE_BUILD "${WORK}/build")
  set(ORACLE "${WORK}/build/oracle.tsv")
  set(AUDIO_ORACLE "${WORK}/build/audio-oracle.tsv")
endif()
set(FAILURE "")

function(expect_failure expected)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "1" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: control expected exit 1 and exact diagnostic '${expected}', got ${rc}: ${combined}")
  endif()
endfunction()

function(expect_failure_timeout expected timeout)
  execute_process(COMMAND ${ARGN} TIMEOUT ${timeout}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "1" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: control expected exit1 with '${expected}' within ${timeout}s, got ${rc}: ${combined}")
  endif()
endfunction()

function(expect_success expected)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "0" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: control expected success and '${expected}', got ${rc}: ${combined}")
  endif()
endfunction()

function(expect_success_timeout expected timeout)
  execute_process(COMMAND ${ARGN} TIMEOUT ${timeout}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "0" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: control expected success within ${timeout}s and '${expected}', got ${rc}: ${combined}")
  endif()
endfunction()

function(run_audio_no_run_control suite profile)
  execute_process(
    COMMAND "${TEST_BINARY}" no-run audio "${suite}" "${profile}" "${SUITE_BUILD}" "${ORACLE}" "${AUDIO_ORACLE}"
    TIMEOUT 120 RESULT_VARIABLE observe_rc OUTPUT_VARIABLE observe_out ERROR_VARIABLE observe_err)
  if(NOT observe_rc STREQUAL "0" OR
     NOT observe_out MATCHES "audio no-run control: held guest did not execute; same instance released for resumed completion" OR
     NOT observe_out MATCHES "audio observation complete: suite=${suite} profile=${profile}" OR
     NOT observe_out MATCHES "source_rows=18 source_agreements=18 model=${profile}")
    message(FATAL_ERROR "suite_driver: audio no-run/resume control failed for ${suite}/${profile}: ${observe_rc} ${observe_out} ${observe_err}")
  endif()
  set(raw "${SUITE_BUILD}/raw-results-${suite}-${profile}.tsv")
  set(comparison "${SUITE_BUILD}/comparison-${suite}-${profile}.tsv")
  set(record "${SUITE_BUILD}/run-record-${suite}-${profile}.txt")
  if(NOT EXISTS "${raw}" OR NOT EXISTS "${comparison}" OR NOT EXISTS "${record}")
    message(FATAL_ERROR "suite_driver: audio no-run/resume control omitted output records for ${suite}/${profile}")
  endif()
  file(STRINGS "${raw}" raw_rows REGEX "^result\t[0-9]+\t[0-9a-f][0-9a-f]$")
  file(STRINGS "${comparison}" comparison_rows REGEX "^result\t[0-9]+\t")
  list(LENGTH raw_rows raw_count)
  list(LENGTH comparison_rows comparison_count)
  if(NOT raw_count EQUAL 18 OR NOT comparison_count EQUAL 18)
    message(FATAL_ERROR "suite_driver: audio no-run/resume control lost exact 18-row evidence for ${suite}/${profile}: raw=${raw_count} comparison=${comparison_count}")
  endif()
  foreach(row IN LISTS comparison_rows)
    if(NOT row MATCHES "source-accepted$")
      message(FATAL_ERROR "suite_driver: audio no-run/resume control found source mismatch: ${row}")
    endif()
  endforeach()
  file(READ "${record}" record_text)
  if(NOT record_text MATCHES "meaningful_count=18" OR
     NOT record_text MATCHES "source_agreements=18" OR
     NOT record_text MATCHES "completion_sentinel=a5" OR
     NOT record_text MATCHES "profile_qualification=unresolved")
    message(FATAL_ERROR "suite_driver: audio no-run/resume control record is incomplete for ${suite}/${profile}")
  endif()
  expect_success("audio record: complete 18-row capture, source and build identities valid for ${suite}/${profile}"
    "${TEST_BINARY}" validate-audio-record "${suite}" "${profile}" "${SUITE_BUILD}"
    "${raw}" "${record}" "${ORACLE}" "${AUDIO_ORACLE}")
endfunction()

function(audio_assert_baseline_unchanged suite profile)
  set(base "${WORK}/audio-baseline/${suite}/${profile}")
  set(manifest "${WORK}/audio-baseline/${suite}/${profile}.sha256")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "suite_driver: audio baseline identity manifest is missing for ${suite}/${profile}")
  endif()
  file(STRINGS "${manifest}" entries)
  list(LENGTH entries entry_count)
  if(NOT entry_count EQUAL 10)
    message(FATAL_ERROR "suite_driver: audio baseline manifest is incomplete for ${suite}/${profile}")
  endif()
  foreach(entry IN LISTS entries)
    if(NOT entry MATCHES "^([0-9a-f]+)  ([^ ]+)$")
      message(FATAL_ERROR "suite_driver: malformed audio baseline identity entry: ${entry}")
    endif()
    set(expected_sha "${CMAKE_MATCH_1}")
    set(file_name "${CMAKE_MATCH_2}")
    if(NOT EXISTS "${base}/${file_name}")
      message(FATAL_ERROR "suite_driver: immutable audio baseline file disappeared: ${suite}/${profile}/${file_name}")
    endif()
    file(SHA256 "${base}/${file_name}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
      message(FATAL_ERROR "suite_driver: immutable audio baseline changed: ${suite}/${profile}/${file_name}")
    endif()
  endforeach()
endfunction()

function(audio_batch_positive suite)
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/audio-baseline/${suite}/${profile}")
    audio_assert_baseline_unchanged("${suite}" "${profile}")
    expect_success("audio record: complete 18-row capture, source and build identities valid for ${suite}/${profile}"
      "${TEST_BINARY}" validate-audio-record "${suite}" "${profile}" "${base}"
      "${base}/raw-results-${suite}-${profile}.tsv"
      "${base}/run-record-${suite}-${profile}.txt"
      "${base}/oracle.tsv" "${base}/audio-oracle.tsv")
    audio_assert_baseline_unchanged("${suite}" "${profile}")
  endforeach()
endfunction()

function(audio_batch_begin suite)
  run_audio_no_run_control("${suite}" lynx1)
  run_audio_no_run_control("${suite}" lynx2)
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/audio-baseline/${suite}/${profile}")
    if(EXISTS "${base}" OR IS_SYMLINK "${base}")
      message(FATAL_ERROR "suite_driver: audio baseline directory already exists for ${suite}/${profile}")
    endif()
    file(MAKE_DIRECTORY "${base}")
    file(COPY "${SUITE_BUILD}/" DESTINATION "${base}")
    write_suite_owner_marker("${base}")
    set(manifest "${WORK}/audio-baseline/${suite}/${profile}.sha256")
    file(WRITE "${manifest}" "")
    foreach(file_name
        "raw-results-${suite}-${profile}.tsv" "run-record-${suite}-${profile}.txt"
        "comparison-${suite}-${profile}.tsv" "audio.bll" "audio.lbl" "audio2.bll" "audio2.lbl"
        "build-record.txt" "oracle.tsv" "audio-oracle.tsv")
      if(NOT EXISTS "${base}/${file_name}")
        message(FATAL_ERROR "suite_driver: fresh audio baseline is incomplete: ${suite}/${profile}/${file_name}")
      endif()
      file(SHA256 "${base}/${file_name}" file_sha)
      file(APPEND "${manifest}" "${file_sha}  ${file_name}\n")
    endforeach()
  endforeach()
  set_property(GLOBAL PROPERTY AUDIO_CONTROL_CASE_COUNT 0)
  set_property(GLOBAL PROPERTY AUDIO_CONTROL_CASE_NAMES "")
  audio_batch_positive("${suite}")
endfunction()

function(audio_case_begin case_name suite profile)
  audio_batch_positive("${suite}")
  get_property(case_names GLOBAL PROPERTY AUDIO_CONTROL_CASE_NAMES)
  if(case_names STREQUAL "")
    set(case_names "")
  endif()
  list(FIND case_names "${case_name}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "suite_driver: duplicate audio data-control case: ${case_name}")
  endif()
  list(APPEND case_names "${case_name}")
  set_property(GLOBAL PROPERTY AUDIO_CONTROL_CASE_NAMES "${case_names}")
  get_property(case_count GLOBAL PROPERTY AUDIO_CONTROL_CASE_COUNT)
  if(case_count STREQUAL "")
    set(case_count 0)
  endif()
  math(EXPR case_count "${case_count} + 1")
  set_property(GLOBAL PROPERTY AUDIO_CONTROL_CASE_COUNT "${case_count}")
  string(MAKE_C_IDENTIFIER "${case_name}" case_slug)
  set(case_dir "${WORK}/audio-cases/${case_count}-${case_slug}")
  if(EXISTS "${case_dir}" OR IS_SYMLINK "${case_dir}")
    message(FATAL_ERROR "suite_driver: audio private mutation directory already exists")
  endif()
  file(MAKE_DIRECTORY "${case_dir}")
  file(COPY "${WORK}/audio-baseline/${suite}/${profile}/" DESTINATION "${case_dir}")
  write_suite_owner_marker("${case_dir}")
  set(AUDIO_CASE_DIR "${case_dir}" PARENT_SCOPE)
endfunction()

function(audio_case_end suite)
  audio_assert_baseline_unchanged("${suite}" lynx1)
  audio_assert_baseline_unchanged("${suite}" lynx2)
  audio_batch_positive("${suite}")
endfunction()

function(audio_expect_record_failure expected suite profile directory)
  expect_failure("${expected}" "${TEST_BINARY}" validate-audio-record "${suite}" "${profile}"
    "${directory}" "${directory}/raw-results-${suite}-${profile}.tsv"
    "${directory}/run-record-${suite}-${profile}.txt" "${directory}/oracle.tsv"
    "${directory}/audio-oracle.tsv")
endfunction()

function(audio_record_mutation case_name suite profile key replacement expected)
  audio_case_begin("${case_name}" "${suite}" "${profile}")
  set(record "${AUDIO_CASE_DIR}/run-record-${suite}-${profile}.txt")
  file(STRINGS "${record}" matching_rows REGEX "^${key}=")
  list(LENGTH matching_rows matching_count)
  if(NOT matching_count EQUAL 1)
    message(FATAL_ERROR "suite_driver: audio record mutation did not find exactly one ${key} field")
  endif()
  list(GET matching_rows 0 original_row)
  file(READ "${record}" record_text)
  if(replacement STREQUAL "__REMOVE__")
    string(REPLACE "${original_row}\n" "" changed_record "${record_text}")
  else()
    string(REPLACE "${original_row}\n" "${key}=${replacement}\n" changed_record "${record_text}")
  endif()
  if(changed_record STREQUAL record_text)
    message(FATAL_ERROR "suite_driver: audio record mutation did not alter ${key}")
  endif()
  file(WRITE "${record}" "${changed_record}")
  audio_expect_record_failure("${expected}" "${suite}" "${profile}" "${AUDIO_CASE_DIR}")
  audio_case_end("${suite}")
endfunction()

function(audio_mutate_ticks_record record_path ticks)
  file(READ "${record_path}" record_text)
  if(NOT record_text MATCHES "ticks_observed=([0-9]+)")
    message(FATAL_ERROR "suite_driver: audio tick mutation source lacks ticks_observed")
  endif()
  set(old_ticks "${CMAKE_MATCH_1}")
  string(REPLACE "ticks_observed=${old_ticks}" "ticks_observed=${ticks}" changed "${record_text}")
  if(ticks MATCHES "^[0-9]+$" AND ticks GREATER_EQUAL 256 AND ticks LESS_EQUAL 50000000)
    math(EXPR margin "${ticks} - 256")
    math(EXPR headroom "50000000 - ${ticks}")
    string(REPLACE "guest_ticks=${old_ticks}" "guest_ticks=${ticks}" changed "${changed}")
    string(REGEX REPLACE "completion_margin_ticks=[0-9]+" "completion_margin_ticks=${margin}" changed "${changed}")
    string(REGEX REPLACE "maximum_headroom_ticks=[0-9]+" "maximum_headroom_ticks=${headroom}" changed "${changed}")
  endif()
  file(WRITE "${record_path}" "${changed}")
endfunction()

function(audio_tick_case case_name suite ticks expected success)
  audio_case_begin("${case_name}" "${suite}" lynx1)
  set(record "${AUDIO_CASE_DIR}/run-record-${suite}-lynx1.txt")
  audio_mutate_ticks_record("${record}" "${ticks}")
  if(success)
    expect_success("audio record: complete 18-row capture, source and build identities valid for ${suite}/lynx1"
      "${TEST_BINARY}" validate-audio-record "${suite}" lynx1 "${AUDIO_CASE_DIR}"
      "${AUDIO_CASE_DIR}/raw-results-${suite}-lynx1.tsv" "${record}"
      "${AUDIO_CASE_DIR}/oracle.tsv" "${AUDIO_CASE_DIR}/audio-oracle.tsv")
  else()
    audio_expect_record_failure("${expected}" "${suite}" lynx1 "${AUDIO_CASE_DIR}")
  endif()
  audio_case_end("${suite}")
endfunction()

function(audio_rows_case case_name suite profile operation expected)
  audio_case_begin("${case_name}" "${suite}" "${profile}")
  set(raw "${AUDIO_CASE_DIR}/raw-results-${suite}-${profile}.tsv")
  file(READ "${raw}" raw_text)
  if(operation STREQUAL "zero")
    string(REGEX REPLACE "result\t[0-9]+\t[0-9a-f][0-9a-f]\n" "" changed_raw "${raw_text}")
  elseif(operation STREQUAL "short")
    string(REPLACE "result\t17\t" "result\t17\t__removed__" changed_raw "${raw_text}")
    string(REGEX REPLACE "result\t17\t__removed__[0-9a-f][0-9a-f]\n" "" changed_raw "${changed_raw}")
  elseif(operation STREQUAL "extra")
    set(changed_raw "${raw_text}result\t18\t00\n")
  elseif(operation STREQUAL "duplicate")
    string(REPLACE "result\t1\t" "result\t0\t" changed_raw "${raw_text}")
  elseif(operation STREQUAL "zero-values")
    file(STRINGS "${raw}" raw_lines)
    set(changed_raw "")
    foreach(raw_line IN LISTS raw_lines)
      if(raw_line MATCHES "^result\t([0-9]+)\t[0-9a-f][0-9a-f]$")
        set(index "${CMAKE_MATCH_1}")
        string(APPEND changed_raw "result\t${index}\t00\n")
      else()
        string(APPEND changed_raw "${raw_line}\n")
      endif()
    endforeach()
  else()
    message(FATAL_ERROR "suite_driver: unknown audio raw-row mutation: ${operation}")
  endif()
  if(changed_raw STREQUAL raw_text)
    message(FATAL_ERROR "suite_driver: audio raw-row mutation did not change its input")
  endif()
  file(WRITE "${raw}" "${changed_raw}")
  audio_expect_record_failure("${expected}" "${suite}" "${profile}" "${AUDIO_CASE_DIR}")
  audio_case_end("${suite}")
endfunction()

function(audio_symbol_case case_name suite label delta expected)
  audio_case_begin("${case_name}" "${suite}" lynx1)
  set(labels "${AUDIO_CASE_DIR}/${suite}.lbl")
  file(STRINGS "${labels}" label_rows REGEX "${label}$")
  list(LENGTH label_rows label_count)
  if(NOT label_count EQUAL 1)
    message(FATAL_ERROR "suite_driver: audio symbol control did not find exactly one ${label}")
  endif()
  list(GET label_rows 0 old_row)
  if(NOT old_row MATCHES "^al ([0-9A-Fa-f]+) (.+)$")
    message(FATAL_ERROR "suite_driver: audio symbol control found malformed ${label} row")
  endif()
  set(old_address "${CMAKE_MATCH_1}")
  set(label_name "${CMAKE_MATCH_2}")
  math(EXPR new_address "0x${old_address} ${delta}" OUTPUT_FORMAT HEXADECIMAL)
  string(REGEX REPLACE "^0x" "" new_address "${new_address}")
  string(TOUPPER "${new_address}" new_address)
  string(LENGTH "${old_address}" address_width)
  string(LENGTH "${new_address}" new_width)
  while(new_width LESS address_width)
    string(PREPEND new_address "0")
    math(EXPR new_width "${new_width} + 1")
  endwhile()
  set(new_row "al ${new_address} ${label_name}")
  file(READ "${labels}" labels_text)
  string(REPLACE "${old_row}" "${new_row}" changed_labels "${labels_text}")
  if(changed_labels STREQUAL labels_text)
    message(FATAL_ERROR "suite_driver: audio symbol mutation did not change ${label}")
  endif()
  file(WRITE "${labels}" "${changed_labels}")
  expect_failure("${expected}" "${TEST_BINARY}" validate-audio-symbols "${labels}" "${AUDIO_CASE_DIR}/${suite}.bll")
  audio_case_end("${suite}")
endfunction()

function(audio_missing_return_case suite)
  audio_case_begin("guest-time-limit-before-return" "${suite}" lynx1)
  execute_process(
    COMMAND "${TEST_BINARY}" run-audio-bounded "${suite}" lynx1 lynx1 "${AUDIO_CASE_DIR}"
            "${AUDIO_CASE_DIR}/oracle.tsv" "${AUDIO_CASE_DIR}/audio-oracle.tsv" 256
    TIMEOUT 15 RESULT_VARIABLE run_rc OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
  if(NOT run_rc STREQUAL "1" OR
     NOT "${run_out}${run_err}" MATCHES "suite: completion: guest-time limit reached before the suite returned")
    message(FATAL_ERROR "suite_driver: audio bounded no-return control lacked its exact guest refusal: ${run_rc} ${run_out} ${run_err}")
  endif()
  foreach(output_name "raw-results-${suite}-lynx1.tsv" "run-record-${suite}-lynx1.txt" "comparison-${suite}-lynx1.tsv")
    if(EXISTS "${AUDIO_CASE_DIR}/${output_name}")
      message(FATAL_ERROR "suite_driver: incomplete audio guest published downstream evidence: ${output_name}")
    endif()
  endforeach()
  audio_case_end("${suite}")
endfunction()

function(audio_wrong_selection_case suite)
  audio_case_begin("wrong-selected-model" "${suite}" lynx1)
  execute_process(
    COMMAND "${TEST_BINARY}" run-audio-selected "${suite}" lynx1 lynx2 "${AUDIO_CASE_DIR}"
            "${AUDIO_CASE_DIR}/oracle.tsv" "${AUDIO_CASE_DIR}/audio-oracle.tsv"
    TIMEOUT 120 RESULT_VARIABLE run_rc OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
  if(NOT run_rc STREQUAL "1" OR
     NOT "${run_out}${run_err}" MATCHES "suite: audio guest model does not match selected profile input")
    message(FATAL_ERROR "suite_driver: opposite selected-profile control missed its exact model refusal: ${run_rc} ${run_out} ${run_err}")
  endif()
  audio_case_end("${suite}")
endfunction()

function(audio_alternative_case case_name operation suite)
  audio_case_begin("${case_name}" "${suite}" lynx2)
  if(operation STREQUAL "lost")
    set(oracle "${AUDIO_CASE_DIR}/audio-oracle.tsv")
    file(STRINGS "${oracle}" rows REGEX "^result\t9\t2\t30\t34$")
    list(LENGTH rows row_count)
    if(NOT row_count EQUAL 1)
      message(FATAL_ERROR "suite_driver: extended oracle did not contain exactly the pinned audio2 30|34 alternative")
    endif()
    list(GET rows 0 old_row)
    string(REPLACE "result\t9\t2\t30\t34" "result\t9\t1\t30" changed_oracle "${rows}")
    file(READ "${oracle}" oracle_text)
    string(REPLACE "${old_row}" "result\t9\t1\t30" changed_oracle "${oracle_text}")
    file(WRITE "${oracle}" "${changed_oracle}")
    set(expected "suite: audio oracle: malformed result row or lost alternative set")
  else()
    set(raw "${AUDIO_CASE_DIR}/raw-results-audio2-lynx2.tsv")
    file(STRINGS "${raw}" row REGEX "^result\t9\t")
    list(LENGTH row row_count)
    if(NOT row_count EQUAL 1 OR NOT row MATCHES "^result\t9\t([0-9a-f][0-9a-f])$")
      message(FATAL_ERROR "suite_driver: audio2 changed-observation baseline row9 is malformed")
    endif()
    set(actual "${CMAKE_MATCH_1}")
    if(actual STREQUAL "30")
      set(alternate "34")
    elseif(actual STREQUAL "34")
      set(alternate "30")
    else()
      message(FATAL_ERROR "suite_driver: audio2 baseline row9 is outside its pinned 30|34 alternatives: ${actual}")
    endif()
    file(READ "${raw}" raw_text)
    string(REPLACE "${row}" "result\t9\t${alternate}" changed_raw "${raw_text}")
    file(WRITE "${raw}" "${changed_raw}")
    set(expected "suite: audio comparison record row mismatch")
  endif()
  audio_expect_record_failure("${expected}" "audio2" lynx2 "${AUDIO_CASE_DIR}")
  audio_case_end("${suite}")
endfunction()

function(assert_timer_subcase_manifest expected_file actual_file expected_diagnostic)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    -DTIMER_SUBCASE_MANIFEST_CHECK=1
    "-DEXPECTED_MANIFEST=${expected_file}"
    "-DACTUAL_MANIFEST=${actual_file}"
    -P "${ROOT}/tests/suites/run.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  if(expected_diagnostic STREQUAL "")
    if(NOT rc STREQUAL "0" OR NOT combined MATCHES "timer subcase manifest:.*unique names valid")
      message(FATAL_ERROR "suite_driver: exact timer subcase manifest was rejected: ${rc} ${combined}")
    endif()
  else()
    set(compact_combined "${combined}")
    set(compact_expected "${expected_diagnostic}")
    foreach(whitespace " " "\t" "\r" "\n")
      string(REPLACE "${whitespace}" "" compact_combined "${compact_combined}")
      string(REPLACE "${whitespace}" "" compact_expected "${compact_expected}")
    endforeach()
    string(FIND "${compact_combined}" "${compact_expected}" found)
    if(rc STREQUAL "0" OR found EQUAL -1)
      message(FATAL_ERROR "suite_driver: timer subcase manifest self-control missed '${expected_diagnostic}': ${rc} ${combined}")
    endif()
  endif()
endfunction()

function(expect_empty_selection kind required controls)
  if(kind STREQUAL "required-suite")
    set(expected "suite: selection: required-suite selection is empty")
  else()
    set(expected "suite: selection: control selection is empty")
  endif()
  execute_process(COMMAND "${TEST_BINARY}" validate-selection "${required}" "${controls}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "1" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: empty ${kind} selection did not reach '${expected}': ${rc} ${combined}")
  endif()
endfunction()

function(assert_timer_artifacts profile)
  set(raw "${SUITE_BUILD}/raw-results-${profile}.tsv")
  set(record "${SUITE_BUILD}/run-record-${profile}.txt")
  set(comparison "${SUITE_BUILD}/comparison-${profile}.tsv")
  set(proof "${SUITE_BUILD}/cascade-proof-${profile}.json")
  set(companion "${SUITE_BUILD}/pending-record-${profile}.json")
  if(NOT EXISTS "${raw}" OR NOT EXISTS "${record}" OR NOT EXISTS "${comparison}" OR
     NOT EXISTS "${proof}" OR NOT EXISTS "${companion}")
    message(FATAL_ERROR "suite_driver: missing timer evidence output for ${profile}")
  endif()
  file(STRINGS "${raw}" raw_rows REGEX "^result\t[0-9]+\t[0-9a-f][0-9a-f]$")
  file(STRINGS "${comparison}" comparison_rows REGEX "^result\t[0-9]+\t")
  list(LENGTH raw_rows raw_count)
  list(LENGTH comparison_rows comparison_count)
  set(accepted 0)
  set(mismatch 0)
  foreach(row IN LISTS comparison_rows)
    if(row MATCHES "source-accepted$")
      math(EXPR accepted "${accepted} + 1")
    elseif(row MATCHES "source-mismatch$")
      math(EXPR mismatch "${mismatch} + 1")
      if(NOT row STREQUAL "result\t14\t0c\tEXPECT(0d)\tsource-mismatch")
        message(FATAL_ERROR "suite_driver: unexpected timer source mismatch row: ${row}")
      endif()
    else()
      message(FATAL_ERROR "suite_driver: malformed timer comparison status: ${row}")
    endif()
  endforeach()
  if(NOT raw_count EQUAL 16 OR NOT comparison_count EQUAL 16 OR
     NOT accepted EQUAL 15 OR NOT mismatch EQUAL 1)
    message(FATAL_ERROR "suite_driver: timer evidence counts drifted for ${profile}: raw=${raw_count} comparison=${comparison_count} accepted=${accepted} mismatch=${mismatch}")
  endif()
  file(STRINGS "${proof}" phase_rows REGEX "first_edge_delay_ticks")
  list(LENGTH phase_rows phase_count)
  if(NOT phase_count EQUAL 64)
    message(FATAL_ERROR "suite_driver: cascade proof must enumerate64 phases, got ${phase_count}")
  endif()
  foreach(phase_row IN LISTS phase_rows)
    if(NOT phase_row MATCHES "\\\"acknowledgements\\\":12" OR
       NOT phase_row MATCHES "\\\"final_poll_ticks\\\":13990")
      message(FATAL_ERROR "suite_driver: phase enumeration output drifted: ${phase_row}")
    endif()
  endforeach()
  file(READ "${record}" record_text)
  if(NOT record_text MATCHES "meaningful_count=16" OR
     NOT record_text MATCHES "reserved_count=2" OR
     NOT record_text MATCHES "result_extent=18" OR
     NOT record_text MATCHES "source_comparison=separate-record" OR
     NOT record_text MATCHES "hardware_accuracy=unverified" OR
     NOT record_text MATCHES "profile_qualification=unresolved" OR
     NOT record_text MATCHES "cost_model=flat5" OR
     NOT record_text MATCHES "sample_offset_ticks=0")
    message(FATAL_ERROR "suite_driver: timer run record does not retain the bounded policy/evidence fields")
  endif()
  expect_success("timer record: complete observation, executable phase proof and pending row valid for ${profile}"
    "${TEST_BINARY}" validate-timer-record "${profile}" "${raw}" "${record}" "${ORACLE}"
    "${ROOT}/docs/timer-source-index14-pending.json")
endfunction()

function(run_timer_observation profile)
  execute_process(
    COMMAND "${TEST_BINARY}" observe timers "${profile}" "${SUITE_BUILD}" "${ORACLE}"
            "${ROOT}/docs/timer-source-index14-pending.json"
    TIMEOUT 45 RESULT_VARIABLE observe_rc OUTPUT_VARIABLE observe_out ERROR_VARIABLE observe_err)
  if(NOT observe_rc STREQUAL "0" OR
     NOT observe_out MATCHES "timer raw source comparison: mismatch; suite=timers machine_profile=${profile}" OR
     NOT observe_out MATCHES "results=16/18" OR
     NOT observe_out MATCHES "hardware_accuracy=unverified profile_qualification=unresolved" OR
     NOT observe_out MATCHES "timer observation complete: profile=${profile} meaningful=16 reserved=2 source_agreements=15 pending_index=14 actual=0c expected=0d")
    message(FATAL_ERROR "suite_driver: bounded timer observation failed for ${profile}: ${observe_rc} ${observe_out} ${observe_err}")
  endif()
  assert_timer_artifacts("${profile}")
endfunction()

function(run_timer_video_observation profile)
  execute_process(
    COMMAND "${TEST_BINARY}" observe timers-video "${profile}" "${SUITE_BUILD}" "${ORACLE}"
    TIMEOUT 120 RESULT_VARIABLE observe_rc OUTPUT_VARIABLE observe_out ERROR_VARIABLE observe_err)
  if(NOT observe_rc STREQUAL "0" AND NOT observe_rc STREQUAL "1")
    message(FATAL_ERROR "suite_driver: video-policy timer observation did not return a semantic comparator status for ${profile}: ${observe_rc} ${observe_out} ${observe_err}")
  endif()
  if(NOT observe_out MATCHES "timer raw source comparison: (match|mismatch); suite=timers machine_profile=${profile}" OR
     NOT observe_out MATCHES "hardware_accuracy=unverified profile_qualification=unresolved")
    message(FATAL_ERROR "suite_driver: video-policy timer observation omitted its selected profile or qualification limits for ${profile}: ${observe_rc} ${observe_out} ${observe_err}")
  endif()
  set(raw "${SUITE_BUILD}/raw-results-${profile}.tsv")
  set(comparison "${SUITE_BUILD}/comparison-${profile}.tsv")
  set(record "${SUITE_BUILD}/run-record-${profile}.txt")
  set(video_record "${SUITE_BUILD}/timer-video-revisit-${profile}.txt")
  if(NOT EXISTS "${raw}" OR NOT EXISTS "${comparison}" OR NOT EXISTS "${record}" OR NOT EXISTS "${video_record}")
    message(FATAL_ERROR "suite_driver: video-policy timer evidence is incomplete for ${profile}")
  endif()
  file(STRINGS "${raw}" raw_rows REGEX "^result\t[0-9]+\t[0-9a-f][0-9a-f]$")
  file(STRINGS "${comparison}" comparison_rows REGEX "^result\t[0-9]+\t")
  file(STRINGS "${comparison}" comparison_summary REGEX "^comparison\t(match|mismatch)$")
  list(LENGTH raw_rows raw_count)
  list(LENGTH comparison_rows comparison_count)
  list(LENGTH comparison_summary summary_count)
  if(NOT raw_count EQUAL 16 OR NOT comparison_count EQUAL 16 OR NOT summary_count EQUAL 1)
    message(FATAL_ERROR "suite_driver: video-policy timer comparison lost rows for ${profile}: raw=${raw_count} comparison=${comparison_count} summary=${summary_count}")
  endif()
  set(accepted 0)
  set(mismatch 0)
  foreach(row IN LISTS comparison_rows)
    if(row MATCHES "source-accepted$")
      math(EXPR accepted "${accepted} + 1")
    elseif(row MATCHES "source-mismatch$")
      math(EXPR mismatch "${mismatch} + 1")
    else()
      message(FATAL_ERROR "suite_driver: malformed video-policy timer status for ${profile}: ${row}")
    endif()
  endforeach()
  if(mismatch EQUAL 0)
    if(NOT accepted EQUAL 16 OR NOT observe_rc STREQUAL "0" OR NOT comparison_summary STREQUAL "comparison\tmatch")
      message(FATAL_ERROR "suite_driver: strict video-policy timer match status is inconsistent for ${profile}: ${observe_rc} ${comparison_summary}")
    endif()
    set(strict_status agreement)
  else()
    if(NOT observe_rc STREQUAL "1" OR NOT comparison_summary STREQUAL "comparison\tmismatch" OR
       NOT observe_err MATCHES "suite: timer source oracle comparison mismatch")
      message(FATAL_ERROR "suite_driver: strict video-policy timer mismatch did not retain its exact failure for ${profile}: ${observe_rc} ${observe_out} ${observe_err}")
    endif()
    set(strict_status mismatch)
  endif()
  file(READ "${video_record}" video_record_text)
  if(NOT video_record_text MATCHES "core_timing_policy=video-policy1" OR
     NOT video_record_text MATCHES "strict_comparison=all-16-source-rows" OR
     NOT video_record_text MATCHES "pending_descriptor=none" OR
     NOT video_record_text MATCHES "wrapper_counter_clear_write_seen=true" OR
     NOT video_record_text MATCHES "source_acknowledged_increments=[0-9]+" OR
     NOT video_record_text MATCHES "initial_counter_value=00")
    message(FATAL_ERROR "suite_driver: video-policy timer environment identity/counter evidence is incomplete for ${profile}")
  endif()
  expect_success("timer video record: strict"
    "${TEST_BINARY}" validate-timer-video-record "${profile}" "${raw}" "${record}"
    "${video_record}" "${comparison}" "${ORACLE}" "${SUITE_BUILD}")
  string(REGEX MATCH "ticks=([0-9]+)" tick_match "${observe_out}")
  if(tick_match STREQUAL "")
    message(FATAL_ERROR "suite_driver: video-policy timer observation omitted the measured guest tick count for ${profile}")
  endif()
  set(VIDEO_PROFILE_TICKS "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(normalized_rows "")
  foreach(row IN LISTS comparison_rows)
    string(APPEND normalized_rows "${row}\n")
  endforeach()
  string(APPEND all_run_out "VIDEO_POLICY1 ${profile}: 16 exact source rows, ${accepted} accepted, ${mismatch} mismatches, strict status=${strict_status}; no pending descriptor\n${observe_out}\n")
  string(APPEND all_run_out "VIDEO_POLICY1 ${profile} strict rows:\n${normalized_rows}${video_record_text}\n")
  set(all_run_out "${all_run_out}" PARENT_SCOPE)
endfunction()

function(run_timer_video_pair)
  run_timer_video_observation(lynx1)
  set(lynx1_ticks "${VIDEO_PROFILE_TICKS}")
  run_timer_video_observation(lynx2)
  set(lynx2_ticks "${VIDEO_PROFILE_TICKS}")
  set(all_run_out "${all_run_out}" PARENT_SCOPE)
  set(VIDEO_LYNX1_TICKS "${lynx1_ticks}" PARENT_SCOPE)
  set(VIDEO_LYNX2_TICKS "${lynx2_ticks}" PARENT_SCOPE)
endfunction()

function(video_assert_baseline_unchanged)
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/video-baseline/${profile}")
    set(manifest "${WORK}/video-baseline/${profile}.sha256")
    if(NOT EXISTS "${manifest}")
      message(FATAL_ERROR "suite_driver: video-policy immutable baseline manifest is missing for ${profile}")
    endif()
    file(STRINGS "${manifest}" entries)
    foreach(entry IN LISTS entries)
      if(NOT entry MATCHES "^([0-9a-f]+)  ([^ ]+)$")
        message(FATAL_ERROR "suite_driver: malformed video-policy baseline entry: ${entry}")
      endif()
      set(expected_sha "${CMAKE_MATCH_1}")
      set(file_name "${CMAKE_MATCH_2}")
      if(NOT EXISTS "${base}/${file_name}")
        message(FATAL_ERROR "suite_driver: video-policy immutable baseline file disappeared: ${profile}/${file_name}")
      endif()
      file(SHA256 "${base}/${file_name}" actual_sha)
      if(NOT actual_sha STREQUAL expected_sha)
        message(FATAL_ERROR "suite_driver: video-policy immutable baseline changed: ${profile}/${file_name}")
      endif()
    endforeach()
  endforeach()
endfunction()

function(video_case_begin case_name)
  video_assert_baseline_unchanged()
  get_property(case_names GLOBAL PROPERTY VIDEO_CONTROL_CASE_NAMES)
  if(case_names STREQUAL "")
    set(case_names "")
  endif()
  list(FIND case_names "${case_name}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "suite_driver: duplicate video-policy control case ${case_name}")
  endif()
  list(APPEND case_names "${case_name}")
  set_property(GLOBAL PROPERTY VIDEO_CONTROL_CASE_NAMES "${case_names}")
  get_property(case_count GLOBAL PROPERTY VIDEO_CONTROL_CASE_COUNT)
  if(case_count STREQUAL "")
    set(case_count 0)
  endif()
  math(EXPR case_count "${case_count} + 1")
  set_property(GLOBAL PROPERTY VIDEO_CONTROL_CASE_COUNT "${case_count}")
  string(MAKE_C_IDENTIFIER "${case_name}" case_slug)
  set(case_dir "${WORK}/video-cases/${case_count}-${case_slug}")
  if(EXISTS "${case_dir}" OR IS_SYMLINK "${case_dir}")
    message(FATAL_ERROR "suite_driver: video-policy case directory already exists")
  endif()
  file(MAKE_DIRECTORY "${case_dir}")
  file(COPY "${WORK}/video-baseline/lynx1/" DESTINATION "${case_dir}")
  write_suite_owner_marker("${case_dir}")
  set(VIDEO_CASE_DIR "${case_dir}" PARENT_SCOPE)
endfunction()

function(video_validate_failure expected directory)
  expect_failure("${expected}" "${TEST_BINARY}" validate-timer-video-record lynx1
    "${directory}/raw-results-lynx1.tsv" "${directory}/run-record-lynx1.txt"
    "${directory}/timer-video-revisit-lynx1.txt" "${directory}/comparison-lynx1.tsv"
    "${directory}/oracle.tsv" "${directory}")
endfunction()

function(video_case_end)
  video_assert_baseline_unchanged()
endfunction()

function(video_rewrite_case case_name file_name old_text new_text diagnostic)
  video_case_begin("${case_name}")
  set(path "${VIDEO_CASE_DIR}/${file_name}")
  file(READ "${path}" original)
  string(FIND "${original}" "${old_text}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "suite_driver: video-policy control target text missing for ${case_name}")
  endif()
  string(REPLACE "${old_text}" "${new_text}" mutated "${original}")
  if(mutated STREQUAL original)
    message(FATAL_ERROR "suite_driver: video-policy mutation did not change its private copy for ${case_name}")
  endif()
  file(WRITE "${path}" "${mutated}")
  video_validate_failure("${diagnostic}" "${VIDEO_CASE_DIR}")
  video_case_end()
endfunction()

function(video_expect_time_candidate case_name tick expected_failure)
  video_case_begin("${case_name}")
  set(candidate "${WORK}/video-candidates/${case_name}.txt")
  file(MAKE_DIRECTORY "${WORK}/video-candidates")
  file(WRITE "${candidate}" "tick_min=256\ntick_max=50000000\nticks_observed=${tick}\n")
  if(expected_failure STREQUAL "")
    expect_success("not guest evidence" "${TEST_BINARY}" timer-video-time-candidate "${candidate}")
  else()
    expect_failure("${expected_failure}" "${TEST_BINARY}" timer-video-time-candidate "${candidate}")
  endif()
  video_case_end()
endfunction()

function(run_timer_video_revisit_controls)
  run_timer_video_pair()
  set(file_names
    "raw-results-lynx1.tsv" "run-record-lynx1.txt" "comparison-lynx1.tsv"
    "timer-video-revisit-lynx1.txt" "timers.bll" "timers.lbl" "build-record.txt" "oracle.tsv"
    "raw-results-lynx2.tsv" "run-record-lynx2.txt" "comparison-lynx2.tsv" "timer-video-revisit-lynx2.txt")
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/video-baseline/${profile}")
    if(EXISTS "${base}" OR IS_SYMLINK "${base}")
      message(FATAL_ERROR "suite_driver: video-policy baseline directory already exists")
    endif()
    file(MAKE_DIRECTORY "${base}")
    write_suite_owner_marker("${base}")
    set(manifest "${WORK}/video-baseline/${profile}.sha256")
    file(WRITE "${manifest}" "")
    foreach(file_name IN LISTS file_names)
      if((profile STREQUAL "lynx1" AND file_name MATCHES "lynx2") OR
         (profile STREQUAL "lynx2" AND file_name MATCHES "lynx1"))
        continue()
      endif()
      if(NOT EXISTS "${SUITE_BUILD}/${file_name}")
        message(FATAL_ERROR "suite_driver: video-policy baseline output missing: ${profile}/${file_name}")
      endif()
      file(COPY_FILE "${SUITE_BUILD}/${file_name}" "${base}/${file_name}")
      file(SHA256 "${base}/${file_name}" file_sha)
      file(APPEND "${manifest}" "${file_sha}  ${file_name}\n")
    endforeach()
  endforeach()
  set_property(GLOBAL PROPERTY VIDEO_CONTROL_CASE_COUNT 0)
  set_property(GLOBAL PROPERTY VIDEO_CONTROL_CASE_NAMES "")
  video_assert_baseline_unchanged()
  expect_success("timer video record: strict"
    "${TEST_BINARY}" validate-timer-video-record lynx1
    "${WORK}/video-baseline/lynx1/raw-results-lynx1.tsv"
    "${WORK}/video-baseline/lynx1/run-record-lynx1.txt"
    "${WORK}/video-baseline/lynx1/timer-video-revisit-lynx1.txt"
    "${WORK}/video-baseline/lynx1/comparison-lynx1.tsv"
    "${WORK}/video-baseline/lynx1/oracle.tsv" "${WORK}/video-baseline/lynx1")

  video_case_begin("legacy-pending-rejected")
  expect_failure("timer record: environment metadata mismatch"
    "${TEST_BINARY}" validate-timer-record lynx1
    "${WORK}/video-baseline/lynx1/raw-results-lynx1.tsv"
    "${WORK}/video-baseline/lynx1/run-record-lynx1.txt"
    "${WORK}/video-baseline/lynx1/oracle.tsv" "${ROOT}/docs/timer-source-index14-pending.json")
  video_case_end()
  video_case_begin("missing-descriptor-rejected")
  expect_failure("pending descriptor malformed JSON"
    "${TEST_BINARY}" validate-timer-record lynx1
    "${WORK}/video-baseline/lynx1/raw-results-lynx1.tsv"
    "${WORK}/video-baseline/lynx1/run-record-lynx1.txt"
    "${WORK}/video-baseline/lynx1/oracle.tsv" "${WORK}/missing-video-pending.json")
  video_case_end()
  video_case_begin("stale-descriptor-rejected")
  set(stale_descriptor "${VIDEO_CASE_DIR}/stale-pending.json")
  file(COPY_FILE "${ROOT}/docs/timer-source-index14-pending.json" "${stale_descriptor}")
  file(READ "${stale_descriptor}" stale_text)
  string(REPLACE "\"actual\": \"0c\"" "\"actual\": \"0b\"" stale_text "${stale_text}")
  file(WRITE "${stale_descriptor}" "${stale_text}")
  expect_failure("pending descriptor value mismatch"
    "${TEST_BINARY}" validate-timer-record lynx1
    "${VIDEO_CASE_DIR}/raw-results-lynx1.tsv" "${VIDEO_CASE_DIR}/run-record-lynx1.txt"
    "${VIDEO_CASE_DIR}/oracle.tsv" "${stale_descriptor}")
  video_case_end()
  video_rewrite_case("descriptor-inheritance-field" "timer-video-revisit-lynx1.txt"
    "pending_descriptor=none" "pending_descriptor=docs/timer-source-index14-pending.json"
    "timer video record: legacy descriptor inheritance refused")
  video_rewrite_case("policy-changed" "timer-video-revisit-lynx1.txt"
    "core_timing_policy=video-policy1" "core_timing_policy=legacy-flat5"
    "timer video record: timing policy mismatch")
  video_rewrite_case("cpu-cost-changed" "run-record-lynx1.txt"
    "cost_ticks=5" "cost_ticks=6" "timer video record: CPU cost identity mismatch")
  video_rewrite_case("video-cost-changed" "timer-video-revisit-lynx1.txt"
    "video_byte_grant_ticks=5" "video_byte_grant_ticks=6"
    "timer video record: video cost identity mismatch")
  video_rewrite_case("refresh-cost-changed" "timer-video-revisit-lynx1.txt"
    "refresh_grant_ticks=5" "refresh_grant_ticks=6"
    "timer video record: refresh cost identity mismatch")
  video_rewrite_case("refresh-period-changed" "timer-video-revisit-lynx1.txt"
    "refresh_period_ticks=256" "refresh_period_ticks=257"
    "timer video record: refresh period mismatch")
  video_rewrite_case("initial-counter-changed" "timer-video-revisit-lynx1.txt"
    "initial_counter_value=00" "initial_counter_value=01"
    "timer video record: counter initialization mismatch")
  video_rewrite_case("source-hash-changed" "timer-video-revisit-lynx1.txt"
    "source_archive_sha256=a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e"
    "source_archive_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: source archive mismatch")
  video_rewrite_case("source-tests-hash-changed" "timer-video-revisit-lynx1.txt"
    "timer_tests_sha256=dde10583ae050aba1dcd72f3e0b92cdf2d38d5037c4380c5e8864f2c81702e1f"
    "timer_tests_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: source identity mismatch")
  video_rewrite_case("oracle-hash-changed" "timer-video-revisit-lynx1.txt"
    "oracle_sha256=08b5a4f48caf0f92e7bc177d9f3a6ac8ff68d757b57b9bc346aad8bd4c698b4f"
    "oracle_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: oracle identity mismatch")
  file(SHA256 "${WORK}/video-baseline/lynx1/build-record.txt" video_build_record_sha)
  video_rewrite_case("build-hash-changed" "timer-video-revisit-lynx1.txt"
    "build_record_sha256=${video_build_record_sha}"
    "build_record_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: build identity mismatch")
  video_rewrite_case("layout-hash-changed" "timer-video-revisit-lynx1.txt"
    "layout_manifest_sha256=177acdcf8690ed73cd303aacf608b4c5aaaf1b69d5a27e472a08aba673b8121f"
    "layout_manifest_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: layout identity mismatch")
  video_rewrite_case("object-hash-changed" "timer-video-revisit-lynx1.txt"
    "bll_sha256=d766a24031246ad166edc6b417e4d30a841e18dda5115eae201804847ef96c79"
    "bll_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: object identity mismatch")
  video_rewrite_case("labels-hash-changed" "timer-video-revisit-lynx1.txt"
    "labels_sha256=b1423949c03badc1158b5cacfb67af146cbe18228cc73f156b11dd548776efa3"
    "labels_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: labels identity mismatch")
  video_rewrite_case("wrapper-hash-changed" "timer-video-revisit-lynx1.txt"
    "wrapper_sha256=fa0973d355eeba6311d9540f3d2ec7d429f6991e1e5641fb36ed7af6c2749e6c"
    "wrapper_sha256=0000000000000000000000000000000000000000000000000000000000000000"
    "timer video record: wrapper identity mismatch")
  video_rewrite_case("result14-record-changed" "timer-video-revisit-lynx1.txt"
    "result14=0f" "result14=0d" "timer video record: counter/result evidence mismatch")
  video_case_begin("second-mismatch")
  set(second_mismatch_raw "${VIDEO_CASE_DIR}/raw-results-lynx1.tsv")
  set(second_mismatch_comparison "${VIDEO_CASE_DIR}/comparison-lynx1.tsv")
  file(READ "${second_mismatch_raw}" second_mismatch_raw_text)
  file(READ "${second_mismatch_comparison}" second_mismatch_comparison_text)
  string(FIND "${second_mismatch_raw_text}" "result\t0\t08\n" second_raw_found)
  string(FIND "${second_mismatch_comparison_text}" "result\t0\t08\tEXPECT(08)\tsource-accepted" second_comparison_found)
  if(second_raw_found EQUAL -1 OR second_comparison_found EQUAL -1)
    message(FATAL_ERROR "suite_driver: second-mismatch control source row is missing")
  endif()
  string(REPLACE "result\t0\t08\n" "result\t0\t09\n" second_mismatch_raw_text "${second_mismatch_raw_text}")
  string(REPLACE "result\t0\t08\tEXPECT(08)\tsource-accepted"
    "result\t0\t09\tEXPECT(08)\tsource-mismatch" second_mismatch_comparison_text "${second_mismatch_comparison_text}")
  file(WRITE "${second_mismatch_raw}" "${second_mismatch_raw_text}")
  file(WRITE "${second_mismatch_comparison}" "${second_mismatch_comparison_text}")
  video_validate_failure("timer video record: strict mismatch set changed" "${VIDEO_CASE_DIR}")
  video_case_end()
  video_rewrite_case("raw-empty" "raw-results-lynx1.tsv" "suite\ttimers" "" "timer raw record shape mismatch")
  video_rewrite_case("raw-count-0" "raw-results-lynx1.tsv" "result_count\t16" "result_count\t0" "timer raw record count drift")
  video_rewrite_case("raw-count-1" "raw-results-lynx1.tsv" "result_count\t16" "result_count\t1" "timer raw record count drift")
  video_rewrite_case("raw-count-15" "raw-results-lynx1.tsv" "result_count\t16" "result_count\t15" "timer raw record count drift")
  video_rewrite_case("raw-count-17" "raw-results-lynx1.tsv" "result_count\t16" "result_count\t17" "timer raw record count drift")
  video_rewrite_case("raw-duplicate-row" "raw-results-lynx1.tsv" "result\t15\t08" "result\t14\t08" "timer raw record count or byte malformed")
  video_rewrite_case("raw-extra-row" "raw-results-lynx1.tsv" "result\t15\t08\n" "result\t15\t08\nresult\t16\t00\n" "timer raw record has extra rows")
  video_rewrite_case("raw-empty-result" "raw-results-lynx1.tsv" "result\t14\t0f" "result\t14\t" "timer raw record count or byte malformed")
  video_rewrite_case("raw-uppercase-hex" "raw-results-lynx1.tsv" "result\t14\t0f" "result\t14\t0F" "timer raw record count or byte malformed")
  video_rewrite_case("raw-odd-hex" "raw-results-lynx1.tsv" "result\t14\t0f" "result\t14\t0f0" "timer raw record count or byte malformed")
  video_rewrite_case("comparison-missing-row" "comparison-lynx1.tsv" "result\t15\t08\tEXPECT(08)\tsource-accepted\n" "" "timer comparison record row mismatch")
  video_rewrite_case("comparison-duplicate-row" "comparison-lynx1.tsv" "result\t15\t08\tEXPECT(08)\tsource-accepted" "result\t14\t08\tEXPECT(08)\tsource-accepted" "timer comparison record row mismatch")
  video_rewrite_case("comparison-extra-row" "comparison-lynx1.tsv" "comparison\tmismatch" "result\t16\t00\tEXPECT(00)\tsource-accepted\ncomparison\tmismatch" "timer comparison record summary mismatch")
  video_rewrite_case("comparison-summary" "comparison-lynx1.tsv" "comparison\tmismatch" "comparison\tmatch" "timer comparison record summary mismatch")
  video_case_begin("null-inputs")
  expect_success("timer video null control: all NULL source/record inputs refused"
    "${TEST_BINARY}" timer-video-null-controls)
  video_case_end()
  video_case_begin("manufactured-agreement-comparator-only")
  set(fake_raw "${VIDEO_CASE_DIR}/raw-results-lynx1.tsv")
  set(fake_comparison "${VIDEO_CASE_DIR}/comparison-lynx1.tsv")
  file(READ "${fake_raw}" fake_raw_text)
  file(READ "${fake_comparison}" fake_comparison_text)
  string(REPLACE "result\t14\t0f" "result\t14\t0d" fake_raw_text "${fake_raw_text}")
  string(REPLACE "result\t14\t0f\tEXPECT(0d)\tsource-mismatch" "result\t14\t0d\tEXPECT(0d)\tsource-accepted" fake_comparison_text "${fake_comparison_text}")
  string(REPLACE "comparison\tmismatch" "comparison\tmatch" fake_comparison_text "${fake_comparison_text}")
  file(WRITE "${fake_raw}" "${fake_raw_text}")
  file(WRITE "${fake_comparison}" "${fake_comparison_text}")
  expect_success("not guest evidence" "${TEST_BINARY}" validate-timer-video-comparator-only lynx1
    "${fake_raw}" "${fake_comparison}" "${VIDEO_CASE_DIR}/oracle.tsv")
  video_case_end()

  video_expect_time_candidate("time-255" 255 "timer video time candidate: ticks outside completion bounds")
  video_expect_time_candidate("time-256" 256 "")
  video_expect_time_candidate("time-257" 257 "")
  video_expect_time_candidate("time-49999999" 49999999 "")
  video_expect_time_candidate("time-50000000" 50000000 "")
  video_expect_time_candidate("time-50000001" 50000001 "timer video time candidate: ticks outside completion bounds")
  video_expect_time_candidate("time-uint64-overflow" 18446744073709551616 "timer video time candidate: invalid ticks_observed")

  video_case_begin("slicing-lynx1")
  expect_success("timer video slicing control: profile=lynx1 common_tick=${VIDEO_LYNX1_TICKS}"
    "${TEST_BINARY}" timer-video-slicing-control lynx1 "${SUITE_BUILD}" "${ORACLE}" "${VIDEO_LYNX1_TICKS}")
  video_case_end()
  video_case_begin("slicing-lynx2")
  expect_success("timer video slicing control: profile=lynx2 common_tick=${VIDEO_LYNX2_TICKS}"
    "${TEST_BINARY}" timer-video-slicing-control lynx2 "${SUITE_BUILD}" "${ORACLE}" "${VIDEO_LYNX2_TICKS}")
  video_case_end()

  video_assert_baseline_unchanged()
  get_property(actual_cases GLOBAL PROPERTY VIDEO_CONTROL_CASE_NAMES)
  set(expected_cases
    "legacy-pending-rejected" "missing-descriptor-rejected" "stale-descriptor-rejected"
    "descriptor-inheritance-field" "policy-changed" "cpu-cost-changed" "video-cost-changed"
    "refresh-cost-changed" "refresh-period-changed" "initial-counter-changed" "source-hash-changed"
    "source-tests-hash-changed" "oracle-hash-changed" "build-hash-changed" "layout-hash-changed"
    "object-hash-changed" "labels-hash-changed" "wrapper-hash-changed"
    "result14-record-changed" "second-mismatch" "raw-empty" "raw-count-0" "raw-count-1"
    "raw-count-15" "raw-count-17" "raw-duplicate-row" "raw-extra-row" "raw-empty-result" "raw-uppercase-hex"
    "raw-odd-hex" "comparison-missing-row" "comparison-duplicate-row" "comparison-extra-row"
    "comparison-summary" "null-inputs" "manufactured-agreement-comparator-only"
    "time-255" "time-256" "time-257" "time-49999999" "time-50000000" "time-50000001"
    "time-uint64-overflow" "slicing-lynx1" "slicing-lynx2")
  if(NOT "${actual_cases}" STREQUAL "${expected_cases}")
    message(FATAL_ERROR "suite_driver: video-policy control case inventory drifted; actual=${actual_cases}; expected=${expected_cases}")
  endif()
  run_timer_video_pair()
  string(APPEND all_run_out "video-policy controls: exact identities, strict rows, counters, boundaries, hostile copies, NULLs, descriptor refusal and common-tick slicing/watching verified; no synthetic comparator result is guest evidence\n")
endfunction()

function(run_timer_pair)
  run_timer_observation(lynx1)
  run_timer_observation(lynx2)
endfunction()

function(run_cpu_no_run_control profile)
  expect_success_timeout("suite raw source comparison: match; suite=cpu machine_profile=${profile}"
    60 "${TEST_BINARY}" run cpu "${profile}" "${SUITE_BUILD}" "${ORACLE}" zero)
  execute_process(COMMAND "${TEST_BINARY}" no-run cpu "${profile}" "${SUITE_BUILD}" "${ORACLE}"
    TIMEOUT 90 RESULT_VARIABLE no_run_rc OUTPUT_VARIABLE no_run_out ERROR_VARIABLE no_run_err)
  set(no_run_text "${no_run_out}${no_run_err}")
  if(NOT no_run_rc STREQUAL "0" OR
     NOT no_run_text MATCHES "suite: completion: guest execution absent; post-return sentinel was not written" OR
     NOT no_run_out MATCHES "no-run control: exact absent-execution refusal observed after minimum held guest time; hold released" OR
     NOT no_run_out MATCHES "suite raw source comparison: match; suite=cpu machine_profile=${profile}")
    message(FATAL_ERROR "suite_driver: CPU ${profile} no-run did not prove exact refusal and a resumed positive: ${no_run_rc} ${no_run_out} ${no_run_err}")
  endif()
  expect_success("cpu record: 18 raw results, completion and model evidence valid for ${profile}"
    "${TEST_BINARY}" validate-cpu-record "${profile}" "${SUITE_BUILD}/raw-results-zero.tsv"
    "${SUITE_BUILD}/run-record-zero.txt" "${ORACLE}" "${SUITE_BUILD}")
  set(all_run_out "${all_run_out}CPU ${profile}: fresh positive before hold; true no-run refused after held guest time; hold released and same instance produced fresh positive\n${no_run_out}\n" PARENT_SCOPE)
endfunction()

function(run_timer_no_run_control profile)
  run_timer_observation("${profile}")
  execute_process(COMMAND "${TEST_BINARY}" no-run timers "${profile}" "${SUITE_BUILD}" "${ORACLE}"
          "${ROOT}/docs/timer-source-index14-pending.json"
    TIMEOUT 120 RESULT_VARIABLE no_run_rc OUTPUT_VARIABLE no_run_out ERROR_VARIABLE no_run_err)
  set(no_run_text "${no_run_out}${no_run_err}")
  if(NOT no_run_rc STREQUAL "0" OR
     NOT no_run_text MATCHES "suite: completion: guest execution absent; post-return sentinel was not written" OR
     NOT no_run_out MATCHES "no-run control: exact absent-execution refusal observed after minimum held guest time; hold released" OR
     NOT no_run_out MATCHES "timer observation complete: profile=${profile} meaningful=16 reserved=2 source_agreements=15 pending_index=14 actual=0c expected=0d")
    message(FATAL_ERROR "suite_driver: timer ${profile} no-run did not prove exact refusal and a resumed observation: ${no_run_rc} ${no_run_out} ${no_run_err}")
  endif()
  assert_timer_artifacts("${profile}")
  set(all_run_out "${all_run_out}Timers ${profile}: fresh pending-row observation before hold; true no-run refused after held guest time; hold released and same instance produced fresh bound observation\n${no_run_out}\n" PARENT_SCOPE)
endfunction()

function(build_fault_wrapper suite)
  set(default_wrapper "${ROOT}/fixtures/suites/${suite}-wrapper.s")
  set(suite_source "${ROOT}/build/pins/lynx-tests/${suite}")
  set(cc65_dir "${ROOT}/build/pins/cc65")
  set(fault_root "${WORK}/fault-wrappers")
  set(fault_dir "${WORK}/fault-build/${suite}")
  set(fault_wrapper "${fault_root}/${suite}-omit-sentinel.s")
  file(MAKE_DIRECTORY "${fault_root}" "${fault_dir}")
  write_suite_owner_marker("${fault_dir}")

  file(READ "${default_wrapper}" default_source)
  set(sentinel_source [=[        lda #$A5
        sta suite_done]=])
  set(nop_source [=[        nop
        nop
        nop
        nop
        nop]=])
  string(LENGTH "${default_source}" original_length)
  string(REPLACE "${sentinel_source}" "" remainder "${default_source}")
  string(LENGTH "${remainder}" remainder_length)
  math(EXPR patch_bytes "${original_length} - ${remainder_length}")
  string(LENGTH "${sentinel_source}" sentinel_source_length)
  if(NOT patch_bytes EQUAL sentinel_source_length)
    message(FATAL_ERROR "suite_driver: ${suite} wrapper does not have exactly one final sentinel sequence")
  endif()
  string(REPLACE "${sentinel_source}" "${nop_source}" fault_source "${default_source}")
  if(fault_source STREQUAL default_source)
    message(FATAL_ERROR "suite_driver: ${suite} omitted-sentinel wrapper mutation did not change its source")
  endif()
  file(WRITE "${fault_wrapper}" "${fault_source}")

  set(tool_manifest "${cc65_dir}/.lynx-build-files")
  if(NOT EXISTS "${tool_manifest}" OR NOT EXISTS "${cc65_dir}/bin/ca65" OR
     NOT EXISTS "${cc65_dir}/bin/ld65")
    message(FATAL_ERROR "suite_driver: pinned cc65 fault-wrapper tools or manifest are missing")
  endif()
  foreach(tool bin/ca65 bin/ld65)
    file(STRINGS "${tool_manifest}" tool_rows REGEX "${tool}$")
    list(LENGTH tool_rows tool_row_count)
    if(NOT tool_row_count EQUAL 1)
      message(FATAL_ERROR "suite_driver: pinned tool manifest lacks one exact row for ${tool}")
    endif()
    list(GET tool_rows 0 tool_row)
    if(NOT tool_row MATCHES "^([0-9a-f]+)[ \t]+${tool}$")
      message(FATAL_ERROR "suite_driver: malformed pinned tool row for ${tool}")
    endif()
    set(expected_tool_sha "${CMAKE_MATCH_1}")
    file(SHA256 "${cc65_dir}/${tool}" actual_tool_sha)
    if(NOT actual_tool_sha STREQUAL expected_tool_sha)
      message(FATAL_ERROR "suite_driver: pinned fault-wrapper tool hash mismatch for ${tool}")
    endif()
    if(tool STREQUAL "bin/ca65")
      set(ca65_sha "${actual_tool_sha}")
    else()
      set(ld65_sha "${actual_tool_sha}")
    endif()
  endforeach()

  set(build_record "${SUITE_BUILD}/build-record.txt")
  file(READ "${build_record}" record_text)
  file(SHA256 "${default_wrapper}" default_wrapper_sha)
  file(SHA256 "${suite_source}/tests.s" tests_sha_before)
  if(NOT record_text MATCHES "${suite}_wrapper_sha256=${default_wrapper_sha}" OR
     NOT record_text MATCHES "${suite}_source_sha256=([0-9a-f]+)")
    message(FATAL_ERROR "suite_driver: ${suite} default wrapper or source identity is not bound by the verified build")
  endif()
  set(record_tests_sha "${CMAKE_MATCH_1}")
  if(NOT tests_sha_before STREQUAL record_tests_sha)
    message(FATAL_ERROR "suite_driver: ${suite} pinned tests.s changed before fault-wrapper build")
  endif()

  execute_process(
    COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin" "LC_ALL=C" "CC65_HOME=${cc65_dir}"
            "${cc65_dir}/bin/ca65" --cpu 65C02 -I "${suite_source}" -I "${cc65_dir}/asminc"
            -o "${fault_dir}/${suite}.o" "${fault_wrapper}"
    WORKING_DIRECTORY "${suite_source}" TIMEOUT 30
    RESULT_VARIABLE ca65_rc OUTPUT_VARIABLE ca65_out ERROR_VARIABLE ca65_err)
  if(NOT ca65_rc STREQUAL "0")
    message(FATAL_ERROR "suite_driver: ${suite} fault-wrapper assembly failed: ${ca65_rc} ${ca65_out} ${ca65_err}")
  endif()
  execute_process(
    COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin" "LC_ALL=C" "CC65_HOME=${cc65_dir}"
            "${cc65_dir}/bin/ld65" -C "${ROOT}/fixtures/suites/bll.cfg"
            -Ln "${fault_dir}/${suite}.lbl" -o "${fault_dir}/${suite}.bll" "${fault_dir}/${suite}.o"
    TIMEOUT 30 RESULT_VARIABLE ld65_rc OUTPUT_VARIABLE ld65_out ERROR_VARIABLE ld65_err)
  if(NOT ld65_rc STREQUAL "0" OR NOT EXISTS "${fault_dir}/${suite}.bll" OR
     NOT EXISTS "${fault_dir}/${suite}.lbl")
    message(FATAL_ERROR "suite_driver: ${suite} fault-wrapper link failed: ${ld65_rc} ${ld65_out} ${ld65_err}")
  endif()

  file(SIZE "${SUITE_BUILD}/${suite}.bll" default_size)
  file(SIZE "${fault_dir}/${suite}.bll" fault_size)
  if(NOT default_size EQUAL fault_size)
    message(FATAL_ERROR "suite_driver: ${suite} sentinel omission changed linked object extent")
  endif()
  file(READ "${SUITE_BUILD}/${suite}.lbl" default_labels)
  file(READ "${fault_dir}/${suite}.lbl" fault_labels)
  if(NOT default_labels STREQUAL fault_labels)
    message(FATAL_ERROR "suite_driver: ${suite} sentinel omission changed linked addresses")
  endif()
  file(READ "${SUITE_BUILD}/${suite}.bll" default_bll_hex HEX)
  file(READ "${fault_dir}/${suite}.bll" fault_bll_hex HEX)
  string(REGEX MATCHALL "a9a58d[0-9a-f][0-9a-f][0-9a-f][0-9a-f]" sentinel_store_bytes "${default_bll_hex}")
  list(LENGTH sentinel_store_bytes sentinel_store_count)
  if(NOT sentinel_store_count EQUAL 1)
    message(FATAL_ERROR "suite_driver: ${suite} linked wrapper lacks one address-bound sentinel store")
  endif()
  list(GET sentinel_store_bytes 0 sentinel_store_hex)
  string(REPLACE "${sentinel_store_hex}" "eaeaeaeaea" expected_fault_bll_hex "${default_bll_hex}")
  if(NOT fault_bll_hex STREQUAL expected_fault_bll_hex)
    message(FATAL_ERROR "suite_driver: ${suite} linked fault differs by more than the five-byte sentinel store")
  endif()

  file(SHA256 "${fault_wrapper}" fault_wrapper_sha)
  file(SHA256 "${fault_dir}/${suite}.bll" fault_object_sha)
  string(REGEX REPLACE "${suite}_wrapper_sha256=[0-9a-f]+"
    "${suite}_wrapper_sha256=${fault_wrapper_sha}" fault_record "${record_text}")
  string(REGEX REPLACE "${suite}_object_sha256=[0-9a-f]+"
    "${suite}_object_sha256=${fault_object_sha}" fault_record "${fault_record}")
  file(WRITE "${fault_dir}/build-record.txt" "${fault_record}")
  file(COPY_FILE "${ORACLE}" "${fault_dir}/oracle.tsv")
  file(SHA256 "${suite_source}/tests.s" tests_sha_after)
  if(NOT tests_sha_after STREQUAL tests_sha_before)
    message(FATAL_ERROR "suite_driver: ${suite} pinned tests.s changed during fault-wrapper build")
  endif()
  file(WRITE "${fault_dir}/fault-provenance.txt"
    "fault=omit-post-return-sentinel\nsuite=${suite}\ndefault_wrapper_sha256=${default_wrapper_sha}\nfault_wrapper_sha256=${fault_wrapper_sha}\ntests_s_sha256=${tests_sha_after}\nca65_sha256=${ca65_sha}\nld65_sha256=${ld65_sha}\ndefault_linked_size=${default_size}\nfault_linked_size=${fault_size}\nlinked_label_identity=unchanged\nchanged_code_bytes=one five-byte sentinel store replaced by five NOPs\n")
  set(${suite}_fault_build "${fault_dir}" PARENT_SCOPE)
endfunction()

function(run_completion_positive suite profile)
  if(suite STREQUAL "cpu")
    expect_success_timeout("suite raw source comparison: match; suite=cpu machine_profile=${profile} profile_input=${profile} ram_seed=zero"
      60 "${TEST_BINARY}" run cpu "${profile}" "${SUITE_BUILD}" "${ORACLE}" zero)
    expect_success("cpu record: 18 raw results, completion and model evidence valid for ${profile}"
      "${TEST_BINARY}" validate-cpu-record "${profile}" "${SUITE_BUILD}/raw-results-zero.tsv"
      "${SUITE_BUILD}/run-record-zero.txt" "${ORACLE}" "${SUITE_BUILD}")
  else()
    run_timer_observation("${profile}")
  endif()
endfunction()

function(assert_no_fault_outputs suite profile directory)
  if(suite STREQUAL "cpu")
    set(names "raw-results-zero.tsv;run-record-zero.txt;comparison-zero.tsv")
  else()
    set(names "raw-results-${profile}.tsv;run-record-${profile}.txt;comparison-${profile}.tsv;cascade-proof-${profile}.json;pending-record-${profile}.json")
  endif()
  foreach(name IN LISTS names)
    if(EXISTS "${directory}/${name}")
      message(FATAL_ERROR "suite_driver: fault refusal produced downstream evidence ${name}")
    endif()
  endforeach()
endfunction()

function(seed_prior_completion_outputs suite profile destination case_name)
  set(source "${SUITE_BUILD}")
  if(suite STREQUAL "cpu")
    set(names "raw-results-zero.tsv;run-record-zero.txt;comparison-zero.tsv")
  else()
    set(names "raw-results-${profile}.tsv;run-record-${profile}.txt;comparison-${profile}.tsv;cascade-proof-${profile}.json;pending-record-${profile}.json")
  endif()
  set(provenance "${WORK}/completion-stale/${case_name}.tsv")
  get_filename_component(provenance_dir "${provenance}" DIRECTORY)
  file(MAKE_DIRECTORY "${provenance_dir}")
  file(WRITE "${provenance}" "file\tsha256\n")
  foreach(name IN LISTS names)
    if(NOT EXISTS "${source}/${name}")
      message(FATAL_ERROR "suite_driver: fresh positive did not produce stale-input seed ${suite}/${profile}/${name}")
    endif()
    file(SHA256 "${source}/${name}" source_sha)
    if(NOT source STREQUAL destination)
      file(COPY_FILE "${source}/${name}" "${destination}/${name}")
    endif()
    if(NOT EXISTS "${destination}/${name}")
      message(FATAL_ERROR "suite_driver: prior positive output was not staged for stale-input control: ${name}")
    endif()
    file(SHA256 "${destination}/${name}" copied_sha)
    if(NOT source_sha STREQUAL copied_sha)
      message(FATAL_ERROR "suite_driver: stale-input seed changed during scratch copy: ${name}")
    endif()
    file(APPEND "${provenance}" "${name}\t${copied_sha}\n")
  endforeach()
endfunction()

function(run_completion_fault suite profile fault)
  run_completion_positive("${suite}" "${profile}")
  if(fault STREQUAL "omit_sentinel")
    set(fault_dir "${${suite}_fault_build}")
    if(suite STREQUAL "cpu")
      set(maximum 100000)
    else()
      set(maximum 500000)
    endif()
    set(expected "completion: post-return sentinel absent after the suite returned")
  else()
    set(fault_dir "${SUITE_BUILD}")
    set(maximum 256)
    set(expected "completion: guest-time limit reached before the suite returned")
  endif()
  seed_prior_completion_outputs("${suite}" "${profile}" "${fault_dir}" "${suite}.${profile}.${fault}")
  execute_process(COMMAND "${TEST_BINARY}" run-bounded "${suite}" "${profile}"
          "${fault_dir}" "${ORACLE}" "${maximum}"
    TIMEOUT 60 RESULT_VARIABLE fault_rc OUTPUT_VARIABLE fault_out ERROR_VARIABLE fault_err)
  set(fault_text "${fault_out}${fault_err}")
  if(NOT fault_rc STREQUAL "1" OR NOT fault_text MATCHES "suite: ${expected}")
    message(FATAL_ERROR "suite_driver: ${suite}/${profile}/${fault} lacked the exact guest refusal: ${fault_rc} ${fault_out} ${fault_err}")
  endif()
  if(fault STREQUAL "omit_sentinel")
    if(NOT fault_text MATCHES "completion state: suite=${suite} execution_started=true progress=8 sentinel=00 ticks=${maximum} maximum=${maximum} post_return_watch=absent")
      message(FATAL_ERROR "suite_driver: ${suite}/${profile} omitted-sentinel case did not return from the real suite")
    endif()
  elseif(NOT fault_text MATCHES "completion state: suite=${suite} execution_started=true progress=[0-9]+ sentinel=00 ticks=256 maximum=256 post_return_watch=absent")
    message(FATAL_ERROR "suite_driver: ${suite}/${profile} guest-time bound case lacked actual guest execution evidence")
  endif()
  assert_no_fault_outputs("${suite}" "${profile}" "${fault_dir}")
  run_completion_positive("${suite}" "${profile}")
  if(fault STREQUAL "omit_sentinel")
    file(APPEND "${COMPLETION_MANIFEST}" "${suite}.${profile}.omit_sentinel\t${suite}\t${profile}\tactual_guest\tpost_return_sentinel_missing\tprior_positive_outputs_cleared\n")
  else()
    file(APPEND "${COMPLETION_MANIFEST}" "${suite}.${profile}.guest_time_bound\t${suite}\t${profile}\tactual_guest\tguest_time_limit_before_suite_return\tprior_positive_outputs_cleared\n")
  endif()
endfunction()

function(timer_assert_baseline_unchanged profile)
  set(base "${WORK}/timer-baseline/${profile}")
  set(manifest "${WORK}/timer-baseline/${profile}.sha256")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "suite_driver: timer baseline identity manifest is missing for ${profile}")
  endif()
  file(STRINGS "${manifest}" entries)
  list(LENGTH entries entry_count)
  if(entry_count LESS 10)
    message(FATAL_ERROR "suite_driver: timer baseline identity manifest is incomplete for ${profile}")
  endif()
  foreach(entry IN LISTS entries)
    if(NOT entry MATCHES "^([0-9a-f]+)  ([^ ]+)$")
      message(FATAL_ERROR "suite_driver: malformed timer baseline identity entry: ${entry}")
    endif()
    set(expected_sha "${CMAKE_MATCH_1}")
    set(file_name "${CMAKE_MATCH_2}")
    if(NOT EXISTS "${base}/${file_name}")
      message(FATAL_ERROR "suite_driver: immutable timer baseline file disappeared: ${profile}/${file_name}")
    endif()
    file(SHA256 "${base}/${file_name}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
      message(FATAL_ERROR "suite_driver: immutable timer baseline changed: ${profile}/${file_name}")
    endif()
  endforeach()
endfunction()

function(timer_batch_positive)
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/timer-baseline/${profile}")
    timer_assert_baseline_unchanged("${profile}")
    expect_success("timer record: complete observation, executable phase proof and pending row valid for ${profile}"
      "${TEST_BINARY}" validate-timer-record "${profile}"
      "${base}/raw-results-${profile}.tsv" "${base}/run-record-${profile}.txt"
      "${base}/oracle.tsv" "${base}/timer-source-index14-pending.json")
    timer_assert_baseline_unchanged("${profile}")
  endforeach()
endfunction()

function(timer_batch_begin)
  run_timer_pair()
  file(MAKE_DIRECTORY "${WORK}/timer-baseline")
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/timer-baseline/${profile}")
    if(EXISTS "${base}" OR IS_SYMLINK "${base}")
      message(FATAL_ERROR "suite_driver: timer baseline output directory already exists")
    endif()
    file(MAKE_DIRECTORY "${base}")
    file(COPY "${SUITE_BUILD}/" DESTINATION "${base}")
    write_suite_owner_marker("${base}")
    file(COPY_FILE "${ROOT}/docs/timer-source-index14-pending.json"
      "${base}/timer-source-index14-pending.json")
    set(manifest "${WORK}/timer-baseline/${profile}.sha256")
    file(WRITE "${manifest}" "")
    foreach(file_name
        "raw-results-${profile}.tsv" "run-record-${profile}.txt"
        "comparison-${profile}.tsv" "cascade-proof-${profile}.json"
        "pending-record-${profile}.json" "timers.bll" "timers.lbl"
        "build-record.txt" "oracle.tsv" "timer-source-index14-pending.json")
      if(NOT EXISTS "${base}/${file_name}")
        message(FATAL_ERROR "suite_driver: timer baseline capture is incomplete for ${profile}/${file_name}")
      endif()
      file(SHA256 "${base}/${file_name}" file_sha)
      file(APPEND "${manifest}" "${file_sha}  ${file_name}\n")
    endforeach()
  endforeach()
  set_property(GLOBAL PROPERTY TIMER_DATA_CASE_COUNT 0)
  set_property(GLOBAL PROPERTY TIMER_DATA_CASE_NAMES "")
  timer_batch_positive()
endfunction()

function(timer_case_begin case_name profile)
  timer_batch_positive()
  get_property(case_names GLOBAL PROPERTY TIMER_DATA_CASE_NAMES)
  if(case_names STREQUAL "")
    set(case_names "")
  endif()
  list(FIND case_names "${case_name}" duplicate_index)
  if(NOT duplicate_index EQUAL -1)
    message(FATAL_ERROR "suite_driver: duplicate timer subcase name: ${case_name}")
  endif()
  list(APPEND case_names "${case_name}")
  set_property(GLOBAL PROPERTY TIMER_DATA_CASE_NAMES "${case_names}")
  get_property(case_count GLOBAL PROPERTY TIMER_DATA_CASE_COUNT)
  if(case_count STREQUAL "")
    set(case_count 0)
  endif()
  math(EXPR case_count "${case_count} + 1")
  set_property(GLOBAL PROPERTY TIMER_DATA_CASE_COUNT "${case_count}")
  string(MAKE_C_IDENTIFIER "${case_name}" case_slug)
  set(case_dir "${WORK}/timer-cases/${case_count}-${case_slug}")
  if(EXISTS "${case_dir}" OR IS_SYMLINK "${case_dir}")
    message(FATAL_ERROR "suite_driver: timer data-control directory already exists")
  endif()
  file(MAKE_DIRECTORY "${case_dir}")
  file(COPY "${WORK}/timer-baseline/${profile}/" DESTINATION "${case_dir}")
  write_suite_owner_marker("${case_dir}")
  set(TIMER_CASE_DIR "${case_dir}" PARENT_SCOPE)
endfunction()

function(timer_case_end)
  timer_assert_baseline_unchanged(lynx1)
  timer_assert_baseline_unchanged(lynx2)
  timer_batch_positive()
endfunction()

function(timer_batch_end)
  get_property(case_count GLOBAL PROPERTY TIMER_DATA_CASE_COUNT)
  get_property(case_names GLOBAL PROPERTY TIMER_DATA_CASE_NAMES)
  if(MODE STREQUAL "timer_result_controls")
    set(expected_names
      "copied-lynx2-profile"
      "comparator-8-04" "comparator-8-05" "comparator-11-35" "comparator-11-36" "comparator-11-37"
      "comparator-8-03" "comparator-8-06" "comparator-11-34" "comparator-11-38"
      "record-model_probe_bytes-__REMOVE__" "record-model_probe_bytes-ff,a1"
      "record-detected_model-lynx2" "record-guest_probe_byte-00" "record-guest_model_marker-a2"
      "record-meaningful_count-15" "record-reserved_count-1" "record-result_extent-17"
      "record-meaningful_results-17" "record-reserved_results-3"
      "record-completion_sentinel-00" "record-completion_sentinel-__REMOVE__"
      "record-guest_progress-7" "record-cost_ticks-6" "record-sample_offset_ticks-5"
      "record-counter_initialization-seeded" "record-page_mode_video_enabled-true"
      "record-refresh_dma_enabled-true" "record-phase_policy-timer-phase-shifted"
      "record-scheduler_policy-timer-cpu-reversed" "record-profile_input-lynx2"
      "record-bll_sha256-0000000000000000000000000000000000000000000000000000000000000000"
      "record-labels_sha256-0000000000000000000000000000000000000000000000000000000000000000"
      "record-oracle_sha256-0000000000000000000000000000000000000000000000000000000000000000"
      "record-wrapper_sha256-0000000000000000000000000000000000000000000000000000000000000000"
      "record-tick_min-255"
      "ticks-255" "ticks-50000001" "ticks-4294967296"
      "zero-result-block" "index14-0b" "index14-0d" "second-mismatch"
      "raw-row-missing-15" "raw-index14-duplicate" "raw-extra-row"
      "comparison-row-missing-15" "comparison-index14-duplicate" "comparison-summary-mismatch")
    set(expected_count 49)
  elseif(MODE STREQUAL "timer_pending_controls")
    set(expected_names
      "symlink-output-publication"
      "identity-revision-strict" "identity-source-self-consistent-no-run"
      "identity-object-strict" "identity-object-no-run" "identity-labels-proof"
      "descriptor-missing-file" "descriptor-malformed" "descriptor-unknown-key" "descriptor-duplicate-key"
      "descriptor-missing-schema" "descriptor-missing-version" "descriptor-unsupported-schema"
      "descriptor-unsupported-version" "descriptor-actual-type" "descriptor-source-index"
      "descriptor-expected-source-set" "descriptor-actual-value" "descriptor-profiles"
      "descriptor-revisit-phase" "descriptor-pin-revision" "descriptor-archive-hash"
      "descriptor-cc65-hash" "descriptor-timers-tests-hash" "descriptor-timers-main-hash"
      "descriptor-oracle-hash" "descriptor-wrapper-hash" "descriptor-build-recipe-hash"
      "descriptor-oracle-generator-hash" "descriptor-phase-policy" "descriptor-scheduler-policy"
      "descriptor-cost-model" "descriptor-cost-ticks" "descriptor-sample-offset" "descriptor-video-policy"
      "descriptor-dma-policy" "descriptor-counter-init" "descriptor-proof-report-path"
      "descriptor-proof-report-hash"
      "companion-malformed" "companion-unknown-key" "companion-duplicate-key"
      "companion-missing-schema" "companion-unsupported-schema" "companion-version-type"
      "companion-actual-type" "companion-actual-value" "companion-version-value"
      "companion-missing-version" "companion-missing-file" "companion-raw-hash"
      "companion-record-hash" "companion-comparison-hash" "companion-bll-hash"
      "companion-labels-hash" "companion-oracle-hash" "companion-descriptor-hash"
      "companion-proof-hash" "companion-self-hash" "proof-corrupt" "proof-missing")
    set(expected_count 61)
  else()
    message(FATAL_ERROR "suite_driver: timer batch has no exact manifest for ${MODE}")
  endif()
  if(NOT case_count EQUAL expected_count)
    message(FATAL_ERROR "suite_driver: timer ${MODE} emitted ${case_count} subcases; exact inventory requires ${expected_count}")
  endif()
  if(NOT "${case_names}" STREQUAL "${expected_names}")
    message(FATAL_ERROR "suite_driver: timer ${MODE} emitted a different or reordered subcase name inventory")
  endif()
  set(manifest_expected "${WORK}/timer-subcases.expected.txt")
  set(manifest_actual "${WORK}/timer-subcases.actual.txt")
  set(expected_text "")
  set(actual_text "")
  foreach(name IN LISTS expected_names)
    string(APPEND expected_text "${name}\n")
  endforeach()
  foreach(name IN LISTS case_names)
    string(APPEND actual_text "${name}\n")
  endforeach()
  file(WRITE "${manifest_expected}" "${expected_text}")
  file(WRITE "${manifest_actual}" "${actual_text}")
  assert_timer_subcase_manifest("${manifest_expected}" "${manifest_actual}" "")
  set(incomplete_names "${expected_names}")
  list(REMOVE_AT incomplete_names 0)
  set(missing_manifest "${WORK}/timer-subcases-missing.txt")
  set(missing_text "")
  foreach(name IN LISTS incomplete_names)
    string(APPEND missing_text "${name}\n")
  endforeach()
  file(WRITE "${missing_manifest}" "${missing_text}")
  list(GET expected_names 0 first_expected_name)
  assert_timer_subcase_manifest("${manifest_expected}" "${missing_manifest}"
    "suite_driver: timer subcase manifest missing name: ${first_expected_name}")
  set(duplicate_manifest "${WORK}/timer-subcases-duplicate.txt")
  file(WRITE "${duplicate_manifest}" "${expected_text}${first_expected_name}\n")
  assert_timer_subcase_manifest("${manifest_expected}" "${duplicate_manifest}"
    "suite_driver: timer subcase manifest duplicate name: ${first_expected_name}")
  timer_batch_positive()
  run_timer_pair()
endfunction()

function(run_timer_comparator_value index expected_set to_value accepted)
  timer_case_begin("comparator-${index}-${to_value}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(raw_base "${case_dir}/raw-results-lynx1.tsv")
  set(comparison_base "${case_dir}/comparison-lynx1.tsv")
  file(READ "${raw_base}" raw_text)
  file(READ "${comparison_base}" comparison_text)
  file(STRINGS "${raw_base}" raw_index_rows REGEX "^result\t${index}\t")
  file(STRINGS "${comparison_base}" comparison_index_rows REGEX "^result\t${index}\t")
  list(LENGTH raw_index_rows raw_index_count)
  list(LENGTH comparison_index_rows comparison_index_count)
  if(NOT raw_index_count EQUAL 1 OR NOT comparison_index_count EQUAL 1)
    message(FATAL_ERROR "suite_driver: comparator corpus does not contain one exact source row ${index}")
  endif()
  list(GET raw_index_rows 0 raw_row)
  list(GET comparison_index_rows 0 comparison_row)
  if(NOT raw_row MATCHES "^result\t${index}\t([0-9a-f][0-9a-f])$")
    message(FATAL_ERROR "suite_driver: malformed comparator raw row ${index}: ${raw_row}")
  endif()
  set(from_value "${CMAKE_MATCH_1}")
  if(NOT comparison_row STREQUAL "result\t${index}\t${from_value}\tEXPECT(${expected_set})\tsource-accepted")
    message(FATAL_ERROR "suite_driver: comparator row ${index} was not the expected source-accepted row: ${comparison_row}")
  endif()
  if(accepted)
    set(status source-accepted)
  else()
    set(status source-mismatch)
  endif()
  if(NOT from_value STREQUAL to_value)
    string(REPLACE "${raw_row}\n" "result\t${index}\t${to_value}\n" changed_raw "${raw_text}")
    string(REPLACE "${comparison_row}"
                   "result\t${index}\t${to_value}\tEXPECT(${expected_set})\t${status}"
                   changed_comparison "${comparison_text}")
    if(changed_raw STREQUAL "${raw_text}" OR changed_comparison STREQUAL "${comparison_text}")
      message(FATAL_ERROR "suite_driver: comparator corpus replacement for result ${index} did not occur exactly once")
    endif()
  else()
    set(changed_raw "${raw_text}")
    set(changed_comparison "${comparison_text}")
  endif()
  file(WRITE "${raw_base}" "${changed_raw}")
  file(WRITE "${comparison_base}" "${changed_comparison}")
  if(accepted)
    expect_success("captured comparator corpus: 16 rows, 15 accepted, exact pending index14; not guest execution"
      "${TEST_BINARY}" validate-timer-comparison lynx1 "${case_dir}" "${raw_base}"
      "${comparison_base}" "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  else()
    expect_failure("timer comparator corpus: alternatives or exact pending row mismatch"
      "${TEST_BINARY}" validate-timer-comparison lynx1 "${case_dir}" "${raw_base}"
      "${comparison_base}" "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  endif()
  timer_case_end()
endfunction()

function(timer_record_control field old_value new_value expected)
  timer_case_begin("record-${field}-${new_value}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(record "${case_dir}/run-record-lynx1.txt")
  file(READ "${record}" record_text)
  if(new_value STREQUAL "__REMOVE__")
    string(REPLACE "${field}=${old_value}\n" "" changed "${record_text}")
  else()
    string(REPLACE "${field}=${old_value}\n" "${field}=${new_value}\n" changed "${record_text}")
  endif()
  if(changed STREQUAL record_text)
    message(FATAL_ERROR "suite_driver: timer record control did not mutate exactly ${field}")
  endif()
  file(WRITE "${record}" "${changed}")
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${case_dir}/raw-results-lynx1.tsv" "${record}" "${case_dir}/oracle.tsv"
      "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_build_identity_control case_name mutation entry_mode expected)
  timer_case_begin("identity-${case_name}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(build_record_path "${case_dir}/build-record.txt")
  file(READ "${build_record_path}" build_record)
  if(mutation STREQUAL "revision")
    string(REGEX REPLACE "lynx_tests_revision=[0-9a-f]+" "lynx_tests_revision=0000000000000000000000000000000000000000" changed_record "${build_record}")
    if(changed_record STREQUAL build_record)
      message(FATAL_ERROR "suite_driver: timer revision identity control did not mutate the intended field")
    endif()
    file(WRITE "${build_record_path}" "${changed_record}")
  elseif(mutation STREQUAL "source-self-consistent")
    set(zero_sha "0000000000000000000000000000000000000000000000000000000000000000")
    set(oracle_path "${case_dir}/oracle.tsv")
    file(READ "${oracle_path}" oracle_text)
    string(REGEX REPLACE "timers_main\t[0-9a-f]+" "timers_main\t${zero_sha}" changed_oracle "${oracle_text}")
    string(REGEX REPLACE "timers_main_sha256=[0-9a-f]+" "timers_main_sha256=${zero_sha}" changed_record "${build_record}")
    if(changed_oracle STREQUAL oracle_text OR changed_record STREQUAL build_record)
      message(FATAL_ERROR "suite_driver: self-consistent timer source identity control did not mutate both source records")
    endif()
    file(WRITE "${oracle_path}" "${changed_oracle}")
    file(WRITE "${build_record_path}" "${changed_record}")
  elseif(mutation STREQUAL "object")
    file(APPEND "${case_dir}/timers.bll" "changed")
  elseif(mutation STREQUAL "labels")
    file(APPEND "${case_dir}/timers.lbl" "changed\n")
  else()
    message(FATAL_ERROR "suite_driver: unknown timer build identity mutation ${mutation}")
  endif()

  set(names "raw-results-lynx1.tsv;run-record-lynx1.txt;comparison-lynx1.tsv;cascade-proof-lynx1.json;pending-record-lynx1.json")
  foreach(name IN LISTS names)
    string(MAKE_C_IDENTIFIER "${name}" name_id)
    file(SHA256 "${case_dir}/${name}" before_sha)
    set_property(GLOBAL PROPERTY "TIMER_IDENTITY_BEFORE_${name_id}" "${before_sha}")
  endforeach()
  if(entry_mode STREQUAL "strict")
    expect_failure("${expected}" "${TEST_BINARY}" observe timers lynx1 "${case_dir}"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  elseif(entry_mode STREQUAL "no-run")
    expect_failure("${expected}" "${TEST_BINARY}" no-run timers lynx1 "${case_dir}"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  elseif(entry_mode STREQUAL "proof")
    expect_failure("${expected}" "${TEST_BINARY}" timer-cascade-model lynx1 "${case_dir}"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json"
      "${ROOT}/build/pins/lynx-tests")
  else()
    message(FATAL_ERROR "suite_driver: unknown timer build identity entry point ${entry_mode}")
  endif()
  foreach(name IN LISTS names)
    string(MAKE_C_IDENTIFIER "${name}" name_id)
    get_property(before_sha GLOBAL PROPERTY "TIMER_IDENTITY_BEFORE_${name_id}")
    file(SHA256 "${case_dir}/${name}" after_sha)
    if(NOT after_sha STREQUAL before_sha)
      message(FATAL_ERROR "suite_driver: rejected timer identity mutation changed seeded output ${name}")
    endif()
  endforeach()
  timer_case_end()
endfunction()

function(timer_symlink_output_control)
  timer_case_begin("symlink-output-publication" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(outside_dir "${WORK}/outside-output-fixtures")
  file(MAKE_DIRECTORY "${outside_dir}")
  file(WRITE "${outside_dir}/proof-target.json" "preserve external proof target\n")
  file(WRITE "${outside_dir}/record-target.txt" "preserve external timer record\n")
  file(WRITE "${outside_dir}/companion-target.json" "preserve external companion target\n")
  file(SHA256 "${outside_dir}/proof-target.json" proof_before)
  file(SHA256 "${outside_dir}/record-target.txt" record_before)
  file(SHA256 "${outside_dir}/companion-target.json" companion_before)

  file(REMOVE "${case_dir}/cascade-proof-lynx1.json")
  file(CREATE_LINK "${outside_dir}/proof-target.json" "${case_dir}/cascade-proof-lynx1.json"
    SYMBOLIC RESULT proof_link_result)
  if(NOT proof_link_result STREQUAL "0")
    message(FATAL_ERROR "suite_driver: proof output symlink control could not create its fixture: ${proof_link_result}")
  endif()
  expect_success("timer cascade model: 64 phase offsets derive 12 acknowledgments for lynx1"
    "${TEST_BINARY}" timer-cascade-model lynx1 "${case_dir}" "${case_dir}/oracle.tsv"
    "${case_dir}/timer-source-index14-pending.json" "${ROOT}/build/pins/lynx-tests")
  file(SHA256 "${outside_dir}/proof-target.json" proof_after)
  if(NOT proof_after STREQUAL proof_before OR IS_SYMLINK "${case_dir}/cascade-proof-lynx1.json" OR
     NOT EXISTS "${case_dir}/cascade-proof-lynx1.json" OR IS_DIRECTORY "${case_dir}/cascade-proof-lynx1.json")
    message(FATAL_ERROR "suite_driver: proof writer followed or published a symlink target")
  endif()

  file(REMOVE "${case_dir}/run-record-lynx1.txt" "${case_dir}/pending-record-lynx1.json")
  file(CREATE_LINK "${outside_dir}/record-target.txt" "${case_dir}/run-record-lynx1.txt"
    SYMBOLIC RESULT record_link_result)
  file(CREATE_LINK "${outside_dir}/companion-target.json" "${case_dir}/pending-record-lynx1.json"
    SYMBOLIC RESULT companion_link_result)
  if(NOT record_link_result STREQUAL "0" OR NOT companion_link_result STREQUAL "0")
    message(FATAL_ERROR "suite_driver: run/companion output symlink controls could not create their fixtures")
  endif()
  execute_process(COMMAND "${TEST_BINARY}" observe timers lynx1 "${case_dir}"
    "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json"
    TIMEOUT 60 RESULT_VARIABLE observe_rc OUTPUT_VARIABLE observe_out ERROR_VARIABLE observe_err)
  if(NOT observe_rc STREQUAL "0" OR
     NOT observe_out MATCHES "timer observation complete: profile=lynx1 meaningful=16 reserved=2 source_agreements=15 pending_index=14 actual=0c expected=0d" OR
     IS_SYMLINK "${case_dir}/run-record-lynx1.txt" OR IS_SYMLINK "${case_dir}/pending-record-lynx1.json" OR
     NOT EXISTS "${case_dir}/run-record-lynx1.txt" OR NOT EXISTS "${case_dir}/pending-record-lynx1.json")
    message(FATAL_ERROR "suite_driver: clear/observation path failed to replace owned output symlinks: ${observe_rc} ${observe_out} ${observe_err}")
  endif()
  file(SHA256 "${outside_dir}/proof-target.json" proof_after)
  file(SHA256 "${outside_dir}/record-target.txt" record_after)
  file(SHA256 "${outside_dir}/companion-target.json" companion_after)
  if(NOT proof_after STREQUAL proof_before OR NOT record_after STREQUAL record_before OR
     NOT companion_after STREQUAL companion_before)
    message(FATAL_ERROR "suite_driver: output symlink control changed an outside target")
  endif()
  file(REMOVE "${case_dir}/run-record-lynx1.txt")
  file(MAKE_DIRECTORY "${case_dir}/run-record-lynx1.txt")
  expect_failure("suite: cannot clear stale suite output"
    "${TEST_BINARY}" observe timers lynx1 "${case_dir}" "${case_dir}/oracle.tsv"
    "${case_dir}/timer-source-index14-pending.json")
  if(NOT IS_DIRECTORY "${case_dir}/run-record-lynx1.txt")
    message(FATAL_ERROR "suite_driver: output cleanup removed a directory")
  endif()
  timer_case_end()
endfunction()

function(timer_tick_control ticks expected)
  timer_case_begin("ticks-${ticks}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(record "${case_dir}/run-record-lynx1.txt")
  file(READ "${record}" record_text)
  string(REGEX MATCH "ticks_observed=([0-9]+)" tick_match "${record_text}")
  set(old_ticks "${CMAKE_MATCH_1}")
  if(old_ticks STREQUAL "")
    message(FATAL_ERROR "suite_driver: timer record has no ticks_observed row")
  endif()
  string(REPLACE "ticks_observed=${old_ticks}" "ticks_observed=${ticks}" changed "${record_text}")
  string(REPLACE "guest_ticks=${old_ticks}" "guest_ticks=${ticks}" changed "${changed}")
  file(WRITE "${record}" "${changed}")
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${case_dir}/raw-results-lynx1.tsv" "${record}" "${case_dir}/oracle.tsv"
      "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_descriptor_control case_name needle replacement expected)
  timer_case_begin("${case_name}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(descriptor "${case_dir}/pending-descriptor-mutated.json")
  file(READ "${ROOT}/docs/timer-source-index14-pending.json" descriptor_text)
  if(replacement STREQUAL "__MALFORMED__")
    set(changed "{")
  elseif(replacement STREQUAL "__MISSING_FILE__")
    set(descriptor "${case_dir}/pending-descriptor-absent.json")
    file(REMOVE "${descriptor}")
    set(changed "")
  elseif(replacement STREQUAL "__DUPLICATE__")
    set(changed "{\n  \"schema\":\"lynxplosion.timer-source-pending\",\n  \"schema\":\"lynxplosion.timer-source-pending\"\n}\n")
  else()
    string(REPLACE "${needle}" "${replacement}" changed "${descriptor_text}")
    if(changed STREQUAL descriptor_text)
      message(FATAL_ERROR "suite_driver: pending descriptor control did not mutate field: ${needle}")
    endif()
  endif()
  if(NOT replacement STREQUAL "__MISSING_FILE__")
    file(WRITE "${descriptor}" "${changed}")
  endif()
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${case_dir}/raw-results-lynx1.tsv" "${case_dir}/run-record-lynx1.txt"
      "${case_dir}/oracle.tsv" "${descriptor}")
  timer_case_end()
endfunction()

function(timer_companion_control case_name needle replacement expected)
  timer_case_begin("${case_name}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(companion "${case_dir}/pending-record-lynx1.json")
  file(READ "${companion}" companion_text)
  if(replacement STREQUAL "__MALFORMED__")
    set(changed "{")
  elseif(replacement STREQUAL "__MISSING_FILE__")
    file(REMOVE "${companion}")
    set(changed "")
  elseif(replacement STREQUAL "__DUPLICATE__")
    set(changed "{\n  \"schema\":\"lynxplosion.timer-source-pending-record\",\n  \"schema\":\"lynxplosion.timer-source-pending-record\"\n}\n")
  elseif(replacement STREQUAL "__REMOVE_FIELD__")
    string(REPLACE "${needle}" "" changed "${companion_text}")
  elseif(replacement STREQUAL "__UNKNOWN__")
    string(REPLACE "\n}" ",\n  \"unknown_key\":true\n}" changed "${companion_text}")
  elseif(replacement STREQUAL "__SELF_HASH__")
    string(REPLACE "\n}" ",\n  \"companion_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\"\n}" changed "${companion_text}")
  elseif(needle MATCHES "^HASH:([A-Za-z0-9_]+)$")
    set(hash_key "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "\"${hash_key}\":\"[0-9a-f]+\""
      "\"${hash_key}\":\"0000000000000000000000000000000000000000000000000000000000000000\""
      changed "${companion_text}")
  else()
    string(REPLACE "${needle}" "${replacement}" changed "${companion_text}")
  endif()
  if(NOT replacement STREQUAL "__MISSING_FILE__" AND changed STREQUAL companion_text)
    message(FATAL_ERROR "suite_driver: timer companion control did not change its intended field")
  endif()
  if(NOT replacement STREQUAL "__MISSING_FILE__")
    file(WRITE "${companion}" "${changed}")
  endif()
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${case_dir}/raw-results-lynx1.tsv" "${case_dir}/run-record-lynx1.txt"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_proof_control replacement expected)
  if(replacement STREQUAL "__CORRUPT__")
    set(case_name "proof-corrupt")
  else()
    set(case_name "proof-missing")
  endif()
  timer_case_begin("${case_name}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(proof "${case_dir}/cascade-proof-lynx1.json")
  if(replacement STREQUAL "__CORRUPT__")
    file(WRITE "${proof}" "{\n")
  else()
    file(REMOVE "${proof}")
  endif()
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${case_dir}/raw-results-lynx1.tsv" "${case_dir}/run-record-lynx1.txt"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_captured_file_control case_name kind needle replacement expected)
  timer_case_begin("${case_name}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  if(kind STREQUAL "raw")
    set(path "${case_dir}/raw-results-lynx1.tsv")
    set(record "${case_dir}/run-record-lynx1.txt")
  elseif(kind STREQUAL "comparison")
    set(path "${case_dir}/comparison-lynx1.tsv")
    set(record "${case_dir}/run-record-lynx1.txt")
  else()
    message(FATAL_ERROR "suite_driver: unknown timer captured-file control kind ${kind}")
  endif()
  file(READ "${path}" file_text)
  if(replacement STREQUAL "__REMOVE__")
    string(REPLACE "${needle}" "" changed "${file_text}")
  else()
    string(REPLACE "${needle}" "${replacement}" changed "${file_text}")
  endif()
  if(changed STREQUAL file_text)
    message(FATAL_ERROR "suite_driver: timer ${kind} control failed to mutate the intended row")
  endif()
  file(WRITE "${path}" "${changed}")
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-timer-record lynx1 "${case_dir}/raw-results-lynx1.tsv"
      "${record}" "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_zero_result_control)
  timer_case_begin("zero-result-block" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  file(STRINGS "${case_dir}/raw-results-lynx1.tsv" raw_lines)
  set(zero_raw "")
  foreach(raw_line IN LISTS raw_lines)
    if(raw_line MATCHES "^result\t([0-9]+)\t[0-9a-f][0-9a-f]$")
      set(index "${CMAKE_MATCH_1}")
      string(APPEND zero_raw "result\t${index}\t00\n")
    else()
      string(APPEND zero_raw "${raw_line}\n")
    endif()
  endforeach()
  file(WRITE "${case_dir}/raw-results-lynx1.tsv" "${zero_raw}")
  expect_failure("timer record: zero/unexecuted result block"
    "${TEST_BINARY}" validate-timer-record lynx1 "${case_dir}/raw-results-lynx1.tsv"
      "${case_dir}/run-record-lynx1.txt" "${case_dir}/oracle.tsv"
      "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_index14_control value)
  timer_case_begin("index14-${value}" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(raw "${case_dir}/raw-results-lynx1.tsv")
  set(comparison "${case_dir}/comparison-lynx1.tsv")
  file(READ "${raw}" raw_text)
  file(READ "${comparison}" comparison_text)
  string(REPLACE "result\t14\t0c\n" "result\t14\t${value}\n" changed_raw "${raw_text}")
  if(value STREQUAL "0d")
    set(status source-accepted)
    string(REPLACE "comparison\tmismatch\n" "comparison\tmatch\n" comparison_text "${comparison_text}")
  else()
    set(status source-mismatch)
  endif()
  string(REPLACE "result\t14\t0c\tEXPECT(0d)\tsource-mismatch"
                 "result\t14\t${value}\tEXPECT(0d)\t${status}"
                 changed_comparison "${comparison_text}")
  if(changed_raw STREQUAL raw_text OR changed_comparison STREQUAL comparison_text)
    message(FATAL_ERROR "suite_driver: index14 control did not mutate the raw/comparison row")
  endif()
  file(WRITE "${raw}" "${changed_raw}")
  file(WRITE "${comparison}" "${changed_comparison}")
  expect_failure("timer observation: source comparison is outside the exact pending row"
    "${TEST_BINARY}" validate-timer-record lynx1 "${raw}" "${case_dir}/run-record-lynx1.txt"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(timer_second_mismatch_control)
  timer_case_begin("second-mismatch" lynx1)
  set(case_dir "${TIMER_CASE_DIR}")
  set(raw "${case_dir}/raw-results-lynx1.tsv")
  set(comparison "${case_dir}/comparison-lynx1.tsv")
  file(READ "${raw}" raw_text)
  file(READ "${comparison}" comparison_text)
  string(REPLACE "result\t0\t08\n" "result\t0\t09\n" changed_raw "${raw_text}")
  string(REPLACE "result\t0\t08\tEXPECT(08)\tsource-accepted"
                 "result\t0\t09\tEXPECT(08)\tsource-mismatch"
                 changed_comparison "${comparison_text}")
  if(changed_raw STREQUAL raw_text OR changed_comparison STREQUAL comparison_text)
    message(FATAL_ERROR "suite_driver: second-mismatch control did not mutate the intended row")
  endif()
  file(WRITE "${raw}" "${changed_raw}")
  file(WRITE "${comparison}" "${changed_comparison}")
  expect_failure("timer observation: source comparison is outside the exact pending row"
    "${TEST_BINARY}" validate-timer-record lynx1 "${raw}" "${case_dir}/run-record-lynx1.txt"
      "${case_dir}/oracle.tsv" "${case_dir}/timer-source-index14-pending.json")
  timer_case_end()
endfunction()

function(run_cpu_profile_pair)
  foreach(profile lynx1 lynx2)
    expect_success_timeout("suite raw source comparison: match; suite=cpu machine_profile=${profile} profile_input=${profile} ram_seed=zero"
      60 "${TEST_BINARY}" run cpu "${profile}" "${SUITE_BUILD}" "${ORACLE}" zero)
    expect_success("cpu record: 18 raw results, completion and model evidence valid for ${profile}"
      "${TEST_BINARY}" validate-cpu-record "${profile}"
        "${SUITE_BUILD}/raw-results-zero.tsv"
        "${SUITE_BUILD}/run-record-zero.txt" "${ORACLE}" "${SUITE_BUILD}")
  endforeach()
endfunction()

function(cpu_assert_baseline_unchanged profile)
  set(base "${WORK}/cpu-baseline/${profile}")
  set(manifest "${WORK}/cpu-baseline/${profile}.sha256")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "suite_driver: CPU baseline identity manifest is missing for ${profile}")
  endif()
  file(STRINGS "${manifest}" entries)
  list(LENGTH entries entry_count)
  if(NOT entry_count EQUAL 7)
    message(FATAL_ERROR "suite_driver: CPU baseline identity manifest is incomplete for ${profile}")
  endif()
  foreach(entry IN LISTS entries)
    if(NOT entry MATCHES "^([0-9a-f]+)  ([^ ]+)$")
      message(FATAL_ERROR "suite_driver: malformed CPU baseline identity entry: ${entry}")
    endif()
    set(expected_sha "${CMAKE_MATCH_1}")
    set(file_name "${CMAKE_MATCH_2}")
    if(NOT EXISTS "${base}/${file_name}")
      message(FATAL_ERROR "suite_driver: immutable CPU baseline file disappeared: ${profile}/${file_name}")
    endif()
    file(SHA256 "${base}/${file_name}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
      message(FATAL_ERROR "suite_driver: immutable CPU baseline changed: ${profile}/${file_name}")
    endif()
  endforeach()
endfunction()

function(cpu_batch_positive)
  foreach(profile lynx1 lynx2)
    set(base "${WORK}/cpu-baseline/${profile}")
    cpu_assert_baseline_unchanged("${profile}")
    expect_success("cpu record: 18 raw results, completion and model evidence valid for ${profile}"
      "${TEST_BINARY}" validate-cpu-record "${profile}"
      "${base}/raw-results-zero.tsv" "${base}/run-record-zero.txt"
      "${base}/oracle.tsv" "${base}")
    cpu_assert_baseline_unchanged("${profile}")
  endforeach()
endfunction()

function(cpu_batch_begin)
  file(MAKE_DIRECTORY "${WORK}/cpu-baseline")
  foreach(profile lynx1 lynx2)
    expect_success_timeout("suite raw source comparison: match; suite=cpu machine_profile=${profile} profile_input=${profile} ram_seed=zero"
      60 "${TEST_BINARY}" run cpu "${profile}" "${SUITE_BUILD}" "${ORACLE}" zero)
    expect_success("cpu record: 18 raw results, completion and model evidence valid for ${profile}"
      "${TEST_BINARY}" validate-cpu-record "${profile}"
      "${SUITE_BUILD}/raw-results-zero.tsv" "${SUITE_BUILD}/run-record-zero.txt"
      "${ORACLE}" "${SUITE_BUILD}")
    set(base "${WORK}/cpu-baseline/${profile}")
    if(EXISTS "${base}" OR IS_SYMLINK "${base}")
      message(FATAL_ERROR "suite_driver: CPU baseline output directory already exists")
    endif()
    file(MAKE_DIRECTORY "${base}")
    foreach(file_name raw-results-zero.tsv run-record-zero.txt comparison-zero.tsv cpu.bll cpu.lbl build-record.txt)
      if(NOT EXISTS "${SUITE_BUILD}/${file_name}")
        message(FATAL_ERROR "suite_driver: CPU baseline capture is incomplete for ${profile}/${file_name}")
      endif()
      file(COPY_FILE "${SUITE_BUILD}/${file_name}" "${base}/${file_name}")
    endforeach()
    file(COPY_FILE "${ORACLE}" "${base}/oracle.tsv")
    write_suite_owner_marker("${base}")
    set(manifest "${WORK}/cpu-baseline/${profile}.sha256")
    file(WRITE "${manifest}" "")
    foreach(file_name raw-results-zero.tsv run-record-zero.txt comparison-zero.tsv cpu.bll cpu.lbl build-record.txt oracle.tsv)
      file(SHA256 "${base}/${file_name}" file_sha)
      file(APPEND "${manifest}" "${file_sha}  ${file_name}\n")
    endforeach()
    cpu_assert_baseline_unchanged("${profile}")
  endforeach()
  cpu_batch_positive()
endfunction()

function(expect_cpu_record_failure expected raw_path record_path)
  if(ARGC GREATER 3)
    set(profile "${ARGV3}")
  else()
    set(profile lynx1)
  endif()
  set(base "${WORK}/cpu-baseline/${profile}")
  cpu_batch_positive()
  cpu_assert_baseline_unchanged(lynx1)
  cpu_assert_baseline_unchanged(lynx2)
  expect_failure("${expected}" "${TEST_BINARY}" validate-cpu-record "${profile}"
    "${raw_path}" "${record_path}" "${base}/oracle.tsv" "${base}")
  cpu_assert_baseline_unchanged(lynx1)
  cpu_assert_baseline_unchanged(lynx2)
  cpu_batch_positive()
endfunction()

function(cpu_build_identity_control case_name mutation expected)
  cpu_batch_positive()
  set(base "${WORK}/cpu-baseline/lynx1")
  set(case_dir "${WORK}/cpu-identity-cases/${case_name}")
  if(EXISTS "${case_dir}" OR IS_SYMLINK "${case_dir}")
    message(FATAL_ERROR "suite_driver: CPU identity-control directory already exists")
  endif()
  file(MAKE_DIRECTORY "${case_dir}")
  foreach(file_name raw-results-zero.tsv run-record-zero.txt comparison-zero.tsv cpu.bll cpu.lbl build-record.txt oracle.tsv)
    file(COPY_FILE "${base}/${file_name}" "${case_dir}/${file_name}")
  endforeach()
  write_suite_owner_marker("${case_dir}")
  file(READ "${case_dir}/build-record.txt" build_record)
  if(mutation STREQUAL "revision")
    string(REGEX REPLACE "lynx_tests_revision=[0-9a-f]+" "lynx_tests_revision=0000000000000000000000000000000000000000" changed_record "${build_record}")
    if(changed_record STREQUAL build_record)
      message(FATAL_ERROR "suite_driver: CPU revision identity control did not mutate the intended field")
    endif()
    file(WRITE "${case_dir}/build-record.txt" "${changed_record}")
  elseif(mutation STREQUAL "source-self-consistent")
    set(zero_sha "0000000000000000000000000000000000000000000000000000000000000000")
    file(READ "${case_dir}/oracle.tsv" oracle_text)
    string(REGEX REPLACE "cpu_main\t[0-9a-f]+" "cpu_main\t${zero_sha}" changed_oracle "${oracle_text}")
    string(REGEX REPLACE "cpu_main_sha256=[0-9a-f]+" "cpu_main_sha256=${zero_sha}" changed_record "${build_record}")
    if(changed_oracle STREQUAL oracle_text OR changed_record STREQUAL build_record)
      message(FATAL_ERROR "suite_driver: self-consistent CPU source identity control did not mutate both source records")
    endif()
    file(WRITE "${case_dir}/oracle.tsv" "${changed_oracle}")
    file(WRITE "${case_dir}/build-record.txt" "${changed_record}")
    file(SHA256 "${case_dir}/oracle.tsv" oracle_sha)
    file(READ "${case_dir}/run-record-zero.txt" run_record)
    string(REGEX REPLACE "oracle_sha256=[0-9a-f]+" "oracle_sha256=${oracle_sha}" changed_run_record "${run_record}")
    file(WRITE "${case_dir}/run-record-zero.txt" "${changed_run_record}")
  elseif(mutation STREQUAL "object")
    file(APPEND "${case_dir}/cpu.bll" "changed")
  elseif(mutation STREQUAL "labels")
    file(APPEND "${case_dir}/cpu.lbl" "changed\n")
  else()
    message(FATAL_ERROR "suite_driver: unknown CPU build identity mutation ${mutation}")
  endif()
  expect_failure("${expected}"
    "${TEST_BINARY}" validate-cpu-record lynx1
      "${case_dir}/raw-results-zero.tsv" "${case_dir}/run-record-zero.txt"
      "${case_dir}/oracle.tsv" "${case_dir}")
  cpu_batch_positive()
endfunction()

function(replace_cpu_tuple source_text from_tuple to_tuple output_var)
  string(REPLACE "," ";" from_bytes "${from_tuple}")
  string(REPLACE "," ";" to_bytes "${to_tuple}")
  set(changed "${source_text}")
  foreach(offset RANGE 0 3)
    list(GET from_bytes ${offset} from_byte)
    list(GET to_bytes ${offset} to_byte)
    math(EXPR result_index "12 + ${offset}")
    string(REPLACE "result\t${result_index}\t${from_byte}"
                   "result\t${result_index}\t${to_byte}" changed "${changed}")
  endforeach()
  set(${output_var} "${changed}" PARENT_SCOPE)
endfunction()

function(write_ticks_record destination ticks)
  file(READ "${WORK}/cpu-baseline/lynx1/run-record-zero.txt" record_text)
  string(REGEX MATCH "ticks_observed=([0-9]+)" old_tick_row "${record_text}")
  set(old_ticks "${CMAKE_MATCH_1}")
  string(REPLACE "ticks_observed=${old_ticks}" "ticks_observed=${ticks}" changed "${record_text}")
  string(REPLACE "guest_ticks=${old_ticks}" "guest_ticks=${ticks}" changed "${changed}")
  if(ticks GREATER_EQUAL 256 AND ticks LESS_EQUAL 50000000)
    math(EXPR margin "${ticks} - 256")
    math(EXPR headroom "50000000 - ${ticks}")
    string(REGEX REPLACE "completion_margin_ticks=[0-9]+" "completion_margin_ticks=${margin}" changed "${changed}")
    string(REGEX REPLACE "maximum_headroom_ticks=[0-9]+" "maximum_headroom_ticks=${headroom}" changed "${changed}")
  endif()
  file(WRITE "${destination}" "${changed}")
endfunction()

function(reset_shape_source)
  file(MAKE_DIRECTORY "${WORK}/source/cpu" "${WORK}/source/timers")
  configure_file("${ROOT}/build/pins/lynx-tests/cpu/main.c" "${WORK}/source/cpu/main.c" COPYONLY)
  configure_file("${ROOT}/build/pins/lynx-tests/cpu/tests.s" "${WORK}/source/cpu/tests.s" COPYONLY)
  configure_file("${ROOT}/build/pins/lynx-tests/timers/main.c" "${WORK}/source/timers/main.c" COPYONLY)
  configure_file("${ROOT}/build/pins/lynx-tests/timers/tests.s" "${WORK}/source/timers/tests.s" COPYONLY)
endfunction()

function(expect_shape_failure expected)
  expect_failure("suite oracle: source shape: ${expected}"
    sh "${ROOT}/scripts/generate-suite-oracle.sh"
      --source "${WORK}/source" --test-source-shape-only)
endfunction()

if(MODE MATCHES "^page_mode")
  execute_process(
    COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin" "TMPDIR=${WORK}/tmp"
            sh "${ROOT}/scripts/build-page-mode.sh" --out "${SUITE_BUILD}"
    TIMEOUT 30 RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
  if(NOT build_rc STREQUAL "0" OR
     NOT build_out MATCHES "lynx_tests_pin=a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e" OR
     NOT build_out MATCHES "result_count=6")
    set(FAILURE "pinned page-mode source build failed: ${build_rc} ${build_out} ${build_err}")
  endif()
  if(FAILURE STREQUAL "")
    execute_process(
      COMMAND sh "${ROOT}/scripts/generate-page-mode-oracle.sh"
              --source "${ROOT}/build/pins/lynx-tests" --out "${ORACLE}"
      TIMEOUT 15 RESULT_VARIABLE oracle_rc OUTPUT_VARIABLE oracle_out ERROR_VARIABLE oracle_err)
    if(NOT oracle_rc STREQUAL "0")
      set(FAILURE "page-mode source oracle generation failed: ${oracle_rc} ${oracle_err}")
    endif()
  endif()
  if(FAILURE STREQUAL "")
    execute_process(COMMAND "${TEST_BINARY}" validate-page-mode-build
      "${WORK}" "${SUITE_BUILD}" "${ORACLE}"
      TIMEOUT 10 RESULT_VARIABLE validate_rc OUTPUT_VARIABLE validate_out ERROR_VARIABLE validate_err)
    if(NOT validate_rc STREQUAL "0" OR
       NOT validate_out MATCHES "page-mode build: pinned source, tool, notice, wrapper, object, labels and six-row oracle identities valid")
      set(FAILURE "page-mode build/oracle identity validation failed: ${validate_rc} ${validate_out} ${validate_err}")
    endif()
  endif()
else()
  execute_process(
    COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
            "TMPDIR=${WORK}/tmp"
            sh "${ROOT}/scripts/build-suites.sh" --out "${SUITE_BUILD}"
    TIMEOUT 30 RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
  if(NOT build_rc STREQUAL "0" OR
     NOT build_out MATCHES "lynx_tests_pin=a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e")
    set(FAILURE "pinned suite source build failed: ${build_rc} ${build_out} ${build_err}")
  endif()

  if(FAILURE STREQUAL "")
    execute_process(
      COMMAND sh "${ROOT}/scripts/generate-suite-oracle.sh"
              --source "${ROOT}/build/pins/lynx-tests" --out "${ORACLE}"
      TIMEOUT 15 RESULT_VARIABLE oracle_rc OUTPUT_VARIABLE oracle_out ERROR_VARIABLE oracle_err)
    if(NOT oracle_rc STREQUAL "0")
      set(FAILURE "source oracle generation failed: ${oracle_rc} ${oracle_err}")
    endif()
  endif()

  if(FAILURE STREQUAL "")
    execute_process(COMMAND "${TEST_BINARY}" validate-oracle "${ORACLE}"
      TIMEOUT 10 RESULT_VARIABLE validate_rc OUTPUT_VARIABLE validate_out ERROR_VARIABLE validate_err)
    if(NOT validate_rc STREQUAL "0" OR NOT validate_out MATCHES "cpu=18/18 timers=16/18")
      set(FAILURE "generated oracle validation failed: ${validate_rc} ${validate_out} ${validate_err}")
    endif()
  endif()

  if(FAILURE STREQUAL "" AND MODE MATCHES "^audio")
    execute_process(
      COMMAND sh "${ROOT}/scripts/generate-suite-oracle.sh"
              --source "${ROOT}/build/pins/lynx-tests" --include-audio --out "${AUDIO_ORACLE}"
      TIMEOUT 15 RESULT_VARIABLE audio_oracle_rc OUTPUT_VARIABLE audio_oracle_out ERROR_VARIABLE audio_oracle_err)
    if(NOT audio_oracle_rc STREQUAL "0")
      set(FAILURE "extended audio source oracle generation failed: ${audio_oracle_rc} ${audio_oracle_out} ${audio_oracle_err}")
    endif()
  endif()

  if(FAILURE STREQUAL "" AND MODE MATCHES "^audio")
    execute_process(COMMAND "${TEST_BINARY}" validate-audio-oracle
      "${ORACLE}" "${AUDIO_ORACLE}" "${SUITE_BUILD}"
      TIMEOUT 10 RESULT_VARIABLE audio_validate_rc OUTPUT_VARIABLE audio_validate_out ERROR_VARIABLE audio_validate_err)
    if(NOT audio_validate_rc STREQUAL "0" OR
       NOT audio_validate_out MATCHES "audio oracle: exact audio/audio2 source rows, case coverage, and build identities valid")
      set(FAILURE "extended audio oracle/build identity validation failed: ${audio_validate_rc} ${audio_validate_out} ${audio_validate_err}")
    endif()
  endif()
endif()

if(FAILURE STREQUAL "" AND MODE STREQUAL "page_mode_observation")
  set(all_run_out "")
  set(record "${WORK}/page-mode-run-${PROFILE}.txt")
  expect_success("LYNX-SKIP: page-mode source requires unavailable SERCTL write at $FD8C"
    "${TEST_BINARY}" observe page-mode "${PROFILE}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
  expect_success("page-mode record: exact SERCTL blocker and missing six-byte observation valid for ${PROFILE}"
    "${TEST_BINARY}" validate-page-mode-record "${PROFILE}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
  string(APPEND all_run_out "LYNX-SKIP: page-mode source requires unavailable SERCTL write at $FD8C\n")
elseif(FAILURE STREQUAL "" AND MODE STREQUAL "audio_observation")
  set(all_run_out "")
  expect_success("audio observation complete: suite=${SUITE} profile=${PROFILE}"
    "${TEST_BINARY}" observe audio "${SUITE}" "${PROFILE}" "${SUITE_BUILD}" "${ORACLE}" "${AUDIO_ORACLE}")
  set(audio_raw "${SUITE_BUILD}/raw-results-${SUITE}-${PROFILE}.tsv")
  set(audio_comparison "${SUITE_BUILD}/comparison-${SUITE}-${PROFILE}.tsv")
  set(audio_record "${SUITE_BUILD}/run-record-${SUITE}-${PROFILE}.txt")
  if(NOT EXISTS "${audio_raw}" OR NOT EXISTS "${audio_comparison}" OR NOT EXISTS "${audio_record}")
    message(FATAL_ERROR "suite_driver: fresh audio observation omitted a required record for ${SUITE}/${PROFILE}")
  endif()
  file(STRINGS "${audio_raw}" audio_rows REGEX "^result\t[0-9]+\t[0-9a-f][0-9a-f]$")
  file(STRINGS "${audio_comparison}" audio_comparison_rows REGEX "^result\t[0-9]+\t")
  list(LENGTH audio_rows audio_count)
  list(LENGTH audio_comparison_rows audio_comparison_count)
  set(audio_accepted 0)
  foreach(row IN LISTS audio_comparison_rows)
    if(row MATCHES "source-accepted$")
      math(EXPR audio_accepted "${audio_accepted} + 1")
    else()
      message(FATAL_ERROR "suite_driver: source mismatch in fresh audio comparison: ${row}")
    endif()
  endforeach()
  file(READ "${audio_record}" audio_record_text)
  if(NOT audio_count EQUAL 18 OR NOT audio_comparison_count EQUAL 18 OR NOT audio_accepted EQUAL 18 OR
     NOT audio_record_text MATCHES "meaningful_count=18" OR
     NOT audio_record_text MATCHES "source_agreements=18" OR
     NOT audio_record_text MATCHES "completion_sentinel=a5" OR
     NOT audio_record_text MATCHES "hardware_accuracy=unverified" OR
     NOT audio_record_text MATCHES "profile_qualification=unresolved")
    message(FATAL_ERROR "suite_driver: fresh audio observation evidence shape or policy fields drifted")
  endif()
  string(APPEND all_run_out "${SUITE} observation: fresh ${PROFILE} guest returned; 18 raw rows and 18 source-agreement rows retained\n")
elseif(FAILURE STREQUAL "" AND (MODE STREQUAL "audio_controls" OR MODE STREQUAL "audio2_controls"))
  set(all_run_out "")
  audio_batch_begin("${SUITE}")
  audio_missing_return_case("${SUITE}")
  audio_wrong_selection_case("${SUITE}")
  audio_record_mutation("record-detected-model-lynx2" "${SUITE}" lynx1 detected_model lynx2
    "suite: audio record: detected model does not match guest evidence")
  audio_record_mutation("record-missing-return" "${SUITE}" lynx1 guest_return __REMOVE__
    "suite: audio record missing field: guest_return")
  audio_record_mutation("record-progress-seven" "${SUITE}" lynx1 guest_progress 7
    "suite: audio record: guest progress evidence mismatch")
  audio_record_mutation("record-sentinel-zero" "${SUITE}" lynx1 completion_sentinel 00
    "suite: audio record: completion sentinel mismatch")
  audio_rows_case("rows-zero" "${SUITE}" lynx1 zero "suite: audio raw record result row count/order mismatch")
  audio_rows_case("rows-short" "${SUITE}" lynx1 short "suite: audio raw record result row count/order mismatch")
  audio_rows_case("rows-extra" "${SUITE}" lynx1 extra "suite: audio raw record contains extra or duplicate result rows")
  audio_rows_case("rows-duplicate" "${SUITE}" lynx1 duplicate "suite: audio raw record result row count/order mismatch")
  audio_rows_case("rows-zero-values" "${SUITE}" lynx1 zero-values "suite: audio comparison: source oracle mismatch")
  audio_symbol_case("symbol-result-extent" "${SUITE}" suite_result_end "- 1"
    "suite: symbol: result extent or code overlap")
  audio_symbol_case("symbol-entry-alias" "${SUITE}" suite_test_entry "+ 1"
    "suite: audio symbol extent, aliases or wrapper return gate invalid")
  audio_tick_case("ticks-minimum-256" "${SUITE}" 256 "" TRUE)
  audio_tick_case("ticks-maximum-50000000" "${SUITE}" 50000000 "" TRUE)
  audio_tick_case("ticks-below-minimum-255" "${SUITE}" 255
    "suite: audio record: ticks outside completion bounds" FALSE)
  audio_tick_case("ticks-above-maximum-50000001" "${SUITE}" 50000001
    "suite: audio record: ticks outside completion bounds" FALSE)
  audio_tick_case("ticks-overflow-4294967296" "${SUITE}" 4294967296
    "suite: audio record: invalid ticks_observed" FALSE)
  audio_record_mutation("record-content-stale" "${SUITE}" lynx1 bll_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: content identity mismatch")
  audio_record_mutation("record-source-stale" "${SUITE}" lynx1 source_main_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: source identity mismatch")
  audio_record_mutation("record-tool-stale" "${SUITE}" lynx1 ca65_tool_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: tool identity mismatch")
  audio_record_mutation("record-build-stale" "${SUITE}" lynx1 object_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: object identity mismatch")
  audio_record_mutation("record-oracle-stale" "${SUITE}" lynx1 oracle_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: oracle identity mismatch")
  audio_record_mutation("record-layout-stale" "${SUITE}" lynx1 layout_sha256
    0000000000000000000000000000000000000000000000000000000000000000
    "suite: audio record: layout identity mismatch")
  if(SUITE STREQUAL "audio2")
    audio_alternative_case("lost-audio2-alternative" lost audio2)
    audio_alternative_case("changed-audio2-observation" changed audio2)
  endif()
  get_property(audio_case_names GLOBAL PROPERTY AUDIO_CONTROL_CASE_NAMES)
  list(LENGTH audio_case_names audio_case_count)
  if(SUITE STREQUAL "audio")
    set(expected_audio_case_count 24)
  else()
    set(expected_audio_case_count 26)
  endif()
  if(NOT audio_case_count EQUAL expected_audio_case_count)
    message(FATAL_ERROR "suite_driver: audio control corpus case count drifted: ${audio_case_count}")
  endif()
  string(APPEND all_run_out "${SUITE} controls: ${audio_case_count} named controls passed against immutable fresh Lynx1/Lynx2 baselines; held/resumed guests, semantic refusals, exact rows, tick bounds, identities, symbol extents and source alternatives were checked\n")
elseif(FAILURE STREQUAL "" AND MODE STREQUAL "audio_source_controls")
  set(all_run_out "")
  set(required_registrations
    "suite.cpu.lynx1|lynx_suite_test(suite.cpu.lynx1 cpu 60 lynx1)"
    "suite.cpu.lynx2|lynx_suite_test(suite.cpu.lynx2 cpu 60 lynx2)"
    "suite.timers.observation.lynx1|lynx_suite_test(suite.timers.observation.lynx1 timer_observation 60 lynx1)"
    "suite.timers.observation.lynx2|lynx_suite_test(suite.timers.observation.lynx2 timer_observation 60 lynx2)"
    "suite.page_mode.observation.lynx1|lynx_suite_test(suite.page_mode.observation.lynx1 page_mode_observation 300 lynx1)"
    "suite.page_mode.observation.lynx2|lynx_suite_test(suite.page_mode.observation.lynx2 page_mode_observation 300 lynx2)"
    "suite.audio.observation.lynx1|lynx_suite_test(suite.audio.observation.lynx1 audio_observation 180 lynx1 audio)"
    "suite.audio.observation.lynx2|lynx_suite_test(suite.audio.observation.lynx2 audio_observation 180 lynx2 audio)"
    "suite.audio2.observation.lynx1|lynx_suite_test(suite.audio2.observation.lynx1 audio_observation 180 lynx1 audio2)"
    "suite.audio2.observation.lynx2|lynx_suite_test(suite.audio2.observation.lynx2 audio_observation 180 lynx2 audio2)"
    "suite.audio.controls|lynx_suite_test(suite.audio.controls audio_controls 240 \"\" audio)"
    "suite.audio.source_controls|lynx_suite_test(suite.audio.source_controls audio_source_controls 240)"
    "suite.audio2.controls|lynx_suite_test(suite.audio2.controls audio2_controls 240 \"\" audio2)")
  file(STRINGS "${ROOT}/tests/inventory/fast.txt" inventory_rows)
  foreach(registration IN LISTS required_registrations)
    string(REPLACE "|" ";" registration_parts "${registration}")
    list(GET registration_parts 0 case_id)
    list(GET registration_parts 1 expected_registration)
    string(REPLACE "." "\\." escaped_id "${case_id}")
    file(STRINGS "${ROOT}/tests/suites/tests.cmake" registration_matches REGEX "^lynx_suite_test\\(${escaped_id} ")
    list(LENGTH registration_matches registration_count)
    if(NOT registration_count EQUAL 1 OR NOT "${registration_matches}" STREQUAL "${expected_registration}")
      message(FATAL_ERROR "suite_driver: required mode/profile registration drifted for ${case_id}: ${registration_matches}")
    endif()
    set(inventory_count 0)
    foreach(inventory_row IN LISTS inventory_rows)
      if(inventory_row MATCHES "^${escaped_id}\t")
        math(EXPR inventory_count "${inventory_count} + 1")
      endif()
    endforeach()
    if(NOT inventory_count EQUAL 1)
      message(FATAL_ERROR "suite_driver: expected exactly one fast inventory row for ${case_id}, found ${inventory_count}")
    endif()
  endforeach()
  file(STRINGS "${ROOT}/tests/inventory/skips.txt" audio_skip_rows REGEX "^suite\\.page_mode\\.observation\\.")
  list(LENGTH audio_skip_rows audio_skip_count)
  string(ASCII 9 skip_tab)
  set(skip_reason "page-mode source reaches unsupported SERCTL write $FD8C before tests")
  set(skip_one "suite.page_mode.observation.lynx1${skip_tab}${skip_reason}")
  set(skip_two "suite.page_mode.observation.lynx2${skip_tab}${skip_reason}")
  list(FIND audio_skip_rows "${skip_one}" skip_one_index)
  list(FIND audio_skip_rows "${skip_two}" skip_two_index)
  if(NOT audio_skip_count EQUAL 2 OR skip_one_index EQUAL -1 OR skip_two_index EQUAL -1)
    message(FATAL_ERROR "suite_driver: inherited page-mode skip inventory changed")
  endif()
  expect_success_timeout("audio script controls: source identity, object and label controls passed" 180
    sh "${ROOT}/tests/gates/audio-script-controls.sh")
    string(APPEND all_run_out "audio source controls: 13 required observation/control CTests have exact intended modes/profiles and occur once in the fast inventory; only the two inherited SERCTL skips remain; direct isolated builder/oracle source controls passed\n")
elseif(FAILURE STREQUAL "" AND MODE STREQUAL "page_mode_controls")
  set(all_run_out "")
  foreach(profile lynx1 lynx2)
    set(record "${WORK}/page-mode-run-${profile}.txt")
    expect_success("page-mode control: exact SERCTL write blocker at $FD8C"
      "${TEST_BINARY}" observe page-mode "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}" control)
    expect_success("page-mode record: exact SERCTL blocker and missing six-byte observation valid for ${profile}"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(READ "${record}" record_baseline)

    if(profile STREQUAL "lynx1")
      set(wrong_profile lynx2)
    else()
      set(wrong_profile lynx1)
    endif()
    string(REPLACE "detected_model=${profile}\n" "detected_model=${wrong_profile}\n" mutated "${record_baseline}")
    file(WRITE "${record}" "${mutated}")
    expect_failure("page-mode record detected model mismatch"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(WRITE "${record}" "${record_baseline}")

    string(REPLACE "meaningful_count=0\n" "meaningful_count=1\n" mutated "${record_baseline}")
    file(WRITE "${record}" "${mutated}")
    expect_failure("page-mode record must preserve missing results"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(WRITE "${record}" "${record_baseline}")

    string(REGEX REPLACE "ticks_observed=[0-9]+" "ticks_observed=39999" mutated "${record_baseline}")
    file(WRITE "${record}" "${mutated}")
    expect_failure("page-mode record ticks outside bound or do not match guest time"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(WRITE "${record}" "${record_baseline}")

    string(REPLACE "request_address=fd8c\n" "request_address=fe00\n" mutated "${record_baseline}")
    file(WRITE "${record}" "${mutated}")
    expect_failure("page-mode record unsupported request address mismatch"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(WRITE "${record}" "${record_baseline}")

    file(APPEND "${record}" "unexpected=value\n")
    expect_failure("page-mode record unknown key"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    file(WRITE "${record}" "${record_baseline}")

    expect_success("page-mode control: exact SERCTL write blocker at $FD8C"
      "${TEST_BINARY}" no-run page-mode "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
    if(NOT EXISTS "${record}")
      message(FATAL_ERROR "suite_driver: page-mode no-run control did not retain its blocker record")
    endif()
    file(READ "${record}" no_run_record)
    if(NOT no_run_record MATCHES "no_run_hold_verified=true")
      message(FATAL_ERROR "suite_driver: page-mode no-run record omitted the verified hold")
    endif()
    expect_success("page-mode record: exact SERCTL blocker and missing six-byte observation valid for ${profile}"
      "${TEST_BINARY}" validate-page-mode-record "${profile}" "${WORK}" "${SUITE_BUILD}" "${ORACLE}" "${record}")
  endforeach()

  set(tab "\t")
  file(READ "${ORACLE}" page_oracle)
  string(REPLACE "result_count${tab}6\n" "result_count${tab}5\n" bad_count "${page_oracle}")
  file(WRITE "${WORK}/page-oracle-count.tsv" "${bad_count}")
  expect_failure("page-mode oracle: result count mismatch"
    "${TEST_BINARY}" validate-page-mode-oracle "${WORK}/page-oracle-count.tsv")
  string(REPLACE "result${tab}4${tab}2${tab}74${tab}75\n" "result${tab}4${tab}2${tab}73${tab}75\n" bad_alternative "${page_oracle}")
  file(WRITE "${WORK}/page-oracle-alternative.tsv" "${bad_alternative}")
  expect_failure("page-mode oracle: source expectation mismatch"
    "${TEST_BINARY}" validate-page-mode-oracle "${WORK}/page-oracle-alternative.tsv")

  file(MAKE_DIRECTORY "${WORK}/source/page-mode")
  configure_file("${ROOT}/build/pins/lynx-tests/page-mode/main.c" "${WORK}/source/page-mode/main.c" COPYONLY)
  configure_file("${ROOT}/build/pins/lynx-tests/page-mode/tests.s" "${WORK}/source/page-mode/tests.s" COPYONLY)
  configure_file("${ROOT}/build/pins/lynx-tests/LICENSE" "${WORK}/source/LICENSE" COPYONLY)
  file(READ "${WORK}/source/page-mode/main.c" source_main)
  string(REPLACE "EXPECT(0x74, 0x75)" "EXPECT(0x73, 0x75)" source_mutant "${source_main}")
  file(WRITE "${WORK}/source/page-mode/main.c" "${source_mutant}")
  expect_failure("page-mode oracle: source shape: source alternative at result 4 must remain EXPECT(0x74,0x75)"
    sh "${ROOT}/scripts/generate-page-mode-oracle.sh" --source "${WORK}/source" --test-source-shape-only)

  expect_failure("page-mode build: output is outside invocation-owned scratch"
    sh "${ROOT}/scripts/build-page-mode.sh" --out "${ROOT}/build/check/page-mode-outside-${SUITE_RUN_NONCE}")
  expect_success_timeout("page-mode script controls: isolated source, tool, marker, notice, shape, and output hostility controls refused; positive brackets remained valid" 120
    sh "${ROOT}/tests/gates/page-mode-script-controls.sh" "${ROOT}" "${WORK}/page-mode-script-hostility")
  expect_success_timeout("inventory_controls: junit: all cases ok" 30
    ${CMAKE_COMMAND} -DMODE=junit -DSCRIPTS=${ROOT}/tests/cmake
    -DWORK=${WORK}/junit-controls -P "${ROOT}/tests/cmake/inventory_controls.cmake")
  string(ASCII 9 page_tab)
  set(page_inventory "${WORK}/page-mode-skips.inventory.txt")
  set(page_skips "${WORK}/page-mode-skips.txt")
  set(page_junit "${WORK}/page-mode-skips.junit.xml")
  file(WRITE "${page_inventory}" "# name<TAB>labels\nsuite.page_mode.observation.lynx1${page_tab}suite\nsuite.page_mode.observation.lynx2${page_tab}suite\n")
  file(WRITE "${page_skips}" "# name<TAB>reason\nsuite.page_mode.observation.lynx1${page_tab}page-mode source reaches unsupported SERCTL write $FD8C before tests\nsuite.page_mode.observation.lynx2${page_tab}page-mode source reaches unsupported SERCTL write $FD8C before tests\n")
  set(page_junit_skipped "<testsuite>\n  <testcase name=\"suite.page_mode.observation.lynx1\" status=\"notrun\"><skipped message=\"SERCTL unsupported\" /></testcase>\n  <testcase name=\"suite.page_mode.observation.lynx2\" status=\"notrun\"><skipped message=\"SERCTL unsupported\" /></testcase>\n</testsuite>\n")
  file(WRITE "${page_junit}" "${page_junit_skipped}")
  expect_success("check_junit: 2 tests accounted for once each (2 declared skips)"
    "${CMAKE_COMMAND}" "-DJUNIT=${page_junit}" "-DINVENTORY=${page_inventory}" "-DSKIPS=${page_skips}"
    -P "${ROOT}/tests/cmake/check_junit.cmake")
  file(WRITE "${page_skips}" "# name<TAB>reason\nsuite.page_mode.observation.lynx1${page_tab}page-mode source reaches unsupported SERCTL write $FD8C before tests\n")
  expect_failure("suite.page_mode.observation.lynx2: skipped but not declared in skips.txt"
    "${CMAKE_COMMAND}" "-DJUNIT=${page_junit}" "-DINVENTORY=${page_inventory}" "-DSKIPS=${page_skips}"
    -P "${ROOT}/tests/cmake/check_junit.cmake")
  file(WRITE "${page_skips}" "# name<TAB>reason\nsuite.page_mode.observation.lynx1${page_tab}page-mode source reaches unsupported SERCTL write $FD8C before tests\nsuite.page_mode.observation.lynx2${page_tab}page-mode source reaches unsupported SERCTL write $FD8C before tests\n")
  set(page_junit_stale "<testsuite>\n  <testcase name=\"suite.page_mode.observation.lynx1\" status=\"notrun\"><skipped message=\"SERCTL unsupported\" /></testcase>\n  <testcase name=\"suite.page_mode.observation.lynx2\" status=\"run\"></testcase>\n</testsuite>\n")
  file(WRITE "${page_junit}" "${page_junit_stale}")
  expect_failure("suite.page_mode.observation.lynx2: declared as skipped but ran"
    "${CMAKE_COMMAND}" "-DJUNIT=${page_junit}" "-DINVENTORY=${page_inventory}" "-DSKIPS=${page_skips}"
    -P "${ROOT}/tests/cmake/check_junit.cmake")
  string(APPEND all_run_out "page-mode controls: both profiles prove the exact unsupported request; no-run holds then resumes; isolated script source/tool/marker/notice/shape/path hostility, altered model/count/time/address/unknown-key/source/oracle, and actual-name declared, undeclared and stale-skip controls refuse\n")
endif()

if(FAILURE STREQUAL "")
  if(MODE STREQUAL "cpu_no_run")
    set(all_run_out "")
    foreach(profile lynx1 lynx2)
      run_cpu_no_run_control("${profile}")
    endforeach()
  elseif(MODE STREQUAL "timers_no_run")
    set(all_run_out "")
    foreach(profile lynx1 lynx2)
      run_timer_no_run_control("${profile}")
    endforeach()
  endif()
endif()

if(FAILURE STREQUAL "")
  if(MODE STREQUAL "completion_controls")
    set(all_run_out "")
    set(required_selection "suite.cpu.lynx1,suite.cpu.lynx2,suite.timers.observation.lynx1,suite.timers.observation.lynx2,suite.page_mode.observation.lynx1,suite.page_mode.observation.lynx2,suite.audio.observation.lynx1,suite.audio.observation.lynx2,suite.audio2.observation.lynx1,suite.audio2.observation.lynx2")
    set(control_selection "suite.cpu.no_run,suite.timers.no_run,suite.completion_controls,suite.selection_controls,suite.page_mode.controls,suite.audio.controls,suite.audio.source_controls,suite.audio2.controls")
    expect_success("suite selection: required=10 controls=8 inventory complete"
      "${TEST_BINARY}" validate-selection "${required_selection}" "${control_selection}")
    build_fault_wrapper(cpu)
    build_fault_wrapper(timers)
    set(COMPLETION_MANIFEST "${WORK}/completion-cases.tsv")
    file(WRITE "${COMPLETION_MANIFEST}" "")
    foreach(suite cpu timers)
      foreach(profile lynx1 lynx2)
        run_completion_fault("${suite}" "${profile}" omit_sentinel)
        run_completion_fault("${suite}" "${profile}" guest_time_bound)
      endforeach()
    endforeach()
    set(expected_manifest "")
    foreach(suite cpu timers)
      foreach(profile lynx1 lynx2)
        string(APPEND expected_manifest "${suite}.${profile}.omit_sentinel\t${suite}\t${profile}\tactual_guest\tpost_return_sentinel_missing\tprior_positive_outputs_cleared\n")
        string(APPEND expected_manifest "${suite}.${profile}.guest_time_bound\t${suite}\t${profile}\tactual_guest\tguest_time_limit_before_suite_return\tprior_positive_outputs_cleared\n")
      endforeach()
    endforeach()
    file(READ "${COMPLETION_MANIFEST}" actual_manifest)
    file(STRINGS "${COMPLETION_MANIFEST}" manifest_rows)
    list(LENGTH manifest_rows manifest_count)
    if(NOT manifest_count EQUAL 8 OR NOT actual_manifest STREQUAL expected_manifest)
      message(FATAL_ERROR "suite_driver: completion fault manifest is empty, incomplete, duplicated, or out of inventory order")
    endif()
    string(APPEND all_run_out "completion controls: exact eight-case manifest covers omitted-sentinel and bounded guest-time refusal for both suites and profiles; every fault starts with hash-verified prior positive files, proves fresh guest/watch refusal clears them, and is bracketed by fresh valid positives\n")
  elseif(MODE STREQUAL "selection_controls")
    set(all_run_out "")
    set(required_selection "suite.cpu.lynx1,suite.cpu.lynx2,suite.timers.observation.lynx1,suite.timers.observation.lynx2,suite.page_mode.observation.lynx1,suite.page_mode.observation.lynx2,suite.audio.observation.lynx1,suite.audio.observation.lynx2,suite.audio2.observation.lynx1,suite.audio2.observation.lynx2")
    set(control_selection "suite.cpu.no_run,suite.timers.no_run,suite.completion_controls,suite.selection_controls,suite.page_mode.controls,suite.audio.controls,suite.audio.source_controls,suite.audio2.controls")
    expect_success("suite selection: required=10 controls=8 inventory complete"
      "${TEST_BINARY}" validate-selection "${required_selection}" "${control_selection}")
    expect_empty_selection(required-suite "" "${control_selection}")
    expect_empty_selection(control "${required_selection}" "")
    expect_failure("suite: selection: unknown required-suite case ID"
      "${TEST_BINARY}" validate-selection "suite.cpu.lynx1,suite.cpu.lynx2,suite.timers.observation.lynx1,suite.timers.observation.lynx2,suite.page_mode.observation.lynx1,suite.page_mode.observation.lynx2,suite.audio.observation.lynx1,suite.audio.observation.lynx2,suite.audio2.observation.lynx1,suite.audio2.observation.lynx9" "${control_selection}")
    expect_failure("suite: selection: duplicate required-suite case ID"
      "${TEST_BINARY}" validate-selection "suite.cpu.lynx1,suite.cpu.lynx1,suite.cpu.lynx2,suite.timers.observation.lynx1,suite.timers.observation.lynx2,suite.page_mode.observation.lynx1,suite.page_mode.observation.lynx2,suite.audio.observation.lynx1,suite.audio.observation.lynx2,suite.audio2.observation.lynx1,suite.audio2.observation.lynx2" "${control_selection}")
    expect_failure("suite: selection: incomplete required-suite inventory"
      "${TEST_BINARY}" validate-selection "suite.cpu.lynx1,suite.cpu.lynx2,suite.timers.observation.lynx1,suite.timers.observation.lynx2,suite.page_mode.observation.lynx1,suite.page_mode.observation.lynx2,suite.audio.observation.lynx1,suite.audio.observation.lynx2,suite.audio2.observation.lynx1" "${control_selection}")
    expect_failure("suite: selection: duplicate control case ID"
      "${TEST_BINARY}" validate-selection "${required_selection}" "suite.cpu.no_run,suite.cpu.no_run,suite.timers.no_run,suite.completion_controls,suite.selection_controls,suite.page_mode.controls,suite.audio.controls,suite.audio.source_controls,suite.audio2.controls")
    expect_failure("suite: selection: incomplete control inventory"
      "${TEST_BINARY}" validate-selection "${required_selection}" "suite.cpu.no_run,suite.timers.no_run,suite.completion_controls,suite.selection_controls,suite.page_mode.controls,suite.audio.controls,suite.audio.source_controls")

    file(READ "${ROOT}/tests/suites/tests.cmake" registration_text)
    string(ASCII 9 suite_tab)
    file(STRINGS "${ROOT}/tests/inventory/fast.txt" suite_inventory_rows)
    file(STRINGS "${ROOT}/tests/suites/tests.cmake" registration_rows REGEX "^lynx_suite_test\\(")
    list(LENGTH registration_rows registration_count)
    if(registration_count LESS 1)
      message(FATAL_ERROR "suite_driver: suite CTest registration inventory is empty")
    endif()
    foreach(case_id suite.cpu.lynx1 suite.cpu.lynx2 suite.cpu.no_run suite.completion_controls
                    suite.selection_controls suite.timers.observation.lynx1 suite.timers.observation.lynx2
                    suite.timers.no_run suite.page_mode.observation.lynx1 suite.page_mode.observation.lynx2
                    suite.page_mode.controls suite.audio.observation.lynx1 suite.audio.observation.lynx2
                    suite.audio.controls suite.audio.source_controls suite.audio2.observation.lynx1
                    suite.audio2.observation.lynx2 suite.audio2.controls)
      string(FIND "${registration_text}" "lynx_suite_test(${case_id} " registration_found)
      if(registration_found EQUAL -1)
        message(FATAL_ERROR "suite_driver: required suite/control CTest ID is not registered: ${case_id}")
      endif()
      set(inventory_count 0)
      foreach(inventory_row IN LISTS suite_inventory_rows)
        if(inventory_row STREQUAL "${case_id}${suite_tab}suite")
          math(EXPR inventory_count "${inventory_count} + 1")
        endif()
      endforeach()
      if(NOT inventory_count EQUAL 1)
        message(FATAL_ERROR "suite_driver: required suite/control ID does not occur exactly once in fast inventory: ${case_id}")
      endif()
    endforeach()
    foreach(row IN LISTS registration_rows)
      if(NOT row MATCHES "^lynx_suite_test\\(([^ ]+)[ ]")
        message(FATAL_ERROR "suite_driver: malformed suite registration row: ${row}")
      endif()
      set(case_id "${CMAKE_MATCH_1}")
      set(inventory_count 0)
      foreach(inventory_row IN LISTS suite_inventory_rows)
        if(inventory_row STREQUAL "${case_id}${suite_tab}suite")
          math(EXPR inventory_count "${inventory_count} + 1")
        endif()
      endforeach()
      if(NOT inventory_count EQUAL 1)
        message(FATAL_ERROR "suite_driver: registered suite ID lacks one exact fast inventory row: ${case_id}")
      endif()
    endforeach()
    string(APPEND all_run_out "selection controls: exact nonempty required/control lists, empty/wrong/duplicate/incomplete refusals and CTest-to-fast-inventory bindings verified\n")
  endif()
endif()

if(FAILURE STREQUAL "")
  if(MODE STREQUAL "cpu")
    set(all_run_out "")
    foreach(ram_case zero seeded)
      execute_process(
        COMMAND "${TEST_BINARY}" run cpu "${PROFILE}" "${SUITE_BUILD}" "${ORACLE}" "${ram_case}"
        TIMEOUT 45 RESULT_VARIABLE run_rc OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
      if(NOT run_rc STREQUAL "0" OR
         NOT run_out MATCHES "suite raw source comparison: match; suite=cpu machine_profile=${PROFILE} profile_input=${PROFILE} ram_seed=${ram_case} ticks=[0-9]+ results=18 detected_model=${PROFILE} source_tuple_model=${PROFILE}" OR
         NOT run_out MATCHES "hardware_accuracy=unverified profile_qualification=unresolved")
        set(FAILURE "bounded CPU guest did not produce ${PROFILE}/${ram_case} source comparison evidence: ${run_rc} ${run_out} ${run_err}")
        break()
      endif()
      string(APPEND all_run_out "${run_out}\n")
      file(READ "${SUITE_BUILD}/run-record-${ram_case}.txt" run_record)
      file(READ "${SUITE_BUILD}/raw-results-${ram_case}.tsv" raw_results)
      file(READ "${SUITE_BUILD}/comparison-${ram_case}.tsv" comparison)
      if(NOT run_record MATCHES "labels_sha256=[0-9a-f][0-9a-f]+" OR
         NOT run_record MATCHES "oracle_sha256=[0-9a-f][0-9a-f]+" OR
         NOT run_record MATCHES "detected_model=${PROFILE}" OR
         NOT run_record MATCHES "source_tuple_model=${PROFILE}" OR
         NOT run_record MATCHES "meaningful_count=18" OR
         NOT run_record MATCHES "reserved_count=0" OR
         NOT run_record MATCHES "tick_min=256" OR
         NOT run_record MATCHES "tick_max=50000000" OR
         NOT run_record MATCHES "ticks_observed=[0-9]+" OR
         NOT run_record MATCHES "completion_margin_ticks=[0-9]+" OR
         NOT run_record MATCHES "maximum_headroom_ticks=[0-9]+" OR
         NOT run_record MATCHES "guest_progress=8" OR
         NOT raw_results MATCHES "result_count\t18" OR
         NOT raw_results MATCHES "result\t17\t00" OR
         NOT comparison MATCHES "comparison\tmatch")
        set(FAILURE "raw, comparison, model or source-bound ${ram_case} record is incomplete")
        break()
      endif()
      file(STRINGS "${SUITE_BUILD}/raw-results-${ram_case}.tsv" raw_lines REGEX "^result\t[0-9]+\t[0-9a-f][0-9a-f]$")
      list(LENGTH raw_lines raw_count)
      if(NOT raw_count EQUAL 18)
        set(FAILURE "CPU ${PROFILE}/${ram_case} raw observation must contain exactly 18 values; got ${raw_count}")
        break()
      endif()
      if(ram_case STREQUAL "zero")
        set(zero_results "${raw_lines}")
        set(zero_comparison "${comparison}")
      else()
        if(NOT raw_lines STREQUAL zero_results OR NOT comparison STREQUAL zero_comparison)
          set(FAILURE "fresh zero and seeded CPU instances produced different raw or source comparison results")
          break()
        endif()
      endif()
    endforeach()
  endif()
endif()

if(MODE STREQUAL "cpu_model_controls" AND FAILURE STREQUAL "")
  cpu_batch_begin()
  set(cpu_lynx1_base "${WORK}/cpu-baseline/lynx1")
  set(cpu_lynx2_base "${WORK}/cpu-baseline/lynx2")
  execute_process(COMMAND "${TEST_BINARY}" run cpu
    RESULT_VARIABLE missing_profile_rc OUTPUT_VARIABLE missing_profile_out ERROR_VARIABLE missing_profile_err)
  set(missing_profile_output "${missing_profile_out}${missing_profile_err}")
  if(NOT missing_profile_rc STREQUAL "2" OR
     NOT missing_profile_output MATCHES "usage: lynx-test-suites COMMAND" OR
     NOT missing_profile_output MATCHES "run cpu PROFILE BUILDDIR ORACLE")
    set(FAILURE "missing CPU suite profile was not rejected with the explicit usage contract")
  endif()
  if(FAILURE STREQUAL "")
    expect_failure("suite: explicit lynx1 or lynx2 profile input required"
      "${TEST_BINARY}" run cpu none "${SUITE_BUILD}" "${ORACLE}" zero)
  endif()

  if(FAILURE STREQUAL "")
    file(READ "${cpu_lynx2_base}/run-record-zero.txt" lynx2_record)
    if(NOT lynx2_record MATCHES "detected_model=lynx2" OR
       NOT lynx2_record MATCHES "model_probe_bytes=00,a2" OR
       NOT lynx2_record MATCHES "source_tuple_model=lynx2" OR
       NOT lynx2_record MATCHES "meaningful_count=18")
      set(FAILURE "real Lynx II execution record did not retain its independent model evidence")
    else()
      expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx2"
        "${TEST_BINARY}" validate-cpu-record lynx2
          "${cpu_lynx2_base}/raw-results-zero.tsv"
          "${cpu_lynx2_base}/run-record-zero.txt" "${cpu_lynx2_base}/oracle.tsv" "${cpu_lynx2_base}")
    endif()

  endif()

  if(FAILURE STREQUAL "")
    run_cpu_profile_pair()
    cpu_build_identity_control("revision" "revision" "suite: oracle: source identity mismatch")
    cpu_build_identity_control("source-self-consistent" "source-self-consistent" "suite: oracle: source identity mismatch")
    cpu_build_identity_control("object" "object" "suite: build record object identity mismatch")
    cpu_build_identity_control("labels" "labels" "suite: build record labels identity mismatch")
    expect_failure("suite: detected model does not match selected profile input"
      "${TEST_BINARY}" run cpu-selected lynx1 lynx2 "${SUITE_BUILD}" "${ORACLE}" zero)
    run_cpu_profile_pair()
    file(READ "${cpu_lynx1_base}/run-record-zero.txt" base_record)
    file(READ "${cpu_lynx1_base}/raw-results-zero.tsv" base_raw)

    set(mutated_record "${WORK}/cpu-missing-probe.record")
    set(mutated_raw "${WORK}/cpu-missing-probe.raw")
    string(REPLACE "model_probe_bytes=01,a1\n" "" changed_record "${base_record}")
    file(WRITE "${mutated_record}" "${changed_record}")
    file(WRITE "${mutated_raw}" "${base_raw}")
    expect_cpu_record_failure("cpu record missing field: model_probe_bytes" "${mutated_raw}" "${mutated_record}")

    set(mutated_record "${WORK}/cpu-corrupt-probe.record")
    set(mutated_raw "${WORK}/cpu-corrupt-probe.raw")
    string(REPLACE "model_probe_bytes=01,a1" "model_probe_bytes=ff,a1" changed_record "${base_record}")
    file(WRITE "${mutated_record}" "${changed_record}")
    file(WRITE "${mutated_raw}" "${base_raw}")
    expect_cpu_record_failure("cpu record: invalid model probe bytes" "${mutated_raw}" "${mutated_record}")

    file(STRINGS "${cpu_lynx1_base}/oracle.tsv" tuple_rows REGEX "^tuple\t")
    set(lynx1_tuple "")
    set(lynx2_tuple "")
    foreach(tuple_row IN LISTS tuple_rows)
      if(tuple_row MATCHES "^tuple\tlynx1\t")
        string(REPLACE "tuple\tlynx1\t" "" lynx1_tuple "${tuple_row}")
      elseif(tuple_row MATCHES "^tuple\tlynx2\t")
        string(REPLACE "tuple\tlynx2\t" "" lynx2_tuple "${tuple_row}")
      endif()
    endforeach()
    if(lynx1_tuple STREQUAL "" OR lynx2_tuple STREQUAL "")
      set(FAILURE "source model tuples were not available for the captured tuple disagreement control")
    else()
      set(mutated_record "${WORK}/cpu-tuple-probe-disagreement.record")
      set(mutated_raw "${WORK}/cpu-tuple-probe-disagreement.raw")
      replace_cpu_tuple("${base_raw}" "${lynx1_tuple}" "${lynx2_tuple}" changed_raw)
      if(changed_raw STREQUAL base_raw)
        set(FAILURE "captured tuple disagreement control did not change source-derived bytes")
      else()
        file(WRITE "${mutated_record}" "${base_record}")
        file(WRITE "${mutated_raw}" "${changed_raw}")
        expect_cpu_record_failure("cpu record: guest model probe and source-defined tuple disagree" "${mutated_raw}" "${mutated_record}")
      endif()
    endif()

    if(FAILURE STREQUAL "")
      set(mutated_record "${WORK}/cpu-count-drift.record")
      set(mutated_raw "${WORK}/cpu-count-drift.raw")
      string(REPLACE "meaningful_count=18" "meaningful_count=17" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      file(WRITE "${mutated_raw}" "${base_raw}")
      expect_cpu_record_failure("cpu record: result count drift" "${mutated_raw}" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-raw-count-drift.record")
      set(mutated_raw "${WORK}/cpu-raw-count-drift.raw")
      string(REPLACE "result_count\t18\n" "result_count\t17\n" changed_raw "${base_raw}")
      file(WRITE "${mutated_record}" "${base_record}")
      file(WRITE "${mutated_raw}" "${changed_raw}")
      expect_cpu_record_failure("cpu record: result count drift" "${mutated_raw}" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-zero-results.record")
      set(mutated_raw "${WORK}/cpu-zero-results.raw")
      file(STRINGS "${cpu_lynx1_base}/raw-results-zero.tsv" raw_lines)
      set(zero_raw "")
      foreach(raw_line IN LISTS raw_lines)
        if(raw_line MATCHES "^result\t([0-9]+)\t[0-9a-f][0-9a-f]$")
          set(index "${CMAKE_MATCH_1}")
          string(APPEND zero_raw "result\t${index}\t00\n")
        else()
          string(APPEND zero_raw "${raw_line}\n")
        endif()
      endforeach()
      file(WRITE "${mutated_record}" "${base_record}")
      file(WRITE "${mutated_raw}" "${zero_raw}")
      expect_cpu_record_failure("cpu record: zero/unexecuted result block" "${mutated_raw}" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-zero-progress.record")
      string(REPLACE "guest_progress=8" "guest_progress=0" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record: guest progress evidence mismatch" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-missing-sentinel.record")
      string(REPLACE "completion_sentinel=a5\n" "" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record missing field: completion_sentinel" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-corrupt-sentinel.record")
      string(REPLACE "completion_sentinel=a5" "completion_sentinel=00" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record: completion sentinel mismatch" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-stale-oracle.record")
      string(REGEX REPLACE "oracle_sha256=[0-9a-f]+" "oracle_sha256=0000000000000000000000000000000000000000000000000000000000000000" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record: source identity mismatch" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-stale-bll.record")
      string(REGEX REPLACE "bll_sha256=[0-9a-f]+" "bll_sha256=0000000000000000000000000000000000000000000000000000000000000000" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record: BLL object identity mismatch" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(mutated_record "${WORK}/cpu-stale-labels.record")
      string(REGEX REPLACE "labels_sha256=[0-9a-f]+" "labels_sha256=0000000000000000000000000000000000000000000000000000000000000000" changed_record "${base_record}")
      file(WRITE "${mutated_record}" "${changed_record}")
      expect_cpu_record_failure("cpu record: labels identity mismatch" "${cpu_lynx1_base}/raw-results-zero.tsv" "${mutated_record}")

      set(cross_dir "${WORK}/cpu-cross-profile")
      if(EXISTS "${cross_dir}" OR IS_SYMLINK "${cross_dir}")
        message(FATAL_ERROR "suite_driver: CPU cross-profile directory already exists")
      endif()
      file(MAKE_DIRECTORY "${cross_dir}")
      foreach(file_name raw-results-zero.tsv run-record-zero.txt cpu.bll cpu.lbl build-record.txt oracle.tsv)
        file(COPY_FILE "${cpu_lynx2_base}/${file_name}" "${cross_dir}/${file_name}")
      endforeach()
      write_suite_owner_marker("${cross_dir}")
      expect_cpu_record_failure("cpu record: run identity mismatch"
        "${cross_dir}/raw-results-zero.tsv" "${cross_dir}/run-record-zero.txt" lynx1)
      string(APPEND all_run_out "model controls: exact profile parser refusals plus actual opposite-selection fault bracketed by fresh two-profile positives; immutable captured cases cover zero data/progress, missing/corrupt probe/sentinel, result counts, tuple disagreement, stale source/object/label identity and copied Lynx II completion refused for Lynx I\n")
    endif()
  endif()
endif()

if(MODE STREQUAL "cpu_bound_controls" AND FAILURE STREQUAL "")
  cpu_batch_begin()
  file(READ "${WORK}/cpu-baseline/lynx1/run-record-zero.txt" base_record)
  file(READ "${WORK}/cpu-baseline/lynx1/raw-results-zero.tsv" base_raw)
  file(COPY_FILE "${WORK}/cpu-baseline/lynx1/raw-results-zero.tsv" "${WORK}/cpu-bound-positive.raw")
  set(bound_raw "${WORK}/cpu-bound-positive.raw")

  write_ticks_record("${WORK}/cpu-minimum.record" 256)
  expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx1"
    "${TEST_BINARY}" validate-cpu-record lynx1 "${bound_raw}" "${WORK}/cpu-minimum.record"
    "${WORK}/cpu-baseline/lynx1/oracle.tsv" "${WORK}/cpu-baseline/lynx1")
  cpu_batch_positive()

  write_ticks_record("${WORK}/cpu-maximum.record" 50000000)
  expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx1"
    "${TEST_BINARY}" validate-cpu-record lynx1 "${bound_raw}" "${WORK}/cpu-maximum.record"
    "${WORK}/cpu-baseline/lynx1/oracle.tsv" "${WORK}/cpu-baseline/lynx1")
  cpu_batch_positive()

  write_ticks_record("${WORK}/cpu-below-minimum.record" 255)
  expect_cpu_record_failure("cpu record: ticks outside completion bounds" "${bound_raw}" "${WORK}/cpu-below-minimum.record")

  write_ticks_record("${WORK}/cpu-above-maximum.record" 50000001)
  expect_cpu_record_failure("cpu record: ticks outside completion bounds" "${bound_raw}" "${WORK}/cpu-above-maximum.record")

  write_ticks_record("${WORK}/cpu-time-overflow.record" 4294967296)
  expect_cpu_record_failure("cpu record: invalid ticks_observed" "${bound_raw}" "${WORK}/cpu-time-overflow.record")
  string(APPEND all_run_out "bound controls: exact 256/50000000 captured-record boundaries pass; 255/50000001 and uint32 overflow fail the named time validator against invocation-owned positives\n")
endif()

if(MODE STREQUAL "timer_observation" AND FAILURE STREQUAL "")
  run_timer_observation("${PROFILE}")
  string(APPEND all_run_out "timer observation: profile=${PROFILE}, 16 source rows retained with 15 agreements and the exact pending index14 row; independent proof and acyclic companion validated\n")
endif()

if(MODE STREQUAL "timer_video_revisit" AND FAILURE STREQUAL "")
  run_timer_video_pair()
  string(APPEND all_run_out "video-policy timer revisit: both profiles independently configured before execution; exact strict row sets and outcomes retained without a pending exception\n")
endif()

if(MODE STREQUAL "timer_video_revisit_controls" AND FAILURE STREQUAL "")
  run_timer_video_revisit_controls()
endif()

if(MODE STREQUAL "timer_strict_controls" AND FAILURE STREQUAL "")
  foreach(profile lynx1 lynx2)
    run_timer_observation("${profile}")
    expect_failure("suite: timer source oracle comparison mismatch"
      "${TEST_BINARY}" run timers "${profile}" "${SUITE_BUILD}" "${ORACLE}")
    set(comparison "${SUITE_BUILD}/comparison-${profile}.tsv")
    file(STRINGS "${comparison}" comparison_rows REGEX "^result\t[0-9]+\t")
    list(LENGTH comparison_rows comparison_count)
    set(accepted 0)
    set(mismatch 0)
    foreach(row IN LISTS comparison_rows)
      if(row MATCHES "source-accepted$")
        math(EXPR accepted "${accepted} + 1")
      elseif(row MATCHES "source-mismatch$")
        math(EXPR mismatch "${mismatch} + 1")
        if(NOT row STREQUAL "result\t14\t0c\tEXPECT(0d)\tsource-mismatch")
          message(FATAL_ERROR "suite_driver: strict source mismatch is not the exact D-10 row: ${row}")
        endif()
      endif()
    endforeach()
    if(NOT comparison_count EQUAL 16 OR NOT accepted EQUAL 15 OR NOT mismatch EQUAL 1)
      message(FATAL_ERROR "suite_driver: strict timer rejection lost comparison evidence for ${profile}")
    endif()
    file(READ "${SUITE_BUILD}/run-record-${profile}.txt" strict_record)
    if(NOT strict_record MATCHES "source_comparison=separate-record" OR
       NOT strict_record MATCHES "hardware_accuracy=unverified" OR
       NOT strict_record MATCHES "profile_qualification=unresolved")
      message(FATAL_ERROR "suite_driver: strict timer record obscured the unresolved software mismatch")
    endif()
    string(APPEND all_run_out "strict comparison: profile=${profile} exit1 on exact source mismatch; 16 rows, 15 accepted, one index14 actual0c/EXPECT(0d)\n")
  endforeach()
endif()

if(MODE STREQUAL "timer_cascade_model" AND FAILURE STREQUAL "")
  foreach(profile lynx1 lynx2)
    run_timer_observation("${profile}")
    expect_success("timer cascade model: 64 phase offsets derive 12 acknowledgments for ${profile}"
      "${TEST_BINARY}" timer-cascade-model "${profile}" "${SUITE_BUILD}" "${ORACLE}"
      "${ROOT}/docs/timer-source-index14-pending.json" "${ROOT}/build/pins/lynx-tests")
    assert_timer_artifacts("${profile}")
    string(APPEND all_run_out "cascade model: profile=${profile}; linked bytes, 192-poll arithmetic and all64 phases reproduce12; proof/companion bindings valid\n")
  endforeach()
endif()

if(MODE STREQUAL "timer_model_controls" AND FAILURE STREQUAL "")
  run_timer_pair()
  expect_failure("suite: timer guest model does not match selected profile input"
    "${TEST_BINARY}" run timers-selected lynx1 lynx2 "${SUITE_BUILD}" "${ORACLE}")
  run_timer_pair()
  string(APPEND all_run_out "model controls: actual Lynx1 guest with opposite Lynx2 selection refuses before source comparison; both fresh profile observations pass before and after\n")
endif()

if(MODE STREQUAL "timer_result_controls" AND FAILURE STREQUAL "")
  timer_batch_begin()
  timer_case_begin("copied-lynx2-profile" lynx1)
  set(cross_profile_case "${TIMER_CASE_DIR}")
  foreach(mapping
      "raw-results-lynx2.tsv|raw-results-lynx1.tsv"
      "run-record-lynx2.txt|run-record-lynx1.txt"
      "comparison-lynx2.tsv|comparison-lynx1.tsv"
      "cascade-proof-lynx2.json|cascade-proof-lynx1.json"
      "pending-record-lynx2.json|pending-record-lynx1.json")
    string(REPLACE "|" ";" parts "${mapping}")
    list(GET parts 0 source_name)
    list(GET parts 1 destination_name)
    file(COPY_FILE "${WORK}/timer-baseline/lynx2/${source_name}" "${cross_profile_case}/${destination_name}")
  endforeach()
  expect_failure("timer raw record identity mismatch"
    "${TEST_BINARY}" validate-timer-record lynx1
      "${cross_profile_case}/raw-results-lynx1.tsv"
      "${cross_profile_case}/run-record-lynx1.txt"
      "${cross_profile_case}/oracle.tsv"
      "${cross_profile_case}/timer-source-index14-pending.json")
  timer_case_end()
  run_timer_comparator_value(8 "04,05" 04 TRUE)
  run_timer_comparator_value(8 "04,05" 05 TRUE)
  run_timer_comparator_value(11 "35,36,37" 35 TRUE)
  run_timer_comparator_value(11 "35,36,37" 36 TRUE)
  run_timer_comparator_value(11 "35,36,37" 37 TRUE)
  run_timer_comparator_value(8 "04,05" 03 FALSE)
  run_timer_comparator_value(8 "04,05" 06 FALSE)
  run_timer_comparator_value(11 "35,36,37" 34 FALSE)
  run_timer_comparator_value(11 "35,36,37" 38 FALSE)
  timer_record_control("model_probe_bytes" "01,a1" "__REMOVE__" "timer record missing field: model_probe_bytes")
  timer_record_control("model_probe_bytes" "01,a1" "ff,a1" "timer record: model probe or selected profile mismatch")
  timer_record_control("detected_model" "lynx1" "lynx2" "timer record: model probe or selected profile mismatch")
  timer_record_control("guest_probe_byte" "01" "00" "timer record: probe evidence mismatch")
  timer_record_control("guest_model_marker" "a1" "a2" "timer record: marker evidence mismatch")
  timer_record_control("meaningful_count" "16" "15" "timer record: result count drift")
  timer_record_control("reserved_count" "2" "1" "timer record: result count drift")
  timer_record_control("result_extent" "18" "17" "timer record: result count drift")
  timer_record_control("meaningful_results" "16" "17" "timer record: result count drift")
  timer_record_control("reserved_results" "2" "3" "timer record: result count drift")
  timer_record_control("completion_sentinel" "a5" "00" "timer record: completion sentinel mismatch")
  timer_record_control("completion_sentinel" "a5" "__REMOVE__" "timer record missing field: completion_sentinel")
  timer_record_control("guest_progress" "8" "7" "timer record: guest progress evidence mismatch")
  timer_record_control("cost_ticks" "5" "6" "timer record: environment metadata mismatch")
  timer_record_control("sample_offset_ticks" "0" "5" "timer record: environment metadata mismatch")
  timer_record_control("counter_initialization" "zero" "seeded" "timer record: environment metadata mismatch")
  timer_record_control("page_mode_video_enabled" "false" "true" "timer record: environment metadata mismatch")
  timer_record_control("refresh_dma_enabled" "false" "true" "timer record: environment metadata mismatch")
  timer_record_control("phase_policy" "timer-phase-fixed-zero-1" "timer-phase-shifted" "timer record: policy metadata mismatch")
  timer_record_control("scheduler_policy" "timer-cpu-poll-order-1" "timer-cpu-reversed" "timer record: policy metadata mismatch")
  timer_record_control("profile_input" "lynx1" "lynx2" "timer record: run identity mismatch")
  timer_record_control("bll_sha256" "d766a24031246ad166edc6b417e4d30a841e18dda5115eae201804847ef96c79" "0000000000000000000000000000000000000000000000000000000000000000" "timer record: BLL object identity mismatch")
  timer_record_control("labels_sha256" "b1423949c03badc1158b5cacfb67af146cbe18228cc73f156b11dd548776efa3" "0000000000000000000000000000000000000000000000000000000000000000" "timer record: labels identity mismatch")
  timer_record_control("oracle_sha256" "08b5a4f48caf0f92e7bc177d9f3a6ac8ff68d757b57b9bc346aad8bd4c698b4f" "0000000000000000000000000000000000000000000000000000000000000000" "timer record: oracle identity mismatch")
  timer_record_control("wrapper_sha256" "fa0973d355eeba6311d9540f3d2ec7d429f6991e1e5641fb36ed7af6c2749e6c" "0000000000000000000000000000000000000000000000000000000000000000" "timer record: wrapper identity mismatch")
  timer_record_control("tick_min" "256" "255" "timer record: completion bound metadata mismatch")
  timer_tick_control(255 "timer record: ticks outside completion bounds")
  timer_tick_control(50000001 "timer record: ticks outside completion bounds")
  timer_tick_control(4294967296 "timer record: invalid ticks_observed")
  timer_zero_result_control()
  timer_index14_control(0b)
  timer_index14_control(0d)
  timer_second_mismatch_control()
  timer_captured_file_control("raw-row-missing-15" raw "result\t15\t08\n" "__REMOVE__" "timer raw record shape mismatch")
  timer_captured_file_control("raw-index14-duplicate" raw "result\t14\t0c\n" "result\t14\t0c\nresult\t14\t0c\n" "timer raw record count or byte malformed")
  timer_captured_file_control("raw-extra-row" raw "result\t15\t08\n" "result\t15\t08\nresult\t16\t00\n" "timer raw record has extra rows")
  timer_captured_file_control("comparison-row-missing-15" comparison "result\t15\t08\tEXPECT(08)\tsource-accepted\n" "__REMOVE__" "timer comparison record row mismatch")
  timer_captured_file_control("comparison-index14-duplicate" comparison "result\t14\t0c\tEXPECT(0d)\tsource-mismatch\n" "result\t14\t0c\tEXPECT(0d)\tsource-mismatch\nresult\t14\t0c\tEXPECT(0d)\tsource-mismatch\n" "timer comparison record row mismatch")
  timer_captured_file_control("comparison-summary-mismatch" comparison "comparison\tmismatch\n" "__REMOVE__" "timer comparison record summary mismatch")
  timer_batch_end()
  string(APPEND all_run_out "alternative controls: captured comparator corpus accepts every 04/05 and 35/36/37 member and rejects all adjacent outside values; no corpus row is labeled guest execution\n")
endif()

if(MODE STREQUAL "timer_pending_controls" AND FAILURE STREQUAL "")
  timer_batch_begin()
  timer_symlink_output_control()
  timer_build_identity_control("revision-strict" "revision" strict "suite: oracle: source identity mismatch")
  timer_build_identity_control("source-self-consistent-no-run" "source-self-consistent" no-run "suite: oracle: source identity mismatch")
  timer_build_identity_control("object-strict" "object" strict "suite: build record object identity mismatch")
  timer_build_identity_control("object-no-run" "object" no-run "suite: build record object identity mismatch")
  timer_build_identity_control("labels-proof" "labels" proof "suite: build record labels identity mismatch")
  timer_descriptor_control("descriptor-missing-file" "" "__MISSING_FILE__" "pending descriptor malformed JSON")
  timer_descriptor_control("descriptor-malformed" "" "__MALFORMED__" "pending descriptor malformed JSON")
  timer_descriptor_control("descriptor-unknown-key" "\n}" ",\n  \"unknown_key\":true\n}" "pending descriptor unknown key")
  timer_descriptor_control("descriptor-duplicate-key" "" "__DUPLICATE__" "pending descriptor duplicate key")
  timer_descriptor_control("descriptor-missing-schema" "  \"schema\": \"lynxplosion.timer-source-pending\",\n" "" "pending descriptor missing schema/version")
  timer_descriptor_control("descriptor-missing-version" "  \"version\": 1,\n" "" "pending descriptor missing schema/version")
  timer_descriptor_control("descriptor-unsupported-schema" "\"schema\": \"lynxplosion.timer-source-pending\"" "\"schema\": \"lynxplosion.timer-source-pending-stale\"" "pending descriptor unsupported schema/version")
  timer_descriptor_control("descriptor-unsupported-version" "\"version\": 1" "\"version\": 2" "pending descriptor unsupported schema/version")
  timer_descriptor_control("descriptor-actual-type" "\"actual\": \"0c\"" "\"actual\": 12" "pending descriptor invalid field type")
  timer_descriptor_control("descriptor-source-index" "\"source_index\": 14" "\"source_index\": 13" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-expected-source-set" "\"expected_source_set\": [\"0d\"]" "\"expected_source_set\": [\"0c\"]" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-actual-value" "\"actual\": \"0c\"" "\"actual\": \"0b\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-profiles" "\"profiles\": [\"lynx1\", \"lynx2\"]" "\"profiles\": [\"lynx1\"]" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-revisit-phase" "\"revisit_phase\": 5" "\"revisit_phase\": 6" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-pin-revision" "\"lynx_tests_revision\": \"b583dc43d532d3c41563a6fa35420614fbcead89\"" "\"lynx_tests_revision\": \"0000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-archive-hash" "\"lynx_tests_archive_sha256\": \"a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e\"" "\"lynx_tests_archive_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-cc65-hash" "\"cc65_pin_sha256\": \"d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1\"" "\"cc65_pin_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-timers-tests-hash" "\"timers_tests_sha256\": \"dde10583ae050aba1dcd72f3e0b92cdf2d38d5037c4380c5e8864f2c81702e1f\"" "\"timers_tests_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-timers-main-hash" "\"timers_main_sha256\": \"b89d4207e837fb4a85b1dcf6b62ffb56c8e62317292b02deeafc22ac02bada3a\"" "\"timers_main_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-oracle-hash" "\"oracle_sha256\": \"08b5a4f48caf0f92e7bc177d9f3a6ac8ff68d757b57b9bc346aad8bd4c698b4f\"" "\"oracle_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-wrapper-hash" "\"wrapper_sha256\": \"fa0973d355eeba6311d9540f3d2ec7d429f6991e1e5641fb36ed7af6c2749e6c\"" "\"wrapper_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-build-recipe-hash" "\"build_recipe_sha256\": \"a6f9e8b56e5f632d7d3186076fab5d0712e9b53017e3f4842888091670829c02\"" "\"build_recipe_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-oracle-generator-hash" "\"oracle_generator_sha256\": \"5edca26b46bbfeff95e7b348322455df7f32a2a0bce1fec3cc1bca6723d25a32\"" "\"oracle_generator_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-phase-policy" "\"phase_policy\": \"timer-phase-fixed-zero-1\"" "\"phase_policy\": \"timer-phase-shifted\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-scheduler-policy" "\"scheduler_policy\": \"timer-cpu-poll-order-1\"" "\"scheduler_policy\": \"timer-cpu-reversed\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-cost-model" "\"cost_model\": \"flat5\"" "\"cost_model\": \"flat4\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-cost-ticks" "\"cost_ticks\": 5" "\"cost_ticks\": 6" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-sample-offset" "\"sample_offset_ticks\": 0" "\"sample_offset_ticks\": 5" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-video-policy" "\"page_mode_video_enabled\": false" "\"page_mode_video_enabled\": true" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-dma-policy" "\"refresh_dma_enabled\": false" "\"refresh_dma_enabled\": true" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-counter-init" "\"counter_initialization\": \"zero\"" "\"counter_initialization\": \"seeded\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-proof-report-path" "\"proof_report_path\": \".planning/debug/evidence/timer-suite-cascade-count.md\"" "\"proof_report_path\": \"other.md\"" "pending descriptor value mismatch")
  timer_descriptor_control("descriptor-proof-report-hash" "\"proof_report_sha256\": \"80c8fbb7fd082ad8f0e5a909e249a243049c2315a9e3a72e8835b4e053f2f2af\"" "\"proof_report_sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"" "pending descriptor value mismatch")

  timer_companion_control("companion-malformed" "" "__MALFORMED__" "suite: pending record malformed JSON")
  timer_companion_control("companion-unknown-key" "" "__UNKNOWN__" "suite: pending record unknown key")
  timer_companion_control("companion-duplicate-key" "" "__DUPLICATE__" "suite: pending record duplicate key")
  timer_companion_control("companion-missing-schema" "  \"schema\":\"lynxplosion.timer-source-pending-record\",\n" "__REMOVE_FIELD__" "suite: pending record missing key")
  timer_companion_control("companion-unsupported-schema" "\"schema\":\"lynxplosion.timer-source-pending-record\"" "\"schema\":\"lynxplosion.timer-source-pending-record-stale\"" "suite: pending record unsupported schema/version")
  timer_companion_control("companion-version-type" "\"version\":1" "\"version\":\"1\"" "suite: pending record invalid field type")
  timer_companion_control("companion-actual-type" "\"actual\":\"0c\"" "\"actual\":12" "suite: pending record invalid field type")
  timer_companion_control("companion-actual-value" "\"actual\":\"0c\"" "\"actual\":\"0b\"" "suite: pending record value mismatch")
  timer_companion_control("companion-version-value" "\"version\":1" "\"version\":2" "suite: pending record unsupported schema/version")
  timer_companion_control("companion-missing-version" "  \"version\":1,\n" "__REMOVE_FIELD__" "suite: pending record missing key")
  timer_companion_control("companion-missing-file" "" "__MISSING_FILE__" "suite: pending record malformed JSON")
  timer_companion_control("companion-raw-hash" "HASH:raw_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-record-hash" "HASH:record_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-comparison-hash" "HASH:comparison_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-bll-hash" "HASH:bll_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-labels-hash" "HASH:labels_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-oracle-hash" "HASH:oracle_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-descriptor-hash" "HASH:descriptor_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-proof-hash" "HASH:proof_sha256" "unused" "suite: pending record artifact identity mismatch")
  timer_companion_control("companion-self-hash" "" "__SELF_HASH__" "suite: pending record unknown key")
  timer_proof_control("__CORRUPT__" "timer companion: cascade proof is stale or does not match linked source evidence")
  timer_proof_control("__MISSING__" "timer companion: complete raw, run, comparison or proof inputs missing")
  timer_batch_end()
  string(APPEND all_run_out "pending controls: descriptor schema, D-10 value/source/tool/environment/proof identity and companion parser/hash faults all refuse; captured cases use private copies with immutable positive baselines and fresh guest observations at batch boundaries\n")
endif()

if(MODE STREQUAL "tracer" AND FAILURE STREQUAL "")
  file(SHA256 "${ROOT}/fixtures/suites/cpu-wrapper.s" wrapper_sha)
  file(READ "${ROOT}/fixtures/manifests/cpu-tests.manifest" manifest)
  if(NOT manifest MATCHES "wrapper_sha256 = ${wrapper_sha}")
    set(FAILURE "CPU wrapper identity is not bound by its fixture manifest")
  endif()
endif()

if(MODE STREQUAL "oracle_controls" AND FAILURE STREQUAL "")
  expect_success("oracle: cpu=18/18 timers=16/18 source identity and alternatives valid"
    "${TEST_BINARY}" validate-oracle "${ORACLE}")
  expect_failure("suite: timer record: invalid profile"
    "${TEST_BINARY}" timer-cascade-model invalid-profile "${WORK}/invalid-profile-not-created"
    "${ORACLE}" "${ROOT}/docs/timer-source-index14-pending.json"
    "${ROOT}/build/pins/lynx-tests")
  if(EXISTS "${WORK}/invalid-profile-not-created")
    set(FAILURE "invalid profile created an output directory")
  endif()

  string(REPEAT "a" 48 cleanup_nonce)
  set(cleanup_parent "${WORK}/cleanup-controls")
  set(cleanup_outside "${WORK}/cleanup-outside.txt")
  file(WRITE "${cleanup_outside}" "outside cleanup target stays unchanged\n")
  file(SHA256 "${cleanup_outside}" cleanup_outside_before)
  foreach(cleanup_case valid missing-marker wrong-marker)
    set(cleanup_dir "${cleanup_parent}/${cleanup_case}/run-${cleanup_nonce}")
    file(MAKE_DIRECTORY "${cleanup_dir}/nested")
    file(COPY "${SUITE_BUILD}/cpu.bll" DESTINATION "${cleanup_dir}/nested")
    if(NOT cleanup_case STREQUAL "missing-marker")
      set(cleanup_owner "lynx-suite-output-v1\n${cleanup_dir}\n${cleanup_nonce}\n")
      if(cleanup_case STREQUAL "wrong-marker")
        string(APPEND cleanup_owner "stale-owner\n")
      endif()
      file(WRITE "${cleanup_dir}/.lynx-suite-owner" "${cleanup_owner}")
    endif()
    if(cleanup_case STREQUAL "valid")
      file(CREATE_LINK "${cleanup_outside}" "${cleanup_dir}/nested/outside-link" SYMBOLIC RESULT cleanup_link_result)
      if(NOT cleanup_link_result STREQUAL "0")
        message(FATAL_ERROR "suite_driver: cleanup child symlink fixture could not be created")
      endif()
      expect_success("suite cleanup: exact owned invocation removed"
        "${CMAKE_COMMAND}" -DSUITE_CLEANUP_CHECK=1 "-DSCRATCH_ROOT=${SCRATCH_ROOT}"
        "-DCLEANUP_DIRECTORY=${cleanup_dir}" "-DCLEANUP_NONCE=${cleanup_nonce}"
        -P "${ROOT}/tests/suites/run.cmake")
      if(EXISTS "${cleanup_dir}" OR IS_SYMLINK "${cleanup_dir}")
        message(FATAL_ERROR "suite_driver: successful cleanup retained generated artifacts")
      endif()
    else()
      if(cleanup_case STREQUAL "missing-marker")
        set(cleanup_refusal "suite_driver: cleanup owner marker missing or invalid")
      else()
        set(cleanup_refusal "suite_driver: cleanup owner marker mismatch")
      endif()
      expect_failure("${cleanup_refusal}"
        "${CMAKE_COMMAND}" -DSUITE_CLEANUP_CHECK=1 "-DSCRATCH_ROOT=${SCRATCH_ROOT}"
        "-DCLEANUP_DIRECTORY=${cleanup_dir}" "-DCLEANUP_NONCE=${cleanup_nonce}"
        -P "${ROOT}/tests/suites/run.cmake")
      if(NOT EXISTS "${cleanup_dir}/nested/cpu.bll")
        message(FATAL_ERROR "suite_driver: refused cleanup changed an unowned artifact")
      endif()
    endif()
  endforeach()
  set(cleanup_alias "${cleanup_parent}/alias")
  file(CREATE_LINK "${cleanup_parent}/missing-marker" "${cleanup_alias}" SYMBOLIC RESULT cleanup_alias_result)
  if(NOT cleanup_alias_result STREQUAL "0")
    message(FATAL_ERROR "suite_driver: cleanup ancestor symlink fixture could not be created")
  endif()
  expect_failure("suite_driver: refusing symlink ancestor in cleanup directory"
    "${CMAKE_COMMAND}" -DSUITE_CLEANUP_CHECK=1 "-DSCRATCH_ROOT=${SCRATCH_ROOT}"
    "-DCLEANUP_DIRECTORY=${cleanup_alias}/run-${cleanup_nonce}" "-DCLEANUP_NONCE=${cleanup_nonce}"
    -P "${ROOT}/tests/suites/run.cmake")
  file(SHA256 "${cleanup_outside}" cleanup_outside_after)
  if(NOT cleanup_outside_before STREQUAL cleanup_outside_after OR
     NOT EXISTS "${cleanup_parent}/missing-marker/run-${cleanup_nonce}/nested/cpu.bll")
    message(FATAL_ERROR "suite_driver: cleanup changed an outside symlink target")
  endif()

  file(READ "${ROOT}/build/pins/lynx-tests/.lynx-pin" live_pin_marker)
  string(STRIP "${live_pin_marker}" expected_pin_marker)
  string(LENGTH "${expected_pin_marker}" expected_pin_length)
  if(NOT expected_pin_length EQUAL 64 OR NOT expected_pin_marker MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "suite_driver: expected source pin identity is malformed")
  endif()
  file(WRITE "${WORK}/pin-marker-bare.txt" "${expected_pin_marker}")
  file(WRITE "${WORK}/pin-marker-newline.txt" "${expected_pin_marker}\n")
  foreach(valid_marker bare newline)
    expect_success("pin marker: exact identity valid" "${TEST_BINARY}" validate-pin-marker
      "${WORK}/pin-marker-${valid_marker}.txt" "${expected_pin_marker}")
  endforeach()
  file(WRITE "${WORK}/pin-marker-trailing-data.txt" "${expected_pin_marker}\nextra-data")
  file(WRITE "${WORK}/pin-marker-extra-newline.txt" "${expected_pin_marker}\n\n")
  file(WRITE "${WORK}/pin-marker-extra-byte.txt" "${expected_pin_marker}x")
  string(SUBSTRING "${expected_pin_marker}" 0 63 short_pin_marker)
  file(WRITE "${WORK}/pin-marker-short.txt" "${short_pin_marker}")
  string(REPEAT "0" 64 wrong_pin_marker)
  file(WRITE "${WORK}/pin-marker-wrong.txt" "${wrong_pin_marker}\n")
  foreach(invalid_marker trailing-data extra-newline extra-byte short wrong)
    expect_failure("suite: pin marker: malformed or mismatched identity"
      "${TEST_BINARY}" validate-pin-marker "${WORK}/pin-marker-${invalid_marker}.txt"
      "${expected_pin_marker}")
  endforeach()

  set(oracle_attack_parent "${WORK}/oracle-temp-attack")
  set(oracle_attack_out "${oracle_attack_parent}/generated.tsv")
  set(oracle_outside_fixture "${WORK}/oracle-outside-fixture.txt")
  set(oracle_candidate_record "${WORK}/oracle-temp-candidate.txt")
  file(MAKE_DIRECTORY "${oracle_attack_parent}")
  file(WRITE "${oracle_outside_fixture}" "preserve this outside-parent fixture\n")
  file(READ "${oracle_outside_fixture}" oracle_outside_before)
  execute_process(COMMAND sh -c
    "set -eu; out=\"\$1\"; root=\"\$2\"; outside=\"\$3\"; candidate=\"\$4\"; ln -s \"\$outside\" \"\$out.\$\$\"; printf '%s\\n' \"\$out.\$\$\" > \"\$candidate\"; exec sh \"\$root/scripts/generate-suite-oracle.sh\" --source \"\$root/build/pins/lynx-tests\" --out \"\$out\""
    sh "${oracle_attack_out}" "${ROOT}" "${oracle_outside_fixture}" "${oracle_candidate_record}"
    RESULT_VARIABLE oracle_attack_rc OUTPUT_VARIABLE oracle_attack_out_text ERROR_VARIABLE oracle_attack_err)
  file(READ "${oracle_outside_fixture}" oracle_outside_after)
  if(EXISTS "${oracle_candidate_record}")
    file(READ "${oracle_candidate_record}" oracle_candidate_path)
    string(STRIP "${oracle_candidate_path}" oracle_candidate_path)
  else()
    set(oracle_candidate_path "")
    set(FAILURE "oracle temporary symlink control could not capture the planted candidate path")
  endif()
  if(NOT oracle_attack_rc STREQUAL "0" OR
     NOT oracle_outside_after STREQUAL oracle_outside_before OR
     NOT EXISTS "${oracle_attack_out}" OR IS_DIRECTORY "${oracle_attack_out}" OR
     IS_SYMLINK "${oracle_attack_out}" OR oracle_candidate_path STREQUAL "" OR
     NOT EXISTS "${oracle_candidate_path}" OR
     NOT IS_SYMLINK "${oracle_candidate_path}")
    set(FAILURE "oracle temporary symlink control did not preserve the outside fixture and publish a regular file: ${oracle_attack_rc} ${oracle_attack_out_text} ${oracle_attack_err}")
  else()
    execute_process(COMMAND sh -c
      "mode=\$(stat -c %a \"\$1\" 2>/dev/null || stat -f %Lp \"\$1\"); test \"\$mode\" = 600"
      sh "${oracle_attack_out}" RESULT_VARIABLE oracle_mode_rc)
    if(NOT oracle_mode_rc STREQUAL "0")
      set(FAILURE "oracle temporary symlink control did not publish a mode-600 regular file")
    endif()
  endif()
  if(NOT oracle_candidate_path STREQUAL "")
    file(REMOVE "${oracle_candidate_path}")
  endif()

  set(outside_path_fixture "${WORK}/outside-path-fixture")
  set(symlink_work_parent "${WORK}/symlink-work-parent")
  file(MAKE_DIRECTORY "${outside_path_fixture}")
  file(WRITE "${outside_path_fixture}/sentinel.txt" "keep outside sentinel\n")
  file(WRITE "${outside_path_fixture}/run-record-zero.txt" "keep outside record\n")
  file(SHA256 "${outside_path_fixture}/sentinel.txt" sentinel_before)
  file(SHA256 "${outside_path_fixture}/run-record-zero.txt" record_before)
  file(CREATE_LINK "${outside_path_fixture}" "${symlink_work_parent}" SYMBOLIC RESULT symlink_create_result)
  if(NOT symlink_create_result STREQUAL "0")
    set(FAILURE "symlink ancestor control could not create its outside fixture link: ${symlink_create_result}")
  else()
    execute_process(COMMAND "${CMAKE_COMMAND}"
      "-DMODE=oracle_controls" "-DROOT=${ROOT}"
      "-DWORK=${symlink_work_parent}/attempt"
      "-DSCRATCH_ROOT=${SCRATCH_ROOT}" "-DTEST_BINARY=${TEST_BINARY}"
      "-DENV_EXECUTABLE=${ENV_EXECUTABLE}"
      -P "${ROOT}/tests/suites/run.cmake"
      RESULT_VARIABLE symlink_work_rc OUTPUT_VARIABLE symlink_work_out ERROR_VARIABLE symlink_work_err)
    set(symlink_work_text "${symlink_work_out}${symlink_work_err}")
    file(SHA256 "${outside_path_fixture}/sentinel.txt" sentinel_after)
    file(SHA256 "${outside_path_fixture}/run-record-zero.txt" record_after)
    if(symlink_work_rc STREQUAL "0" OR
       NOT symlink_work_text MATCHES "suite_driver: refusing symlink ancestor in WORK" OR
       NOT sentinel_after STREQUAL sentinel_before OR NOT record_after STREQUAL record_before OR
       EXISTS "${outside_path_fixture}/attempt" OR EXISTS "${outside_path_fixture}/attempt/build")
      set(FAILURE "symlink ancestor control failed to reject before changing outside sentinel/record: ${symlink_work_rc} ${symlink_work_text}")
    endif()
  endif()
  file(REMOVE "${symlink_work_parent}" "${outside_path_fixture}/sentinel.txt"
    "${outside_path_fixture}/run-record-zero.txt")

  file(READ "${ORACLE}" oracle_text)
  file(WRITE "${WORK}/oracle-empty.tsv" "")
  expect_failure("suite: oracle: invalid version header" "${TEST_BINARY}" validate-oracle "${WORK}/oracle-empty.tsv")

  file(WRITE "${WORK}/text-token.txt" "token")
  expect_success("text control: token present within 5 bytes"
    "${TEST_BINARY}" validate-text-token "${WORK}/text-token.txt" 5 token)
  file(WRITE "${WORK}/text-zero-limit.txt" "")
  expect_failure("suite: text control: token missing"
    "${TEST_BINARY}" validate-text-token "${WORK}/text-zero-limit.txt" 0 token)
  string(REPEAT "x" 16384 exact_limit_text)
  file(WRITE "${WORK}/text-exact-limit.txt" "${exact_limit_text}")
  expect_failure("suite: text control: token missing"
    "${TEST_BINARY}" validate-text-token "${WORK}/text-exact-limit.txt" 16384 token)
  string(REPEAT "x" 16385 over_limit_text)
  file(WRITE "${WORK}/text-over-limit.txt" "${over_limit_text}")
  expect_failure("suite: input exceeds its bound or contains NUL"
    "${TEST_BINARY}" validate-text-token "${WORK}/text-over-limit.txt" 16384 token)
  execute_process(COMMAND sh -c "printf 'ab\\000cd' > \"\$1\"" sh
    "${WORK}/text-embedded-nul.txt" RESULT_VARIABLE nul_fixture_rc)
  if(NOT nul_fixture_rc STREQUAL "0")
    set(FAILURE "text parser control could not create an embedded-NUL fixture")
  else()
    expect_failure("suite: text input contains NUL"
      "${TEST_BINARY}" validate-text-token "${WORK}/text-embedded-nul.txt" 16 token)
  endif()

  set(mutated "${WORK}/oracle-malformed-byte.tsv")
  string(REPLACE "result\t17\t1\t00\n" "result\t17\t1\tGG\n" changed "${oracle_text}")
  if(changed STREQUAL oracle_text)
    set(FAILURE "oracle byte control did not mutate the intended row")
  else()
    file(WRITE "${mutated}" "${changed}")
    expect_failure("suite: oracle: malformed result byte" "${TEST_BINARY}" validate-oracle "${mutated}")
  endif()

  string(REPLACE "result_count\t18\n" "result_count\t17\n" changed "${oracle_text}")
  file(WRITE "${WORK}/oracle-count-drift.tsv" "${changed}")
  expect_failure("suite: oracle: result count drift" "${TEST_BINARY}" validate-oracle "${WORK}/oracle-count-drift.tsv")

  string(REPLACE "suite\ttimers\nresult_count\t18\nextent\t18\nmeaningful\t16\n"
                 "suite\ttimers\nresult_count\t18\nextent\t18\nmeaningful\t18\n" changed "${oracle_text}")
  file(WRITE "${WORK}/oracle-timer-extent.tsv" "${changed}")
  expect_failure("suite: oracle: meaningful result count mismatch" "${TEST_BINARY}" validate-oracle "${WORK}/oracle-timer-extent.tsv")

  string(REPLACE "result\t8\t2\t04\t05\n" "result\t8\t1\t04\n" changed "${oracle_text}")
  file(WRITE "${WORK}/oracle-lost-alternative.tsv" "${changed}")
  expect_failure("suite: oracle: timer accepted alternatives were lost" "${TEST_BINARY}" validate-oracle "${WORK}/oracle-lost-alternative.tsv")

  string(REGEX MATCH "cpu_main\t[0-9a-f]+" identity_row "${oracle_text}")
  if(identity_row STREQUAL "")
    set(FAILURE "oracle identity control could not locate the CPU source identity")
  else()
    string(REPLACE "${identity_row}" "cpu_main\t0000000000000000000000000000000000000000000000000000000000000000" changed "${oracle_text}")
    file(WRITE "${WORK}/oracle-stale-identity.tsv" "${changed}")
    expect_failure("suite: oracle: source identity mismatch" "${TEST_BINARY}" run cpu lynx1 "${SUITE_BUILD}" "${WORK}/oracle-stale-identity.tsv")
  endif()

  string(REGEX MATCH "revision\t[0-9a-f]+" revision_row "${oracle_text}")
  if(revision_row STREQUAL "")
    set(FAILURE "oracle revision control could not locate the source revision")
  else()
    string(REPEAT "f" 64 oversized_revision)
    string(REPLACE "${revision_row}" "revision\t${oversized_revision}" changed "${oracle_text}")
    file(WRITE "${WORK}/oracle-overlong-revision.tsv" "${changed}")
    expect_failure("suite: oracle: malformed source identity record"
      "${TEST_BINARY}" validate-oracle "${WORK}/oracle-overlong-revision.tsv")
  endif()

  expect_failure("suite: time: guest ticks outside bounded completion window" "${TEST_BINARY}" validate-time 255)
  expect_success("suite time: accepted guest ticks=256" "${TEST_BINARY}" validate-time 256)
  expect_success("suite time: accepted guest ticks=257" "${TEST_BINARY}" validate-time 257)
  expect_success("suite time: accepted guest ticks=50000000" "${TEST_BINARY}" validate-time 50000000)
  expect_failure("suite: time: guest ticks outside bounded completion window" "${TEST_BINARY}" validate-time 50000001)
  expect_failure("suite: time: invalid guest tick integer" "${TEST_BINARY}" validate-time 4294967296)

  if(FAILURE STREQUAL "")
    reset_shape_source()
    file(READ "${WORK}/source/cpu/main.c" source_text)
    set(needle "    EXPECT(0x00),                       /* Pass=0 (IRQ inside block), 1=fail */")
    string(REPLACE "${needle}" "" changed "${source_text}")
    file(WRITE "${WORK}/source/cpu/main.c" "${changed}")
    expect_shape_failure("CPU must have 18 expectations and both model tuples")

    reset_shape_source()
    file(READ "${WORK}/source/cpu/main.c" source_text)
    string(REPLACE "${needle}" "${needle}\n${needle}" changed "${source_text}")
    file(WRITE "${WORK}/source/cpu/main.c" "${changed}")
    expect_shape_failure("CPU must have 18 expectations and both model tuples")

    reset_shape_source()
    file(READ "${WORK}/source/cpu/main.c" source_text)
    string(REPLACE "EXPECT(0x00),                       /* Pass=0, 1=INC fail, 2=IRQ not taken */"
                   "EXPECT(0xGG),                       /* Pass=0, 1=INC fail, 2=IRQ not taken */" changed "${source_text}")
    file(WRITE "${WORK}/source/cpu/main.c" "${changed}")
    expect_shape_failure("unexpected EXPECT token or nonbyte value")

    reset_shape_source()
    file(READ "${WORK}/source/cpu/main.c" source_text)
    string(REPLACE "EXPECT(0x00),                       /* Pass=0, 1=INC fail, 2=IRQ not taken */"
                   "EXPECT(0x100),                      /* Pass=0, 1=INC fail, 2=IRQ not taken */" changed "${source_text}")
    file(WRITE "${WORK}/source/cpu/main.c" "${changed}")
    expect_shape_failure("unexpected EXPECT token or nonbyte value")

    reset_shape_source()
    file(READ "${WORK}/source/timers/main.c" source_text)
    string(REPLACE "EXPECT(0x04, 0x05)" "EXPECT(0x04)" changed "${source_text}")
    file(WRITE "${WORK}/source/timers/main.c" "${changed}")
    expect_shape_failure("accepted EXPECT alternatives lost")

    reset_shape_source()
    file(READ "${WORK}/source/cpu/main.c" source_text)
    string(REPLACE "{ 0, 1, 2, 7, 9, 10, 12, 16 }" "{ 0, 0, 2, 7, 9, 10, 12, 16 }" changed "${source_text}")
    file(WRITE "${WORK}/source/cpu/main.c" "${changed}")
    expect_shape_failure("overlapping test result offsets")

    reset_shape_source()
    file(READ "${WORK}/source/timers/main.c" source_text)
    string(REPLACE "#define RESULT_COUNT 18" "#define RESULT_COUNT 16" changed "${source_text}")
    file(WRITE "${WORK}/source/timers/main.c" "${changed}")
    expect_shape_failure("result extent must be 18 bytes")

    reset_shape_source()
    file(WRITE "${WORK}/source/cpu/main.c" "")
    expect_shape_failure("missing or truncated EXPECT array")
  endif()

  if(FAILURE STREQUAL "")
    expect_success("oracle: cpu=18/18 timers=16/18 source identity and alternatives valid"
      "${TEST_BINARY}" validate-oracle "${ORACLE}")
  endif()
endif()

if(MODE STREQUAL "symbol_controls" AND FAILURE STREQUAL "")
  set(LABELS "${SUITE_BUILD}/cpu.lbl")
  expect_success("symbols: required suite labels and nonoverlapping extents valid"
    "${TEST_BINARY}" validate-symbols "${LABELS}")
  file(READ "${LABELS}" labels)
  string(REGEX MATCH "al ([0-9A-Fa-f]+) [.]?suite_entry" entry_row "${labels}")
  set(entry_address "${CMAKE_MATCH_1}")
  string(REGEX MATCH "al ([0-9A-Fa-f]+) [.]?_g_results" result_row "${labels}")
  set(result_address "${CMAKE_MATCH_1}")
  if(entry_row STREQUAL "" OR result_row STREQUAL "")
    set(FAILURE "symbol controls could not locate required rows in fresh ld65 output")
  else()
    string(REPLACE "${entry_row}\n" "" changed "${labels}")
    file(WRITE "${WORK}/symbols-missing.lbl" "${changed}")
    expect_failure("suite: symbol missing: suite_entry" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-missing.lbl")

    file(WRITE "${WORK}/symbols-duplicate.lbl" "${labels}${entry_row}\n")
    expect_failure("suite: symbol: duplicate required label" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-duplicate.lbl")

    string(REPLACE "${entry_row}" "${entry_row} extra" changed "${labels}")
    file(WRITE "${WORK}/symbols-malformed.lbl" "${changed}")
    expect_failure("suite: symbol: malformed ld65 label row" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-malformed.lbl")

    string(REPLACE "${result_row}" "al FFFF ._g_results" changed "${labels}")
    file(WRITE "${WORK}/symbols-extent-overflow.lbl" "${changed}")
    expect_failure("suite: symbol: result extent or code overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-extent-overflow.lbl")

    string(REGEX MATCH "al [0-9A-Fa-f]+ [.]?suite_model" model_row "${labels}")
    if(model_row STREQUAL "")
      set(FAILURE "symbol controls could not locate the model byte")
    else()
      string(REPLACE "${model_row}" "al ${result_address} suite_model" changed "${labels}")
      file(WRITE "${WORK}/symbols-result-overlap.lbl" "${changed}")
      expect_failure("suite: symbol: result and completion overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-result-overlap.lbl")

      string(REGEX MATCH "al ([0-9A-Fa-f]+) [.]?suite_done" done_row "${labels}")
      set(done_address "${CMAKE_MATCH_1}")
      if(done_row STREQUAL "")
        set(FAILURE "symbol controls could not locate the completion sentinel")
      else()
        string(REPLACE "${model_row}" "al ${done_address} suite_model" changed "${labels}")
        file(WRITE "${WORK}/symbols-sentinel-model-overlap.lbl" "${changed}")
        expect_failure("suite: symbol: completion symbols overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-sentinel-model-overlap.lbl")
      endif()
    endif()

    foreach(control progress model done probe_result)
      set(control_name "suite_${control}")
      string(REGEX MATCH "al ([0-9A-Fa-f]+) [.]?${control_name}" control_row "${labels}")
      if(control_row STREQUAL "")
        set(FAILURE "symbol controls could not locate ${control_name}")
      else()
        string(REPLACE "${control_row}" "al ${entry_address} ${control_name}" changed "${labels}")
        file(WRITE "${WORK}/symbols-${control}-code-alias.lbl" "${changed}")
        expect_failure("suite: symbol: control and code overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-${control}-code-alias.lbl")

        string(REPLACE "${control_row}" "al ${result_address} ${control_name}" changed "${labels}")
        file(WRITE "${WORK}/symbols-${control}-result-alias.lbl" "${changed}")
        expect_failure("suite: symbol: result and completion overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-${control}-result-alias.lbl")
      endif()
    endforeach()

    set(control_pairs
      "suite_progress|suite_model"
      "suite_progress|suite_done"
      "suite_progress|suite_probe_result"
      "suite_model|suite_done"
      "suite_model|suite_probe_result"
      "suite_done|suite_probe_result")
    foreach(pair IN LISTS control_pairs)
      string(REPLACE "|" ";" pair_parts "${pair}")
      list(GET pair_parts 0 first_control)
      list(GET pair_parts 1 second_control)
      string(REGEX MATCH "al ([0-9A-Fa-f]+) [.]?${first_control}" first_row "${labels}")
      set(first_address "${CMAKE_MATCH_1}")
      string(REGEX MATCH "al [0-9A-Fa-f]+ [.]?${second_control}" second_row "${labels}")
      if(first_row STREQUAL "" OR second_row STREQUAL "")
        set(FAILURE "symbol controls could not locate pair ${first_control}/${second_control}")
      else()
        string(REPLACE "${second_row}" "al ${first_address} ${second_control}" changed "${labels}")
        string(REPLACE "|" "-" pair_slug "${pair}")
        file(WRITE "${WORK}/symbols-${pair_slug}-alias.lbl" "${changed}")
        expect_failure("suite: symbol: completion symbols overlap" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-${pair_slug}-alias.lbl")
      endif()
    endforeach()
  endif()

  file(WRITE "${WORK}/symbols-empty.lbl" "")
  expect_failure("suite: symbol missing: suite_entry" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-empty.lbl")
endif()

if(NOT FAILURE STREQUAL "")
  message(FATAL_ERROR "suite_driver: ${FAILURE}")
endif()
cleanup_suite_run("${WORK}" "${SCRATCH_ROOT}" "${SUITE_RUN_NONCE}")
if(MODE STREQUAL "page_mode_observation")
  message("${all_run_out}")
else()
  message(STATUS "${all_run_out}")
endif()
