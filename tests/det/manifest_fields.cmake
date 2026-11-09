# det.manifest_fields: run.manifest of real runner output passes the strict
# validator, holds its keys in the documented order, and records exact values
# at the boundaries (empty input, zero budget, the largest budget).
# Usage: cmake -DRUNNER=... -DCONTENT=... -DWORK=... -DSH=... -DVALIDATE=... -P manifest_fields.cmake
#
# Fault prevented: a manifest that drifts in key order, rounds a tick count,
# leaks a host path or date, or is accepted by a validator that checks nothing.
foreach(var RUNNER CONTENT WORK SH VALIDATE)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "manifest_fields: -D${var}= is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(required
  profile startup_tier content_sha256 content_load_addr input_digest tick_budget
  ticks_consumed stop_reason core_build ram_policy ram_seed evidence_tier)
string(REPLACE ";" " " required_text "${required}")
set(empty_input_sha256 e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)

# run(<tag> <content> <ticks>): runs the runner into WORK/<tag>.
function(run tag content ticks)
  execute_process(
    COMMAND "${RUNNER}" --content "${content}" --load-addr 0200
      --ram-policy splitmix64 --ram-seed 0123456789abcdef --ticks ${ticks}
      --out "${WORK}/${tag}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "manifest_fields: ${tag}: runner exited ${rc}: ${err}")
  endif()
  if(NOT out MATCHES "\\(repeatability \\(R\\), not accuracy\\)")
    message(FATAL_ERROR "manifest_fields: ${tag}: digest lines are not labelled repeatability:\n${out}")
  endif()
endfunction()

# value(<tag> <key> <out var>): the value of a manifest key, which must exist.
function(value tag key out)
  file(STRINGS "${WORK}/${tag}/run.manifest" lines)
  foreach(line IN LISTS lines)
    if(line MATCHES "^${key} = (.*)$")
      set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "manifest_fields: ${tag}: no ${key} in run.manifest")
endfunction()

# (a) the fixed workload.
run(fixed "${CONTENT}" 4096)
execute_process(
  COMMAND "${SH}" "${VALIDATE}" --required "${required_text}" --no-host-data
    "${WORK}/fixed/run.manifest"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "manifest_fields: validate-kv rejected the runner manifest:\n${out}")
endif()

file(STRINGS "${WORK}/fixed/run.manifest" lines)
set(keys "")
foreach(line IN LISTS lines)
  if(NOT line MATCHES "^([a-z0-9_]+) = ")
    message(FATAL_ERROR "manifest_fields: bad manifest line: ${line}")
  endif()
  list(APPEND keys "${CMAKE_MATCH_1}")
endforeach()
if(NOT keys STREQUAL required)
  message(FATAL_ERROR "manifest_fields: manifest keys are\n  ${keys}\nexpected the documented order\n  ${required}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env LC_ALL=C sort -c "${WORK}/fixed/digests.txt"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "manifest_fields: digests.txt is not in byte order: ${out}")
endif()

value(fixed input_digest input_digest)
if(NOT input_digest STREQUAL empty_input_sha256)
  message(FATAL_ERROR "manifest_fields: input_digest ${input_digest} is not the SHA-256 of zero bytes")
endif()
value(fixed evidence_tier tier)
value(fixed profile profile)
if(NOT tier STREQUAL "R" OR NOT profile STREQUAL "none")
  message(FATAL_ERROR "manifest_fields: expected evidence_tier = R and profile = none, got ${tier} and ${profile}")
endif()
value(fixed ram_seed seed)
if(NOT seed MATCHES "^0x[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$")
  message(FATAL_ERROR "manifest_fields: ram_seed ${seed} is not 0x plus 16 lowercase hex digits")
endif()
value(fixed tick_budget budget)
value(fixed ticks_consumed consumed)
if(NOT budget STREQUAL "4096" OR NOT consumed STREQUAL "4096")
  message(FATAL_ERROR "manifest_fields: expected tick counts 4096, got ${budget} and ${consumed}")
endif()

# (b) a zero budget.
run(zero "${CONTENT}" 0)
value(zero tick_budget budget)
value(zero ticks_consumed consumed)
if(NOT budget STREQUAL "0" OR NOT consumed STREQUAL "0")
  message(FATAL_ERROR "manifest_fields: a zero budget recorded tick_budget=${budget} ticks_consumed=${consumed}")
endif()

# (c) the largest budget on an image that halts: the fixture's bytes with db at
# offset 3, written here at test time.
file(READ "${CONTENT}" original)
string(REPLACE "\n03 0a 11 18" "\n03 0a 11 db" halting "${original}")
if(halting STREQUAL original)
  message(FATAL_ERROR "manifest_fields: the fixture no longer starts its data with '03 0a 11 18'")
endif()
file(WRITE "${WORK}/halting-content.hex" "${halting}")
run(max "${WORK}/halting-content.hex" 18446744073709551615)
value(max tick_budget budget)
value(max ticks_consumed consumed)
value(max stop_reason stop)
if(NOT budget STREQUAL "18446744073709551615")
  message(FATAL_ERROR "manifest_fields: tick_budget ${budget} is not exactly 18446744073709551615")
endif()
if(NOT consumed STREQUAL "12" OR NOT stop STREQUAL "halt")
  message(FATAL_ERROR "manifest_fields: expected ticks_consumed = 12 and stop_reason = halt, got ${consumed} and ${stop}")
endif()
execute_process(
  COMMAND "${SH}" "${VALIDATE}" --required "${required_text}" --no-host-data
    "${WORK}/zero/run.manifest" "${WORK}/max/run.manifest"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "manifest_fields: validate-kv rejected a boundary manifest:\n${out}")
endif()
message(STATUS "manifest_fields: ok")
