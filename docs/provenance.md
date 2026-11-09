# Provenance and third-party notices

Where code, algorithms and pinned tools in this repository come from, and which
notices that creates. The project licence is not chosen (owner decision O5 is
open); this file does not choose one. The check `docs.provenance` fails when a code
file whose header says it was adapted from the owner's muhtari project is missing
from the list below.

## Adapted from the owner's muhtari project (MIT)

muhtari is an owner-authored project, licensed MIT, "Copyright (c) 2026 muhtari
contributors". The files below were adapted from it at commit `7fd0386`. Each file
keeps a header line naming the source and the commit. The permission notice of the
MIT licence applies to the adapted parts.

- `cmake/Tests.cmake`
- `core/sha256.c`
- `core/sha256.h`
- `scripts/fetch-pin.sh`
- `scripts/hygiene.sh` (header names the project and the original script path; adapted, not copied verbatim)
- `tests/check.h`
- `tests/cmake/abi.cmake`
- `tests/cmake/check_junit.cmake`
- `tests/cmake/expect_fail.cmake`
- `tests/cmake/inventory_equal.cmake`
- `tests/cmake/no_float.cmake`
- `tests/cmake/no_skip.cmake`
- `tests/cmake/pins.cmake`

Assumption A10 (research): the owner may confirm the notice form for copying MIT
helpers from one owner project into a repository whose own licence is still open.
Until then this list is the notice. It is an open question, not a decision.

## Algorithms and public specifications

- **SplitMix64** (S. Vigna, 2015, public domain): the seeded RAM fill policy in
  `core/machine.c`, with an independently written reference in the API tests.
- **SHA-256** as specified in FIPS 180-4: `core/sha256.c` and `core/sha256.h`
  (constants, padding and message schedule follow the standard; the code is the
  muhtari adaptation listed above).

## Pinned external sources (fetched at check time, never committed)

- **cc65** (zlib licence): the guest toolchain, pinned in `pins.txt` by commit and
  SHA-256 of the source archive. Its runtime is linked into guest test programs, so
  the zlib notice travels with any distributed guest binary.
- **lynx-tests** (MIT, test source by the Gearlynx author): pinned in `pins.txt` by
  tag and SHA-256. Used as test source only; no built binary or source is committed.

No emulator source (Gearlynx, MAME, Holani, Beetle) is copied, linked or vendored;
see `docs/policy/clean-room.md` and `docs/policy/exposure-record.md`.
