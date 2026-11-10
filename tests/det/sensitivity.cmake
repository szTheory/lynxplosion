# det.sensitivity: the observation digest depends on seed, content and tick
# budget, so equal digests elsewhere are not vacuous.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DFLIP_FROM=... -DFLIP_TO=... -DWORK=...
#        -P sensitivity.cmake
#
# Fault prevented: a digest that ignores an input (or a runner that ignores a
# flag) would make every equality test pass for the wrong reason.
# Script mode starts with old policies on CMake 3.28 (CI): without these,
# while(TRUE) below is false there and the occurrence count reads 0.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(var RUNNER CONTENT FLIP_FROM FLIP_TO WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "sensitivity: -D${var}= is required and must not be empty")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# digest(<out var> <tag> <content file> <seed hex> <ticks>)
function(digest out tag content seed ticks)
  execute_process(
    COMMAND "${RUNNER}" --content "${content}" --load-addr 0200
      --ram-policy splitmix64 --ram-seed ${seed} --ticks ${ticks}
      --out "${WORK}/${tag}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sensitivity: ${tag}: runner exited ${rc}: ${err}")
  endif()
  if(NOT text MATCHES "observation_digest ([0-9a-f]+) ")
    message(FATAL_ERROR "sensitivity: ${tag}: no observation digest in\n${text}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# The changed content: the one place where FLIP_FROM occurs becomes FLIP_TO.
# The match is unanchored and must be unique: the occurrences are counted, and
# any count but 1 is a failure. Built here at test time and never committed.
file(READ "${CONTENT}" original)
set(occurrences 0)
set(rest "${original}")
string(LENGTH "${FLIP_FROM}" from_length)
while(TRUE)
  string(FIND "${rest}" "${FLIP_FROM}" at)
  if(at EQUAL -1)
    break()
  endif()
  math(EXPR occurrences "${occurrences} + 1")
  math(EXPR cut "${at} + ${from_length}")
  string(SUBSTRING "${rest}" ${cut} -1 rest)
endwhile()
if(NOT occurrences EQUAL 1)
  message(FATAL_ERROR "sensitivity: FLIP_FROM '${FLIP_FROM}' must occur exactly once in ${CONTENT}, found ${occurrences}")
endif()
string(REPLACE "${FLIP_FROM}" "${FLIP_TO}" changed "${original}")
if(changed STREQUAL original)
  message(FATAL_ERROR "sensitivity: FLIP_TO '${FLIP_TO}' leaves the content unchanged")
endif()
file(WRITE "${WORK}/changed-content.hex" "${changed}")

set(base_seed 0123456789abcdef)
digest(base base "${CONTENT}" ${base_seed} 4096)
digest(again again "${CONTENT}" ${base_seed} 4096)
if(NOT base STREQUAL again)
  message(FATAL_ERROR "sensitivity: repeating the base workload changed the digest: ${base} ${again}")
endif()

digest(seed_changed seed "${CONTENT}" 0123456789abcdf0 4096)
digest(content_changed content "${WORK}/changed-content.hex" ${base_seed} 4096)
digest(ticks_changed ticks "${CONTENT}" ${base_seed} 4092)
foreach(variant seed content ticks)
  if(${variant}_changed STREQUAL base)
    message(FATAL_ERROR "sensitivity: ${variant}: digest unchanged")
  endif()
  message(STATUS "sensitivity: ${variant}: digest differs from the base")
endforeach()
message(STATUS "sensitivity: ok base ${base}")
