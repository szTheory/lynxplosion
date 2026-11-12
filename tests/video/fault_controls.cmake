cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(required SOURCE_DIR BUILD_DIR API_TEST_BINARY C_COMPILER FAULT_ID)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "video fault control: -D${required} is required")
  endif()
endforeach()

if(FAULT_ID STREQUAL "dma_cost_zero")
  set(API_CASE "video_dma_cost_fault")
  set(API_TAP_CASE "video_dma_cost_fault")
  set(MUTANT_SOURCE "core/video.c")
  set(anchor [=[    video->dma_busy_until = tick + VIDEO_DMA_TICKS;]=])
  set(replacement [=[    video->dma_busy_until = tick;]=])
  set(assertion_id "video.fault.dma-cost-holds-cpu-until-release")
  set(assertion_pattern "video\\.fault\\.dma-cost-holds-cpu-until-release")
elseif(FAULT_ID STREQUAL "dma_request_omitted")
  set(API_CASE "video_dma_request_fault")
  set(API_TAP_CASE "video_dma_request_fault")
  set(MUTANT_SOURCE "core/video.c")
  set(anchor [=[bool lynx__video_next_dma(const lynx__video_state *video, uint64_t now, uint64_t *tick)
{
    if (video == NULL || tick == NULL ||
        video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        (video->dispctl & 1u) == 0u ||
        video->row_active == 0u || video->dma_cursor >= LYNX_FRAME_ROW_BYTES) {
        return false;
    }
    uint64_t grant = video->dma_next_tick;
    if (grant < video->dma_busy_until) grant = video->dma_busy_until;
    if (grant < video->refresh_busy_until) grant = video->refresh_busy_until;
    if (grant < now) grant = now;
    *tick = grant;
    return true;
}]=])
  set(replacement [=[bool lynx__video_next_dma(const lynx__video_state *video, uint64_t now, uint64_t *tick)
{
    if (video == NULL || tick == NULL ||
        video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        (video->dispctl & 1u) == 0u ||
        video->row_active == 0u || video->dma_cursor >= LYNX_FRAME_ROW_BYTES) {
        return false;
    }
    uint64_t grant = video->dma_next_tick;
    if (grant < video->dma_busy_until) grant = video->dma_busy_until;
    if (grant < video->refresh_busy_until) grant = video->refresh_busy_until;
    if (grant < now) grant = now;
    *tick = grant;
    return false;
}]=])
  set(assertion_id "video.fault.active-row-receives-video-ram-grant")
  set(assertion_pattern "video\\.fault\\.active-row-receives-video-ram-grant")
elseif(FAULT_ID STREQUAL "palette_boundary")
  set(API_CASE "video_palette_fault")
  set(API_TAP_CASE "video_palette_fault")
  set(MUTANT_SOURCE "core/video.c")
  set(anchor [=[    /* MUTATION_ANCHOR: video.palette-row-snapshot */
    memcpy(video->palettes + row * 32u, video->palette, 32u);]=])
  set(replacement [=[    /* MUTATION_ANCHOR: video.palette-row-snapshot */
    if (row != 0u) memcpy(video->palettes + row * 32u, video->palette, 32u);]=])
  set(assertion_id "video.fault.palette-row-boundary-late")
  set(assertion_pattern "video\\.fault\\.palette-row-boundary-late")
else()
  message(FATAL_ERROR "video fault control: unsupported fault '${FAULT_ID}'")
endif()

file(REAL_PATH "${SOURCE_DIR}" source_root)
file(REAL_PATH "${BUILD_DIR}" build_root)
if(NOT IS_DIRECTORY "${source_root}" OR NOT IS_DIRECTORY "${build_root}")
  message(FATAL_ERROR "video fault control: source and build roots must already exist")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef invocation_nonce)
set(scratch "${build_root}/scratch/video-fault-${FAULT_ID}-${invocation_nonce}")
set(owner_file "${scratch}/.lynx-fault-owner")
set(owner_value "source=${source_root}\nfault=${FAULT_ID}\nnonce=${invocation_nonce}\n")
if(EXISTS "${scratch}" OR IS_SYMLINK "${scratch}")
  message(FATAL_ERROR "video fault control: invocation scratch path already exists")
endif()

function(count_literal_occurrences text needle output_var)
  string(LENGTH "${needle}" needle_length)
  set(rest "${text}")
  set(count 0)
  while(TRUE)
    string(FIND "${rest}" "${needle}" position)
    if(position EQUAL -1)
      break()
    endif()
    math(EXPR count "${count} + 1")
    math(EXPR next "${position} + ${needle_length}")
    string(SUBSTRING "${rest}" ${next} -1 rest)
  endwhile()
  set(${output_var} "${count}" PARENT_SCOPE)
endfunction()

