---
schema_version: 1
open_count: 5
waived_count: 0
fixed_count: 1
total_count: 6
last_updated: 2026-11-12T09:14:53.962Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 03 | stub | include/lynxplosion.h | 11 | Flat five-master-tick timing remains provisional; this plan proves deterministic bounded guest execution without hardware timing accuracy. | open |  | 2026-11-10T16:36:22.165Z |  |
| 2 | 03 | deviation | tests/det/sensitivity.cmake |  | Migrated the required runner sensitivity test to binary BLL after removal of hex and host load-address inputs. | fixed |  | 2026-11-10T17:05:08.443Z | 2026-11-10T17:05:11.869Z |
| 3 | 03 | stub | consumer/main.c | 464 | Phase 3 WAV output is intentional 800-sample silence pending Phase 6 audio. | open |  | 2026-11-10T21:55:54.394Z |  |
| 4 | 04 | stub | docs/timers-interrupts.md | 58 | CTLB bits 2–0 are not modeled and read as zero. | open |  | 2026-11-11T04:53:22.726Z |  |
| 5 | 04 | stub | docs/timers-interrupts.md | 90 | Timer bank and event cursor are absent from snapshots until Phase 4 Plan 06. | open |  | 2026-11-11T04:53:22.856Z |  |
| 6 | 05 | stub | consumer/main.c | 682 | Placeholder PCM silence remains pending Phase 6 audio implementation. | open |  | 2026-11-12T09:14:53.962Z |  |

````json
[
  {
    "id": 1,
    "kind": "stub",
    "phase": "03",
    "file": "include/lynxplosion.h",
    "line": 11,
    "description": "Flat five-master-tick timing remains provisional; this plan proves deterministic bounded guest execution without hardware timing accuracy.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-10T16:36:22.165Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 2,
    "kind": "deviation",
    "phase": "03",
    "file": "tests/det/sensitivity.cmake",
    "line": null,
    "description": "Migrated the required runner sensitivity test to binary BLL after removal of hex and host load-address inputs.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-10T17:05:08.443Z",
    "resolved_at": "2026-11-10T17:05:11.869Z",
    "milestone": null
  },
  {
    "id": 3,
    "kind": "stub",
    "phase": "03",
    "file": "consumer/main.c",
    "line": 464,
    "description": "Phase 3 WAV output is intentional 800-sample silence pending Phase 6 audio.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-10T21:55:54.394Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 4,
    "kind": "stub",
    "phase": "04",
    "file": "docs/timers-interrupts.md",
    "line": 58,
    "description": "CTLB bits 2–0 are not modeled and read as zero.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-11T04:53:22.726Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 5,
    "kind": "stub",
    "phase": "04",
    "file": "docs/timers-interrupts.md",
    "line": 90,
    "description": "Timer bank and event cursor are absent from snapshots until Phase 4 Plan 06.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-11T04:53:22.856Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 6,
    "kind": "stub",
    "phase": "05",
    "file": "consumer/main.c",
    "line": 682,
    "description": "Placeholder PCM silence remains pending Phase 6 audio implementation.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-12T09:14:53.962Z",
    "resolved_at": null,
    "milestone": null
  }
]
````
