# Timer evidence preparation

This package prepares a small timer study. Physical execution is **not approved**.
The original strict timer comparison remains unresolved; this diagnostic does
not qualify a Lynx model or close the milestone. Available console and loader
are unknown.

## Build and software checks

Use the project's existing pinned source and cc65 tools, prepared by the normal
check setup. No BIOS, commercial game, CRT, TGI or cartridge bootstrap is used.
The manifest is in `fixtures/manifests/timer-evidence.manifest`; the full MIT
notice is in `fixtures/timer-evidence/NOTICE`. Generated
images and captures stay in private scratch; they are not release artifacts.

From the repository root, with an existing canonical private temporary root:

```sh
TMPDIR="$PRIVATE_ROOT" sh scripts/build-timer-evidence.sh --out "$PRIVATE_ROOT/package"
TMPDIR="$PRIVATE_ROOT" sh scripts/build-timer-evidence.sh --verify "$PRIVATE_ROOT/package"
TMPDIR="$PRIVATE_ROOT" build/check/tests/lynx-test-timer-evidence \
  --export-software "$PRIVATE_ROOT/package" "$PRIVATE_ROOT/captures"
build/check/tests/lynx-test-timer-evidence \
  --validate-physical "$PRIVATE_ROOT/package/physical-results.tsv"
```

Choose fresh output and capture leaves. The last command must report missing
physical evidence (exit 3) for the prepared sheet. Exit 0 means twelve unique,
structurally complete records; it does **not** establish that they came from
hardware. A human still reviews photos, setup and provenance. A malformed record
returns 1; a missing file returns 2. The software checks are registered as `timer_evidence.tracer`,
`timer_evidence.matrix` and `timer_evidence.controls` in the normal local gate.

The builder verifies pinned inputs and tools, compares every byte of the
original 487-byte timer CODE with an independently built ordinary fixture,
checks the entry, memory ranges and display instructions, and binds all eight
complete images to `fixtures/timer-evidence/images.sha256`. Records contain
source, tool, linker, image, labels and map hashes. The case-set hash binds the
approved image list; tool receipts and map hashes remain separate evidence.
Output ownership markers permit cleanup only within the invocation's scratch.

## Cases and interpretation

| Hardware image | Display requested during measurement | MAPCTL requested |
| --- | --- | --- |
| hardware-dma0-page0 | 0C | 8C |
| hardware-dma0-page1 | 0C | 0C |
| hardware-dma1-page0 | 0D | 8C |
| hardware-dma1-page1 | 0D | 0C |

Each hardware image requests display 0D after measurement for readable results.
The unchanged emulator refuses these unsupported display bits before completion;
that refusal is required by the software check. Each separately named
`software-*` companion requests display 00/01 during measurement and restores
01. All four companions complete on both CPU profiles. Their PGM screenshots
and text receipts are software observations, never hardware photos.

The wrapper masks interrupts before branching, initializes the stack and timer
setup explicitly, reads the acknowledgment counter after clearing it, and
calls the unmodified timer CODE at its original addresses. Test6 starts at
03DC, polls at 0400 and uses zero-page counter 01. Rendering occurs after the
timed code. Framebuffer 6000..7FDF and BSS from 8000 do not overlap the guest
code or stack. Timer setup requests T0 backup 9E/count 00/control 18/00 and T2
backup 68/count 00/control 1F/00, display address 6000 and PBKUP 29.

This explicit startup differs from the historical CRT startup. Results from
these cases cannot be substituted for that suite's strict comparison. The
source counter expectation 13 is a reference, never injected into results or
used to accept these captures. Software companions currently show counter 13
with DMA off and 14 with DMA on; those values are observations of the current
model and this setup, not predictions for physical hardware.

The screen shows family, requested modes, `INIT00`, `DONEA5`, a guest CPU probe
(`CPUA1` or `CPUA2`), and eighteen raw result bytes. Row indices are hexadecimal:
**0E is counter result 14**. Preserve all bytes; 10 and 11 are upstream reserved
slots. Record the guest probe separately from the console's owner-reported
model; neither alone establishes a hardware revision. Missing or unreadable
completion is a failed/incomplete run. No count is silently normalized.

Requested register values are not readback evidence. Leave observed DISPCTL
and MAPCTL as `unknown` when a reliable readback is unavailable. Case receipts'
`restored_display` describes the software preflight, while
`requested_restore_dispctl` describes the image's instruction. Hardware values
remain unknown until an approved observation.

## Proposed human protocol, pending final approval

1. Identify one available Lynx model and an existing loader that is confirmed to
   accept these BS93 images. Record the loader/version and visible model label;
   record board revision only if already known without opening the console.
   If compatibility or equipment is unknown, keep the study stopped.
2. Review the private package identity and notices. Use only the four
   `hardware-*` images; software companions are controls. Confirm the transfer
   preserves the recorded image bytes. Preserve the original empty sheet.
3. After explicit approval, allow at most **15 minutes and twelve fresh loads**:
   three runs per hardware case on that one console. Use the existing loader's
   documented safe restart procedure. Stop at either bound, an unexpected
   fault, unreadable output, wrong case, missing completion or any uncertain
   setup. Do not automatically retry or enlarge the window.
4. Photograph each complete screen and record case, run, exact image/case-set
   hashes, setup, requested modes, any supported readback, model, loader,
   initial counter, all eighteen bytes, completion and photo reference in the
   twelve prepared slots. Keep photos private and avoid personal surroundings.
   Keep the guest CPU probe alongside the photo in a separate note.
5. Transcribe and validate offline. Retain every mismatch and incomplete run.
   Compare configuration effects without interpreting a matching counter as
   full bus accuracy. Further cycle traces or a changed experiment require a
   new decision; this protocol cannot close the historical strict gap alone.

No purchases, opening, soldering, wiring, powered cartridge swaps, firmware
updates/extraction, commercial content, public uploads or new distribution
channel are included. Offline transcription can continue after the physical
window without renewing it. A timeout is not permission for another run.

If no compatible equipment is available, leave physical results empty and seek
existing trace evidence or fuller primary documentation. Do not tune the
emulator to the source count merely to make the comparison pass.
