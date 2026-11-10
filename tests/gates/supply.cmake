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

# The vector pin record is complete and trustworthy: an https source, a full
# commit, exactly 512 well-formed rows in byte order, both variants and all 256
# opcodes each. The self-test copies the real record, applies one fault per
# case at run time (http URL, 41-digit commit, 63-digit digest, 511 rows, a
# repeated row, an unsorted row, a path outside the two variants) and requires
# exit 1 with the finding and its line; the unchanged copy must pass.
lynx_add_test(NAME policy.vectors_pin
  COMMAND ${CMAKE_COMMAND}
    -DFILE=${PROJECT_SOURCE_DIR}/fixtures/vectors/sst65x02/upstream.sha256
    -P ${LYNX_TEST_SCRIPTS}/vectors_pin.cmake
  LABELS policy TIMEOUT 30)

lynx_add_test(NAME policy.vectors_pin.self_test
  COMMAND ${CMAKE_COMMAND}
    -DSCRIPTS=${LYNX_TEST_SCRIPTS}
    -DRECORD=${PROJECT_SOURCE_DIR}/fixtures/vectors/sst65x02/upstream.sha256
    -DWORK=${LYNX_SCRATCH_DIR}/gates/vectors_pin
    -P ${LYNX_TEST_SCRIPTS}/vectors_pin_controls.cmake
  LABELS policy TIMEOUT 60)

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

# A step of scripts/check.sh that fails before its last command, or a tool that
# fails in the identities step, stops the run (set -e is off in a tested
# function). The self-test runs the step machinery as written.
lynx_add_test(NAME hygiene.check_steps.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/gates/check_controls.sh
    ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 30)

# The probe .lnx built by the pinned cc65 (the probe step of scripts/check.sh)
# has the SHA-256 its manifest records. The self-test points the same script at
# a manifest copy whose identity is 64 zeros and requires the mismatch message.
lynx_add_test(NAME toolchain.probe
  COMMAND ${CMAKE_COMMAND}
    -DLNX=${PROJECT_SOURCE_DIR}/build/toolchain/probe.lnx
    -DMANIFEST=${PROJECT_SOURCE_DIR}/fixtures/manifests/cc65-probe.manifest
    -P ${LYNX_TEST_SCRIPTS}/toolchain_probe.cmake
  LABELS toolchain TIMEOUT 30)

set(lynx_probe_wrong ${LYNX_SCRATCH_DIR}/gates/toolchain_probe)
file(REMOVE_RECURSE ${lynx_probe_wrong})
file(READ ${PROJECT_SOURCE_DIR}/fixtures/manifests/cc65-probe.manifest lynx_probe_text)
string(REGEX REPLACE "(^|\n)sha256 = [0-9a-f]+" "\\1sha256 = 0000000000000000000000000000000000000000000000000000000000000000"
  lynx_probe_text "${lynx_probe_text}")
file(WRITE ${lynx_probe_wrong}/cc65-probe.manifest "${lynx_probe_text}")
set(lynx_probe_cmd
  ${CMAKE_COMMAND}
    -DLNX=${PROJECT_SOURCE_DIR}/build/toolchain/probe.lnx
    -DMANIFEST=${lynx_probe_wrong}/cc65-probe.manifest
    -P ${LYNX_TEST_SCRIPTS}/toolchain_probe.cmake)
string(REPLACE ";" "$<SEMICOLON>" lynx_probe_cmd "${lynx_probe_cmd}")
lynx_add_test(NAME toolchain.probe.self_test
  COMMAND ${CMAKE_COMMAND} "-DCMD=${lynx_probe_cmd}" -DCODE=1
    "-DREGEX=differs from"
    -P ${LYNX_TEST_SCRIPTS}/expect_fail.cmake
  LABELS toolchain TIMEOUT 30)
