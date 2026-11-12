# Raw video and logical input

This native C slice returns raw guest capture and logical button levels. It
does not poll physical devices, render RGB in the core, or claim qualified
hardware timing.

## Frame and policy contract

The caller supplies arrays of up to 8,160 packed pen bytes (160 pixels by 102
rows), 3,264 raw palette bytes (32 per row), and 102 `uint64_t` guest
line-start ticks. Pens are four-bit indexes, high nibble first, in raw display
order. Each row's palette stores GREEN0..15 then BLUERED0..15. The host owns
color conversion, rotation and scaling. Fixed-capacity tails are zero.

The first Timer 2 boundary synchronizes capture and returns sequence 0 with
zero rows; it is not a renderable image. The following Timer 0 events count
blank ordinals 1 and 2, with the backup display address transferred at
ordinal 2 (the third blank line). Visible rows 0..101 are captured at ordinals
3..104; Timer 2 completes the interval at ordinal 105. PBKUP is retained as a
guest latch, but raw output is not flipped or rotated in the core. The normal
display address is aligned down to a four-byte boundary when the backup
transfers at blank ordinal 2; DMA then walks forward through the 16-bit RAM
address space, including underlying RAM where the CPU sees a device overlay.
`DISPCTL` currently accepts only bit 0 (DMA enable); flip, two-bit and
monochrome mode writes are refused without changing the selected mode. This
limited mode does not implement the manual's bottom-up flip traversal.
Interrupted rows are omitted from `row_count`, their unused slot is cleared,
and the frame is flagged `LYNX_FRAME_PARTIAL`; DMA grants and guest time
already consumed are not rolled back.

Cadence comes from guest Timer 0/2 configuration, not a host refresh rate.
For steady reloads, Timer 0's line period is its source interval multiplied by
`backup + 1`; linked Timer 2 completes after `backup + 1` Timer 0 borrows. The
core's fixed-zero divider phase defines the first edge after guest timer setup.
That initial setup phase is an unverified software policy and is independent
of the steady reload-period checks; it is not a calibrated display edge. At
each Timer 0 row-start event, the current 32 palette register bytes are
snapshotted after timer work and before a same-tick CPU store. This
line-snapshot ordering is project policy; hardware palette latch behavior
remains unverified.
Budgets are half-open: a boundary exactly at the target is deferred, while
already-due current-tick work can return FRAME with zero ticks consumed. Zero
budget makes no progress. Only FRAME copies output arrays; other guest stops
leave them unchanged. Ordinary `lynx_advance` discards frame delivery while
retaining guest effects, and a later frame request waits for the next
completion. Calls serialize on one instance; buffers are caller-owned and no
pointer is retained. Independent instances share no mutable state.

The default `LEGACY_FLAT5` policy has no video or refresh grants. The explicit
immutable `VIDEO_POLICY1` identity schedules one RAM byte per five-tick video
grant and a five-tick refresh grant at ticks 256, 512, and so on; same-tick
order is timer, video, refresh, CPU. Grant ownership delays CPU execution
without changing priced cycle readiness. These cadence, costs and boundary
choices are versioned software policy, not hardware measurements. Identity
and snapshot state include the selected policy; reset/load preserve it and
clear capture/input state. Per-row palette sampling does not prove intra-line
color accuracy.

## Input and evidence boundaries

`lynx_set_input` copies a nine-bit current pressed mask without advancing time.
Bits 0..3 are Up, Down, Left, Right; bits 4..8 are A, B, Option 1, Option 2,
Pause. A/B map to JOYSTICK bits 0/1, Option 2/1 to bits 2/3, Pause to
SWITCHES bit 0. With SPRSYS.Lefthand bit 3 set, directions map Up/Down/Left/
Right to JOYSTICK bits 7/6/5/4; clear swaps each direction pair. These
asserted-high mappings and disconnected cartridge-line behavior are software
choices, not physical polarity or device claims. Other SPRSYS behavior is
unsupported. Input is sampled by guest register reads; tick-zero events occur
after load. Successful reset/load clears all buttons. Snapshots store the
current mask, not host recording bytes, event cursor, physical devices or
clock.

The project-authored tier-A fixture is built from pinned ca65/ld65 source and
configuration. Its manifest binds source, config, tools and generated guest
bytes and records all rights reserved while O5 is open. No BIOS, commercial
ROM, game content or generated binary is included. The tracer proves semantic
repeatability for this guest, not hardware accuracy. The unchanged page-mode
recipe and source descriptor remain scheduled for their later evidence plan.
Separately, the Phase 4 strict timer-offset-14 result remains expected `0D`
versus observed `0C`, pending the Phase 5 timer revisit. This video policy
changes neither record and implies no suite compatibility. O9/HOST-02 and
real audio remain outside the native slice.

Assumption delta: no change. Another emulator appears only in a prohibition
against importing its constants; identity remains profile, content, startup,
build and timing policy.
