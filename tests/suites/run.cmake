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
if(NOT MODE STREQUAL "cpu" AND NOT MODE STREQUAL "tracer")
  message(FATAL_ERROR "suite_driver: unknown mode")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/tmp" "${WORK}/build")
set(SUITE_BUILD "${WORK}/build")
set(ORACLE "${WORK}/build/oracle.tsv")
set(FAILURE "")

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

file(REMOVE_RECURSE "${WORK}")
if(NOT FAILURE STREQUAL "")
  message(FATAL_ERROR "suite_driver: ${FAILURE}")
endif()
message(STATUS "${run_out}")
