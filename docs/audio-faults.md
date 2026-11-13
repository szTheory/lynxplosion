# Private audio semantic fault controls

These tests check that authored audio assertions catch representative source
defects. They use private copies of the source tree and do not add mutation
switches to production code.

For each `audio.fault.*` case, the harness creates a nonce-owned scratch tree,
copies `core/`, `include/`, the audio test source, and the current generated
build identity, then verifies a manifest of every non-mutated input. It applies
one exact source anchor and requires configuration, compilation, and linking to
succeed. A clean test binary must pass before and after the mutant run with its
source hash, executable hash, and inode unchanged. The private mutant must exit
1 with only its named semantic assertion; build errors, signals, sanitizer
reports, timeouts, or other assertions are failures, not detections. Successful
scratch is removed after its ownership and containment checks. Failed scratch
retains its owner marker for diagnosis.

## Channel arithmetic and ring links

| CTest | Private source fault | Exact assertion |
|---|---|---|
| `audio.fault.lfsr_tap` | Map feedback bit 6 to shift bit 9 instead of bit 10 | `audio.lfsr.tap-map` |
| `audio.fault.integrator_clip` | Clip positive integrator overflow to 126 instead of 127 | `audio.integrator.saturates` |
| `audio.fault.link_a1` | Drop the A0→A1 ring successor pulse | `audio.links.a0-a1` |
| `audio.fault.link_a2` | Drop the A1→A2 ring successor pulse | `audio.links.a1-a2` |
| `audio.fault.link_a3` | Drop the A2→A3 ring successor pulse | `audio.links.a2-a3` |
| `audio.fault.ring_return` | Drop the A3→T1 closing pulse | `audio.links.a3-t1` |

The LFSR and integrator controls use dedicated one-assertion cases so an
arithmetic defect cannot pass as detection by also changing a downstream
channel output. The four link controls each clock one source/target pair and
assert the target count directly. The broader `audio.links` case remains the
positive regression for the complete eight-node ring and its A3→T1 wake path.

The six native fault controls passed individually in the Task 1 selector. The
supporting native positive selector also passed `audio.order`, `audio.capacity`,
`det.audio_paths`, and all four fresh audio/audio2 source observations on
lynx1/lynx2. Those source observations retained their 18-row comparisons and
matched the independent software model. GNU qualification for this source
commit is recorded with the Plan 10 execution summary.

These results establish software fault sensitivity and pinned-source
repeatability only. They do not qualify a Lynx model or establish hardware
accuracy.
