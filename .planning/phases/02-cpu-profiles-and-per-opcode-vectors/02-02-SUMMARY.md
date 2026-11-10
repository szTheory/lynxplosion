---
phase: 02-cpu-profiles-and-per-opcode-vectors
plan: 02
subsystem: evidence-records
tags: [validate-kv, startup-tier, manifests, claims, exposure-record, clean-room]

requires:
  - phase: 01-evidence-harness-and-walking-skeleton
    provides: "validate-kv.sh, check-manifests.sh, check-claims.sh and their controls; exposure record; claim template"
  - phase: 02-cpu-profiles-and-per-opcode-vectors
    provides: "plan 02-01 CPU core (no code dependency; records only)"
provides:
  - "scripts/validate-kv.sh --startup-tiers \"T1 T2 ...\" (default A, malformed list is usage error 2)"
  - "fixture manifests and claims accept startup_tier none (check-manifests.sh and check-claims.sh pass \"A none\"); run manifests keep the A-only default"
  - "seeded controls for each direction in det.manifest_fields.self_test, hygiene.manifests.self_test and docs.claim_template.self_test (no new CTest name)"
  - "Phase 2 exposure rows (nine sources) and the sentence 'Phase 2: no emulator source was opened.'"
  - "claim.template note describing a future CPU vector claim (content_sha256 = SHA-256 of tests/inventory/vectors/<profile>.txt, startup_tier none, T1, software model on a flat bus, no timing/interrupt/memory-map claim)"
affects: [02-03, 02-04, 02-05, 02-11]

actuals:
  tokens: 2254
  tasks: 2
  commits: 2

plan_head_before: 76098b3d8f82f44ae700cd5676f52aa0abafd7e0
plan_head_after: ec04b3121787a812975aa2dd04cb260bdface71d

tech-stack:
  added: []
  patterns:
    - "Vocabulary option on a shared validator with a conservative default; callers opt in, and the run-manifest path stays untouched so its refusal of the new value is a control, not an assumption"

key-files:
  created: []
  modified:
    - scripts/validate-kv.sh
    - scripts/check-manifests.sh
    - scripts/check-claims.sh
    - tests/gates/manifests_controls.sh
    - tests/gates/claims_controls.sh
    - tests/det/manifest_controls.sh
    - docs/policy/exposure-record.md
    - docs/claims/claim.template

key-decisions:
  - "A malformed --startup-tiers list (empty, leading digit, character outside letters/digits/underscore) exits 2 with the usage line rather than yielding an empty vocabulary that would refuse everything or accept nothing silently"
  - "Phase 2 exposure rows cite the Phase 2 discuss and research records in the last column; the table header text was extended to say so (it previously said rows come from .planning/research only)"

requirements-completed: []

duration: not measured per task (one session; check.sh total 93 s)
completed: 2026-11-09
status: complete
---

# Phase 2 Plan 02: startup_tier vocabulary, exposure record and claim-template note Summary

**`validate-kv.sh` gains `--startup-tiers` (default `A`); fixture manifests and claims accept `none` for software-model vectors, run manifests still accept only `A`, each direction proven by a seeded control; the exposure record lists every Phase 2 research source and states no emulator source was opened.**

## Accomplishments

- Task 1 (commit 7e346ae): the hard-coded `value != "A"` rule is now membership in the `--startup-tiers` list passed to awk through the environment (same convention as the other options, so awk never interprets escapes). `check-manifests.sh` and `check-claims.sh` pass `"A none"`; the run-manifest validator call (`tests/det`, runner) is unchanged. Controls added:
  - `det.manifest_fields.self_test`: `tier_none_run` (default refuses `none`), `tier_none_listed`, `tier_none_a_only`, `tier_b_listed` (refuses `B` even with `A none`), `tier_a_listed`, `tier_list_usage` (empty, `1x`, `a-b` all exit 2).
  - `hygiene.manifests.self_test`: `startup_tier_none` (exit 0) and `startup_tier_unknown` (exit 1, names the file, line and key).
  - `docs.claim_template.self_test`: `startup_tier_none` (passes) and `startup_tier_unknown` (refused, names the claim, line and key).
