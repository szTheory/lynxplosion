# Negative controls for the pins lint, run as
#   cmake -DSCRIPTS=<tests/cmake dir> -DWORK=<scratch dir> -P pins_controls.cmake
# Fault prevented: a pins lint that accepts a row fetch-pin.sh would misread or
# that downloads over plain HTTP or with a truncated digest. Each case writes a
# faulty pins file at run time (nothing faulty is committed), runs the real lint
# in a separate process and requires exit 1 and the finding.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var SCRIPTS WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "pins_controls: -D${var} is required")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

string(REPEAT "a" 64 good)
string(REPEAT "a" 63 short)
set(ok_url "https://example.invalid/a.tar.gz")

# reject(<case> <regex> <pins file text>)
function(reject case regex text)
  file(WRITE "${WORK}/${case}.txt" "${text}")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DPINS=${WORK}/${case}.txt"
    -P "${SCRIPTS}/pins.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc STREQUAL "1")
    message(FATAL_ERROR "pins_controls: ${case}: expected exit 1, got exit ${rc}\n${out}")
  endif()
  if(NOT out MATCHES "${regex}")
    message(FATAL_ERROR "pins_controls: ${case}: expected output matching\n  ${regex}\ngot\n${out}")
  endif()
  message(STATUS "pins_controls: ${case}: ok")
endfunction()

# accept(<case> <pins file text>): the lint must pass, so a reject is not vacuous.
function(accept case text)
  file(WRITE "${WORK}/${case}.txt" "${text}")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DPINS=${WORK}/${case}.txt"
    -P "${SCRIPTS}/pins.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "pins_controls: ${case}: expected exit 0, got exit ${rc}\n${out}")
  endif()
  message(STATUS "pins_controls: ${case}: ok")
endfunction()

accept(good_row "# header\nalpha 1 ${ok_url} ${good}\n")
reject(http_url "line 2: alpha: the URL is not https://"
  "# header\nalpha 1 http://example.invalid/a.tar.gz ${good}\n")
reject(short_digest "line 1: alpha: the digest is not 64 lower-case hex digits"
  "alpha 1 ${ok_url} ${short}\n")
reject(upper_digest "line 1: alpha: the digest is not 64 lower-case hex digits"
  "alpha 1 ${ok_url} ${good}A\n")
reject(duplicate_name "line 2: alpha is listed twice"
  "alpha 1 ${ok_url} ${good}\nalpha 2 ${ok_url} ${good}\n")
reject(three_fields "line 1: 3 fields, not 4"
  "alpha ${ok_url} ${good}\n")
reject(five_fields "line 1: 5 fields, not 4"
  "alpha 1 ${ok_url} ${good} extra\n")
reject(comments_only "no rows"
  "# only a comment\n\n# and another\n")
