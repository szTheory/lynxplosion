# api.seam_agreement.<profile>: runs the vector runner's seam-agreement
# subcommand for one profile with the sample of its source variant (none uses
# the synertek data, the same as lynx1). One test per profile so the 1,024
# undefined-row restores under none do not hold the other two profiles on the
# critical path (plan 02-11 CI efficiency review).
# Usage: cmake -DTOOL=... -DVEC_DIR=... -DMANIFESTS=... -DPROFILE=none|lynx1|lynx2
#              -DVARIANT=synertek65c02|rockwell65c02 -P seam_agreement.cmake
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

foreach(var TOOL VEC_DIR MANIFESTS PROFILE VARIANT)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "seam_agreement: -D${var}= is required and must not be empty")
  endif()
endforeach()

set(profile "${PROFILE}")
set(variant "${VARIANT}")
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
