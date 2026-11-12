# Timer video-policy revisit

## Frozen preconditions

Frozen at `2026-11-12T11:54:02Z`, before any VIDEO_POLICY1 timer observation. The
source, oracle, wrapper, object layout, profile input, and initial timer counter
remain the pinned Phase 4 inputs. This is a deliberately selected software
environment, not a claim that the upstream CRT executes these exact writes or
that VIDEO_POLICY1 is hardware-qualified.

| Input | Frozen value | Basis and limitation |
| --- | --- | --- |
| Timer 0 backup/control | `9e` / `18` | Upstream CRT-derived 60 Hz row cadence values recorded in Phase 5 research. |
| Timer 2 backup/control | `68` / `1f` | Upstream CRT-derived frame cadence values recorded in Phase 5 research. |
| PBKUP | `29` | Upstream CRT-derived value; the current model uses it as a row address step. |
| DISPCTL | `01` | Display DMA enabled in the supported model. The upstream CRT value is `0d`; the current model refuses its other mode bits, so those remain unsupported evidence. |
| DISPADDR | `2000` | Fixed zero-filled scratch display buffer, selected before observation; it is not inferred from the upstream CRT. |
| MAPCTL / page latch | `0c` / reset value `00` | Same selected byte as the pinned suite wrapper; page-mode policy remains governed by VIDEO_POLICY1 and the model's opcode eligibility. |
| Timing / refresh policy | `video-policy1`; refresh first due at tick `256`, then every `256` ticks | Project policy only; refresh DMA is coalesced and consumes the configured five-tick bus grant. |
| CPU bus cost / video grants | flat5 CPU cycles; five ticks per video byte and refresh grant | Project policy values, recorded separately from source-backed register choices. |
| Timer counter initialization | zero-page byte `$01` starts at `00`; wrapper's zero-page clear is observed before the timer source interval | Counter seed remains zero. Guest `INC $01` writes are observed with the private address watch. |
| Start phase / ownership | guest tick `0`; all guest and host API calls serialized | No host clock, external controller, or concurrent access participates. |

The new lane will use fresh machine instances for Lynx I and Lynx II, the same
pinned timer source/oracle/wrapper bytes and linked layout, and no pending
descriptor. Its comparator is strict across all 16 meaningful rows. The legacy
flat5/no-DMA lane and its exact D-10 descriptor remain a separate evidence
path. A result mismatch in this new lane is reported as a mismatch, never as a
new accepted exception.

## Baseline and observation results

The unchanged legacy gate ran before the new observations and passed for both
profiles, including strict comparison controls and the independent 64-phase
cascade proof. It continues to validate 16 source rows, 15 agreements, and the
single D-10 row `result 14 0c EXPECT(0d) source-mismatch` under flat5, no
display DMA, counter zero. That exact descriptor remains bound only to that
legacy environment.

The frozen video-policy run used `suite.timers.video_revisit` on fresh Lynx I
and Lynx II instances. The strict exit was `1` on each profile, with 15 source
agreements and one mismatch. Both profiles produced the same complete row set:

| Index | Lynx I | Lynx II | Pinned source set | Status |
| ---: | ---: | ---: | --- | --- |
| 0 | `08` | `08` | `08` | agree |
| 1 | `7f` | `7f` | `7f` | agree |
| 2 | `00` | `00` | `00` | agree |
| 3 | `08` | `08` | `08` | agree |
| 4 | `00` | `00` | `00` | agree |
| 5 | `01` | `01` | `01` | agree |
| 6 | `08` | `08` | `08` | agree |
| 7 | `00` | `00` | `00` | agree |
| 8 | `04` | `04` | `04,05` | agree |
| 9 | `08` | `08` | `08` | agree |
| 10 | `00` | `00` | `00` | agree |
| 11 | `37` | `37` | `35,36,37` | agree |
| 12 | `00` | `00` | `00` | agree |
| 13 | `01` | `01` | `01` | agree |
| 14 | `0f` | `0f` | `0d` | **mismatch** |
| 15 | `08` | `08` | `08` | agree |

