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

# Package the source fixture's hex text into the binary container consumed by
# lynx-run. printf's %b output is binary-safe (including zero bytes); every
# input octet is emitted as a fixed three-digit octal escape.
function(write_bll hex_text path)
  string(REGEX REPLACE "#[^\r\n]*" "" payload_hex "${hex_text}")
  string(REGEX REPLACE "[ \t\r\n]" "" payload_hex "${payload_hex}")
  string(LENGTH "${payload_hex}" hex_len)
  if(hex_len EQUAL 0 OR NOT payload_hex MATCHES "^[0-9A-Fa-f]+$" OR hex_len GREATER 131050)
    message(FATAL_ERROR "sensitivity: payload is empty, malformed or too large for BS93")
  endif()
  math(EXPR odd "${hex_len} % 2")
  if(NOT odd EQUAL 0)
    message(FATAL_ERROR "sensitivity: payload has an odd number of hex digits")
  endif()
  math(EXPR payload_len "${hex_len} / 2")
  math(EXPR object_len "${payload_len} + 10")
  math(EXPR len_hi "${object_len} / 256")
  math(EXPR len_lo "${object_len} % 256")
  set(header "80080200")
  foreach(byte IN ITEMS ${len_hi} ${len_lo})
    math(EXPR high "${byte} / 16")
    math(EXPR low "${byte} % 16")
    string(SUBSTRING "0123456789abcdef" ${high} 1 high_hex)
    string(SUBSTRING "0123456789abcdef" ${low} 1 low_hex)
    string(APPEND header "${high_hex}${low_hex}")
  endforeach()
  string(APPEND header "42533933${payload_hex}")
  string(LENGTH "${header}" object_hex_len)
  math(EXPR byte_count "${object_hex_len} / 2")
  math(EXPR last_byte "${byte_count} - 1")
  set(octal_bytes "")
  foreach(i RANGE 0 ${last_byte})
    math(EXPR at "${i} * 2")
    string(SUBSTRING "${header}" ${at} 2 byte_hex)
    math(EXPR byte "0x${byte_hex}")
    math(EXPR digit1 "${byte} / 64")
    math(EXPR remainder "${byte} % 64")
    math(EXPR digit2 "${remainder} / 8")
    math(EXPR digit3 "${remainder} % 8")
    string(APPEND octal_bytes "\\${digit1}${digit2}${digit3}")
  endforeach()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env sh -c "printf '%b' '${octal_bytes}'"
    OUTPUT_FILE "${path}" RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sensitivity: could not write binary BLL: ${err}")
  endif()
  file(SIZE "${path}" written_len)
  if(NOT written_len EQUAL object_len)
    message(FATAL_ERROR "sensitivity: BLL writer produced ${written_len} bytes, expected ${object_len}")
  endif()
endfunction()

# digest(<out var> <tag> <binary BLL file> <seed hex> <ticks>)
function(digest out tag object seed ticks)
  execute_process(
    COMMAND "${RUNNER}" --content "${object}"
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
write_bll("${original}" "${WORK}/base.bll")
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
write_bll("${changed}" "${WORK}/changed.bll")

set(base_seed 0123456789abcdef)
digest(base base "${WORK}/base.bll" ${base_seed} 4096)
digest(again again "${WORK}/base.bll" ${base_seed} 4096)
if(NOT base STREQUAL again)
  message(FATAL_ERROR "sensitivity: repeating the base workload changed the digest: ${base} ${again}")
endif()

digest(seed_changed seed "${WORK}/base.bll" 0123456789abcdf0 4096)
digest(content_changed content "${WORK}/changed.bll" ${base_seed} 4096)
digest(ticks_changed ticks "${WORK}/base.bll" ${base_seed} 4092)
foreach(variant seed content ticks)
  if(${variant}_changed STREQUAL base)
    message(FATAL_ERROR "sensitivity: ${variant}: digest unchanged")
  endif()
  message(STATUS "sensitivity: ${variant}: digest differs from the base")
endforeach()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "sensitivity: ok base ${base}")
