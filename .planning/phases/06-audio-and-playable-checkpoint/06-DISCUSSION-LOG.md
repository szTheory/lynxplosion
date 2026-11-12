# Phase 6: Audio and Playable Checkpoint - Discussion Log

> **Audit trail only.** Downstream work consumes CONTEXT.md, not this log.

**Date:** 2026-11-12
**Phase:** 06-audio-and-playable-checkpoint
**Areas discussed:** sound/timing, native A/V ownership and restore, suite/fault/performance evidence

Three independent configured advisor researchers compared the decision clusters.
The owner's existing autonomous delegation applies; no new rights, authority,
cost or physical-evidence decision was required. Recommendations were synthesized
once into 06-CONTEXT.md under discuss --auto.

## Sound behavior and timing

| Option | Consequence | Selected |
|---|---|---|
| Shared bounded timer/audio event graph | One total order for the complete link ring; ring saturation needs explicit policy | Yes |
| Separate independent banks and bridge | Focused modules, but adds re-entrant event and lost-pulse risk | Module helpers only |

**Delegated choice:** shared ordered graph, with focused audio helpers allowed.
Keep undocumented timing explicitly provisional; preserve strict timer failures.

## PCM rate and delivery

| Option | Consequence | Selected |
|---|---|---|
| 40 kHz caller spans | Exact 400-tick lattice; integer WAV rate; no retained FIFO | Yes |
| 48 kHz caller spans | Common host rate, but a 1000/3-tick sample phase is unnecessary for the file consumer | No |
| 32 kHz caller spans | Simpler 500-tick lattice, lower bandwidth | No |
| Core FIFO | Decouples delivery but adds queue/back-pressure/restore surface | No |

**Delegated choice:** 40 kHz stereo signed-16 plus separate channel events.
The 48 kHz advisor recommendation was considered; host-rate compatibility does
not outweigh a simpler exact guest-tick contract for this scope. Rate and mix
are software observation policy, not hardware claims.

## Suite, stereo and benchmark evidence

| Option | Consequence | Selected |
|---|---|---|
| Extend existing pinned-source evidence runner | Reuses identity/model/completion/bounds controls | Yes |
| Separate audio runner | Duplicates provenance and control rules | No |
| Defer audio suites as skipped | Hides implementable work; no demonstrated external blocker | No |

**Delegated choice:** source-built assembly wrappers for both audio suites,
18 source-derived rows each, preserving the audio2 alternative. Both avoid
SERCTL. Neither tests MSTEREO; add original project-authored stereo coverage,
as the roadmap already requires. Keep page-mode's exact skip and timer's
strict disagreement visible. Baseline a fixed homebrew workload without
optimization, holding separate stream digests constant.

## The agent's Discretion

Routine API/module names, integer normalization, bounded ring details and task
breakdown remain implementation choices inside the captured contracts. Source
research and targeted integration checks precede final qualification; no new
approval flow is introduced.

## Deferred Ideas

Physical qualification, host resampling/presentation, UART, shared LibFuturo
contract, optimization, licensing and release retain their existing boundaries.
