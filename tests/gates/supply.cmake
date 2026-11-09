# Supply-chain gates: the pins lint, the no-network scan and their negative
# controls. Pinned sources are fetched only by the pins step of scripts/check.sh;
# no test here (or anywhere) reaches the network.
set(LYNX_TEST_SCRIPTS ${PROJECT_SOURCE_DIR}/tests/cmake)

# Every pins.txt row is well formed (https, 64-digit digest, unique name). The
# self-test runs the lint over built faulty pins files and requires each finding.
lynx_add_test(NAME policy.pins
  COMMAND ${CMAKE_COMMAND} -DPINS=${PROJECT_SOURCE_DIR}/pins.txt
    -P ${LYNX_TEST_SCRIPTS}/pins.cmake
  LABELS policy TIMEOUT 30)

lynx_add_test(NAME policy.pins.self_test
  COMMAND ${CMAKE_COMMAND}
    -DSCRIPTS=${LYNX_TEST_SCRIPTS}
    -DWORK=${LYNX_SCRATCH_DIR}/gates/pins
    -P ${LYNX_TEST_SCRIPTS}/pins_controls.cmake
  LABELS policy TIMEOUT 30)

# No test or build definition names a network tool or a download command. The
# self-test scans a fixture whose two files do; the fixture text is assembled
# from parts here, so no committed file contains a network token and the scan
# of this repository stays clean.
lynx_add_test(NAME policy.no_network
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
    -P ${LYNX_TEST_SCRIPTS}/no_network.cmake
  LABELS policy TIMEOUT 30)

set(lynx_no_network_fixture ${LYNX_SCRATCH_DIR}/gates/no_network_fixture)
file(REMOVE_RECURSE ${lynx_no_network_fixture})
set(lynx_net_tool "cu" "rl")
string(CONCAT lynx_net_tool ${lynx_net_tool})
set(lynx_net_cmd "FI" "LE(DOWN" "LOAD")
string(CONCAT lynx_net_cmd ${lynx_net_cmd})
file(WRITE ${lynx_no_network_fixture}/tests/x.sh
  "${lynx_net_tool} -fsSL -o out https://example.invalid/a\n")
file(WRITE ${lynx_no_network_fixture}/tests/y.cmake
  "${lynx_net_cmd} https://example.invalid/a out)\n")
set(lynx_no_network_cmd
  ${CMAKE_COMMAND} -DROOT=${lynx_no_network_fixture} -P ${LYNX_TEST_SCRIPTS}/no_network.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_no_network_cmd "${lynx_no_network_cmd}")
string(TOLOWER "${lynx_net_tool}" lynx_net_tool_lc)
lynx_add_test(NAME policy.no_network.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_no_network_cmd}" -DCODE=1
    "-DREGEX=tests/x[.]sh:1: ${lynx_net_tool_lc}.*tests/y[.]cmake:1: file[(]down"
    -P ${LYNX_TEST_SCRIPTS}/expect_fail.cmake
  LABELS policy TIMEOUT 30)

# fetch-pin.sh verifies before it unpacks, lists the verified archive and reuses
# a cache only with its list. The control is offline: file:// payloads in a
# temporary directory outside the repository and the build tree.
find_program(LYNX_SH sh REQUIRED)
lynx_add_test(NAME fetch_pin.mismatch.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/gates/fetch_pin_control.sh ${PROJECT_SOURCE_DIR}
  LABELS fetch_pin TIMEOUT 30)

# Every fixture manifest is a complete, valid key = value file, no two share an
# identity, and every file under fixtures/ is named by a manifest. The
# self-test builds isolated trees and requires each fault to be reported.
lynx_add_test(NAME hygiene.manifests
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/scripts/check-manifests.sh
    --root ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 30)

lynx_add_test(NAME hygiene.manifests.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/gates/manifests_controls.sh
    ${PROJECT_SOURCE_DIR} ${LYNX_SCRATCH_DIR}/gates/manifests
  LABELS hygiene TIMEOUT 30)
