# Fault prevented: the core reaching a host facility through a header. Every
# #include in a .c or .h file under ROOTS must name stdint.h, stddef.h,
# stdbool.h or string.h, or a quoted project header that exists in the same
# root (lynxplosion.h always counts). assert.h is refused because it adds
# __assert_* symbols, limits.h because nothing in the core needs it. Each
# other include is reported as <file>:<line>: <text>, all of them before the
# failure.
#
#   cmake "-DROOTS=<dir>;<dir>" -P core_includes.cmake
cmake_minimum_required(VERSION 3.25)

set(allowed_system stdint.h stddef.h stdbool.h string.h)
set(found "")
set(count 0)
foreach(root IN LISTS ROOTS)
  if(NOT IS_DIRECTORY "${root}")
    message(FATAL_ERROR "core_includes: ${root} is not a directory")
  endif()
  file(GLOB_RECURSE files "${root}/*.c" "${root}/*.h")
  foreach(file IN LISTS files)
    math(EXPR count "${count} + 1")
    file(READ "${file}" text)
    # One list element per line: neutralise the characters that split or
    # join list elements, then split on newlines.
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
      elseif(name STREQUAL "lynxplosion.h" OR EXISTS "${root}/${name}")
        set(ok TRUE)
      endif()
      if(NOT ok)
        list(APPEND found "${file}:${n}: ${line}")
      endif()
    endforeach()
  endforeach()
endforeach()

list(LENGTH found lines)
if(lines GREATER 0)
  foreach(line IN LISTS found)
    message("${line}")
  endforeach()
  message(FATAL_ERROR "core_includes: ${lines} lines with a disallowed include, listed above")
endif()
message(STATUS "core_includes: ${count} files scanned, every include allowed")
