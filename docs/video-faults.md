# Video and input copied-source fault evidence

The controls in `tests/video/fault_controls.cmake` mutate a unique anchor in a
nonce-owned copy of the current core source. They never modify production
source. Each child is built from the copied current sources with the strict C17
warnings and AddressSanitizer/UndefinedBehaviorSanitizer settings, then run
against the independently named API probe. A control passes only when the
mutant exits exactly 1 at its one intended assertion and fresh clean API
processes pass before and after it. Successful scratch is removed after
containment and owner-marker checks; failures retain their scratch for review.

## Phase 5 video controls

All three ran with profile `none` and timing policy `video_policy1`. On the
final combined eight-control verification, the generated build identity
source SHA-256 was
`44245a60bc737a8169457b8e7620c17f81d5ba659c6ce4da06f4d3ea2d80246f`, and the
isolated strict sanitizer CMake configuration SHA-256 was
`43fcb1cd3f372153982557b82b1fd8e5fef3b18e52410ec22aec37c59e20ddd7`.

| CTest control | Copied source change | Intended assertion | Clean / mutant source SHA-256 | Mutant binary SHA-256 | CTest time |
| --- | --- | --- | --- | --- | ---: |
| `video.fault.dma_cost_zero` | `video->dma_busy_until = tick + VIDEO_DMA_TICKS` becomes `video->dma_busy_until = tick` | `video.fault.dma-cost-holds-cpu-until-release` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `d35becc14b197473cc6f71959e0c7b5f530dec266dcc6ec019aff4b03cd9a750` | `12b798969d0dfb9a61f3ee0443fb248cba1e6633cbe8a0f64a3b568ce788592a` | 1.42 s |
| `video.fault.dma_request_omitted` | `lynx__video_next_dma` reports no active-row request | `video.fault.active-row-receives-video-ram-grant` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `90035aa8241f49f3996b23cd08eeda659d0efb5da02ab62ca2fc356eefdd0a1a` | `98d5d16c3a7410c7bc1056d3d463a6e3e80cf31d15808d6acdf53cccb74acce9` | 1.41 s |
| `video.fault.palette_boundary` | The first captured row skips its palette snapshot | `video.fault.palette-row-boundary-late` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `11d923ec408c0de149954f31ea622e0bea38b724ee9349747ef85217b495aec0` | `0113d5c645036345a659b3049842a479932260fa0665aefc381a7272c718dd32` | 1.43 s |

The DMA-cost probe compares the complete raw pixel, palette, and line-timestamp
buffers against the expected frame, so the timing-only mutation leaves raw
output unchanged. The request probe also asserts that an idle video state has
no DMA request. Existing `api.video_bus_edges` and `api.palette_boundary`
regressions passed, covering the established tie-order and boundary controls.
An absent `FAULT_ID` was rejected as required, and an unknown selector was
rejected before any scratch was created.

## Remaining evidence scope

## Phase 5 input controls

The five controls below also use profile `none` and timing policy
`video_policy1`, with the same final build identity and isolated strict
sanitizer configuration SHA-256 listed above.

| CTest control | Copied source change | Intended assertion | Clean / mutant source SHA-256 | Mutant binary SHA-256 | CTest time |
| --- | --- | --- | --- | --- | ---: |
| `input.fault.a_b_swapped` | Swap logical A and B register bits | `input.fault.a-b-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `177052b961cd2a2b1ac7756d9f36e0458acbe2ecdac6d35daaeb3ee133ac3491` | `2eb39fddb10a3574b0c57bfc998d8ae847a76e0670ceb94cba137a1655c9418c` | 2.74 s |
| `input.fault.option1_option2_swapped` | Swap Option 1 and Option 2 register bits | `input.fault.option-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `28d15489ecc2a81424a93c831b5688ea06fec0b289e9b46b464ceef1347904be` | `68acfc24c2bef6a66c455a5e8414fa41c705f056f381c9d118060e8dec00ee13` | 2.47 s |
| `input.fault.pause_register` | Make the Pause switch register stay clear | `input.fault.pause-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `bb78bbc68258c479eda699858fa815ff02cf7fd8135dec138d337f8899f086cf` | `6ae9e3dfeb3120958c87f3ea536602adc9df5d34387b898ebd142ab9bbfcd9c9` | 2.21 s |
| `input.fault.orientation` | Reverse the guest Lefthand direction branch | `input.fault.lefthand-direction-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `4cc4a2c6307bcca2ebb7885b6b693e432b52ed096ff30dcfba785c7f9456ae20` | `e572e1e4b1b0896e8e61235414892184b27e4526320f7f73a968a3be5e1a7a30` | 1.97 s |
| `input.fault.edge_late` | Delay selected joystick/switch CPU grants by one guest tick | `input.fault.sample-edge-at-guest-tick` | `b9e37062e2899aa7b5a6a5b6480cefe276f50229452d4a956a7a96648d567625` / `7111f025650f66503cfaf027f1c2ff48efbb36262c79b3765926ae2a0986671d` | `9e0a13743ea401f6767220f478c39b5cc4962e80f1b6dd84a28899254ef7cad5` | 1.39 s |

Each input probe drives the real guest bus through a pressed read and a later
released read. The sample-edge probe sets the logical A level at the predicted
guest request tick and checks that the next CPU grant is exactly five policy
ticks later; the one-tick mutant fails that named assertion. Existing
`api.input_bits`, `api.input_orientation`, and `api.input_edges` remain the
independent logical-table, handedness, and T-1/T/T+1 edge regressions.

All eight controls passed in 4.13 seconds of CTest wall time on the final
combined run. Together they establish only the Phase 5 device-fault slice of
EVD-05; the requirement remains partial for the Phase 6 audio obligation.
EVD-02 and CON-01 remain pending where their later obligations apply.
