# Phase 3 Startup Research: Direct Load and BS93

**Researched:** 2026-11-10  
**Scope:** Startup tier A API, BLL/BS93 object parsing, strict refusal, runner integration, startup state and snapshot identity.  
**Exposure:** No GPL emulator source was opened. No firmware or game content was used.

## Decision-ready recommendation

Use a dedicated public `lynx_load_bll_object(machine, bytes, len)` entry point for strict tier-A startup. It parses a complete 10-byte-header BS93 object, copies only the body into RAM, and starts at the object's load address. Do not autodetect BS93 inside the raw-byte loader. The current public raw loader is explicitly provisional and marked “replaced in Phase 3” in the header, so remove or replace it on the final public surface; migrate test setup, runner and consumer call sites to BLL fixtures. Retain any raw-byte helper only as a private test seam if converting every CPU vector fixture would add unnecessary churn. `[VERIFIED: include/lynxplosion.h:141-150 — “PROVISIONAL raw RAM image loader, replaced in Phase 3.”]`

The strict parser should support the measured pinned-cc65 BS93 subset: exact first bytes `80 08`, big-endian 16-bit load address, big-endian 16-bit total file length including the 10-byte header, `BS93` at offsets 6–9, and a non-empty body whose declared and supplied lengths match exactly. The body starts at offset 10; PC starts at the load address. Bounds are `load_addr < 0x10000` and `body_len <= 0x10000 - load_addr`, calculated without overflowing. This format is a RAM-load container, not a cartridge image or boot-loader image. `[CITED: https://atarigamer.github.io/lynxcc/lnx.html, accessed 2026-11-10; VERIFIED: pinned cc65 source and generated artifacts below]`

## Pinned cc65 output measurement