The watched zero-page byte `$01` began at `00`, the wrapper's zero-page clear
was observed, and the source interval performed 15 increments. At Test6 return,
the watched counter and result row 14 were both `0f`. The run did not seed the
counter or alter timer phase, policy, layout, oracle, or source alternatives.
The row is not admitted by the legacy descriptor; the video-policy record has
`pending_descriptor=none` and `strict_comparison=all-16-source-rows`.

### Identity and run details

The two observations use the same source/tool/object/layout identities:

| Identity | Value |
| --- | --- |
| Lynx Tests revision | `b583dc43d532d3c41563a6fa35420614fbcead89` |
| Lynx Tests archive SHA-256 | `a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e` |
| cc65 archive SHA-256 | `d3e4bfa57c5a40dd386d1a38b27fffad3bbc296950303f3c9017211465f5c8a1` |
| Timer tests source SHA-256 | `dde10583ae050aba1dcd72f3e0b92cdf2d38d5037c4380c5e8864f2c81702e1f` |
| Timer main source SHA-256 | `b89d4207e837fb4a85b1dcf6b62ffb56c8e62317292b02deeafc22ac02bada3a` |
| Wrapper SHA-256 | `fa0973d355eeba6311d9540f3d2ec7d429f6991e1e5641fb36ed7af6c2749e6c` |
| Oracle SHA-256 | `08b5a4f48caf0f92e7bc177d9f3a6ac8ff68d757b57b9bc346aad8bd4c698b4f` |
| Build record SHA-256 | `43f74b8cff5f754938b769504423b1bcc3fc9717a7beba8e60ef261fb560d3b7` |
| Linked BLL SHA-256 | `d766a24031246ad166edc6b417e4d30a841e18dda5115eae201804847ef96c79` |
| Linked labels SHA-256 | `b1423949c03badc1158b5cacfb67af146cbe18228cc73f156b11dd548776efa3` |
| Layout manifest SHA-256 | `177acdcf8690ed73cd303aacf608b4c5aaaf1b69d5a27e472a08aba673b8121f` |
| Suite builder SHA-256 | `1399702fdb178bbd1d4ff662c6d704db90c92a58a770628f99784e7dae68c893` |
| Oracle generator SHA-256 | `894866d4c91c2c422a8f3199b98aee026234d8db36351f7a2ed5701e4883eb78` |
| Core build ID | `git=dd5ccc59f3bf32be4d62873284a2a71aa680eafd+dirty cc=AppleClang-21.0.0.21000101 sys=Darwin-arm64` |

The Lynx I run completed at guest tick `366080`; the Lynx II run completed at
`366101`. Initial refresh was due at tick `256`, at a `256` tick period. The
wrapper itself clears zero page and sets MAPCTL `0c` but does not initialize
display registers; this lane applies the frozen host setup before guest tick
zero. Timer0/2, PBKUP, display, MAPCTL/page, final timer state, refresh phase,
bus grant costs and ownership are retained in the private per-profile run
records and checked by CTest. Raw records remain in invocation-owned ignored
build scratch and are not committed.

The hardware-oriented evidence remains incomplete: the selected project
environment is not a hardware result, and the upstream CRT's `DISPCTL=0d` is
unsupported by the current model. O1 and EVD-02 therefore remain open. The
observation is complete and useful for the revisit, but it is not source
agreement or hardware qualification.

Verification command:

```sh
cmake --preset check && cmake --build --preset check && ctest --preset check --no-tests=error --output-on-failure -R '^suite\.timers\.(video_revisit|observation\.(lynx1|lynx2)|strict_comparison_controls|cascade_model)$'
```

The five focused tests passed. The new test passes only after confirming the
exact 16-row records and the expected strict mismatch on each profile.

