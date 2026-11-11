# Deferred items: Phase 4

## Timer scheduler cursor snapshot serialization — owned by Plan 06

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

Plan 06 owns timer/scheduler snapshot serialization. Keep both test IDs visible
as failures until that work is implemented and these whole-buffer checks pass.
No timer snapshot-continuation claim is made by Plan 04.
