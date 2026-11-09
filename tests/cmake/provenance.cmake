# Provenance-list check, used as
#   cmake -DROOT=<tree> -DPROVENANCE=<file> -P provenance.cmake
# Fault prevented: a code file taken from an owner project without a notice. Every
# regular file under ROOT/{scripts,cmake,tests,core,runner,include} whose first ten
# lines name the owner project as the source it was taken from must appear, by
# repository path in backticks, in PROVENANCE. PROVENANCE must also mention the
# other notices the repository owes. Findings are reported together, then fail.
if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "" OR NOT DEFINED PROVENANCE
    OR "${PROVENANCE}" STREQUAL "")
  message(FATAL_ERROR "provenance: -DROOT and -DPROVENANCE are required")
endif()
if(NOT EXISTS "${PROVENANCE}")
  message(FATAL_ERROR "provenance: ${PROVENANCE}: file is missing")
endif()
file(READ "${PROVENANCE}" notice)

# The phrases are assembled from parts so that this file's own header never matches.
set(phrase_a "adapted from " "mu" "htari")
string(CONCAT phrase_a ${phrase_a})
set(phrase_b "adapted from the owner's " "mu" "htari")
string(CONCAT phrase_b ${phrase_b})

# First ten lines of a file, without reading more than a small prefix.
function(file_header path out)
  file(READ "${path}" text LIMIT 2048)
  set(header "")
  foreach(n RANGE 1 10)
    string(FIND "${text}" "\n" nl)
    if(nl EQUAL -1)
      string(APPEND header "${text}")
      break()
    endif()
    math(EXPR upto "${nl} + 1")
    string(SUBSTRING "${text}" 0 ${upto} line)
    string(APPEND header "${line}")
    string(SUBSTRING "${text}" ${upto} -1 text)
  endforeach()
  set(${out} "${header}" PARENT_SCOPE)
endfunction()

set(findings "")
foreach(dir scripts cmake tests core runner include)
  file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${ROOT}" "${ROOT}/${dir}/*")
  list(SORT files)
  foreach(rel IN LISTS files)
    file_header("${ROOT}/${rel}" header)
    string(TOLOWER "${header}" header)
    string(FIND "${header}" "${phrase_a}" pa)
    string(FIND "${header}" "${phrase_b}" pb)
    if(pa EQUAL -1 AND pb EQUAL -1)
      continue()
    endif()
    string(FIND "${notice}" "`${rel}`" listed)
    if(listed EQUAL -1)
      string(APPEND findings
        "provenance: ${rel}: adapted from muhtari but not listed in provenance.md\n")
    endif()
  endforeach()
endforeach()

foreach(needle SplitMix64 "FIPS 180-4" cc65 lynx-tests)
  string(FIND "${notice}" "${needle}" pos)
  if(pos EQUAL -1)
    string(APPEND findings "provenance: provenance.md does not mention ${needle}\n")
  endif()
endforeach()

# Findings go out as status lines: FATAL_ERROR text is re-wrapped at 79 columns,
# which would split a finding across lines.
if(NOT findings STREQUAL "")
  string(STRIP "${findings}" findings)
  message(STATUS "${findings}")
  message(FATAL_ERROR "provenance: findings above")
endif()
message(STATUS "provenance: ok")
