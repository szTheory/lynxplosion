---
phase: "06"
status: qualified
qualified: "2026-11-13T15:12:02.607413+00:00"
source_head: bc943eecc7a498b4405a150961c036e6d911e60e
ci_run: 43448772929
inventory_sha256: 25102ec41d5d179a1adf178911b2cf9bc287b6cecbecbb07d2b0d925d0ac9ac2
registered_tests: 246
passed_tests: 244
declared_skips: 2
failed_tests: 0
---

# Phase 6 — Software Qualification

The reviewed audio/playable software checkpoint passes the canonical gate on
native macOS, the existing self-contained GNU/Linux clone, and hosted CI at
exact source `bc943eecc7a498b4405a150961c036e6d911e60e`. This is scoped software evidence;
strict timer source agreement and physical/model qualification remain open.
Final reports and lifecycle commits are separate metadata successors; no local
qualification of an untested successor is implied.

## Source and review identity

The source was clean before each configure/gate. GNU used actual fetch and
fast-forward in `/work/lynxplosion` inside `lynx-phase5-task12-linux-reuse`,
with readonly `/source`; no source overlay, replacement container or new package
installation was used. The inherited base image is pinned Ubuntu24.04 as
recorded in Phase5 qualification. Native host identifier is `Mac17,9`; final
benchmark metadata reports Apple M5 Pro.

Independent standard review covers54 paths. Iterations2–4 accepted the ordering,
fixture-containment and lifecycle repairs, then the legacy sampling assertions
and the replay-matrix reduction. The final committed frame-driver diff SHA-256
`0337be79b2446feeb031a27dadcaad69f3421f99dfd15801ef3e3658be5f9a96`
matches the reviewed dirty diff. See06-REVIEW.md,06-REVIEW-FIX.md and
06-REVIEW-DISPOSITION.md. Security/prohibition judgments and coverage mapping are
in06-SECURITY.md and06-VALIDATION.md.

## Canonical results

Every invocation uses `sh scripts/check.sh` and the exact sorted fast inventory.
Each accepted gate accounts for246 rows once:244 passes, two declared SERCTL
skips, zero failures. JUnit, fixture/rights, privacy, imports, artifact and full
history gates pass; no missing or unexpected skipped row is counted as success.

