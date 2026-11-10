# Tier-A diagnostic tests. Every CTest driver builds its BLL and -Ln labels
# into its own scratch directory with the already-pinned ca65/ld65 binaries.
add_executable(lynx-test-diag ${CMAKE_CURRENT_LIST_DIR}/test_diag.c)
target_link_libraries(lynx-test-diag PRIVATE lynx_core)
lynx_warnings(lynx-test-diag)
lynx_sanitize(lynx-test-diag)
set_target_properties(lynx-test-diag PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

set(LYNX_DIAG_DRIVER "${PROJECT_BINARY_DIR}/tests/diag/runner_diagnostic.cmake")
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/diag")
find_program(LYNX_ENV_EXECUTABLE env REQUIRED)
file(GENERATE OUTPUT "${LYNX_DIAG_DRIVER}" CONTENT [==[
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(required MODE ROOT WORK SCRATCH_ROOT TEST_BINARY RUNNER ENV_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner_diagnostic: -D${required} is required")
  endif()
endforeach()
set(SOURCE "${ROOT}/fixtures/diagnostic/first.s")
set(CONFIG "${ROOT}/fixtures/diagnostic/bll.cfg")
set(HEX "${ROOT}/fixtures/diagnostic/first.hex")
set(CA65 "${ROOT}/build/pins/cc65/bin/ca65")
set(LD65 "${ROOT}/build/pins/cc65/bin/ld65")
if(NOT EXISTS "${CA65}" OR NOT EXISTS "${LD65}")
  message(FATAL_ERROR "runner_diagnostic: pinned cc65 tools are missing; scripts/check.sh owns pin preparation")
endif()
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK}" NORMALIZE work_is_owned)
if(NOT work_is_owned OR "${WORK}" STREQUAL "${SCRATCH_ROOT}")
  message(FATAL_ERROR "runner_diagnostic: refusing scratch path outside its isolated owned directory: ${WORK}")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(assemble_fixture name fault)
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02 -D DIAG_FAULT=${fault}
      -o "${name}.o" "${SOURCE}"
    WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE ca_rc
    OUTPUT_VARIABLE ca_out ERROR_VARIABLE ca_err)
  if(NOT ca_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: ca65 failed for ${name}\n${ca_out}${ca_err}")
  endif()
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "CC65_HOME=${ROOT}/build/pins/cc65" "${LD65}" -C "${CONFIG}" -Ln "${name}.lbl"
      -o "${name}.bll" "${name}.o"
    WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE ld_rc
    OUTPUT_VARIABLE ld_out ERROR_VARIABLE ld_err)
  if(NOT ld_rc STREQUAL "0" OR NOT EXISTS "${WORK}/${name}.bll" OR
     NOT EXISTS "${WORK}/${name}.lbl")
    message(FATAL_ERROR "runner_diagnostic: ld65 failed for ${name}\n${ld_out}${ld_err}")
  endif()
  file(REMOVE "${WORK}/${name}.o")
endfunction()

function(run_ok)
  execute_process(COMMAND "${TEST_BINARY}" ${ARGN}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: expected accepted test, got ${rc}\n${out}${err}")
  endif()
  message(STATUS "${out}${err}")
endfunction()

function(run_fail expected)
  execute_process(COMMAND "${TEST_BINARY}" ${ARGN}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(all_output "${out}${err}")
  if(NOT rc STREQUAL "1" OR NOT all_output MATCHES "${expected}")
    message(FATAL_ERROR "runner_diagnostic: expected exit 1 matching '${expected}', got ${rc}\n${all_output}")
  endif()
  message(STATUS "runner_diagnostic: rejected as intended: ${expected}")
endfunction()

if(MODE STREQUAL "header" OR MODE STREQUAL "header_self_test")
  assemble_fixture(first 0)
  run_ok(header "${WORK}/first.bll" "${WORK}/first.lbl" "${HEX}")
  if(MODE STREQUAL "header_self_test")
    execute_process(COMMAND "${TEST_BINARY}" corrupt-header "${WORK}/first.bll"
        "${WORK}/first-corrupt.bll"
      RESULT_VARIABLE corrupt_rc OUTPUT_VARIABLE corrupt_out ERROR_VARIABLE corrupt_err)
    if(NOT corrupt_rc STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: could not create bounded bad-header control\n${corrupt_out}${corrupt_err}")
    endif()
    run_fail("bll header: signature" header "${WORK}/first-corrupt.bll" "${WORK}/first.lbl" "${HEX}")
  endif()
elseif(MODE STREQUAL "symbols")
  assemble_fixture(first 0)
  run_ok(symbols "${WORK}/first.lbl")
elseif(MODE STREQUAL "controls")
  assemble_fixture(first 0)
  assemble_fixture(first-broken 1)
  assemble_fixture(first-timeout 2)
  run_ok(controls "${WORK}/first.bll" "${WORK}/first.lbl"
    "${WORK}/first-broken.bll" "${WORK}/first-broken.lbl"
    "${WORK}/first-timeout.bll" "${WORK}/first-timeout.lbl")

  set(bundle "${WORK}/runner-bundle")
  execute_process(COMMAND "${RUNNER}" --content "${WORK}/first.bll" --ticks 5000
      --out "${bundle}" --profile none --ram-policy zero
    RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_out ERROR_VARIABLE runner_err)
  if(NOT runner_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: normal runner refused diagnostic\n${runner_out}${runner_err}")
  endif()
  file(READ "${bundle}/run.manifest" manifest)
  foreach(required_line IN ITEMS "profile = none" "stop_reason = budget" "tick_budget = 5000" "ticks_consumed = 5000")
    string(FIND "${manifest}" "${required_line}\n" line_position)
    if(line_position EQUAL -1)
      message(FATAL_ERROR "runner_diagnostic: run manifest lacks '${required_line}'\n${manifest}")
    endif()
  endforeach()
  file(STRINGS "${bundle}/obs.ram.hex" ram_lines)
  list(LENGTH ram_lines ram_line_count)
  if(ram_line_count LESS 41)
    message(FATAL_ERROR "runner_diagnostic: runner RAM observation is truncated")
  endif()
  list(GET ram_lines 32 result_line)
  list(GET ram_lines 40 pixels_line)
  string(LENGTH "${result_line}" result_line_len)
  if(NOT result_line_len EQUAL 64 OR NOT result_line MATCHES "^4c584433010300a5[0]+$")
    message(FATAL_ERROR "runner_diagnostic: bundle result differs from the public-API oracle: ${result_line}")
  endif()
  if(NOT pixels_line STREQUAL "0123456723456789456789ab6789abcd89abcdefabcdef01cdef0123ef012345")
    message(FATAL_ERROR "runner_diagnostic: bundle pixels differ from the diagnostic contract: ${pixels_line}")
  endif()
  message(STATUS "runner_diagnostic: runner manifest, result and packed pixels agree with public API contract")
elseif(MODE STREQUAL "restore" OR MODE STREQUAL "restore_self_test")
  assemble_fixture(first 0)
  run_ok(restore "${WORK}/first.bll" "${WORK}/first.lbl")
  if(MODE STREQUAL "restore_self_test")
    run_fail("invalid snapshot was rejected" restore "${WORK}/first.bll" "${WORK}/first.lbl" fault-accept-forgery)
  endif()
else()
  message(FATAL_ERROR "runner_diagnostic: unknown mode '${MODE}'")
endif()
file(REMOVE_RECURSE "${WORK}")
]==])

function(lynx_diag_test name mode timeout)
  string(FIND "${name}" "." label_end)
  string(SUBSTRING "${name}" 0 ${label_end} test_label)
  lynx_add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DMODE=${mode}
      -DROOT=${PROJECT_SOURCE_DIR}
      -DWORK=${LYNX_SCRATCH_DIR}/diag/${mode}
      -DSCRATCH_ROOT=${LYNX_SCRATCH_DIR}/diag
      -DTEST_BINARY=$<TARGET_FILE:lynx-test-diag>
      -DRUNNER=$<TARGET_FILE:lynx-run>
      -DENV_EXECUTABLE=${LYNX_ENV_EXECUTABLE}
      -P "${LYNX_DIAG_DRIVER}"
    LABELS ${test_label} TIMEOUT ${timeout})
endfunction()

lynx_diag_test(fixture.bll_header header 30)
lynx_diag_test(fixture.bll_header.self_test header_self_test 30)
lynx_diag_test(diag.result_symbol symbols 30)
lynx_diag_test(diag.result_controls controls 60)
lynx_diag_test(det.diagnostic_restore restore 60)
lynx_diag_test(det.diagnostic_restore.self_test restore_self_test 60)
