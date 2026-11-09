# Lynxplosion

Lynxplosion is an Atari Lynx emulator core in early development. Phase 1 is a
walking skeleton: a build, a test gate and a small public C API around a
provisional machine. There is no CPU and no emulation yet.

## What is and is not claimed

- No claim of compatibility or accuracy is made. Nothing here runs a game.
- A digest that two runs of the same core agree on is repeatability evidence
  only. It does not show that the core matches Lynx hardware.
- The raw RAM image loader is provisional. A later phase replaces it with the
  real direct-load format.
- RAM is filled with `0x00` at power-on unless a caller asks for the seeded
  fill. Fixed `0x00` is the default everywhere.

## Rights

All rights reserved. No licence has been chosen yet (owner decision O5 is
open).

This repository contains no BIOS, firmware or commercial game content, and
contributions must never add any.

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
