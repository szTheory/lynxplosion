cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(required ROOT WORK CASE DET)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "det.av_driver: -D${required} is required")
  endif()
endforeach()
if(NOT CASE STREQUAL "audio_paths" AND NOT CASE STREQUAL "audio_sensitivity")
  message(FATAL_ERROR "det.av_driver: unsupported case '${CASE}'")
endif()

file(REAL_PATH "${ROOT}" canonical_root)
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." expected_root)
if(NOT canonical_root STREQUAL expected_root)
  message(FATAL_ERROR "det.av_driver: refusing unexpected project root '${ROOT}'")
endif()
set(expected_work "${canonical_root}/build/check/scratch/det")
set(work_input "${WORK}")
cmake_path(NORMAL_PATH work_input OUTPUT_VARIABLE work_normal)
if(NOT work_normal STREQUAL expected_work)
  message(FATAL_ERROR "det.av_driver: refusing scratch outside its owned directory: ${WORK}")
endif()
file(MAKE_DIRECTORY "${expected_work}")
file(REAL_PATH "${WORK}" work_real)
if(NOT work_real STREQUAL expected_work)
  message(FATAL_ERROR "det.av_driver: refusing redirected scratch directory: ${WORK}")
endif()

set(source "${canonical_root}/fixtures/diagnostic/av_probe.s")
set(config "${canonical_root}/fixtures/diagnostic/av_probe.cfg")
set(manifest "${canonical_root}/fixtures/manifests/av-probe.manifest")
set(cc65_home "${canonical_root}/build/pins/cc65")
set(ca65 "${cc65_home}/bin/ca65")
set(ld65 "${cc65_home}/bin/ld65")
set(tool_files "${cc65_home}/.lynx-build-files")
if(NOT EXISTS "${ca65}" OR NOT EXISTS "${ld65}" OR NOT EXISTS "${tool_files}" OR
   NOT EXISTS "${source}" OR NOT EXISTS "${config}" OR NOT EXISTS "${manifest}")
  message(FATAL_ERROR "det.av_driver: pinned tools or A/V fixture files are missing; scripts/check.sh owns pin preparation")
endif()

# Bind both source inputs and the assembler/linker bytes to the existing pin
# records before executing them. The resulting BLL and symbol are checked by
# av_fixture_load in lynx-test-det against the same fixture manifest.
file(STRINGS "${canonical_root}/pins.txt" pin_rows REGEX "^cc65 ")
list(LENGTH pin_rows pin_count)
if(NOT pin_count EQUAL 1)
  message(FATAL_ERROR "det.av_driver: cc65 source pin is missing or ambiguous")
endif()
list(GET pin_rows 0 pin_row)
string(REGEX REPLACE "^[^ ]+ +[^ ]+ +[^ ]+ +([^ ]+)$" "\\1" expected_pin "${pin_row}")
file(READ "${cc65_home}/.lynx-pin" pin_marker)
string(STRIP "${pin_marker}" pin_marker)
file(STRINGS "${cc65_home}/.lynx-build" build_pin REGEX "^pin_sha256 = ")
file(STRINGS "${cc65_home}/.lynx-build" build_recipe REGEX "^recipe = ")
file(STRINGS "${tool_files}" tools_pin REGEX "^# pin_sha256 = ")
file(STRINGS "${tool_files}" tools_recipe REGEX "^# recipe = ")
if(NOT pin_marker STREQUAL expected_pin OR
   NOT build_pin STREQUAL "pin_sha256 = ${expected_pin}" OR
   NOT build_recipe STREQUAL "recipe = 1" OR
   NOT tools_pin STREQUAL "# pin_sha256 = ${expected_pin}" OR
   NOT tools_recipe STREQUAL "# recipe = 1")
  message(FATAL_ERROR "det.av_driver: cc65 build record does not match the pinned source and recipe")
