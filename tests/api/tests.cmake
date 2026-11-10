add_executable(lynx-test-api ${CMAKE_CURRENT_LIST_DIR}/test_api.c)
target_link_libraries(lynx-test-api PRIVATE lynx_core)
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
lynx_add_test(NAME api.load_refusal_unchanged COMMAND lynx-test-api load_refusal_unchanged LABELS api TIMEOUT 30)
lynx_add_test(NAME api.profile_select COMMAND lynx-test-api profile_select LABELS api TIMEOUT 30)
lynx_add_test(NAME cpu.profile_divergence COMMAND lynx-test-api profile_divergence LABELS cpu TIMEOUT 30)
lynx_add_test(NAME api.reset_power_on COMMAND lynx-test-api reset_power_on LABELS api TIMEOUT 30)
lynx_add_test(NAME api.snapshot_refusal COMMAND lynx-test-api snapshot_refusal LABELS api TIMEOUT 60)
lynx_add_test(NAME api.snapshot_roundtrip COMMAND lynx-test-api snapshot_roundtrip LABELS api TIMEOUT 30)
lynx_add_test(NAME core.sha256 COMMAND lynx-test-api sha256 LABELS core TIMEOUT 30)
