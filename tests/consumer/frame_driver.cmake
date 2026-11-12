cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(required CASE ROOT WORK SCRATCH_ROOT CONSUMER TEST_CONSUMER ARTIFACT_CHECKER ENV_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "frame_driver: -D${required} is required")
  endif()
endforeach()
cmake_path(IS_PREFIX SCRATCH_ROOT "${WORK}" NORMALIZE work_is_owned)
if(NOT work_is_owned OR "${WORK}" STREQUAL "${SCRATCH_ROOT}")
  message(FATAL_ERROR "frame_driver: refusing scratch path outside its isolated owned directory: ${WORK}")
endif()
set(SOURCE "${ROOT}/fixtures/diagnostic/input_probe.s")
set(LEGACY_SOURCE "${ROOT}/fixtures/diagnostic/input_probe_tracer.s")
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
file(SHA256 "${LEGACY_SOURCE}" legacy_source_sha)
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
file(STRINGS "${MANIFEST}" manifest_legacy_source REGEX "^legacy_source_sha256 = ")
if(NOT manifest_source STREQUAL "sha256 = ${source_sha}" OR
   NOT manifest_config STREQUAL "config_sha256 = ${config_sha}" OR
   NOT manifest_ca65 STREQUAL "ca65_sha256 = ${ca65_sha}" OR
   NOT manifest_ld65 STREQUAL "ld65_sha256 = ${ld65_sha}" OR
   NOT manifest_legacy_source STREQUAL "legacy_source_sha256 = ${legacy_source_sha}")
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
    "LYNX-INPUT 1\n0 0\n108545 16\n217089 32\n325633 1\n434177 2\n542721 4\n651265 8\n759809 64\n868353 128\n976897 256\n1085441 511\n1193985 0\n")
elseif(CASE STREQUAL "replay_controls")
  file(WRITE "${WORK}/input.timeline" "LYNX-INPUT 1\n0 0\n108545 16\n")
elseif(CASE STREQUAL "frame_publication_controls")
  file(WRITE "${WORK}/input.timeline" "LYNX-INPUT 1\n108544 16\n")
elseif(CASE STREQUAL "frame_response_controls")
  file(WRITE "${WORK}/input.timeline"
    "LYNX-INPUT 1\n0 0\n108545 16\n217089 32\n325633 1\n434177 2\n542721 4\n651265 8\n759809 64\n868353 128\n976897 256\n1085441 511\n1193985 0\n")
else()
  message(FATAL_ERROR "frame_driver: unknown CASE ${CASE}")
