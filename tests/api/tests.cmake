add_executable(lynx-test-api ${CMAKE_CURRENT_LIST_DIR}/test_api.c)
target_link_libraries(lynx-test-api PRIVATE lynx_bundle)
# The white-box checks read the instance layout, so the tests see core/internal.h.
target_include_directories(lynx-test-api PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-api)
lynx_sanitize(lynx-test-api)
set_target_properties(lynx-test-api PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

lynx_add_test(NAME api.advance_budget COMMAND lynx-test-api advance_budget LABELS api TIMEOUT 30)
lynx_add_test(NAME api.advance_slicing COMMAND lynx-test-api advance_slicing LABELS api TIMEOUT 60)
lynx_add_test(NAME api.bll_alias COMMAND lynx-test-api bll_alias LABELS api TIMEOUT 30)
lynx_add_test(NAME api.bll_identity COMMAND lynx-test-api bll_identity LABELS api TIMEOUT 30)
lynx_add_test(NAME api.bll_load COMMAND lynx-test-api bll_load LABELS api TIMEOUT 30)
lynx_add_test(NAME api.bll_tracer COMMAND lynx-test-api bll_tracer LABELS api TIMEOUT 30)
# Success criterion 3: a non-default per-access cost (the internal test_skew model,
# set through lynx__machine_set_cost_model) changes consumed ticks by exactly the
# sum over the seam bus log of (price - 5) and changes no register or RAM byte.
lynx_add_test(NAME api.cost_model COMMAND lynx-test-api cost_model LABELS api TIMEOUT 30)
# Persistent control: the same case with the machine left on flat5 where the test
# expects test_skew (what a timing function that ignores its table produces) must
# fail, and for that reason.
lynx_add_test(NAME api.cost_model.self_test
  COMMAND ${CMAKE_COMMAND}
    "-DCMD=$<TARGET_FILE:lynx-test-api>$<SEMICOLON>cost_model$<SEMICOLON>fault-constant"
    -DCODE=1 "-DREGEX=cost model ignored"
    -P ${PROJECT_SOURCE_DIR}/tests/cmake/expect_fail.cmake
  LABELS api TIMEOUT 30)
lynx_add_test(NAME api.cpu_program COMMAND lynx-test-api cpu_program LABELS api TIMEOUT 30)
lynx_add_test(NAME api.build_id COMMAND lynx-test-api build_id LABELS api TIMEOUT 30)
# The identity follows the checkout (commit, staged change, linked worktree)
# across a plain build; the control builds a tiny project in a temporary git
# repository outside the checkout and includes the real cmake/BuildId.cmake.
find_program(LYNX_SH sh REQUIRED)
lynx_add_test(NAME api.build_id.refresh
  COMMAND ${LYNX_SH} ${CMAKE_CURRENT_LIST_DIR}/build_id_refresh.sh ${PROJECT_SOURCE_DIR}
  LABELS api TIMEOUT 120)
lynx_add_test(NAME api.identity COMMAND lynx-test-api identity LABELS api TIMEOUT 30)
lynx_add_test(NAME api.init_refusal COMMAND lynx-test-api init_refusal LABELS api TIMEOUT 30)
lynx_add_test(NAME api.inspect_pure COMMAND lynx-test-api inspect_pure LABELS api TIMEOUT 30)
lynx_add_test(NAME api.instances_independent COMMAND lynx-test-api instances_independent LABELS api TIMEOUT 30)
lynx_add_test(NAME api.irq_pending COMMAND lynx-test-api irq_pending LABELS api TIMEOUT 30)
lynx_add_test(NAME api.irq_poll COMMAND lynx-test-api irq_poll LABELS api TIMEOUT 30)
lynx_add_test(NAME api.load_refusal_unchanged COMMAND lynx-test-api load_refusal_unchanged LABELS api TIMEOUT 30)
lynx_add_test(NAME api.profile_select COMMAND lynx-test-api profile_select LABELS api TIMEOUT 30)
lynx_add_test(NAME api.request_boundary COMMAND lynx-test-api request_boundary LABELS api TIMEOUT 30)
lynx_add_test(NAME api.request_hold COMMAND lynx-test-api request_hold LABELS api TIMEOUT 30)
lynx_add_test(NAME api.request_wake COMMAND lynx-test-api request_wake LABELS api TIMEOUT 30)
lynx_add_test(NAME cpu.profile_divergence COMMAND lynx-test-api profile_divergence LABELS cpu TIMEOUT 30)
lynx_add_test(NAME api.reset_power_on COMMAND lynx-test-api reset_power_on LABELS api TIMEOUT 30)
lynx_add_test(NAME api.scheduler_limits COMMAND lynx-test-api scheduler_limits LABELS api TIMEOUT 30)
lynx_add_test(NAME api.scheduler_order COMMAND lynx-test-api scheduler_order LABELS api TIMEOUT 30)
lynx_add_test(NAME api.scheduler_watch COMMAND lynx-test-api scheduler_watch LABELS api TIMEOUT 30)
lynx_add_test(NAME api.snapshot_refusal COMMAND lynx-test-api snapshot_refusal LABELS api TIMEOUT 60)
lynx_add_test(NAME api.snapshot_roundtrip COMMAND lynx-test-api snapshot_roundtrip LABELS api TIMEOUT 30)
lynx_add_test(NAME api.snapshot_continue COMMAND lynx-test-api snapshot_continue LABELS api TIMEOUT 60)
lynx_add_test(NAME api.timer_snapshot COMMAND lynx-test-api timer_snapshot LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_phase COMMAND lynx-test-api timer_phase LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_standalone COMMAND lynx-test-api timer_standalone LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_tracer COMMAND lynx-test-api timer_tracer LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_done_borrow COMMAND lynx-test-api timer_done_borrow LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_link_tracer COMMAND lynx-test-api timer_link_tracer LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_links COMMAND lynx-test-api timer_links LABELS api TIMEOUT 30)
lynx_add_test(NAME api.timer_unavailable COMMAND lynx-test-api timer_unavailable LABELS api TIMEOUT 30)
lynx_add_test(NAME api.watch_controls COMMAND lynx-test-api watch_controls LABELS api TIMEOUT 30)
lynx_add_test(NAME api.watch_independent COMMAND lynx-test-api watch_independent LABELS api TIMEOUT 30)
lynx_add_test(NAME api.watch_post_write COMMAND lynx-test-api watch_post_write LABELS api TIMEOUT 30)
lynx_add_test(NAME core.sha256 COMMAND lynx-test-api sha256 LABELS core TIMEOUT 30)

# One persistent control per public contract groups its isolated fault cases.
# The generated driver requires each child to exit 1 and match its intended
# assertion, so usage errors and unrelated failures cannot pass the control.
set(API_FAULT_BATCH "${PROJECT_BINARY_DIR}/tests/api_fault_batch.cmake")
file(GENERATE OUTPUT "${API_FAULT_BATCH}" CONTENT [==[
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var TEST_BINARY FAULT_GROUP)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "api fault control: -D${var} is required")
  endif()
endforeach()
if(FAULT_GROUP STREQUAL "map")
  set(positive_case map_matrix)
  set(fault_cases
    "map_matrix|fault-wrong-map-bit|stop == LYNX_STOP_BUDGET failed"
    "map_matrix|fault-special-address-order|stop == LYNX_STOP_UNAVAILABLE failed")
elseif(FAULT_GROUP STREQUAL "peek")
  set(positive_case memory_peek_pure)
  set(fault_cases
    "memory_peek_pure|fault-raw-as-mapped|raw_status == LYNX_OK failed")
elseif(FAULT_GROUP STREQUAL "rom")
  set(positive_case rom_unavailable)
  set(fault_cases
    "rom_unavailable|fault-fallback-to-ram|stop == LYNX_STOP_UNAVAILABLE failed")
elseif(FAULT_GROUP STREQUAL "watch")
  set(positive_case watch_controls)
  set(fault_cases
    "watch_post_write|fault-stop-before-store|watch returned before store committed"
    "watch_controls|fault-repeat-store|watch continuation replayed first store before counter increment")
elseif(FAULT_GROUP STREQUAL "snapshot")
  set(positive_case snapshot_continue)
  set(fault_cases
    "snapshot_continue|fault-watch-in-digest|host watch state leaked into bundle observation digest")
else()
  message(FATAL_ERROR "api fault control: unknown FAULT_GROUP ${FAULT_GROUP}")
endif()
execute_process(COMMAND "${TEST_BINARY}" "${positive_case}"
  RESULT_VARIABLE positive_result OUTPUT_VARIABLE positive_out ERROR_VARIABLE positive_err)
if(NOT "${positive_result}" STREQUAL "0")
  message(FATAL_ERROR "${positive_case}: accepted control failed with exit ${positive_result}\n${positive_out}${positive_err}")
endif()
foreach(spec IN LISTS fault_cases)
  string(REPLACE "|" ";" fields "${spec}")
  list(GET fields 0 case_name)
  list(GET fields 1 fault_name)
  list(GET fields 2 expected_assertion)
  execute_process(COMMAND "${TEST_BINARY}" "${case_name}" "${fault_name}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(output "${out}${err}")
  if(NOT "${result}" STREQUAL "1" OR NOT output MATCHES "${expected_assertion}")
    message(FATAL_ERROR "${case_name} ${fault_name}: expected exit 1 and assertion '${expected_assertion}', got exit ${result}\n${output}")
  endif()
endforeach()
message(STATUS "api ${FAULT_GROUP} fault subcases failed at their intended assertions")
]==])

