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
    TIMEOUT 30
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: expected accepted test, got ${rc}\n${out}${err}")
  endif()
  message(STATUS "${out}${err}")
endfunction()

function(run_fail expected)
  execute_process(COMMAND "${TEST_BINARY}" ${ARGN}
    TIMEOUT 30
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(all_output "${out}${err}")
  if(NOT rc STREQUAL "1" OR NOT all_output MATCHES "${expected}")
    message(FATAL_ERROR "runner_diagnostic: expected exit 1 matching '${expected}', got ${rc}\n${all_output}")
  endif()
  message(STATUS "runner_diagnostic: rejected as intended: ${expected}")
endfunction()

function(suite_positive root output)
  file(REMOVE_RECURSE "${output}")
  file(MAKE_DIRECTORY "${WORK}/tmp")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "TMPDIR=${WORK}/tmp" sh "${root}/scripts/build-suites.sh" --out "${output}"
    TIMEOUT 20 RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
  if(NOT build_rc STREQUAL "0" OR
     NOT build_out MATCHES "lynx_tests_pin=a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e")
    message(FATAL_ERROR "runner_diagnostic: fresh positive suite build failed (${build_rc})\n${build_out}${build_err}")
  endif()
  run_ok(suite_build "${output}" cpu lynx1)
endfunction()

function(suite_builder_refusal root output expected)
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "TMPDIR=${WORK}/tmp" sh "${root}/scripts/build-suites.sh" --out "${output}"
    TIMEOUT 20 RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
  set(all_output "${build_out}${build_err}")
  if(NOT build_rc STREQUAL "1" OR NOT all_output MATCHES "${expected}")
    message(FATAL_ERROR "runner_diagnostic: suite builder expected exit 1 matching '${expected}', got ${build_rc}\n${all_output}")
  endif()
  message(STATUS "runner_diagnostic: suite builder rejected as intended: ${expected}")
endfunction()

function(seed_suite_root name)
  set(dst "${WORK}/roots/${name}")
  file(MAKE_DIRECTORY
    "${dst}/scripts"
    "${dst}/fixtures/suites"
    "${dst}/build/pins/cc65/bin"
    "${dst}/build/pins/lynx-tests/cpu"
    "${dst}/build/pins/lynx-tests/timers"
    "${dst}/build/pins/lynx-tests/audio"
    "${dst}/build/pins/lynx-tests/audio2"
    "${dst}/build/check/scratch")
  file(COPY_FILE "${ROOT}/scripts/build-suites.sh" "${dst}/scripts/build-suites.sh")
  file(COPY_FILE "${ROOT}/pins.txt" "${dst}/pins.txt")
  foreach(file cpu-wrapper.s timers-wrapper.s audio-wrapper.s audio2-wrapper.s bll.cfg)
    file(COPY_FILE "${ROOT}/fixtures/suites/${file}" "${dst}/fixtures/suites/${file}")
  endforeach()
  foreach(file .lynx-pin .lynx-pin-files LICENSE)
    file(COPY_FILE "${ROOT}/build/pins/lynx-tests/${file}" "${dst}/build/pins/lynx-tests/${file}")
  endforeach()
  foreach(file
      cpu/tests.s timers/tests.s audio/tests.s audio2/tests.s
      cpu/main.c timers/main.c audio/main.c audio2/main.c)
    file(COPY_FILE "${ROOT}/build/pins/lynx-tests/${file}" "${dst}/build/pins/lynx-tests/${file}")
  endforeach()
  foreach(file .lynx-pin .lynx-build .lynx-build-files)
    file(COPY_FILE "${ROOT}/build/pins/cc65/${file}" "${dst}/build/pins/cc65/${file}")
  endforeach()
  file(CREATE_LINK "${ROOT}/build/pins/cc65/bin/ca65"
    "${dst}/build/pins/cc65/bin/ca65" SYMBOLIC RESULT ca65_link)
  file(CREATE_LINK "${ROOT}/build/pins/cc65/bin/ld65"
    "${dst}/build/pins/cc65/bin/ld65" SYMBOLIC RESULT ld65_link)
  file(CREATE_LINK "${ROOT}/build/pins/cc65/asminc"
    "${dst}/build/pins/cc65/asminc" SYMBOLIC RESULT include_link)
  if(NOT ca65_link STREQUAL "0" OR NOT ld65_link STREQUAL "0" OR
     NOT include_link STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: could not seed isolated pinned tool links")
  endif()
  set(SUITE_ROOT "${dst}" PARENT_SCOPE)
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
  execute_process(COMMAND "${C_COMPILER}" -std=c17 -Werror=implicit-function-declaration
      -I "${PUBLIC_INCLUDE_DIR}"
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

if(MODE STREQUAL "suite_build")
  set(SUITES_OUT "${WORK}/suites")
  file(MAKE_DIRECTORY "${WORK}/tmp")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "TMPDIR=${WORK}/tmp" sh "${ROOT}/scripts/build-suites.sh" --out "${SUITES_OUT}"
    RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
  if(NOT build_rc STREQUAL "0" OR
     NOT build_out MATCHES "lynx_tests_pin=a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e")
    message(FATAL_ERROR "runner_diagnostic: pinned suite source build failed (${build_rc})\n${build_out}${build_err}")
  endif()
  message(STATUS "runner_diagnostic: source build and untouched source identity pass\n${build_out}")
  run_ok(suite_build "${SUITES_OUT}" cpu lynx1)
  run_ok(suite_build "${SUITES_OUT}" cpu lynx2)
  run_ok(suite_build "${SUITES_OUT}" timers lynx1)
  run_ok(suite_build "${SUITES_OUT}" timers lynx2)
elseif(MODE STREQUAL "suite_build_controls")
  file(MAKE_DIRECTORY "${WORK}/tmp" "${WORK}/roots")
  set(SUITES_OUT "${WORK}/suites")
  suite_positive("${ROOT}" "${SUITES_OUT}")

  # Missing label and malformed BLL length are parser-level refusals against a
  # freshly built positive object, with the target diagnostic asserted exactly.
  file(READ "${SUITES_OUT}/cpu.lbl" cpu_labels)
  string(REPLACE ".suite_entry" ".suite_entry_missing" bad_labels "${cpu_labels}")
  if(bad_labels STREQUAL cpu_labels)
    message(FATAL_ERROR "runner_diagnostic: could not seed missing suite label")
  endif()
  file(WRITE "${SUITES_OUT}/cpu.lbl" "${bad_labels}")
  run_fail("suite symbol missing or duplicate: suite_entry" suite_build
    "${SUITES_OUT}" cpu lynx1)
  suite_positive("${ROOT}" "${SUITES_OUT}")

  execute_process(COMMAND "${TEST_BINARY}" corrupt-suite-length
      "${SUITES_OUT}/cpu.bll" "${WORK}/bad-suite-length.bll"
    RESULT_VARIABLE length_seed_rc OUTPUT_VARIABLE length_seed_out
    ERROR_VARIABLE length_seed_err)
  if(NOT length_seed_rc STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: could not seed malformed suite length\n${length_seed_out}${length_seed_err}")
  endif()
  file(COPY_FILE "${WORK}/bad-suite-length.bll" "${SUITES_OUT}/cpu.bll")
  run_fail("bll header: declared length differs from object length" suite_build
    "${SUITES_OUT}" cpu lynx1)
  suite_positive("${ROOT}" "${SUITES_OUT}")

  # Each build-input mutation is isolated under this test's owned work tree.
  seed_suite_root(missing-tool)
  set(fake_out "${SUITE_ROOT}/build/check/scratch/positive")
  suite_positive("${SUITE_ROOT}" "${fake_out}")
  file(REMOVE "${SUITE_ROOT}/build/pins/cc65/bin/ca65")
  suite_builder_refusal("${SUITE_ROOT}" "${SUITE_ROOT}/build/check/scratch/missing-tool"
    "pinned ca65/ld65 are missing")
  file(CREATE_LINK "${ROOT}/build/pins/cc65/bin/ca65"
    "${SUITE_ROOT}/build/pins/cc65/bin/ca65" SYMBOLIC RESULT ca65_restore)
  if(NOT ca65_restore STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: could not restore isolated ca65 link")
  endif()
  suite_positive("${SUITE_ROOT}" "${fake_out}")

  seed_suite_root(missing-source)
  set(fake_out "${SUITE_ROOT}/build/check/scratch/positive")
  suite_positive("${SUITE_ROOT}" "${fake_out}")
  file(REMOVE "${SUITE_ROOT}/build/pins/lynx-tests/cpu/tests.s")
  suite_builder_refusal("${SUITE_ROOT}" "${SUITE_ROOT}/build/check/scratch/missing-source"
    "pinned source missing: cpu/tests.s")
  file(COPY_FILE "${ROOT}/build/pins/lynx-tests/cpu/tests.s"
    "${SUITE_ROOT}/build/pins/lynx-tests/cpu/tests.s")
  suite_positive("${SUITE_ROOT}" "${fake_out}")

  seed_suite_root(changed-source)
  set(fake_out "${SUITE_ROOT}/build/check/scratch/positive")
  suite_positive("${SUITE_ROOT}" "${fake_out}")
  file(APPEND "${SUITE_ROOT}/build/pins/lynx-tests/cpu/tests.s" "\n; changed identity control\n")
  suite_builder_refusal("${SUITE_ROOT}" "${SUITE_ROOT}/build/check/scratch/changed-source"
    "pinned source hash mismatch: cpu/tests.s")
  file(COPY_FILE "${ROOT}/build/pins/lynx-tests/cpu/tests.s"
    "${SUITE_ROOT}/build/pins/lynx-tests/cpu/tests.s")
  suite_positive("${SUITE_ROOT}" "${fake_out}")

  suite_positive("${ROOT}" "${SUITES_OUT}")
  file(MAKE_DIRECTORY "${WORK}/outside-target")
  file(WRITE "${WORK}/outside-target/owner.marker" "preserve me\n")
  file(CREATE_LINK "${WORK}/outside-target" "${WORK}/unsafe-output" SYMBOLIC RESULT unsafe_link)
  if(NOT unsafe_link STREQUAL "0")
    message(FATAL_ERROR "runner_diagnostic: could not create bounded symlink output control")
  endif()
  suite_builder_refusal("${ROOT}" "${WORK}/unsafe-output" "refusing symlink output directory")
  file(READ "${WORK}/outside-target/owner.marker" owner_marker)
  if(NOT owner_marker STREQUAL "preserve me\n" OR NOT IS_SYMLINK "${WORK}/unsafe-output")
    message(FATAL_ERROR "runner_diagnostic: unsafe output refusal changed its target or link")
  endif()
  file(REMOVE "${WORK}/unsafe-output")
  suite_positive("${ROOT}" "${SUITES_OUT}")
  message(STATUS "suite_build_controls: verified fresh positives around six exact build/input refusals")
elseif(MODE STREQUAL "header" OR MODE STREQUAL "header_self_test")
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
    message(STATUS "${publish_out}${publish_err}")
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
lynx_diag_test(suite.build suite_build 60)
lynx_diag_test(suite.build_controls suite_build_controls 90)