endif()
function(run_replay bll labels timeline output scenario handedness profile interleaved)
  if(interleaved)
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "LYNX_TEST_REPLAY_INTERLEAVED=1" "${TEST_CONSUMER}" --mode frames
        --content "${bll}" --symbols "${labels}"
        --timeline "${timeline}" --out "${output}" --profile "${profile}"
      TIMEOUT 120 RESULT_VARIABLE consumer_rc
      OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
  else()
    execute_process(COMMAND "${CONSUMER}" --mode frames
        --content "${bll}" --symbols "${labels}"
        --timeline "${timeline}" --out "${output}" --profile "${profile}"
      TIMEOUT 120 RESULT_VARIABLE consumer_rc
      OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
  endif()
  if(NOT consumer_rc STREQUAL "0")
    message(FATAL_ERROR "frame_driver: ${scenario} public-only frame consumer failed (${consumer_rc})\n${consumer_out}${consumer_err}\nArtifacts retained under ${output}")
  endif()
  if(interleaved AND NOT consumer_out MATCHES "lynx-consume-test: interleaved replay verification passed")
    message(FATAL_ERROR "frame_driver: test consumer did not confirm the interleaved replay verification\n${consumer_out}${consumer_err}")
  endif()
  file(SHA256 "${timeline}" timeline_sha)
  file(SHA256 "${bll}" content_sha)
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${output}" "${scenario}"
      "${timeline_sha}" "${handedness}" "${profile}" "${content_sha}"
    TIMEOUT 30 RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
  if(NOT check_rc STREQUAL "0")
    message(FATAL_ERROR "frame_driver: independent ${scenario} check failed (${check_rc})\n${check_out}${check_err}\nArtifacts retained under ${output}")
  endif()
  file(STRINGS "${output}/frame-trace.manifest" frame_count_line REGEX "^frame_count = ")
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
    file(SHA256 "${output}/${prefix}.raw" raw_sha)
    file(SHA256 "${output}/${prefix}.ppm" rendered_sha)
    file(STRINGS "${output}/frame-trace.manifest" frame_line REGEX "^frame\\[${index}\\] = ")
    if(NOT frame_line MATCHES "raw_sha256:${raw_sha},rendered_sha256:${rendered_sha}$")
      message(FATAL_ERROR "frame_driver: independent file SHA256 differs from manifest for ${prefix}")
    endif()
  endforeach()
  message(STATUS "${consumer_out}${check_out}")
endfunction()
function(compare_replay_outputs first second count expected_equal)
  set(start_index 0)
  if(ARGC GREATER 4)
    set(start_index "${ARGV4}")
  endif()
  math(EXPR last_index "${count} - 1")
  foreach(index RANGE ${start_index} ${last_index})
    if(index LESS 10)
      set(prefix "frame-0${index}")
    else()
      set(prefix "frame-${index}")
    endif()
    file(SHA256 "${first}/${prefix}.raw" first_raw)
    file(SHA256 "${second}/${prefix}.raw" second_raw)
    file(SHA256 "${first}/${prefix}.ppm" first_rendered)
    file(SHA256 "${second}/${prefix}.ppm" second_rendered)
    if(expected_equal)
      if(NOT first_raw STREQUAL second_raw OR NOT first_rendered STREQUAL second_rendered)
        message(FATAL_ERROR "frame_driver: fresh/interleaved replay outputs differ at ${prefix}")
      endif()
    else()
      if(first_raw STREQUAL second_raw OR first_rendered STREQUAL second_rendered)
        message(FATAL_ERROR "frame_driver: controlled event mutation did not change raw and rendered observations at ${prefix}")
      endif()
    endif()
  endforeach()
endfunction()
if(CASE STREQUAL "frame_replay")
  file(WRITE "${WORK}/input.sensitivity.timeline"
    "LYNX-INPUT 1\n0 0\n108545 16\n217089 32\n325633 1\n434177 2\n542721 4\n651265 8\n759809 64\n868353 128\n976897 256\n1085441 511\n1193985 16\n")
  file(SHA256 "${WORK}/input.timeline" baseline_timeline_sha)
  file(SHA256 "${WORK}/input.sensitivity.timeline" sensitivity_timeline_sha)
  if(baseline_timeline_sha STREQUAL sensitivity_timeline_sha)
    message(FATAL_ERROR "frame_driver: controlled input event mutation did not change canonical timeline bytes")
  endif()
  foreach(handedness RANGE 0 1)
    set(guest_dir "${WORK}/handedness-${handedness}")
    file(MAKE_DIRECTORY "${guest_dir}")
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02
        -DLEFTHAND=${handedness} -o "${guest_dir}/input_probe.o" "${SOURCE}"
      WORKING_DIRECTORY "${guest_dir}" TIMEOUT 30 RESULT_VARIABLE ca_rc
      OUTPUT_VARIABLE ca_out ERROR_VARIABLE ca_err)
    if(NOT ca_rc STREQUAL "0")
      message(FATAL_ERROR "frame_driver: pinned ca65 failed for handedness ${handedness}\n${ca_out}${ca_err}")
    endif()
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "CC65_HOME=${ROOT}/build/pins/cc65" "${LD65}" -C "${CONFIG}"
        -Ln "${guest_dir}/input_probe.lbl" -o "${guest_dir}/input_probe.bll" "${guest_dir}/input_probe.o"
      WORKING_DIRECTORY "${guest_dir}" TIMEOUT 30 RESULT_VARIABLE ld_rc
      OUTPUT_VARIABLE ld_out ERROR_VARIABLE ld_err)
    if(NOT ld_rc STREQUAL "0" OR NOT EXISTS "${guest_dir}/input_probe.bll" OR
       NOT EXISTS "${guest_dir}/input_probe.lbl")
      message(FATAL_ERROR "frame_driver: pinned ld65 failed for handedness ${handedness}\n${ld_out}${ld_err}")
    endif()
    file(SHA256 "${guest_dir}/input_probe.bll" guest_sha)
    file(SIZE "${guest_dir}/input_probe.bll" guest_size)
    file(STRINGS "${MANIFEST}" manifest_guest_sha
      REGEX "^source_built_guest_sha256_lefthand_${handedness} = ")
    file(STRINGS "${MANIFEST}" manifest_guest_size
      REGEX "^source_built_guest_bytes_lefthand_${handedness} = ")
    if(NOT manifest_guest_sha STREQUAL "source_built_guest_sha256_lefthand_${handedness} = ${guest_sha}" OR
       NOT manifest_guest_size STREQUAL "source_built_guest_bytes_lefthand_${handedness} = ${guest_size}")
      message(FATAL_ERROR "frame_driver: source-built handedness ${handedness} guest differs from its fixture manifest")
    endif()
    file(REMOVE "${guest_dir}/input_probe.o")
    foreach(profile lynx1 lynx2)
      set(case_dir "${WORK}/handedness-${handedness}/${profile}")
      file(MAKE_DIRECTORY "${case_dir}")
      set(baseline_dir "${case_dir}/baseline")
      set(fresh_dir "${case_dir}/fresh")
      set(sensitivity_dir "${case_dir}/sensitivity")
      run_replay(
        "${guest_dir}/input_probe.bll" "${guest_dir}/input_probe.lbl"
        "${WORK}/input.timeline" "${baseline_dir}" frame_replay
        "${handedness}" "${profile}" TRUE)
      if(handedness EQUAL 0 AND profile STREQUAL "lynx1")
        execute_process(COMMAND "${ARTIFACT_CHECKER}" frames-color-negative "${baseline_dir}" frame_replay
            "${baseline_timeline_sha}" "${handedness}" "${profile}"
          TIMEOUT 30 RESULT_VARIABLE color_negative_rc
          OUTPUT_VARIABLE color_negative_out ERROR_VARIABLE color_negative_err)
        if(NOT color_negative_rc STREQUAL "0")
          message(FATAL_ERROR "frame_driver: independent checker failed to reject a corrupted transition pixel color\n${color_negative_out}${color_negative_err}")
        endif()
        message(STATUS "${color_negative_out}")
        set(negative_dir "${case_dir}/invalid-transition-pen")
        file(MAKE_DIRECTORY "${negative_dir}")
        file(COPY "${baseline_dir}/" DESTINATION "${negative_dir}")
        execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
            dd if=/dev/zero "of=${negative_dir}/frame-01.raw" bs=1 count=1 conv=notrunc
          TIMEOUT 10 RESULT_VARIABLE mutate_rc
          OUTPUT_VARIABLE mutate_out ERROR_VARIABLE mutate_err)
        if(NOT mutate_rc STREQUAL "0")
          message(FATAL_ERROR "frame_driver: could not create the controlled invalid raw-pen negative control\n${mutate_out}${mutate_err}")
        endif()
        execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${negative_dir}" frame_replay
            "${baseline_timeline_sha}" "${handedness}" "${profile}"
          TIMEOUT 30 RESULT_VARIABLE negative_rc
          OUTPUT_VARIABLE negative_out ERROR_VARIABLE negative_err)
        if(negative_rc STREQUAL "0" OR NOT negative_err MATCHES "raw guest input response differs from the recorded logical input")
          message(FATAL_ERROR "frame_driver: independent checker accepted an invalid transition pen byte\n${negative_out}${negative_err}")
        endif()
      endif()
      run_replay(
        "${guest_dir}/input_probe.bll" "${guest_dir}/input_probe.lbl"
        "${WORK}/input.timeline" "${fresh_dir}" frame_replay
        "${handedness}" "${profile}" FALSE)
      compare_replay_outputs("${baseline_dir}" "${fresh_dir}" 23 TRUE)
      run_replay(
        "${guest_dir}/input_probe.bll" "${guest_dir}/input_probe.lbl"
        "${WORK}/input.sensitivity.timeline" "${sensitivity_dir}" frame_replay_sensitivity
        "${handedness}" "${profile}" TRUE)
      compare_replay_outputs("${baseline_dir}" "${sensitivity_dir}" 21 TRUE)
      compare_replay_outputs("${baseline_dir}" "${sensitivity_dir}" 23 FALSE 22)
    endforeach()
  endforeach()
  message(STATUS "frame_replay: both guest handedness mappings and lynx1/lynx2 profiles passed fresh, interleaved, and event-sensitivity checks")
  return()
