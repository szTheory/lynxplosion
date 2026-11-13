# Phase 7 research: Suzy math and register mirrors

Date: 2026-11-13. Status: implementation-ready, with named software policies
for incomplete hardware evidence. The typed researcher turn was interrupted
after failing to return an artifact; the orchestrator completed this bounded
primary-source synthesis. The independent integration advisory is retained.
No emulator implementation source was opened or copied. The public author
comment below links GPL implementation code; that link was deliberately not
followed. No hardware, firmware, binary asset or dependency was used.

## Recommendation

Use a private `core/suzy.c` instance-owned device with 48 backing bytes,
read/write/availability helpers, latched operation inputs, one completion
deadline and fieldwise validation. CPU and public mapped inspection share
decoding; inspection is pure. Math completes as an internal event before a
same-tick CPU result read. It never claims the sprite/video bus or modifies
CPU grant/page ownership. Preserve input sampling in `core/input.c` and compose
SPRSYS exactly once at the machine adapter.

Keep full deferred SUZ-02 separate from Phase 7's SMR requirements. Arithmetic
and register comparisons must pass; all timing results remain captured and
strictly compared without turning a subset pass into whole-suite agreement.

## Primary evidence

Original Epyx [math chapter](https://www.monlynx.de/lynx/lynx9.html):
16x16 product, optional 32-bit accumulation, unsigned 32/16 quotient and
remainder; zero divisor yields quotient FFFFFFFF. Low writes clear paired
high bytes. A starts multiply; E starts divide. Signed conversion occurs on
explicit upper-operand writes, so a low write does not refresh the sign latch.
The text identifies positive 8000 and negative zero sign quirks, an unspecified
remainder defect, unreliable math unsafe-access indication, and non-reentrancy.
It gives 44/54 tick multiplication and 176+14N division timing, N being leading
divisor zeros. These are specification observations, not our hardware results.

Original [address appendix](https://www.monlynx.de/lynx/hardware.html):
SCB low-byte writes clear their paired high byte. The physical register table
identifies FC00..2F and named math ports at FC52..57, FC60..63 and FC6C..6F.
SPRSYS writes select signed/accumulate and clear unsafe; its read bits report
busy, warning, carry and unsafe, alongside existing input orientation.

The test author's [carry explanation](https://github.com/drhelius/lynx-tests/issues/1#issuecomment-3750852033)
relates carry to nonzero signed negation, accumulator overflow, division
remainder and zero divisor. This is first-party explanatory prose, not a
physical trace. Do not follow its implementation link. Its interpretation
matches the pinned tests' covered carry cases but does not establish all
unmeasured hardware behavior.

Primary pages were rechecked directly with bounded HTTPS reads after the web
reader could not retrieve them. Private raw HTML is retained in the Phase 7
temporary research directory. Do not commit copyrighted manual text.

## Exact pinned test observations

Inputs stay at `pins.txt`'s existing lynx-tests and cc65 revisions. Inspect
`build/pins/lynx-tests/{math,memio}/{tests.s,main.c}`; do not change or repin.

- Math has 8 tests, 18 result bytes. Tests 1..7 cover unsigned multiply,
  accumulation/overflow, signed/stale sign, 8000 quirk, divide, exact divide
  and zero divisor. Their meaningful pairs occupy offsets 0..13. Test 8 is
  timing at 14..15; 16..17 are reserved. Expected meaningful pairs are FF/00.
- Test 8 waits on Timer2 VBlank and uses Timer6 for elapsed observations. It
  expects multiplication delta 5..7 and division delta 16. Those measurements
  include CPU polling and bus costs; they cannot justify fitting math latency.
- Math Test5 deliberately comments out its remainder-low-byte failure branch;
  Test6 still checks zero remainder. Do not claim a general hardware remainder
  oracle from the public suite. Add independent arithmetic policy vectors.
- Math checks unsafe set after operations. Signed Test3 explicitly verifies
  that a MATHD-only write preserves a stale sign. Test4 expects 8000*2 positive.
- Memio has 3 tests and 18 result bytes; meaningful groups have 4,4,5 bytes
  (offsets 0..12). Preserve and label the five reserved bytes. Its source
  initializer also contains unused entries; derive counts from group metadata.
- Memio first checks existing palette behavior, then base FC00..2F storage and
  FC40..6F aliases. Ordinary mirror writes exclude named math command ports.
  Named mirror writes must have math side effects, while direct base writes
  must not accidentally trigger an operation. Paired-byte clearing still
  applies to ordinary low writes.
- `_run_tests` in math calls all eight tests and re-enables IRQ on return;
  fixture startup must provide Timer0/2 VBlank progress and mask after return
  using the established suite wrapper pattern. No CRT/TGI/ROM is needed for
  RAM-result capture. Do not silently omit timing or fake suite completion.

## Concrete register and arithmetic design

Expose FC00..2F and FC40..6F with backing index `address & 0x3F`; indices
30..3F are outside this backing. Keep FC30..3F, FC70..7F and other unsupported
Suzy addresses unavailable. Preserve JOYSTICK/SWITCHES handling. Named ports:

| Port | Address | Meaning |
| --- | --- | --- |
| D,C,B,A | FC52..55 | CD and AB operands; explicit A write starts multiply |
| P,N | FC56..57 | unsigned divisor NP |
| H,G,F,E | FC60..63 | dividend/product; explicit E write starts divide |
| M,L,K,J | FC6C..6F | accumulator/remainder; M write clears math warning |

Low-byte auto-clear is a storage side effect, not an implicit upper-port
command/sign conversion. In signed mode, explicit C/A writes convert their
16-bit operand and latch sign, including the documented zero/8000 quirks.
Use unsigned C arithmetic, explicit sign/magnitude conversion and uint64_t
for accumulate carry; avoid signed overflow, implementation-defined casts and
undefined shifts. Division is unsigned regardless of multiply sign controls.
Use the author explanation for covered carry behavior and record its origin.

SPRSYS must compose math read bits with existing lefthand state, and validate
all control bits before modifying either device. Signed, accumulate, clear
unsafe and lefthand are in scope; bit0 is documented as write-ignored. Reject
unsupported sprite controls before any mutation rather than adding a sprite
state machine. Public inspection must never set unsafe, start/finish work or
change guest time. Keep raw RAM inspection distinct from mapped inspection.

## Named policies and unresolved details

1. **Completion policy:** interpret the documented tick counts as guest master
   ticks, 44 plain multiply / 54 with signed or accumulation / 176+14*clz16 for
   division (clz16(0)=16). This is a versioned software interpretation, not a
   measured hardware unit conversion. Latch modes/operands at trigger; publish
   results/status at the deadline. Do not alter constants to fit Test8.
2. **Remainder policy:** compute the mathematical unsigned remainder, zero for
   zero divisor, and label it unverified against the unspecified silicon defect.
   Independent vectors establish this policy; they do not repair hardware proof.
3. **Non-reentrant writes:** refuse writes to active operation's math backing
   registers (including aliases) before mutation. Permit pure reads and the
   documented SPRSYS clear/orientation path; latch calculation mode at trigger.
   Other silicon behavior under forbidden access is unspecified, so refusal is
   an explicit supported-domain boundary, not an invented corruption model.
4. **Sign latches/control transitions:** document and test deterministic latch
   updates on explicit upper writes; do not infer a new latch update from low
   auto-clear or a mode-only write. Unknown reset values use the project's
   declared deterministic reset/power-on policy rather than claiming silicon
   power-on zero. Keep incomplete hardware details visibly unverified.

Check ambiguous details against these primary refs when implementing, but
stop research when a missing hardware observation cannot change this scoped
software-policy recommendation. Never inspect GPL emulator code to fill it.

## Integration and compatibility

The independent `07-INTEGRATION-ADVICE.md` names actual adapters/readers and
copied builds. Snapshot format 7 becomes 8 under the existing explicit-version
refusal policy. Serialize all mutable register/sign/control/progress fields and
validate operation kind, flags, backing consistency and deadline reachability
before applying anything. Derive/recompute redundant pending outputs instead
of trusting forged serialized results. Test snapshots idle, in progress,
immediately before/on completion and after control changes. Include Suzy in
guest-state digest with a new domain and re-establish digest goldens through
actual runs and sensitivity controls before updating any claim binding.

Existing mixed-span negative controls assume FC00 unavailable. Replace the
generic proof with FDFF..FE00 under MAPCTL=02, preserve unchanged output/no RAM
fallback, and add a genuine unsupported Suzy hole. Keep opcode availability,
CPU access and mapped inspection consistent. Update the three copied fault
build source lists with the first new core source, plus independent snapshot
readers and documentation before expanding fixture behavior.

## Tracer-first implementation and tests

Start one actual guest writing unsigned operands through the mapped bus,
observing busy and then result, with mid-operation snapshot continuation.
That first state/source-list change owns reader/version/refusal/source-list
repairs and an early full registered inventory. Expand signed/divide/alias and
hostile snapshot vectors only after it passes. Then extend existing pinned
suite producer/consumer paths, exact inventory and active hash bindings.

Independent vectors cover hand-computable 0/1/FFFF/8000 pairs; 64-bit reference
products/accumulation; quotient/remainder identities; stale sign and ignored
auto-clears; rejected busy/control writes; event before/on/after deadline;
coarse/sliced advance; two instances; reset and snapshot refusal atomicity.
Seed targeted decoder/trigger, result, completion and snapshot mutations so
one shared wrong implementation cannot satisfy both guest and host checks.

No new CI lane, dependency, cache, model baseline, physical step or rights
decision is necessary. Native and hosted canonical qualify final software;
Linux focused checks use existing verified environment and both ShellCheck
versions. Keep artifacts private with owned success cleanup/failure retention.
