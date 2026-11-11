# Memory and direct-load startup

This project currently accepts the measured BS93/BLL object subset for direct
RAM startup. It does not implement a firmware or cartridge interface.

## Container and ownership

The accepted object starts with the ten-byte header used by the project-built
fixture: `80 08`, a big-endian load address, a big-endian total object length,
and the four ASCII bytes `BS93`. The total length includes the header, so the
payload is `object[10..length)` and must contain at least one byte. The core
requires the supplied buffer length to equal the header length, caps the object
at 65535 bytes, and checks that the payload ends at or before address `0x10000`.
Unknown headers, unsupported object shapes, trailing bytes, truncation, range
overflow and any overlap with the whole machine instance are refused before
the instance changes.

On success, the core copies payload bytes into its own RAM, starts the CPU at
the load address, and hashes the complete object, including the header. Public
identity keeps the object format, startup tier, entry address, total object
length, payload address and payload length distinct. The caller may reuse or
free its source bytes immediately. SHA-256 identifies these bytes; it does not
establish ownership, authenticity or permission.

The behavior is covered by the project-owned API tests and by the original
fixture recipe in `03-STARTUP-RESEARCH.md`. Format references include the
[cc65 Lynx documentation](https://cc65.github.io/doc/lynx.html) and
[ld65 documentation](https://cc65.github.io/doc/ld65.html).

## Startup policy and evidence limits

`startup_tier = A` means direct load into RAM. It is not power-on or firmware
boot evidence. No BIOS bytes, commercial game bytes or external emulator
implementation are part of this path.

The sourced CPU decoder has four independent windows: Suzy at `0xFC00–0xFCFF`,
Mikey at `0xFD00–0xFDFF`, ROM at `0xFE00–0xFFF7`, and vectors at
`0xFFFA–0xFFFF`. MAPCTL bits 0–3 select underlying RAM for those windows;
`0xFFF8` remains RAM and `0xFFF9` remains the MAPCTL write address. The full
byte is retained, while only its low nibble affects routing. Raw RAM reads and
CPU-mapped peeks remain separate observations.

The decoder is source-backed; unsupported target behavior is a separate
provisional policy. This build has no ROM or device contents, so mapped Suzy,
Mikey, absent-ROM, vector and MAPCTL-read CPU requests stop as unavailable
before CPU completion and do not fall through to hidden RAM. Only guest writes
to the MAPCTL latch are implemented. Host observation can read the latch, but
that does not establish guest MAPCTL readback, device register values, or
hidden-RAM write-through on hardware. Named controls exercise these refusals
and RAM-selected counterparts through real CPU requests.

The current core starts MAPCTL at zero on init, reset and successful load as a
deterministic project policy. That is not a claim about hardware reset state.
CPU reset register values likewise remain unverified while the Lynx I/II and
CPU baseline decision O1 is open. The current flat five-master-tick timing is
`PLACEHOLDER`, not a hardware timing claim.

## Snapshot continuation and write-watch observations

Phase 3 introduced snapshot format 3 with expanded startup and content
identity. Phase 4 format 4 adds canonical timer/IRQ state, ownership hold and
the current-tick scheduler cursor; earlier snapshot formats are refused.
Format 4 records timer registers, the pending IRQ byte, timer phase policy,
the current-tick event cursor, ownership hold, and CPU readiness as a bounded
priced-cycle residual. Readiness does not grow while a request waits for bus
ownership; timer events continue during a private hold. Timer pending state is
independent of the CPU interrupt mask, while the mask still controls IRQ
acceptance at CPU polls. The shared timer phase starts at fixed zero on
init/reset/load, and timer-bank events precede a same-tick CPU bus action and
its poll. These are deterministic software rules; the exact hardware phase
and tie behavior remain unqualified.

The timer registers `$FD00`–`$FD1F` and pending/acknowledge aliases `$FD80` and
`$FD81` are implemented. `$FD91` remains unavailable: a guest request stays
pending and returns the unavailable stop without changing hidden backing RAM.
The private ownership hold does not implement Suzy bus ownership, CPUSLEEP,
WAI, or STP. See [timer and interrupt behavior](timers-interrupts.md) for the
modeled register details and [suite evidence](public-suites.md) for the
separate observation and source-comparison status.

Snapshot continuation tests restore a fresh same-identity machine and compare
its final guest observations with uninterrupted execution; this is
repeatability evidence, not a hardware-accuracy result.

Snapshots contain guest machine state only. Watch configuration and hit history
belong to the host observer and are absent from both snapshot bytes and the
runner's guest observation digest. A successful restore clears the host watch
configuration and event after all guest-state validation and application have
succeeded, so the host must explicitly rearm it. A refused restore preserves the
whole instance, including any active watch and prior hit. Snapshot bytes at the
same guest state therefore remain identical with a watch configured, after a
watch hit, and with no watch.

A watch matches actual CPU writes in its bounded half-open range. The current
contract records the bus grant tick and returns `LYNX_STOP_WATCH` at grant+1,
after the write, CPU request completion and next-ready tick are committed.
Resuming execution does not repeat that store. Reads do not match. Operations on
one instance require caller serialization; independent instances own separate
watch state.

For segmented, watched and restored paths, compare guest observations at the
same final guest tick. Normalize the bundle run metadata to cumulative requested
budget, total guest ticks and the final stop reason; intermediate host `WATCH`
returns are not part of the guest result. No guest-digest or golden rebaseline
was needed for this change: host-only watch state did not alter public guest
observations or normalized digest inputs. The permanent API and determinism
controls compare serialized state, RAM, registers, identity, time and digests;
the golden digest remains unchanged. These checks establish same-build
repeatability, not hardware accuracy. DET-03's remaining unclassified probe is
still a flagged assumption; these tests do not mark it automatically classified.

## Public diagnostic consumer and runnable demo

`lynx-consume` is a separate C program linked through `include/lynxplosion.h`.
It loads the complete BLL object, resolves `diag_result` and `diag_pixels` from
the matching `ld65 -Ln` file, runs the diagnostic and reads its guest RAM
through public API calls. It validates the completion block and exact packed
pixels before creating output.

The consumer writes three files: `snapshot.ppm`, `silence.wav`, and
`snapshot.manifest`. The P3 image is an unscaled 8x8 `SNAPSHOT` of
diagnostic-owned pixel bytes using `diagnostic-palette-v1`; its metadata
records the guest tick, startup tier, selected profile and complete BLL SHA.
It is not display DMA, scanout, a canonical Lynx palette or a video frame. The
WAV is mono 8000 Hz signed 16-bit little-endian PCM with exactly 800 zero
samples; it is labelled placeholder silence pending Phase 6 audio.

After input, symbols, result, pixels and metadata pass validation, the consumer
writes each file into a private sibling staging directory and checks each
write and close. It stages the manifest last, then publishes the whole
directory with an exclusive no-replace rename (`renamex_np(RENAME_EXCL)` on
macOS or `renameat2(RENAME_NOREPLACE)` on Linux). Existing directories, files
and symlinks are refused without modification. If a process is interrupted
before publication, a private staging directory may remain. Before the manifest
is staged it contains only the closed PPM and WAV; after the manifest is closed
it may contain the complete validated artifact set. Until the whole-directory
exclusive rename succeeds, the requested output directory remains absent and
the private stage is not a published success. Unsupported exclusive rename
fails closed.

`scripts/check.sh` runs CTest, whose diagnostic test drivers remove their
isolated scratch directories and generated BLL/label inputs during cleanup.
This recipe builds a fresh project-owned object and matching symbols in a
unique temporary directory, so it works after the check has cleaned its test
fixtures. Keep the generated files temporary and do not commit them.

```sh
sh scripts/check.sh
demo_dir=$(mktemp -d "${TMPDIR:-/tmp}/lynxplosion-demo.XXXXXX")
cc65_home="$PWD/build/pins/cc65"
env -i PATH="$cc65_home/bin:/usr/bin:/bin" CC65_HOME="$cc65_home" \
  "$cc65_home/bin/ca65" --cpu 65SC02 \
  -o "$demo_dir/first.o" fixtures/diagnostic/first.s
env -i PATH="$cc65_home/bin:/usr/bin:/bin" CC65_HOME="$cc65_home" \
  "$cc65_home/bin/ld65" -C fixtures/diagnostic/bll.cfg \
  -Ln "$demo_dir/first.lbl" -o "$demo_dir/first.bll" "$demo_dir/first.o"
build/check/consumer/lynx-consume \
  --content "$demo_dir/first.bll" --symbols "$demo_dir/first.lbl" \
  --out "$demo_dir/output" --profile none
build/check/tests/lynx-check-artifacts "$demo_dir/output"
```

The output directory is intentionally fresh and the command refuses to replace
it. Inspect the PPM and manifest in `"$demo_dir/output"`; remove only this
generated directory when finished with `rm -rf "$demo_dir"`.

No baseline or suite-pass compatibility claim is made. O5 (project licence),
O6 (clean-room policy) and the firmware/BIOS rights stop remain open. Public
distribution of copyrighted firmware or game content, physical hardware tests,
licence selection and release activity require their separately authorized
decisions.

Primary map reference: [Epyx Handy hardware addresses](https://www.monlynx.de/lynx/hardware.html).
