# CPU tests: the profile tables and the cycle-stepped core on a flat test bus.
add_executable(lynx-test-cpu ${CMAKE_CURRENT_LIST_DIR}/test_cpu.c)
target_link_libraries(lynx-test-cpu PRIVATE lynx_core)
# The white-box checks read the CPU instance and tables, so the tests see core/.
target_include_directories(lynx-test-cpu PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-cpu)
lynx_sanitize(lynx-test-cpu)
set_target_properties(lynx-test-cpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

# The per-opcode vector runner and its strict reader (test-side code, not core).
add_executable(lynx-test-cpu-vectors
  ${CMAKE_CURRENT_LIST_DIR}/test_cpu_vectors.c ${CMAKE_CURRENT_LIST_DIR}/sst_reader.c)
target_link_libraries(lynx-test-cpu-vectors PRIVATE lynx_core)
target_include_directories(lynx-test-cpu-vectors PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-cpu-vectors)
lynx_sanitize(lynx-test-cpu-vectors)
set_target_properties(lynx-test-cpu-vectors PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

# The reader rejects every listed malformation with a located message (exit 1
# here means a malformed input was accepted or reported without its location).
lynx_add_test(NAME cpu.vectors.reader_malformed COMMAND lynx-test-cpu-vectors self-test-reader
  LABELS cpu TIMEOUT 60)

lynx_add_test(NAME cpu.cycles COMMAND lynx-test-cpu cycles LABELS cpu TIMEOUT 30)
lynx_add_test(NAME cpu.tables COMMAND lynx-test-cpu tables LABELS cpu TIMEOUT 30)

# The include guard for the CPU: core/cpu* may include only cpu.h, the profile
# def and four system headers, so the CPU cannot reach memory, time or the
# machine. The control builds a fixture under the scratch directory holding two
# plain violations, two allowed names, a prefix and a suffix of an allowed name
# and an allowed name in angle brackets; the count (5) shows the allowed ones
# passed and the look-alikes did not.
set(lynx_cpu_scripts ${PROJECT_SOURCE_DIR}/tests/cmake)
lynx_add_test(NAME core.cpu_includes
  COMMAND ${CMAKE_COMMAND} "-DROOT=${PROJECT_SOURCE_DIR}" -P ${lynx_cpu_scripts}/cpu_includes.cmake
  LABELS core TIMEOUT 30)
set(lynx_cpu_includes_fixture ${LYNX_SCRATCH_DIR}/gates/cpu_includes)
file(WRITE ${lynx_cpu_includes_fixture}/core/cpu_x.c
  "#include \"internal.h\"\n#include <stdio.h>\n#include \"cpu.h\"\n#include \"cpu_profile.def\"\n#include \"cpu.hx\"\n#include \"sub/cpu.h\"\n#include <cpu.h>\n")
set(lynx_cpu_includes_cmd ${CMAKE_COMMAND} -DROOT=${lynx_cpu_includes_fixture}
  -P ${lynx_cpu_scripts}/cpu_includes.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_cpu_includes_cmd "${lynx_cpu_includes_cmd}")
lynx_add_test(NAME core.cpu_includes.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_cpu_includes_cmd}" -DCODE=1
    "-DREGEX=cpu_x.c:1: #include .internal.h..*cpu_x.c:2: #include <stdio.h>.*cpu_x.c:5: #include .cpu.hx..*cpu_x.c:6: #include .sub/cpu.h..*cpu_x.c:7: #include <cpu.h>.*cpu_includes: 5 lines"
    -P ${lynx_cpu_scripts}/expect_fail.cmake
  LABELS core TIMEOUT 30)

# Control for the table check: a test-local copy of the table with the lynx2 row
# of $69 changed must make the differ-set check fail with exit 1.
set(lynx_cpu_mutant_cmd $<TARGET_FILE:lynx-test-cpu> tables_mutant)
string(REPLACE ";" "$<SEMICOLON>" lynx_cpu_mutant_cmd "${lynx_cpu_mutant_cmd}")
lynx_add_test(NAME cpu.tables.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_cpu_mutant_cmd}" -DCODE=1 "-DREGEX=differ set"
    -P ${lynx_cpu_scripts}/expect_fail.cmake
  LABELS cpu TIMEOUT 30)
