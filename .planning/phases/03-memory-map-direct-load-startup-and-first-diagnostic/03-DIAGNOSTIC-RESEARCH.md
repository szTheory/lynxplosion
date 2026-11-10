# Phase 3 Diagnostic and Consumer Research

**Researched:** 2026-11-10  
**Scope:** Original tier-A guest-RAM diagnostic, viewable guest-memory snapshot, watch/snapshot evidence, rights and fast-check integration.  
**Confidence:** High for the existing project interfaces and measured BS93 subset; medium for implementation guidance; hardware semantics intentionally remain bounded by Phase 3 CONTEXT.

<user_constraints>
## User Constraints (from 03-CONTEXT.md)

### Locked Decisions

#### Authority and autonomous discussion
- Apply the owner's existing delegation in `.planning/AUTONOMY.md`: research
  decision clusters independently, synthesize, and auto-follow recommendations
  within scope. These are orchestrator-selected implementation choices, not
  newly invented owner decisions. No extra acceptance prompt is required.
- Use Codex and resolve each GSD role's configured model and effort together.
  Preserve both runtime mappings and the owner's existing configuration edits.
- O1 remains open; profile `none` stays the default. No baseline, suite-pass
  compatibility claim, licence decision, firmware use, release or physical test.
  O6 remains a draft policy; no emulator implementation source is consulted.

#### Memory map and unsupported hardware
- Route CPU reads/writes through one instance-owned decoder: Suzy FC00–FCFF,
  Mikey FD00–FDFF, ROM FE00–FFF7, vectors FFFA–FFFF. MAPCTL bits 0–3 select
  underlying RAM when set. FFF8 is always RAM; FFF9 is always MAPCTL.
- Preserve raw RAM inspection and add a separate side-effect-free CPU-mapped
  inspection path. Validate a whole request before writing its output buffer.
- Cover all 16 low-nibble mappings and every window boundary. Store bit 7 but
  retain explicitly PLACEHOLDER flat5 timing. Source-backed page-mode timing
  and device side effects arrive with their device phases.
- Supply no ROM bytes and no firmware input API. Access to mapped absent ROM
  must be explicitly unavailable, never silently backing RAM or fabricated
  BIOS. Prefer an additive unavailable stop/status with a named control.
- Unimplemented device-register behavior, MAPCTL guest readback and hidden-RAM
  write-through are not established by the sources reviewed. Keep any necessary
  deterministic provisional policy explicit and unverified, separate from the
  source-backed decoder. Expose the MAPCTL latch for host observation without
  claiming it is hardware-accurate guest readback. Do not build a fake full
  device model solely to make the diagnostic pass.

#### Direct-load contract and fixture
- Use the canonical BS93/BLL subset measured with the pinned cc65: 10-byte
  header, `80 08`, big-endian load address, big-endian total file length
  INCLUDING the header, and `BS93`; nonempty payload; exact declared length;
  no address wrap. Entry is the payload load address. Other formats/markers
  are refused rather than detected heuristically.
- Replace the provisional raw-image external startup path with this documented
  direct-load path. The planner may retain a clearly named internal test seam
  or a justified low-level RAM helper, but normal runner/consumer/diagnostic
  content loading must use the strict container API; do not leave two
  ambiguous public content formats or use raw loading as firmware support.
- Content identity is computed by the core from the FULL accepted object bytes;
  report the payload load address and length separately. This binds header and
  payload without trusting a host-supplied hash. Include the
  format/startup strategy and entry/load semantics in identity and snapshots.
  Check all refusals before mutation, including aliased instance buffers.
- Tier A sets PC directly and otherwise retains declared power-on values,
  including MAPCTL zero from the cited Epyx reset table. The diagnostic starts
  in low RAM and writes MAPCTL $0C itself to select RAM for ROM/vectors, just
  as the inspected cc65 CRT does. That write is guest initialization, not a
  host-injected post-boot state. No speculative DISPCTL or boot-state preset.
- Write original ca65 source and a project-owned ld65 config/header recipe.
  Rebuild with pinned tools, emit labels with `-Ln`, and verify identities and
  the measured format. Do not copy emulator code or rely on a boot loader/C
  runtime. Record source, recipe, hash, notices and allowed use under current
  project rights; do not choose a new licence.
