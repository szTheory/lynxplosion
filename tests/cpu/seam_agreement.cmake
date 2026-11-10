# api.seam_agreement: runs the vector runner's seam-agreement subcommand for
# every profile, each with the sample of its source variant (none uses the
# synertek data, the same as lynx1).
# Usage: cmake -DTOOL=... -DVEC_DIR=... -DMANIFESTS=... -P seam_agreement.cmake
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(var TOOL VEC_DIR MANIFESTS)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "seam_agreement: -D${var}= is required and must not be empty")
  endif()
endforeach()

foreach(case IN ITEMS none:synertek65c02 lynx1:synertek65c02 lynx2:rockwell65c02)
  string(REPLACE ":" ";" parts "${case}")
  list(GET parts 0 profile)
  list(GET parts 1 variant)
  execute_process(
    COMMAND "${TOOL}" seam-agreement --profile ${profile}
      --sample "${VEC_DIR}/${variant}.sample"
      --manifest "${MANIFESTS}/sst65x02-${variant}.manifest"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  message(STATUS "seam_agreement: ${profile}: ${out}")
  if(NOT rc EQUAL 0)
    message(STATUS "seam_agreement: ${profile}: exit ${rc}: ${err}")
    message(FATAL_ERROR "seam_agreement: the machine disagrees with the vector seam under ${profile}")
  endif()
endforeach()
