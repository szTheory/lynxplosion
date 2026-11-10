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

## Third-party data committed with notice

- **SingleStepTests/65x02** (MIT, "Copyright (c) 2024 Thomas Harte et al"): per-opcode
  CPU test vectors, pinned at commit `2f6980a2d95757486c7bee24355c360e40e2a224`
  (2025-05-10). Only the `synertek65c02/v1` variant (profile `lynx1`) and the
  `rockwell65c02/v1` variant (profile `lynx2`) are used. What is committed under
  `fixtures/vectors/sst65x02/`: one sample container per variant (the first 32 objects
  of each of the 256 opcode files, upstream bytes verbatim), `upstream.sha256` (the
  SHA-256 of each of the 512 whole upstream files) and `LICENSE` (the upstream licence
  text, verbatim, which is the notice that travels with the data). The data is
  software-model test data (evidence tier T1), not hardware evidence, and it carries no
  BIOS, firmware or commercial ROM content.
- The two samples are recorded in `fixtures/manifests/sst65x02-synertek65c02.manifest`
  and `fixtures/manifests/sst65x02-rockwell65c02.manifest`; the synertek manifest also
  names the shared `LICENSE` and `upstream.sha256`.
- The sample is produced by `scripts/vectors-full.sh pin sample`, which downloads the
  files over https from the pinned commit, verifies each against `upstream.sha256` and
  fails closed on any error. That script, and the later full run of every vector in
  every file, is the only network use; it runs outside `scripts/check.sh` and CTest, so
  the check needs no network.
- Clean-room: the vector generator was not read. Only the repository README, the LICENSE
  and the committed data files were read; see `docs/policy/exposure-record.md`.
- The `.gitattributes` rule `fixtures/vectors/sst65x02/* -text` keeps line endings
  untouched so no client changes a SHA-256.

## Pinned external sources (fetched at check time, never committed)

- **cc65** (zlib licence): the guest toolchain, pinned in `pins.txt` by commit and
  SHA-256 of the source archive. Its runtime is linked into guest test programs, so
  the zlib notice travels with any distributed guest binary.
- **lynx-tests** (MIT, test source by the Gearlynx author): pinned in `pins.txt` by
  tag and SHA-256. Used as test source only; no built binary or source is committed.

No emulator source (Gearlynx, MAME, Holani, Beetle) is copied, linked or vendored;
see `docs/policy/clean-room.md` and `docs/policy/exposure-record.md`.
