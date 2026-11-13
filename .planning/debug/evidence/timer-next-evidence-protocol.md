# Proposed next timer evidence step

Status: proposal only. Physical testing has not been approved or started.

## Decision in plain language

The software repair is independently testable, but the historical timer
expectation depends on bus timing that the available specification does not
fully describe. The recommended next scope is a small, non-invasive study on
one identified Lynx using an existing homebrew loader. First establish whether
the owner has that equipment; do not buy hardware or assume a model. This is
separate from accepting the current gap or closing the milestone.

## Concrete proposed scope

- Prepare source-built diagnostics using the already pinned timer source and
  toolchain after checking the existing fixture rights/loader recipe. Keep
  binaries private; use no commercial game or extracted firmware images.
- Keep the timed Test6 instructions, loop placement and source13 expectation
  unchanged. Record the complete build/map identity. Explicitly clear the
  acknowledgment counter before entry and record its zero value outside the
  timed interval. Label any instrumented companion separately.
- On one named physical model, record three fresh runs for each combination
  of display DMA enabled/disabled and page mode enabled/disabled:12 total.
  Restore display after the timed interval so results can be photographed.
  Record actual register setup, model/board information when available,
  startup method and result bytes. No logging inside the polling loop.
- Compare the four configurations with fresh emulator instances using matching
  preconditions. Retain mismatches. A matching count alone does not qualify
  the full bus model; unexpected results determine whether a later externally
  captured cycle trace is necessary.
- Proposed human run window: at most15 minutes,12 runs, no automatic retries
  after unexpected behavior. Stop at the bound or any unexpected fault. No
  soldering, case opening, hardware modification, cartridge swapping while
  powered, measurement wiring, firmware extraction or public publication.

## Authority and responsibilities

The agent can prepare and check the software recipe once the next scope is
selected. The owner must identify available hardware and explicitly authorize
physical execution; a human operates the console and supplies the results.
There is currently no such authorization. Further instrumentation, purchases,
physical changes or a larger run window would be separate decisions. If no
hardware is available, keep strict EVD-02 pending and seek an existing trace
or fuller primary bus documentation rather than tune the emulator to13.

## Evidence acceptance

Keep source observations, physical results and inference separate. The record
must bind the diagnostic bytes/map, initial counter, model, relevant registers
and all12 results. Software tests must still show strict failure for a wrong
count. Neither owner indifference nor this proposal waives the comparison,
qualifies a baseline model, grants rights or authorizes a release.
