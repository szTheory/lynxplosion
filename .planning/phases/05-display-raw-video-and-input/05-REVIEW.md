---
phase: 05-display-raw-video-and-input
reviewed: 2026-11-12T13:46:30Z
depth: standard
files_reviewed: 51
files_reviewed_list:
  - CMakeLists.txt
  - README.md
  - consumer/main.c
  - core/bus.c
  - core/bus.h
  - core/input.c
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/timer.c
  - core/video.c
  - docs/bus-video-policy.md
  - docs/claims/skeleton-repeatability.claim
  - docs/input-replay.md
  - docs/public-suites.md
  - docs/snapshot-video.md
  - docs/timer-video-revisit.md
  - docs/video-faults.md
  - docs/video-input.md
  - fixtures/diagnostic/input_probe.cfg
  - fixtures/diagnostic/input_probe.s
  - fixtures/diagnostic/input_probe_tracer.s
  - fixtures/manifests/input-probe.manifest
  - fixtures/manifests/page-mode-tests.manifest
  - fixtures/suites/page-mode-wrapper.s
  - include/lynxplosion.h
  - runner/bundle.c
  - runner/bundle.h
  - scripts/build-page-mode.sh
  - scripts/check-manifests.sh
  - scripts/generate-page-mode-oracle.sh
  - scripts/validate-kv.sh
  - tests/api/test_api.c
  - tests/api/tests.cmake
  - tests/cmake/no_skip.cmake
  - tests/consumer/check_artifacts.c
  - tests/consumer/frame_driver.cmake
  - tests/consumer/tests.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/det/two_runs.cmake
  - tests/gates/manifests_controls.sh
  - tests/gates/page-mode-script-controls.sh
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/inventory/fast.txt
  - tests/inventory/skips.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/video/fault_controls.cmake
findings:
  critical: 1
  warning: 3
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-12T13:46:30Z
**Depth:** standard
**Files Reviewed:** 51
**Frozen HEAD:** `013de47ff566ae9c997542ddfba6fe15ff93aebe`
**Status:** issues_found

## Summary

Reviewed the 51 explicit product paths at the frozen phase head, including the scheduler and bus routes, video and input state, frame and snapshot APIs, replay consumer, scripts, fixtures, records, and negative controls. Three Phase 5 warnings affect mapped opcode handling, manifest hash validation, and an environment-controlled production replay path. A separate pre-existing test-gate hazard can delete a caller-selected directory before its containment check; it is labeled outside the Phase 5 changes. No tests or builds were run for this read-only review. The dispatched broad macOS/Linux exact-head CI remains pending.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: Test gate deletes the supplied WORK path before validating it (pre-existing; outside Phase 5 diff)

**File:** `tests/gates/manifests_controls.sh:22-30`
**Classification:** BLOCKER
**Issue:** The script recursively removes `$work` at line 22, then checks whether the canonical path is inside checked repository trees at lines 24-30. For example, passing `$repo/fixtures` as `WORK` deletes the fixture tree before the later guard detects that path and exits. This is a direct data-loss hazard in the test gate; the destructive line predates the Phase 5 diff.
**Fix:** Validate the resolved parent and final target before any recursive removal. Restrict `WORK` to a dedicated invocation scratch subtree (the registered caller already uses `build/check/scratch`), reject the repository root and its source directories, and create a fresh owned directory with an exclusive `mkdir` before writing into it.

## Warnings

### WR-01: Undefined-opcode check reads backing RAM for mapped video/input fetches

**File:** `core/machine.c:586-588`
**Classification:** WARNING
**Issue:** `bus_request_available` now accepts mapped Mikey video and Suzy input targets, and `bus_access` fetches their actual values through `lynx__video_peek` or `lynx__input_peek`. The opcode-boundary check still calls `bus_peek`, which always reads `m->ram[addr]`. If the CPU fetches an opcode from one of these supported device addresses, the undefined-opcode decision can be made from a different byte than the CPU will receive. Backing RAM contents can therefore make the core stop with `LYNX_STOP_UNDEFINED` for a device-provided defined opcode, or allow a device-provided undefined opcode through the precheck.

**Fix:** Use a side-effect-free, route-aware opcode peek that returns the same mapped byte as the granted read, or reject device opcode fetches consistently before the CPU boundary check. Add a test where the backing RAM byte and mapped device value differ.

### WR-02: Two source-built guest identity fields bypass SHA-256 validation

**File:** `scripts/validate-kv.sh:127-138,178-180`
**Classification:** WARNING
**Issue:** `is_hash_key` recognizes `sha256`, `input_digest`, and keys ending in `_sha256`. The optional provenance names `source_built_guest_sha256_lefthand_0` and `source_built_guest_sha256_lefthand_1` do not end in `_sha256`, so truncated or non-hex values for both source-built guest identities are accepted. The new negative control only corrupts `config_sha256`, leaving this gap untested.

**Fix:** Explicitly include both left-hand guest identity keys in the hash predicate (or rename them to end in `_sha256`) and add reject controls for malformed values in each field.

### WR-03: Test-only environment variable selects legacy replay in the production consumer

**File:** `consumer/main.c:2257-2260`; `tests/consumer/frame_driver.cmake:633-638`
**Classification:** WARNING
**Issue:** The shipped `lynx-consume` target honors `LYNX_TEST_REPLAY_LEGACY=1` and silently dispatches frame mode to `run_frame_consumer_legacy`. The name and only use identify this as a test switch, but the branch is outside `LYNX_CONSUMER_TESTING`; the frame-tracer test currently sets it on the production consumer. An inherited or deliberately set environment variable can therefore make production replay use the older path instead of the normal replay implementation.

**Fix:** Compile the legacy function and environment dispatch only under `LYNX_CONSUMER_TESTING`, and have the frame-tracer baseline invoke `TEST_CONSUMER` for that case.

---

_Reviewed: 2026-11-12T13:46:30Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
