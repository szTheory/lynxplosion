# Lessons (rolling)

Lessons carried forward between phases and, per AGENTS.md, between emulator projects.
Each entry names the evidence. Update at every phase transition and milestone close.

## Phase 1: Evidence Harness and Walking Skeleton (2026-11-09)

**Gates must be proven in the failing direction, and proven on the other OS.**
- CI caught three Linux-only defects the dev host hid: CMake script mode needs
  `cmake_policy(VERSION 3.25)` for `IN_LIST` (CI cmake 3.31.6 vs dev 4.4.3), shellcheck 0.9.0
  reports SC2317 where 0.11 reports SC2329, and shellcheck 0.9.0 flags SC2015 in a control the
  newer one accepts. Run `check.sh` in an `ubuntu:24.04` container before spending CI runs.
- A standard-depth code review after execution found a critical flaw in the privacy gate that all
  13 plans' own tests had missed: a per-line allow-list let an allowed token hide a real leak on
  the same line. Self-tests that only seed the obvious fault are not enough; review the gates as
  code. Keep a seeded same-line / prefix-suffix control for every allow-list.
- `set -e` does not fire inside a function called under `if !`. A step wrapper written that way
  silently disables fail-fast for everything but the last command. Call bare and use an EXIT trap.

**Rights and privacy scans need a derived, hash-keyed allowlist, not a path exemption.**
- A pinned third-party archive contains 71 non-text files; a build adds ~1,700 more. The rule
  that worked: pass only on a (path, SHA-256) match against a list derived from the verified
  archive at extraction time, plus a second list written after the pinned recipe's build.
  Anything added or changed later fails. `grep -I` is NUL-only, so detect non-text by control
  bytes and invalid UTF-8.
- A rights gate that only inspects non-text files misses firmware stored as hex or base64;
  document that limit visibly rather than letting the gate read as stronger than it is.
- Writing a review report that quotes a leak repro literally trips the history scan. Quote
  repro strings with placeholders. A local unpushed commit can be rebuilt (verify the final tree
  is byte-identical); a pushed one cannot.

**Determinism evidence.**
- A golden digest is usable across compilers only if it excludes build identity (git revision).
  Keep the build-independent observation digest golden and the bundle digest non-golden.
- Add sensitivity controls (seed, content, tick budget, one state bit must change the digest) or
  digest equality is vacuous.
- A full-width tick budget (`UINT64_MAX`) on a non-halting image never returns; use a halt image
  for that control and a white-box start state for wraparound.

**Verification cost.** The full check grew from 5 s to ~64 s serial as gates were added.
`"jobs": 4` in the test preset (every test already used its own scratch path) halved it. CI
measured 23-29 s for the job; the cc65 build (7-16 s) is the largest single step but a cache
would not beat runner variance at that size. Revisit only past 30 s.

**cc65 as a pinned toolchain.** `make bin lib TARGETS=lynx` as one invocation races; run two.
The Makefile embeds the enclosing repository's commit in tool bytes unless `BUILD_ID` is pinned,
which breaks tool-hash reproducibility. Tool hashes differ between macOS clang and Linux gcc
builds; the linked probe `.lnx` is byte-identical across both and is the evidence to record.

**Process.** The owner's autonomy rules worked: discuss, research and plan fan-out produced 13
plans that executed with no owner stops. Plan checking cut 14 findings to 0 in three passes; the
costliest were scope (plans over 15 files) and a rights-gate rule that did not account for the
toolchain's own build outputs. Decide such conflicts at the CONTEXT level and record them.

## Phase 2: CPU Profiles and Per-Opcode Vectors (2026-11-10)

**Independent oracles beat self-authored ones.** The per-cycle expectation lists the core author wrote catch
slips, not table errors; the pinned vectors (final state), a hand-derived fixture trace written before the CPU
met it (matched on the first run), and a seeded wrong-profile control are what make the result credible.
Constant "poison" for unlisted reads would have let 8 of 16 bit-op opcodes pass the reverse control vacuously;
use an address-hashed value.

**Where the hardware evidence is thin, label it instead of choosing.** Epyx's CPU spec is non-public, and
SingleStepTests disagrees with the datasheets on several reserved opcodes. The rule that worked: follow
hardware-informed Lynx evidence where it exists, follow the source variant elsewhere, tag every value with its
source and an unverified flag, and let the check print counts per tag.

**Parallel lenses need an explicit conflict pass.** Four research lenses disagreed on mid-instruction budget
stops; resolving it against an already-verified Phase 1 property (consumed never exceeds the budget) settled it.

**Cost control.** Committing 6.4 MB of test data made scans 10x slower until the scanners read each file once
and prefiltered fixed strings; part of an apparent regression was host load, so compare CPU seconds. The
history scan still grows about 1.5 s per commit because the data is in every later commit (candidate: scan each
blob once).

