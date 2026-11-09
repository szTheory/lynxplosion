add_executable(lynx-test-api ${CMAKE_CURRENT_LIST_DIR}/test_api.c)
target_link_libraries(lynx-test-api PRIVATE lynx_core)
# The white-box checks read the instance layout, so the tests see core/internal.h.
target_include_directories(lynx-test-api PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-api)
lynx_sanitize(lynx-test-api)
set_target_properties(lynx-test-api PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

add_test(NAME api.skeleton COMMAND lynx-test-api skeleton)
