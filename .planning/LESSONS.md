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
