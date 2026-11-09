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
