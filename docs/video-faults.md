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

The five independent input and sample-timing controls are added by Task 2.
These video controls establish only the Phase 5 device-fault slice of EVD-05;
the requirement remains partial for the Phase 6 audio obligation. EVD-02 and
CON-01 remain pending where their later obligations apply.
