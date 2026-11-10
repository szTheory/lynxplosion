# Negative controls for the test-inventory checks, run as
#   cmake -DMODE=junit|registered -DSCRIPTS=<tests/cmake dir> -DWORK=<scratch dir>
#         -P inventory_controls.cmake
# Fault prevented: an inventory check that passes on a fault it is meant to
# catch. Each case builds a tiny fixture at run time (nothing is committed),
# feeds it to the real checker in a separate process and requires the exact
# exit code and a message matching the case's regex. A crash, a typo or a
# vacuous pass cannot satisfy a case.
#   MODE=junit        tests/cmake/check_junit.cmake
#   MODE=registered   tests/cmake/inventory_equal.cmake
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
foreach(var MODE SCRIPTS WORK)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "inventory_controls: -D${var} is required")
  endif()
endforeach()
if(NOT MODE STREQUAL "junit" AND NOT MODE STREQUAL "registered")
  message(FATAL_ERROR "inventory_controls: -DMODE must be junit or registered, got '${MODE}'")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(header "# fixture header comment\n")

# expect(<case> <code> <regex> <command...>): run the command and compare.
function(expect case code regex)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc
    OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc STREQUAL code)
    message(FATAL_ERROR "inventory_controls: ${case}: expected exit ${code}, got exit ${rc}\n${out}")
  endif()
  if(NOT out MATCHES "${regex}")
    message(FATAL_ERROR "inventory_controls: ${case}: expected output matching\n  ${regex}\ngot\n${out}")
  endif()
  message(STATUS "inventory_controls: ${case}: ok")
endfunction()

# A message line as printed by FATAL_ERROR: indented, on its own line.
function(line_rx var text)
  set(${var} "(^|\n)[ ]*${text}(\n|$)" PARENT_SCOPE)
endfunction()

if(MODE STREQUAL "registered")
  set(checker "${SCRIPTS}/inventory_equal.cmake")
  # registered(<case> <code> <regex> <registered rows> <inventory rows>)
  function(registered case code regex reg inv)
    file(WRITE "${WORK}/${case}.registered.txt" "${reg}")
    file(WRITE "${WORK}/${case}.inventory.txt" "${header}${inv}")
    expect("${case}" "${code}" "${regex}" "${CMAKE_COMMAND}"
      "-DREGISTERED=${WORK}/${case}.registered.txt"
      "-DINVENTORY=${WORK}/${case}.inventory.txt" -P "${checker}")
  endfunction()

  set(ab "a.b\tx\n")
  set(cd "c.d\ty\n")
  set(ef "e.f\ty\n")

  registered(equal 0 "inventory: registered tests equal" "${ab}${cd}" "${ab}${cd}")
  # FND-06 ordering edge: registration order is irrelevant, the file is sorted.
  registered(registration_order_free 0 "inventory: registered tests equal" "${cd}${ab}" "${ab}${cd}")
  # FND-06 adjacency edge: x.y and x.y.self_test are two rows; tab sorts first.
  set(xy "x.y\tp\n")
  set(xyself "x.y.self_test\tp\n")
  registered(adjacent_names 0 "inventory: registered tests equal" "${xyself}${xy}" "${xy}${xyself}")
  line_rx(rx "inventory: not sorted")
  registered(adjacent_names_swapped 1 "${rx}" "${xy}${xyself}" "${xyself}${xy}")
  line_rx(rx "inventory: registered but not listed: e[.]f")
  registered(missing 1 "${rx}" "${ab}${cd}${ef}" "${ab}${cd}")
  line_rx(rx "inventory: listed but not registered: e[.]f")
  registered(extra 1 "${rx}" "${ab}${cd}" "${ab}${cd}${ef}")
  line_rx(rx "inventory: not sorted")
  registered(unsorted 1 "${rx}" "${ab}${cd}" "${cd}${ab}")
  line_rx(rx "inventory: duplicate row: a[.]b")
  registered(duplicate 1 "${rx}" "${ab}${cd}" "${ab}${ab}${cd}")
  line_rx(rx "inventory: labels differ: a[.]b: registered x, listed z")
  registered(labels_differ 1 "${rx}" "${ab}${cd}" "a.b\tz\n${cd}")
  line_rx(rx "inventory: registered but not listed: a[.]b")
  registered(empty_inventory 1 "${rx}" "${ab}" "")
