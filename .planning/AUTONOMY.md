# Autonomy and working defaults

Owner-delegated 2026-11-09. `AGENTS.md` is the authority; this file records
how GSD workflows apply it. Update it when the owner changes a default.

## Explicit stops (owner decision required)

- Copyrighted firmware, BIOS or commercial game content: no use, commit or
  distribution without documented permission.
- Physical hardware testing or qualification.
- Project licence (O5), boot ROM / HLE rights (O2, O4), owner BIOS (O3).
- Releases and any further change of visibility or distribution channel.

## Owner decisions already given

- 2026-11-09: the repository may be **public** at `github.com/szTheory/lynxplosion`
  (owner, Phase 1 discuss). Condition: no PII or credentials are committed.
  Commits use the GitHub noreply address; local home paths are written as
  `~/...`. The check command enforces this (hygiene scan).
- 2026-11-09: O8 accepted research default: C17, CMake ≥3.25 workflow
  presets, Ninja, CTest, C ABI with an opaque handle.
- 2026-11-09: O6 proceeds as a draft clean-room policy (implementers do not
  open GPL emulator source; research exposure recorded). Final decision pending.
- 2026-11-11: owner said "follow ur recs" to the documented timer-comparison
  disposition. Finish Phase 4 with timer result offset 14 explicitly pending
  against the unchanged source oracle (observed 0C, expected 0D on both
  profiles), then revisit it in Phase 5's bus/video prerequisite work. All
  other comparisons and execution/model/count/fault controls remain required.
  Strict source agreement remains failing and visible; no suite-pass or
  hardware-correctness claim is authorized. See Phase 4 04-09-DISPOSITION.md.

- 2026-11-11: after automatic approval review rejected the checked Phase 5
  Plan 02 production commit to the default branch, the owner explicitly selected
  "Commit checked changes on main". The normal task-scoped commits succeeded
  with hooks. Preserve this branch-authority evidence with the existing
  `branching_strategy=none` / `allow_default_branch_commits=true` policy so the
  same authorized action does not become another acknowledgment gate. This is
  not authority for a release, a new publication channel, rights-bearing
  content or physical testing. See Phase 5 05-02-COMMIT-BLOCKER.md.

## Discuss phases: fan out, synthesize, auto-follow

For every discuss phase (and milestone discussion):

1. Enumerate the phase's decision points.
2. Fan out with subagents, one per decision cluster, for context efficiency.
   Each considers breadth and depth through every relevant role lens
   (hardware, CPU/bus, graphics/video, audio, input, API/host, security and
   licensing, verification, performance, DevOps, product, DX), with pros,
   cons, trade-offs, patterns, anti-patterns, footguns and lessons from other
   emulators and ecosystems. Prefer primary, authoritative sources; think
   independently too. Include an adversarial pass.
3. Stop at diminishing returns. Synthesize one coherent set of one-shot
   recommendations consistent with project goals and prior decisions.
4. **Auto-follow the recommendations.** Ask the owner only for decisions on
   the explicit-stops list above or ones that materially change scope, rights,
   authority, risk or cost. Batch those questions.
5. Dependency rule: another copy-paste is better than another dependency.
   Prefer small, flat dependency trees; add a dependency only when its benefit
   clearly exceeds its lifetime and supply-chain cost.

## Verify phases: automate and shift left

- Goal: zero human verification. Automate integration, e2e, smoke and seam
  tests as early as they give useful feedback. Hand the owner only what truly
  needs a human (subjective judgement, rights, physical hardware).
- Throwaway verification is fine. Not everything belongs in CI: add a CI
  check only when it catches a distinct recurring failure at a useful cost.
- Do not build verification that corners the design; only what gives useful
  feedback.
- When a `human_needed` item can be turned into an automated check, do that
  before asking the owner.
- Generated GSD/TDD reports can include local checkout paths even when the
  test command is relative. Keep private raw logs in temporary storage;
  normalize path presentation in repository evidence without changing exit
  status, test identity or failure provenance. Run tree hygiene before
  committing generated evidence. Do not rely on the later full gate to catch
  paths already copied into local history (Phase 3 lesson, 2026-11-10).