- Task 2 (commit ec04b31): nine exposure rows (WDC W65C02S, Rockwell R65C02, CMD G65SC02 datasheets; Epyx Lynx specification chapters; ca65/ld65 manual; SingleStepTests/65x02 README, LICENSE and sampled data at 2f6980a2d95757486c7bee24355c360e40e2a224 with the generator not read; lynx-tests `lynx-page-mode.md` prose with links not followed; the nesdev interrupts page; a cycle-stepped 6502 design article, prose only) and the closing sentence. The claim template gained the vector-claim paragraph and the startup_tier key description now mentions `none`.

## Verification (measured)

- `ctest --preset check -R '^(hygiene\.manifests|docs\.claim_template|det\.manifest_fields)(\.self_test)?$'`: 6 of 6 passed (0.1 to 4.5 s each); every new case printed its `: ok` line under `ctest -V`.
- `shellcheck -s sh` on the six touched scripts exits 0.
- `sh scripts/check.sh` exits 0 on the dev host: total 93 s (workflow step 42 s, artifacts 14 s, history 34 s; the 02-01 run measured 29 s total, so the history step and a cold build account for the difference); 62 tests accounted once each, no `hygiene:` finding, no `check: step` line.
- `tests/inventory/fast.txt` and `docs/claims/skeleton-repeatability.claim` are unchanged (no new CTest name), as the plan states.

## Deviations from Plan

### Auto-fixed Issues

None - plan executed as written, with two small additions inside the plan's intent:

1. **[Rule 2 - Missing critical functionality] Malformed tier list is a usage error.** A tier list that is empty or contains a name outside `[A-Za-z][A-Za-z0-9_]*` exits 2, with a control. Without it a typo in a caller would silently change the vocabulary. Commit 7e346ae.
2. **Header text corrections.** `check-manifests.sh` and `check-claims.sh` header comments, the claim-template `startup_tier` line and the exposure-record intro were updated so the documentation matches the new rule. Commits 7e346ae, ec04b31.

### Requirement bookkeeping

`requirements.mark-complete CPU-01` was not run. CPU-01 ("passes pinned per-opcode functional vectors for each defined profile, with an exact per-profile inventory") is only prepared by this plan; the vectors land in plans 02-03 and 02-04 and the requirement should be closed then. `requirements-completed` is therefore empty.

## Known Stubs

None.

## Threat Flags

None. T-02-04 (default vocabulary stays `A`; run manifest with `none` refused by `tier_none_run`) and T-02-05 (every Phase 2 source listed with links-not-followed statement) are mitigated as planned.

## Notes for later plans

- 02-03 fixture manifests for SingleStepTests data must carry `startup_tier = none` and are validated with `--startup-tiers "A none"` automatically through `check-manifests.sh`; `files`/`independence` handling is unchanged.
- 02-04/02-11 vector claim: use the wording in the claim-template paragraph (profile `none` until O1; `content_sha256` is the SHA-256 of `tests/inventory/vectors/<profile>.txt`). A claim with `none` passes `check-claims.sh` only because of this plan.
- The `.planning/state.json` untracked file was present before this plan started and was left alone.

## Self-Check: PASSED

- FOUND: scripts/validate-kv.sh contains `--startup-tiers`; `startup-tiers "A none"` appears once in each of check-manifests.sh and check-claims.sh.
- FOUND: docs/policy/exposure-record.md contains SingleStepTests and "no emulator source was opened"; docs/claims/claim.template contains `tests/inventory/vectors/`.
- FOUND commits 7e346ae and ec04b31 as ancestors of HEAD (`plan_head_before` 76098b3 to `plan_head_after` ec04b31, 2 commits).
