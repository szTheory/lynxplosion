---
phase: 06-audio-and-playable-checkpoint
plan: "02"
subsystem: testing
tags: [lynx-tests, audio, ca65, ld65, source-oracle]

requires: []
provides:
  - Source-built audio and audio2 suite objects with returning project wrappers
  - Explicit extended oracle generation for the pinned audio/audio2 C sources
affects: [06-09, public-suite-evidence]

actuals:
  tokens: 11438 # 45,755 changed diff characters / 4; a diff proxy, not API usage
  tasks: 2
  commits: 4 # Measured from plan_head_before through task commits at summary creation

tech-stack:
  added: []
  patterns:
    - Portable fixture manifests bind pinned source and recipe; native tool hashes stay in host-local build records
    - Extended audio oracle is opt-in so the strict CPU/timer reader keeps its existing input contract

key-files:
  created:
    - fixtures/suites/audio-wrapper.s
    - fixtures/suites/audio2-wrapper.s
    - fixtures/manifests/audio-tests.manifest
    - fixtures/manifests/audio2-tests.manifest
    - tests/gates/audio-script-controls.sh
  modified:
    - scripts/build-suites.sh
    - scripts/generate-suite-oracle.sh
    - fixtures/suites/NOTICE
    - docs/public-suites.md
    - docs/provenance.md
    - .planning/phases/06-audio-and-playable-checkpoint/06-02-PLAN.md

key-decisions:
  - "Keep audio oracle rows behind --include-audio to preserve the existing CPU/timer schema and consumer."
  - "Verify each host's ca65/ld65 binaries against its successful local build record; keep executable hashes out of portable manifests."
  - "Keep EVD-06 open until a later plan records guest suite observations; this plan establishes source construction only."

patterns-established:
  - "Returning suite wrapper: clear all zero page, independently probe the guest model, call the pinned tests, then write progress and a completion sentinel."
  - "Build verification: compare source, wrapper, layout, tool, object and label identities against an invocation-local record."

requirements-completed: [EVD-06] # Source-construction slice only; global completion awaits Plans 09 and 12.
coverage:
  - id: D1
    description: "Pinned audio/audio2 objects and labels build without entering either upstream TGI CRT main."
    verification:
      - kind: integration
        ref: "sh scripts/build-suites.sh --out build/check/scratch/phase06-suite-build; local identity verification on macOS and Linux"
        status: pass
    human_judgment: false
  - id: D2
    description: "The opt-in audio oracle contains exact source rows, case labels and audio2 index 9 alternative."
    verification:
      - kind: other
        ref: "sh scripts/generate-suite-oracle.sh --include-audio; audio-script-controls.sh on macOS and Linux"
        status: pass
    human_judgment: false

plan_head_before: bc6c43f788185a1d91bbb22b61bdc29a81df5a70
plan_head_after: 10e7a2a8a2a1988b74ffa382e5d398d4855f191f
duration: 33min
completed: 2026-11-12
status: complete
---

# Phase 6 Plan 02: Pinned Audio Suite Source Construction Summary

**Pinned audio/audio2 guests now build from unchanged MIT assembly, and an opt-in source oracle emits all 36 expected rows without changing the CPU/timer default.**

## Performance

- **Duration:** 33 min
- **Started:** 2026-11-12T21:22:34Z (dispatch time)
- **Completed:** 2026-11-12T21:56:00Z
- **Tasks:** 2
- **Files modified:** 11, including this plan's scope metadata
- **Estimate proxy:** 45,755 changed diff characters / 4 = 11,438; this is not API usage.

## Accomplishments

- Added returning tier-A wrappers for the unchanged pinned `audio/tests.s` and `audio2/tests.s`. Both clear the full zero page, independently record the RMB0 CPU probe, expose progress/result/model/sentinel symbols, return from `_run_tests`, and write completion afterward.
- Extended the pinned builder and manifests. Local build records bind source and main hashes, wrapper and linker-layout hashes, native tool hashes, object/label hashes, and the cc65 build-record identities. A verification mode rejects stale outputs and an invalid local tool recipe record.
- Added strict source-derived audio/audio2 oracle parsing. `--include-audio` emits 18 rows per suite with six/seven exact case labels and retains audio2 index 9's `30|34` alternative. The default oracle stays CPU/timer-only for the current strict C reader.
- Added private source, identity, malformed-row, stale-object and stale-label controls. Updated provenance and public-suite docs to describe source-construction evidence and the explicit Plan 09 input mode.

## Task Commits

1. **Task 1: Build unchanged audio guests with auditable wrappers and portable identities** — `4112f91` (`feat`)
2. **Task 1 integrity fix: Validate the pinned local cc65 build record** — `2953dbb` (`fix`)
3. **Task 2: Generate complete source oracles and reject malformed source shape** — `bddf2f5` (`feat`)
4. **Task 1 portability fix: Satisfy Linux ShellCheck 0.9** — `10e7a2a` (`fix`)

## Verification

