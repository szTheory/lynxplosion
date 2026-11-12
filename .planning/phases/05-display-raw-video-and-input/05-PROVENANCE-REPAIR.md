# Portable input-probe build provenance

The Ubuntu canonical gate at `d09503c` completed CTest but seven frame/replay cases refused before guest execution. Their source/config/legacy-source hashes matched; the fixture manifest required the exact macOS-built ca65/ld65 executable hashes. Ubuntu native binaries correctly matched their own verified build records and differed from the macOS executables. This was a platform-specific fixture contract, not source or guest-output agreement evidence.

Commit `931fa03` binds the fixture to the same portable cc65 archive SHA-256 and build recipe1. The driver cross-checks that pin against pins.txt, the fetched-source marker, successful build stamp and build-files header, then checks each actual native executable against its recorded SHA-256. Source/config/legacy source identities and all source-built guest hashes/lengths remain unchanged. Host-specific native hashes remain in per-build records and tool observations. Historical macOS tool identities are preserved in the pre-repair manifest/history; they are not claimed as portable executable identities.

The manifest extension remains strict/all-or-none: the two obsolete native-tool fields are replaced by cc65_pin_sha256 and cc65_build_recipe. Hash shape, exact recipe1, missing/unknown fields and existing source-built identity constraints are enforced. New malformed pin and recipe controls extend the existing self-test. No rights, licence, pin source/version, guest oracle, claim scope, CTest inventory or emulator behavior changed.

Nine affected macOS tests passed (10.52 seconds), and the same nine Ubuntu tests passed (152.24 seconds, including the complete frame replay matrix). Both ShellCheck0.9 and0.11 accept the changed scripts. Seven isolated preflight controls mutated build-stamp pin/recipe, record-header pin/recipe, manifest pin/recipe and native tool bytes; each was refused with its intended diagnostic before invocation-work creation. Two initial proof-wrapper assertions required whitespace normalization for CMake line wrapping; they did not indicate product failures. Raw proofs remain private. The old full Linux failure remains recorded; these focused passes do not replace final canonical qualification.

Final55-path fingerprint: `ea140006093038be023cdceeb67c4c1468576c940f0aa942b1b4b69bbcf1a1f3`; changed source identities:

| Path | SHA-256 |
|---|---|
| `fixtures/manifests/input-probe.manifest` | `34640404dd0bdd6e05e3e74853625c980f20c10f906231aaddc97ba849b43d89` |
| `scripts/check-manifests.sh` | `dc072995ee0db82b827b6eb0e60370afbf2289934c616d0dcdd381a9d13fff60` |
| `scripts/validate-kv.sh` | `c3cb11e9a2eb68be1b5521272f971f4ebe99145d303e928d736c430c98a01993` |
| `tests/consumer/frame_driver.cmake` | `52d546fb1a92fca8474c37b33db44240f47a28ded05a4da0a509366a7132fafe` |
| `tests/gates/manifests_controls.sh` | `0f46341510e03be12bc5d7869fae8e894a633b14552e34a58fe05417e61cef35` |

Independent code/security affected-scope reviews and final-source macOS/Ubuntu/hosted qualification remain pending.