**Portability and process.** Run the full check in an ubuntu:24.04 container before pushing: it caught a CMake
policy class (while-loop and IN_LIST under script-mode policies) twice. A post-execution review found
restore-validation gaps (forged snapshots reaching states execution never produces) that every plan's tests
missed; review restore paths as attack surface. Plan-check estimates were low-confidence: executors used a
fraction of the projected tokens.

**Open follow-ups.** Test fragility: `cpu.vectors.malformed`/`complete_mode` fail when the repo path is
about 170 characters (diagnostic truncation); the bus tick stamp is plumbed but unobserved until a device
consumes it (Phase 4).

## Phase 3: In-progress integration lessons (2026-11-10)

**Generated evidence needs the same privacy checks as source.** A TDD RED JSON
record captured local checkout paths in report/command/console fields. Tree
normalization alone did not repair historical copies. The unpublished-only
repair checked live remote main, changed only that evidence in six historical
trees, and preserved both the final committed tree and all working changes.
See the Phase 3 `03-HISTORY-REPAIR.md` audit. Check generated reports before
their first commit; preserve observed failure semantics when normalizing paths.

**Validate scanner behavior with valid long text too.** On this macOS host,
`iconv` writing valid long UTF-8 to `/dev/null` misclassified a progress line as
binary. A seeded valid-text control reproduced the issue. The fix changes the
validation sink while preserving iconv failure handling and existing invalid
UTF-8/control-byte rejection. The full local gate subsequently passed all 98
tests, artifact accounting and history hygiene; Linux qualification remains
part of final Phase 3 delivery.

**Binary input migrations include test-output ownership.** Moving runner tests
from text payloads to binary BLL objects exposed leftover temporary inputs and
a manually compiled API probe to the artifact gate. Clean up only test-owned
files after successful checks; do not add an artifact exemption to make the
gate green.

**A plan's threat disposition is not owner acceptance.** The initial Phase 3
plans marked the pinned-toolchain threat as `accept`, but there was no owner
risk-acceptance record. The current delivery was corrected to `mitigate` and
verified against the existing hash-pinned download/build controls. That closes
the current-delivery threat only; it does not define a future dependency or
package-allowlist policy. Reassess if dependency installation scope changes.

**Keep inherited snapshot edge cases as explicit follow-ups.** The 03 review
noted that a forged snapshot with a 255-tick `cpu_ready_at` span is accepted;
this predates the reviewed delta and was not changed in Plan 03-09. Track it in
STATE before making broader snapshot-validation claims.

**Bind the regression command explicitly for CMake.** The generic GSD language sniff did not recognize this project and would resolve to a successful no-op. Set workflow.test_command to the documented fast gate. Run that gate once for both regression and final local qualification; do not duplicate it under two labels.

**Use self-contained Git objects in containers.** A shared local clone points outside the container. Use a self-contained clone before requiring history hygiene. Keep raw logs temporary and generated report links relative to the repository before committing.

**Keep closeout handoffs bounded.** Reuse reviewed evidence and return concrete edits; repeated report reconstruction can dominate the critical path after tests finish. Parent-owned metadata closeout can avoid another broad executor context reload.

**Validate MVP goal form before execution.** A roadmap `mode: mvp` requires the centralized user-story guard, including its literal `As a ...` prefix. Phase 3's inherited prose goal only failed at final verification. Reformat the same approved outcome and validate it before dispatch; changing grammar is routine planning repair, not a new scope decision.

**Treat timing-test startup as part of the fixture contract.** Untouched test
assembly and an exact source oracle do not establish equivalent execution
preconditions after replacing CRT startup. Check display/refresh/page-mode
state, zero-page initialization, link geometry and caller IRQ behavior before
requiring fixed-loop timing agreement. In Phase 4 the current timer mismatch
was explained by independent cycle accounting, not resolved by phase tuning;
a synthetic page crossing could force agreement and was correctly rejected as
a remedy. Preserve raw disagreements and seek a scope decision when the next
real prerequisite belongs to a later phase.


## Generated test output and strict artifact scans (Phase 4)

A filesystem hardening change retained successful suite scratch. The next full
gate passed every test but rejected generated binaries and intentional text
fixtures, and the accumulated files added minutes to scanning. Check cleanup
contracts against the canonical gate before choosing retention as a remedy.
Use unique invocation directories, then recheck canonical containment and the
exact path/nonce owner marker before success cleanup. Preserve failed runs for
diagnosis. Prove missing/stale marker and symlink boundaries with outside-target
controls; keep the artifact policy unchanged. Existing leftovers can be moved
reversibly to private temporary evidence. Run pure manifest self-controls
directly while fixing their assertions before repeating an expensive batch.


