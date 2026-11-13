# Lessons (rolling)

## Timer evidence preparation (2026-11-13)

- Remove stale owner questions when their investigation is already authorized
  and complete. Unknown equipment means a parked physical track, not another
  request to identify a model or approve an infeasible run. A bounded search
  of existing sources can supply evidence or document the narrow remaining
  gap; it cannot manufacture hardware facts or waive the comparison.

- Reach a complete guest-to-pixel path before expanding the case matrix.
  Independent glyph/palette checks found a wrong palette pen; viewing the full
  captured frame found uncleared pixels hidden by the fresh software palette.
  Verify unused screen areas and actual guest probe/completion values too.
- Private artifact retention requires permissions, not just a scratch name.
  Use owner-only leaves before writing content, reject widened package modes,
  and test that refusal. Preserve earlier failure artifacts with complete
  inventories under private storage before removing them from the build scan.
- Preserve timed CODE independently of wrapper/UI instrumentation and bind the
  entire image to an approved identity list. A refreshed self-reported receipt
  must not authorize changed code. Explicit startup can change observations;
  matching a source count does not establish historical CRT equivalence.
- Measure timeout cost before changing limits. A local GNU Debug case used
  23.900 CPU seconds; the optimized build retained both sanitizers and passed
  the unchanged group limits. Keep the failed Debug evidence and let hosted
  Debug own the ordinary CI gate. Do not turn this local qualification into
  another CI matrix or a global build-policy change.
- Include the already-installed Linux ShellCheck in the first focused portability
  check. Native 0.11 accepted guard chains that hosted 0.9 flagged as SC2015.
  Rewrite those guards explicitly, verify both versions, then retry CI only
  after the source repair. Optimized GNU execution alone is not a lint check.

See [the software preparation summary](quick/261113-h5w-prepare-a-source-built-timer-hardware-ev/261113-h5w-SUMMARY.md).

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

**Check the existing inventory after a broad contract change.** Phase6 focused
audio tests passed while the early full inventory exposed stale unavailable
address assertions, an incomplete private suite-builder fixture and a helper
identity bound into the timer pending descriptor. Audit refusal boundaries,
synthetic fixture inputs and evidence-bound helper hashes alongside raw readers
and copied build lists. Preserve each failure class at a still-valid boundary;
an identity migration needs old/new output equivalence and unchanged comparison
scope. Use the early full inventory to discover integration work, then focused
repairs before final qualification.

**Preserve failure logs before the next CTest call.** Phase6's initial full run
had failures whose per-case output was overwritten by later discovery and
focused runs. Capture each integration invocation in a distinct private log
with its real exit status. Two scanner timeout causes could not be recovered
from the lost output; bounded traces later showed active case progression and
serial diagnostics completed within the existing limit. That establishes the
later behavior, not the cause of the original timeouts. Do not increase limits,
drop controls or blame host pressure without attributable evidence.

**Use real Git synchronization for qualification.** A shorthand request to
fast-forward a reused Linux clone was misread as a command and led to a patch
overlay on an older commit. That run was excluded from final evidence. Explicit
fetch/merge commands, exact full commit identity, a clean tracked tree and a new
configure preceded the valid scoped run. Keep source and build identity checks
concrete in handoffs.

**Name existing fixtures before extending a neighboring helper.** Plan06's
first continuation design followed the input-only synthetic video helper and
missed the already source-built A/V guest. The handoff was corrected to name
`fixtures/diagnostic/av_probe.s`, its linker config, provenance manifest and
existing consumer driver. The deterministic harness now reuses that guest;
synthetic state is limited to additional capacity/ring boundaries. Existing
producer paths and their identity checks belong in the next task's handoff.

**Validate snapshot reachability with accepted boundary controls.** An initial
inference that a valid event cursor could never have sequence zero was wrong:
device application can open the cursor without emitting an event. The proposed
restriction was removed before commit and a positive roundtrip protects this
state. For refusal controls, use a fresh same-content candidate and a separate
later watched target; assert that mutations differ from the candidate baseline
and recompute the payload hash to reach state validation.

