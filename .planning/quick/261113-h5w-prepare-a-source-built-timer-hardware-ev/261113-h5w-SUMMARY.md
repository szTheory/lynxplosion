---
phase: quick-261113-h5w
plan: "01"
subsystem: testing
one_liner: Source-built timer cases and private software captures with a pending physical approval boundary.
tags: [timers, evidence, bs93, software-only]
requires: []
provides:
  - Source-built timer evidence preparation
affects: [EVD-02]
tech-stack:
  added: []
  patterns: [pinned-source-diagnostic, explicit-physical-approval-boundary]
key-files:
  created:
    - fixtures/timer-evidence/images.sha256
    - tests/timer-evidence/producer-controls.cmake
    - docs/timer-hardware-evidence.md
  modified:
    - scripts/build-timer-evidence.sh
    - tests/timer-evidence/test_timer_evidence.c
key-decisions:
  - Physical execution stays unapproved; unknown equipment does not block software preparation.
requirements-completed: []
duration: unknown
completed: 2026-11-13
---

# Timer evidence software preparation

**Source-built timer cases and private software captures are prepared and locally
qualified; physical execution remains unapproved.**

The tracer source task was committed separately. The matrix and controls now
preserve complete pinned CODE, bind all eight images to independently checked
identities, capture software screens, refuse unsupported hardware modes and
reject forged identities, missing execution/completion, duplicate physical
rows and unsafe output paths. A private package exists with actual software
captures and twelve empty physical slots. No physical run was performed.

The parent completed adjacent matrix implementation after an executor handoff;
the first runnable path exposed real palette and framebuffer-clear bugs, fixed
with independent pixel checks. A producer-control self-check also caught an
ineffective zero-to-zero mutation before qualification; it now targets a known
nonzero glyph byte. Explicit startup differs from the historical CRT, so its
software counts do not replace the strict historical comparison.

## Task commits and verification

| Task | Source commit | Evidence |
| --- | --- | --- |
| First runnable tracer | aac4a88 | Independently checked pinned CODE, fresh software execution and raster observation; subsequent matrix fixes supersede its image identities. |
| Matrix and controls | f212825 | Eight approved images; full family/config/profile and pixel checks; refreshed-receipt tampering, no-run/no-completion, strict sheet and filesystem controls. All focused tests passed after the privacy repair. |
| Protocol and private packet | 0e5d5da plus this closeout | Actual software captures, source/tool/map receipts, native/GNU byte comparison and twelve empty physical slots. No physical run or result. |

Native `sh scripts/check.sh` exited 0 at source 0e5d5da. It accounted for all
249 tests exactly once: 247 passed and the two previously declared unsupported
SERCTL/page-mode observations remained skipped. CTest took 125.18 seconds;
workflow 126 seconds, artifacts 5 seconds, full history 87 seconds and total
220 seconds. No gate or allowlist was weakened. Detailed raw output remains
private under `canonical-native.log`.

The reused GNU 13.3/Linux aarch64 checkout first exposed a runtime-cost limit:
Debug matrix and controls timed out. A separately timed already-built case took
23.915 seconds wall / 23.900 seconds user / 0.009 seconds system for unchanged
correct output. This was CPU work, not a failed image build or successful
qualification. Failure artifacts remain retained.

The same checkout's RelWithDebInfo build retained AddressSanitizer and
UndefinedBehaviorSanitizer and passed all three groups within their original
bounds (5.93 / 21.64 / 16.54 seconds, total 44.12). All eight native/GNU images
were compared byte for byte, including hardware cases whose execution must
refuse in the current core. Actual maps and tool receipts are retained for
each build; raw path-dependent map hashes are not claimed equal. The local
GNU Debug timeout remains recorded rather than erased by this result.

Independent [code review](261113-h5w-REVIEW.md) found one privacy blocker and
confirmed its repair: owner-only package/invocation directories, restrictive
umask and rejection of widened permissions. The planned level-1
[security check](261113-h5w-SECURITY.md) closes all seven software threats.
No dependency or package install was introduced.

## CI and remaining boundary

**The same-HEAD hosted Debug workflow must succeed before the final owner
handoff.** Its live GitHub run is the authoritative post-commit receipt; this
summary is committed before that run and does not predict its result. Retain
the raw job log and critical-path review privately, and provide the successful
run link in chat. Do not trigger another run merely to commit its receipt.

The first pushed batch (d3dfbb2, run 43499267151) failed at ShellCheck 0.9's
SC2015 guard diagnostics, which native ShellCheck 0.11 did not report. This was
a missed local portability check. Guards were rewritten as explicit conditionals
without weakening their checks. Both installed ShellCheck versions then passed,
and all three affected native groups passed again (25.64 seconds). The original
full native gate remains anchored to 0e5d5da; the lint repair has these fresh
focused checks plus the subsequent same-HEAD hosted canonical gate. Source/tool
pins, guest bytes, identities and comparison expectations did not change.

The failed CI run stopped before tests: cc65 setup was 13 seconds, ShellCheck
about 5 seconds. It did not repeat a completed test lane. An additional push is
required for the actual source repair, not for bookkeeping. No cache, matrix,
concurrency or global build-policy change is justified by that failure.

EVD-02 is intentionally absent from `requirements-completed`: this task
prepares evidence without satisfying the historical strict comparison. Counts
12/15 versus source 13 remain unresolved. New explicit-startup companion counts
13/14 do not substitute for the historical CRT. Hardware model, loader and BS93
compatibility remain unknown. No milestone close, new phase, archive, release,
tag, public binary distribution or physical action occurred.

## Lessons applied

The final source batch reused existing pins, fixture builders and the running
Linux environment. Parent implementation followed the executor's terminal
handoff; no shared-file edit race was introduced. Native canonical owns the
full history scan; task handoffs scan only new commits. One new package review
and a bounded repair recheck supplied independent scrutiny without reopening
completed phases. Failed scratch was moved to private retention only after
matching complete byte/type/mode inventories and nonce owner markers.

Hardware preparation is now concrete. The owner acts next, after hosted CI,
to identify compatible equipment and decide on the bounded protocol in
[APPROVAL-PACKET.md](APPROVAL-PACKET.md). Keep physical work stopped if equipment
is unavailable or the protocol is not explicitly approved.
