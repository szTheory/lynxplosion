# Lynxplosion

Lynxplosion is an Atari Lynx emulator core in early development. Phase 1 is a
walking skeleton: a build, a test gate and a small public C API. Phase 2 adds a
65C02-class CPU that runs under data-defined profiles. There is no memory map,
no timer, no video and no way to run a game yet.

## What is and is not claimed

- No claim of compatibility or accuracy is made. Nothing here runs a game.
- A digest that two runs of the same core agree on is repeatability evidence
  only. It does not show that the core matches Lynx hardware.
- The raw RAM image loader is provisional. A later phase replaces it with the
  real direct-load format.
- RAM is filled with `0x00` at power-on unless a caller asks for the seeded
  fill. Fixed `0x00` is the default everywhere.

## Profiles

The CPU runs under one of three profiles, chosen when the machine is created
(`lynx-run --profile none|lynx1|lynx2`).

- `none` is the default. It runs only what the Lynx I and Lynx II instruction
  sets agree on. Where they differ (32 opcodes) a run stops with an
  `UNDEFINED` stop before the opcode, changing nothing.
- `lynx1` (65SC02 set) and `lynx2` (Rockwell R65C02 set, with RMB, SMB, BBR and
  BBS) are explicit choices.
- No profile is a baseline: which behaviour to trust is an open owner decision
  (O1).
- Ticks are not timing evidence. The per-access cost is a placeholder of five
  ticks.

Every value, its source and every unverified behaviour is listed in
[docs/cpu-profiles.md](docs/cpu-profiles.md).

## Rights

All rights reserved. No licence has been chosen yet (owner decision O5 is
open).

This repository contains no BIOS, firmware or commercial game content, and
contributions must never add any.

## Documents

- [Clean-room policy](docs/policy/clean-room.md) (draft, pending owner
  decision O6)
- [Exposure record](docs/policy/exposure-record.md)
- [Provenance notice](docs/provenance.md)
- [CPU profiles](docs/cpu-profiles.md)
- [Claim template](docs/claims/claim.template)

A claim is bound to the exact test inventory: it records the SHA-256 of
`tests/inventory/fast.txt`, and a check fails when the inventory changes
without the claim being regenerated.

## Running the check

```sh
scripts/check.sh
```

This one command is also what CI runs. It prints the tool versions, lints the
shell scripts, then runs `cmake --workflow --preset check` (configure, build
with warnings as errors and sanitizers, test), and prints the wall time of
each step.

Prerequisites: a C17 compiler, CMake 3.25 or newer, Ninja, shellcheck, make,
curl, git and iconv.

The 3.25 floor is the oldest CMake that supports workflow presets. CI runs a
newer CMake; the floor was verified once in a container.
