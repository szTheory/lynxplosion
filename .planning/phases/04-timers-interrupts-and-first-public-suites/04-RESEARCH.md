# Phase 4 — Implementation Research Synthesis

**Date:** 2026-11-10
**Confidence:** HIGH for current core and exact source identity; MEDIUM for the deterministic model; unresolved for hardware phase/tie behavior. Three independent typed research tracks and root adversarial synthesis produced this record. The canonical decisions are in 04-CONTEXT.md.

## Summary and Standard Stack

Keep C17, integer guest ticks, CMake/Ninja/CTest and the existing pinned cc65 toolchain. No new dependency is indicated. A bounded next-event scan fits the small fixed event-source set; adding a heap or per-tick loop has no measured justification. Reuse posted CPU requests, pure bus decode, the public bounded API, whole-instance refusal checks, exact test inventories and the diagnostic source-build pattern.

## Requirement Guidance

| Requirement | Implementation and proof |
|---|---|
| TIM-01 | Machine owns the only clock and explicit total event order. Test target−1/target/target+1, equal-tick edges, stable sequences, idle skips, overflow and coarse/one-tick/irregular slicing. |
| TIM-02 | One shared free-running divider phase, eight counters and source-backed owned links; DONE/reset-DONE/borrow/reload and count/source/enable changes. Unsupported inputs remain unavailable. Inspect public mapped registers plus independent original stimuli; use source tests as comparison evidence, not a hardware oracle. |
| TIM-03 | Enables gate new latch events; the pending latch drives IRQ independently of later enables. INTSET forces and INTRST clears selected bits. Test masked/unmasked CPU acceptance, before/at/after POLL, CLI/SEI/RTI profile rules and in-flight cycles. |
| TIM-04 | Preserve posted requests while grants are withheld. Test no double completion/write, continued timer progress and wake requests. A private hold seam can demonstrate the minimal model without adding WAI/STP, active Suzy or unsupported public CPUSLEEP claims. |
| EVD-02 | Build a strict tier-A wrapper and actual labels first; generate expectations from exact pinned C/ASM source shape and counts. Sentinel, semantic progress and independent minimum/maximum time guard completion. |
| EVD-03 | Hold/halt the CPU before any test instruction using an internal test seam; prove both suite validators reject, even if RAM starts with oracle-like zero bytes. |
| EVD-04 | Every suite provides guest-executed model evidence, checked against selected profile. Host metadata cannot manufacture the observation. Wrong selection fails its specific assertion. |
| EVD-05 | Permanent named controls for DONE/reset-DONE, independent phase/ordering, model/count/symbol/completion and IRQ behavior; positive controls must succeed before seeded faults count. |

## Hardware Evidence and Corrections