endif()
file(SHA256 "${ca65}" ca65_sha)
file(SHA256 "${ld65}" ld65_sha)
file(STRINGS "${tool_files}" ca65_record REGEX "^[0-9a-f]+ bin/ca65$")
file(STRINGS "${tool_files}" ld65_record REGEX "^[0-9a-f]+ bin/ld65$")
if(NOT ca65_record STREQUAL "${ca65_sha} bin/ca65" OR
   NOT ld65_record STREQUAL "${ld65_sha} bin/ld65")
  message(FATAL_ERROR "det.av_driver: pinned assembler/linker bytes differ from their build record")
endif()
file(SHA256 "${source}" source_sha)
file(SHA256 "${config}" config_sha)
file(STRINGS "${manifest}" manifest_source REGEX "^sha256 = ")
file(STRINGS "${manifest}" manifest_config REGEX "^av_config_sha256 = ")
if(NOT manifest_source STREQUAL "sha256 = ${source_sha}" OR
   NOT manifest_config STREQUAL "av_config_sha256 = ${config_sha}")
  message(FATAL_ERROR "det.av_driver: source/config disagree with the A/V fixture manifest")
endif()

string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef nonce)
set(case_work "${work_real}/${CASE}-${nonce}")
if(EXISTS "${case_work}")
  message(FATAL_ERROR "det.av_driver: unique scratch path already exists")
endif()
file(MAKE_DIRECTORY "${case_work}")
file(REAL_PATH "${case_work}" case_work_real)
if(NOT case_work_real STREQUAL case_work)
  message(FATAL_ERROR "det.av_driver: refusing redirected case scratch directory")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=/usr/bin:/bin"
    "CC65_HOME=${cc65_home}" "${ca65}" --cpu 65SC02 -o av_probe.o "${source}"
  WORKING_DIRECTORY "${case_work}" TIMEOUT 30 RESULT_VARIABLE ca_rc
  OUTPUT_VARIABLE ca_out ERROR_VARIABLE ca_err)
if(NOT ca_rc STREQUAL "0")
  message(FATAL_ERROR "det.av_driver: pinned ca65 failed (${ca_rc})\n${ca_out}${ca_err}\nArtifacts retained at ${case_work}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=/usr/bin:/bin"
    "CC65_HOME=${cc65_home}" "${ld65}" -C "${config}" -Ln av_probe.lbl
    -o av_probe.bll av_probe.o
  WORKING_DIRECTORY "${case_work}" TIMEOUT 30 RESULT_VARIABLE ld_rc
  OUTPUT_VARIABLE ld_out ERROR_VARIABLE ld_err)
if(NOT ld_rc STREQUAL "0" OR NOT EXISTS "${case_work}/av_probe.bll" OR
   NOT EXISTS "${case_work}/av_probe.lbl")
  message(FATAL_ERROR "det.av_driver: pinned ld65 failed (${ld_rc})\n${ld_out}${ld_err}\nArtifacts retained at ${case_work}")
endif()
file(REMOVE "${case_work}/av_probe.o")
if(EXISTS "${case_work}/av_probe.o")
  message(FATAL_ERROR "det.av_driver: temporary assembler object could not be removed; artifacts retained at ${case_work}")
endif()

if(CASE STREQUAL "audio_paths")
  set(test_timeout 300)
else()
  set(test_timeout 120)
endif()
execute_process(COMMAND "${DET}" "${CASE}" "${case_work}/av_probe.bll"
    "${case_work}/av_probe.lbl" "${manifest}"
  WORKING_DIRECTORY "${case_work}" TIMEOUT "${test_timeout}" RESULT_VARIABLE test_rc
  OUTPUT_VARIABLE test_out ERROR_VARIABLE test_err)
if(NOT test_rc STREQUAL "0")
  message(FATAL_ERROR "det.av_driver: ${CASE} failed (${test_rc})\n${test_out}${test_err}\nArtifacts retained at ${case_work}")
endif()

file(REMOVE_RECURSE "${case_work}")
if(EXISTS "${case_work}")
  message(FATAL_ERROR "det.av_driver: successful case scratch could not be removed: ${case_work}")
endif()
message(STATUS "det.av_driver: ${CASE} passed with the manifest-bound source-built guest; owned scratch removed")