- At each executor plan handoff, before dispatching its successor, run tree
  hygiene and history hygiene over that plan's new reachable commit range.
  Generated evidence can contain checkout paths even when all focused source
  tests pass. This local check supplements the executor's required pre-commit
  evidence scan; it does not add a duplicate CI job. If a finding is confined
  to unpublished history, preserve a private backup and all source/workspace
  bytes, verify the live remote boundary, and repair only the evidence path.
  Never rewrite published history or weaken the privacy scanner.

  Do not add an optional all-history scan at each plan closeout. The new reachable
  range supplies the handoff proof; the final canonical gate owns the full
  history scan. Preserve the status of interrupted optional work and stop only
  the attributable process when necessary, without weakening required checks.

- Before invoking GSD state updates, copy and hash any pre-existing untracked
  `.planning/state.json` into private temporary storage. Never stage or delete
  it. The SDK can refresh this derived phase/status/next cache while updating
  `STATE.md`; verify that any refresh contains only expected generated fields.
  Such a generated-cache refresh is routine authorized bookkeeping, not lost
  source work or an owner approval gate. Preserve the backup and report any
  unexpected fields or unexplained changes; never fabricate an exact restore
  when prior bytes were not captured. Plan 05-07 exposed this SDK side effect.

- After writing the final SUMMARY and all planning metadata, run tree hygiene
  and `git diff --check` in the same ordered command that calls the GSD SDK
  metadata commit. Use the orchestrator-supplied guarded commit helper, which
  preserves normal hooks, exact arguments and SDK skip semantics. A scan before
  writing the summary is insufficient. The Plan 05-10 handoff caught a late
  summary path; the helper was proven to block SDK execution on that finding.

## CI and CD efficiency review

After every CI run (and at release/CD steps): inspect the slowest
critical-path jobs, duplicated work, caching and concurrency, and take the
low-risk wins for speed, iteration time and Actions cost, without losing a
distinct failure class or proof. Measure before changing. Record what was
changed and why in the phase summary so the review is not repeated.

## Implementation and orchestration efficiency

Owner-requested sanity review, 2026-11-12; evidence is in Phase 6
`06-EFFICIENCY-REVIEW.md`.

- At the owner's repeated 24-hour sanity check, keep execution focused on the
  first runnable complete path, then its required negative cases. Reuse the
  loaded consumer executor for the adjacent replay/publication plans. Fold new
  lessons into normal closeout rather than starting further process audits;
  retain the planned independent reviews and final qualification gates.
- Persist the plan dispatch timestamp in private temporary storage so elapsed
  handoff time survives context recovery. Keep it distinct from active model
  time and API usage; report missing historical timing as unknown.

- Use GSD's supported compact content mode. Read mandatory contracts once
  per owning context; locate large source files by symbol and bounded sections.
  Keep each batched tool response within its output budget so truncation does
  not force repeated reads.
- Reuse completed advisor/source reports. Give implementation research only
  the unresolved questions that can change a plan or its acceptance evidence.
  Finish when those questions are answered or explicitly bounded as unknown.
  Pattern mapping is opt-in for an unfamiliar architecture or a concrete missing
  analog; existing pattern maps remain valid planning inputs.
- Name the existing producer artifacts and their consumer contracts in each
  handoff. Locate the proven fixture/build recipe before extending a nearby
  synthetic test helper; keep synthetic boundary controls distinct from the
  real source-built guest evidence.
- At the first public-struct, snapshot or core-source-list change, repair its
  compile-time ABI assertions and independent readers/build recipes before
  expanding consumer behavior. Verify that affected group immediately in the
  supported compiler/lint environments; keep final gates at phase close.
- Batch review findings into one owned repair handoff with atomic code commits
  per finding and focused semantic controls. Recheck the changed batch once;
  reopen review only for a new code change, finding or unresolved concern.
  Consolidate evidence updates in the owning report and one metadata commit.
