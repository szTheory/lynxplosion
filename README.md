# Lynxplosion

Lynxplosion is an Atari Lynx emulator core in early development. The current
slice covers explicit CPU profiles, the CPU memory map, direct-load startup
tier A, an original guest-RAM diagnostic, timer-driven raw video, logical
input, and headless public CPU/timer suite observations. Phase 5 adds a bounded
raw-frame API and a public-only consumer that replays guest-tick input against
an original source-built homebrew. These are deterministic software
observations; they do not qualify physical Lynx behavior.

## Current scope and claims

- `startup=A` means direct RAM load from the project's measured BS93/BLL
  subset. It is not BIOS or cartridge boot evidence.
- `none` is the default CPU profile; `lynx1` and `lynx2` are explicit profiles.
  No Lynx I/II baseline or compatibility pass has been adopted (owner decision
  O1 remains open).
- The pinned headless CPU suite has complete 18-byte software observations for
  both explicit profiles. The timer guest also completes for both profiles,
  with 15 of 16 source comparisons agreed and the owner-approved D-10 row still
  pending (`0c` observed, unchanged source `0d`). The strict timer comparison
  continues to fail on that row; these software results do not qualify hardware
  profiles. See [suite evidence](docs/public-suites.md).
- The sourced MAPCTL decoder selects RAM behind the Suzy, Mikey, ROM and vector
  windows. Missing ROM and unimplemented device routes refuse explicitly.
  Remaining device behavior and flat five-tick timing are provisional.
- The diagnostic consumer emits an unscaled 8x8 P3 `SNAPSHOT` using a
  diagnostic-owned palette and an 800-sample silence WAV labelled as
  placeholder audio. These are not a video frame, hardware scanout or real
  audio output.
- The Phase 5 consumer records packed 4-bpp pens, one raw palette snapshot per
  visible line, guest line ticks, and rendered PPM output as separate evidence.
  Its input timeline is a canonical file of logical masks applied at exact
  guest ticks and sampled by guest CPU reads. The guest fixture, bus timing,
  page-mode behavior, palette boundary, refresh cadence and raster output are
  software policies or observations; none is physical hardware qualification.
- `LEGACY_FLAT5` remains the default timing policy. In its no-DMA, zero-counter
  timer environment, the only pending D-10 row is result index 14 (`0c`
  observed versus unchanged source `0d`). The separate `VIDEO_POLICY1`
  observation reports `0f` versus `0d` at that index and retains a strict
  mismatch with no pending exception. Both environments agree on the other
  15 of 16 timer rows; global EVD-02 and O1 remain open. See the [timer
  revisit](docs/timer-video-revisit.md).
- The pinned page-mode source and oracle build are recorded, but its two
  profile observations stop before the first test result at the unsupported
  `SERCTL` write of `$06` to `$FD8C`. Both exact profile cases are declared
  skips with missing raw results; they are not source agreement or profile
  qualification. See [suite evidence](docs/public-suites.md).
- Matching digests across runs or compilers establish repeatability only. They
  do not establish hardware accuracy.

## Rights

All rights reserved. No project licence has been chosen yet (owner decision O5
is open). This repository contains no BIOS, firmware or commercial game
content, and contributions must never add any without documented permission.

## Documents

- [Memory map, direct-load startup and consumer demo](docs/memory-startup.md)
- [Clean-room policy](docs/policy/clean-room.md) (draft, pending owner
  decision O6)
- [Exposure record](docs/policy/exposure-record.md)
- [Provenance notice](docs/provenance.md)
- [CPU profiles](docs/cpu-profiles.md)
- [Raw video and input](docs/video-input.md)
- [Bus and video software policies](docs/bus-video-policy.md)
- [Input replay](docs/input-replay.md)
- [Format 5 video snapshot state](docs/snapshot-video.md)
- [Video and input fault evidence](docs/video-faults.md)
- [Claim template](docs/claims/claim.template)

A claim is bound to the exact test inventory: it records the SHA-256 of
`tests/inventory/fast.txt`, and a check fails when the inventory changes
without the claim being regenerated.

## Running the checks

```sh
sh scripts/check.sh
```

This one command is also what CI runs. It prints tool versions, lints the shell
scripts, then runs `cmake --workflow --preset check` (configure, build with
warnings as errors and sanitizers, test), and reports each step's wall time.

At reviewed source `d3fb002`, the canonical gate passed on macOS 26.6.2 arm64,
a self-contained Ubuntu 24.04 arm64 checkout, and hosted Ubuntu 24.04 CI. Each
accounted for 202 tests: 200 passed and the two declared page-mode observations
were skipped. The strict timer-source disagreements and missing page-mode
observations documented above remain unresolved; these checks do not qualify
hardware behavior.

Prerequisites: a C17 compiler, CMake 3.25 or newer, Ninja, shellcheck, make,
curl, git and iconv.

The 3.25 floor is the oldest CMake that supports workflow presets. CI runs a
newer CMake; the floor was verified in a container.
