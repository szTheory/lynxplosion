# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
# Link-surface checks on a static library, run as
#   cmake -DMODE=<undefined|globals|symbols> -DLIB=<library> -DNM=<nm>
#         -DSYSTEM=<system>/<processor> [-DHEADER=<lynxplosion.h>] -P abi.cmake
# Faults prevented, one per mode:
#   undefined  the core calling libc, libm or a floating-point helper. Every
#              undefined symbol that no member defines must be one of the C
#              memory functions or a compiler helper listed for SYSTEM below.
#   globals    mutable state outside the instance: no writable data symbol
#              (nm types B b D d C), so const integer tables only.
#   symbols    a public name the header does not declare, or a declared
#              function nothing defines. Defined external lynx_* names
#              equal HEADER's functions both ways; every other defined
#              external name starts lynx__.
cmake_minimum_required(VERSION 3.25)
foreach(var MODE LIB NM SYSTEM)
  if("${${var}}" STREQUAL "")
    message(FATAL_ERROR "abi: -D${var} is required")
  endif()
endforeach()
if(NOT EXISTS "${LIB}" OR IS_DIRECTORY "${LIB}")
  message(FATAL_ERROR "abi: library ${LIB} does not exist")
endif()

# Per-target compiler helpers, "system regex:names". Nothing else is allowed:
# the core builds with -fno-stack-protector -U_FORTIFY_SOURCE, so no
# __stack_chk_* or __*_chk name ever appears.
set(helpers
  # Apple clang 21 emits ___chkstk_darwin for the large local snapshot/video
  # buffer in snapshot.c.o. nm -P reports that Mach-O spelling; CMake's global
  # ^_ replacement below removes every leading underscore, so the exact name
  # checked here is chkstk_darwin. Do not broaden this to arbitrary helpers.
  "^Darwin/:chkstk_darwin"
  # Apple clang lowers a zeroing memset to bzero (measured: nm -u on the core,
  # Apple clang 21, -O3).
  "^Darwin/:bzero __bzero"
  # x86-64 ELF position-independent code names the linker-defined GOT symbol
  # (System V x86-64 psABI, section 3.5).
  "^Linux/(x86_64|amd64|AMD64)$:_GLOBAL_OFFSET_TABLE_")
set(allowed memcpy memmove memset memcmp)
foreach(row IN LISTS helpers)
  string(REGEX MATCH "^([^:]*):(.*)$" unused "${row}")
  set(names "${CMAKE_MATCH_2}")
  if(SYSTEM MATCHES "${CMAKE_MATCH_1}")
    separate_arguments(names UNIX_COMMAND "${names}")
    list(APPEND allowed ${names})
  endif()
endforeach()

# nm -P prints "name type [value size]" per symbol and a header per member.
execute_process(COMMAND "${NM}" -P "${LIB}" RESULT_VARIABLE rc
  OUTPUT_VARIABLE text ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "abi: ${NM} -P ${LIB} failed (${rc}): ${err}")
endif()
string(REPLACE "\n" ";" lines "${text}")
foreach(line IN LISTS lines)
  if(NOT line MATCHES "^([^ ]+) ([A-Za-z])( |$)")
    continue()
  endif()
  set(name "${CMAKE_MATCH_1}")
  set(type "${CMAKE_MATCH_2}")
  if(SYSTEM MATCHES "^Darwin/")
    string(REGEX REPLACE "^_" "" name "${name}") # CMake removes all leading Mach-O underscores
  endif()
  if(type STREQUAL "U")
    list(APPEND undefined "${name}")
    continue()
  endif()
  list(APPEND defined "${name}")
  if(type MATCHES "^[BbDdC]$")
    list(APPEND writable "${type} ${name}")
  endif()
  if(type MATCHES "^[A-Z]$")
    list(APPEND external "${name}")
  endif()
endforeach()

set(bad "")
if(MODE STREQUAL "undefined")
  list(REMOVE_ITEM undefined ${defined} ${allowed})
  set(bad ${undefined})
  set(what "unexpected undefined symbol")
elseif(MODE STREQUAL "globals")
  set(bad ${writable})
  set(what "writable data symbol")
elseif(MODE STREQUAL "symbols")
  file(READ "${HEADER}" header)
  string(REGEX MATCHALL "lynx_[a-z0-9_]*\\(" declared "${header}")
  string(REPLACE "(" "" declared "${declared}")
  set(public ${external})
  list(FILTER public INCLUDE REGEX "^lynx_[^_]")
  list(FILTER external EXCLUDE REGEX "^lynx_")
  set(missing ${declared})
  list(REMOVE_ITEM missing ${public})
  list(REMOVE_ITEM public ${declared})
  list(TRANSFORM missing PREPEND "declared, not defined: ")
  list(TRANSFORM public PREPEND "defined, not declared: ")
  list(TRANSFORM external PREPEND "outside the lynx_ namespace: ")
  set(bad ${missing} ${public} ${external})
  set(what "public name mismatch")
else()
  message(FATAL_ERROR "abi: unknown -DMODE=${MODE}")
endif()
list(REMOVE_DUPLICATES bad)
if(bad)
  list(JOIN bad "\n  " bad)
  message(FATAL_ERROR "abi ${MODE}: ${LIB}: ${what}:\n  ${bad}")
endif()
message(STATUS "abi ${MODE}: ${LIB} ok")
