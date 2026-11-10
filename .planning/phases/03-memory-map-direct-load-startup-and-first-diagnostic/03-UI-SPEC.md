---
phase: "3"
slug: "memory-map-direct-load-startup-and-first-diagnostic"
status: approved
shadcn_initialized: false
preset: none
created: "2026-11-10"
---

# Phase 3 — UI Design Contract

> Scope: no website, native screen, or interactive UI is delivered. “CPU-mapped view” names a memory-inspection API. User-visible outputs are CLI text and generated diagnostic artifacts.

## Design System

| Property | Value |
|----------|-------|
| Tool | none |
| Preset / component library / icons | not applicable |
| Font | not applicable |

## Spacing Scale

Not applicable: no screen layout or touch targets.

## Typography

Not applicable: no UI typography system. CLI output uses plain, readable text.

## Color

Not applicable: no application UI color system or 60/30/10 surface split. PPM colors represent the guest diagnostic's packed pixels using the explicitly identified palette interpretation; do not invent a UI palette.

## Copywriting Contract

| Element | Contract |
|---------|----------|
| Primary CTA / empty state / destructive confirmation | Not applicable; no interactive UI. |
| CLI error | State the refused input or failed check and the next corrective action. Refusal exits unsuccessfully and publishes no PPM, WAV, or success metadata. |
| CLI success | Identify the produced diagnostic snapshot and mark audio as placeholder silence. |

## Artifact and Interaction Contract

- Emit a viewable P3 PPM from the diagnostic's guest-RAM pixel bytes. Preserve the original pattern and dimensions; do not scale, interpolate, or substitute host-rendered pixels.
- The PPM comment and companion metadata must call the image `SNAPSHOT`, never a frame, scanout, or video frame. Include guest tick, `startup=A`, selected CPU profile, full accepted-content identity, and palette interpretation.
- Emit a structurally valid integer PCM WAV containing short silence. Companion metadata and CLI output label it placeholder audio pending Phase 6; do not imply emulated sound.
- Resolve required result and pixel addresses from the generated diagnostic symbol file. Reject malformed, missing, duplicate, or out-of-range required symbols before guest-memory reads; do not silently substitute hard-coded addresses.
- Validate inputs, symbols, diagnostic completion, and result status before finalizing success artifacts. Any refusal or failed diagnostic leaves no success artifacts or success metadata.

## UI Considerations

The deterministic probe was run for CLI/static-content and artifact/media surfaces.
No interactive widget exists; these resolved truths apply to artifact generation.

- **empty — resolved, explicit:** Missing input, required symbols, completion or pixel data cause unsuccessful refusal and publish no success artifacts.
- **loading — resolved, explicit:** Artifacts are finalized only after validation and guest completion; no partial artifact is presented as success.
- **error — resolved, explicit:** Refusal identifies the failed check and exits unsuccessfully without success artifacts.
- **populated — resolved, explicit:** Successful output is an exact unscaled diagnostic P3 SNAPSHOT with identity/tick/startup/profile/palette metadata and a valid labelled silence WAV.
- **overflow — resolved, explicit:** Generated image sample counts and WAV lengths are checked against their declared bounds; invalid sizes are refused before success publication.
- **long-text — resolved, explicit:** Metadata uses validated bounded fields; CLI errors must remain complete and must not silently truncate into a success message.

## Registry Safety

Not applicable: no component library or registry is used.

## Checker Sign-Off

- Dimensions 1–7: not applicable to this non-UI phase; artifact/CLI contracts above are the visual-output scope. Approval: independent checker approved; no interactive UI dimensions apply
