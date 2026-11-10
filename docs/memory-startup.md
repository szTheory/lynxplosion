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

The memory map sources identify MAPCTL at `0xFFF9` and describe its overlay
bits; the exact device behavior is handled in later Phase 3 work. The current
core starts MAPCTL at zero on init, reset and successful load as a deterministic
project policy. That is not a claim about hardware reset state. CPU reset
register values likewise remain unverified while the Lynx I/II and CPU baseline
decision O1 is open. The current flat five-master-tick timing is
`PLACEHOLDER`, not a hardware timing claim.

No baseline or suite-pass compatibility claim is made. O5 (project licence),
O6 (clean-room policy) and the firmware/BIOS rights stop remain open. Public
distribution of copyrighted firmware or game content, physical hardware tests,
licence selection and release activity require their separately authorized
decisions.

Primary map reference: [Epyx Handy hardware addresses](https://www.monlynx.de/lynx/hardware.html).
