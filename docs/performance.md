# Fixed A/V workload baseline

This baseline measures one source-built, input-responsive guest through the public A/V API. It is a software throughput and repeatability observation for a named build; it does not measure hardware accuracy or represent performance on other machines.

## Workload identity

| Item | Value |
| --- | --- |
| Tested source/build identity | `93959ca26a0017a0130ffa0abd71856ffad8262c` |
| Build mode and compiler | Debug, AppleClang 21.0.0.21000101 |
| Host | Darwin 25.6.0, arm64; exact CPU model unavailable because the host metadata query was denied |
| Clock | `CLOCK_MONOTONIC`, nanosecond report precision |
| Guest fixture | Project-authored `fixtures/diagnostic/av_probe.s`, built with pinned ca65 (`--cpu 65SC02`) and ld65; 220-byte BLL, SHA-256 `79cf9d23aec06364f9a9704f90c8bdb72ae89e3aea6ac99a311a906a55802774` |
| Input timeline | 12 fixed input events, 127 bytes, SHA-256 `24220e8011cf154c552ae2eee954e2f22c8f18d27697c66a7df689093c0de83b` |
| Profile and policies | `none`, `video-policy1-unverified`, `digital-1` |
| Bounded guest work | Window limit 1,253,985 ticks; actual successful progress 1,248,256 ticks across 34 `lynx_advance_av` calls |
| Captured extents per run | 22 frames, 3,120 PCM frames, 1,706 audio events, 12 applied inputs, 13 output streams |
| Combined output identity | SHA-256 `ed77c90bbc0287adc4fd9f672bf292c7b657047096df9e2c8e8a2d66380971f2` |

The window limit is a bound. The measured completion tick is accumulated public progress, and each report validates `guest_ticks == guest_endpoint_tick <= guest_window_bound_tick`. All five fresh runs matched the fixture, timeline, profile, policies, work extents, every stream size and digest, and the combined output digest. The test also checked the published bundles with the independent A/V artifact checker.

## Five native samples

The API interval sums host-clock durations immediately around the public `lynx_advance_av` calls. It excludes fixture and timeline parsing, checkpoint-sidecar prefix hashing, frame encoding, output assembly, checksums, and filesystem publication. No clock or filesystem work is performed in the emulated core.

| Run | API advance (ns) | Frame capture/encoding (ns) | Stream assembly (ns) | Publication (ns) | Full consumer (ns) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 164,738,000 | 85,123,000 | 195,187,000 | 4,441,000 | 2,697,818,000 |
| 2 | 218,932,000 | 114,954,000 | 253,738,000 | 4,787,000 | 3,639,186,000 |
| 3 | 199,861,000 | 102,967,000 | 194,940,000 | 4,115,000 | 3,099,807,000 |
| 4 | 182,802,000 | 94,632,000 | 197,890,000 | 3,817,000 | 2,925,093,000 |
| 5 | 184,878,000 | 95,348,000 | 219,243,000 | 2,740,000 | 2,955,953,000 |

| Measure | Median | Minimum–maximum |
| --- | ---: | ---: |
| Public API advance time | 184,878,000 ns | 164,738,000–218,932,000 ns |
| Guest ticks per API second | 6,751,782.256 ticks/s | 5,701,569.437–7,577,219.585 ticks/s |
| Frame capture/encoding | 95,348,000 ns | 85,123,000–114,954,000 ns |
| Stream assembly and report hashing | 197,890,000 ns | 194,940,000–253,738,000 ns |
| Filesystem publication | 4,115,000 ns | 2,740,000–4,787,000 ns |
| Full consumer operation | 2,955,953,000 ns | 2,697,818,000–3,639,186,000 ns |

The five-run measurement is from the exact source/build identity above. Throughput is calculated per run as actual guest ticks divided by the summed API advance duration; the table reports the median and the observed minimum/maximum. The full-consumer duration includes host work through publication and must not be read as core throughput.

## Parser evidence and CI timing

`consumer.av_benchmark_controls` builds one separate five-run real baseline, validates its 13 streams and identities, then derives malformed records from that immutable corpus. Those named parser controls reject missing, duplicate, extra, incomplete, unlike, non-progressing, invalid-duration, overflow, and nonmonotonic records. The mutated records are parser evidence; they are not benchmark samples.

CI queue, setup, build, CTest, hygiene, and history timings are a separate category and remain pending the Plan 12 final qualification. That qualification should add or link those measurements without combining them with this local guest-throughput result. No performance optimization was made for this baseline.
