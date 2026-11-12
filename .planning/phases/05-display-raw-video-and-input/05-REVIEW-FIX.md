---
phase: 05
fixed_at: 2026-11-12T14:27:44Z
review_path: .planning/phases/05-display-raw-video-and-input/05-REVIEW.md
iteration: 1
findings_in_scope: 4
fixed: 4
skipped: 0
status: all_fixed
---

# Phase 05: Code Review Fix Report

**Fixed at:** 2026-11-12T14:27:44Z
**Source review:** `.planning/phases/05-display-raw-video-and-input/05-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 4
- Fixed: 4
- Skipped: 0

All focused builds and checks below ran headlessly in the main project checkout. The broad `check.sh` gate and final phase qualification were not run; root owns that later qualification step.

## Fixed Issues

### CR-01: Test gate deletes the supplied WORK path before validating it (pre-existing; outside Phase 5 diff)

**Files modified:** `tests/gates/manifests_controls.sh`
**Commit:** `620e7c285e73250da5298e2ebfdf5d6e48a8661a`
**Applied fix:** Validate the target and resolved parents before any mutation, require a fresh supported scratch location and invocation ownership marker, reject aliases/traversal/source paths, and limit recursive cleanup to the owned child directory.

**Old/new provenance:** Reviewed source SHA-256 `e52b7f97ffce6577684b7c8d5fcf168e4a3c1620cf8e6101f3a573f9b0f22c29`; fixed source at the commit SHA-256 `3bc0dee178e32d9d704826441aa8b469a25f8266acc6d53d41fdb04919776a75`.

**Old-behavior proof:** Ran only the exact reviewed script bytes in a synthetic replica under `/private/tmp/lynx-phase5-cr01-old.c9OkH1/replica`. Its marker was deleted by the old cleanup path. The reproduction never targeted repository fixtures, source files, or unrelated outside content. The new guard controls proved refusal preserved the synthetic marker.

**Verification:** `sh -n tests/gates/manifests_controls.sh`; direct manifest controls (32 checks) and work-target guard controls (7 checks) passed; CTest `hygiene.manifests.self_test` passed.

### WR-02: Two source-built guest identity fields bypass SHA-256 validation

**Files modified:** `scripts/validate-kv.sh`, `tests/gates/manifests_controls.sh`
**Commit:** `f61a82f37717d53752188eaa4589cf308a777ece`
**Applied fix:** Recognize both exact left-hand guest SHA-256 keys as hash fields and reject truncated and non-hex values for each, while retaining valid-manifest and strict missing, incomplete, and unknown-field behavior.

**Old/new provenance:** Reviewed validator SHA-256 `cc433c0ea38dc4c18008bfd13182cb47daa49730b819d2f093da3282121bdc` (recorded at old-source capture); fixed validator SHA-256 `82778e9233b9a77232ae094ccb297e744071e7f28a0c472f65c1084228ada811`. Controls immediately before this finding: `3bc0dee178e32d9d704826441aa8b469a25f8266acc6d53d41fdb04919776a75`; after: `c62a8d1a94d05bd3fd258129609688a928d04423ce90029ee44895f17c6bde06`.

**Old-behavior proof:** The exact old validator accepted synthetic manifests with short and non-hex values for both handedness keys. The new focused controls reject all four malformed cases.

**Verification:** CTest `hygiene.manifests` and `hygiene.manifests.self_test` passed.

### WR-01: Undefined-opcode check reads backing RAM for mapped video/input fetches

**Files modified:** `core/machine.c`, `tests/api/test_api.c`
**Commit:** `358874d2a644ad1bc400b633439a3f0680819726`
**Applied fix:** Add a side-effect-free, route-aware opcode peek for the device-backed address regions so opcode legality and page-mode eligibility use the byte the CPU will fetch. Preserve flat RAM behavior for private content tests and leave raw RAM access unchanged. Add guest-fetch controls for mapped-defined/RAM-undefined and mapped-undefined/RAM-defined cases.

**Old/new provenance:** Reviewed `core/machine.c` SHA-256 `b9e37062e2899aa7b5a6a5b6480cefe276f50229452d4a956a7a96648d567625`; fixed machine SHA-256 `74dc654c2a5292680058d2f0d046015f847bdd6a934cd3b0194d2cd86b6faa7c`. Test source before/after: `fdae740881c8846bcee5c235396bfe0016b4f47fd312d718be03b794cef14575` / `cb1732ba82501c544bdb714d4751b70233e2f0b9b99069f772de205b5ee43770`.

**Old-behavior proof:** Built the regression against the exact old machine source; `api.video_bus` produced 12 failed assertions across both mismatch directions. With the fix, the mapped device byte controls actual CPU fetch behavior; profile-`none` refusal still consumes zero ticks and leaves the machine buffer unchanged.

**Verification:** Build target `lynx-test-api` passed; CTest `api.video_bus`, `api.video_page_mode`, and `cpu.profile_divergence` passed. The regression also retains page continuation and private flat-vector bus checks. This is a logic fix and requires human verification of the route decision before final qualification.

### WR-03: Test-only environment variable selects legacy replay in the production consumer

**Files modified:** `consumer/main.c`, `tests/consumer/frame_driver.cmake`
**Commit:** `ec38c34f233168c15a6049ef5768b1989fee6353`
**Applied fix:** Compile the legacy tracer implementation, its private helpers, and environment-variable dispatch only in `LYNX_CONSUMER_TESTING` builds. Keep historical tracer assertions on `TEST_CONSUMER`; add a source-built production replay comparison with and without `LYNX_TEST_REPLAY_LEGACY=1`, checking the manifest and raw/rendered frame outputs.

**Old/new provenance:** Reviewed `consumer/main.c` SHA-256 `77ba351b36417ef8831daad869fa42fcc83d33d3aed5c78490aeb9cb40f9f6ec`; fixed source `a0cdfe145390dea8eb7854ed208ed2ee4341ebce86dbf2ac2814c604e15dabaa`. Reviewed test driver SHA-256 `a432f97ecd7d1fbfeea2227d006db57bf662e630d9a7c1718d7e86a14eb1702b`; fixed driver `8e63fccfe8f33ce8928a2fed8f2023eb991bb21871e017a2aeea872f80e10387`.

**Old-behavior proof:** Before the source fix, the production override run was redirected into the legacy tracer and failed with `recorded A event is not the fixture's exact non-frame tick`. After the fix, the production target completes both runs and produces identical validated replay manifests and raw/rendered frames while the test target retains legacy raw pen, palette, timing, and input-sample assertions.

**Verification:** Built `lynx-consume`, `lynx-consume-test`, and `lynx-check-artifacts`; CTest `consumer.frame_tracer`, `consumer.frame_replay`, `consumer.snapshot_ppm`, and `consumer.silence_wav` passed.

## Skipped Issues

None.

---

_Fixed: 2026-11-12T14:27:44Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
