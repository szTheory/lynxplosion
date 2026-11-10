---
phase: "2"
slug: "cpu-profiles-and-per-opcode-vectors"
status: verified
threats_open: 0
asvs_level: 1
created: "2026-11-10"
---

# Phase 2 — Security

> Register authored at plan time (T-02-01..T-02-23 plus the per-plan supply-chain rows T-02-SC).
> Verified by gsd-security-auditor against HEAD `b35349c` (CI green, run 42770160893).

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Host to core (profile, budget, snapshot) | Profile id, tick budgets and snapshot blobs are validated before any state change | Untrusted ids, lengths, blobs |
| Vector data to test runner | Third-party JSON vectors parsed by a strict reader | Untrusted text, size-bounded |
| Network to vector sample (full lane only) | `scripts/vectors-full.sh` downloads 512 pinned files over https | Third-party MIT test data |
| Repository to public GitHub | Tracked files and history are world-readable | PII, credentials, rights-bearing bytes |

## Threat Register (all closed)

| Threat ID | Severity | Disposition | Mitigation (evidence) | Status |
|-----------|----------|-------------|-----------------------|--------|
| T-02-01 | medium | mitigate | `lynx__cpu_state_valid` (profile, P bits, latch bits, step per mode); refused/accepted states tested | closed |
| T-02-02 | medium | mitigate | `core.cpu_includes` (+ self-test), `core.nm_globals`, `core.nm_undefined`, `core.no_float` | closed |
| T-02-03 | low | mitigate | flat test bus cycle bound sets `overrun` instead of looping | closed |
| T-02-04 | medium | mitigate | `validate-kv.sh` tiers default `A`; run manifest with `none` refused (`det.manifest_fields.self_test`) | closed |
| T-02-05 | low | mitigate | exposure record lists the prose source read; emulator source links not followed | closed |
| T-02-06 | high | mitigate | `vectors-full.sh`: commit-pinned base, https-only with proto-redir, size cap, hash verified before any read | closed |
| T-02-07 | medium | mitigate | generated file names, no archive extraction, mktemp work dir outside the repo removed on EXIT | closed |
| T-02-08 | medium | mitigate | verbatim MIT LICENSE (sha matches pin), `-text` attributes, manifests with licence/notices/revision/sha256, provenance entry | closed |
| T-02-09 | high | mitigate | reader bounds (64 ram, 16 cycles), number-range and name checks, exit 3; `cpu.vectors.malformed`, `.reader_malformed` | closed |
| T-02-10 | medium | mitigate | sample hash vs manifest before parse; plan derived and compared byte for byte; closed reason vocabulary; no pass counts in inventories | closed |
| T-02-11 | high | mitigate | address-hashed poison; `cpu.vectors.wrong_profile(_reverse)`, `.no_step`, `.mutant_profile` | closed |
| T-02-12 | high | mitigate | full lane: pin record check, per-file hash before use, rebuilt samples `cmp`ed to committed ones | closed |
| T-02-13 | medium | mitigate | closed source-tag set; refusal on unknown tag; `.unv` flag on every non-hardware row | closed |
| T-02-14 | medium | mitigate | `fixtures.cpu_asm` (+ self-test) compares pinned ca65/ld65 output to committed hex | closed |
| T-02-15 | low | mitigate | assembled fixture hashes equal manifest `sha256`; scan accepts them only by manifest hash | closed |
| T-02-16 | medium | mitigate | `lynx__bus_cost_model_valid` on the setter and snapshot restore; `api.cost_model(.self_test)` | closed |
| T-02-17 | high | mitigate | snapshot restore validates every field then applies with no failure path; `api.snapshot_refusal` | closed |
| T-02-18 | medium | mitigate | tick-headroom clamp and UNDEFINED stop; `api.advance_budget`, `api.advance_slicing` | closed |
| T-02-19 | medium | mitigate | profile range and config refusal before any write; `api.init_refusal` | closed |
| T-02-20 | low | mitigate | runner `--profile` closed set, else usage error exit 2 | closed |
| T-02-21 | medium | mitigate | `api.seam_agreement.none/.lynx1/.lynx2` | closed |
| T-02-22 | high | mitigate | `hygiene.sh --tree` and `--history` exit 0; noreply identities only | closed |
| T-02-23 | medium | mitigate | CI selected by `headSha`; recorded runs match the SUMMARY | closed |
| T-02-SC (x11) | low | accept | no new package manager or dependency; only commit- and hash-pinned vector data | closed (accepted) |

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-02-01 | T-02-SC | Phase 2 adds no third-party dependency; third-party data is pinned, licensed, manifested | plans 02-01..02-11 | 2026-11-10 |

## Residual Risks and Owner Follow-ups (none block the phase)

1. `vectors-full.sh pin` mode is trust-on-first-use for the 512 rows, bounded by the commit pin and the LICENSE hash check; later modes verify against the committed record.
2. The 2 GB full lane stays manual (not in CI): the full-data evidence is as fresh as the last manual run (recorded in 02-11-SUMMARY).
3. Snapshot payload integrity is a plain SHA-256 without authentication, so safety rests on the value-range checks (T-02-17).
4. Owner assumption A10 (notice form for committed MIT material) remains open in docs/provenance.md; Phase 1 owner follow-ups (required-check ruleset, `sha_pinning_required`) are unchanged.
5. Open owner decisions bound the claims: O1 (no baseline, no profile-naming claim), O5, O6.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-10 | 33 | 33 | 0 | gsd-security-auditor |

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-11-10
