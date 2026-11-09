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

## CI and CD efficiency review

After every CI run (and at release/CD steps): inspect the slowest
critical-path jobs, duplicated work, caching and concurrency, and take the
low-risk wins for speed, iteration time and Actions cost, without losing a
distinct failure class or proof. Measure before changing. Record what was
changed and why in the phase summary so the review is not repeated.
