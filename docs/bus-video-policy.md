# Video, refresh and CPU bus policy

This document separates the source-backed bus rules from the project's
unverified schedule. It applies to the native Lynxplosion core and does not
describe calibrated Lynx hardware timing.

## Policy identities

`LEGACY_FLAT5` remains the default. It has no display or refresh grants and
keeps the historical flat-five CPU timing identity. Enabling the video model
requires the explicit `VIDEO_POLICY1` timing policy; it does not rewrite the
legacy cost table or reinterpret results recorded under `LEGACY_FLAT5`.

`VIDEO_POLICY1` is a deterministic software schedule:

| Requester | Request schedule | Grant duration | Status |
|---|---|---:|---|
| Video | One byte per visible row, up to 80 bytes; the next byte is requested at the prior grant's release | 5 guest ticks | Unverified software choice |
| Refresh | One request at each 256-tick phase, with at most one pending request | 5 guest ticks | Unverified software choice |
| CPU | Existing posted micro-operation when its priced cycle is ready | Price is selected only when granted | Existing guest CPU operation |

Missed refresh phases coalesce into one pending request. After a delayed grant,
the next deadline remains on the original 256-tick phase grid and is the first
phase strictly after the grant. No burst of catch-up refreshes is generated.

## Exclusive arbitration

One guest-time scheduler arbitrates timer events and bus grants. The source
specification says one bus master owns the system bus at a time and orders
Video before Refresh before CPU. Equal-tick video and refresh requests therefore
grant video first; the refresh request stays due until the video grant releases
the bus. A video request that arrives during refresh waits for refresh release.
CPU requests remain posted with their address, data, flags and cycle kind while
another master owns the bus. `cpu_ready_at` describes only the end of the
priced CPU cycle; it does not include ownership delay. The private `bus_hold`
test seam remains a separate gate.

Every bus grant consumes its interval before another requester can start. The
half-open advance budget still excludes an event exactly at the target, while
timer work continues during video and refresh ownership. A one-shot watch is
reported only after the original CPU write completes. Disabling display DMA in
the middle of a row abandons that partial row and its remaining DMA requests;
re-enabling waits for the next Timer 0 row event. Already consumed guest time
and completed byte grants are not rolled back.

## Evidence boundary

The Epyx Handy specification documents exclusive bus ownership, the
Video/Refresh/CPU priority, display DMA enable, and CPU page-mode constraints.
It does not define the complete DMA cadence, per-byte grant duration, refresh
period, or all ownership transition edges. Those numeric schedules above are
therefore explicit software policy, not hardware measurements. They must not
be tuned to match the pinned page-mode or timer-suite oracle.

The source also says page mode applies only to opcode fetches, normal data
reads and writes use full cycles, MAPCTL bit 7 disables page mode, and other
opcode/system conditions may exclude an otherwise same-page fetch. The exact
software eligibility subset and the conservative stream-close rule are
specified with the page-mode implementation in this document's next section.

Primary references: Epyx, *Handy Hardware Specification*, [CPU/ROM chapter,
"RAM Page Mode" and "CPU Cycle Tick Summary"](https://www.monlynx.de/lynx/lynx4.html),
and [Appendix 1, "System Bus Interplay"](https://www.monlynx.de/lynx/lynx11.html).
The documentation establishes constraints and priority; it does not establish
the unverified schedule in `VIDEO_POLICY1`.
