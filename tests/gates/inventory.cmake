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