# 03-04 already implemented these public-path controls; they become permanent
# inventory gates here after their route mutants are proven in the failing direction.
lynx_add_test(NAME memory.map_matrix COMMAND lynx-test-api map_matrix LABELS memory TIMEOUT 60)
lynx_add_test(NAME memory.map_matrix.self_test
  COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:lynx-test-api>" -DFAULT_GROUP=map
    -P "${API_FAULT_BATCH}"
  LABELS memory TIMEOUT 30)
lynx_add_test(NAME api.memory_peek_pure COMMAND lynx-test-api memory_peek_pure LABELS api TIMEOUT 30)
lynx_add_test(NAME api.memory_peek_pure.self_test
  COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:lynx-test-api>" -DFAULT_GROUP=peek
    -P "${API_FAULT_BATCH}"
  LABELS api TIMEOUT 30)
lynx_add_test(NAME memory.rom_unavailable COMMAND lynx-test-api rom_unavailable LABELS memory TIMEOUT 30)
lynx_add_test(NAME memory.rom_unavailable.self_test
  COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:lynx-test-api>" -DFAULT_GROUP=rom
    -P "${API_FAULT_BATCH}"
  LABELS memory TIMEOUT 30)
lynx_add_test(NAME api.snapshot_continue.self_test
  COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:lynx-test-api>" -DFAULT_GROUP=snapshot
    -P "${API_FAULT_BATCH}"
  LABELS api TIMEOUT 30)
lynx_add_test(NAME api.watch_controls.self_test
  COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:lynx-test-api>" -DFAULT_GROUP=watch
    -P "${API_FAULT_BATCH}"
  LABELS api TIMEOUT 30)
