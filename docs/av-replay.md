# Public A/V replay

`lynx-consume --mode av-replay` replays the existing canonical host input
timeline through the public emulator API and records video, palette, line-tick,
audio-event and PCM observations. It uses no firmware and does not claim
hardware accuracy.

```sh
lynx-consume --mode av-replay --content guest.bll --symbols guest.lbl \
  --timeline input.timeline --out new-output-directory --profile none
```

## Timeline and guest-time ordering

The input file uses the same strict, versioned grammar documented in
[`input-replay.md`](input-replay.md): `LYNX-INPUT 1` followed by LF-terminated
`<tick> <mask>` rows. The parser reads and hashes the complete canonical byte
file before creating output. It allows at most 128 KiB and 4,096 events, masks
from 0 through `0x1ff`, and strictly increasing ticks. The timeline must
contain a rising A edge so the source-built probe can show a released/pressed
response.

Replay advances only to the next declared edge, using bounded public
`lynx_advance_av` calls. A frame or output-span stop is handled with fresh
caller-owned spans; returned sample ordinals, event/frame counts, and guest
ticks are checked before continuing. A full-span run can hold at most 3,135
PCM frames and 32,768 audio-event records; the complete replay retains at most
32 rendered frames. The positive 4,096-row timeline boundary control replays
the dense one-tick edges through the final post-edge checkpoint and retains its
complete A/V bundle. The required scenario ends at tick 1,193,985; adding its
60,000-tick final-frame window gives a floor of 3,134 sample ordinals at 400
ticks per sample. One additional caller slot lets the final frame advance
without dropping the last output span. The consumer rejects timelines that
exceed this fixed bound. It observes through the first complete frame ending
after the last input edge, with a 60,000-tick bound for that final interval.

If a frame ends exactly at an input tick, that frame belongs to the state
before the edge. The host then applies the new mask at that exact guest tick.
Coincident sample and input ticks follow the core's half-open advance contract:
the sample at the target tick remains pending until the next call. Edges one
tick before or after a sample or frame boundary therefore remain distinct.

## Checkpoints and restore ownership

The replay checkpoint is an in-memory host sidecar record paired with the
existing exact-build public core snapshot. It is not a persisted checkpoint
file and does not promise restartable replay across processes. One machine
advances the transcript; a second machine is only the restore target used by
the conformance path. After a validated restore the roles swap, so the event
is applied once and neither output prefix nor input edge is replayed.

The sidecar records schema version, profile, timing and audio policies, sample
format, content digest, core build digest, complete timeline digest and byte
length, current file offset, event cursor, current mask, pre-edge or post-edge
phase, guest tick, next sample ordinal, applied-input byte/count offset, PCM
append offset, audio-event append offset, frame append offset and last frame
sequence and guest-progress record count (the frame count). It also binds the
complete captured applied-input, guest-progress, PCM, event, raw-frame and
rendered-frame prefix with a canonical prefix digest.
Counts are append offsets: restoring resumes after the owned prefix, never at
its beginning.

Before the core snapshot is restored into the target machine, the host checks
every identity, extent, cursor, offset, phase, guest-tick and prefix field
against the current replay. The public guest clock is checked again after
restore. A mismatch is rejected before target restore or output publication;
the caller's output directory must still be absent. Tests compare complete
restored bundles with an uninterrupted test-only control and exercise
short-span output, a zero-output prefix at tick zero, a single final-edge
suffix, and bad timeline offsets, identity and prefix digests. A watched guest
write is also cleared through the public watch API, resumed, and compared
byte-for-byte with the uninterrupted bundle.
These checks establish same-build repeatability, not accuracy or durable
checkpoint persistence.

## Artifact schema 1

