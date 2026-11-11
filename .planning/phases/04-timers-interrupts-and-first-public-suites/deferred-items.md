# Deferred items: Phase 4

## Timer scheduler cursor snapshot serialization — resolved in Plan 06

The full check preset exposed two failing existing cases after the timer
scheduler was added: `api.snapshot_refusal` and `api.snapshot_roundtrip`. The
same-profile restore controls compare the complete `lynx_machine` buffer.
Field-level inspection showed the timer array, pending byte, and phase policy
match; only `timer_bank.last_event_valid` and `timer_bank.last_event_tick` do
not round-trip. A roundtrip restore retained cursor `(valid=1, tick=4110)`
instead of the saved `(valid=1, tick=2060)`. A fresh cross-profile control had
`(valid=0, tick=0)` while the source held `(valid=1, tick=495)`. CPU state,
guest ticks, CPU-ready tick, and host observation matched in the roundtrip
comparison.

Plan 06 format 4 stores only the current-tick applied-event bit and rebuilds
the cursor timestamp from guest time; stale historical cursor values normalize
away when time advances. `api.snapshot_refusal` and `api.snapshot_roundtrip`
passed in the full CTest run; that run's four failures were stale version/size
checks in API and diagnostic consumers, later fixed and passed in a 9/9
targeted retest. The earlier cursor failures are retained here as the diagnosis
record, not as open failures.

## ShellCheck SC1007 in the canonical check — owned by Phase 12

`sh scripts/check.sh` stopped at its shellcheck step under the installed
ShellCheck 0.11.0. It reported SC1007 at lines 4, 39, 44, 45, and 57 of the
unchanged `scripts/build-suites.sh` for the existing `CDPATH= cd` assignments.
The script stopped before its workflow/CTest step. This is outside Plan 06's
changed files; Phase 12 owns the narrow correction and successful canonical
gate. Do not report the full check as green until then.
