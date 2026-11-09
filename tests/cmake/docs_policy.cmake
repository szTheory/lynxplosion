# Policy-document check, used as
#   cmake -DROOT=<repository or fixture root> -P docs_policy.cmake
# Fault prevented: the clean-room draft, the per-source exposure record or the
# provenance notice losing a required statement without anything failing. It
# reads ROOT/docs/policy/clean-room.md, ROOT/docs/policy/exposure-record.md and
# ROOT/docs/provenance.md, reports every finding, then fails.
if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "")
  message(FATAL_ERROR "docs_policy: -DROOT is required")
endif()

set(banner "DRAFT pending owner decision O6")
set(findings "")

function(read_doc name path out)
  if(NOT EXISTS "${path}")
    set(findings "${findings}docs_policy: ${name}: file is missing\n" PARENT_SCOPE)
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  file(READ "${path}" text)
  set(${out} "${text}" PARENT_SCOPE)
endfunction()

# clean-room.md: the draft banner and the four emulators it forbids opening.
read_doc(clean-room.md "${ROOT}/docs/policy/clean-room.md" clean_room)
string(FIND "${clean_room}" "${banner}" pos)
if(pos EQUAL -1)
  string(APPEND findings "docs_policy: clean-room.md: missing banner\n")
endif()
foreach(name Gearlynx MAME Holani Beetle)
  string(FIND "${clean_room}" "${name}" pos)
  if(pos EQUAL -1)
    string(APPEND findings "docs_policy: clean-room.md: does not name ${name}\n")
  endif()
endforeach()

# exposure-record.md: the banner and one table row per named source. A row is a
# table line that starts with the source name (case-sensitive).
read_doc(exposure-record.md "${ROOT}/docs/policy/exposure-record.md" exposure)
string(FIND "${exposure}" "${banner}" pos)
if(pos EQUAL -1)
  string(APPEND findings "docs_policy: exposure-record.md: missing banner\n")
endif()
foreach(name Gearlynx MAME Holani Beetle Handy lynx-tests)
  if(NOT "${exposure}" MATCHES "(^|\n)\\|[ ]*${name}")
    string(APPEND findings "docs_policy: exposure-record.md: no row for ${name}\n")
  endif()
endforeach()

# provenance.md: the owner project and the commit the helpers were taken at.
read_doc(provenance.md "${ROOT}/docs/provenance.md" provenance)
string(FIND "${provenance}" "7fd0386" pos)
if(pos EQUAL -1)
  string(APPEND findings "docs_policy: provenance.md: missing 7fd0386\n")
endif()

# Findings go out as status lines: FATAL_ERROR text is re-wrapped at 79 columns,
# which would split a finding across lines.
if(NOT findings STREQUAL "")
  string(STRIP "${findings}" findings)
  message(STATUS "${findings}")
  message(FATAL_ERROR "docs_policy: findings above")
endif()
message(STATUS "docs_policy: ok")