endif()
set(BUILD_SOURCE "${SOURCE}")
set(ca_definitions "-DLEFTHAND=1")
if(CASE STREQUAL "frame_tracer")
  set(BUILD_SOURCE "${LEGACY_SOURCE}")
  set(ca_definitions "")
endif()
execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
    "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02
    ${ca_definitions} -o "${WORK}/input_probe.o" "${BUILD_SOURCE}"
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
if(CASE STREQUAL "frame_tracer")
  file(STRINGS "${MANIFEST}" manifest_guest_sha REGEX "^source_built_legacy_tracer_guest_sha256 = ")
  file(STRINGS "${MANIFEST}" manifest_guest_size REGEX "^source_built_legacy_tracer_guest_bytes = ")
  if(NOT manifest_guest_sha STREQUAL "source_built_legacy_tracer_guest_sha256 = ${guest_sha}" OR
     NOT manifest_guest_size STREQUAL "source_built_legacy_tracer_guest_bytes = ${guest_size}")
    message(FATAL_ERROR "frame_driver: source-built legacy tracer guest differs from its fixture manifest")
  endif()
else()
  file(STRINGS "${MANIFEST}" manifest_guest_sha REGEX "^source_built_guest_sha256_lefthand_1 = ")
  file(STRINGS "${MANIFEST}" manifest_guest_size REGEX "^source_built_guest_bytes_lefthand_1 = ")
  if(NOT manifest_guest_sha STREQUAL "source_built_guest_sha256_lefthand_1 = ${guest_sha}" OR
     NOT manifest_guest_size STREQUAL "source_built_guest_bytes_lefthand_1 = ${guest_size}")
    message(FATAL_ERROR "frame_driver: source-built guest differs from its fixture manifest")
  endif()
