# Fault prevented: the CPU reaching memory, time or the machine through a
# header. core/cpu*.c and core/cpu*.h under ROOT may include only the quoted
# project files cpu.h and cpu_profile.def and the system headers stdint.h,
# stdbool.h, stddef.h and string.h. Names are compared whole, so a name that only
# starts or ends like an allowed one ("cpu.hx", "sub/cpu.h") or an allowed quoted
# name in angle brackets is a finding. Each finding is printed as
# <file>:<line>: <directive> before the failure. A scan that finds no cpu file
# fails, so the gate cannot pass vacuously.
#
#   cmake -DROOT=<repository root> -P cpu_includes.cmake
cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

set(allowed_quoted cpu.h cpu_profile.def)
set(allowed_system stdint.h stdbool.h stddef.h string.h)

if(NOT IS_DIRECTORY "${ROOT}/core")
  message(FATAL_ERROR "cpu_includes: ${ROOT}/core is not a directory")
endif()
file(GLOB files "${ROOT}/core/cpu*.c" "${ROOT}/core/cpu*.h")
list(LENGTH files count)
if(count EQUAL 0)
  message(FATAL_ERROR "cpu_includes: no core/cpu*.c or core/cpu*.h under ${ROOT}")
endif()

set(found "")
foreach(file IN LISTS files)
  file(READ "${file}" text)
  # One list element per line: neutralise the characters that split or join
  # list elements, then split on newlines.
  string(REPLACE "\r" "" text "${text}")
  string(REPLACE "\\" "/" text "${text}")
  string(REPLACE ";" "," text "${text}")
  string(REPLACE "[" "(" text "${text}")
  string(REPLACE "]" ")" text "${text}")
  string(REPLACE "\n" ";" text "${text}")
  set(n 0)
  foreach(line IN LISTS text)
    math(EXPR n "${n} + 1")
    if(NOT line MATCHES "^[ \t]*#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
      continue()
    endif()
    set(open "${CMAKE_MATCH_1}")
    set(name "${CMAKE_MATCH_2}")
    set(ok FALSE)
    if(open STREQUAL "<")
      if(name IN_LIST allowed_system)
        set(ok TRUE)
      endif()
    elseif(name IN_LIST allowed_quoted)
      set(ok TRUE)
    endif()
    if(NOT ok)
      list(APPEND found "${file}:${n}: ${line}")
    endif()
  endforeach()
endforeach()

list(LENGTH found lines)
if(lines GREATER 0)
  foreach(line IN LISTS found)
    message(STATUS "${line}")
  endforeach()
  message(FATAL_ERROR "cpu_includes: ${lines} lines with a disallowed include, listed above")
endif()
message(STATUS "cpu_includes: ${count} files scanned, every include allowed")