| Environment | Tool/build identity | CTest | Artifacts | History / total |
|---|---|---:|---|---|
| Native macOS | Darwin25.6.0 arm64; AppleClang21.0.0.21000101; CMake4.4.3; Ninja1.13.2; ShellCheck0.11.0; Debug |306.63s|6380 files; one exactly512-byte source file; no findings|201s /531s|
| Existing GNU clone | Linux aarch64; GCC13.3.0; CMake3.28.3; Ninja1.11.1; ShellCheck0.9.0; Git2.43.0; Debug with sanitizer evidence |278.65s|6336 files; one exactly512-byte source file; no findings|26s /313s|
| Hosted CI | [Run43448772929](https://github.com/szTheory/lynxplosion/actions/runs/43448772929), push/main, exact source above; Ubuntu24 x86_64, image20261111.370.1; GCC13.3.0; CMake3.31.6; Ninja1.13.2; ShellCheck0.9.0; Git2.56.1 |151.50s|6333 files; one exactly512-byte source file; no findings|43s /236s|

The required Plan12 focused group passed26/26 before final integration repairs;
all of its registrations also pass in each final canonical inventory. Focused
repair batches, actual mutants, source-suite captures and the five-run benchmark
are reused from their owning summaries and the final gates, rather than rerun
for report prose. The current inventory includes prior-phase regression tests;
the accepted canonical run is the regression-gate evidence. Historical reports
retain their original source identities rather than receiving blind hash refreshes.

## Goal and requirement evidence

| Obligation | Concrete proof |
|---|---|
| AUD-01: four channels/routes | Independent register/LFSR/integrator/counter/mix assertions, every ring edge, eight stereo routes; four fresh audio/audio2 profile observations with18 source rows each; six channel/link and two stereo mutants. |
| AUD-02: integer sample/event contract |400-tick lattice at16MHz, half-open edges, device→sample→CPU ordering, bounded deferred ring work, caller spans and exact event/PCM continuation; cadence/snapshot mutants; forbidden-import gates. |
| HB-03 and CON-01: real public consumer | Original220-byte source-built av_probe reacts to canonical input; independent13-file validator checks actual non-silent PCM, WAV, frame/palette, event, applied-input and progress streams. Ownership, normal-process publication race and refusal/fault controls pass. |
| DET-05: separate repeatable observations | Fresh/sliced/watched/interleaved/restore paths compare state, progress, input, raw video, PCM and events independently; record sensitivity and accepted-restored suffix controls. Host checkpoint is an in-memory sidecar, not a persisted file API. |
| EVD-06: exact public inventory | CPU/timers/page-mode/audio/audio2 source-built tier-A entries, model/progress/return/sentinel/row controls and exact selection. Only the two page-mode observations below skip. |
| EVD-05: actual named faults | Ten private audio mutants build and fail only their intended assertion with clean brackets; earlier timer, DMA/palette and input faults remain in the canonical inventory. |
| PERF-01: named digest-bound workload | Five fresh equal full-workload runs; host-only public-call timing separate from encoding/assembly/publication/consumer time. Historical and final-source observations remain distinct in docs/performance.md. |

## Unchanged evidence and authority limits

- `suite.page_mode.observation.lynx1` and `.lynx2` visibly skip because a guest
  writes06 to unsupported SERCTL FD8C before producing results. These are missing
  observations, not profile/source passes.
- Strict timer row14 agreement still fails: legacy flat5 observes0C against
  source0D; video-policy1 observes0F against source0D. Existing observation and
  control acceptance does not close global EVD-02. No new pending exception or
  source-oracle change was introduced.
- Same-core repeatability and source observations do not establish physical
  accuracy, O1 baseline qualification or a named-model suite pass.
- O1/O5/O6/O9, rights-bearing assets, physical tests, release/new-channel and
  shared LibFuturo boundaries remain open. No firmware or commercial content was
  used. Public-repository checked-main authority does not authorize release.

## CI critical path and duplicate-work inspection

Workflow creation15:03:49Z to job start15:03:52Z was3s. Setup1s, checkout2s,
canonical command236s. Pinned cc65 setup/build21s, CMake configure/build roughly
7–8s, CTest151.50s, JUnit1s, artifacts2s and history43s. The single job has no
matrix or duplicated broad job. Slowest tests were pending timer controls72.06s,
result controls56.16s, benchmark controls28.64s, none-profile seam28.11s,
benchmark25.59s; the repaired full replay matrix took12.97s.

The measured low-risk reductions are already applied: warm executor/container
reuse, first runnable path, focused publication transcript, batched repairs,
new-range handoff scans, and removal of sensitivity's redundant interleaved
instances while retaining every distinct full-transcript assertion. No evidence
justifies a new cache, dependency, concurrency setting or CI lane here. Any
further timer-control corpus optimization must preserve its strict comparison,
identity and refusal proofs; it is not required to qualify this checkpoint.

## Evidence retention and failed attempts

Private native final evidence is `/private/tmp/lynx-06-canonical-native-u8elrdjn/`.
GNU final evidence is `/tmp/lynx-06-canonical-gnu.NcgJH5/` in the reused container,
with a host copy under `/private/tmp/lynx-06-final-review-czwgzg_5/gnu-final/`.
That private bundle also retains raw CI job/run metadata and job logs.

The first native integration failure, GNU stale-artifact failure and GNU replay
timeout retain their actual nonzero statuses and outputs. They are not counted
as successful qualification or semantic fault detections. Only attributable old
outputs were moved after provenance/nonce checks and matching complete
before/after inventories; see06-REVIEW-FIX.md. The original timeout's exact
execution/cleanup/environment contribution is unknown. Final accepted runs do
not retrospectively turn it into a pass.
