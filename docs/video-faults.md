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

All three ran with profile `none` and timing policy `video_policy1`. The
generated build identity source SHA-256 was
`11bca039f104e74450197abfe16518662f1d138bad32ebeea0a733fb39576eee`, and the
isolated strict sanitizer CMake configuration SHA-256 was
`43fcb1cd3f372153982557b82b1fd8e5fef3b18e52410ec22aec37c59e20ddd7`.

| CTest control | Copied source change | Intended assertion | Clean / mutant source SHA-256 | Mutant binary SHA-256 | CTest time |
| --- | --- | --- | --- | --- | ---: |
| `video.fault.dma_cost_zero` | `video->dma_busy_until = tick + VIDEO_DMA_TICKS` becomes `video->dma_busy_until = tick` | `video.fault.dma-cost-holds-cpu-until-release` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `d35becc14b197473cc6f71959e0c7b5f530dec266dcc6ec019aff4b03cd9a750` | `519404440dc20d54574ac328ec5376698905c53535b3070a8d8036eddad8a8fe` | 1.95 s |
| `video.fault.dma_request_omitted` | `lynx__video_next_dma` reports no active-row request | `video.fault.active-row-receives-video-ram-grant` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `90035aa8241f49f3996b23cd08eeda659d0efb5da02ab62ca2fc356eefdd0a1a` | `44ca5d87a72a43d0d98947e882a950688dab5ed25f8a1abc2deb71b7a0c34d95` | 1.46 s |
| `video.fault.palette_boundary` | The first captured row skips its palette snapshot | `video.fault.palette-row-boundary-late` | `65ce9ce4c9153df61aeb6b854599da3dade59b5514ef201030dd07efc0c99192` / `11d923ec408c0de149954f31ea622e0bea38b724ee9349747ef85217b495aec0` | `d06a067030e0889bb4bd725612cc0cf655e02ac54dc4c7042676255bf2631bc8` | 1.71 s |

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
`video_policy1`. The generated build identity source SHA-256 was
`aa5e392a4cba1ff742d3d2246ea0a5747c6414bb9d1aaad7df46b3a40f9d086b`, with
the same isolated strict sanitizer configuration SHA-256 listed above.

| CTest control | Copied source change | Intended assertion | Clean / mutant source SHA-256 | Mutant binary SHA-256 | CTest time |
| --- | --- | --- | --- | --- | ---: |
| `input.fault.a_b_swapped` | Swap logical A and B register bits | `input.fault.a-b-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `177052b961cd2a2b1ac7756d9f36e0458acbe2ecdac6d35daaeb3ee133ac3491` | `3f9e845555d0793def5b46ab0dfa7cdfe1ced2cde438ef6b35a296c3404ebf8a` | 2.61 s |
| `input.fault.option1_option2_swapped` | Swap Option 1 and Option 2 register bits | `input.fault.option-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `28d15489ecc2a81424a93c831b5688ea06fec0b289e9b46b464ceef1347904be` | `98f8442660c0b06284cfbc65f4d7f41655b0fea50565945e83bb8f1d11feff2f` | 2.08 s |
| `input.fault.pause_register` | Make the Pause switch register stay clear | `input.fault.pause-register-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `bb78bbc68258c479eda699858fa815ff02cf7fd8135dec138d337f8899f086cf` | `08a50b14c14f734030211f6059da446b50725ed14091cf784d175a9c038fd0aa` | 1.83 s |
| `input.fault.orientation` | Reverse the guest Lefthand direction branch | `input.fault.lefthand-direction-mismatch` | `85bec6ec489227af1e3f700ec1f4da10c42dec3bda4d1da7fc8207386ea56d4b` / `4cc4a2c6307bcca2ebb7885b6b693e432b52ed096ff30dcfba785c7f9456ae20` | `afa5179106da8fe13a91f681a2dc3ba1c1a410b25e84bbc435c85a5c447728c8` | 2.34 s |
| `input.fault.edge_late` | Delay selected joystick/switch CPU grants by one guest tick | `input.fault.sample-edge-at-guest-tick` | `b9e37062e2899aa7b5a6a5b6480cefe276f50229452d4a956a7a96648d567625` / `7111f025650f66503cfaf027f1c2ff48efbb36262c79b3765926ae2a0986671d` | `4183c9211053ac4d9ff20e47fad886bbbf5a48dbf8abea6a3b6be92e64d6da30` | 1.52 s |

Each input probe drives the real guest bus through a pressed read and a later
released read. The sample-edge probe sets the logical A level at the predicted
guest request tick and checks that the next CPU grant is exactly five policy
ticks later; the one-tick mutant fails that named assertion. Existing
`api.input_bits`, `api.input_orientation`, and `api.input_edges` remain the
independent logical-table, handedness, and T-1/T/T+1 edge regressions.

Together the eight controls establish only the Phase 5 device-fault slice of
EVD-05; the requirement remains partial for the Phase 6 audio obligation.
EVD-02 and CON-01 remain pending where their later obligations apply.
