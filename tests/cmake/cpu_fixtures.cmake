# CPU fixture assembly check, run as
#   cmake -DBINDIR=<dir> -DHEXDIR=<dir> -DNAMES=<a,b> -P cpu_fixtures.cmake
# Fault prevented: committed hex that differs from what the pinned ca65/ld65
# produce from the committed source. For each name, <BINDIR>/<name>.bin is built
# by the cpu-fixtures step of scripts/check.sh; <HEXDIR>/<name>.hex (comment
# lines starting with "#", whitespace-separated byte pairs) must decode to
# exactly the same bytes.
cmake_minimum_required(VERSION 3.25)
foreach(var BINDIR HEXDIR NAMES)
  if("${${var}}" STREQUAL "")
    message(FATAL_ERROR "cpu_fixtures: -D${var} is required")
  endif()
endforeach()
string(REPLACE "," ";" name_list "${NAMES}")

# FATAL_ERROR wraps long lines, so findings are printed whole with NOTICE and
# the error that follows only fails the run.
foreach(name IN LISTS name_list)
  set(bin "${BINDIR}/${name}.bin")
  set(hex "${HEXDIR}/${name}.hex")
  if(NOT EXISTS "${bin}")
    message(NOTICE "cpu_fixtures: ${bin} missing; run scripts/check.sh")
    message(FATAL_ERROR "cpu_fixtures: failed")
  endif()
  if(NOT EXISTS "${hex}")
    message(FATAL_ERROR "cpu_fixtures: ${hex} missing")
  endif()

  file(READ "${bin}" got HEX)

  file(STRINGS "${hex}" hex_lines)
  set(want "")
  foreach(line IN LISTS hex_lines)
    if(line MATCHES "^#")
      continue()
    endif()
    string(REGEX REPLACE "[ \t\r]" "" line "${line}")
    string(APPEND want "${line}")
  endforeach()
  string(TOLOWER "${want}" want)

  if(NOT want MATCHES "^([0-9a-f][0-9a-f])+$")
    message(NOTICE "cpu_fixtures: ${hex} is not whole hex byte pairs")
    message(FATAL_ERROR "cpu_fixtures: ${name}.hex is not valid hex text")
  endif()

  string(LENGTH "${got}" got_len)
  string(LENGTH "${want}" want_len)
  math(EXPR got_bytes "${got_len} / 2")
  math(EXPR want_bytes "${want_len} / 2")
  if(NOT got STREQUAL want)
    # First differing byte offset; a length mismatch differs at the shorter end.
    if(got_len LESS want_len)
      set(common ${got_len})
    else()
      set(common ${want_len})
    endif()
    set(offset ${common})
    set(pos 0)
    while(pos LESS common)
      string(SUBSTRING "${got}" ${pos} 2 got_byte)
      string(SUBSTRING "${want}" ${pos} 2 want_byte)
      if(NOT got_byte STREQUAL want_byte)
        set(offset ${pos})
        break()
      endif()
      math(EXPR pos "${pos} + 2")
    endwhile()
    math(EXPR byte_offset "${offset} / 2")
    message(NOTICE
      "cpu_fixtures: ${bin} (${got_bytes} bytes) differs from ${hex} (${want_bytes} bytes) at byte offset ${byte_offset}")
    message(FATAL_ERROR "cpu_fixtures: ${name} differs from ${hex}")
  endif()
  message(STATUS "cpu_fixtures: ${name}.hex matches the assembled binary (${got_bytes} bytes)")
endforeach()
