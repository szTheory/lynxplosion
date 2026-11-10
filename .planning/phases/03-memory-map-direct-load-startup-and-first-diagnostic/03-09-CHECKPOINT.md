# Phase 3 Plan 03-09 — Internal checkpoint

**Status:** implementation, documentation and local/Linux qualification are complete for parent review. This is not plan or phase completion. No CI was run and nothing was pushed.

## Commits

| Commit | Work |
|---|---|
| `441605b` | Test-only publication controls, independent consumer parser controls, exact consumer inventory and claim hash. |
| `d851094` | Whole-directory exclusive publication with test-only write/close fault injection. |
| `1d11450` | Make the private-symbol public-boundary probe fail consistently on GCC and Apple clang. |

The RED evidence is in `03-09-RED.json`; the GSD TDD evidence classifier accepted it. The injected write failure failed for the intended product behavior: the old consumer returned success and published output. Raw JUnit output stays in temporary storage; committed evidence uses `PROJECT_ROOT` instead of the checkout path.

## Qualification

| Environment | Command and tools | Result |
|---|---|---|
| macOS arm64 | `sh scripts/check.sh`; Apple clang 21.0.0.21000101, CMake 4.4.3, Ninja 1.13.2, ShellCheck 0.11.0 | Full run: 120/120 tests, zero declared skips; CTest 33.11 s, CTest workflow 34 s, full script 82 s. Artifact scan: 6,478 files, one exactly 512 bytes. After the GCC portability fix, targeted `consumer.public_api` also passed, 1/1 in 0.56 s. |
| Isolated Linux copy | `docker run --rm --platform linux/arm64 -v "$PROJECT_ROOT:/work" -w /work ubuntu:24.04 bash -lc 'apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build shellcheck curl git ca-certificates && sh scripts/check.sh'`; GCC 13.3, CMake 3.28.3, Ninja 1.11.1, ShellCheck 0.9.0 | Full run after the portability fix: 120/120 tests, zero declared skips; CTest 22.40 s, CTest workflow 22 s, full script 74 s. Artifact scan: 6,319 files, one exactly 512 bytes. Targeted `consumer.public_api` passed 1/1 in 0.14 s. |

`det.golden` and `det.golden.self_test` pass with both compilers. The shared repeatability digest remains `b8bc5a908a4c00be6bb1fc0d5fc8093740b6323ac5fce0cda683ce4e1bad233f`; no golden value was changed. The original diagnostic BLL hashes to `16ecb83e09554082f1d51212bbe7c31fd67d33ba68873bca3441308eec284ba0` in both runs.

The fast inventory registers 120 tests. `tests/inventory/fast.txt` and the claim's `inventory_sha256` both equal `1e5eee703b7d5a8009c1cb05fd20625b228418d1eedbd92f18c53f6dc1625654`. The four consumer entries are exactly `consumer.public_api`, `consumer.snapshot_ppm`, `consumer.silence_wav` and `consumer.refusal_controls`.

The documented demo was run after the full check. It rebuilt `first.bll` and matching `first.lbl` with the pinned `ca65`/`ld65`, invoked `build/check/consumer/lynx-consume --content "$demo_dir/first.bll" --symbols "$demo_dir/first.lbl" --out "$demo_dir/output" --profile none`, then ran `build/check/tests/lynx-check-artifacts "$demo_dir/output"`. The independent checker accepted the 8x8 P3 `SNAPSHOT` at tick 5000, startup A, profile none, palette `diagnostic-palette-v1`, and the labelled 800-sample placeholder WAV. Output sizes were 960-byte PPM, 1,644-byte WAV and 384-byte manifest. The temporary directory was removed.

The first Linux attempt stopped before tests because the minimal image lacked CA certificates. The next run exposed GCC accepting the implicit private-symbol declaration as a warning; the probe now makes that diagnostic fatal. The corrected targeted and full Linux checks pass. Only the disposable container received OS packages; no project dependency was added. No full-data vector lane was run.

## Parent handoff

The implementation and docs cover exclusive macOS/Linux whole-directory publication, fail-closed unsupported behavior, existing path preservation, concurrent publishers, injected write/close failures and output labels. `.planning/state.json` remains untracked and untouched. Remaining work belongs to the parent: code and security review, coverage audit, accepted fixes with bounded retests, then exact-head CI and phase closeout. Keep the firmware, copyrighted content, physical hardware, licence, release and publication stops in force.
