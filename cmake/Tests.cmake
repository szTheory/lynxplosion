# Test registration and the exact test inventory.
# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
#
# Fault prevented: a test that is silently not registered lets CI pass without
# running it. Configure fails unless the tests this build registers, with their
# labels, equal the lines of tests/inventory/fast.txt.
#
# Inventory format: one test per line, "name<TAB>labels", labels sorted and
# separated by commas, lines sorted (LC_ALL=C order); lines starting with # are
# comments. This is the configure-time registration check (comparison in
# tests/cmake/inventory_equal.cmake). The JUnit step in scripts/check.sh
# (tests/cmake/check_junit.cmake) covers the set that actually ran.

# Scratch directory. Every file a test or a configure-time self-test fixture
# writes (generated fixtures, work dirs, runner bundles) lives under this
# directory. The rights scan treats build/check/scratch/ as outside the
# toolchain-output zone, so a toolchain-magic file placed there fails the scan
# (CONTEXT amendment 2026-11-09).
set(LYNX_SCRATCH_DIR "${PROJECT_BINARY_DIR}/scratch")

# lynx_add_test(NAME n COMMAND cmd... LABELS l... TIMEOUT seconds)
# All four arguments are required. By convention the first label is the first
# dot component of the name.
function(lynx_add_test)
  cmake_parse_arguments(PARSE_ARGV 0 T "" "NAME;TIMEOUT" "COMMAND;LABELS")
  if(NOT T_NAME OR NOT T_COMMAND OR NOT T_LABELS OR NOT T_TIMEOUT)
    message(FATAL_ERROR "lynx_add_test: NAME, COMMAND, LABELS and TIMEOUT are required")
  endif()
  add_test(NAME ${T_NAME} COMMAND ${T_COMMAND})
  set_tests_properties(${T_NAME} PROPERTIES LABELS "${T_LABELS}" TIMEOUT ${T_TIMEOUT})
endfunction()

include(${PROJECT_SOURCE_DIR}/tests/cmake/inventory_equal.cmake)

# Runs once the top-level directory has been processed, so every include()d
# test file has registered its tests. It writes the registered rows to
# <build>/inventory/registered.txt and compares that file with the inventory
# through lynx_inventory_compare, the function the self-tests also exercise.
function(lynx__check_inventory)
  set(file "${CMAKE_SOURCE_DIR}/tests/inventory/fast.txt")
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "test inventory ${file} does not exist")
  endif()

  get_property(names DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY TESTS)
  set(registered "")
  foreach(name IN LISTS names)
    get_test_property(${name} LABELS labels)
    list(SORT labels)
    list(JOIN labels "," labels)
    string(APPEND registered "${name}\t${labels}\n")
  endforeach()
  set(registered_file "${CMAKE_BINARY_DIR}/inventory/registered.txt")
  file(WRITE "${registered_file}" "${registered}")

  lynx_inventory_compare("${registered_file}" "${file}")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL lynx__check_inventory)