endif()
file(REMOVE "${WORK}/input_probe.o")
if(CASE STREQUAL "frame_response_controls")
  function(build_input_variant variant_name anchor replacement)
    set(variant_dir "${WORK}/variant-${variant_name}")
    file(MAKE_DIRECTORY "${variant_dir}")
    file(READ "${SOURCE}" original_text)
    string(FIND "${original_text}" "${anchor}" anchor_position)
    if(anchor_position LESS 0)
      message(FATAL_ERROR "frame_driver: unique source anchor for ${variant_name} is missing")
    endif()
    string(REPLACE "${anchor}" "" text_without_anchor "${original_text}")
    string(FIND "${text_without_anchor}" "${anchor}" duplicate_anchor_position)
    if(NOT duplicate_anchor_position LESS 0)
      message(FATAL_ERROR "frame_driver: source anchor for ${variant_name} is not unique")
    endif()
    string(REPLACE "${anchor}" "${replacement}" variant_text "${original_text}")
    file(WRITE "${variant_dir}/input_probe.s" "${variant_text}")
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02
        -DLEFTHAND=1 -o "${variant_dir}/input_probe.o" "${variant_dir}/input_probe.s"
      WORKING_DIRECTORY "${variant_dir}" TIMEOUT 30 RESULT_VARIABLE variant_ca_rc
      OUTPUT_VARIABLE variant_ca_out ERROR_VARIABLE variant_ca_err)
    if(NOT variant_ca_rc STREQUAL "0")
      message(FATAL_ERROR "frame_driver: pinned ca65 failed to build ${variant_name}\n${variant_ca_out}${variant_ca_err}")
    endif()
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "CC65_HOME=${ROOT}/build/pins/cc65" "${LD65}" -C "${CONFIG}"
        -Ln "${variant_dir}/input_probe.lbl" -o "${variant_dir}/input_probe.bll" "${variant_dir}/input_probe.o"
      WORKING_DIRECTORY "${variant_dir}" TIMEOUT 30 RESULT_VARIABLE variant_ld_rc
      OUTPUT_VARIABLE variant_ld_out ERROR_VARIABLE variant_ld_err)
    if(NOT variant_ld_rc STREQUAL "0" OR NOT EXISTS "${variant_dir}/input_probe.bll" OR
       NOT EXISTS "${variant_dir}/input_probe.lbl")
      message(FATAL_ERROR "frame_driver: pinned ld65 failed to link ${variant_name}\n${variant_ld_out}${variant_ld_err}")
    endif()
    file(SHA256 "${SOURCE}" original_source_sha)
    file(SHA256 "${variant_dir}/input_probe.s" variant_source_sha)
    file(SHA256 "${variant_dir}/input_probe.o" object_sha)
    file(SHA256 "${variant_dir}/input_probe.bll" variant_bll_sha)
    file(SHA256 "${variant_dir}/input_probe.lbl" variant_labels_sha)
    file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" driver_sha)
    file(WRITE "${variant_dir}/fixture.identity"
      "variant = ${variant_name}\nsource_original_sha256 = ${original_source_sha}\nsource_variant_sha256 = ${variant_source_sha}\nconfig_sha256 = ${config_sha}\nca65_sha256 = ${ca65_sha}\nld65_sha256 = ${ld65_sha}\ndriver_sha256 = ${driver_sha}\nobject_sha256 = ${object_sha}\nbll_sha256 = ${variant_bll_sha}\nlabels_sha256 = ${variant_labels_sha}\nanchor = ${anchor}")
    set(${variant_name}_BLL "${variant_dir}/input_probe.bll" PARENT_SCOPE)
    set(${variant_name}_LBL "${variant_dir}/input_probe.lbl" PARENT_SCOPE)
  endfunction()
  function(expect_semantic_refusal scenario content labels timeline output expected)
    execute_process(COMMAND "${CONSUMER}" --mode frames
        --content "${content}" --symbols "${labels}"
        --timeline "${timeline}" --out "${output}" --profile none
      TIMEOUT 60 RESULT_VARIABLE semantic_rc
      OUTPUT_VARIABLE semantic_out ERROR_VARIABLE semantic_err)
    if(NOT semantic_rc STREQUAL "1" OR NOT semantic_out STREQUAL "" OR
       EXISTS "${output}" OR NOT semantic_err MATCHES "${expected}")
      message(FATAL_ERROR
        "frame_driver: ${scenario} did not fail at the intended guest semantic assertion '${expected}'\n"
        "actual exit=${semantic_rc}\n${semantic_out}${semantic_err}")
    endif()
  endfunction()
  build_input_variant(no-progress "poll_input:\n"
    "stall_before_poll:\n        jmp stall_before_poll\npoll_input:\n")
  build_input_variant(no-response "        and probe_result + 5\n"
    "        and #$00\n")
  build_input_variant(bad-schema "        lda #$03\n        sta probe_result + 4\n"
    "        lda #$02\n        sta probe_result + 4\n")
  file(SHA256 "${WORK}/input.timeline" timeline_sha)
  set(positive_before_no_progress "${WORK}/positive-before-no-progress")
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${positive_before_no_progress}" frame_replay 1 none FALSE)
  expect_semantic_refusal(no-progress "${no-progress_BLL}" "${no-progress_LBL}"
    "${WORK}/input.timeline" "${WORK}/no-progress-output" "homebrew progress did not advance")
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/positive-after-no-progress" frame_replay 1 none FALSE)
  set(positive_before_no_response "${WORK}/positive-before-no-response")
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${positive_before_no_response}" frame_replay 1 none FALSE)
  set(stale_good "${WORK}/stale-good-output")
  file(COPY "${positive_before_no_response}/" DESTINATION "${stale_good}")
  file(SHA256 "${stale_good}/frame-trace.manifest" stale_manifest_before)
  file(SHA256 "${stale_good}/frame-00.raw" stale_raw_before)
  expect_semantic_refusal(no-response "${no-response_BLL}" "${no-response_LBL}"
    "${WORK}/input.timeline" "${stale_good}/new-frame-run" "homebrew input response mismatch")
  file(SHA256 "${stale_good}/frame-trace.manifest" stale_manifest_after)
  file(SHA256 "${stale_good}/frame-00.raw" stale_raw_after)
  if(NOT stale_manifest_before STREQUAL stale_manifest_after OR NOT stale_raw_before STREQUAL stale_raw_after)
    message(FATAL_ERROR "frame_driver: no-response invocation changed stale-good guest output")
  endif()
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames-color-negative
      "${positive_before_no_response}" frame_replay "${timeline_sha}" 1 none
    TIMEOUT 30 RESULT_VARIABLE color_rc OUTPUT_VARIABLE color_out ERROR_VARIABLE color_err)
  if(NOT color_rc STREQUAL "0" OR NOT color_out MATCHES "rejected the deliberately corrupted transition pixel color")
    message(FATAL_ERROR "frame_driver: correct raw input response did not remain independent from wrong rendered color\n${color_out}${color_err}")
  endif()
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/positive-after-no-response" frame_replay 1 none FALSE)
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/positive-before-schema" frame_replay 1 none FALSE)
  expect_semantic_refusal(bad-schema "${bad-schema_BLL}" "${bad-schema_LBL}"
    "${WORK}/input.timeline" "${WORK}/bad-schema-output" "homebrew result block schema is invalid")
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/positive-after-schema" frame_replay 1 none FALSE)
  message(STATUS "frame_response_controls: source-bound progress, response, and result-schema mutants failed with fresh positive brackets")
  return()
