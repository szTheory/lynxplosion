# Probe identity check, run as
#   cmake -DLNX=<probe.lnx> -DMANIFEST=<cc65-probe.manifest> -P toolchain_probe.cmake
# Fault prevented: a cc65 toolchain whose linked output differs from the one the
# probe manifest records. The probe .lnx is built by the pinned cc65 in the probe
# step of scripts/check.sh; its SHA-256 must equal the manifest's "sha256" key.
cmake_minimum_required(VERSION 3.25)
foreach(var LNX MANIFEST)
  if("${${var}}" STREQUAL "")
    message(FATAL_ERROR "toolchain.probe: -D${var} is required")
  endif()
endforeach()
# FATAL_ERROR wraps long lines, so the finding is printed whole with NOTICE and
# the error that follows only fails the run.
if(NOT EXISTS "${LNX}")
  message(NOTICE "toolchain.probe: ${LNX} missing; run scripts/check.sh")
  message(FATAL_ERROR "toolchain.probe: failed")
endif()
if(NOT EXISTS "${MANIFEST}")
  message(FATAL_ERROR "toolchain.probe: ${MANIFEST} missing")
endif()
file(STRINGS "${MANIFEST}" sha_lines REGEX "^sha256 = ")
list(LENGTH sha_lines sha_count)
if(NOT sha_count EQUAL 1)
  message(FATAL_ERROR "toolchain.probe: ${MANIFEST} has ${sha_count} sha256 lines, not 1")
endif()
string(REGEX REPLACE "^sha256 = " "" want "${sha_lines}")
file(SHA256 "${LNX}" got)
if(NOT got STREQUAL want)
  message(NOTICE
    "toolchain.probe: probe.lnx sha256 ${got} differs from ${MANIFEST} (${want})")
  message(FATAL_ERROR "toolchain.probe: failed")
endif()
message(STATUS "toolchain.probe: probe.lnx sha256 ${got} matches the manifest")
