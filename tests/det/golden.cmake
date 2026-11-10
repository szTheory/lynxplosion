# det.golden: the observation digest of the fixed workload equals the
# committed golden, so the value is the same on every compiler that builds it.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DGOLDEN=... -DWORK=... -P golden.cmake
#
# Fault prevented: a digest that depends on the compiler (padding, signed
# shift, evaluation order) passes every same-build test and breaks only on the
# other platform.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var RUNNER CONTENT GOLDEN WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "golden: -D${var}= is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/bll.cmake")

# The last non-comment, non-empty line of the golden is the digest.
file(STRINGS "${GOLDEN}" golden_lines)
set(want "")
foreach(line IN LISTS golden_lines)
  if(NOT line MATCHES "^#" AND NOT line STREQUAL "")
    set(want "${line}")
  endif()
endforeach()
if(NOT want MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "golden: ${GOLDEN}: no digest line found")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${CONTENT}" payload_hex)
lynx_write_bll("${payload_hex}" "${WORK}/content.bll" 0x0200)
execute_process(
  COMMAND "${RUNNER}" --content "${WORK}/content.bll"
    --ram-policy splitmix64 --ram-seed 0123456789abcdef --ticks 4096
    --out "${WORK}/run"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "golden: runner exited ${rc}: ${err}")
endif()
if(NOT out MATCHES "observation_digest ([0-9a-f]+) \\(repeatability \\(R\\), not accuracy\\)")
  message(FATAL_ERROR "golden: no observation digest line in runner output:\n${out}")
endif()
set(got "${CMAKE_MATCH_1}")
if(NOT got STREQUAL want)
  message(FATAL_ERROR "golden: observation digest ${got} differs from ${GOLDEN} (${want})")
endif()
message(STATUS "golden: ok observation digest ${got} equals the committed golden")
