cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(required CASE ROOT WORK SCRATCH_ROOT CONSUMER ARTIFACT_CHECKER ENV_EXECUTABLE)
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
if(CASE STREQUAL "frame_tracer")
  file(WRITE "${WORK}/input.timeline" "LYNX-INPUT 1\n109321 16\n")
elseif(CASE STREQUAL "replay_edges")
  file(WRITE "${WORK}/input.timeline"
    "LYNX-INPUT 1\n0 16\n54271 0\n54272 16\n54273 0\n162816 16\n")
elseif(CASE STREQUAL "replay_restore")
  file(WRITE "${WORK}/input.timeline" "LYNX-INPUT 1\n108544 16\n")
elseif(CASE STREQUAL "frame_replay")
  file(WRITE "${WORK}/input.timeline"
    "LYNX-INPUT 1\n0 0\n65536 16\n131072 32\n196608 1\n262144 64\n327680 128\n393216 256\n458752 52\n524288 0\n")
else()
  message(FATAL_ERROR "frame_driver: unknown CASE ${CASE}")
endif()
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
if(CASE STREQUAL "replay_edges")
  file(WRITE "${WORK}/input.invalid" "LYNX-INPUT 1\n0 16\n0 0\n")
  execute_process(COMMAND "${CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.invalid" --out "${WORK}/invalid-trace"
    TIMEOUT 60 RESULT_VARIABLE invalid_rc OUTPUT_VARIABLE invalid_out ERROR_VARIABLE invalid_err)
  if(invalid_rc STREQUAL "0" OR EXISTS "${WORK}/invalid-trace" OR
     NOT invalid_err MATCHES "timeline is not canonical LYNX-INPUT 1 ASCII")
    message(FATAL_ERROR "frame_driver: duplicate timeline negative control did not refuse before output creation\n${invalid_out}${invalid_err}")
  endif()
endif()
if(CASE STREQUAL "frame_tracer")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "LYNX_TEST_REPLAY_LEGACY=1" "${CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.timeline" --out "${WORK}/trace"
    TIMEOUT 60 RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
elseif(CASE STREQUAL "replay_restore")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "LYNX_TEST_REPLAY_RESTORE=1" "${CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.timeline" --out "${WORK}/trace"
    TIMEOUT 60 RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
else()
  execute_process(COMMAND "${CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.timeline" --out "${WORK}/trace"
    TIMEOUT 60 RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
endif()
if(NOT consumer_rc STREQUAL "0")
  message(FATAL_ERROR "frame_driver: public-only frame consumer failed (${consumer_rc})\n${consumer_out}${consumer_err}\nArtifacts retained under ${WORK}")
endif()
if(CASE STREQUAL "replay_edges")
  file(WRITE "${WORK}/input.empty" "LYNX-INPUT 1\n")
  execute_process(COMMAND "${CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.empty" --out "${WORK}/empty-trace"
    TIMEOUT 60 RESULT_VARIABLE empty_rc OUTPUT_VARIABLE empty_out ERROR_VARIABLE empty_err)
  if(NOT empty_rc STREQUAL "0")
    message(FATAL_ERROR "frame_driver: bounded empty timeline did not start with released input\n${empty_out}${empty_err}")
  endif()
  file(SHA256 "${WORK}/input.empty" empty_sha)
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/empty-trace" empty "${empty_sha}"
    TIMEOUT 30 RESULT_VARIABLE empty_check_rc OUTPUT_VARIABLE empty_check_out ERROR_VARIABLE empty_check_err)
  if(NOT empty_check_rc STREQUAL "0")
    message(FATAL_ERROR "frame_driver: independent empty timeline check failed\n${empty_check_out}${empty_check_err}")
  endif()
  file(SHA256 "${WORK}/empty-trace/frame-00.raw" empty_raw_sha)
  file(SHA256 "${WORK}/empty-trace/frame-00.ppm" empty_rendered_sha)
  file(STRINGS "${WORK}/empty-trace/frame-trace.manifest" empty_frame_line REGEX "^frame\\[0\\] = ")
  if(NOT empty_frame_line MATCHES "raw_sha256:${empty_raw_sha},rendered_sha256:${empty_rendered_sha}$")
    message(FATAL_ERROR "frame_driver: independent empty-frame digests differ from the manifest")
  endif()
endif()
if(CASE STREQUAL "frame_tracer")
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/trace"
    TIMEOUT 30 RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
else()
  file(SHA256 "${WORK}/input.timeline" timeline_sha)
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/trace" "${CASE}" "${timeline_sha}" "1" "none"
    TIMEOUT 30 RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
endif()
if(NOT check_rc STREQUAL "0")
  message(FATAL_ERROR "frame_driver: independent raw-frame semantic check failed (${check_rc})\n${check_out}${check_err}\nArtifacts retained under ${WORK}")
endif()
if(NOT CASE STREQUAL "frame_tracer")
  file(STRINGS "${WORK}/trace/frame-trace.manifest" frame_count_line REGEX "^frame_count = ")
  if(NOT frame_count_line MATCHES "^frame_count = ([0-9]+)$")
    message(FATAL_ERROR "frame_driver: generic replay manifest has no bounded frame_count")
  endif()
  set(frame_count "${CMAKE_MATCH_1}")
  math(EXPR last_frame "${frame_count} - 1")
  foreach(index RANGE 0 ${last_frame})
    if(index LESS 10)
      set(prefix "frame-0${index}")
    else()
      set(prefix "frame-${index}")
    endif()
    file(SHA256 "${WORK}/trace/${prefix}.raw" raw_sha)
    file(SHA256 "${WORK}/trace/${prefix}.ppm" rendered_sha)
    file(STRINGS "${WORK}/trace/frame-trace.manifest" frame_line REGEX "^frame\\[${index}\\] = ")
    if(NOT frame_line MATCHES "raw_sha256:${raw_sha},rendered_sha256:${rendered_sha}$")
      message(FATAL_ERROR "frame_driver: independent file SHA256 differs from manifest for ${prefix}")
    endif()
  endforeach()
endif()
message(STATUS "${consumer_out}${check_out}")
