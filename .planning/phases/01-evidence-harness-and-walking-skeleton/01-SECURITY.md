---
phase: "1"
slug: "evidence-harness-and-walking-skeleton"
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: "2026-11-09"
---

# Phase 1 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Register authored at plan time (`register_authored_at_plan_time: true`); 36 unique threat IDs
> T-01-01 to T-01-36 plus the reserved supply-chain rows T-01-SC (13 rows across the plans).
> Verified by gsd-security-auditor against HEAD `8f71e6c` (CI green, run 42733279755).

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Host to core API | The host supplies buffers, content bytes, config and snapshot blobs to a deterministic, allocation-free core | Untrusted bytes, lengths, addresses |
| Snapshot blob to machine state | A snapshot is parsed and validated before any state is replaced | Untrusted blob |
| Runner CLI / hex files to core | `lynx-run` parses arguments and hex content before calling the public API | Untrusted text |
| Network to build tree | Pinned source archives are fetched once by `scripts/fetch-pin.sh`; no test touches the network | Third-party source archives |
| Repository to public GitHub | Everything tracked, plus history, is world-readable | PII, credentials, rights-bearing bytes |
| CI runner | GitHub Actions executes `scripts/check.sh` on push and pull request | Repository source, read-only token |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-01-01 | Tampering | CI action pinning | high | mitigate | `actions/checkout` pinned by commit SHA (ci.yml:20); only `uses:` in file | closed |
| T-01-02 | Elevation of Privilege | CI token | high | mitigate | `permissions: contents: read`, `persist-credentials: false`, `timeout-minutes`, no secrets, no `pull_request_target` | closed |
| T-01-03 | Information Disclosure | first public push | high | mitigate | pre-push tree and history scans; `hygiene.sh --history` exits 0; noreply identities only | closed |
| T-01-04 | Tampering / DoS | core arithmetic | medium | mitigate | subtraction-form bounds, 32-bit PC wrap, `-fno-sanitize-recover=all`; gates `api.advance_budget`, `api.load_refusal_unchanged`, `api.inspect_pure` | closed |
| T-01-05 | Repudiation | test inventory | high | mitigate | configure-time equality, JUnit completeness, `policy.no_skip` | closed |
| T-01-06 | Tampering | negative controls | medium | mitigate | `expect_fail.cmake` requires exact exit code and message regex | closed |
| T-01-07 | Tampering | core host imports | high | mitigate | `core.includes`, `core.no_float`, `core.nm_undefined`, `core.nm_globals` with failing fixtures | closed |
| T-01-08 | Repudiation | nm on instrumented build | medium | mitigate | nm checks read only the uninstrumented `lynx_core_plain` twin | closed |
| T-01-09 | Tampering / DoS | init, load, advance refusal | high | mitigate | argument/size/alignment checks before state change; whole-buffer `memcmp` oracle tests under ASan+UBSan | closed |
| T-01-10 | Information Disclosure | instance memory | low | mitigate | `memset` of whole struct in init and reset; `api.instances_independent` over garbage | closed |
| T-01-11 | Tampering | snapshot restore | high | mitigate | ordered validation then a no-failure apply; every header byte flipped in `api.snapshot_refusal` | closed |
| T-01-12 | Spoofing | snapshot identity | medium | mitigate | identity compared against the instance, not the blob | closed |
| T-01-13 | Tampering / DoS | snapshot save | medium | mitigate | capacity and overlap checked before the first write | closed |
| T-01-14 | Repudiation | determinism evidence | high | mitigate | `det.sensitivity*`, `det.golden`, `det.two_instances`; digests labelled repeatability, not accuracy | closed |
| T-01-15 | Information Disclosure | run manifest | medium | mitigate | `validate-kv.sh --no-host-data`; no timestamps, hosts or paths in bundles | closed |
| T-01-16 | Information Disclosure | push of plans 01-02..01-08 | high | mitigate | pre-push hygiene recorded; re-verified clean | closed |
| T-01-17 | Tampering | pin fetch | high | mitigate | SHA-256 verified before unpack; https only; commit-SHA URLs; `fetch_pin.mismatch.self_test`, `policy.pins` | closed |
| T-01-18 | Tampering | extraction | medium | mitigate | extraction and file list built in a temp dir, moved together, stamp last | closed |
| T-01-19 | Tampering | cc65 probe | low | accept | accepted in 01-10-PLAN; probe hash re-checked every run by `toolchain.probe` | closed (accepted) |
| T-01-20 | Information Disclosure (rights) | binary and firmware scan | high | mitigate | control-byte and UTF-8 detection, manifest allowlist, 512-byte rule everywhere, text-side hex/base64 rules; `hygiene.firmware_scan` + self-test | closed |
| T-01-21 | Information Disclosure | PII and credential scan | high | mitigate | `hygiene.sh` tree and history, whole-token allow-list (CR-01 fix), CI `fetch-depth: 0`; `hygiene.pii.self_test` | closed |
| T-01-22 | Denial of Service (push blocked) | negative controls | medium | mitigate | fake credentials built from fragments at run time outside the checkout | closed |
| T-01-23 | Tampering | file name handling | medium | mitigate | `ls-files -z`, newline refusal, `set -f`, status-checked listings | closed |
| T-01-24 | Repudiation | claims | high | mitigate | `check-claims.sh`: profile defaults to `none`, `inventory_sha256` bound to `fast.txt` | closed |
| T-01-25 | Tampering (legal) | clean-room record | medium | mitigate | `docs.policy` requires the DRAFT banner and per-emulator exposure rows | closed |
| T-01-26 | Tampering | CI cache poisoning | high | mitigate | no `actions/cache` in use; conditions recorded in 01-13 SUMMARY for any later cache | closed (not triggered) |
| T-01-27 | Information Disclosure | final push | medium | mitigate | tree and history scans re-verified; read-only token | closed |
| T-01-28 | Information Disclosure | build id | medium | mitigate | `git=`/`cc=`/`sys=` only, length-limited; `api.build_id` rejects path characters | closed |
| T-01-29 | Spoofing | content identity | medium | mitigate | content SHA-256 computed in-core from accepted bytes | closed |
| T-01-30 | Tampering | runner parsing | medium | mitigate | strict hex parser, overflow-checked decimal parser, 1 MiB cap; `runner.cli` | closed |
| T-01-31 | Information Disclosure | bundle contents | medium | mitigate | text-only bundle with no host data | closed |
| T-01-32 | DoS / Information Disclosure | tests reaching the network | medium | mitigate | `policy.no_network` + self-test; offline `file://` control gated by an env flag | closed |
| T-01-33 | Repudiation | fixture manifests | medium | mitigate | `check-manifests.sh` hash-keyed, full coverage; `hygiene.manifests` + self-test | closed |
| T-01-34 | Tampering (rights) | pin-zone scan | high | mitigate | pin files pass only on a path+hash match against the archive or build list; five seeded controls | closed |
| T-01-35 | Tampering (rights) | cc65 build list | high | mitigate | non-pristine extraction re-fetched; build list written only after both make runs and the probe; cache requires list, stamp and tools to agree | closed |
| T-01-36 | Repudiation (licensing) | copied helpers | medium | mitigate | `docs.provenance` + self-test list every muhtari-adapted file | closed |
| T-01-SC (11 rows) | Tampering | no new third-party dependency | low | accept | recorded in plans 01-08, 01-11..01-13; no package manager in scripts, tests, cmake or CI | closed (accepted) |
| T-01-SC (2 rows) | Tampering | pinned toolchain and fixture sources | high | mitigate | plans 01-09 and 01-10: SHA-256-verified commit-SHA pins, build only from the verified pin, tool and probe hashes printed every run | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on (high) count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-01-01 | T-01-19 | The cc65 probe is project-authored and its hash is re-checked every run; a tampered probe would also fail the pinned-source hash checks first | plan (01-10-PLAN) | 2026-11-09 |
| AR-01-02 | T-01-SC (accept rows) | Phase 1 adds no third-party dependency beyond the pinned checkout action and two pinned source archives | plans 01-08, 01-11..01-13 | 2026-11-09 |