**A successful fixture test does not establish artifact ownership.** Plan06
removed its generated object but retained a compiled guest whose digest was in
the derived-guest manifest field; the binary scanner permits only its declared
generic identity exemption. An early artifact scan exposed that output plus
old retained failures. Keep the rights policy intact, use per-case owned scratch
with success cleanup, and preserve attributable old failures outside the final
scan with verified byte/type/mode inventories. Do not infer that every binary
in a project build tree is permitted or that all old scratch can be deleted.

**Close the observation-record handshake before checker expansion.** Plan07's
first bundle retained frame/audio streams but lacked explicit applied-input and
complete guest-progress records. These were added before closing the producer
task, with sizes, identities and checkpoint-prefix ownership. Name the concrete
record for every required observation class. Decode the fixture's actual byte
layout and use its response marker; a saturating progress counter need not
change between otherwise distinct input responses.


**Recheck integration after later semantic repairs.** Plan09 corrected audio
output latching and ring pulse lifetimes against unchanged pinned source
oracles. The final gate then exposed six older sampling assertions and the
ordering fault harness's inherited clean baseline. They now distinguish the
CPU's same-tick pulse read from expired status at the returned tick, preserving
DONE/IRQ, divider LAST_CLOCK and the separate even timer chain. A full inventory
after that owning repair would have found this before final review. Repeat the
early integration check after later shared semantic changes, not only the first
layout change; keep focused repairs between broad gates.

**Preflight the environment actually being qualified.** The first final GNU
run passed every test but failed artifact scanning on two older generated suite
roots. Native cleanup did not cover the reused container. Build-record/nonce
verification and complete equal before/after tar inventories allowed private
preservation, followed by the unchanged scanner. Run that environment's artifact
preflight before paying for the broad gate. Original failed logs/status remain
separate from qualification.

**Remove repeated work at its proof boundary.** The legacy frame replay matrix
ran two extra interleaved instances for each sensitivity transcript after
already proving interleaving for both profiles and handedness on the complete
baseline. One GNU invocation took234.34s and another timed out at240.01s. The
sensitivity run now uses a fresh production instance; all12 input events,
23 frames, profile/handedness combinations, fresh equality, baseline
interleaving, independent semantic/corruption checks and changed-event assertions
remain. Focused native/GNU runs passed14.09s/167.93s; final GNU canonical replay
passed138.69s. These observations establish useful limit margin, not a controlled
emulator speed comparison or a proven cause for every prior timing variation.
Stage labels now identify each matrix operation; timeout limits are unchanged.


**Keep lifecycle status cells machine-readable.** Shared requirements carried
phase-slice prose in the traceability Status column, so the SDK correctly refused
to advance them as Pending. After checking all sibling evidence, move the slice
detail to the definition notes and use the supported Pending/Complete status.
Inspect the per-requirement write set; a zero tool exit does not mean every
requested requirement was reconciled. Preserve any genuinely open requirement.


**Validate summary YAML before handoff.** The milestone audit found four older
regex references with a single backslash inside YAML double quotes. The strict
SDK refused each summary instead of returning empty requirements. JSON quoting
preserved the exact command text while making the frontmatter parseable; all69
summaries then extracted successfully. Keep parse failure distinct from absent
coverage, and validate each new summary before committing it. Historical
verification hashes are not blindly rewritten after this metadata repair.

## CPU and device ownership intervals (2026-11-13)

- Bus-priority tests must exercise requests arriving inside another owner's
  priced interval, in both directions, as well as equal-ready ties. A CPU
  readiness deadline also bounds when video/refresh may start; priority alone
  does not establish exclusive ownership.
- A scheduler repair can make new pending states reachable at public returns,
  including an exact half-open release boundary. Couple it with snapshot
  round-trips, bounded hostile timestamp controls and coarse/sliced continuation.
- Single-factor timing experiments over a full guest can shift earlier display
  phase and the number of stalls. Account for actual intervals and counts;
  differences between complete runs are not additive timing corrections.
  See `.planning/debug/evidence/timer-focused-bus-accounting.md`.
- Select affected tests by their owning contracts, including fault-control
  prefixes, rather than only `api.*`. The first full inventory caught an old
  DMA fault fixture that duplicated the same pre-release setup; its clean
  positive had been omitted from the narrow API selection. Align the fixture
  with the release boundary, retain its actual mutant and clean brackets, and
  rerun the final gate only after that concrete change.