Ubuntu ShellCheck0.9.0 rejected compound `A && B || refusal` expressions that
the local newer version accepted. Use explicit conditional refusals in portable
scripts and keep lint severity intact. When script bytes are evidence-bound,
update their exact descriptor and mutation-control hashes together, preserving
source oracle content and guest behavior.

**Container reporting wrappers need a quoting check before execution.** A
double-quoted `dpkg-query -f="${Version}"` inside `sh -lc` expands in the
container shell; with `set -u`, an unset variable stops setup before the clone
or project gate. Use `dpkg-query -W ca-certificates` for simple version
evidence, or keep the format expression single-quoted at the inner shell level.
Confirm the emitted OS/tool/source preamble before treating a container run as
qualification evidence.


**Distinguish negative claim guards from open hardware decisions.** A review can verify that unsupported hardware/profile claims were not made while leaving calibration and baseline selection open. Preserving an existing stop does not require renewed owner confirmation.

**Treat reference warnings according to their actual targets.** Phase completion scans inline code as paths: shell commands and brace shorthand produce false missing-file warnings. The Phase 4 command targets and all three RED JSON files exist; these warnings do not invalidate test or goal evidence. Use literal file paths for future artifact links and keep commands in verification prose.


**Measure executor progress, not just commit cadence.** Reading the execution contract, active source edits, passing focused checks and closeout tool activity are progress. A long verification tail is not a new approval gate. Keep the parent read-only during a foreground handoff; do not inject finalization urgency into an active executor. Reconcile summary plus reachable commits before classifying an abnormal end.

**Keep report commands executable and metrics evidenced.** Markdown code spans need the actual shell regex, not a JSON/YAML-escaped copy. Preserve YAML escaping inside quoted frontmatter separately. Omit token actuals without an instrumented provenance source; task/commit counts can be checked from the repository.


**Respect the GSD metric unit before overriding a schema field.** Executor summaries define `actuals.tokens` as realized source-diff characters divided by four. Keep that required, reproducibly measured proxy and label its unit; do not present it as measured model/API usage or remove it merely because the field name is overloaded. Record the source-diff range/path convention. Large shared test files can be read by locating relevant helpers/cases/dispatchers with rg and bounded sections; keep the required execution contract and actual dependency context intact.


**Run integration checks when structural contracts change.** Phase5 final qualification found twelve failing CTests after narrow feature checks passed: old snapshot header/hash offsets in CPU/diagnostic harnesses, private mutant source lists missing new modules, and a platform compiler-helper allowlist. When changing serialized layout or core module dependencies, locate all raw snapshot consumers, independent build recipes and ABI checks in the same implementation plan and run that affected group then. A canonical gate before the review/closeout tail catches cross-plan omissions earlier; repeat it only for new changes or unresolved failures. Preserve exact semantic negative controls rather than treating failed builds as detection.


**Validate supported lint versions before final gates.** Phase5 macOS ShellCheck0.11 accepted compound shell fallback expressions that Ubuntu0.9 rejected. Use explicit conditional refusals and check both supported environments during script work; do not disable the diagnostics. For copied-executable mutation controls on macOS, byte restoration over the same inode can still leave a process killed while a byte-identical fresh inode executes; restore the private fixture through a verified fresh file and atomic rename, retaining identity checks and positive brackets. The observed inode differential is evidence; the OS termination reason was not established by logs.


**Separate portable fixture identity from native compiler output identity.** Phase5 consumer checks initially hard-coded macOS-built ca65/ld65 hashes in a fixture manifest, rejecting verified Linux tools before guest execution. Bind the portable manifest to pinned tool source and recipe, validate native executable hashes against each successful local build record, and retain exact source-built guest hashes as cross-platform output oracles. Check the actual Linux consumer slice before the final review tail; source pin equality alone is not a claim of identical native executables.

**Reduce orchestration before optimizing a short CI gate.** The Phase5 final
plan records 9h35 elapsed across review/qualification, while its canonical gates
took roughly two to three minutes each. Phase6 also spent about 90 minutes
between the Phase5 closeout commit and the reviewed plan commit, with repeated
research and mapping handoffs. These are elapsed spans, not measured active
agent or API usage. Reuse research, opt into mapping only when a concrete analog
is missing, batch repair/review deltas and evidence updates, and keep required
independent acceptance checks. The operational defaults are in AUTONOMY.md;
Phase6 06-EFFICIENCY-REVIEW.md retains the evidence and limits.

**Compare rendered payloads separately from metadata.** The first audio tracer
produced valid PPMs with equal raster dimensions but different comment lengths;
an equal-file-length assertion hid that distinction behind an aggregate error.
The checker now validates each raster against raw pens/palettes and reports its
failing stage. Clean captures and pixel/metadata/silence/WAV corruption controls
establish the remedy without loosening audio or image semantics.