- Guest results have magic, schema, progress, error and a nonzero done marker.
  Resolve the result address from the linked symbol file; require completion,
  semantic progress and bounded guest time. A deliberately broken diagnostic,
  no execution, corrupt result and timeout must fail their intended checks.

#### Observation, continuation and consumer
- Add a bounded host write-watch range with no callback or host code in the
  core. Stop after the matching guest write has completed, preserve CPU/bus
  continuation, and document exact tick/priority/rearm behavior. Resuming to
  the same final tick must yield the same guest state and output digests.
- Watch configuration/hit reporting is host observation, not guest machine
  state. Keep it out of guest-state digests and define reset/load/restore
  handling explicitly. Test actual CPU writes, boundaries and no-hit controls.
- Bump snapshot format for new guest fields, validate before apply, expose
  serialized state through public observations, and prove mid-diagnostic
  restore/continue equals uninterrupted execution (also across budget slices).
- Build a separate consumer linked only against the public header/API. Read
  original diagnostic result and packed framebuffer bytes from guest RAM;
  render a deterministic P3 PPM with metadata identifying it as a SNAPSHOT,
  guest tick, content/startup/profile and palette interpretation. This is
  HB-01 evidence, not display DMA, scanout, frame timing or a VID-04 freeze.
- Put a valid integer PCM WAV writer in that consumer; produce a short silence
  file labelled as placeholder audio until Phase 6. Use temporary outputs for
  binary WAV checks and maintain the existing artifact-rights policy.
- Compare runner and consumer public observations to independent expected
  diagnostic values; sharing the core proves repeatability, not accuracy.

#### Verification and delivery
- Extend the existing exact inventory and named-fault controls. Every inventory
  change refreshes the existing claim's inventory hash in the same commit.
- Use targeted checks during implementation and the full `scripts/check.sh`
  at integration. Follow the existing Linux-container-before-push practice and
  inspect CI for the exact tested head plus critical-path efficiency.
- Keep implementation, goal verification, code review/fixes, security and
  validation coverage within this phase's existing approval. No extra
  acknowledgments for ordinary builds, checks, commits or bounded retests.

### the agent's Discretion
- Concrete API names, deterministic stub policy and explicit unsupported
  status details, watch API/event shape, snapshot field layout, diagnostic
  result schema/pixel pattern and test decomposition are reversible choices.
  Preserve existing refusal, ownership and half-open budget guarantees.
- Keep an adversarial conflict pass over all three research notes before
  finalizing plans. Unknown hardware behavior must remain visible.

### Deferred Ideas (OUT OF SCOPE)
- Real timers/IRQ and bus arbitration (Phase 4); page-mode calibration,
  timer-generated display/DMA/raw-video form and logical input (Phase 5);
  actual audio (Phase 6). No raw-video compatibility decision for O9 here.
- MAPCTL model-specific readback and electrical write-through qualification
  need additional source evidence; provisional behavior is never a claim.
- BIOS/HLE, cartridge boot, licences, releases and hardware tests remain
  owner-gated. Long-path vector diagnostics and history-scan growth remain
  carried follow-ups unless measured phase verification justifies a small fix.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research support |
|---|---|---|
| MEM-01..04 | MAPCTL CPU mapping and distinct raw RAM; direct-load tier A; manifest/claim startup tag; no firmware API | Phase 3 CONTEXT locks the map, strict BS93 container, tier-A state, and absent-ROM behavior. Keep unsupported device details visibly provisional. |
| EVD-01 | Original diagnostic reports symbol-addressed RAM progress and pass/fail | Use project-owned assembly, linked result symbol, versioned result block, semantic progress and named no-run/corruption/timeout/fault controls. |
| API-08 | Memory watch stops on writes without changing machine behavior | Stop after completing the matching write; test exact range boundaries, rearm/no-hit behavior, and digest equality at a common final guest tick. |
| DET-03 | Mid-run snapshot restore and continuation equal uninterrupted run | Snapshot the diagnostic between progress and done; continue both paths to equal guest time and compare state and output digests. |
| HB-01 | Labelled viewable diagnostic snapshot | Consumer writes tick-stamped P3 from a guest-RAM framebuffer buffer, explicitly labelled SNAPSHOT and with palette interpretation; this is not video scanout. |
| CON-01 | Independent public API consumer writes image and WAV | Separate consumer links only public API; writes P3 and a short valid silence WAV. Keep binary WAV under a temporary directory. |
</phase_requirements>

