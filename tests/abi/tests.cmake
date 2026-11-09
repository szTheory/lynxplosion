# Determinism boundary of the core (FND-05), in two layers. Source layer: an
# include allowlist and a floating-point scan over core/ and include/. Each
# scan has a self-test that runs it over a fixture holding the violation and
# requires the exact finding. Fixtures are written at configure time under
# LYNX_SCRATCH_DIR, never committed.
set(lynx_abi_scripts ${CMAKE_CURRENT_LIST_DIR}/../cmake)
set(lynx_core_roots "${PROJECT_SOURCE_DIR}/core$<SEMICOLON>${PROJECT_SOURCE_DIR}/include")

lynx_add_test(NAME core.includes
  COMMAND ${CMAKE_COMMAND} "-DROOTS=${lynx_core_roots}"
    -P ${lynx_abi_scripts}/core_includes.cmake
  LABELS core TIMEOUT 30)
lynx_add_test(NAME core.no_float
  COMMAND ${CMAKE_COMMAND} "-DROOTS=${lynx_core_roots}"
    -P ${lynx_abi_scripts}/no_float.cmake
  LABELS core TIMEOUT 30)

# Self-test of the include scan: a disallowed system include, a second one, and
# a quoted project header that exists beside the sample. The count in the
# finding (2) shows the quoted include was accepted.
set(lynx_includes_fixture ${LYNX_SCRATCH_DIR}/gates/core_includes)
file(WRITE ${lynx_includes_fixture}/internal.h "/* project header */\n")
file(WRITE ${lynx_includes_fixture}/sample.c
  "#include <stdio.h>\n#include <time.h>\n#include \"internal.h\"\n")
set(lynx_includes_cmd ${CMAKE_COMMAND} -DROOTS=${lynx_includes_fixture}
  -P ${lynx_abi_scripts}/core_includes.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_includes_cmd "${lynx_includes_cmd}")
lynx_add_test(NAME core.includes.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_includes_cmd}" -DCODE=1
    "-DREGEX=sample.c:1: #include <stdio.h>.*sample.c:2: #include <time.h>.*core_includes: 2 lines"
    -P ${lynx_abi_scripts}/expect_fail.cmake
  LABELS core TIMEOUT 30)

# Self-test of the float scan: a decimal literal is reported; a hex integer
# and an identifier that merely contains a type word are not (the count is 1).
# The violating text is written here, never committed.
set(lynx_float_fixture ${LYNX_SCRATCH_DIR}/gates/core_no_float)
file(WRITE ${lynx_float_fixture}/sample.c
  "int y = 0x1E5;\nx = 1.5;\nint double_free;\n")
set(lynx_float_cmd ${CMAKE_COMMAND} -DROOTS=${lynx_float_fixture}
  -P ${lynx_abi_scripts}/no_float.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_float_cmd "${lynx_float_cmd}")
lynx_add_test(NAME core.no_float.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_float_cmd}" -DCODE=1
    "-DREGEX=sample.c:2: x = 1.5.*no_float: 1 lines"
    -P ${lynx_abi_scripts}/expect_fail.cmake
  LABELS core TIMEOUT 30)