endif()
if(CASE STREQUAL "frame_publication_controls")
  file(SHA256 "${WORK}/input.timeline" timeline_sha)
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/publication-positive-before" replay_restore 1 none FALSE)
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frame-publication
      "${TEST_CONSUMER}" "${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
      "${WORK}/input.timeline" "${WORK}/publication-race" "${timeline_sha}"
    TIMEOUT 90 RESULT_VARIABLE race_rc OUTPUT_VARIABLE race_out ERROR_VARIABLE race_err)
  if(NOT race_rc STREQUAL "0" OR NOT race_out MATCHES "one complete bundle and one refusal")
    message(FATAL_ERROR "frame_driver: concurrent frame publication did not yield one verified winner\n${race_out}${race_err}")
  endif()
  set(decoy "${WORK}/decoy.lynxplosion-frame-stage-UNOWND")
  file(MAKE_DIRECTORY "${decoy}")
  file(WRITE "${decoy}/.lynx-owner" "other-invocation\n")
  file(WRITE "${decoy}/outside.marker" "preserve this unowned stage\n")
  foreach(failure IN ITEMS
      "write:frame-00.raw" "write:frame-00.ppm" "write:frame-trace.manifest"
      "close:frame-00.raw" "close:frame-00.ppm" "close:frame-trace.manifest")
    string(REPLACE ":" "-" failure_name "${failure}")
    set(failed_output "${WORK}/failure-${failure_name}")
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
        "LYNX_TEST_IO_FAILURE=${failure}" "${TEST_CONSUMER}" --mode frames
        --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
        --timeline "${WORK}/input.timeline" --out "${failed_output}" --profile none
      TIMEOUT 60 RESULT_VARIABLE failure_rc OUTPUT_VARIABLE failure_out ERROR_VARIABLE failure_err)
    file(GLOB leftover_stages "${failed_output}.lynxplosion-frame-stage-*")
    if(NOT failure_rc STREQUAL "1" OR NOT failure_out STREQUAL "" OR
       EXISTS "${failed_output}" OR leftover_stages OR
       NOT failure_err MATCHES "cannot write the complete frame output")
      message(FATAL_ERROR "frame_driver: ${failure} failure did not clean only its owned unpublished stage\n${failure_out}${failure_err}")
    endif()
    file(READ "${decoy}/.lynx-owner" decoy_owner)
    file(READ "${decoy}/outside.marker" decoy_marker)
    if(NOT decoy_owner STREQUAL "other-invocation\n" OR
       NOT decoy_marker STREQUAL "preserve this unowned stage\n")
      message(FATAL_ERROR "frame_driver: failed frame publication modified a stale stage without its nonce")
    endif()
  endforeach()
  set(aborted_output "${WORK}/failure-before-publish")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "LYNX_TEST_FRAME_ABORT=before-publish" "${TEST_CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.timeline" --out "${aborted_output}" --profile none
    TIMEOUT 60 RESULT_VARIABLE abort_rc OUTPUT_VARIABLE abort_out ERROR_VARIABLE abort_err)
  file(GLOB aborted_stages "${aborted_output}.lynxplosion-frame-stage-*")
  if(NOT abort_rc STREQUAL "1" OR NOT abort_out STREQUAL "" OR
     EXISTS "${aborted_output}" OR aborted_stages OR
     NOT abort_err MATCHES "cannot write the complete frame output")
    message(FATAL_ERROR "frame_driver: prepublish interruption did not preserve the absent destination and clean its owned stage\n${abort_out}${abort_err}")
  endif()
  run_replay("${WORK}/input_probe.bll" "${WORK}/input_probe.lbl"
    "${WORK}/input.timeline" "${WORK}/publication-positive-after" replay_restore 1 none FALSE)
  message(STATUS "${race_out}frame_publication_controls: all frame writes/close and prepublish faults cleaned their nonce-owned stage")
  return()
