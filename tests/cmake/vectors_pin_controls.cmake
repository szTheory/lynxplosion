# Negative controls for the vector pin-record lint, run as
#   cmake -DSCRIPTS=<tests/cmake dir> -DRECORD=<upstream.sha256> -DWORK=<scratch dir> \
#     -P vectors_pin_controls.cmake
# Fault prevented: a lint that accepts a widened or malformed pin record. Each
# case copies the real record, makes one change at run time (nothing faulty is
# committed), runs the real lint in a separate process and requires exit 1 and
# the finding with its line number. The unchanged copy must pass, so no reject
# is vacuous.
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var SCRIPTS RECORD WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "vectors_pin_controls: -D${var} is required")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${RECORD}" good)

# The real record's layout: three header lines, then rows from line 4 on, the
# first two rows being rockwell65c02/v1/00.json and 01.json.
string(REGEX MATCH "\n([0-9a-f]+)  rockwell65c02/v1/00\\.json\n([0-9a-f]+)  rockwell65c02/v1/01\\.json\n" two_rows "${good}")
set(row0 "${CMAKE_MATCH_1}  rockwell65c02/v1/00.json")
set(row1 "${CMAKE_MATCH_2}  rockwell65c02/v1/01.json")
set(digest0 "${CMAKE_MATCH_1}")
if("${digest0}" STREQUAL "" OR "${CMAKE_MATCH_2}" STREQUAL "")
  message(FATAL_ERROR "vectors_pin_controls: the first two rows of ${RECORD} were not found")
endif()
string(REGEX MATCH "# commit ([0-9a-f]+)\n" commit_line "${good}")
set(commit "${CMAKE_MATCH_1}")

function(lint case expected_rc regex text)
  file(WRITE "${WORK}/${case}.sha256" "${text}")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DFILE=${WORK}/${case}.sha256"
    -P "${SCRIPTS}/vectors_pin.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc STREQUAL "${expected_rc}")
    message(FATAL_ERROR "vectors_pin_controls: ${case}: expected exit ${expected_rc}, got exit ${rc}\n${out}")
  endif()
  if(NOT out MATCHES "${regex}")
    message(FATAL_ERROR "vectors_pin_controls: ${case}: expected output matching\n  ${regex}\ngot\n${out}")
  endif()
  message(STATUS "vectors_pin_controls: ${case}: ok")
endfunction()

lint(committed_record 0 "ok [(]512 rows" "${good}")

string(REPLACE "https://github.com/SingleStepTests/65x02" "http://github.com/SingleStepTests/65x02" text "${good}")
lint(http_url 1 "line 1: a plain http:// URL.*no header line names https://github.com/SingleStepTests/65x02" "${text}")

string(REPLACE "# commit ${commit}\n" "# commit ${commit}0\n" text "${good}")
lint(commit_41_hex 1 "line 2: the commit is 41 hex digits, not 40" "${text}")

string(SUBSTRING "${digest0}" 1 -1 short_digest)
string(REPLACE "${digest0}" "${short_digest}" text "${good}")
lint(digest_63_hex 1 "line 4: the digest is not 64 lower-case hex digits [(]63 characters[)]" "${text}")

string(REGEX REPLACE "[0-9a-f]+  synertek65c02/v1/ff\\.json\n$" "" text "${good}")
lint(rows_511 1 "511 rows, not 512.*missing row for synertek65c02/v1/ff.json" "${text}")

string(REPLACE "${row1}\n" "${row0}\n" text "${good}")
lint(duplicate_row 1 "line 5: rockwell65c02/v1/00.json is listed twice.*missing row for rockwell65c02/v1/01.json" "${text}")

string(REPLACE "${row0}\n${row1}\n" "${row1}\n${row0}\n" text "${good}")
lint(unsorted_row 1 "line 5: rockwell65c02/v1/00.json is not in sorted [(]byte[)] order after rockwell65c02/v1/01.json" "${text}")

string(REPLACE "rockwell65c02/v1/00.json" "wdc65c02/v1/00.json" text "${good}")
lint(outside_variant 1 "line 4: the path is not <synertek65c02[|]rockwell65c02>/v1/<xx>.json.*missing row for rockwell65c02/v1/00.json" "${text}")
