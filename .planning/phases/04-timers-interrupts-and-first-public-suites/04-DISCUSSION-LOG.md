# Phase 4 — Discussion Log

**Date:** 2026-11-10
**Mode:** autonomous single-pass discussion under existing owner delegation
**Authority:** `.planning/AUTONOMY.md`; no new owner acceptance recorded.

| Area | Alternatives | Auto-selected recommendation | Reason |
|---|---|---|---|
| Event selection | bounded scan / heap / per-tick loop | bounded next-event scan | Small fixed source set; no measured heap benefit. |
| Equal-tick behavior | implicit module order / explicit policy | timer catch-up, CPU action, IRQ poll/completion; stable key | Deterministic host budget and watch prefix need a complete order; hardware certainty is not invented. |
| Timer unknowns | fabricate all links / unsupported mode | source-backed owned links; explicit unsupported inputs | Preserve honest device coverage. |
| Reset phase | claim physical constant / declared deterministic phase | fixed-zero project policy, serialized/identified | Epyx reset uncertainty and shared-source software expectations remain visible. |
| IRQ masking | pending AND enables / latch-driven line | pending-driven line; enables gate generation | Primary interrupt appendix explicitly describes forced pending and disable+ack behavior. |
| Sleep | fake Suzy/full sleep / minimal seam | request/grant test seam; unsupported guest behavior explicit | Hardware caveat matters; later arbitration stays separate. |
| Public fixtures | cartridge builds / original BLL wrapper | untouched test assembly, original wrapper, temporary outputs | Tier A, no firmware/TGI initialization and exact provenance. |
| Results | zero array/halt / semantic completion+model+source counts | independent sentinel/progress/time/model/count controls | Unexecuted zero values cannot establish completion. |
| Claims/rights | adopt baseline/licence / keep stops | raw per-profile software observations; existing stops preserved | No confirmation is needed to keep an unresolved stop. |

Three independent decision-cluster researchers produced the adjacent memos. Root adversarial synthesis corrected pending/enable masking and overbroad publication wording; technical recommendations do not become owner decisions. Timers2 remains supporting source evidence rather than a new mandatory suite. The canonical context was written once; downstream technical research/planning now consumes it.