endif()
if(CASE STREQUAL "replay_controls")
  function(expect_frame_refusal binary content labels timeline output expected)
    execute_process(COMMAND "${binary}" --mode frames
        --content "${content}" --symbols "${labels}"
        --timeline "${timeline}" --out "${output}" --profile none
      TIMEOUT 30 RESULT_VARIABLE child_rc
      OUTPUT_VARIABLE child_out ERROR_VARIABLE child_err)
    if(NOT child_rc STREQUAL "1" OR NOT child_out STREQUAL "" OR
       EXISTS "${output}" OR NOT child_err MATCHES "${expected}")
      message(FATAL_ERROR
        "frame_driver: refusal assertion expected exit=1, no stdout, no output, and '${expected}'\n"
        "actual exit=${child_rc}\n${child_out}${child_err}")
    endif()
  endfunction()
  function(expect_existing_destination_refusal binary content labels timeline output expected)
    execute_process(COMMAND "${binary}" --mode frames
        --content "${content}" --symbols "${labels}"
        --timeline "${timeline}" --out "${output}" --profile none
      TIMEOUT 30 RESULT_VARIABLE child_rc
      OUTPUT_VARIABLE child_out ERROR_VARIABLE child_err)
    if(NOT child_rc STREQUAL "1" OR NOT child_out STREQUAL "" OR
       NOT EXISTS "${output}" OR NOT child_err MATCHES "${expected}")
      message(FATAL_ERROR
        "frame_driver: existing-destination refusal expected exit=1, empty stdout, preserved path, and '${expected}'\n"
        "actual exit=${child_rc}\n${child_out}${child_err}")
    endif()
  endfunction()
  set(content "${WORK}/input_probe.bll")
  set(labels "${WORK}/input_probe.lbl")
  set(timeline "${WORK}/input.timeline")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${content}"
    "${WORK}/alias-output" "frame input paths must refer to distinct files")
  file(WRITE "${WORK}/bad-mask.timeline" "LYNX-INPUT 1\n1 512\n")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/bad-mask.timeline"
    "${WORK}/bad-mask-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/bad-decimal.timeline" "LYNX-INPUT 1\n00 0\n")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/bad-decimal.timeline"
    "${WORK}/bad-decimal-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/duplicate-zero.timeline" "LYNX-INPUT 1\n0 0\n0 16\n")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/duplicate-zero.timeline"
    "${WORK}/duplicate-zero-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/missing-lf.timeline" "LYNX-INPUT 1\n1 0")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/missing-lf.timeline"
    "${WORK}/missing-lf-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/carriage-return.timeline" "LYNX-INPUT 1\r\n")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/carriage-return.timeline"
    "${WORK}/carriage-return-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/records-4097.timeline" "LYNX-INPUT 1\n")
  foreach(index RANGE 0 4096)
    file(APPEND "${WORK}/records-4097.timeline" "${index} 0\n")
  endforeach()
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/records-4097.timeline"
    "${WORK}/records-4097-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  file(WRITE "${WORK}/records-4096.timeline" "LYNX-INPUT 1\n")
  foreach(index RANGE 0 4095)
    file(APPEND "${WORK}/records-4096.timeline" "${index} 0\n")
  endforeach()
  execute_process(COMMAND "${CONSUMER}" --mode frames
      --content "${content}" --symbols "${labels}"
      --timeline "${WORK}/records-4096.timeline" --out "${WORK}/records-4096-output" --profile none
    TIMEOUT 60 RESULT_VARIABLE boundary_rc OUTPUT_VARIABLE boundary_out ERROR_VARIABLE boundary_err)
  if(NOT boundary_rc STREQUAL "0" OR NOT boundary_out MATCHES "replayed 4096 canonical input events" OR
     NOT EXISTS "${WORK}/records-4096-output/frame-trace.manifest")
    message(FATAL_ERROR "frame_driver: exact 4096-record canonical boundary did not publish a complete output\n${boundary_out}${boundary_err}")
  endif()
  string(REPEAT "x" 131072 exact_limit_bytes)
  file(WRITE "${WORK}/timeline-128k.timeline" "${exact_limit_bytes}")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/timeline-128k.timeline"
    "${WORK}/timeline-128k-output" "timeline is not canonical LYNX-INPUT 1 ASCII")
  string(APPEND exact_limit_bytes "x")
  file(WRITE "${WORK}/timeline-over-128k.timeline" "${exact_limit_bytes}")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/timeline-over-128k.timeline"
    "${WORK}/timeline-over-128k-output" "input timeline exceeds its 131072-byte bound")
  file(CREATE_LINK "${content}" "${WORK}/content-symlink" SYMBOLIC RESULT content_symlink_result)
  if(NOT content_symlink_result STREQUAL "0")
    message(FATAL_ERROR "frame_driver: could not create a same-file symlink alias: ${content_symlink_result}")
  endif()
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/content-symlink"
    "${WORK}/symlink-alias-output" "frame input paths must refer to distinct files")
  file(CREATE_LINK "${content}" "${WORK}/content-hardlink" RESULT content_hardlink_result)
  if(NOT content_hardlink_result STREQUAL "0")
    message(FATAL_ERROR "frame_driver: could not create a same-file hardlink alias: ${content_hardlink_result}")
  endif()
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${WORK}/content-hardlink"
    "${WORK}/hardlink-alias-output" "frame input paths must refer to distinct files")
  expect_frame_refusal("${CONSUMER}" "${content}" "${content}" "${timeline}"
    "${WORK}/symbols-content-alias-output" "frame input paths must refer to distinct files")
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${labels}"
    "${WORK}/symbols-timeline-alias-output" "frame input paths must refer to distinct files")
  file(SHA256 "${content}" content_before_output_alias)
  expect_existing_destination_refusal("${CONSUMER}" "${content}" "${labels}" "${timeline}"
    "${content}" "output directory already exists")
  file(SHA256 "${content}" content_after_output_alias)
  if(NOT content_before_output_alias STREQUAL content_after_output_alias)
    message(FATAL_ERROR "frame_driver: output alias refusal modified the content input")
  endif()
  expect_existing_destination_refusal("${CONSUMER}" "${content}" "${labels}" "${timeline}"
    "${timeline}" "output directory already exists")
  file(READ "${labels}" label_text)
  string(REGEX MATCH "al [0-9A-Fa-f]+ [. ]probe_result\n" probe_result_line "${label_text}")
  if(probe_result_line STREQUAL "")
    message(FATAL_ERROR "frame_driver: source fixture omitted its unique probe_result symbol")
  endif()
  string(REPLACE "${probe_result_line}" "" missing_symbol_labels "${label_text}")
  file(WRITE "${WORK}/missing-symbol.lbl" "${missing_symbol_labels}")
  expect_frame_refusal("${CONSUMER}" "${content}" "${WORK}/missing-symbol.lbl" "${timeline}"
    "${WORK}/missing-symbol-output" "input-probe BLL or linked probe_result symbol is invalid")
  file(WRITE "${WORK}/duplicate-symbol.lbl" "${label_text}${probe_result_line}")
  expect_frame_refusal("${CONSUMER}" "${content}" "${WORK}/duplicate-symbol.lbl" "${timeline}"
    "${WORK}/duplicate-symbol-output" "input-probe BLL or linked probe_result symbol is invalid")
  string(REGEX REPLACE "al [0-9A-Fa-f]+ [. ]probe_result" "al 10000 .probe_result" out_of_range_labels "${label_text}")
  file(WRITE "${WORK}/out-of-range-symbol.lbl" "${out_of_range_labels}")
  expect_frame_refusal("${CONSUMER}" "${content}" "${WORK}/out-of-range-symbol.lbl" "${timeline}"
    "${WORK}/out-of-range-symbol-output" "input-probe BLL or linked probe_result symbol is invalid")
  file(MAKE_DIRECTORY "${WORK}/output-real-parent")
  file(CREATE_LINK "${WORK}/output-real-parent" "${WORK}/output-link-parent" SYMBOLIC RESULT link_result)
  if(NOT link_result STREQUAL "0")
    message(FATAL_ERROR "frame_driver: could not create symlink-parent control: ${link_result}")
  endif()
  expect_frame_refusal("${CONSUMER}" "${content}" "${labels}" "${timeline}"
    "${WORK}/output-link-parent/symlink-output" "symbolic-link parent")
  file(MAKE_DIRECTORY "${WORK}/existing-output")
  file(WRITE "${WORK}/existing-output/outside.marker" "existing caller output\n")
  file(SHA256 "${WORK}/existing-output/outside.marker" existing_marker_before)
  expect_existing_destination_refusal("${CONSUMER}" "${content}" "${labels}" "${timeline}"
    "${WORK}/existing-output" "output directory already exists")
  file(CREATE_LINK "${WORK}/existing-output" "${WORK}/final-output-symlink" SYMBOLIC RESULT final_link_result)
  if(NOT final_link_result STREQUAL "0")
    message(FATAL_ERROR "frame_driver: could not create final-output symlink: ${final_link_result}")
  endif()
  expect_existing_destination_refusal("${CONSUMER}" "${content}" "${labels}" "${timeline}"
    "${WORK}/final-output-symlink" "output directory already exists")
  file(SHA256 "${WORK}/existing-output/outside.marker" existing_marker_after)
  if(NOT existing_marker_before STREQUAL existing_marker_after)
    message(FATAL_ERROR "frame_driver: output refusal modified existing caller content")
  endif()
  file(WRITE "${WORK}/identity.timeline" "LYNX-INPUT 1\n108544 16\n")
  file(SHA256 "${WORK}/identity.timeline" identity_timeline_sha)
  file(SHA256 "${content}" identity_content_sha)
  run_replay("${content}" "${labels}" "${WORK}/identity.timeline"
    "${WORK}/identity-positive" replay_restore 1 none FALSE)
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/identity-positive"
      replay_restore "${identity_timeline_sha}" 1 none
      0000000000000000000000000000000000000000000000000000000000000000
    TIMEOUT 30 RESULT_VARIABLE content_id_rc OUTPUT_VARIABLE content_id_out ERROR_VARIABLE content_id_err)
  if(NOT content_id_rc STREQUAL "1" OR NOT content_id_out STREQUAL "" OR
     NOT content_id_err MATCHES "content identity differs from the source-built guest")
    message(FATAL_ERROR "frame_driver: wrong source content identity was not rejected specifically\n${content_id_out}${content_id_err}")
  endif()
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/identity-positive"
      replay_restore "${identity_timeline_sha}" 1 lynx1 "${identity_content_sha}"
    TIMEOUT 30 RESULT_VARIABLE profile_id_rc OUTPUT_VARIABLE profile_id_out ERROR_VARIABLE profile_id_err)
  if(NOT profile_id_rc STREQUAL "1" OR NOT profile_id_out STREQUAL "" OR
     NOT profile_id_err MATCHES "profile identity differs from the expected profile")
    message(FATAL_ERROR "frame_driver: wrong profile identity was not rejected specifically\n${profile_id_out}${profile_id_err}")
  endif()
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${WORK}/identity-positive"
      replay_restore
      0000000000000000000000000000000000000000000000000000000000000000
      1 none "${identity_content_sha}"
    TIMEOUT 30 RESULT_VARIABLE script_id_rc OUTPUT_VARIABLE script_id_out ERROR_VARIABLE script_id_err)
  if(NOT script_id_rc STREQUAL "1" OR NOT script_id_out STREQUAL "" OR
     NOT script_id_err MATCHES "input identity differs from the canonical timeline")
    message(FATAL_ERROR "frame_driver: wrong replay script identity was not rejected specifically\n${script_id_out}${script_id_err}")
  endif()
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames-identity "${WORK}/identity-positive"
      replay_restore "${identity_timeline_sha}" 1 none "${identity_content_sha}" video-policy2-unverified
    TIMEOUT 30 RESULT_VARIABLE policy_id_rc OUTPUT_VARIABLE policy_id_out ERROR_VARIABLE policy_id_err)
  if(NOT policy_id_rc STREQUAL "1" OR NOT policy_id_out STREQUAL "" OR
     NOT policy_id_err MATCHES "timing-policy identity differs from the expected policy")
    message(FATAL_ERROR "frame_driver: wrong timing-policy identity was not rejected specifically\n${policy_id_out}${policy_id_err}")
  endif()
  file(COPY "${WORK}/identity-positive/" DESTINATION "${WORK}/partial-row")
  execute_process(COMMAND "${ARTIFACT_CHECKER}" frames-partial-negative "${WORK}/partial-row"
      replay_restore "${identity_timeline_sha}" 1 none "${identity_content_sha}"
    TIMEOUT 30 RESULT_VARIABLE partial_rc OUTPUT_VARIABLE partial_out ERROR_VARIABLE partial_err)
  if(NOT partial_rc STREQUAL "0" OR NOT partial_out MATCHES "rejected truncated raw frame row metadata")
    message(FATAL_ERROR "frame_driver: invalid partial-row metadata did not have its own rejection\n${partial_out}${partial_err}")
  endif()
  set(restore_output "${WORK}/wrong-restore-identity")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "LYNX_TEST_REPLAY_RESTORE=1" "LYNX_TEST_REPLAY_RESTORE_BAD_SCRIPT=1" "${TEST_CONSUMER}" --mode frames
      --content "${content}" --symbols "${labels}" --timeline "${WORK}/identity.timeline"
      --out "${restore_output}" --profile none
    TIMEOUT 60 RESULT_VARIABLE restore_rc OUTPUT_VARIABLE restore_out ERROR_VARIABLE restore_err)
  if(NOT restore_rc STREQUAL "1" OR NOT restore_out STREQUAL "" OR EXISTS "${restore_output}" OR
     NOT restore_err MATCHES "pre-edge checkpoint restore failed script digest")
    message(FATAL_ERROR "frame_driver: wrong restore script identity was not rejected before publication\n${restore_out}${restore_err}")
  endif()
  message(STATUS "replay_controls: successful source build followed by canonical bounds, symbols, and alias refusals")
  return()
