cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(required ROOT WORK SCRATCH_ROOT CONSUMER ARTIFACT_CHECKER ENV_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "frame_driver: -D${required} is required")
  endif()
endforeach()
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK}" NORMALIZE work_is_owned)
if(NOT work_is_owned OR "${WORK}" STREQUAL "${SCRATCH_ROOT}")
  message(FATAL_ERROR "frame_driver: refusing scratch path outside its isolated owned directory: ${WORK}")
endif()
set(SOURCE "${ROOT}/fixtures/diagnostic/input_probe.s")
set(CONFIG "${ROOT}/fixtures/diagnostic/input_probe.cfg")
set(MANIFEST "${ROOT}/fixtures/manifests/input-probe.manifest")
set(CA65 "${ROOT}/build/pins/cc65/bin/ca65")
set(LD65 "${ROOT}/build/pins/cc65/bin/ld65")
set(TOOL_FILES "${ROOT}/build/pins/cc65/.lynx-build-files")
if(NOT EXISTS "${CA65}" OR NOT EXISTS "${LD65}" OR NOT EXISTS "${TOOL_FILES}")
  message(FATAL_ERROR "frame_driver: pinned ca65/ld65 build is missing; scripts/check.sh owns pin preparation")
endif()
file(SHA256 "${CA65}" ca65_sha)
file(SHA256 "${LD65}" ld65_sha)
file(SHA256 "${SOURCE}" source_sha)
file(SHA256 "${CONFIG}" config_sha)
file(STRINGS "${TOOL_FILES}" ca65_record REGEX "^[0-9a-f]+ bin/ca65$")
file(STRINGS "${TOOL_FILES}" ld65_record REGEX "^[0-9a-f]+ bin/ld65$")
if(NOT ca65_record MATCHES "^${ca65_sha} bin/ca65$" OR NOT ld65_record MATCHES "^${ld65_sha} bin/ld65$")
  message(FATAL_ERROR "frame_driver: pinned cc65 binaries differ from their successful build record")
endif()
file(STRINGS "${MANIFEST}" manifest_source REGEX "^sha256 = ")
file(STRINGS "${MANIFEST}" manifest_config REGEX "^config_sha256 = ")
file(STRINGS "${MANIFEST}" manifest_ca65 REGEX "^ca65_sha256 = ")
file(STRINGS "${MANIFEST}" manifest_ld65 REGEX "^ld65_sha256 = ")
if(NOT manifest_source STREQUAL "sha256 = ${source_sha}" OR
   NOT manifest_config STREQUAL "config_sha256 = ${config_sha}" OR
   NOT manifest_ca65 STREQUAL "ca65_sha256 = ${ca65_sha}" OR
   NOT manifest_ld65 STREQUAL "ld65_sha256 = ${ld65_sha}")
  message(FATAL_ERROR "frame_driver: source/config/cc65 bytes disagree with the rights and build provenance manifest")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/input.timeline" "LYNX-INPUT 1\n109321 16\n")
execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
    "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02
    -o "${WORK}/input_probe.o" "${SOURCE}"
  WORKING_DIRECTORY "${WORK}" TIMEOUT 30 RESULT_VARIABLE ca_rc
  OUTPUT_VARIABLE ca_out ERROR_VARIABLE ca_err)
if(NOT ca_rc STREQUAL "0")
  message(FATAL_ERROR "frame_driver: pinned ca65 failed\n${ca_out}${ca_err}")
endif()
execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
    "CC65_HOME=${ROOT}/build/pins/cc65" "${LD65}" -C "${CONFIG}"
    -Ln "${WORK}/input_probe.lbl" -o "${WORK}/input_probe.bll" "${WORK}/input_probe.o"
  WORKING_DIRECTORY "${WORK}" TIMEOUT 30 RESULT_VARIABLE ld_rc
  OUTPUT_VARIABLE ld_out ERROR_VARIABLE ld_err)
if(NOT ld_rc STREQUAL "0" OR NOT EXISTS "${WORK}/input_probe.bll" OR
   NOT EXISTS "${WORK}/input_probe.lbl")
  message(FATAL_ERROR "frame_driver: pinned ld65 failed\n${ld_out}${ld_err}")
endif()
file(SHA256 "${WORK}/input_probe.bll" guest_sha)
file(SIZE "${WORK}/input_probe.bll" guest_size)
file(STRINGS "${MANIFEST}" manifest_guest_sha REGEX "^source_built_guest_sha256 = ")
file(STRINGS "${MANIFEST}" manifest_guest_size REGEX "^source_built_guest_bytes = ")
if(NOT manifest_guest_sha STREQUAL "source_built_guest_sha256 = ${guest_sha}" OR
   NOT manifest_guest_size STREQUAL "source_built_guest_bytes = ${guest_size}")
  message(FATAL_ERROR "frame_driver: source-built guest differs from its fixture manifest")
endif()
file(REMOVE "${WORK}/input_probe.o")
execute_process(COMMAND "${CONSUMER}" --mode frames
    --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
    --timeline "${WORK}/input.timeline" --out "${WORK}/trace"
  TIMEOUT 60 RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
if(NOT consumer_rc STREQUAL "0")
  message(FATAL_ERROR "frame_driver: public-only frame consumer failed (${consumer_rc})\n${consumer_out}${consumer_err}\nArtifacts retained under ${WORK}")
endif()
execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/trace"
  TIMEOUT 30 RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
if(NOT check_rc STREQUAL "0")
  message(FATAL_ERROR "frame_driver: independent raw-frame semantic check failed (${check_rc})\n${check_out}${check_err}\nArtifacts retained under ${WORK}")
endif()
message(STATUS "${consumer_out}${check_out}")