## Summary

**Primary recommendation:** Use a tiny original ca65 diagnostic and a project-authored ld65 header/config that emits only the measured pinned-tool BS93/BLL subset. Replace the provisional public raw-image path with explicit strict container loading, keep the diagnostic's result and packed pixel buffer in guest RAM, and use a separate public-API consumer for assertions, P3 output and a valid silence WAV. `[CITED: 03-CONTEXT.md:51-100; VERIFIED: include/lynxplosion.h:141-150 — “PROVISIONAL raw RAM image loader, replaced in Phase 3.”]`

The phase context settles the previously open wire-format ambiguity: on-disk total length includes the 10-byte header. The pinned cc65 source writes the `80 08` marker, big-endian load and total-length fields, then `BS93`; the startup research measured both a 329-byte linked output and a 13-byte minimal-body output. Use those measurements as format evidence only. Do not link cc65 CRT/TGI/runtime code into the fixture; author the 10-byte header segment in-project and record the cc65 source consulted in the exposure record. `[VERIFIED: build/pins/cc65/libsrc/lynx/bllhdr.s:13-18; 03-STARTUP-RESEARCH.md, “Pinned cc65 output measurement”; CITED: https://cc65.github.io/doc/lynx.html, https://cc65.github.io/doc/ld65.html]`

Keep three observations named distinctly: serialized machine-state snapshot, raw guest RAM snapshot image, and eventual hardware video frame. The phase's viewable artifact is a rendering of diagnostic-owned bytes from guest RAM, not a claim about DMA, scanout, palette hardware or frame timing. Audio output is a valid host-written WAV of silence until the core audio phase. `[VERIFIED: 03-CONTEXT.md:90-100; .planning/ROADMAP.md:162]`

## Recommended Design and Evidence

### Original fixture and direct-load identity

- Build a small, terminating, project-authored 65SC02 source with pinned `ca65` and `ld65`. Have its body set up only what the diagnostic needs; MAPCTL starts at the context's reset value and the guest itself writes `$0C` to expose RAM at the ROM/vector windows. Do not add a speculative DISPCTL value. `[VERIFIED: 03-CONTEXT.md:67-75; CITED: https://cc65.github.io/doc/lynx.html]`
- Use the project's custom linker config and project-authored 10-byte segment to emit the strict BLL container. The pinned cc65 source expresses `.word $0880`, `.dbyt __MAIN_START__`, `.dbyt __BSS_LOAD__ - __MAIN_START__ + 10`, and `BS93`; this is format evidence, not code to copy. Emit an ld65 `-Ln` label file for `diag_result` and `diag_pixels`. Official ld65 documentation defines the custom memory/segment configuration and label output. `[VERIFIED: build/pins/cc65/libsrc/lynx/bllhdr.s:13-18 — exact expressions above; build/pins/cc65/cfg/lynx-bll.cfg:8-24; CITED: https://cc65.github.io/doc/ld65.html, https://cc65.github.io/doc/debugging.html]`
- The loader must accept only the documented marker/signature/length subset, reject empty/truncated/trailing/wrapping inputs, and validate before any mutation. Hash the entire accepted object bytes for identity, while separately reporting body load address and length. Keep a raw loader only as a clearly private test seam if required; normal runner, diagnostic and consumer use the explicit container API. `[VERIFIED: 03-CONTEXT.md:51-66; VERIFIED: include/lynxplosion.h:141-150]`
- Reuse the current deterministic fixture gate shape: assemble in `build/toolchain` under `env -i`, remove temporary object/source/config copies, compare the resulting object byte-for-byte with committed text hex, and record its exact hash and recipe in a fixture manifest. The manifest validator requires the exact keys `source revision recipe sha256 licence notices allowed_use startup_tier profile evidence_tier` and allows `startup_tier` A or none. `[VERIFIED: scripts/check.sh:307-340; tests/cmake/cpu_fixtures.cmake:1-7,18-74; scripts/check-manifests.sh:6-17,67-73 — quote: “source revision recipe sha256 licence notices allowed_use startup_tier profile evidence_tier”]`

### Result block, controls and assertions

