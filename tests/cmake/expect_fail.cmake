# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
# Negative-control runner, used as
#   cmake -DCMD=<command list> -DCODE=<n> -DREGEX=<rule> -P expect_fail.cmake
# Fault prevented: a negative control that passes on any failure. A bare
# WILL_FAIL accepts a crash, a typo or a missing file as success; this passes
# only when CMD exits with exactly CODE and its output matches REGEX.
foreach(var CMD CODE REGEX)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "expect_fail: -D${var} is required")
  endif()
endforeach()

# Naming one variable for both pipes merges them in the order produced.
execute_process(COMMAND ${CMD} RESULT_VARIABLE rc
  OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc STREQUAL CODE OR NOT out MATCHES "${REGEX}")
  message(FATAL_ERROR "expect_fail: expected exit ${CODE} and output matching\n"
    "  ${REGEX}\ngot exit ${rc} and output\n${out}")
endif()
message(STATUS "expect_fail: exit ${rc} and output matching ${REGEX}")
