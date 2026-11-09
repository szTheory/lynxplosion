# Documentation gates: the draft clean-room policy and exposure record, the
# provenance list of adapted code, and the claim template. Each
# self-test builds its fixture at configure time under ${LYNX_SCRATCH_DIR}, from
# the real documents with one edit per fault, and aborts configure if an edit
# changed nothing (an edit that silently misses would make the control vacuous).
set(LYNX_TEST_SCRIPTS ${PROJECT_SOURCE_DIR}/tests/cmake)
find_program(LYNX_SH sh REQUIRED)

# docs.policy: the draft banner, the named emulators and one exposure row per
# research source are present. The self-test removes the banner from the policy,
# the Holani row from the exposure record and the commit from the provenance
# notice, and requires all three findings, in the order the check reports them.
lynx_add_test(NAME docs.policy
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
    -P ${LYNX_TEST_SCRIPTS}/docs_policy.cmake
  LABELS docs TIMEOUT 30)

set(lynx_policy_fixture ${LYNX_SCRATCH_DIR}/gates/docs_policy)
file(REMOVE_RECURSE ${lynx_policy_fixture})

file(READ ${PROJECT_SOURCE_DIR}/docs/policy/clean-room.md lynx_doc_before)
string(REPLACE "DRAFT pending owner decision O6" "" lynx_doc_after "${lynx_doc_before}")
if(lynx_doc_after STREQUAL lynx_doc_before)
  message(FATAL_ERROR "docs.policy.self_test: removing the banner changed nothing")
endif()
file(WRITE ${lynx_policy_fixture}/docs/policy/clean-room.md "${lynx_doc_after}")

file(READ ${PROJECT_SOURCE_DIR}/docs/policy/exposure-record.md lynx_doc_before)
string(REGEX REPLACE "(^|\n)\\|[ ]*Holani[^\n]*" "\\1" lynx_doc_after "${lynx_doc_before}")
if(lynx_doc_after STREQUAL lynx_doc_before)
  message(FATAL_ERROR "docs.policy.self_test: removing the Holani row changed nothing")
endif()
file(WRITE ${lynx_policy_fixture}/docs/policy/exposure-record.md "${lynx_doc_after}")

file(READ ${PROJECT_SOURCE_DIR}/docs/provenance.md lynx_doc_before)
string(REPLACE "7fd0386" "" lynx_doc_after "${lynx_doc_before}")
if(lynx_doc_after STREQUAL lynx_doc_before)
  message(FATAL_ERROR "docs.policy.self_test: removing the commit changed nothing")
endif()
file(WRITE ${lynx_policy_fixture}/docs/provenance.md "${lynx_doc_after}")

set(lynx_policy_cmd
  ${CMAKE_COMMAND} -DROOT=${lynx_policy_fixture} -P ${LYNX_TEST_SCRIPTS}/docs_policy.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_policy_cmd "${lynx_policy_cmd}")
lynx_add_test(NAME docs.policy.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_policy_cmd}" -DCODE=1
    "-DREGEX=clean-room[.]md: missing banner.*exposure-record[.]md: no row for Holani.*provenance[.]md: missing 7fd0386"
    -P ${LYNX_TEST_SCRIPTS}/expect_fail.cmake
  LABELS docs TIMEOUT 30)

# docs.provenance: every code file whose header says it was adapted from the owner
# project is listed in docs/provenance.md. The self-test checks the real tree
# against a copy of the list without the line that names tests/cmake/expect_fail.cmake.
lynx_add_test(NAME docs.provenance
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
    -DPROVENANCE=${PROJECT_SOURCE_DIR}/docs/provenance.md
    -P ${LYNX_TEST_SCRIPTS}/provenance.cmake
  LABELS docs TIMEOUT 30)

set(lynx_prov_fixture ${LYNX_SCRATCH_DIR}/gates/provenance)
file(REMOVE_RECURSE ${lynx_prov_fixture})
file(READ ${PROJECT_SOURCE_DIR}/docs/provenance.md lynx_doc_before)
string(REGEX REPLACE "\n[^\n]*tests/cmake/expect_fail[.]cmake[^\n]*" ""
  lynx_doc_after "${lynx_doc_before}")
if(lynx_doc_after STREQUAL lynx_doc_before)
  message(FATAL_ERROR "docs.provenance.self_test: dropping the expect_fail line changed nothing")
endif()
file(WRITE ${lynx_prov_fixture}/provenance.md "${lynx_doc_after}")

set(lynx_prov_cmd
  ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
    -DPROVENANCE=${lynx_prov_fixture}/provenance.md
    -P ${LYNX_TEST_SCRIPTS}/provenance.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_prov_cmd "${lynx_prov_cmd}")
lynx_add_test(NAME docs.provenance.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_prov_cmd}" -DCODE=1
    "-DREGEX=expect_fail[.]cmake: adapted from muhtari but not listed"
    -P ${LYNX_TEST_SCRIPTS}/expect_fail.cmake
  LABELS docs TIMEOUT 30)

# docs.claim_template: the claim template has exactly the claim keys, and every
# claim under docs/claims is valid, uses profile "none" while O1 is open and holds
# the SHA-256 of the live tests/inventory/fast.txt. The self-test builds isolated
# trees and requires each fault (no profile, stale inventory hash, a Lynx profile,
# a repeated key, an empty value, a reordered inventory, a template without a key)
# to be reported, and a claims directory holding only the template to pass.
lynx_add_test(NAME docs.claim_template
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/scripts/check-claims.sh
    --root ${PROJECT_SOURCE_DIR}
  LABELS docs TIMEOUT 30)

lynx_add_test(NAME docs.claim_template.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/gates/claims_controls.sh
    ${PROJECT_SOURCE_DIR} ${LYNX_SCRATCH_DIR}/gates/claims
  LABELS docs TIMEOUT 30)