function(run_clean_case stage output_failure output_seconds)
  string(TIMESTAMP started "%s" UTC)
  execute_process(
    COMMAND "${API_TEST_BINARY}" "${API_CASE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 20)
  string(TIMESTAMP ended "%s" UTC)
  math(EXPR elapsed "${ended} - ${started}")
  set(output "${out}${err}")
  if(NOT "${result}" STREQUAL "0" OR
     NOT output MATCHES "ok 1 - api\\.${API_TAP_CASE}" OR
     output MATCHES "ASSERT [^\r\n]* failed")
    set(${output_failure} "${stage} fresh clean API positive failed: exit=${result}\n${output}" PARENT_SCOPE)
    set(${output_seconds} "${elapsed}" PARENT_SCOPE)
    return()
  endif()
  set(${output_failure} "" PARENT_SCOPE)
  set(${output_seconds} "${elapsed}" PARENT_SCOPE)
endfunction()

function(run_semantic_fault output_failure output_result)
  file(MAKE_DIRECTORY "${scratch}")
  file(REAL_PATH "${scratch}" scratch_root)
  string(FIND "${scratch_root}/" "${build_root}/" contained_at)
  if(NOT contained_at EQUAL 0 OR scratch_root STREQUAL build_root OR
     IS_SYMLINK "${scratch}" OR IS_SYMLINK "${owner_file}" OR EXISTS "${owner_file}")
    set(${output_failure} "refusing scratch outside build root, via symlink, or with a preexisting owner marker" PARENT_SCOPE)
    return()
  endif()
  file(WRITE "${owner_file}" "${owner_value}")

  set(variant_source "${scratch_root}/src")
  set(variant_build "${scratch_root}/build")
  file(MAKE_DIRECTORY "${variant_source}/runner" "${variant_source}/tests/api"
       "${variant_source}/tests/cpu")
  file(COPY "${source_root}/core" DESTINATION "${variant_source}")
  file(COPY "${source_root}/include" DESTINATION "${variant_source}")
  foreach(relative IN ITEMS
      runner/bundle.c runner/bundle.h tests/api/test_api.c tests/check.h
      tests/cpu/flat_bus.h)
    file(COPY_FILE "${source_root}/${relative}" "${variant_source}/${relative}")
  endforeach()
  if(NOT EXISTS "${build_root}/generated/build_id.c")
    set(${output_failure} "current generated build identity is missing" PARENT_SCOPE)
    return()
  endif()
  file(COPY_FILE "${build_root}/generated/build_id.c"
       "${variant_source}/core/build_id.c")

  set(mutated_file "${variant_source}/${MUTANT_SOURCE}")
  file(READ "${mutated_file}" source_before)
  file(SHA256 "${source_root}/${MUTANT_SOURCE}" clean_source_sha256)
  file(SHA256 "${mutated_file}" copied_source_sha256)
  if(NOT copied_source_sha256 STREQUAL clean_source_sha256)
    set(${output_failure} "clean copied-source hash does not match the live source" PARENT_SCOPE)
    return()
  endif()
  count_literal_occurrences("${source_before}" "${anchor}" anchor_count)
  if(NOT anchor_count EQUAL 1)
    set(${output_failure} "${MUTANT_SOURCE} anchor matched ${anchor_count} times; exactly one is required" PARENT_SCOPE)
    return()
  endif()
  string(REPLACE "${anchor}" "${replacement}" source_mutated "${source_before}")
  if(source_mutated STREQUAL source_before)
    set(${output_failure} "${MUTANT_SOURCE} mutation did not change copied source" PARENT_SCOPE)
    return()
  endif()
  file(WRITE "${mutated_file}" "${source_mutated}")
  file(SHA256 "${mutated_file}" mutant_source_sha256)
  if(mutant_source_sha256 STREQUAL clean_source_sha256)
    set(${output_failure} "mutated source hash did not change" PARENT_SCOPE)
    return()
  endif()

  file(WRITE "${variant_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(lynx_video_fault LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_executable(lynx-test-api
  tests/api/test_api.c
  core/bus.c core/cpu.c core/cpu_profile.c core/input.c core/machine.c
  core/sha256.c core/snapshot.c core/timer.c core/video.c core/build_id.c
  runner/bundle.c)
target_include_directories(lynx-test-api PRIVATE include core runner tests tests/cpu)
target_compile_definitions(lynx-test-api PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(lynx-test-api PRIVATE
  -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror
  -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
  -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
  -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=build)
target_link_options(lynx-test-api PRIVATE -fsanitize=address,undefined)
]=])

  file(SHA256 "${variant_source}/CMakeLists.txt" mutant_config_sha256)
  file(SHA256 "${variant_source}/core/build_id.c" build_id_sha256)
  string(TIMESTAMP configure_started "%s" UTC)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${variant_source}" -B "${variant_build}" -G Ninja
      "-DCMAKE_C_COMPILER=${C_COMPILER}"
    RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err TIMEOUT 60)
  if(NOT "${configure_result}" STREQUAL "0")
    set(${output_failure} "mutant configure failed; not semantic detection: ${configure_result}\n${configure_out}${configure_err}" PARENT_SCOPE)
    return()
  endif()
  string(TIMESTAMP configure_ended "%s" UTC)
  math(EXPR configure_seconds "${configure_ended} - ${configure_started}")

  string(TIMESTAMP build_started "%s" UTC)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${variant_build}" --target lynx-test-api
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err
    TIMEOUT 120)
  if(NOT "${build_result}" STREQUAL "0")
    set(${output_failure} "mutant compile/link failed; not semantic detection: ${build_result}\n${build_out}${build_err}" PARENT_SCOPE)
    return()
  endif()
  string(TIMESTAMP build_ended "%s" UTC)
  math(EXPR build_seconds "${build_ended} - ${build_started}")

  string(TIMESTAMP child_started "%s" UTC)
  execute_process(
    COMMAND "${variant_build}/lynx-test-api" "${API_CASE}"
    RESULT_VARIABLE mutant_result OUTPUT_VARIABLE mutant_out ERROR_VARIABLE mutant_err
    TIMEOUT 20)
  string(TIMESTAMP child_ended "%s" UTC)
  math(EXPR child_seconds "${child_ended} - ${child_started}")
  set(mutant_output "${mutant_out}${mutant_err}")
  string(REGEX MATCHALL "ASSERT [^\r\n]* failed" mutant_assertions "${mutant_output}")
  list(LENGTH mutant_assertions assertion_count)
  if("${mutant_result}" STREQUAL "Process terminated due to timeout" OR
     mutant_output MATCHES "AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:")
    set(${output_failure} "mutant runtime timeout/sanitizer fault; not semantic detection: exit=${mutant_result}\n${mutant_output}" PARENT_SCOPE)
    return()
  endif()
  if(NOT "${mutant_result}" STREQUAL "1" OR assertion_count GREATER 1 OR
     NOT mutant_output MATCHES "ASSERT ${assertion_pattern} failed" OR
     NOT mutant_output MATCHES "not ok 1 - api\\.${API_TAP_CASE}")
    set(${output_failure} "mutant did not fail exactly its intended assertion with exit 1: exit=${mutant_result}\n${mutant_output}" PARENT_SCOPE)
    return()
  endif()

  file(SHA256 "${variant_build}/lynx-test-api" mutant_binary_sha256)
  set(${output_result}
      "fault=${FAULT_ID} case=api.${API_TAP_CASE} assertion=${assertion_id} exit=1 profile=none policy=video_policy1 clean_source=${clean_source_sha256} mutant_source=${mutant_source_sha256} build_id=${build_id_sha256} config=${mutant_config_sha256} binary=${mutant_binary_sha256} configure_s=${configure_seconds} build_s=${build_seconds} child_s=${child_seconds} nonce=${invocation_nonce}"
      PARENT_SCOPE)
  set(${output_failure} "" PARENT_SCOPE)
