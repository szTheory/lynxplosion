# Rights and privacy gates: the binary and firmware scan over the source tree,
# and the PII and credential scan. The artifact-zone scan of the build tree is
# the `artifacts` step of scripts/check.sh, which runs after CTest finishes and
# never while tests may be writing files. Each self-test builds an isolated
# tree outside the repository and the build tree and seeds the faults.
find_program(LYNX_SH sh REQUIRED)

# Every tracked or untracked-not-ignored file is text, or is a fixture whose
# SHA-256 is named by a manifest; no file is a firmware-like 512-byte binary.
lynx_add_test(NAME hygiene.firmware_scan
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/scripts/scan-binaries.sh
    --source --root ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 60)

lynx_add_test(NAME hygiene.firmware_scan.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/hygiene/scan_control.sh
    ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 60)

# No tracked or untracked-not-ignored text file holds a home-directory path, a
# non-noreply email address or a credential-shaped token. The self-test builds
# its fake values from fragments in a temporary repository, so no fake secret is
# ever committed (push protection is enabled on the repository).
lynx_add_test(NAME hygiene.tree
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/scripts/hygiene.sh --tree
  LABELS hygiene TIMEOUT 60)

lynx_add_test(NAME hygiene.pii.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/hygiene/pii_control.sh
    ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 60)

# The one control that carries the real multi-megabyte vector samples: it copies
# them into a temporary repository, seeds leaks and a long base64 run at the end
# of the large files and asserts both real scans (hygiene.sh and scan-binaries.sh)
# still read them to the last byte. The other self-tests use small synthetic
# trees on purpose.
lynx_add_test(NAME hygiene.large_text.self_test
  COMMAND ${LYNX_SH} ${PROJECT_SOURCE_DIR}/tests/hygiene/large_text_control.sh
    ${PROJECT_SOURCE_DIR}
  LABELS hygiene TIMEOUT 60)
