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
if(NOT MODE MATCHES "^(cpu|cpu_model_controls|cpu_bound_controls|tracer|oracle_controls|symbol_controls|timer_observation|timer_strict_controls|timer_cascade_model)$")
  message(FATAL_ERROR "suite_driver: unknown mode")
endif()
if(MODE STREQUAL "cpu" AND NOT PROFILE MATCHES "^(lynx1|lynx2)$")
  message(FATAL_ERROR "suite_driver: explicit CPU PROFILE is required")
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

function(run_clean_cpu)
  expect_success("suite raw source comparison: match; suite=cpu machine_profile=lynx1 profile_input=lynx1 ram_seed=zero"
    "${TEST_BINARY}" run cpu lynx1 "${SUITE_BUILD}" "${ORACLE}" zero)
  expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx1"
    "${TEST_BINARY}" validate-cpu-record lynx1
      "${SUITE_BUILD}/raw-results-zero.tsv"
      "${SUITE_BUILD}/run-record-zero.txt" "${ORACLE}")
endfunction()

function(expect_cpu_record_failure expected raw_path record_path)
  expect_failure("${expected}" "${TEST_BINARY}" validate-cpu-record lynx1
    "${raw_path}" "${record_path}" "${ORACLE}")
  run_clean_cpu()
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
  file(READ "${SUITE_BUILD}/run-record-zero.txt" record_text)
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
  execute_process(COMMAND "${TEST_BINARY}" run cpu
    RESULT_VARIABLE missing_profile_rc OUTPUT_VARIABLE missing_profile_out ERROR_VARIABLE missing_profile_err)
  set(missing_profile_output "${missing_profile_out}${missing_profile_err}")
  if(NOT missing_profile_rc STREQUAL "2" OR
     NOT missing_profile_output MATCHES "usage: lynx-test-suites run cpu PROFILE")
    set(FAILURE "missing CPU suite profile was not rejected with the explicit usage contract")
  endif()
  if(FAILURE STREQUAL "")
    expect_failure("suite: explicit lynx1 or lynx2 profile input required"
      "${TEST_BINARY}" run cpu none "${SUITE_BUILD}" "${ORACLE}" zero)
  endif()

  if(FAILURE STREQUAL "")
    expect_success("suite raw source comparison: match; suite=cpu machine_profile=lynx2 profile_input=lynx2 ram_seed=zero"
      "${TEST_BINARY}" run cpu lynx2 "${SUITE_BUILD}" "${ORACLE}" zero)
    file(READ "${SUITE_BUILD}/run-record-zero.txt" lynx2_record)
    if(NOT lynx2_record MATCHES "detected_model=lynx2" OR
       NOT lynx2_record MATCHES "model_probe_bytes=00,a2" OR
       NOT lynx2_record MATCHES "source_tuple_model=lynx2" OR
       NOT lynx2_record MATCHES "meaningful_count=18")
      set(FAILURE "real Lynx II execution record did not retain its independent model evidence")
    else()
      expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx2"
        "${TEST_BINARY}" validate-cpu-record lynx2
          "${SUITE_BUILD}/raw-results-zero.tsv"
          "${SUITE_BUILD}/run-record-zero.txt" "${ORACLE}")
    endif()

  endif()

  if(FAILURE STREQUAL "")
    run_clean_cpu()
    expect_failure("suite: detected model does not match selected profile input"
      "${TEST_BINARY}" run cpu-selected lynx1 lynx2 "${SUITE_BUILD}" "${ORACLE}" zero)
    run_clean_cpu()
    file(READ "${SUITE_BUILD}/run-record-zero.txt" base_record)
    file(READ "${SUITE_BUILD}/raw-results-zero.tsv" base_raw)

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

    file(STRINGS "${ORACLE}" tuple_rows REGEX "^tuple\t")
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
      file(STRINGS "${SUITE_BUILD}/raw-results-zero.tsv" raw_lines)
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
      string(APPEND all_run_out "model controls: omitted/none profile parser rejection; one opposite-selection actual guest execution; missing/corrupt probe, tuple disagreement, count drift and zero-block captured-record parser controls\n")
    endif()
  endif()
endif()

if(MODE STREQUAL "cpu_bound_controls" AND FAILURE STREQUAL "")
  run_clean_cpu()
  file(READ "${SUITE_BUILD}/run-record-zero.txt" base_record)
  file(READ "${SUITE_BUILD}/raw-results-zero.tsv" base_raw)
  file(COPY_FILE "${SUITE_BUILD}/raw-results-zero.tsv" "${WORK}/cpu-bound-positive.raw")
  set(bound_raw "${WORK}/cpu-bound-positive.raw")

  write_ticks_record("${WORK}/cpu-minimum.record" 256)
  expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx1"
    "${TEST_BINARY}" validate-cpu-record lynx1 "${bound_raw}" "${WORK}/cpu-minimum.record" "${ORACLE}")
  run_clean_cpu()

  write_ticks_record("${WORK}/cpu-maximum.record" 50000000)
  expect_success("cpu record: 18 raw results, completion and model evidence valid for lynx1"
    "${TEST_BINARY}" validate-cpu-record lynx1 "${bound_raw}" "${WORK}/cpu-maximum.record" "${ORACLE}")
  run_clean_cpu()

  write_ticks_record("${WORK}/cpu-below-minimum.record" 255)
  expect_cpu_record_failure("cpu record: ticks outside completion bounds" "${bound_raw}" "${WORK}/cpu-below-minimum.record")

  write_ticks_record("${WORK}/cpu-above-maximum.record" 50000001)
  expect_cpu_record_failure("cpu record: ticks outside completion bounds" "${bound_raw}" "${WORK}/cpu-above-maximum.record")

  write_ticks_record("${WORK}/cpu-time-overflow.record" 4294967296)
  expect_cpu_record_failure("cpu record: invalid ticks_observed" "${bound_raw}" "${WORK}/cpu-time-overflow.record")
  string(APPEND all_run_out "bound controls: exact 256/50000000 captured-record boundaries pass; 255/50000001 and uint32 overflow fail the named time validator\n")
endif()

if(MODE STREQUAL "timer_observation" AND FAILURE STREQUAL "")
  run_timer_observation("${PROFILE}")
  string(APPEND all_run_out "timer observation: profile=${PROFILE}, 16 source rows retained with 15 agreements and the exact pending index14 row; independent proof and acyclic companion validated\n")
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
message(STATUS "${all_run_out}")
