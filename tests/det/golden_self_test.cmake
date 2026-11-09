# det.golden.self_test: an edited golden must make golden.cmake fail.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DGOLDEN=... -DWORK=... -DSCRIPTS=... -P golden_self_test.cmake
#
# Fault prevented: a golden comparison that passes whatever the golden holds.
# The edited copy is built here, at test time, from the committed golden with
# its last hex digit changed; it is never committed.
foreach(var RUNNER CONTENT GOLDEN WORK SCRIPTS)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "golden_self_test: -D${var}= is required")
  endif()
endforeach()

file(STRINGS "${GOLDEN}" golden_lines)
set(digest "")
foreach(line IN LISTS golden_lines)
  if(NOT line MATCHES "^#" AND NOT line STREQUAL "")
    set(digest "${line}")
  endif()
endforeach()
string(LENGTH "${digest}" digits)
if(NOT digits EQUAL 64)
  message(FATAL_ERROR "golden_self_test: ${GOLDEN}: no 64-digit digest line")
endif()
string(SUBSTRING "${digest}" 0 63 head)
string(SUBSTRING "${digest}" 63 1 last)
if(last STREQUAL "0")
  set(changed "1")
else()
  set(changed "0")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/edited.digest" "# edited copy for the negative control\n${head}${changed}\n")

set(cmd ${CMAKE_COMMAND} -DRUNNER=${RUNNER} -DCONTENT=${CONTENT}
  -DGOLDEN=${WORK}/edited.digest -DWORK=${WORK}/run -P ${CMAKE_CURRENT_LIST_DIR}/golden.cmake)
execute_process(
  COMMAND ${CMAKE_COMMAND} "-DCMD=${cmd}" -DCODE=1 "-DREGEX=golden: observation digest"
    -P ${SCRIPTS}/expect_fail.cmake
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "golden_self_test: the edited golden was not rejected as required:\n${out}${err}")
endif()
message(STATUS "golden_self_test: ok, an edited golden is rejected")
