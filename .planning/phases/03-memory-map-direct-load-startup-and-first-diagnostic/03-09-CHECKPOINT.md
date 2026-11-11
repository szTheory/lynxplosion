# Phase 3 Plan 03-09 — Internal checkpoint

**Checkpoint status at commit `64ec67e`:** implementation and initial local/Linux checks were complete; parent review and CI had not run yet. The post-review qualification below supersedes that checkpoint. Phase goal verification is still pending.

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

The implementation and docs cover exclusive macOS/Linux whole-directory publication, fail-closed unsupported behavior, existing path preservation, concurrent publishers, injected write/close failures and output labels. `.planning/state.json` remains untracked and untouched. The initial checkpoint left review and CI with the parent; those gates are now complete on the source head. The final documentation/bookkeeping head still needs CI, and the parent retains Phase 3 goal verification. Keep firmware, copyrighted content, physical hardware, licence, release and publication stops in force.

## Final post-review qualification

The qualified source head is `77218faf00be702c47f13b546763edd7c7d05510`. On macOS arm64, the configured canonical regression gate ran `sh scripts/check.sh` once through `run-with-timeout 600`: exit 0, 120/120 tests, zero declared skips, CTest 32.29 s, full script 89 s, artifact scan 6,478 files / one exactly 512 bytes. Build identity was AppleClang 21.0.0.21000101, CMake 4.4.3, Ninja 1.13.2; the CTest build identity names this exact Git revision.

The same full command passed in a self-contained local clone inside `ubuntu:24.04` arm64 after installing `build-essential cmake ninja-build shellcheck curl git ca-certificates` in the disposable container: exit 0, 120/120, zero skips, CTest 17.48 s, full script 53 s, artifact scan 6,318 files / one exactly 512 bytes. Build identity was GCC 13.3, CMake 3.28.3, Ninja 1.11.1, `Linux-aarch64`, same Git revision. The shared-clone attempt was discarded after its host-only Git object alternate caused the history scanner to fail; no project test failed on that attempt.

Both toolchains passed `det.golden` and `det.golden.self_test` with the unchanged digest `b8bc5a908a4c00be6bb1fc0d5fc8093740b6323ac5fce0cda683ce4e1bad233f`. Both diagnostic builds produced BLL hash `16ecb83e09554082f1d51212bbe7c31fd67d33ba68873bca3441308eec284ba0`; the four exact consumer registrations passed. The inventory remains 120 tests with zero declared skips, and its SHA-256 matches the repeatability claim.

The post-check demo was rebuilt and run again on this source head; the independent checker accepted the 8x8 P3 SNAPSHOT at guest tick 5000, startup A, profile `none`, and the labelled 800-sample placeholder WAV. The temporary output was removed.

GitHub Actions run [42930686204](https://github.com/szTheory/lynxplosion/actions/runs/42930686204) passed on this exact source head. It had one required `check` job on `ubuntu-24.04`, 120/120 tests, zero declared skips; run wall time 69 s, queued about 2 s, checkout 3 s, `scripts/check.sh` 60 s. Inside the check, cc65 build took 11 s, CMake configure/build/test workflow 24 s, artifact scan 1 s and history scan 19 s. There were no duplicated jobs. The artifact scanner printed repeated benign `tr: write error: Broken pipe` messages where its existing `tr | cmp` check stops at the first non-text difference; scan and job both completed successfully. No measured change justified optimizing it.

The plan summary and state/security bookkeeping commit will advance the branch by documentation only and receive a separate exact-head CI run before this handoff completes. No new source fix or full-data CPU-vector lane was added.
