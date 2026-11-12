# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
# Self-skip scan, used as
#   cmake -DROOT=<source dir> -P no_skip.cmake
# Fault prevented: a test that skips itself, or is switched off, on some lane,
# so CI reports green without running it. Fails naming file and line wherever
# tests/**/*.cmake, cmake/*.cmake or CMakeLists.txt sets one of the test
# properties below. The names are built by concatenation so this file does not
# match itself.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
if(NOT IS_DIRECTORY "${ROOT}")
  message(FATAL_ERROR "no_skip: -DROOT=<source dir> is required")
endif()
set(skip SKIP_)
set(dis DIS)
set(rules ${skip}RETURN_CODE ${skip}REGULAR_EXPRESSION ${dis}ABLED)
set(skip_regex_property "${skip}REGULAR_EXPRESSION")
set(page_mode_skip_line "set_tests_properties(suite.page_mode.observation.lynx1 suite.page_mode.observation.lynx2 PROPERTIES ${skip_regex_property} \"^LYNX-SKIP: page-mode source requires unavailable SERCTL write at\")")

file(GLOB_RECURSE files "${ROOT}/tests/*.cmake")
file(GLOB more "${ROOT}/cmake/*.cmake" "${ROOT}/CMakeLists.txt")
set(found "")
foreach(file IN LISTS files more)
  # Neutralise list syntax so every source line stays one list element.
  file(READ "${file}" text)
  string(REPLACE ";" "," text "${text}")
  string(REPLACE "[" "(" text "${text}")
  string(REPLACE "]" ")" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")
  set(n 0)
  foreach(line IN LISTS lines)
    math(EXPR n "${n} + 1")
    foreach(rule IN LISTS rules)
      if(line MATCHES "(^|[^A-Za-z0-9_])${rule}([^A-Za-z0-9_]|$)")
        file(RELATIVE_PATH rel "${ROOT}" "${file}")
        if(rel STREQUAL "tests/suites/tests.cmake" AND
           rule STREQUAL skip_regex_property AND line STREQUAL page_mode_skip_line)
          continue()
        endif()
        string(APPEND found "\n  ${rel}:${n}: ${rule}")
      endif()
    endforeach()
  endforeach()
endforeach()
if(found)
  message(FATAL_ERROR "no_skip: tests must not skip or be disabled:${found}")
endif()
