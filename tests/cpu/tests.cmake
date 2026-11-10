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

# The pinned per-opcode vectors for every defined profile. Each test checks the
# sample hash against its fixture manifest, compares the plan it derives from the
# sample and the profile table with the committed inventory byte for byte, then
# runs every non-excluded vector through the CPU the machine uses. none uses the
# synertek data (same data as lynx1) and also asserts that the 32 bit-op opcodes
# stop as undefined.
set(lynx_vec_dir ${PROJECT_SOURCE_DIR}/fixtures/vectors/sst65x02)
set(lynx_vec_manifests ${PROJECT_SOURCE_DIR}/fixtures/manifests)
set(lynx_vec_inventory ${PROJECT_SOURCE_DIR}/tests/inventory/vectors)
foreach(lynx_vec_case IN ITEMS lynx1:synertek65c02 lynx2:rockwell65c02 none:synertek65c02)
  string(REPLACE ":" ";" lynx_vec_parts "${lynx_vec_case}")
  list(GET lynx_vec_parts 0 lynx_vec_profile)
  list(GET lynx_vec_parts 1 lynx_vec_variant)
  lynx_add_test(NAME cpu.vectors.${lynx_vec_profile}
    COMMAND lynx-test-cpu-vectors run --profile ${lynx_vec_profile}
      --sample ${lynx_vec_dir}/${lynx_vec_variant}.sample
      --manifest ${lynx_vec_manifests}/sst65x02-${lynx_vec_variant}.manifest
      --expect-plan ${lynx_vec_inventory}/${lynx_vec_profile}.txt
    LABELS cpu TIMEOUT 120)
endforeach()

# Named-fault controls for the vector gates (RESEARCH 5.6). Each case builds its
# faulty input at test time under the scratch directory, runs the real runner and
# requires the exact exit code and message; the profile controls also compare the
# exact failing opcode set (the 32 bit-operation opcodes, both directions) with
# the one the profile difference predicts. Only wrong_profile,
# wrong_profile_reverse, no_step and sample_identity read a full sample; the
# other cases cut mini-samples. complete_mode runs the full lane's whole-file
# mode on synthetic 10,000-object files, so that runner path is checked offline.
find_program(LYNX_SH sh REQUIRED)
foreach(lynx_vec_control IN ITEMS wrong_profile wrong_profile_reverse no_step corrupt_expect
    malformed inventory_drift sample_identity mutant_profile complete_mode)
  lynx_add_test(NAME cpu.vectors.${lynx_vec_control}
    COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/cpu/vector_controls.sh ${lynx_vec_control}
      ${PROJECT_SOURCE_DIR} $<TARGET_FILE:lynx-test-cpu-vectors> ${LYNX_SCRATCH_DIR}/vectors
    LABELS cpu TIMEOUT 120)
endforeach()

# Authored scenario vectors (interrupts, latency, BRK, NMI, reset, the lynx-tests
# Test 8 shape, bit operations and the page and wrap edges), each tagged with its
# source and an unverified flag. One test per profile.
add_executable(lynx-test-cpu-scenarios ${CMAKE_CURRENT_LIST_DIR}/test_cpu_scenarios.c)
target_link_libraries(lynx-test-cpu-scenarios PRIVATE lynx_core)
target_include_directories(lynx-test-cpu-scenarios PRIVATE ${PROJECT_SOURCE_DIR}/core ${PROJECT_SOURCE_DIR}/tests)
lynx_warnings(lynx-test-cpu-scenarios)
lynx_sanitize(lynx-test-cpu-scenarios)
set_target_properties(lynx-test-cpu-scenarios PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
foreach(lynx_scen_profile IN ITEMS lynx1 lynx2)
  lynx_add_test(NAME cpu.scenarios.${lynx_scen_profile}
    COMMAND lynx-test-cpu-scenarios ${lynx_scen_profile} LABELS cpu TIMEOUT 30)
endforeach()

# Controls for the scenario runner. Turning $CB into a 2-cycle instruction that
# polls (the $EA row copied over it, test-local) must fail the Test 8 replica and
# the whole 32-NOP block, and nothing else needs to; running the Lynx II
# bit-operation scenarios under lynx1 must fail every one of them (the runner exits
# 4 instead of 1 if any of them passes, so a vacuous control cannot pass here).
set(lynx_scen_cmd $<TARGET_FILE:lynx-test-cpu-scenarios> lynx2 --fault cb-poll)
string(REPLACE ";" "$<SEMICOLON>" lynx_scen_cmd "${lynx_scen_cmd}")
lynx_add_test(NAME cpu.scenarios.test8.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_scen_cmd}" -DCODE=1
    "-DREGEX=scenario irq.test8.replica: mismatch.*scenario irq.test8.block32: mismatch"
    -P ${PROJECT_SOURCE_DIR}/tests/cmake/expect_fail.cmake
  LABELS cpu TIMEOUT 30)
set(lynx_scen_cmd $<TARGET_FILE:lynx-test-cpu-scenarios> --scenario-profile lynx2 --run-profile lynx1)
string(REPLACE ";" "$<SEMICOLON>" lynx_scen_cmd "${lynx_scen_cmd}")
lynx_add_test(NAME cpu.scenarios.wrong_profile.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_scen_cmd}" -DCODE=1
    "-DREGEX=scenario bitop.rmb0: mismatch.*scenario bitop.bbs7.not_taken: mismatch"
    -P ${PROJECT_SOURCE_DIR}/tests/cmake/expect_fail.cmake
  LABELS cpu TIMEOUT 30)

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

# The shipping machine against the vector seam (RESEARCH OQ11): one vector per
# run-row opcode per profile, restored through a recomputed format-2 snapshot and
# advanced by the seam's cycle count; under none every vector of the 32 undefined
# rows must stop with UNDEFINED and leave the instance byte-identical.
foreach(case IN ITEMS none:synertek65c02 lynx1:synertek65c02 lynx2:rockwell65c02)
  string(REPLACE ":" ";" lynx_sa_parts "${case}")
  list(GET lynx_sa_parts 0 lynx_sa_profile)
  list(GET lynx_sa_parts 1 lynx_sa_variant)
  lynx_add_test(NAME api.seam_agreement.${lynx_sa_profile}
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:lynx-test-cpu-vectors>
      -DVEC_DIR=${lynx_vec_dir}
      -DMANIFESTS=${lynx_vec_manifests}
      -DPROFILE=${lynx_sa_profile}
      -DVARIANT=${lynx_sa_variant}
      -P ${CMAKE_CURRENT_LIST_DIR}/seam_agreement.cmake
    LABELS api TIMEOUT 120)
endforeach()
# CTest starts higher-cost tests first. The none run (1,024 undefined-row
# restores) is the longest single test and the other two are the next longest,
# so all three start at once instead of the longest finishing last.
set_tests_properties(api.seam_agreement.none PROPERTIES COST 100)
set_tests_properties(api.seam_agreement.lynx1 api.seam_agreement.lynx2 PROPERTIES COST 50)