endfunction()

run_clean_case("before mutation" failure_before positive_before_seconds)
if(NOT "${failure_before}" STREQUAL "")
  message(FATAL_ERROR "video fault control: ${failure_before}")
endif()
run_semantic_fault(failure_mutant mutant_result)
if(NOT "${failure_mutant}" STREQUAL "")
  message(FATAL_ERROR "video fault control: ${failure_mutant}; owned diagnostic scratch retained at ${scratch}")
endif()
run_clean_case("after mutation" failure_after positive_after_seconds)
if(NOT "${failure_after}" STREQUAL "")
  message(FATAL_ERROR "video fault control: ${failure_after}; owned diagnostic scratch retained at ${scratch}")
endif()

file(REAL_PATH "${scratch}" final_scratch_root)
string(FIND "${final_scratch_root}/" "${build_root}/" final_contained_at)
if(NOT final_contained_at EQUAL 0 OR final_scratch_root STREQUAL build_root OR
   IS_SYMLINK "${scratch}" OR IS_SYMLINK "${owner_file}" OR NOT EXISTS "${owner_file}")
  message(FATAL_ERROR "video fault control: scratch containment/owner proof failed; retained at ${scratch}")
endif()
file(READ "${owner_file}" final_owner)
if(NOT final_owner STREQUAL owner_value)
  message(FATAL_ERROR "video fault control: owner nonce/source/fault changed; retained at ${scratch}")
endif()
file(REMOVE_RECURSE "${final_scratch_root}")
if(EXISTS "${scratch}" OR IS_SYMLINK "${scratch}")
  message(FATAL_ERROR "video fault control: verified scratch cleanup did not complete")
endif()
message(STATUS "video fault proof: ${mutant_result}; clean positives=${positive_before_seconds}s/${positive_after_seconds}s; scratch=removed-after-owner-check")
