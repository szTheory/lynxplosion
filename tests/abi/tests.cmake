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

# Link layer. Every nm check reads lynx_core_plain, the uninstrumented twin;
# the sanitized lynx_core is never an input (its objects hold writable data
# and runtime symbols). The two fixture libraries are negative controls built
# with the twin's own compile options, so each check fails where it runs.
set(lynx_abi_nm ${CMAKE_NM})
if(NOT lynx_abi_nm)
  find_program(LYNX_NM NAMES llvm-nm nm REQUIRED)
  set(lynx_abi_nm ${LYNX_NM})
endif()
set(lynx_abi_args -DNM=${lynx_abi_nm} "-DSYSTEM=${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")

get_target_property(lynx_abi_options lynx_core_plain COMPILE_OPTIONS)
foreach(fixture IN ITEMS libc global)
  add_library(lynx_abi_${fixture}_fixture STATIC ${CMAKE_CURRENT_LIST_DIR}/${fixture}_fixture.c)
  target_compile_options(lynx_abi_${fixture}_fixture PRIVATE ${lynx_abi_options})
  set_target_properties(lynx_abi_${fixture}_fixture PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests/abi)
endforeach()

foreach(mode IN ITEMS undefined globals)
  lynx_add_test(NAME core.nm_${mode}
    COMMAND ${CMAKE_COMMAND} -DMODE=${mode} -DLIB=$<TARGET_FILE:lynx_core_plain>
      ${lynx_abi_args} -P ${lynx_abi_scripts}/abi.cmake
    LABELS core TIMEOUT 30)
endforeach()
lynx_add_test(NAME api.abi_symbols
  COMMAND ${CMAKE_COMMAND} -DMODE=symbols -DLIB=$<TARGET_FILE:lynx_core_plain>
    ${lynx_abi_args} -DHEADER=${PROJECT_SOURCE_DIR}/include/lynxplosion.h
    -P ${lynx_abi_scripts}/abi.cmake
  LABELS api TIMEOUT 30)

# CMake wraps a long message at a column that depends on the build path, so
# the regexes allow a line break anywhere in the finding's title.
foreach(case IN ITEMS "undefined;libc;unexpected[^:]*symbol:.*time"
                      "globals;global;writable[^:]*symbol:.*counter")
  list(GET case 0 mode)
  list(GET case 1 fixture)
  list(GET case 2 regex)
  set(lynx_abi_cmd ${CMAKE_COMMAND} -DMODE=${mode}
    -DLIB=$<TARGET_FILE:lynx_abi_${fixture}_fixture>
    ${lynx_abi_args} -P ${lynx_abi_scripts}/abi.cmake)
  string(REPLACE ";" "$<SEMICOLON>" lynx_abi_cmd "${lynx_abi_cmd}")
  lynx_add_test(NAME core.nm_${mode}.self_test
    COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_abi_cmd}" -DCODE=1 "-DREGEX=${regex}"
      -P ${lynx_abi_scripts}/expect_fail.cmake
    LABELS core TIMEOUT 30)
endforeach()

# Self-test of the symbols mode: a copy of the real header with one more
# declared function that nothing defines. The copy is written at configure
# time under the scratch directory.
file(READ ${PROJECT_SOURCE_DIR}/include/lynxplosion.h lynx_abi_header)
string(REPLACE "#ifdef __cplusplus\n}\n#endif"
  "lynx_status lynx_not_defined(void);\n\n#ifdef __cplusplus\n}\n#endif"
  lynx_abi_header "${lynx_abi_header}")
file(WRITE ${LYNX_SCRATCH_DIR}/gates/abi_symbols/lynxplosion.h "${lynx_abi_header}")
set(lynx_abi_cmd ${CMAKE_COMMAND} -DMODE=symbols -DLIB=$<TARGET_FILE:lynx_core_plain>
  ${lynx_abi_args} -DHEADER=${LYNX_SCRATCH_DIR}/gates/abi_symbols/lynxplosion.h
  -P ${lynx_abi_scripts}/abi.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_abi_cmd "${lynx_abi_cmd}")
lynx_add_test(NAME api.abi_symbols.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_abi_cmd}" -DCODE=1
    "-DREGEX=declared, not defined:[^\n]*\n? *lynx_not_defined"
    -P ${lynx_abi_scripts}/expect_fail.cmake
  LABELS api TIMEOUT 30)

# The public header compiles alone as C17 with warnings fatal; the program
# exits 0. Its static assertions pin the status, stop-reason, config and
# identity layouts.
lynx_add_test(NAME api.header_selfcontained
  COMMAND lynx-test-header
  LABELS api TIMEOUT 30)