endif()
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
      "LYNX_TEST_REPLAY_LEGACY=1" "${TEST_CONSUMER}" --mode frames
      --content "${WORK}/input_probe.bll" --symbols "${WORK}/input_probe.lbl"
      --timeline "${WORK}/input.timeline" --out "${WORK}/trace"
    TIMEOUT 60 RESULT_VARIABLE consumer_rc OUTPUT_VARIABLE consumer_out ERROR_VARIABLE consumer_err)
elseif(CASE STREQUAL "replay_restore")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "LYNX_TEST_REPLAY_RESTORE=1" "${TEST_CONSUMER}" --mode frames
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
if(CASE STREQUAL "replay_restore" AND
   NOT consumer_out MATCHES "lynx-consume-test: pre/post-edge checkpoint restore checks passed")
  message(FATAL_ERROR "frame_driver: test consumer did not confirm both checkpoint restore checks\n${consumer_out}${consumer_err}")
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
if(CASE STREQUAL "frame_tracer")
  # The historical tracer stays on the test-only legacy consumer above. Prove
  # that the production binary follows the normal replay path even when each
  # test-only replay environment variable is deliberately present.
  file(WRITE "${WORK}/production.timeline"
    "LYNX-INPUT 1\n0 16\n54271 0\n54272 16\n54273 0\n162816 16\n")
  file(SHA256 "${WORK}/production.timeline" production_timeline_sha)
  set(production_guest "${WORK}/production-guest")
  file(MAKE_DIRECTORY "${production_guest}")
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "CC65_HOME=${ROOT}/build/pins/cc65" "${CA65}" --cpu 65SC02 -DLEFTHAND=1
      -o "${production_guest}/input_probe.o" "${SOURCE}"
    WORKING_DIRECTORY "${production_guest}" TIMEOUT 30 RESULT_VARIABLE production_ca_rc
    OUTPUT_VARIABLE production_ca_out ERROR_VARIABLE production_ca_err)
  if(NOT production_ca_rc STREQUAL "0")
    message(FATAL_ERROR "frame_driver: pinned ca65 failed to build the production replay control\n${production_ca_out}${production_ca_err}")
  endif()
  execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin"
      "CC65_HOME=${ROOT}/build/pins/cc65" "${LD65}" -C "${CONFIG}"
      -Ln "${production_guest}/input_probe.lbl" -o "${production_guest}/input_probe.bll"
      "${production_guest}/input_probe.o"
    WORKING_DIRECTORY "${production_guest}" TIMEOUT 30 RESULT_VARIABLE production_ld_rc
    OUTPUT_VARIABLE production_ld_out ERROR_VARIABLE production_ld_err)
  if(NOT production_ld_rc STREQUAL "0" OR
     NOT EXISTS "${production_guest}/input_probe.bll" OR
     NOT EXISTS "${production_guest}/input_probe.lbl")
    message(FATAL_ERROR "frame_driver: pinned ld65 failed to build the production replay control\n${production_ld_out}${production_ld_err}")
  endif()
  file(SHA256 "${production_guest}/input_probe.bll" production_guest_sha)
  file(SIZE "${production_guest}/input_probe.bll" production_guest_bytes)
  file(STRINGS "${MANIFEST}" production_manifest_sha
    REGEX "^source_built_guest_sha256_lefthand_1 = ")
  file(STRINGS "${MANIFEST}" production_manifest_bytes
    REGEX "^source_built_guest_bytes_lefthand_1 = ")
  if(NOT production_manifest_sha STREQUAL
      "source_built_guest_sha256_lefthand_1 = ${production_guest_sha}" OR
     NOT production_manifest_bytes STREQUAL
      "source_built_guest_bytes_lefthand_1 = ${production_guest_bytes}")
    message(FATAL_ERROR "frame_driver: production replay control guest differs from its fixture manifest")
  endif()

  set(production_default "${WORK}/production-default")
  foreach(run IN ITEMS default legacy interleaved restore)
    set(production_output "${WORK}/production-${run}")
    set(production_env)
    if(run STREQUAL "legacy")
      list(APPEND production_env "LYNX_TEST_REPLAY_LEGACY=1")
    elseif(run STREQUAL "interleaved")
      list(APPEND production_env "LYNX_TEST_REPLAY_INTERLEAVED=1")
    elseif(run STREQUAL "restore")
      list(APPEND production_env "LYNX_TEST_REPLAY_RESTORE=1")
    endif()
    execute_process(COMMAND "${ENV_EXECUTABLE}" -i "PATH=/usr/bin:/bin" ${production_env}
        "${CONSUMER}" --mode frames --content "${production_guest}/input_probe.bll"
        --symbols "${production_guest}/input_probe.lbl"
        --timeline "${WORK}/production.timeline" --out "${production_output}" --profile none
      TIMEOUT 60 RESULT_VARIABLE production_rc OUTPUT_VARIABLE production_out
      ERROR_VARIABLE production_err)
    if(NOT production_rc STREQUAL "0")
      message(FATAL_ERROR "frame_driver: production replay ${run} run failed (${production_rc})\n${production_out}${production_err}")
    endif()
    execute_process(COMMAND "${ARTIFACT_CHECKER}" frames "${production_output}" replay_edges
        "${production_timeline_sha}" 1 none "${production_guest_sha}"
      TIMEOUT 30 RESULT_VARIABLE production_check_rc OUTPUT_VARIABLE production_check_out
      ERROR_VARIABLE production_check_err)
    if(NOT production_check_rc STREQUAL "0")
      message(FATAL_ERROR "frame_driver: production replay ${run} output failed independent frame checks\n${production_check_out}${production_check_err}")
    endif()
  endforeach()
  file(STRINGS "${production_default}/frame-trace.manifest" production_frame_count_line
    REGEX "^frame_count = ")
  if(NOT production_frame_count_line MATCHES "^frame_count = ([0-9]+)$")
    message(FATAL_ERROR "frame_driver: production replay manifest has no bounded frame_count")
  endif()
  set(production_frame_count "${CMAKE_MATCH_1}")
  if(NOT production_frame_count STREQUAL "4")
    message(FATAL_ERROR "frame_driver: production replay did not emit the four frames required by replay_edges")
  endif()
  file(SHA256 "${production_default}/frame-trace.manifest" production_default_manifest_sha)
  foreach(run IN ITEMS legacy interleaved restore)
    set(production_override "${WORK}/production-${run}")
    file(SHA256 "${production_override}/frame-trace.manifest" production_override_manifest_sha)
    if(NOT production_default_manifest_sha STREQUAL production_override_manifest_sha)
      message(FATAL_ERROR "frame_driver: production ${run} override changed the replay manifest")
    endif()
    compare_replay_outputs("${production_default}" "${production_override}"
      "${production_frame_count}" TRUE)
  endforeach()
  message(STATUS "consumer.frame_tracer: legacy assertions ran through the test consumer; production replay ignored all three LYNX_TEST_REPLAY_* variables")
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
