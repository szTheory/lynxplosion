cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(required SOURCE_DIR BUILD_DIR API_TEST_BINARY C_COMPILER FAULT_ID)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "timer fault control: -D${required} is required")
  endif()
endforeach()

if(FAULT_ID STREQUAL "done")
  set(API_CASE "timer_done_gate")
  set(API_TAP_CASE "timer_done_gate")
  set(MUTANT_SOURCE "core/timer.c")
  set(anchor [=[    return (timer->control_a & TIMER_CONTROL_A_COUNT) != 0u &&
           ((timer->control_b & TIMER_CONTROL_B_DONE) == 0u ||
            (timer->control_a & (TIMER_CONTROL_A_RELOAD | TIMER_CONTROL_A_RESET_DONE)) != 0u);]=])
  set(replacement [=[    return (timer->control_a & TIMER_CONTROL_A_COUNT) != 0u;]=])
  set(assertion_id "timer.done-gates-counting")
  set(assertion_pattern "timer\\.done-gates-counting")
elseif(FAULT_ID STREQUAL "reset_done")
  set(API_CASE "timer_reset_done_held")
  set(API_TAP_CASE "timer_reset_done_held")
  set(MUTANT_SOURCE "core/timer.c")
  set(anchor [=[    const bool reset_done = (timer->control_a & TIMER_CONTROL_A_RESET_DONE) != 0u;]=])
  set(replacement [=[    const bool reset_done = false;]=])
  set(assertion_id "timer.reset-done-held-clear")
  set(assertion_pattern "timer\\.reset-done-held-clear")
elseif(FAULT_ID STREQUAL "phase")
  set(API_CASE "timer_shared_phase")
  set(API_TAP_CASE "timer_shared_phase")
  set(MUTANT_SOURCE "core/timer.c")
  set(anchor [=[        const uint64_t phase = now % period;]=])
  set(replacement [=[        const uint64_t phase = 0u;]=])
  set(assertion_id "timer.phase.shared-divider-edge")
  set(assertion_pattern "timer\\.phase\\.shared-divider-edge")
elseif(FAULT_ID STREQUAL "order")
  set(API_CASE "scheduler_order")
  set(API_TAP_CASE "scheduler_order")
  set(MUTANT_SOURCE "core/machine.c")
  set(anchor [=[        return a.priority < b.priority;]=])
  set(replacement [=[        return a.priority > b.priority;]=])
  set(assertion_id "timer.order.timer-before-cpu-prefix")
  set(assertion_pattern "timer\\.order\\.timer-before-cpu-prefix")
elseif(FAULT_ID STREQUAL "irq")
  set(API_CASE "timer_pending_after_disable")
  set(API_TAP_CASE "timer_pending_after_disable")
  set(MUTANT_SOURCE "core/machine.c")
  set(anchor [=[        const uint8_t lines = m->timer_bank.pending != 0u ? (uint8_t)LYNX__LINE_IRQ : 0u;]=])
  set(replacement [=[        const uint8_t lines = (m->timer_bank.pending != 0u &&
                               (m->timer_bank.timers[0].control_a & 0x80u) != 0u) ?
                                  (uint8_t)LYNX__LINE_IRQ : 0u;]=])
  set(assertion_id "timer.irq.pending-independent-of-enable")
  set(assertion_pattern "timer\\.irq\\.pending-independent-of-enable")
else()
  message(FATAL_ERROR "timer fault control: unsupported fault ${FAULT_ID}")
endif()

set(scratch "${BUILD_DIR}/scratch/timer-fault-${FAULT_ID}")
set(owner_file "${scratch}/.lynx-fault-owner")
set(owner_value "source=${SOURCE_DIR}\nfault=${FAULT_ID}\n")
if(EXISTS "${scratch}")
  if(NOT EXISTS "${owner_file}")
    message(FATAL_ERROR "timer fault control: refusing unowned scratch ${scratch}")
  endif()
  file(READ "${owner_file}" previous_owner)
  if(NOT previous_owner STREQUAL owner_value)
    message(FATAL_ERROR "timer fault control: refusing scratch owned by another invocation: ${scratch}")
  endif()
  file(REMOVE_RECURSE "${scratch}")
endif()
file(MAKE_DIRECTORY "${scratch}")
file(WRITE "${owner_file}" "${owner_value}")

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

