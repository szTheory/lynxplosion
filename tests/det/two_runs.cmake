# det.two_runs: two fresh runner processes, same inputs, equal evidence.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DWORK=... -P two_runs.cmake
#
# Fault prevented: a runner whose output depends on the process (an address, a
# clock, a path, an uninitialised byte) would still pass a single-run test.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var RUNNER CONTENT WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "two_runs: -D${var}= is required")
  endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/bll.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${CONTENT}" payload_hex)
lynx_write_bll("${payload_hex}" "${WORK}/content.bll" 0x0200)
file(SHA256 "${WORK}/content.bll" content_sha256)

set(expected_keys
  profile content_format startup startup_tier content_sha256 content_load_addr entry_addr object_len
  payload_addr payload_len input_digest tick_budget ticks_consumed stop_reason core_build
  ram_policy ram_seed evidence_tier)
set(expected_names obs.ram.hex obs.regs.hex obs.time.hex run.manifest)

foreach(run a b)
  execute_process(
    COMMAND "${RUNNER}" --content "${WORK}/content.bll"
      --ram-policy splitmix64 --ram-seed 0123456789abcdef --ticks 4096
      --out "${WORK}/${run}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out_${run} ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "two_runs: run ${run}: expected exit 0, got ${rc}: ${err}")
  endif()

  # stdout is exactly the two labelled digest lines.
  string(REGEX REPLACE "\n$" "" trimmed "${out_${run}}")
  string(REPLACE "\n" ";" lines "${trimmed}")
  list(LENGTH lines line_count)
  if(NOT line_count EQUAL 2)
    message(FATAL_ERROR "two_runs: run ${run}: expected 2 stdout lines, got ${line_count}")
  endif()
  set(want observation bundle)
  set(index 0)
  foreach(line IN LISTS lines)
    list(GET want ${index} kind)
    if(NOT line MATCHES "^${kind}_digest ([0-9a-f]+) \\(repeatability \\(R\\), not accuracy\\)$")
      message(FATAL_ERROR "two_runs: run ${run}: bad ${kind} digest line: ${line}")
    endif()
    string(LENGTH "${CMAKE_MATCH_1}" digits)
    if(NOT digits EQUAL 64)
      message(FATAL_ERROR "two_runs: run ${run}: ${kind} digest has ${digits} digits, expected 64")
    endif()
    set(${kind}_${run} "${CMAKE_MATCH_1}")
    math(EXPR index "${index} + 1")
  endforeach()

  # run.manifest: keys in the documented order, no more and no fewer.
  file(STRINGS "${WORK}/${run}/run.manifest" manifest_lines)
  set(keys "")
  foreach(line IN LISTS manifest_lines)
    if(NOT line MATCHES "^([a-z0-9_]+) = .+$")
      message(FATAL_ERROR "two_runs: run ${run}: bad manifest line: ${line}")
    endif()
    list(APPEND keys "${CMAKE_MATCH_1}")
    if(CMAKE_MATCH_1 STREQUAL "content_sha256" AND NOT line STREQUAL "content_sha256 = ${content_sha256}")
      message(FATAL_ERROR "two_runs: run ${run}: wrong complete-object content_sha256: ${line}")
    endif()
  endforeach()
  if(NOT keys STREQUAL expected_keys)
    message(FATAL_ERROR "two_runs: run ${run}: manifest keys are\n  ${keys}\nexpected\n  ${expected_keys}")
  endif()

  # digests.txt: byte-sorted, the four files, and the stdout digests agree
  # with an independent recomputation.
  file(STRINGS "${WORK}/${run}/digests.txt" digest_lines)
  set(names "")
  set(obs_text "")
  foreach(line IN LISTS digest_lines)
    if(NOT line MATCHES "^([a-z.]+) ([0-9a-f]+)$")
      message(FATAL_ERROR "two_runs: run ${run}: bad digests.txt line: ${line}")
    endif()
    list(APPEND names "${CMAKE_MATCH_1}")
    if(CMAKE_MATCH_1 MATCHES "^obs\\.")
      string(APPEND obs_text "${line}\n")
    endif()
    if(CMAKE_MATCH_1 STREQUAL "run.manifest")
      file(SHA256 "${WORK}/${run}/run.manifest" manifest_sha)
      if(NOT CMAKE_MATCH_2 STREQUAL manifest_sha)
        message(FATAL_ERROR "two_runs: run ${run}: digests.txt hash of run.manifest is wrong")
      endif()
    endif()
  endforeach()
  set(sorted ${names})
  list(SORT sorted COMPARE STRING CASE SENSITIVE)
  if(NOT names STREQUAL sorted OR NOT names STREQUAL expected_names)
    message(FATAL_ERROR "two_runs: run ${run}: digests.txt names are ${names}, expected sorted ${expected_names}")
  endif()
  file(WRITE "${WORK}/${run}-obs-lines.txt" "${obs_text}")
  file(SHA256 "${WORK}/${run}-obs-lines.txt" recomputed_obs)
  file(SHA256 "${WORK}/${run}/digests.txt" recomputed_bundle)
  if(NOT recomputed_obs STREQUAL observation_${run})
    message(FATAL_ERROR "two_runs: run ${run}: observation digest ${observation_${run}} != recomputed ${recomputed_obs}")
  endif()
  if(NOT recomputed_bundle STREQUAL bundle_${run})
    message(FATAL_ERROR "two_runs: run ${run}: bundle digest ${bundle_${run}} != recomputed ${recomputed_bundle}")
  endif()
endforeach()

# The two processes agree on every byte of evidence.
foreach(name digests.txt run.manifest obs.ram.hex obs.regs.hex obs.time.hex)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${WORK}/a/${name}" "${WORK}/b/${name}"
    RESULT_VARIABLE same)
  if(NOT same EQUAL 0)
    message(FATAL_ERROR "two_runs: ${name} differs between the two runs")
  endif()
endforeach()
if(NOT out_a STREQUAL out_b)
  message(FATAL_ERROR "two_runs: stdout differs between the two runs:\n${out_a}\n${out_b}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "two_runs: ok observation ${observation_a} bundle ${bundle_a}")