Successful publication creates the complete bundle atomically in a new
directory. The manifest begins with `schema_version = 1` and
`artifact = AV_TRACE`. It binds the selected profile, startup policy, timing
and audio policy, sample rate/channel/bit format, core build ID, content hash,
canonical input hash and byte length, event count and first edge, final input
tick, frame count, sample ordinal range, PCM/event counts, byte lengths and
SHA-256 digests for each distinct stream. `frames_raw_bytes` and
`frames_ppm_bytes` describe complete concatenated streams; the `released_*`
and `pressed_*` size/hash pairs describe the preview copies. `pcm_bytes`,
`wav_bytes`, and `events_bytes` bind their complete file extents.
`input_applied_count/bytes/sha256` and
`guest_progress_count/bytes/sha256` bind the two host/guest response streams.
`input_sha256`, `timeline_sha256`, and `timeline_stream_sha256` all identify
the exact `input.timeline` bytes. Schema 1 contains no output path or host
clock value.

| File | Contents |
| --- | --- |
| `input.timeline` | Exact canonical timeline bytes. |
| `input.applied` | `LYNX-APPLIED-INPUT 1`, then one record per successful `lynx_set_input`: cursor index, source byte offset, guest tick, prior mask and newly applied mask. |
| `guest.progress` | `LYNX-GUEST-PROGRESS 1`, then one record per captured frame: sequence and tick range, applied mask, guest JOYSTICK/SWITCHES, progress counter, done/error flags, audio-start flag, guest display marker and handedness. |
| `frames.raw` | Concatenated `LXAVRAW1` frame records: row count, sequence, flags, then each row's 80 pen bytes, 32 palette bytes and 64-bit little-endian line tick. |
| `frames.ppm` | Concatenated rendered PPM frames, retaining each frame's header and guest-tick metadata. |
| `released.raw`, `released.ppm` | First complete frame with the probe's released marker. |
| `pressed.raw`, `pressed.ppm` | First later complete frame with the probe's A-pressed marker. |
| `audio.pcm` | Interleaved signed 16-bit little-endian stereo samples. |
| `audio.wav` | RIFF/WAVE PCM wrapper over the exact `audio.pcm` payload. |
| `audio.events` | `LYNX-AUDIO-EVENTS 1` channel transition transcript in guest-tick order. |
| `av.manifest` | Versioned policies, identities, extents, counts and stream digests. |

The PPM conversion uses the captured palette and pen nibbles; it does not rotate
or otherwise transform guest output. WAV samples use the digital audio policy
at 40,000 Hz, stereo, signed 16-bit. This is a deterministic software format,
not a measured physical output rate or amplitude.

The source probe stores its poll count in the low six bits of result byte 7,
with done and error in bits 6 and 7. `guest.progress` records those fields
separately, and records result byte 8 as its fixture-owned audio-start flag.
That flag is not misreported as progress completion. Input applied records
are written only after the public setter succeeds at the recorded guest tick.

The source-built `av_probe` fixture supplies the input-responsive guest
markers and audio writes. Its provenance and permitted diagnostic use are
recorded in `fixtures/manifests/av-probe.manifest`; its response is project
test evidence, not a substitute for an accuracy oracle.

The `consumer.av_replay` test driver first checks every manifest size and
SHA-256 against the real sequential bundle, then compares every file, including
the manifest, with a test-only pair of fresh instances advanced in alternating
A/V spans. It then runs the independent C checker over each positive bundle;
that checker validates syntax, stream extents, event ordering and guest/frame
semantics, while the driver performs the SHA-256 comparisons. The normal
`lynx-consume` process still produces one transcript. Private-copy mutation
controls go directly to the semantic checker and assert their specific failure
stage for absent guest output, over-bound refusal, mismatched input
application, silent or pre-input-stale PCM, header-only WAV/frame streams,
extra/truncated WAV data, incorrect sample ordinals, altered frame metadata,
bad guest markers and reordered audio events. Those manufactured mutations
are parser controls, separate from actual guest evidence. These checks
establish reproducible same-build output and parser behavior only.