The repository pins cc65 commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2` with archive SHA-256 `d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1`. `[VERIFIED: pins.txt:9-10 — quote: “cc65 eebf54af545f4ae52f8489a2f6d4d5f93e7942d2 … d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1”]`

I built the existing project-authored `fixtures/toolchain/probe.c` with the pinned `cl65 -t lynx -C build/pins/cc65/cfg/lynx-bll.cfg`. The output was 329 bytes with SHA-256 `5e503d0aea60a92721840a93b8340fcaa8e2a8672e9088a81759644535c69591`. Its first 10 bytes were:

```text
80 08 04 00 01 49 42 53 39 33
```

The pinned linker label file reported `__MAIN_START__ = 0x0400` and `__BSS_LOAD__ = 0x053F`. Therefore the header's load address is `0x0400`, its big-endian total length is `0x0149` = 329 bytes, and the body is 319 bytes. The body starts at the load address; its first instructions are cc65 runtime startup code, including stack setup and a write to MAPCTL. `[VERIFIED: measured output; build/pins/cc65/cfg/lynx-bll.cfg:8-24; build/pins/cc65/libsrc/lynx/bllhdr.s:13-18]`

I also assembled a minimal project-authored 65SC02 loop with pinned `ca65`, then linked it with pinned `ld65`, `lynx-bll.cfg` and cc65's BLL header object. The 13-byte result was:

```text
80 08 04 00 00 0D 42 53 39 33 78 80 FE
```

SHA-256: `cfc79808ee86751cdf0ba3193a2809e4bdc68741bd4eabc221c3bfd518446d19`. This independently confirms total length `0x000D` = 13, body length 3, body address and entry `$0400`, and the `BS93` marker. These outputs were temporary spike artifacts under `/private/tmp/lynx-bll-spike`, not committed fixtures. `[VERIFIED: measured output; build/pins/cc65/libsrc/lynx/bllhdr.s:13-18]`

The relevant pinned header source was opened directly this session:

```asm
.word   $0880
.dbyt   __MAIN_START__
.dbyt   __BSS_LOAD__ - __MAIN_START__ + 10
.byte   $42,$53
.byte   $39,$33
```

`lynx-bll.cfg` places the header in a 10-byte output region and `MAIN` at `$0400`. `[VERIFIED: build/pins/cc65/libsrc/lynx/bllhdr.s:13-18 — verbatim quote above; build/pins/cc65/cfg/lynx-bll.cfg:8-24 — “HEADER … size = $000a” and “MAIN … start = $0400”]`

The cc65 `cl65` output is evidence for the format only. Do not use its linked CRT/library body as the Phase 3 diagnostic: it includes runtime initialization and imports; the diagnostic lens recommends a small project-authored assembly body and project-authored header source with the same measured bytes. That keeps the fixture's code ownership clear and avoids introducing a runtime dependency into the diagnostic. `[VERIFIED: measured body begins `78 A2 FF 9A A9 0C 8D F9 FF`; cited source: build/pins/cc65/libsrc/lynx/crt0.s:45-56]`

## BS93 parsing and compatibility contract

| Field | Strict tier-A rule | Evidence |
|---|---|---|
| Bytes 0–1 | Require canonical marker bytes `80 08`; do not interpret them as a load address or execute a loader. This is the marker emitted by the pinned cc65 BLL header. | `[VERIFIED: build/pins/cc65/libsrc/lynx/bllhdr.s:14 — “.word   $0880”; measured bytes `80 08`]`; SDK guide calls it a loader hint not used by conversion `[CITED: https://atarigamer.github.io/lynxcc/lnx.html, accessed 2026-11-10]` |
| Bytes 2–3 | Big-endian load address; this is also the execution start address. | `[CITED: https://atarigamer.github.io/lynxcc/lnx.html and https://book.atarilynx.dev/012-uploader.html, accessed 2026-11-10]` |
| Bytes 4–5 | Big-endian total object length, including the 10-byte header. Require equality with the supplied input length. | `[VERIFIED: pinned cc65 source formula + 329-byte and 13-byte output measurements; CITED: https://atarigamer.github.io/lynxcc/lnx.html, accessed 2026-11-10]` |
| Bytes 6–9 | Exact ASCII `BS93`. | `[VERIFIED: build/pins/cc65/libsrc/lynx/bllhdr.s:17-18 — “.byte   $42,$53” / “.byte   $39,$33”; CITED: SDK guide above]` |
| Bytes 10 onward | Non-empty executable body copied to `[load_addr, load_addr + body_len)`; initial PC equals `load_addr`. | `[CITED: https://book.atarilynx.dev/012-uploader.html, accessed 2026-11-10; confirmed by assembly measurement]` |

Reject null or too-short inputs, bad marker/magic, a total length below 11, truncated input, trailing bytes, load-range wrap or overflow, and any range that ends beyond the 64 KiB address space. Validate every header/range/overlap condition before the first machine write. A refusal returns a status and leaves the entire machine unchanged. An unknown format should use a distinct appended status if the API adds one; do not renumber existing statuses. `[REC; current refusal contract VERIFIED: include/lynxplosion.h:117-124, 141-148; current subtraction-based bound VERIFIED: core/machine.c:167-170]`

The first two bytes have an important compatibility limit: the current SDK documentation labels them a “loader hint” and says the conversion does not use them, but the pinned cc65 build emits only `80 08`. This recommendation intentionally supports the pinned canonical subset and rejects other hint values; it does not claim every historical BLL producer uses that marker. If broader BLL interoperability becomes a requirement, collect independent valid objects and source documentation before widening this rule. Do not copy the ComLynx upload sequence's complemented `(Len - 10) ^ 0xFFFF` field into the on-disk parser; that is a separate transfer protocol. `[CITED: https://atarigamer.github.io/lynxcc/lnx.html and https://book.atarilynx.dev/012-uploader.html, accessed 2026-11-10; compatibility boundary is [ASSUMED]]`

Do not auto-detect a container inside a raw-byte API: raw program bytes can coincidentally contain `BS93` at offset 6. A separate explicit BLL API makes the accepted format and failure behavior clear. The 64-byte `LYNX` cartridge header and cartridge ROMs are different formats; reject them in the BLL API. No `.lnx` boot, BLL secondary loader, BIOS, boot shim or decryption is part of tier A. `[REC; format distinction CITED: https://atarigamer.github.io/lynxcc/lnx.html, accessed 2026-11-10; no-boot scope VERIFIED: .planning/ROADMAP.md phase 3 lines 153-163]`

## API, ownership, startup state and runner

- Keep the interface allocation-free and synchronous. The caller's byte buffer is read only during the API call; the core copies the body into its own RAM and retains no pointer. Document that the input must be valid for `len` bytes and must not overlap the caller-provided machine storage. Detect overlap before resetting the instance. `[REC; current instance storage VERIFIED: core/internal.h:35-47 — “The caller's buffer is the only mutable state”; current successful loader copy VERIFIED: core/machine.c:172-181]`
- Preserve raw-RAM identity semantics: hash the loaded body, and report its load address and body length. The fixture manifest records the SHA-256 of the complete BS93 file, so the source artifact and the bytes loaded are both traceable. `[REC; current semantics VERIFIED: core/machine.c:173-181]`
- Successful load resets to the configured deterministic power-on RAM policy and CPU state, sets PC to the BLL load address, resets guest time, records content identity, and begins at an instruction boundary. Reset still clears loaded content; the host reloads the same object to start again. `[REC; current power-on/load sequence VERIFIED: core/machine.c:79-100, 172-181; API wording VERIFIED: include/lynxplosion.h:130-138]`
- Set only the execution PC from the container. Leave other CPU state at the project's documented power-on values. `MAPCTL` should likewise start at the documented hardware reset value `0`, rather than silently injecting the cc65 CRT's `$0C` write as host state. The Epyx-derived hardware-address reference says Mikey resets MAPCTL to zero and that setting map bits selects underlying RAM; cc65 CRT code writes `$0C` after execution begins. Mark the latter as guest software initialization, not a boot ROM transition. `[CITED: https://www.monlynx.de/lynx/hardware.html, accessed 2026-11-10; VERIFIED: build/pins/cc65/libsrc/lynx/crt0.s:53-56]`
- The runner currently accepts hex-text content plus an explicit load address and calls the raw loader. Replace that with a binary BS93 input option; derive load address from the header, report the full source file digest, loaded body digest/range, profile, RAM policy/seed, tick budget and `startup_tier = A`. Keep `--profile none|lynx1|lynx2` defaulting to `none`; no baseline or compatibility claim is created. `[VERIFIED: runner/main.c:1-5, 145-168, 222-233; runner/bundle.c:198-214; profile names VERIFIED: include/lynxplosion.h:52-57]`
- The independent consumer should use only the public C API for machine operations. Host file loading, symbol lookup from the project's `-Ln` output, result-block decoding, PPM writing and WAV writing stay in the consumer. The result block is read from raw RAM by symbol; mapped peeks are for CPU-visible inspection. Use PPM to avoid adding an image dependency. `[REC; no new package required]`

Tier A is direct image placement and a named initial state, not boot/cartridge/loader evidence. The only startup value supplied by the BLL object is the load/entry address. Keep the project profile default at `none`; do not name either hardware profile a baseline while O1 remains open. `[VERIFIED: .planning/REQUIREMENTS.md:16-24 — quote: “No requirement adopts Lynx I or Lynx II as baseline (O1 is open)” and “Startup tier A means direct RAM load … it is never boot, cartridge or loader evidence.”]`

## Mapped peeks and snapshots

Keep `lynx_read_memory` as the raw 64 KiB RAM view. Add a separate side-effect-free CPU-mapped peek surface for the decoded address view. CPU bus accesses and the mapped peek should share the address decoder, while the peek path must not consume guest time or trigger read-to-clear/device side effects. Keep bus reads/writes distinct from peek operations so later Mikey/Suzy semantics can add side effects without changing the inspector contract. `[VERIFIED: include/lynxplosion.h:183-187 — “lynx_read_memory reads the raw 64 KiB RAM”; REC for new mapped view]`

The memory map source identifies FFF8 as RAM and FFF9 as the MAPCTL register, with overlays over FC00–FCFF Suzy, FD00–FDFF Mikey, FE00–FFF7 ROM and FFFA–FFFF vector space. MAPCTL bits 0–3 select the underlying RAM when set. The project task from the memory-map lens separately confirms FFF8/FFF9 special behavior. Do not put the ROM bytes in the repository; use an explicitly empty/absent-ROM behavior for this phase and label it as a placeholder, not BIOS behavior. `[CITED: https://www.monlynx.de/lynx/hardware.html, accessed 2026-11-10; absent-ROM representation [ASSUMED]]`

Snapshots already use a manually encoded little-endian format 2, bind build/profile/RAM policy/seed/content identity, hash the payload, validate before applying, and refuse mismatches without mutation. MAPCTL and any new mapped device state must become serialized machine state. Bump the snapshot format when the layout changes; same-build-only restore means no migration is needed. Add tests that capture mid-diagnostic, restore to an independently initialized instance, continue to the same final observation digest, and prove refusal on wrong build/content/config, truncated/trailing data, invalid enum/reserved values and recomputed-hash invalid states. `[VERIFIED: core/internal.h:13-24 — “LYNX__SNAPSHOT_FORMAT 2u” and payload length quote; core/snapshot.c:1-40, 221-307]`

If the Phase 3 memory watch is a host observation setting, keep its range/enable configuration outside emulated state and the snapshot payload. Stop immediately before the watched write, leave the pending CPU bus request intact, then allow a resumed run to perform it; compare resumed and uninterrupted final digests. The watch should not alter the machine state or guest time at the stop point. `[REC; API-08 and .planning/ROADMAP.md phase 3 success criterion 4]`

## Verification plan and negative controls

1. Build a small project-authored ca65 diagnostic with a project-authored BLL header and `ld65 -Ln` symbols. Compare the 10-byte header against the pinned measurements (`80 08`, load BE, total length BE, `BS93`) and verify the total length is header plus body. Build/pin this artifact with the existing cc65 source build and record source, recipe, SHA-256, license and allowed use.
2. Run the same body through the strict BLL public API. Test `len=11` with a one-byte body, smallest body that can execute, maximum representable header length, load at the last RAM byte with one byte, and the exact `load + body == 0x10000` boundary. Refuse load overflow and any out-of-range end.
3. Seed a valid existing instance and assert byte-identical refusal on bad magic, bad first two bytes, wrong length endian, length below 11, truncated input, trailing bytes, overrun, null input and machine-buffer overlap. Verify file buffer mutation/free after success cannot change the loaded machine or its identity.
4. Use a deliberately corrupted but structurally valid diagnostic to prove its RAM result block reports FAIL; use a no-run control to prove the harness does not report PASS without a guest-written completion sentinel and minimum progress.
5. Exercise raw-RAM reads and mapped peeks on both sides of each MAPCTL bit. For every refusal/peek, assert unchanged machine bytes and guest time. Make the address decoder's named-fault control swap one overlay bit and fail only the affected mapping checks.
6. Snapshot at a deterministic mid-diagnostic tick, restore into a fresh same-identity instance and continue; compare final RAM/register/time observations and digest with an uninterrupted run. Add a bad-version control and rehashed invalid-payload controls.

No new package or runtime service is needed. Keep the diagnostic and parser dependency-light; the only added build use is the already-pinned cc65 toolchain. The check should add only tests that prove distinct contract failures and keep one fast local command aligned with the PR gate. `[REC; current toolchain pin VERIFIED: pins.txt:9-10]`

## Open compatibility limits

| Unknown | Current evidence | Recommendation |
|---|---|---|
| Whether historical BLL writers use first two bytes other than `80 08` | Pinned cc65 emits `80 08`; modern SDK docs call it an unused loader hint. | Keep the initial parser subset strict. Broaden only with independent sample files and a primary format rule. `[ASSUMED]` |
| Whether all BLL producers interpret declared length exactly as the generated cc65 object does | Both measured outputs and pinned formula include the 10-byte header; SDK guide says the same. | Require supplied length exactly equal to declared total; expose incompatibility instead of guessing. `[CITED/VERIFIED]` |
| Behavior of actual boot ROM bytes in FE00–FFF7 and ROM vector overlay | Firmware is prohibited in v1; the tier-A runner must never claim boot evidence. | Model an explicit absent-ROM placeholder for mapping tests; do not add firmware fixtures or infer boot. `[ASSUMED]` |
| MAPCTL initial-value nuance for models/revisions | Epyx-derived hardware table says Mikey reset value is 0; no specific Lynx I/II reset divergence was observed in this pass. | Use zero as the documented initial value and keep any later CPU-written map preset separate. `[CITED]` |

## Sources and exposure record

### Primary/project-pinned evidence

- `pins.txt:9-10`; pinned cc65 source commit `eebf54af545f4ae52f8489a2f6d4d5f93e7942d2`, archive SHA above.
- `build/pins/cc65/libsrc/lynx/bllhdr.s:13-18` and `build/pins/cc65/cfg/lynx-bll.cfg:8-24`, opened in this session; measured outputs from the existing project probe and a minimal assembly body.
- `include/lynxplosion.h`, `core/machine.c`, `core/internal.h`, `core/snapshot.c`, `runner/main.c`, `runner/bundle.c`, opened in this session.
- Epyx-derived hardware address table: [App. 3: Hardware Addresses](https://www.monlynx.de/lynx/hardware.html), accessed 2026-11-10. MAPCTL reset, overlay bits, FFF8/FFF9 and overlay ranges were checked.

### Format documentation

- lynxcc SDK guide: [BLL objects → cartridge ROM](https://atarigamer.github.io/lynxcc/lnx.html), accessed 2026-11-10. It documents the 10-byte header, endianness, length field, magic and first-field hint; it also distinguishes BS93 objects from `.lnx` cartridge ROMs.
- Programming Atari Lynx guide: [cc65 uploader](https://book.atarilynx.dev/012-uploader.html), updated 2023–2025 (site copyright), accessed 2026-11-10. It states that the load location is also the execution start and distinguishes on-disk BLL objects from the ComLynx transfer sequence.

No emulator source was opened. No firmware, commercial ROM, physical hardware, public release or external service was used. The `cl65` C output was a temporary measurement only and will not be committed or used as the diagnostic fixture.
