# Lynxplosion

Lynxplosion is an Atari Lynx emulator core in early development. The current
slice covers explicit CPU profiles, the CPU memory map, direct-load startup
tier A, an original guest-RAM diagnostic, and headless public CPU/timer suite
observations. The timer suite retains one source disagreement as pending
evidence; its strict comparison still fails on that row.

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

Prerequisites: a C17 compiler, CMake 3.25 or newer, Ninja, shellcheck, make,
curl, git and iconv.

The 3.25 floor is the oldest CMake that supports workflow presets. CI runs a
newer CMake; the floor was verified in a container.
