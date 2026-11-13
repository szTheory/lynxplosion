add_executable(lynx-test-suzy ${CMAKE_CURRENT_LIST_DIR}/test_suzy.c)
target_link_libraries(lynx-test-suzy PRIVATE lynx_core)
target_include_directories(lynx-test-suzy PRIVATE ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-suzy)
lynx_sanitize(lynx-test-suzy)
set_target_properties(lynx-test-suzy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

lynx_add_test(NAME suzy.tracer COMMAND lynx-test-suzy tracer LABELS suzy TIMEOUT 60)
