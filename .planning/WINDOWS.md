---
schema_version: 1
open_count: 1
waived_count: 0
fixed_count: 0
total_count: 1
last_updated: 2026-11-10T16:36:22.165Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 03 | stub | include/lynxplosion.h | 11 | Flat five-master-tick timing remains provisional; this plan proves deterministic bounded guest execution without hardware timing accuracy. | open |  | 2026-11-10T16:36:22.165Z |  |

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
  }
]
````
