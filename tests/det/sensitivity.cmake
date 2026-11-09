# det.sensitivity: the observation digest depends on seed, content and tick
# budget, so equal digests elsewhere are not vacuous.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DWORK=... -P sensitivity.cmake
#
# Fault prevented: a digest that ignores an input (or a runner that ignores a
# flag) would make every equality test pass for the wrong reason.
foreach(var RUNNER CONTENT WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "sensitivity: -D${var}= is required")
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

# The changed content: the first data byte, 03, becomes 04. Built here at test
# time and never committed.
file(READ "${CONTENT}" original)
string(REPLACE "\n03 0a" "\n04 0a" changed "${original}")
if(changed STREQUAL original)
  message(FATAL_ERROR "sensitivity: the fixture no longer starts its data with '03 0a'")
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
