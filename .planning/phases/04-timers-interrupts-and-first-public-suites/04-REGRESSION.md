# Phase 4 regression gate

**Result:** passed, source `baabaee1dc020df206f7f6aedaabc01f74cce050`.

Prior Phase1–3 verification records identify63 unique existing test/check files,
recorded in the canonical `.git/gsd-handoff/regression-files` handoff. The
configured `workflow.test_command` is `sh scripts/check.sh`. Root ran it through
GSD `run-with-timeout600` as the final canonical macOS qualification command:
exit0, all163 tests, exact JUnit once each/no skips, artifacts and history pass;
131s total, below the600s bound. The accepted source then passed the same gate
on self-contained Ubuntuarm64 (94s) and hosted exact-head CI (90s).

This one final local run satisfies regression and qualification under Plan12's
explicit reuse policy; it is not repeated solely for a bookkeeping label. The
nine-file post-fix review and all11 finding dispositions are recorded, with
WR-02 demonstrated before/after. No timeout or regression is waived. The
full-data CPU vector lane is unchanged and was not repeated for a harness-only
pin-parser fix. Global source/hardware qualification limits remain unchanged.

## Prior-phase file set

- `scripts/check-claims.sh`
- `scripts/check-manifests.sh`
- `scripts/check.sh`
- `scripts/fetch-pin.sh`
- `scripts/hygiene.sh`
- `scripts/scan-binaries.sh`
- `scripts/validate-kv.sh`
- `scripts/vectors-full.sh`
- `tests/abi/global_fixture.c`
- `tests/abi/libc_fixture.c`
- `tests/abi/tests.cmake`
- `tests/api/build_id_refresh.sh`
- `tests/api/test_api.c`
- `tests/api/tests.cmake`
- `tests/check.h`
- `tests/cmake/abi.cmake`
- `tests/cmake/check_junit.cmake`
- `tests/cmake/core_includes.cmake`
- `tests/cmake/docs_policy.cmake`
- `tests/cmake/expect_fail.cmake`
- `tests/cmake/inventory_controls.cmake`
- `tests/cmake/inventory_equal.cmake`
- `tests/cmake/no_float.cmake`
- `tests/cmake/no_network.cmake`
- `tests/cmake/no_skip.cmake`
- `tests/cmake/pins.cmake`
- `tests/cmake/pins_controls.cmake`
- `tests/cmake/provenance.cmake`
- `tests/cmake/toolchain_probe.cmake`
- `tests/consumer/check_artifacts.c`
- `tests/consumer/tests.cmake`
- `tests/cpu/test_cpu_scenarios.c`
- `tests/cpu/test_cpu_vectors.c`
- `tests/det/bll.cmake`
- `tests/det/golden.cmake`
- `tests/det/golden_self_test.cmake`
- `tests/det/manifest_controls.sh`
- `tests/det/manifest_fields.cmake`
- `tests/det/runner_cli.cmake`
- `tests/det/sensitivity.cmake`
- `tests/det/test_det.c`
- `tests/det/tests.cmake`
- `tests/det/two_runs.cmake`
- `tests/diag/test_diag.c`
- `tests/diag/tests.cmake`
- `tests/gates/check_controls.sh`
- `tests/gates/claims_controls.sh`
- `tests/gates/docs.cmake`
- `tests/gates/fetch_pin_control.sh`
- `tests/gates/inventory.cmake`
- `tests/gates/manifests_controls.sh`
- `tests/gates/rights.cmake`
- `tests/gates/supply.cmake`
- `tests/golden/skeleton.digest`
- `tests/header/header.c`
- `tests/hygiene/pii_control.sh`
- `tests/hygiene/scan_control.sh`
- `tests/inventory/fast.txt`
- `tests/inventory/skips.txt`
- `tests/inventory/vectors/lynx1.txt`
- `tests/inventory/vectors/lynx2.txt`
- `tests/inventory/vectors/none.txt`
- `tests/tests.cmake`