Use a project-defined, versioned block at one linked symbol. A small layout is sufficient: nonzero magic, schema, monotonic progress, error code, and an exact nonzero done marker. Write the done marker last as the commit point. Exact widths/byte order/error values remain implementation discretion; document them in one header/fixture note and parse them with range-checked code. `[VERIFIED: .planning/REQUIREMENTS.md:94; 03-CONTEXT.md:77-80]`

PASS only when the block has the expected magic/schema, expected completed progress, zero error, exact done marker, and guest time between independently selected lower and upper bounds. Assert selected RAM pixels against independent constants as well. Require the all-zero initial block to fail. Add distinct controls for no advance/zero budget, a fixture that loops before done (guest-tick timeout), a mutated result field, and a diagnostic-side fault path that sets error then completes. The broken-diagnostic control must make guest code report its defined FAIL condition, not have the host prefill an error byte. `[REC: satisfies .planning/ROADMAP.md:158 and .planning/REQUIREMENTS.md:94-98; exact thresholds/schema values are phase implementation choices]`

The result and pixel buffers are inspected through raw `lynx_read_memory`, which the current public header says is raw 64 KiB RAM, not a CPU-mapped view. The consumer gets addresses from the fixture label output; parse exact expected symbol names with bounded line/number lengths and reject missing or duplicate symbols. Keep mapped peeks separate from result inspection. `[VERIFIED: include/lynxplosion.h:183-187 — “lynx_read_memory reads the raw 64 KiB RAM, not a CPU-mapped view”; CITED: https://cc65.github.io/doc/ld65.html]`

### Watch and snapshot continuation

Stop the watch after the guest store has completed, as locked in Phase 3 CONTEXT. Capture a mid-diagnostic snapshot after nonzero progress and before done; restore into a fresh matching instance and continue. Exercise the same workload with different advance budget slices. Compare complete guest state and canonical observation outputs at a common final guest tick. `[VERIFIED: 03-CONTEXT.md:82-92; .planning/REQUIREMENTS.md:46,52]`

The current bundle observation includes raw RAM, public registers, guest ticks, ticks consumed and stop reason. Therefore watched/unwatched runs must reach the same final tick and be finalized with the same stop reason before comparing the existing bundle digest; otherwise the digest can differ for host-run metadata even when guest state is identical. Ensure the watch hit itself is present in one test and absent in its control, while watch configuration/hit metadata stays outside guest-state digests. `[VERIFIED: runner/bundle.c:69-95; 03-CONTEXT.md:83-89]`

Phase 2 explicitly anticipated the snapshot change: “Phase 3 adds startup tier and container identity to the snapshot header: expect format 3, by the same bump rule.” Store MAPCTL and any newly introduced guest-visible latch/state, validate all identity and payload fields before apply, and include public observations for serialized fields. `[VERIFIED: .planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/04-api-snapshot-determinism.md:680-694 — quote above; core/internal.h:13-24 — current format/payload definitions]`

### Public consumer, image and WAV

Build a separate C target with only `include/lynxplosion.h` and the public core library in its dependencies. It loads the same strict BLL file, advances within a guest-tick bound, verifies result and pixel bytes, queries public profile/time/register/raw-memory APIs, then writes P3 PPM with a comment/header naming `SNAPSHOT`, guest tick, content identity, startup tier, profile and the chosen host palette interpretation. The PPM is a test view of the guest's diagnostic buffer, not raw hardware pen/palette output or a completed frame. Do not depend on `runner/bundle` or `core/internal.h`. `[VERIFIED: 03-CONTEXT.md:93-102; include/lynxplosion.h:183-190; .planning/REQUIREMENTS.md:103-106]`

Include a small integer PCM WAV writer in this consumer; emit a short silence file labelled placeholder audio. Validate its RIFF/WAVE format, sizes and zero sample payload, but place generated WAV under `${TMPDIR:-/tmp}` and clean it up. Keep the viewable ASCII PPM in build output. This avoids adding an image/audio package or a binary-file allowlist while proving the writer path. `[VERIFIED: 03-CONTEXT.md:98-100; recommendation based on scripts/scan-binaries.sh:143-155,215-249]`

## Existing Stack and Integration

