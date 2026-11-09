# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
# Fault prevented: floating point entering the integer-only core. Constant
# folding leaves no trace of a float literal in an object file, so this text
# scan reports every line of a .c or .h file under ROOTS that names float,
# double, <math.h> or <float.h>, or holds a hex floating literal (0x1p3) or,
# once hex integers, cited section numbers ("section 6.2") and SPDX licence
# expressions are removed, a decimal one (1.5, 1.e3, 1e3). An expression is
# removed up to the first character none uses, such as the closing */, so the
# rest of its line is still scanned. Comments count, so they write numbers as
# integers.
#
#   cmake "-DROOTS=<dir>;<dir>" -P no_float.cmake
cmake_minimum_required(VERSION 3.25)

set(found "")
set(count 0)
foreach(root IN LISTS ROOTS)
  if(NOT IS_DIRECTORY "${root}")
    message(FATAL_ERROR "no_float: ${root} is not a directory")
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
      # Hex floats are matched before hex integers are removed, since the
      # removal would cut them apart.
      string(REGEX REPLACE
        "0[xX][0-9a-fA-F]+|section [0-9.]*[0-9]|SPDX-License-Identifier: [A-Za-z0-9 .+()-]*"
        "" s " ${line} ")
      if(line MATCHES "0[xX][0-9a-fA-F]*\\.?[0-9a-fA-F]*[pP][+-]?[0-9]"
          OR s MATCHES "[^A-Za-z0-9_](float|double)[^A-Za-z0-9_]"
          OR s MATCHES "<math\\.h>"
          OR s MATCHES "[0-9]\\.[0-9]"
          OR s MATCHES "[0-9]\\.[eE]"
          OR s MATCHES "[^A-Za-z0-9_][0-9]+[eE][+-]?[0-9]")
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
  message(FATAL_ERROR "no_float: ${lines} lines with floating point, listed above")
endif()
message(STATUS "no_float: ${count} files scanned, no floating point")