function(run_semantic_fault)
  set(variant_source "${scratch}/src")
  set(variant_build "${scratch}/build")
  file(MAKE_DIRECTORY "${variant_source}" "${variant_source}/runner"
       "${variant_source}/tests/api" "${variant_source}/tests/cpu")
  file(COPY "${SOURCE_DIR}/core" DESTINATION "${variant_source}")
  file(COPY "${SOURCE_DIR}/include" DESTINATION "${variant_source}")
  foreach(relative IN ITEMS
      runner/bundle.c
      runner/bundle.h
      tests/api/test_api.c
      tests/check.h
      tests/cpu/flat_bus.h)
    file(COPY_FILE "${SOURCE_DIR}/${relative}" "${variant_source}/${relative}")
  endforeach()
  if(NOT EXISTS "${BUILD_DIR}/generated/build_id.c")
    set(CONTROL_FAILURE "generated build identity is missing" PARENT_SCOPE)
    return()
  endif()
  file(COPY_FILE "${BUILD_DIR}/generated/build_id.c"
       "${variant_source}/core/build_id.c")

  set(mutated_file "${variant_source}/${MUTANT_SOURCE}")
  file(READ "${mutated_file}" source_before)
  file(SHA256 "${SOURCE_DIR}/${MUTANT_SOURCE}" clean_source_sha256)
  file(SHA256 "${mutated_file}" copied_source_sha256)
  if(NOT copied_source_sha256 STREQUAL clean_source_sha256)
    set(CONTROL_FAILURE "clean copied source identity differs before mutation" PARENT_SCOPE)
    return()
  endif()
  count_literal_occurrences("${source_before}" "${anchor}" anchor_count)
  if(NOT anchor_count EQUAL 1)
    set(CONTROL_FAILURE "fault ${FAULT_ID} anchor count was ${anchor_count}, expected 1" PARENT_SCOPE)
    return()
  endif()
  string(REPLACE "${anchor}" "${replacement}" source_mutated "${source_before}")
  if(source_mutated STREQUAL source_before)
    set(CONTROL_FAILURE "fault ${FAULT_ID} left ${MUTANT_SOURCE} unchanged" PARENT_SCOPE)
    return()
  endif()
  file(WRITE "${mutated_file}" "${source_mutated}")
  file(SHA256 "${mutated_file}" mutant_source_sha256)
  if(mutant_source_sha256 STREQUAL clean_source_sha256)
    set(CONTROL_FAILURE "mutant source identity did not change" PARENT_SCOPE)
    return()
  endif()

  file(WRITE "${variant_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(lynx_timer_fault LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_executable(lynx-test-api
  tests/api/test_api.c
  core/audio.c core/bus.c core/cpu.c core/cpu_profile.c core/input.c core/machine.c
  core/sha256.c core/snapshot.c core/suzy.c core/timer.c core/video.c core/build_id.c
  runner/bundle.c)
target_include_directories(lynx-test-api PRIVATE include core runner tests tests/cpu)
target_compile_definitions(lynx-test-api PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(lynx-test-api PRIVATE
  -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror
  -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
  -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)
target_link_options(lynx-test-api PRIVATE -fsanitize=address,undefined)
]=])

  string(TIMESTAMP started "%s" UTC)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${variant_source}" -B "${variant_build}" -G Ninja
      "-DCMAKE_C_COMPILER=${C_COMPILER}"
    RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err TIMEOUT 60)
  if(NOT "${configure_result}" STREQUAL "0")
    set(CONTROL_FAILURE "mutant configure failed (not counted as detection): ${configure_result}\n${configure_out}${configure_err}" PARENT_SCOPE)
    return()
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${variant_build}" --target lynx-test-api
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err TIMEOUT 120)
  if(NOT "${build_result}" STREQUAL "0")
    set(CONTROL_FAILURE "mutant compile/link failed (not counted as detection): ${build_result}\n${build_out}${build_err}" PARENT_SCOPE)
    return()
  endif()
  string(TIMESTAMP built "%s" UTC)
  math(EXPR build_seconds "${built} - ${started}")

  execute_process(
    COMMAND "${variant_build}/lynx-test-api" "${API_CASE}"
    RESULT_VARIABLE mutant_result OUTPUT_VARIABLE mutant_out ERROR_VARIABLE mutant_err
    TIMEOUT 20)
  set(mutant_output "${mutant_out}${mutant_err}")
  if("${mutant_result}" STREQUAL "Process terminated due to timeout" OR
     mutant_output MATCHES "AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:")
    set(CONTROL_FAILURE "mutant runtime timeout/sanitizer failure (not counted as detection): ${mutant_result}\n${mutant_output}" PARENT_SCOPE)
    return()
  endif()
  if(NOT "${mutant_result}" STREQUAL "1" OR
     NOT mutant_output MATCHES "ASSERT ${assertion_pattern} failed" OR
     NOT mutant_output MATCHES "not ok 1 - api\\.${API_TAP_CASE}")
    set(CONTROL_FAILURE "mutant did not fail the intended assertion with exit 1: exit=${mutant_result}\n${mutant_output}" PARENT_SCOPE)
    return()
  endif()
  file(SHA256 "${variant_build}/lynx-test-api" mutant_binary_sha256)
  set(CONTROL_RESULT "fault=${FAULT_ID} assertion=${assertion_id} exit=1 clean_source=${clean_source_sha256} mutant_source=${mutant_source_sha256} binary=${mutant_binary_sha256} build_seconds=${build_seconds}" PARENT_SCOPE)
  set(CONTROL_FAILURE "" PARENT_SCOPE)
endfunction()

function(run_clean_case stage)
  execute_process(
    COMMAND "${API_TEST_BINARY}" "${API_CASE}"
    RESULT_VARIABLE positive_result OUTPUT_VARIABLE positive_out ERROR_VARIABLE positive_err
    TIMEOUT 20)
  set(positive_output "${positive_out}${positive_err}")
  if(NOT "${positive_result}" STREQUAL "0" OR
     NOT positive_output MATCHES "ok 1 - api\\.${API_TAP_CASE}")
    set(CONTROL_FAILURE "${stage} clean API positive failed: exit=${positive_result}\n${positive_output}" PARENT_SCOPE)
    return()
  endif()
  set(CONTROL_FAILURE "" PARENT_SCOPE)
endfunction()

run_clean_case("before")
if(CONTROL_FAILURE STREQUAL "")
  run_semantic_fault()
endif()
if(CONTROL_FAILURE STREQUAL "")
  run_clean_case("after")
endif()

if(EXISTS "${owner_file}")
  file(READ "${owner_file}" final_owner)
  if(final_owner STREQUAL owner_value)
    file(REMOVE_RECURSE "${scratch}")
  else()
    set(CONTROL_FAILURE "scratch ownership changed before cleanup: ${scratch}")
  endif()
endif()
if(NOT CONTROL_FAILURE STREQUAL "")
  message(FATAL_ERROR "timer fault control: ${CONTROL_FAILURE}")
endif()
message(STATUS "timer fault proof: ${CONTROL_RESULT}; clean API positive=${API_CASE} before/after")
