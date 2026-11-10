> DRAFT pending owner decision O6

# Clean-room policy (FND-07, draft)

This is a draft. Owner decision O6 (whether this rule binds implementers and how
research exposure is recorded) is open, and so is the project licence (O5). Until
O6 is decided this file states the working rule that the evidence harness assumes;
it does not decide O6 and it is not a licence statement.

- **GPL emulator source stays closed.** Implementers do not open the source of
  Gearlynx (GPL-3.0), MAME (GPL-2.0+), Holani (GPL-3.0) or Beetle (GPL-2.0), and do
  not paste any of it into an assistant's context. Their documentation, release
  notes and issue trackers may be read. Their released binaries may be run as
  black-box behaviour cross-checks by a person other than the implementer, who
  reports behaviour, never code.
- **Allowed sources.** Epyx hardware documents, the lynx-tests suite as test source
  (MIT), cc65 sources (zlib), project-authored tests and diagnostics, and
  permissively licensed documentation. A permissively licensed helper from the
  owner's own projects enters only with a notice in `docs/provenance.md`.
- **Research exposure is recorded per source** in `docs/policy/exposure-record.md`,
  built from the research files and never from memory. The research phase read some
  emulator source; that exposure is listed there, not hidden.
- **Accidental exposure is recorded the same day.** Add a row to the exposure record
  saying what was read, when, by whom and in which file or page, and tell the owner.
  An exposure is never silently dropped, and the affected work is re-derived from an
  allowed source or removed.
- **Another emulator's constants are never imported as expected values.** A register
  preset, timing figure or table seen in an emulator is not evidence about hardware.
  Each value cites an Epyx page or a hardware test, or stays unset.
- **Firmware and game bytes never enter the repository** or its artifacts, whatever
  licence a carrying file claims.
- **The rights scan is a safety net, not a proof.** `scripts/scan-binaries.sh` finds
  a non-text file of exactly 512 bytes, an unmanifested binary, a text file that is
  hex digit pairs decoding to exactly 512 bytes (the syntax `lynx-run --content`
  reads) and a run of 200 or more base64 characters on one line, unless the file's
  SHA-256 is a fixture manifest value. It does not see an image split across many
  short base64 lines, a C array of byte values, any other encoding, or content
  inside a compressed archive. Those stay a review responsibility: a file whose
  provenance is not recorded in a manifest is not added.

Status: FND-07 stays a draft until O6 is decided. The check `docs.policy` keeps the
banner above and the per-source rows in place while the draft stands.