- macOS: pinned builder, local identity verification, and `check-manifests.sh` passed; ShellCheck 0.11 passed for all changed shell scripts.
- Reused Linux container: pinned builder and local identity verification passed with its own native tool hashes; `check-manifests.sh`, extended and legacy oracle generation, ShellCheck 0.9, and the complete private audio script-control matrix passed.
- Linux `suite.oracle_controls` and `suite.symbol_controls` passed (2/2), confirming the default CPU/timer oracle and build records remain accepted by the existing harness.
- Both extended oracle outputs contain exactly 18 audio rows and 18 audio2 rows. No source bytes, firmware, generated BLL/object/labels, or new dependencies were committed.

## Files Created/Modified

- `fixtures/suites/audio-wrapper.s`, `fixtures/suites/audio2-wrapper.s` — project-authored returning wrappers.
- `fixtures/manifests/audio-tests.manifest`, `fixtures/manifests/audio2-tests.manifest` — portable source/recipe identities and allowed-use statements.
- `scripts/build-suites.sh` — builds both suites, validates the host-local cc65 build record, and checks current object/label identities.
- `scripts/generate-suite-oracle.sh` — preserves default CPU/timer output and adds explicit strict audio/audio2 source parsing.
- `tests/gates/audio-script-controls.sh` — private malformed-source, identity, and stale-output controls.
- `fixtures/suites/NOTICE`, `docs/public-suites.md`, `docs/provenance.md`, and `06-02-PLAN.md` — rights inventory, evidence boundary, opt-in contract, and authorized scope update.

## Decisions Made

- Keep audio oracle generation opt-in with `--include-audio` until the audio reader consumes the extended sections. This preserves the current strict CPU/timer reader; both legacy oracle and symbol CTests pass.
- Keep native cc65 executable hashes in local successful build records. Portable manifests instead bind the cc65 archive pin and source recipe.
- Leave EVD-06 open. Plan 06-02 provides source builds and source-derived expectations, not guest observations, device behavior, profile qualification, or hardware accuracy.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Verify local tool-build metadata and reject stale suite artifacts**
- **Found during:** Task 1 (suite builder)
- **Issue:** Executable hashes were compared with `.lynx-build-files`, but the builder did not check the successful `.lynx-build` pin/recipe marker or provide a way to detect an object or labels changed after generation.
- **Fix:** Validate the local cc65 archive pin, recipe marker and build record; add `--verify DIR` to check current source/tool/build identities and object/label digests. Private controls mutate the tool marker, audio object and audio2 labels and require named failures.
- **Files modified:** `scripts/build-suites.sh`, `tests/gates/audio-script-controls.sh`
- **Verification:** macOS/Linux builds and verify mode pass; tool-marker, stale-object and stale-label controls fail for their named reasons.
- **Committed in:** `2953dbb` and `bddf2f5`.

**2. [Rule 3 - Blocking] Make the argument-presence guard pass Linux ShellCheck 0.9**
- **Found during:** Task 2 verification
- **Issue:** Linux ShellCheck 0.9 flagged the combined `&&`/`||` argument guard even though macOS ShellCheck 0.11 accepted it.
- **Fix:** Replace it with an explicit `if` guard and rerun both supported linters.
- **Files modified:** `scripts/build-suites.sh`
- **Verification:** ShellCheck 0.11 and 0.9 pass.
- **Committed in:** `10e7a2a`.

### Parent-authorized scope adjustment

`fixtures/suites/NOTICE` now lists the two new project-authored wrappers because its header enumerates owned files in that directory. The pinned MIT license text and rights statement are unchanged. The plan's `files_modified` and Task 1 file list were updated accordingly.

**Total deviations:** 2 auto-fixed issues and 1 parent-authorized scope adjustment. All were needed for identity integrity, required verification, or accurate rights inventory.

## Known Stubs

| File | Line | Note |
| --- | ---: | --- |
| `docs/provenance.md` | 80 | This pre-existing text describes the Phase 3 diagnostic's intentional zero-valued WAV. The runtime is outside this source-construction plan; the open audio placeholder is already tracked in `.planning/WINDOWS.md` for Phase 6. |

## Issues Encountered

- The first private build control omitted the existing CPU/timer wrappers needed by the shared builder. The control fixture was completed and rerun successfully.
- The first truncated-array mutation left later C source in the synthetic file, causing an earlier generic parser diagnostic. The mutation was bounded at the array and then produced the exact truncation diagnostic.

## User Setup Required

None. No external service, package install, firmware, commercial ROM, or physical hardware access was needed.

## Next Phase Readiness

Plan 09 can request `--include-audio` to obtain the four-suite source oracle, but its audio reader must explicitly accept the extended metadata/sections. EVD-06 remains open until the later guest runner records audio/audio2 observations and controls. Parent-owned `STATE.md`, `ROADMAP.md`, and `REQUIREMENTS.md` remain for the orchestrator's closeout; they were not edited or staged here.

---
*Phase: 06-audio-and-playable-checkpoint*
*Completed: 2026-11-12*

## Self-Check: PASSED

- All created/modified plan files and this summary exist.
- All four task/fix commits recorded above are ancestors of `plan_head_after`.
- `git diff --check` passes for the remaining tracked workspace changes.
