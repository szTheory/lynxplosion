# Phase 04 — UI Review

**Audited:** 2026-11-11
**Baseline:** Phase 04 scope and abstract 6-pillar standards; no `UI-SPEC.md` exists.
**Diff reviewed:** `7b848d0..db7149e`, plus the Phase 04 context, Plans/Summaries 01–11, qualification ledger, Plan 12, README, and public documentation.
**Screenshots:** Not captured; the scoped diff contains no frontend or rendered UI.
**Interaction captures:** off (`workflow.ui_interaction_capture=false`).

## Applicability and Scores

| Pillar | Score | Finding |
|--------|-------|---------|
| 1. Copywriting | 3/4* | Documentation and CLI claims preserve the D-10 mismatch boundary, but CLI help omits timer commands and one example resembles a shell pipeline. |
| 2. Visuals | N/A | No rendered interface is part of Phase 04. |
| 3. Color | N/A | No rendered interface is part of Phase 04. |
| 4. Typography | N/A | No rendered interface is part of Phase 04. |
| 5. Spacing | N/A | No rendered interface is part of Phase 04. |
| 6. Experience Design | N/A | This phase adds headless tools and evidence records, not an interactive UI. |

\*The copywriting score applies only to CLI/documentation claim clarity; it is not a visual-interface score. No combined score is reported because five pillars are non-applicable.

## Findings

### Pillar 1: Copywriting — 3/4

- **WARNING — Incomplete CLI usage text:** [`test_public_suites.c`](../../../tests/suites/test_public_suites.c:3200) prints only `run cpu` usage even though the same dispatcher supports `run timers`, `observe timers`, and other timer-suite modes at lines 3152–3189. Add the supported timer observation and strict-comparison forms to the usage text, or direct users to the documented command list.
- **WARNING — Ambiguous profile notation:** [`public-suites.md`](../../../docs/public-suites.md:29) writes `observe timers lynx1|lynx2 ...` as inline command text. A literal shell paste treats `|` as a pipeline, while the CLI expects one `PROFILE` argument. Use `PROFILE (lynx1 or lynx2)` or show two separate invocations.
- **WARNING — README scope lead-in is stale:** [`README.md`](../../../README.md:3) says the current slice covers CPU profiles, memory map, startup, and the diagnostic, while its later current-scope bullet documents the Phase 04 timer guest and suite evidence. Update the opening summary to include the timer model/headless suite work, or label it as the Phase 03 consumer slice.

### Pillars 2–6: Non-applicable

The Phase 04 change set contains emulator core, scripts, tests, fixtures, and Markdown evidence. It contains no frontend source, `UI-SPEC.md`, or changed diagnostic rendering target. `04-CONTEXT.md` also states that no frontend is delivered. Visual, color, typography, spacing, and interactive-screen findings would be invented for this scope, so they are not scored. The changed diagnostic code adds suite tracing and updates snapshot-format assertions; the existing diagnostic output presentation is outside this phase's UI scope.

The documentation accurately distinguishes complete CPU observations, timer observations, the strict timer mismatch, and hardware/profile qualification limits. The pending timer comparison remains index 14 (`0c` observed versus unchanged `0d` source expectation); this review makes no suite-pass or hardware-accuracy claim.

## Priority Fixes

1. **Expand the suite CLI usage text** — developers invoking an invalid timer command currently see only CPU syntax — list the `run timers` and `observe timers` forms and clarify that `PROFILE` is one of `lynx1` or `lynx2`.
2. **Clarify profile notation in the timer example** — readers may copy `|` as a shell pipeline — replace it with a placeholder or two concrete command forms.
3. **Refresh the README lead-in** — the opening scope summary omits timer work described below it — add the bounded timer model/headless suite evidence or identify the sentence as describing the earlier consumer slice.

## Files Audited

- `README.md`
- `docs/public-suites.md`
- `docs/timers-interrupts.md`
- `docs/memory-startup.md`
- `docs/cpu-profiles.md`
- `docs/provenance.md`
- `docs/policy/exposure-record.md`
- `docs/policy/clean-room.md`
- `tests/suites/test_public_suites.c`
- `tests/diag/test_diag.c`
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-CONTEXT.md`
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-QUALIFICATION.md`
- `.planning/phases/04-timers-interrupts-and-first-public-suites/04-12-PLAN.md`

No build, test, CI job, GUI, screenshot, or publication was run for this applicability review.

## Post-review disposition

The three applicable wording findings are fixed in the reviewed source:
expanded CLI help, an explicit single PROFILE argument, and the README scope
lead-in. Root checked the actual text; the CPU help control passes. Original
scores/findings above retain the initial audit history. Visual applicability
remains N/A; no human visual check is required.