| Concern | Use | Evidence |
|---|---|---|
| Assembly and link | Existing pinned cc65 `ca65`/`ld65`, `--cpu 65SC02`, project-owned linker config/header | `scripts/check.sh:307-340`; official [cc65 Lynx docs](https://cc65.github.io/doc/lynx.html), [ld65 docs](https://cc65.github.io/doc/ld65.html) |
| Core/consumer | Existing C17 public C ABI, CMake/Ninja and CTest; no new package | `.planning/AUTONOMY.md:20-24` — exact default: “C17, CMake ≥3.25 workflow presets, Ninja, CTest, C ABI with an opaque handle.” |
| Fixture rights | Existing required manifest schema and SHA-keyed binary scan; project-authored source/config/hex only | `scripts/check-manifests.sh:6-17`; `scripts/scan-binaries.sh:143-155,215-249` |
| Fast check | Existing `scripts/check.sh`, exact `tests/inventory/fast.txt`, current CI gate | `.planning/REQUIREMENTS.md:27-35`; `cmake/Tests.cmake:14-33`; `tests/tests.cmake:1-12` |

Manifest the executable fixture identity using the SHA-256 of the exact complete BS93 object bytes. List its `.s`, `.cfg` and text `.hex` under `files`; record project authorship, pinned tool recipe, notices, allowed test/developer use, startup A, profile none, and repeatability evidence. Keep the interim rights field consistent with project policy (“all rights reserved” pending O5); this does not select a project license or imply permission for third-party reuse. `[PROJECT CONSTRAINT: user-provided AGENTS.md; O5 remains open per .planning/REQUIREMENTS.md:166-168]`

Run the assembly comparison and new diagnostic/consumer tests through the existing check workflow, register every test and named-fault control in the exact sorted fast inventory, and refresh the claim inventory hash in the same change. Do not add a new CI lane or cache. Existing CI is already measured at 23–29 seconds; inspect the actual Phase 3 critical path before any optimization. A carried Phase 2 path-length failure occurs around 170 characters, so keep new test names/messages and nested paths short and use `LYNX_SCRATCH_DIR`; do not refactor history scans for this small fixture without measured need. `[VERIFIED: .planning/LESSONS.md:40-43,71-84; 03-CONTEXT.md:104-109; cmake/Tests.cmake:14-19]`

## Adversarial Review

| Failure mode | Evidence needed to catch it |
|---|---|
| Empty/default RAM looks like PASS | Require nonzero magic and done, expected progress/error, guest-time lower bound, and independent expected pixels; prove no-execution control fails. |
| Loader accepts a prefix, trailing bytes, wrap, wrong marker or cartridge image | Valid object plus one negative per header/length/range class; assert whole machine bytes unchanged after every refusal. |
| CRT/loader behavior is accidentally treated as tier-A preset | Verify guest starts at the object entry with only declared power-on state; guest code itself writes MAPCTL `$0C`; no firmware argument/path exists. |
| Harness and diagnostic share a mistaken answer | Derive expected result and selected pixels independently; use explicit fixture trace/constants rather than reading its own output as oracle. |
| Watch changes instruction continuation or digest | Stop after completed matching store, prove state remains resumable, test exact range boundaries and compare same-tick final observations against no-watch run. |
| Snapshot omits MAPCTL/startup/container identity or accepts corrupt state | Round-trip with nondefault MAPCTL and mid-diagnostic progress; corrupt payload/hash and mismatch profile/content/container; every refusal preserves full machine state. |
| Guest RAM PPM is described as Lynx video | Label it SNAPSHOT with guest tick and palette interpretation; explicitly state no DMA, scanout, frame timing or hardware palette claim. |
| BS93 fixture gets a path exemption or becomes untracked binary | Keep source/config/hex text manifest-covered and hash exact object; ensure build artifact scan passes without adding path/extension exemptions. |
| New CI work costs time but catches no distinct issue | Reuse the one fast gate and measure its critical path; retain only distinct format, no-run, corruption, timeout, watch and continuation controls. |

## Source Exposure and Rights

This research opened no GPL emulator implementation source and used no firmware/game bytes or physical hardware. It did open pinned cc65's `libsrc/lynx/bllhdr.s` to identify the BLL header expressions; cc65 is a distinct zlib-licensed toolchain. Add this source read to `docs/policy/exposure-record.md` with its purpose (format identification only), and record the official cc65 documentation pages consulted. Do not copy `bllhdr.s` into the project; implement the format in project-authored assembly/config. `[VERIFIED: docs/policy/exposure-record.md:11-34 currently lists sources through Phase 2; build/pins/cc65/libsrc/lynx/bllhdr.s:1-18; CITED: https://cc65.github.io/doc/lynx.html, https://cc65.github.io/doc/ld65.html]`

The project source, diagnostic, consumer and fixture recipe remain project-owned and all rights reserved under O5 pending; the fixture manifest records use for this project's tests/developer preview only. A hash establishes identity, not rights. No public artifact or reuse license is created by this phase. `[PROJECT CONSTRAINT: user-provided AGENTS.md; .planning/AUTONOMY.md:6-12; .planning/REQUIREMENTS.md:166-168]`

## Open Questions to Preserve

1. **Unimplemented device-register values and hidden-RAM write-through:** sources reviewed do not settle these; choose explicit deterministic provisional policy and mark unverified. Do not add a fake device model for diagnostic convenience. `[VERIFIED: 03-CONTEXT.md:44-49]`
2. **MAPCTL guest readback/model differences:** expose its latch to the host without claiming hardware readback accuracy; keep profile `none` and do not assert profile-specific behavior while O1 is open. The public header's exact enum is `enum { LYNX_PROFILE_NONE = 0, LYNX_PROFILE_LYNX1 = 1, LYNX_PROFILE_LYNX2 = 2 };` `[VERIFIED: include/lynxplosion.h:52-58 — quote above; 03-CONTEXT.md:44-49]`
3. **Absent mapped ROM result:** follow context with an explicit unavailable route and named control; do not synthesize ROM or fall back to RAM. `[VERIFIED: 03-CONTEXT.md:41-43]`
4. **Result-block widths and exact error constants:** choose concise, versioned fields and publish one schema; keep exact values a reversible implementation choice. `[VERIFIED: 03-CONTEXT.md:77-80,114-118]`
5. **PPM pixel encoding/palette interpretation:** use an explicit diagnostic-owned encoding and deterministic host palette; do not describe it as canonical Lynx video. `[VERIFIED: 03-CONTEXT.md:93-97]`

## Sources

### Project sources opened
- `.planning/phases/03-memory-map-direct-load-startup-and-first-diagnostic/03-CONTEXT.md`
- `.planning/ROADMAP.md:152-165`; `.planning/REQUIREMENTS.md:27-35,46,52,64-70,92-106`
- `.planning/phases/02-cpu-profiles-and-per-opcode-vectors/discuss/04-api-snapshot-determinism.md:680-694`
- `include/lynxplosion.h:52-58,141-150,183-190,220-249`
- `build/pins/cc65/libsrc/lynx/bllhdr.s:1-18`; `build/pins/cc65/cfg/lynx-bll.cfg:8-24`
- `scripts/check.sh:307-340`; `tests/cmake/cpu_fixtures.cmake:1-74`; `scripts/check-manifests.sh:6-17,67-73`; `scripts/scan-binaries.sh:143-155,215-249`
- `runner/bundle.c:69-95,198-214`; `cmake/Tests.cmake:14-33`; `.planning/LESSONS.md:40-43,71-84`

### Official cc65 documentation
- [Atari Lynx target documentation](https://cc65.github.io/doc/lynx.html) — distinction between Lynx cart output, BLL output, startup and linker configuration.
- [ld65 documentation](https://cc65.github.io/doc/ld65.html) and [cc65 debugging guide](https://cc65.github.io/doc/debugging.html) — linker configuration and `-Ln` labels.

### Coordinated measurement
- `03-STARTUP-RESEARCH.md`, pinned cc65 output probe and minimal-body spike; no spike artifact is treated as a project fixture. The measured total field includes the 10-byte header. `[VERIFIED: 03-STARTUP-RESEARCH.md, “Pinned cc65 output measurement”]`

## Confidence and Remaining Risk

- **High:** Phase 3 scope and startup/watch/snapshot/consumer constraints; existing API semantics; current gate/manifest shape; pinned BS93 byte layout and measured total-length interpretation.
- **Medium:** Minimal assembly/config organization, strict bounded label parser, result-block layout, consumer output and exact named-fault decomposition.
- **Open by design:** MAPCTL readback, device stubs, absent-ROM stop API details, hidden-RAM write-through and hardware accuracy of any unsourced preset. Preserve context's explicit unknowns; no owner decision is required for routine implementation choices already delegated in `03-CONTEXT.md`.
