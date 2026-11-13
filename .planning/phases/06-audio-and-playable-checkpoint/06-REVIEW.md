---
phase: 06-audio-and-playable-checkpoint
reviewed: 2026-11-13T14:13:31Z
depth: standard
source_head: 551459579e67eae92c9046157b4d226e6fe3a570
diff_base: f24e281527f6c36fb31d1c3d830aafaa5bfa6ec1
iteration: 3
working_diff_sha256: d410efadda9cfebce6d1cf54a640ba32c3f31b275592a402a4e7635fca302681
working_diff_files:
  - tests/api/test_api.c
  - tests/det/test_det.c
files_reviewed: 53
files_reviewed_list:
  - CMakeLists.txt
  - consumer/main.c
  - core/audio.c
  - core/internal.h
  - core/machine.c
  - core/snapshot.c
  - core/timer.c
  - docs/audio-faults.md
  - docs/audio.md
  - docs/av-replay.md
  - docs/claims/skeleton-repeatability.claim
  - docs/performance.md
  - docs/provenance.md
  - docs/public-suites.md
  - docs/snapshot-audio.md
  - docs/snapshot-video.md
  - docs/timer-source-index14-pending.json
  - fixtures/diagnostic/av_probe.cfg
  - fixtures/diagnostic/av_probe.s
  - fixtures/manifests/audio-tests.manifest
  - fixtures/manifests/audio2-tests.manifest
  - fixtures/manifests/av-probe.manifest
  - fixtures/suites/NOTICE
  - fixtures/suites/audio-wrapper.s
  - fixtures/suites/audio2-wrapper.s
  - include/lynxplosion.h
  - scripts/build-suites.sh
  - scripts/check-manifests.sh
  - scripts/generate-suite-oracle.sh
  - tests/api/test_api.c
  - tests/audio/fault_controls.cmake
  - tests/audio/test_audio.c
  - tests/audio/tests.cmake
  - tests/consumer/av_driver.cmake
  - tests/consumer/check_artifacts.c
  - tests/consumer/tests.cmake
  - tests/cpu/test_cpu_vectors.c
  - tests/det/av_driver.cmake
  - tests/det/test_det.c
  - tests/det/tests.cmake
  - tests/diag/test_diag.c
  - tests/diag/tests.cmake
  - tests/gates/audio-script-controls.sh
  - tests/gates/manifests_controls.sh
  - tests/golden/skeleton.digest
  - tests/header/header.c
  - tests/inventory/fast.txt
  - tests/suites/run.cmake
  - tests/suites/test_public_suites.c
  - tests/suites/tests.cmake
  - tests/tests.cmake
  - tests/timers/fault_controls.cmake
  - tests/video/fault_controls.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 06: Code Review Report

**Reviewed:** 2026-11-13T14:13:31Z
**Depth:** standard
**Files Reviewed:** 53
**Status:** clean

## Summary

Iteration 3 reuses the prior review of the frozen 53-file Phase 6 scope and independently reviews the two-path dirty diff identified above. It corrects tests that still asserted transient timer status after advancing one tick beyond the event. The source contract says ring borrow bits expire on guest-tick advance, while DONE and divider LAST_CLOCK remain; the same-tick CPU read still observes the pulse. The zero initial audio output expectation also matches reset state and the retained-output policy for setup writes. No assertion was weakened or production code changed. I found no unresolved code or test-reliability defects. The parent reports the focused native boundary batch passed; the post-repair canonical gate, GNU gate and CI remain pending, so this review does not qualify the phase.

## Narrative Findings (AI reviewer)

No unresolved findings in iteration 3.

## Resolved Finding Ledger

- **CR-01 — former blocker, resolved.** `core/machine.c:820-821` marks the audio device cursor only after the bus operation succeeds. The added controls in `tests/audio/test_audio.c:908-1130` cover the tick-400 CPU enable edge (first natural clock at tick 416), capacity retry, watch stop, explicit CTLB strobe, and snapshot continuation. This preserves the write’s explicit strobe while deferring only a newly eligible natural root.
- **CR-02 — former blocker, resolved.** `scripts/check-manifests.sh:71-103,192-226` detects fixture-tree symlinks before validation and rejects symlink path components. `tests/gates/manifests_controls.sh:338-403` covers listed and unlisted links, directory components, dangling links, and symlinked fixture/manifests roots and files.
- **WR-01 — former warning, coverage obligation addressed; not an implementation defect.** `tests/audio/test_audio.c:2511-2724` now creates active nondefault audio state with samples, events, and ring debt, then checks reset/reload equivalence to fresh state, deinitialized A/V refusal postconditions, same-allocation reinitialization, and caller-span ownership.

The focused test and shell-check results above come from `.planning/phases/06-audio-and-playable-checkpoint/06-REVIEW-FIX.md`; they were not independently rerun in this review. Full canonical qualification and CI are still pending.

## Iteration 3 Re-review

The failed pre-repair native canonical blocks matched stale assertions in `api.scheduler_order`, `api.timer_links`, `api.scheduler_watch`, `api.timer_link_tracer`, `api.snapshot_roundtrip`, and `det.timer_paths`; `timer.fault.order` failed because its clean positive API baseline inherited the stale scheduler assertion. The edited assertions now match the documented sampling boundary: the tests advance through tick 80/1024 and inspect at tick 81/1025, after ring borrow pulses expire. `api.timer_link_tracer` still asserts that the CPU read at tick 1024 returned `0x07` before the call returns at 1025, then checks the expired T3/T5 status and latched IRQ `0x28`. The scheduler and watch cases retain DONE/IRQ and divider LAST_CLOCK checks; the even T0/T2 chain values are unchanged. The link-path table retains count and pending comparisons, and `timer.fault.order` retains its injected ordering mutant as a positive-control requirement. The audio lattice assertion expects the reset OUT value `0` before the first output transition to `1`, consistent with `lynx__audio_init` and `docs/audio.md`.

The reviewed dirty diff is only `tests/api/test_api.c` and `tests/det/test_det.c` (SHA-256 recorded in frontmatter). The native 11-case focused result is from the supplied run log and was not rerun here. The failed native canonical gate predates this test-only repair; a post-repair canonical run is still required. GNU and CI results are also pending.

---

_Reviewed: 2026-11-13T14:13:31Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