Read the primary source index in 04-TIMER-RESEARCH.md. The [Epyx timer chapter](https://www.monlynx.de/lynx/lynx8.html) supports the timer/divider/reload and owned-link concepts; it leaves incoming timer6 linkage unspecified. The [interrupt appendix](https://www.monlynx.de/lynx/irq.html) directly establishes that nonzero status can cause IRQ, forcing is possible, and disabling a source does not clear pending status. Consequently the scheduler memo's pending/enables shorthand must not become an AND mask in the implementation.

The same sources do not establish a complete software event ordering. The reset notes describe phase uncertainty. Fixed-zero reset phase, timer-before-CPU equal-tick ordering and held reset-DONE behavior are explicit deterministic model/inference choices, not measured Lynx electrical behavior. Document and isolate them; do not change upstream expectations to conceal a mismatch. A source or test conflict remains visible until resolved. Timer4 expiry is not a substitute UART interrupt; audio feedback is not implemented here. Sleep has a documented Suzy-ownership limitation, so TIM-04 is the narrow request/grant seam, with the guest-visible boundary decided explicitly after the fixture-access spike.

## Current Code and Snapshot Integration

`core/cpu.h` posts a request; `core/machine.c` currently grants/prices/performs/completes it and passes zero IRQ lines. `core/bus.c` is the common decoder. New available Mikey registers must still preserve pure whole-range mapped inspection and explicit unavailable behavior elsewhere. `core/snapshot.c` currently uses format 3 and includes guest time/ready time; new timer/IRQ/request state requires format evolution and validate-before-apply cross-field checks. Tighten the inherited arbitrary 255-tick ready span with a reachable cost bound; represent stall time separately. Host watches stay outside guest state and clear only on successful restore. Watch return at grant+1 must not leave earlier same-tick work queued.

Avoid two canonical copies of event deadlines. Derived next candidates can come from serialized device state; if stable event sequences are persisted, define their creation, reuse, exhaustion and restore rules. Invalid, forged, stale-format and impossible states refuse byte-identically. Tests save before/at/after edges, IRQ pending and blocked requests, then compare public observations at a common final tick. Any golden update is declared in the owning plan and justified by independent old-observation checks, not an observer field or weakened oracle.

## Exact Suite and Rights Findings

Use lynx-tests commit `b583dc43d532d3c41563a6fa35420614fbcead89`, archive SHA `a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e`, already in pins.txt. The exact MIT license names Copyright (c) 2025 Ignacio Sanchez Gines. Correct provenance's tag description and record the actual source exposure while keeping O6 draft. Fetch sources/build products to temporary or existing build storage; preserve required notices with fetched/generated local inputs. Existing public-repository authority covers normal original code/metadata commits; release/new artifact-channel/firmware/licence authority is unchanged.

The pinned `cpu` oracle has 18 values including model-specific four-byte tuples. `timers` reserves 18 bytes but has only 16 meaningful expected values and accepts alternatives in two cases. Generate count/shape checks from source rather than copying a manually curated emulator oracle. `timers2` has ten results/debug samples and broad phase/enable-order envelopes; it does not settle all hardware uncertainties. It is optional supporting evidence in this phase, not a newly required suite.

Start with an original wrapper calling untouched assembly, not cartridge Makefiles or TGI/CRT device initialization. Export a post-run done marker and semantic progress/model evidence; validate actual -Ln labels and extents. Prove that the wrapper links and list its required mapped accesses before relying on it. If a licensed runner adaptation is required, preserve the test assembly and record the exact transformation and identity. Do not reinterpret unsupported suite startup as successful boot.

## Validation Architecture

Keep the existing exact fast-inventory gate, nonempty targeted CTests and bound claim hash in the same change as any inventory edits. Use original timer/IRQ tracer programs early and carry each vertical slice through actual CPU/bus behavior. Separate pure bank unit proofs, public CPU-side access proofs and raw upstream comparisons. Completion must survive no-run, timeout, corrupt/missing/duplicate symbol, count drift, wrong model and zero-buffer negative controls. Derive a minimum from counted executed setup/test work, then qualify observed completion margin and the maximum with a real bounded run; record which evidence established each bound.

`sh scripts/check.sh` is the configured regression and local qualification command. Required final qualification is macOS, isolated self-contained Ubuntu 24.04 and exact-head CI, with no hidden skipped inventory. Preserve the existing vector evidence; the 2 GB full-data lane is not a generic timer check. A new relevant CPU concern can justify broader verification. Measure named suite/build workloads before altering concurrency/caches. Headless/background execution is the default.

## Security Domain

The core is local unmanaged C, not a web service. Threat boundaries are untrusted BLL/symbol/source-generator inputs, snapshot state, guest time/counters/sequences, CPU/device ownership and host evidence/temporary paths. Use subtraction-safe bounds, complete preflight before mutation, reachable-state validation, bounded loops, source identity/shape checks and invocation-owned output cleanup. No authentication/session/transport subsystem is added. Plans must register the actual threats and use supported mitigate dispositions; a template's accept label is not owner risk acceptance.

## Planning Risks and First Proofs

1. Untested wrapper/link setup: make a source-build/identity/count tracer the first slice; do not defer this risk to the final integration plan.
2. Timer semantics can become a self-confirming oracle: retain primary observations, inferred policy and pinned-source expectations separately, with exact mutant failures.
3. Same-tick/watch/snapshot prefixes can disagree: include independent event-order observations and split/restore controls before relying on digests.
4. Global zero arrays can fake completion: independent sentinel/progress/model/time/count obligations are mandatory.
5. O1 remains open: report raw observations/software comparisons, never a named-profile suite-pass claim. Preserve other owner stops automatically.

Detailed evidence and alternatives are in the three adjacent cluster memos. No physical test, firmware, commercial content, emulator implementation-source read, suite build or runtime measurement was performed during this discussion research. Actual bounds/link viability remain executable plan work.
