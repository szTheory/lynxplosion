---
phase: 04
review: 04-REVIEW.md
titles: json
findings:
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "Pin marker parser accepts trailing content"
  - id: CR-01
    severity: critical
    disposition: fixed
    title: "BLOCKER — Predictable oracle temporary can overwrite a symlink target"
  - id: CR-02
    severity: critical
    disposition: fixed
    title: "BLOCKER — Text input is not NUL-terminated before C string operations"
  - id: CR-03
    severity: critical
    disposition: fixed
    title: "BLOCKER — Oracle revision copy can overflow its destination field"
  - id: CR-04
    severity: critical
    disposition: fixed
    title: "BLOCKER — Completion and probe symbols may alias executable code"
  - id: CR-05
    severity: critical
    disposition: fixed
    title: "BLOCKER — Suite validators do not bind all evidence to the verified build object"
  - id: CR-06
    severity: critical
    disposition: fixed
    title: "BLOCKER — Lexical scratch-prefix check permits cleanup through a symlinked ancestor"
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "WARNING — Timer control batches can silently lose or duplicate cases"
  - id: IN-01
    severity: info
    disposition: fixed
    title: "INFO — CLI usage text omits supported timer and validator commands"
  - id: IN-02
    severity: info
    disposition: fixed
    title: "INFO — Timer observation example is parsed as a shell pipeline"
  - id: IN-03
    severity: info
    disposition: fixed
    title: "INFO — README opening omits the newly documented timer suite"
open: 0
total: 11
recorded: 2026-11-11T22:14:28.956Z
---

# Phase 04: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-02 | warning | fixed | 04-REVIEW-FIX.md |
| CR-01 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-02 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-03 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-04 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-05 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-06 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| WR-01 | warning | fixed | 04-REVIEW-FIX.md (not in the current review) |
| IN-01 | info | fixed | 04-REVIEW-FIX.md (not in the current review) |
| IN-02 | info | fixed | 04-REVIEW-FIX.md (not in the current review) |
| IN-03 | info | fixed | 04-REVIEW-FIX.md (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