- Finish and commit each task's buildable test inventory and evidence binding
  before expanding the next task in the same harness. Shared setup can land
  with its first user; avoid implementing both tasks and then reconstructing
  intermediate file versions solely to separate their commits.
- New integration failures must identify the failing invariant or stage and
  preserve attributable artifacts. Compare parsed payloads separately from
  variable metadata; format-valid files alone are insufficient evidence.
- Keep repeated fault cases focused on their owning contract. Publication
  controls can reuse a shorter real guest transcript with every output family
  and independent validation while full replay/checkpoint coverage stays in its
  owning test. Measure both compiler environments before changing limits.
  If a narrow delegated diagnosis stops producing evidence, hand it back to
  the orchestrator with exact commands and logs instead of repeating whole
  matrices. A direct-child pass alone does not establish a wrapper's cause.
- After a broad public API, mapped-device or snapshot change, run one early
  full registered fast test inventory after focused repairs. Audit old refusal
  fixtures and evidence-bound helper identities as well as raw readers; a newly
  supported address must not remove the mixed-span refusal proof. Reserve final
  canonical qualification for the reviewed source, and repeat only affected
  checks while repairing diagnosed failures.
- Capture integration stdout/stderr in a unique private log while preserving
  the command's exit status. CTest's LastTest.log and JUnit output are mutable,
  including during discovery; they are not durable failure evidence. If a
  timeout's stage is unknown, use a bounded attributable diagnostic before
  retrying. Preserve limits and controls, and report an unreproduced cause as
  unknown rather than inferring host pressure from elapsed time alone.
- Reuse qualification containers through actual Git fetch and fast-forward
  commands. Verify the exact commit and clean tracked tree before configuring;
  matching overlaid file bytes do not establish an exact-commit build.
- When an inventory or evidence-bound helper changes, locate its active hash
  bindings and mutation needles in one search. Re-establish the bound evidence
  before an identity-only refresh; preserve historical qualification reports.
  Inventory-owning plans also own the existing repeatability claim's inventory
  binding and its named evidence checks, avoiding a late unowned repair.
- Check artifact ownership when adding retained generated files, before the
  final canonical gate. A derived-guest digest is not automatically a binary
  exemption under the scanner's supported manifest fields. Reuse per-test
  owned scratch cleanup on success; retain failures. Move old attributable
  failure evidence outside the final build scan only after owner/provenance
  checks and verified content inventories, preserving it rather than widening
  allowlists or deleting unrelated files.
- Track elapsed implementation, research, review and qualification spans
  separately from measured build/test/CI time. Commit gaps and diff-size token
  proxies do not establish agent activity or API cost. Preserve configured
  models and required independent reviews; add concurrency only for measured
  independent work that the checkout policy permits.

- A later shared timing or device-semantics repair invalidates the earlier
  integration result even when the public layout is unchanged. After its
  focused checks, run the full registered fast inventory once before expanding
  adjacent fault corpora. Include old sampling-boundary assertions; distinguish
  same-tick guest reads from state inspected after the return tick.
- Run artifact preflight in each reused qualification environment before its
  broad gate. Clean native scratch does not establish clean Linux scratch.
  Preserve only attributable old outputs after nonce/provenance checks and
  matching complete before/after inventories; keep scanner policy unchanged.
- Map expensive replay repetitions to distinct evidence obligations. Preserve
  the complete logical-input classes, profiles, handedness, raw/semantic
  assertions and limits. A baseline interleaving proof need not be repeated
  inside a sensitivity run that independently checks the changed event and
  unchanged prefix. Measure the owning case on both environments and retain
  independent review when reducing such work.

- At milestone audit, earlier phase verdicts are evidence at their recorded
  source commits. Later shared-core changes can make their fingerprints stale
  and reduce the SDK's completed projection. Use current canonical regression
  evidence plus independent cross-phase integration to assess current behavior;
  record that projection limit. Do not rerun completed plans or blindly refresh
  historical fingerprints merely to make a phase counter green.
