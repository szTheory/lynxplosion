cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(required MODE ROOT WORK SCRATCH_ROOT TEST_BINARY ENV_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "suite_driver: -D${required} is required")
  endif()
endforeach()
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK}" NORMALIZE work_is_owned)
if(NOT work_is_owned OR "${WORK}" STREQUAL "${SCRATCH_ROOT}")
  message(FATAL_ERROR "suite_driver: refusing output outside invocation-owned scratch")
endif()
if(NOT MODE MATCHES "^(cpu|tracer|oracle_controls|symbol_controls)$")
  message(FATAL_ERROR "suite_driver: unknown mode")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/tmp" "${WORK}/build")
set(SUITE_BUILD "${WORK}/build")
set(ORACLE "${WORK}/build/oracle.tsv")
set(FAILURE "")

function(expect_failure expected)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(combined "${out}${err}")
  string(FIND "${combined}" "${expected}" found)
  if(NOT "${rc}" STREQUAL "1" OR found EQUAL -1)
    message(FATAL_ERROR "suite_driver: control expected exit 1 and exact diagnostic '${expected}', got ${rc}: ${combined}")
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

if(FAILURE STREQUAL "")
  execute_process(
    COMMAND "${TEST_BINARY}" run cpu lynx1 "${SUITE_BUILD}" "${ORACLE}"
    TIMEOUT 45 RESULT_VARIABLE run_rc OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
  if(NOT run_rc STREQUAL "0" OR
     NOT run_out MATCHES "suite raw source comparison: match; suite=cpu profile_input=lynx1 ticks=[0-9]+ results=18" OR
     NOT run_out MATCHES "hardware_accuracy=unverified profile_qualification=unresolved")
    set(FAILURE "bounded CPU guest did not produce source comparison evidence: ${run_rc} ${run_out} ${run_err}")
  endif()
endif()

if(FAILURE STREQUAL "")
  file(READ "${SUITE_BUILD}/run-record.txt" run_record)
  file(READ "${SUITE_BUILD}/raw-results.tsv" raw_results)
  file(READ "${SUITE_BUILD}/comparison.tsv" comparison)
  if(NOT run_record MATCHES "labels_sha256=[0-9a-f][0-9a-f]+" OR
     NOT run_record MATCHES "oracle_sha256=[0-9a-f][0-9a-f]+" OR
     NOT run_record MATCHES "guest_progress=8" OR
     NOT raw_results MATCHES "result\t17\t00" OR
     NOT comparison MATCHES "comparison\tmatch")
    set(FAILURE "raw, comparison, or source-bound run record is incomplete")
  endif()
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

  file(READ "${ORACLE}" oracle_text)
  file(WRITE "${WORK}/oracle-empty.tsv" "")
  expect_failure("suite: oracle: invalid version header" "${TEST_BINARY}" validate-oracle "${WORK}/oracle-empty.tsv")

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
  string(REGEX MATCH "al [0-9A-Fa-f]+ [.]?suite_entry" entry_row "${labels}")
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
  endif()

  file(WRITE "${WORK}/symbols-empty.lbl" "")
  expect_failure("suite: symbol missing: suite_entry" "${TEST_BINARY}" validate-symbols "${WORK}/symbols-empty.lbl")
endif()

file(REMOVE_RECURSE "${WORK}")
if(NOT FAILURE STREQUAL "")
  message(FATAL_ERROR "suite_driver: ${FAILURE}")
endif()
message(STATUS "${run_out}")
