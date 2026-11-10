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
if(MODE STREQUAL "consumer")
  foreach(required CASE PROFILE CONSUMER TEST_CONSUMER ARTIFACT_CHECKER C_COMPILER PUBLIC_INCLUDE_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
      message(FATAL_ERROR "runner_diagnostic: consumer mode requires -D${required}")
    endif()
  endforeach()
endif()
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

function(consumer_success profile)
  set(output "${WORK}/consumer-output")
  set(arguments --content "${WORK}/first.bll" --symbols "${WORK}/first.lbl" --out "${output}")
  if(NOT profile STREQUAL "default")
    list(APPEND arguments --profile "${profile}")
  endif()
  execute_process(COMMAND "${CONSUMER}" ${arguments}
    RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
  if(NOT consumer_rc STREQUAL "0" OR NOT consumer_out MATCHES "SNAPSHOT P3" OR
     NOT consumer_out MATCHES "placeholder silence")
    message(FATAL_ERROR "runner_diagnostic: public consumer failed or omitted the placeholder-audio label (${consumer_rc})\n${consumer_out}${consumer_err}")
  endif()
  execute_process(COMMAND "${ARTIFACT_CHECKER}" "${output}"
    RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
  if(NOT check_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: independent artifact parser rejected consumer output\n${check_out}${check_err}")
  endif()
  message(STATUS "runner_diagnostic: public consumer and independent P3/WAV parser pass for ${profile}")
endfunction()

function(consumer_io_failure kind)
  set(output "${WORK}/out-injected-${kind}")
  execute_process(COMMAND "${ENV_EXECUTABLE}" "LYNX_TEST_IO_FAILURE=${kind}"
      "${TEST_CONSUMER}" --content "${WORK}/first.bll" --symbols "${WORK}/first.lbl"
      --out "${output}" --profile none
    RESULT_VARIABLE io_rc OUTPUT_VARIABLE io_out ERROR_VARIABLE io_err)
  if(NOT io_rc STREQUAL "1" OR NOT io_err MATCHES "cannot write snapshot.ppm exclusively" OR
     NOT io_out STREQUAL "")
    message(FATAL_ERROR "runner_diagnostic: injected ${kind} failure was not refused before publication (${io_rc})\n${io_out}${io_err}")
  endif()
  if(EXISTS "${output}")
    message(FATAL_ERROR "runner_diagnostic: injected ${kind} failure left a final output path")
  endif()
  file(GLOB stage_paths "${output}.lynxplosion-stage-*")
  if(stage_paths)
    message(FATAL_ERROR "runner_diagnostic: injected ${kind} failure left its private staging directory")
  endif()
  message(STATUS "runner_diagnostic: injected ${kind} failure refused and cleaned only its owned staging path")
endfunction()

function(public_boundary_reject name source expected)
  set(probe "${WORK}/${name}.c")
  file(WRITE "${probe}" "${source}")
  execute_process(COMMAND "${C_COMPILER}" -std=c17 -I "${PUBLIC_INCLUDE_DIR}"
      -fsyntax-only "${probe}"
    RESULT_VARIABLE probe_rc OUTPUT_VARIABLE probe_out ERROR_VARIABLE probe_err)
  set(all_output "${probe_out}${probe_err}")
  if(probe_rc STREQUAL "0" OR NOT all_output MATCHES "${expected}")
    message(FATAL_ERROR "runner_diagnostic: public-only boundary accepted ${name} or failed for an unrelated reason\n${all_output}")
  endif()
  message(STATUS "runner_diagnostic: public-only boundary rejects ${name} as intended")
endfunction()

function(consumer_refusal name expected content symbols output)
  execute_process(COMMAND "${CONSUMER}" --content "${content}" --symbols "${symbols}"
      --out "${output}" --profile none
    RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
  if(NOT consumer_rc STREQUAL "1" OR NOT "${consumer_err}" MATCHES "${expected}" OR
     NOT consumer_out STREQUAL "")
    message(FATAL_ERROR "runner_diagnostic: consumer ${name} refusal expected '${expected}', got ${consumer_rc}\n${consumer_out}${consumer_err}")
  endif()
  if(EXISTS "${output}")
    message(FATAL_ERROR "runner_diagnostic: refused consumer ${name} created output")
  endif()
  message(STATUS "runner_diagnostic: consumer refused ${name}: ${expected}")
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
    file(READ "${HEX}" source_hex)
    string(REPLACE "80 08" "81 08" wrong_source_hex "${source_hex}")
    if(wrong_source_hex STREQUAL source_hex)
      message(FATAL_ERROR "runner_diagnostic: could not seed one-byte source identity disagreement")
    endif()
    file(WRITE "${WORK}/first-wrong-source.hex" "${wrong_source_hex}")
    run_fail("text hex identity differs from assembled BLL" header "${WORK}/first.bll"
      "${WORK}/first.lbl" "${WORK}/first-wrong-source.hex")
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
  execute_process(COMMAND "${TEST_BINARY}" corrupt-map "${WORK}/first.bll" "${HEX}"
      "${WORK}/wrong-map.bll"
    RESULT_VARIABLE wrong_map_seed_rc OUTPUT_VARIABLE wrong_map_seed_out
    ERROR_VARIABLE wrong_map_seed_err)
  if(NOT wrong_map_seed_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: could not create wrong-map control\n${wrong_map_seed_out}${wrong_map_seed_err}")
  endif()

  set(bundle "${WORK}/runner-bundle")
  execute_process(COMMAND "${RUNNER}" --content "${WORK}/first.bll" --ticks 5000
      --out "${bundle}" --profile none --ram-policy zero
    RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_out ERROR_VARIABLE runner_err)
  if(NOT runner_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: normal runner refused diagnostic\n${runner_out}${runner_err}")
  endif()
  run_ok(bundle "${WORK}/first.bll" "${WORK}/first.lbl" "${bundle}" pass)

  set(wrong_map_bundle "${WORK}/runner-wrong-map")
  execute_process(COMMAND "${RUNNER}" --content "${WORK}/wrong-map.bll" --ticks 5000
      --out "${wrong_map_bundle}" --profile none --ram-policy zero
    RESULT_VARIABLE wrong_map_rc OUTPUT_VARIABLE wrong_map_out ERROR_VARIABLE wrong_map_err)
  if(NOT wrong_map_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: wrong-map diagnostic did not produce inspectable evidence\n${wrong_map_out}${wrong_map_err}")
  endif()
  run_ok(bundle "${WORK}/wrong-map.bll" "${WORK}/first.lbl" "${wrong_map_bundle}" map-failure)

  set(broken_bundle "${WORK}/runner-broken-guest")
  execute_process(COMMAND "${RUNNER}" --content "${WORK}/first-broken.bll" --ticks 5000
      --out "${broken_bundle}" --profile none --ram-policy zero
    RESULT_VARIABLE broken_rc OUTPUT_VARIABLE broken_out ERROR_VARIABLE broken_err)
  if(NOT broken_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: broken guest did not produce inspectable evidence\n${broken_out}${broken_err}")
  endif()
  run_ok(bundle "${WORK}/first-broken.bll" "${WORK}/first-broken.lbl"
    "${broken_bundle}" guest-failure)
  message(STATUS "runner_diagnostic: runner agreement includes accepted, wrong-map and guest-error controls")
elseif(MODE STREQUAL "consumer")
  assemble_fixture(first 0)
  if(CASE STREQUAL "public_api")
    consumer_success("default")
    execute_process(COMMAND "${ARTIFACT_CHECKER}" self-test
      RESULT_VARIABLE parser_rc OUTPUT_VARIABLE parser_out ERROR_VARIABLE parser_err)
    if(NOT parser_rc STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: independent artifact parser controls failed\n${parser_out}${parser_err}")
    endif()
    public_boundary_reject(private_include
      "#include \"internal.h\"\nint main(void) { return 0; }\n" "internal.h")
    public_boundary_reject(private_symbol
      "#include <lynxplosion.h>\nint main(void) { lynx__power_on(0); return 0; }\n"
      "lynx__power_on")
  elseif(CASE STREQUAL "snapshot_ppm")
    consumer_success("${PROFILE}")
  elseif(CASE STREQUAL "silence_wav")
    consumer_success("${PROFILE}")
  elseif(CASE STREQUAL "refusal_controls")
    assemble_fixture(first-broken 1)
    assemble_fixture(first-timeout 2)
    execute_process(COMMAND "${TEST_BINARY}" corrupt-pixels "${WORK}/first.bll" "${HEX}"
        "${WORK}/bad-pixels.bll"
      RESULT_VARIABLE pixel_seed_rc OUTPUT_VARIABLE pixel_seed_out ERROR_VARIABLE pixel_seed_err)
    if(NOT pixel_seed_rc STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: could not seed bad-pixels consumer control\n${pixel_seed_out}${pixel_seed_err}")
    endif()
    execute_process(COMMAND "${TEST_BINARY}" corrupt-header "${WORK}/first.bll"
        "${WORK}/bad-header.bll"
      RESULT_VARIABLE header_seed_rc OUTPUT_VARIABLE header_seed_out ERROR_VARIABLE header_seed_err)
    if(NOT header_seed_rc STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: could not seed malformed-BLL consumer control\n${header_seed_out}${header_seed_err}")
    endif()
    file(READ "${WORK}/first.lbl" labels_text)
    string(REPLACE ".diag_result" ".diag_result_missing" missing_symbol_text "${labels_text}")
    file(WRITE "${WORK}/missing-symbol.lbl" "${missing_symbol_text}")
    consumer_refusal("missing content" "cannot open BLL content" "${WORK}/missing.bll"
      "${WORK}/first.lbl" "${WORK}/out-missing-content")
    consumer_refusal("missing symbol" "missing diag_result" "${WORK}/first.bll"
      "${WORK}/missing-symbol.lbl" "${WORK}/out-missing-symbol")
    consumer_refusal("malformed BLL" "strict BS93 load refused" "${WORK}/bad-header.bll"
      "${WORK}/first.lbl" "${WORK}/out-malformed-bll")
    consumer_refusal("guest error" "guest result is incomplete or reports an error"
      "${WORK}/first-broken.bll" "${WORK}/first-broken.lbl" "${WORK}/out-guest-error")
    consumer_refusal("incomplete result" "guest result is incomplete or reports an error"
      "${WORK}/first-timeout.bll" "${WORK}/first-timeout.lbl" "${WORK}/out-incomplete")
    consumer_refusal("bad pixels" "guest packed pixels differ from the independent 8x8 contract"
      "${WORK}/bad-pixels.bll" "${WORK}/first.lbl" "${WORK}/out-bad-pixels")
    file(MAKE_DIRECTORY "${WORK}/existing-output")
    file(WRITE "${WORK}/existing-output/owner.marker" "preserve me\n")
    execute_process(COMMAND "${CONSUMER}" --content "${WORK}/first.bll"
        --symbols "${WORK}/first.lbl" --out "${WORK}/existing-output"
      RESULT_VARIABLE existing_rc OUTPUT_VARIABLE existing_out ERROR_VARIABLE existing_err)
    if(NOT existing_rc STREQUAL "1" OR NOT existing_err MATCHES "output directory already exists")
      message(FATAL_ERROR "runner_diagnostic: consumer did not refuse existing output directory\n${existing_out}${existing_err}")
    endif()
    file(READ "${WORK}/existing-output/owner.marker" preserved_marker)
    if(NOT preserved_marker STREQUAL "preserve me\n")
      message(FATAL_ERROR "runner_diagnostic: refusal changed an existing user file")
    endif()
    file(WRITE "${WORK}/existing-file" "preserve regular file\n")
    execute_process(COMMAND "${CONSUMER}" --content "${WORK}/first.bll"
        --symbols "${WORK}/first.lbl" --out "${WORK}/existing-file"
      RESULT_VARIABLE file_rc OUTPUT_VARIABLE file_out ERROR_VARIABLE file_err)
    if(NOT file_rc STREQUAL "1" OR NOT file_err MATCHES "output directory already exists")
      message(FATAL_ERROR "runner_diagnostic: consumer did not refuse an existing output file\n${file_out}${file_err}")
    endif()
    file(READ "${WORK}/existing-file" preserved_file)
    if(NOT preserved_file STREQUAL "preserve regular file\n")
      message(FATAL_ERROR "runner_diagnostic: refusal changed an existing output file")
    endif()
    file(MAKE_DIRECTORY "${WORK}/symlink-target")
    file(WRITE "${WORK}/symlink-target/owner.marker" "preserve symlink target\n")
    file(CREATE_LINK "${WORK}/symlink-target" "${WORK}/existing-symlink" SYMBOLIC RESULT link_result)
    if(NOT link_result STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: could not prepare isolated symlink destination: ${link_result}")
    endif()
    execute_process(COMMAND "${CONSUMER}" --content "${WORK}/first.bll"
        --symbols "${WORK}/first.lbl" --out "${WORK}/existing-symlink"
      RESULT_VARIABLE link_rc OUTPUT_VARIABLE link_out ERROR_VARIABLE link_err)
    if(NOT link_rc STREQUAL "1" OR NOT link_err MATCHES "output directory already exists")
      message(FATAL_ERROR "runner_diagnostic: consumer did not refuse an existing output symlink\n${link_out}${link_err}")
    endif()
    file(READ "${WORK}/symlink-target/owner.marker" preserved_target)
    file(READ_SYMLINK "${WORK}/existing-symlink" preserved_link)
    if(NOT preserved_target STREQUAL "preserve symlink target\n" OR
       NOT preserved_link STREQUAL "${WORK}/symlink-target")
      message(FATAL_ERROR "runner_diagnostic: refusal followed or changed an existing output symlink")
    endif()
    consumer_io_failure(write)
    consumer_io_failure(close)
    execute_process(COMMAND "${ENV_EXECUTABLE}" -u LYNX_TEST_IO_FAILURE
        "${ARTIFACT_CHECKER}" publication "${TEST_CONSUMER}"
        "${WORK}/first.bll" "${WORK}/first.lbl" "${WORK}/concurrent-output"
      RESULT_VARIABLE publish_rc OUTPUT_VARIABLE publish_out ERROR_VARIABLE publish_err)
    if(NOT publish_rc STREQUAL "0")
      message(FATAL_ERROR "runner_diagnostic: atomic concurrent publication controls failed\n${publish_out}${publish_err}")
    endif()
    message(STATUS "runner_diagnostic: consumer refusal controls passed without output or overwrite")
  else()
    message(FATAL_ERROR "runner_diagnostic: unknown consumer CASE '${CASE}'")
  endif()
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
