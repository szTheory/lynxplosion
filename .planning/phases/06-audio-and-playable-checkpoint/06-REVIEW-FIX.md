---
phase: 06
fixed_at: 2026-11-13T13:44:01Z
review_path: .planning/phases/06-audio-and-playable-checkpoint/06-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 6: Code Review Fix Report

**Fixed at:** 2026-11-13T13:44:01Z  
**Source review:** `.planning/phases/06-audio-and-playable-checkpoint/06-REVIEW.md`  
**Iteration:** 1

**Summary:**

- Findings in scope: 3
- Fixed: 3
- Skipped: 0

## Fixed Issues

### CR-01: A CPU audio write can enable a natural root after the same-tick grant

**Files modified:** `core/machine.c`, `tests/audio/test_audio.c`  
**Commit:** `d40db8dfcd869f5aae1e64d2e82f5496f8d92e2e`  
**Status:** fixed: pending independent automated re-review

**Applied fix:** After `bus_access` succeeds, the scheduler marks the audio device as applied at that grant tick. Capacity refusal returns before this watermark changes. The CTLB explicit strobe remains part of the successful write operation. The guest-driven ordering test starts A0 disabled, writes CTLA `0x08` at tick 400, asserts there is no natural CLOCK at tick 400, and observes its first root at tick 416. The test also covers capacity refusal/retry, watch stop after the committed write, snapshot/restore continuation, and the explicit forced strobe.

**Evidence:** The pre-fix boundary assertion failed with `first-clock-416 failed: got 400, expected 416` in `/private/tmp/lynx-06-rf.8XSbLxhz/`. The focused `audio.order` case passed after the change. The affected native and GNU batches also passed; see Verification.

### CR-02: Fixture manifest coverage accepts symlinks to out-of-tree content

**Files modified:** `scripts/check-manifests.sh`, `tests/gates/manifests_controls.sh`  
**Commit:** `0daef498240dc51068dec7ce07bdc72e981dd59f`  
**Status:** fixed: pending independent automated re-review

**Applied fix:** The gate rejects symlinks in the fixtures tree before opening manifest validators, including listed, unlisted, file, directory, and dangling links. It also rejects symlinked fixtures roots, manifests directories, and manifest files. Each manifest entry is checked for symlink components before regular-file validation, then its canonical parent is checked against the canonical fixture root. The controls now seed listed outside-file and symlink-directory entries, unlisted and dangling links, and links at the roots/manifest level. The outside sentinel is checked to remain unchanged, and foreign bytes are not emitted by the gate.

**Evidence:** The seeded listed outside-file link was accepted by the old gate as `1 manifest, 1 fixture file covered`, so the old-gate control failed as intended. The repaired manifest controls passed. Both scripts passed native ShellCheck 0.11.0, GNU ShellCheck 0.9.0 in POSIX-shell mode, and `sh -n` on both toolchains.

### WR-01: Active A/V lifecycle boundaries lack behavioral evidence

**File modified:** `tests/audio/test_audio.c`  
**Commit:** `e7bce5e8a830d9b13b10baa6af7dac54c52c7004`  
**Status:** fixed

**Applied fix:** Extended `audio.stops` with accepted reset and reload checks that begin from publicly advanced, nondefault A/V state: enabled channels, a nonzero sample, CLOCK events, sample ordinal 1, and deferred ring debt. Reset matches a fresh machine byte-for-byte and restores the documented empty-content and sample/debt state. Accepted reload matches a fresh machine loaded with the same content and checks its identity fields and cleared audio/sample state. The deinitialized A/V refusal checks the complete machine allocation, result, buffer descriptor, and caller storage remain unchanged. Reinitializing and reloading at the same allocation matches the fresh loaded machine; advancing with new spans leaves the old caller spans untouched. The first minimum-capacity A/V call is also checked to return OUTPUT after partial progress and events, then the same machine is advanced to tick 401.

**Evidence:** The focused `audio.stops` case passed. The review described this as an automated coverage gap, not a demonstrated implementation failure; no production lifecycle policy was added.

## Verification

The focused native batch ran in the main checkout at `~/projects/lynxplosion`, with output and actual exit status preserved in `/private/tmp/lynx-06-native-batch.m4DYto/`. It passed 14/14: `audio.order`, `audio.capacity`, `audio.stops`, `audio.snapshot`, `audio.snapshot_refusal`, `det.audio_paths`, `det.audio_sensitivity`, the four Lynx I/II audio/audio2 observations, `suite.timers.pending_controls`, `hygiene.manifests`, and `hygiene.manifests.self_test`.

The focused GNU/Linux batch ran in the existing owned clone `/work/lynxplosion` inside `lynx-phase5-task12-linux-reuse`, fast-forwarded to exact HEAD `e7bce5e8a830d9b13b10baa6af7dac54c52c7004` with clean tracked files before configure/build. It used GNU 13.3.0, CMake 3.28.3, Ninja, Linux aarch64, and linked AddressSanitizer and UndefinedBehaviorSanitizer runtimes. Configure, focused build, ShellCheck 0.9.0, `sh -n`, and the same 14 CTests all exited 0. Durable logs and status files are in the container at `/tmp/lynx-06-gnu.95jEGR/`; CTest completed in 122.32 seconds.

The three source changes are separate exact-path commits. This report is intentionally uncommitted for the parent metadata batch. The final independent review, security/prohibition judgment, canonical gates, exact-HEAD CI, and overall phase qualification remain pending; these results do not claim those gates passed.

---

_Fixed: 2026-11-13T13:44:01Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_
