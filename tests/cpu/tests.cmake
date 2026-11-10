# CPU tests: the profile tables and the cycle-stepped core on a flat test bus.
add_executable(lynx-test-cpu ${CMAKE_CURRENT_LIST_DIR}/test_cpu.c)
target_link_libraries(lynx-test-cpu PRIVATE lynx_core)
# The white-box checks read the CPU instance and tables, so the tests see core/.
target_include_directories(lynx-test-cpu PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-cpu)
lynx_sanitize(lynx-test-cpu)
set_target_properties(lynx-test-cpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
