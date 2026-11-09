# Inventory gates: the exact-inventory checks and their negative controls.
# Each self-test feeds a built fault to the real checker and requires the exact
# exit code and message (tests/cmake/inventory_controls.cmake).
set(LYNX_TEST_SCRIPTS ${PROJECT_SOURCE_DIR}/tests/cmake)

foreach(mode junit registered)
  lynx_add_test(NAME inventory.${mode}.self_test
    COMMAND ${CMAKE_COMMAND}
      -DMODE=${mode}
      -DSCRIPTS=${LYNX_TEST_SCRIPTS}
      -DWORK=${LYNX_SCRATCH_DIR}/gates/inventory-${mode}
      -P ${LYNX_TEST_SCRIPTS}/inventory_controls.cmake
    LABELS inventory TIMEOUT 30)
endforeach()

# No test may skip or disable itself. The scan runs over this repository, and
# its self-test over a fixture that sets a forbidden property. The fixture is
# written at configure time and its text is assembled from parts, so no
# committed file contains the property name and the scan stays clean.
lynx_add_test(NAME policy.no_skip
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
    -P ${LYNX_TEST_SCRIPTS}/no_skip.cmake
  LABELS policy TIMEOUT 30)

set(lynx_no_skip_fixture ${LYNX_SCRATCH_DIR}/gates/no_skip_fixture)
set(lynx_skip_part "SKIP_")
file(WRITE ${lynx_no_skip_fixture}/tests/x.cmake
  "set_tests_properties(t PROPERTIES ${lynx_skip_part}RETURN_CODE 77)\n")
set(lynx_no_skip_cmd
  ${CMAKE_COMMAND} -DROOT=${lynx_no_skip_fixture} -P ${LYNX_TEST_SCRIPTS}/no_skip.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_no_skip_cmd "${lynx_no_skip_cmd}")
lynx_add_test(NAME policy.no_skip.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_no_skip_cmd}" -DCODE=1
    "-DREGEX=tests/x[.]cmake:1: ${lynx_skip_part}RETURN_CODE"
    -P ${LYNX_TEST_SCRIPTS}/expect_fail.cmake
  LABELS policy TIMEOUT 30)
