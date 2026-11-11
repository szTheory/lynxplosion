# Lynxplosion project instructions

Lynxplosion is the owner-selected working name for a new Atari Lynx emulator.
Use this file and project-owned GSD records as current policy. Bootstrap
research is dated input; recheck material claims before turning them into
requirements. The name is not trademark clearance.

## Owner direction and workflow

- Use Codex for OpenGSD until the owner requests another runtime switch
  (2026-11-10). The owner switches between Codex and Claude; preserve each
  runtime's model mappings in `.planning/config.json`. Resolve the configured
  model and effort together when dispatching GSD agents.
- Aim for a high-quality, accurate and useful Atari Lynx emulator. A narrow
  first delivery is sequencing, not a permanent ceiling on the goal.
- The owner values broad console coverage, autonomous OpenGSD progress and
  useful lessons carried forward between emulator projects. Treat Lynx as an
  automation-first breadth/learning choice; do not describe it as a proven
  demand-led expansion.
- At every new milestone discussion, inspect current project-owned state,
  roadmap, prior decisions and lessons. Fan out across decision-relevant
  hardware, CPU/bus, graphics/video, sound, input, product, API/host,
  verification, security/licensing, performance and DevOps lenses. Use
  subagents when independent research saves time. Prefer primary and
  authoritative sources, compare options and failure modes, include an
  adversarial pass, and stop when more research no longer changes the
  recommendation or evidence plan. Produce one synthesized recommendation and
  keep a rolling roadmap.
- Keep owner decisions, measured results, source observations, reports,
  inference, recommendations and unknowns distinct. Handoff research is dated;
  independently recheck mutable details and project stops.
- Proceed autonomously on routine, reversible work using the configured GSD
  workflow. Batch unavoidable questions. Ask only when an answer changes
  material scope, rights, authority, risk, cost or an irreversible action.
  Approval of a phase includes its planned implementation and required
  verification, reviews, fixes and bounded retests. Do not turn each command
  into another approval request.
- Preserve explicit stops. This setup does not authorize public publication,
  use or distribution of copyrighted firmware/game content, or physical
  hardware testing. Seek a human decision where required by rights, authority,
  subjective judgment or external/physical evidence.

- Launch automated apps and tests in the background by default so the user
  can keep working on the shared computer. Use foreground only when the
  specific check genuinely requires it, explain that need before activation,
  and bound the foreground interval. Diagnose background failures within
  existing limits; a timeout alone is not a reason to take focus. Apply this
  preference across projects and future sessions.

## Engineering and evidence

- Keep deterministic machine behavior separate from host clocks, filesystems,
  physical controller discovery, A/V presentation and persistence policy.
  Define instance and buffer ownership, input sampling, guest-time bounds,
  reset/unload behavior, errors and refusal postconditions explicitly.
- Keep machine progress/time, logical input, raw video/palette, audio events or
  samples, and host presentation as separate observations. A shared-core hash
  establishes repeatability, not hardware accuracy.
- Verify specific Lynx I/II and CPU behavior profiles. Do not treat “Atari
  Lynx” as one already-specified CPU/model contract.
- Prefer a small, flat dependency tree and clear domain names. A small,
  understandable copied helper can be cheaper than a new dependency; add a
  meaningful dependency when its concrete benefit exceeds its lifetime and
  supply-chain cost.
- Treat BIOS images, commercial ROMs, saves, manuals, overlays and diagnostic
  assets as rights-bearing. Do not commit or redistribute them without
  documented permission. Record fixture source, build recipe, identity,
  license, notices and allowed use. A hash establishes identity, not rights.

## Verification, CI and milestone close

- Automate useful checks as early as they provide trustworthy feedback. Use
  deterministic input, fresh isolated machine state, semantic guest progress,
  exact test inventories and meaningful negative controls. Missing or skipped
  evidence must be visible.
- Establish one documented local command aligned with the fast PR gate. Add a
  CI check only when it catches a distinct recurring failure at a useful
  cost. Measure queue, setup, build and test time before changing caches,
  concurrency or matrix breadth.
- After every CI run, inspect the slowest critical-path jobs, duplicated work
  and low-risk efficiency improvements. Preserve each distinct failure class
  and proof while reducing demonstrated wait or cost.
- Benchmark named workloads and builds with correct outputs held constant.
  Separate emulator throughput, end-to-end latency/resource use and CI wall
  time. Optimize a measured bottleneck.
- At milestone close, keep main green, triage open PRs, update the rolling
  roadmap and lessons, and target a release from the exact tested commit when
  its contents, rights, qualification and authority are ready. GSD milestone
  numbers and software release versions are separate. A release blocked by a
  real gate remains pending with an owner and next action.

## Bootstrap boundary

Read `.planning/bootstrap/START-HERE.md`, `PROJECT-SEED.md` and
`EMULATOR-DNA.md` before project initialization. GSD should create the formal
project, requirements, roadmap and state. Do not create those generated files
or start phase planning as part of bootstrap setup.
