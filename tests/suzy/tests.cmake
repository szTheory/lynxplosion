add_executable(lynx-test-suzy ${CMAKE_CURRENT_LIST_DIR}/test_suzy.c)
target_link_libraries(lynx-test-suzy PRIVATE lynx_core)
target_include_directories(lynx-test-suzy PRIVATE ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/core)
lynx_warnings(lynx-test-suzy)
lynx_sanitize(lynx-test-suzy)
set_target_properties(lynx-test-suzy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

lynx_add_test(NAME suzy.tracer COMMAND lynx-test-suzy tracer LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.snapshot_tracer COMMAND lynx-test-suzy snapshot_tracer LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.arithmetic COMMAND lynx-test-suzy arithmetic LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.flags COMMAND lynx-test-suzy flags LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.registers COMMAND lynx-test-suzy registers LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.lifecycle COMMAND lynx-test-suzy lifecycle LABELS suzy TIMEOUT 60)
lynx_add_test(NAME suzy.snapshot_refusal COMMAND lynx-test-suzy snapshot_refusal LABELS suzy TIMEOUT 60)

foreach(fault_id IN ITEMS decoder result completion snapshot)
  lynx_add_test(NAME suzy.fault.${fault_id}
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DSUZY_TEST_BINARY=$<TARGET_FILE:lynx-test-suzy>"
      "-DC_COMPILER=${CMAKE_C_COMPILER}"
      "-DFAULT_ID=${fault_id}"
      -P ${CMAKE_CURRENT_LIST_DIR}/fault_controls.cmake
    LABELS suzy TIMEOUT 360)
endforeach()
