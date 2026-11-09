# Adapted from muhtari (MIT, owner project) at commit 7fd0386; skip declarations added.
# JUnit completeness check, run after the test step as
#   cmake -DJUNIT=<junit.xml> -DINVENTORY=<inventory.txt> -DSKIPS=<skips.txt>
#         -P check_junit.cmake
# Fault prevented: a test that silently did not run. A green ctest exit does
# not prove each test ran; this requires every inventory name exactly once in
# the JUnit file, run, with no failure, error or skipped element, and at least
# one test case in all. A skipped test is accepted only when SKIPS lists its
# name with a non-empty reason; a declared skip that ran is a stale declaration.
# Every finding is reported, then the script exits 1.
foreach(var JUNIT INVENTORY SKIPS)
  if(NOT EXISTS "${${var}}")
    message(FATAL_ERROR "check_junit: -D${var}=<file> is required")
  endif()
endforeach()

# Declared skips: name<TAB>reason. Comments and blank lines are ignored.
set(bad "")
set(declared "")
file(STRINGS "${SKIPS}" skip_lines)
foreach(line IN LISTS skip_lines)
  if(line STREQUAL "" OR line MATCHES "^#")
    continue()
  endif()
  string(FIND "${line}" "\t" pos)
  if(pos EQUAL -1)
    set(name "${line}")
    set(reason "")
  else()
    string(SUBSTRING "${line}" 0 ${pos} name)
    math(EXPR from "${pos} + 1")
    string(SUBSTRING "${line}" ${from} -1 reason)
  endif()
  string(STRIP "${reason}" reason)
  list(APPEND declared "${name}")
  if(reason STREQUAL "")
    string(APPEND bad "\n  skips.txt: ${name}: empty reason")
  endif()
endforeach()

# Inventory names (the first field of each row).
set(wanted "")
file(STRINGS "${INVENTORY}" inventory_lines)
foreach(line IN LISTS inventory_lines)
  if(NOT line STREQUAL "" AND NOT line MATCHES "^#")
    string(FIND "${line}" "\t" pos)
    if(pos GREATER -1)
      string(SUBSTRING "${line}" 0 ${pos} line)
    endif()
    list(APPEND wanted "${line}")
  endif()
endforeach()

# One list element per testcase, after neutralising list syntax.
file(READ "${JUNIT}" xml)
string(REPLACE ";" "," xml "${xml}")
string(REPLACE "[" "(" xml "${xml}")
string(REPLACE "]" ")" xml "${xml}")
string(REPLACE "<testcase " ";" cases "${xml}")
list(POP_FRONT cases)
set(ran "")
set(skipped "")
foreach(case IN LISTS cases)
  set(name "")
  if(case MATCHES "^name=\"([^\"]*)\"")
    set(name "${CMAKE_MATCH_1}")
  endif()
  list(APPEND ran "${name}")
  if(case MATCHES "<(failure|error)")
    string(APPEND bad "\n  ${name}: failed, errored or did not run")
  elseif(case MATCHES "<skipped")
    list(APPEND skipped "${name}")
    if(NOT name IN_LIST declared)
      string(APPEND bad "\n  ${name}: skipped but not declared in skips.txt")
    endif()
  elseif(NOT case MATCHES "^[^>]*status=\"run\"")
    string(APPEND bad "\n  ${name}: failed, errored or did not run")
  endif()
endforeach()

if(NOT ran)
  string(APPEND bad "\n  junit: no test cases")
endif()

# Each inventory name exactly once. `others` is `ran` without that name, so the
# length difference is the count; x.y and x.y.self_test are distinct names.
list(LENGTH ran all)
foreach(name IN LISTS wanted)
  set(others ${ran})
  list(REMOVE_ITEM others "${name}")
  list(LENGTH others rest)
  math(EXPR times "${all} - ${rest}")
  if(NOT times EQUAL 1)
    string(APPEND bad "\n  ${name}: in the JUnit file ${times} times, expected once")
  endif()
endforeach()

# Names that ran without being listed.
set(reported "")
foreach(name IN LISTS ran)
  if(NOT name IN_LIST wanted AND NOT name IN_LIST reported)
    list(APPEND reported "${name}")
    string(APPEND bad "\n  ${name}: ran but is not in the inventory")
  endif()
endforeach()

# A declared skip must be in the inventory and must have been skipped.
foreach(name IN LISTS declared)
  if(NOT name IN_LIST wanted)
    string(APPEND bad "\n  ${name}: declared as skipped but not in the inventory")
  elseif(name IN_LIST ran AND NOT name IN_LIST skipped)
    string(APPEND bad "\n  ${name}: declared as skipped but ran")
  endif()
endforeach()

if(bad)
  message(FATAL_ERROR "check_junit: ${JUNIT}:${bad}")
endif()
list(LENGTH wanted count)
list(LENGTH skipped skip_count)
message(STATUS "check_junit: ${count} tests accounted for once each (${skip_count} declared skips)")
