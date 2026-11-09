# No-network scan, used as
#   cmake -DROOT=<source dir> -P no_network.cmake
# Fault prevented: a test or build definition that reaches the network, which
# makes the check flaky and leaks requests. Fails naming file, line and token
# wherever tests/**/*.cmake, *.sh, *.c, *.h, cmake/*.cmake or CMakeLists.txt
# names a network tool or a CMake download or fetch-content command. Pinned
# sources are fetched only by the pins step of scripts/check.sh, outside any
# test. Every token below is assembled from parts so this file does not match
# itself (the same trick as no_skip.cmake). Matching is case-insensitive, so
# the reported token is lower case.
cmake_minimum_required(VERSION 3.25)
if(NOT IS_DIRECTORY "${ROOT}")
  message(FATAL_ERROR "no_network: -DROOT=<source dir> is required")
endif()

set(b "(^|[^a-z0-9_-])")
set(e "([^a-z0-9_-]|$)")
set(gap "[ \t]+")
# network tools
set(rules "")
function(add_rule)
  string(CONCAT rule ${ARGN})
  list(APPEND rules "${rule}")
  set(rules "${rules}" PARENT_SCOPE)
endfunction()
add_rule("${b}cu" "rl${e}")
add_rule("${b}wg" "et${e}")
add_rule("${b}gi" "t${gap}cl" "one${e}")
add_rule("${b}gi" "t${gap}fet" "ch${e}")
add_rule("${b}gi" "t${gap}pu" "ll${e}")
add_rule("${b}gi" "t${gap}ls-re" "mote${e}")
add_rule("${b}g" "h${gap}ap" "i${e}")
add_rule("${b}g" "h${gap}ru" "n${e}")
# CMake download and fetch-content commands
add_rule("fi" "le[ \t]*\\([ \t]*down" "load${e}")
add_rule("fi" "le[ \t]*\\([ \t]*up" "load${e}")
add_rule("${b}fetch" "content")
add_rule("${b}external" "project_add${e}")

file(GLOB_RECURSE files "${ROOT}/tests/*.cmake" "${ROOT}/tests/*.sh"
  "${ROOT}/tests/*.c" "${ROOT}/tests/*.h")
file(GLOB more "${ROOT}/cmake/*.cmake" "${ROOT}/CMakeLists.txt")
set(found "")
foreach(file IN LISTS files more)
  # Neutralise list syntax so every source line stays one list element.
  file(READ "${file}" text)
  string(TOLOWER "${text}" text)
  string(REPLACE ";" "," text "${text}")
  string(REPLACE "[" "(" text "${text}")
  string(REPLACE "]" ")" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")
  set(n 0)
  foreach(line IN LISTS lines)
    math(EXPR n "${n} + 1")
    foreach(rule IN LISTS rules)
      if(line MATCHES "${rule}")
        string(STRIP "${CMAKE_MATCH_0}" token)
        file(RELATIVE_PATH rel "${ROOT}" "${file}")
        string(APPEND found "\n  no_network: ${rel}:${n}: ${token}")
      endif()
    endforeach()
  endforeach()
endforeach()
if(found)
  message(FATAL_ERROR "no_network: tests and build definitions must not reach the network:${found}")
endif()
message(STATUS "no_network: ${ROOT} ok")