else()
  set(checker "${SCRIPTS}/check_junit.cmake")

  # tc(<var> <name> <kind>): one testcase in the shape CTest writes.
  function(tc var name kind)
    set(open "\t<testcase name=\"${name}\" classname=\"${name}\" time=\"0.1\" status=\"")
    if(kind STREQUAL "run")
      set(text "${open}run\">\n\t\t<system-out></system-out>\n\t</testcase>\n")
    elseif(kind STREQUAL "fail")
      set(text "${open}fail\">\n\t\t<failure message=\"Failed\"/>\n\t</testcase>\n")
    elseif(kind STREQUAL "error")
      set(text "${open}run\">\n\t\t<error message=\"Crashed\"/>\n\t</testcase>\n")
    elseif(kind STREQUAL "notrun")
      set(text "${open}notrun\">\n\t\t<system-out>Not Run</system-out>\n\t</testcase>\n")
    elseif(kind STREQUAL "skipped")
      set(text "${open}notrun\">\n\t\t<skipped message=\"Skipped\"/>\n\t</testcase>\n")
    else()
      message(FATAL_ERROR "inventory_controls: unknown testcase kind ${kind}")
    endif()
    set(${var} "${text}" PARENT_SCOPE)
  endfunction()

  # junit(<case> <code> <regex> <inventory rows> <skips rows> <testcases...>)
  function(junit case code regex inv skips)
    set(body "")
    foreach(pair IN LISTS ARGN)
      string(REPLACE ":" ";" pair "${pair}")
      list(GET pair 0 name)
      list(GET pair 1 kind)
      tc(one "${name}" "${kind}")
      string(APPEND body "${one}")
    endforeach()
    file(WRITE "${WORK}/${case}.junit.xml"
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite name=\"(empty)\" tests=\"0\" failures=\"0\" disabled=\"0\" skipped=\"0\" hostname=\"\" time=\"0\">\n${body}</testsuite>\n")
    file(WRITE "${WORK}/${case}.inventory.txt" "${header}${inv}")
    file(WRITE "${WORK}/${case}.skips.txt" "# name<TAB>reason\n${skips}")
    expect("${case}" "${code}" "${regex}" "${CMAKE_COMMAND}"
      "-DJUNIT=${WORK}/${case}.junit.xml"
      "-DINVENTORY=${WORK}/${case}.inventory.txt"
      "-DSKIPS=${WORK}/${case}.skips.txt" -P "${checker}")
  endfunction()

  set(inv "a.b\tx\nc.d\tx\n")
  # The skips file holding only a header comment means no skips (FND-06 empty edge).
  junit(all_run 0 "check_junit: 2 tests accounted for once each" "${inv}" "" a.b:run c.d:run)
  # FND-06 adjacency edge: x.y and x.y.self_test are distinct inventory rows.
  set(adj "x.y\tp\nx.y.self_test\tp\n")
  junit(adjacent_names 0 "check_junit: 2 tests accounted for once each" "${adj}" "" x.y.self_test:run x.y:run)
  line_rx(rx "x[.]y: in the JUnit file 2 times, expected once")
  junit(adjacent_names_twice 1 "${rx}" "${adj}" "" x.y:run x.y:run)
  line_rx(rx "x[.]y[.]self_test: in the JUnit file 0 times, expected once")
  junit(adjacent_names_missing 1 "${rx}" "${adj}" "" x.y:run x.y:run)
  line_rx(rx "c[.]d: in the JUnit file 0 times, expected once")
  junit(junit_missing 1 "${rx}" "${inv}" "" a.b:run)
  line_rx(rx "a[.]b: in the JUnit file 2 times, expected once")
  junit(junit_duplicate 1 "${rx}" "${inv}" "" a.b:run a.b:run c.d:run)
  line_rx(rx "e[.]f: ran but is not in the inventory")
  junit(junit_extra 1 "${rx}" "${inv}" "" a.b:run c.d:run e.f:run)
  line_rx(rx "c[.]d: failed, errored or did not run")
  junit(failed 1 "${rx}" "${inv}" "" a.b:run c.d:fail)
  junit(errored 1 "${rx}" "${inv}" "" a.b:run c.d:error)
  junit(did_not_run 1 "${rx}" "${inv}" "" a.b:run c.d:notrun)
  line_rx(rx "c[.]d: skipped but not declared in skips.txt")
  junit(undeclared_skip 1 "${rx}" "${inv}" "" a.b:run c.d:skipped)
  junit(declared_skip 0 "check_junit: 2 tests accounted for once each [(]1 declared skips[)]"
    "${inv}" "c.d\tneeds the owner BIOS\n" a.b:run c.d:skipped)
  line_rx(rx "skips[.]txt: c[.]d: empty reason")
  junit(empty_reason 1 "${rx}" "${inv}" "c.d\t\n" a.b:run c.d:skipped)
  line_rx(rx "c[.]d: declared as skipped but ran")
  junit(stale_skip 1 "${rx}" "${inv}" "c.d\tstale declaration\n" a.b:run c.d:run)
  line_rx(rx "junit: no test cases")
  junit(empty_junit 1 "${rx}" "${inv}" "")
endif()
message(STATUS "inventory_controls: ${MODE}: all cases ok")