*Accepted risks do not resurface in future audit runs.*

---

## Residual Risks and Owner Follow-ups (from the audit; none block the phase)

1. **No required-check enforcement on `main`.** The repository has no ruleset or branch protection, so CI is advisory. Owner follow-up: a ruleset requiring the `check` job and blocking force pushes. The rights and privacy gates are only as strong as this enforcement.
2. **`sha_pinning_required` is false** in the repository Actions settings and `allowed_actions` is `all`. The workflow pins by SHA, but the setting would enforce it for future edits. Dependabot security updates are off.
3. **Confirmed positive settings:** public repo; secret scanning and push protection on; default workflow permissions read; Actions cannot approve pull requests.
4. T-01-02 register wording says "pull_request only"; the workflow also runs on `push` to `main` (as the plan text states), read-only and secretless. No privilege gap.
5. T-01-34 fails non-text (binary) unlisted or modified pin-zone files plus the two text-side rules. An unlisted ordinary text file in `build/pins/` passes. Fits the rights threat; stricter only if the owner wants it.
6. Documented limits of the firmware scan (docs/policy/clean-room.md): images split over many short base64 lines, C byte arrays, other encodings and compressed archives are not detected. Provenance review of new fixtures remains a human step.
7. Open owner decisions bound the claims: O1 (profile `none` only), O5 (no licence file), O6 (clean-room and exposure record stay DRAFT).
8. Deferred review items with a minor security flavour, all low: IN-07 (source rights scan reads the working tree rather than the index), IN-09 (toolchain-output zone trusts magic bytes by design), `PINS` env override in `fetch-pin.sh` (used only by the offline control), and symlink handling differing between the rights and privacy scans.
9. `aac9f2f` (the review report before the placeholders replaced the repro strings) exists only as a dangling local object. It was never pushed and is unreachable from any ref.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-09 | 38 | 38 | 0 | gsd-security-auditor |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-11-09
