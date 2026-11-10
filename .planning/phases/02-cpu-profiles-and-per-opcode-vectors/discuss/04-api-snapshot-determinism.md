Phase 2 discuss, lens 04: integration with the Phase 1 host API, snapshot,
determinism and product/DX

Date: 2026-11-09. Author role: API/host engineer, product and DX owner.
Status: research notes for the DISCUSS synthesis. Nothing here decides O1.

Labels: [SO] source observation (file read in this repo or in a public doc),
[INF] inference, [UNK] unknown, [REC] recommendation. "Owner" marks an
owner decision already recorded in AGENTS.md, AUTONOMY.md or PROJECT.md.

Clean-room note: no emulator source was opened. Emulator behaviour mentioned
below is quoted only from this repo's research files, which record their own
exposure. LibFuturo was read only for its public contract text
(docs/contract.md and two profile docs, read-only).

Read for this lens: AGENTS.md, .planning/AUTONOMY.md, PROJECT.md,
REQUIREMENTS.md, ROADMAP.md (phases 2-3), LESSONS.md, Phase 1 CONTEXT,
SUMMARY notes (01-01, 01-02, 01-03, 01-06, 01-07, 01-08, 01-12), SKELETON.md,
research ARCHITECTURE/FEATURES/PITFALLS/SUMMARY (profile, snapshot, budget,
inspect sections), include/lynxplosion.h, core/machine.c, core/snapshot.c,
core/internal.h, runner/main.c, runner/bundle.[ch], tests/api/test_api.c,
tests/det/*, tests/abi/tests.cmake, tests/cmake/abi.cmake,
docs/claims/*, scripts/check-claims.sh, fixtures/skeleton/*.


======================================================================
0. What Phase 1 actually fixed that Phase 2 must respect or change
======================================================================

[SO] Public contract today (include/lynxplosion.h):
- lynx_config { struct_size, profile, ram_policy, reserved, ram_seed }.
  Only LYNX_PROFILE_NONE = 0 exists; init refuses any other profile value
  with LYNX_ERR_ARGUMENT, buffer untouched.
- Register ids PC, A, X, Y, S, P and LYNX_REG_COUNT = 6. The runner writes
  obs.regs.hex by looping 0..LYNX_REG_COUNT-1, so a new id changes the
  observation files and the golden.
- lynx_advance: "consumed never exceeds budget"; "a budget smaller than one
  machine step consumes nothing"; HALT stop when the skeleton reads 0xDB;
  a halted machine consumes 0 and is byte-identical afterwards.
- lynx_reset = power-on with the same config; clears content.
- Snapshot format 1: 236-byte header already carries profile (OFF_PROFILE)
  and checks it against the instance (MISMATCH). Payload: pc, a, x, y, s, p,
  halted, guest_ticks, 64 KiB RAM. Value check refuses halted > 1 and a PC
  outside the loaded content range (skeleton-specific).
- lynx_identity already reports profile.

[SO] Gates that constrain design:
- core.nm_globals: no B/b/D/d/C symbols in lynx_core_plain. A PIC build puts
  a `static const` table OF POINTERS (string table, function-pointer
  dispatch table) in .data.rel.ro, which nm shows as d on Linux ELF. [INF]
  It may pass on macOS and fail only on CI Linux (the Phase 1 lesson
  "prove gates on the other OS").
- api.abi_symbols: every defined external lynx_* name must be declared in
  the public header, every other external name must start lynx__. So
  internal lynx__ helpers are allowed in the core library and not in the
  public contract.
- api.header_selfcontained static-asserts the enum and struct layouts.
- Tests already include core/internal.h and poke struct fields white-box
  (guest_ticks = UINT64_MAX - 5 in api.advance_budget; RAM/register flips in
  det.sensitivity_state). [SO] Precedent for a white-box test seam exists.
- Whole-buffer memcmp across two instances is used as an oracle
  (snapshot roundtrip "second fresh instance continues to the same bytes").
  [INF] Any pointer stored in struct lynx_machine would break that oracle
  and make instance bytes address-dependent.
- -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror, ASan+UBSan in
  the check preset; core may include only stdint/stddef/stdbool/string.

[SO] Research facts that change the halt story (from PITFALLS P6, which
reports the lynx-tests `cpu` sources; recheck in the CPU lens):
- lynx-tests Test 8 runs every $x3 and $xB reserved opcode, including $CB
  and $DB (WAI/STP on a W65C02), as NOPs and checks no IRQ is taken inside
  the block. Test 6 checks reserved 1/2/3-byte NOPs. Test 7 bit ops:
  Lynx I executes $07/$87/$0F/$8F as NOPs (operand bytes then execute),
  Lynx II executes RMB/SMB/BBR/BBS.
- [INF] Consequence: on both Lynx profiles 0xDB is a NOP, not STP. The
  Phase 1 "0xDB halts" rule disappears with the skeleton, and no Phase 2
  opcode can produce LYNX_STOP_HALT under a Lynx profile. Every Phase 1 test
  that used a 0xDB halt image (api.advance_budget, runner.cli max-budget
  case, det.manifest_fields halting case) must be re-based on another
  terminating condition.


======================================================================
1. Decision A: profile ids, names, default and selection rules
======================================================================

Options for what LYNX_PROFILE_NONE (the default, value 0) means once a real
CPU exists:

 N1. NONE is refused at init ("you must choose lynx1 or lynx2").
     + honest: no default model.
     - lynx_config_default then yields a config init refuses: a default
       that fails is an anti-pattern for DX. Every Phase 1 test and the
       runner default break.
     - every run then names lynx1 or lynx2, and check-claims.sh (owner
       rule: claims may name only "none" while O1 is open) blocks even the
       repeatability claim. No R claim at all until O1.
 N2. NONE keeps the skeleton step.
     - dead provisional code kept alive; two machines behind one API;
       digests under "none" say nothing about the CPU. Reject.
 N3. NONE is a third, generic 65C02 table (for example WDC-like).
     - adopts behaviour that belongs to neither Lynx model; a de-facto
       baseline by default; contradicts "neither adopted as baseline".
 N4. NONE is the INTERSECTION of the two named profiles. [REC]
     An operation runs under NONE only when the Lynx I and Lynx II profile
     data define it identically (decode, length, cycles, flags, bus
     accesses). Otherwise the CPU does not execute it: lynx_advance stops
     before the opcode with a new stop reason (see Decision C), consumes 0
     for that step and leaves the machine byte-identical.
     + correct by construction if computed at run time as
       "entry_lynx1 == entry_lynx2" per opcode (no third table to drift).
     + a run under NONE that completes is, per the tables, identical under
       lynx1 and lynx2. That is checkable (det.profile_agreement below) and
       is exactly the kind of statement the claim rule allows while O1 is
       open.
     + default config stays valid; cc65-style 65SC02 code (no bit ops, no
       reserved opcodes) likely runs fully under NONE [INF, depends on the
       tables agreeing on all common opcode timing, which the CPU lens
       must confirm], so Phase 3's diagnostic can run and be claimed as R
       under profile none without touching O1.
     + no silent dependency on an unchosen model: the moment a program
       depends on the model, the run stops and says so.
     - one extra comparison per instruction (negligible).
     - scalar differences (IRQ entry, CLI/SEI latency) need the same rule:
       if the two profiles' scalars differ, NONE stops at the point the
       scalar matters (IRQ acceptance). In Phase 2 IRQs come only from the
       internal test seam, so this is a Phase 4 concern; record it now.
     - adversarial: is NONE a hidden baseline? No; it asserts only what
       both tables agree on. If both tables are wrong in the same way, NONE
       is wrong too, but so would be both named profiles. Label every NONE
       result "profile none = agreement of the lynx1 and lynx2 tables at
       build X", never "Atari Lynx".

[REC] Profile ids and names (append-only, never renumbered, never renamed;
ids are persisted in snapshot headers, names in manifests, claims and
fixture manifests):
  LYNX_PROFILE_NONE  = 0  "none"   intersection, the default
  LYNX_PROFILE_LYNX1 = 1  "lynx1"  Lynx I model profile
  LYNX_PROFILE_LYNX2 = 2  "lynx2"  Lynx II model profile
- Call them model profiles, not CPU profiles: Phase 3+ adds MAPCTL
  read-back and other model differences to the same id [SO ARCHITECTURE
  mentions MAPCTL read-back]. Naming them LYNX_CPU_PROFILE_* now would force
  a rename in Phase 3.
- Lowercase, no hyphen: works as a CLI value, a kv value and a claim value.
- Do not encode "baseline" or "default" in any name. The header and README
  say that both are unqualified data hypotheses and neither is baseline
  until O1 (owner gate).

[REC] Selection rules:
- Per instance, fixed at lynx_machine_init (config copied into the
  instance, as today). No setter. To change profile: deinit, init again in
  the same memory. libretro-style hosts already treat model choice as a
  restart-requiring option [INF].
- lynx_reset keeps the profile (it re-runs setup from the stored config,
  as today).
- Unknown profile value (3, 0xFFFFFFFF) at init: LYNX_ERR_ARGUMENT, memory
  and *out untouched (extend api.init_refusal; whole-buffer oracle).
- Host mutating its own lynx_config after init has no effect (test it).
- The instance stores the profile id only (already in cfg). It does NOT
  store a pointer to a table: index a const table by id at each use, or copy
  a small by-value parameter block into the instance at init. Pointers in
  the instance break cross-instance memcmp oracles and snapshot purity.

[REC] Add one small pure query so the vocabulary lives in one place:
  const char *lynx_profile_name(uint32_t profile);
  returns "none", "lynx1", "lynx2", or NULL for an unknown id.
- Implement with a switch returning string literals, never a table of
  pointers (nm gate on Linux, section 0).
- The runner uses it both ways (print; parse by looping ids until NULL).
- Pros: one source of truth for manifests, CLI help and claims vocabulary;
  the consumer of Phase 3 (CON-01, public API only) gets names for free.
  Cons: +1 public symbol; header test and abi_symbols list grow. Worth it.
- Rejected: a profile descriptor struct with feature flags exposed
  publicly. It freezes the internal table shape into the ABI before the
  vectors have validated it. Revisit with HOST-03 (v2).


======================================================================
2. Decision B: guest time, budget semantics and instruction boundaries
======================================================================

This is the one Phase 1 contract a real CPU cannot keep as written.

[SO] Phase 1 guarantees "consumed never exceeds budget" and "a budget
smaller than one machine step consumes nothing", because every step cost
exactly 4 ticks. [SO] det.two_instances advances in 8 chunks of 512 and
asserts each call consumes exactly 512 and the result equals the single
4096-tick golden.

[INF] With variable instruction cost (2-7 cycles, page-cross and
branch-taken extras, decimal penalty, per-access bus cost from CPU-03,
later DMA stalls) the cost of the next instruction is not known before it
runs. Options:

 B1. Keep "never exceeds" by predicting the cost (pre-decode, worst case).
     - worst case is unbounded once Suzy owns the bus (later phases);
       pre-decoding through MMIO with side effects is wrong. Reject.
 B2. Bus-cycle-resumable CPU (each access is a resumable step), stop when
     the next access does not fit.
     + keeps "never exceeds"; finest stop granularity.
     - every opcode becomes a state machine; snapshots must carry
       mid-instruction micro-state (latched operand, effective address,
       step index); much more code before any vector passes. This is the
       CPU/bus lens's AD-01 call; from the API side it is not needed.
 B3. Instruction-boundary stops with reported overshoot. [REC]
     lynx_advance computes deadline = guest_ticks + min(budget, room) and
     starts an instruction only while guest_ticks < deadline. It always
     returns at an instruction boundary. consumed can exceed budget by the
     remainder of the last instruction started before the deadline.
     + research ARCHITECTURE recommends exactly this ("instruction-granular
       stop points remain available ... overshoot bounded by one
       instruction"); the owner's sibling core profile (Nesturbator in the
       LibFuturo docs) documents instruction-boundary overshoot. [SO]
     + snapshots are always taken at an instruction boundary: no
       mid-instruction state in the format (big simplification for DET-03
       in Phase 3).
     + device accuracy is unaffected: devices still catch up per access
       tick inside the instruction (AD-01 recommendation).
     + budget 1 runs exactly one instruction: a free, documented
       single-step for debuggers and the vector seam cross-check.
 B4. Stateless overshoot plus an internal "carry/debt" so fixed budgets
     stay chunk-invariant.
     - hidden accounting state that is not machine state but must be in
       the snapshot; consumed stops meaning elapsed guest time; two clocks.
       Reject: guest time stays the only clock.

[REC] New lynx_advance contract text (replace the Phase 1 sentences):
- "Runs whole CPU instructions. An instruction starts only while guest time
  is below (guest time at entry + budget). The call returns at an
  instruction boundary, so consumed may exceed budget by part of the last
  instruction. A budget of 0 consumes nothing. A budget of 1 runs exactly
  one instruction unless the machine cannot continue."
- Do not publish a numeric overshoot bound in the header beyond Phase 2.
  Phase 2 can state the flat-bus maximum in docs; later phases add bus
  stalls inside an instruction [UNK: whether a long Suzy bus hold inside an
  instruction makes overshoot large; revisit when Suzy lands, possibly by
  making the CPU resumable at the "waiting for bus grant" point only].
- Guest time never wraps: do not start an instruction when fewer than the
  internal maximum instruction cost remain before UINT64_MAX; report
  BUDGET. Keep the white-box UINT64_MAX-5 style test with new numbers.
- Chunk invariance rule for hosts (document in the header and README):
  splitting a run gives identical state only if each call's budget is
  computed from an absolute deadline: budget = (D > ticks) ? D - ticks : 0.
  The clamp matters: after an overshoot, ticks can already be past D, and
  naive unsigned subtraction wraps to an enormous budget. [REC] Ship this
  as a tiny static inline helper in the runner/tests, not as a new public
  function (YAGNI; revisit if a second host needs it).
- Stop reason priority within one call, fixed and documented: UNDEFINED
  (profile none) before BUDGET; later WATCH/FRAME slot in by rule, not by
  accident.

[REC] Tests to re-base for B3 (deliberately, with the reason in the commit):
- api.advance_budget: budget 0 consumes 0 and buffer unchanged; budget 1
  equals exactly one instruction (compare with the internal single-step);
  consumed <= budget + max_instruction_ticks for a sweep of budgets;
  chunk invariance property: for chunk sizes {1, 2, 3, 7, 64, 512, 4095}
  with deadline arithmetic, the final instance bytes equal the single-call
  bytes (whole-buffer memcmp); guest-time clamp near UINT64_MAX; the
  "cannot continue" stop consumes 0 and leaves the buffer byte-identical,
  repeatedly.
- det.two_instances: keep A,B interleaving but drive by deadlines
  (512, 1024, ...), drop the "each call consumes exactly 512" assertion,
  keep "sum equals final ticks" and the golden equality.


======================================================================
3. Decision C: stop reasons and halting
======================================================================

[REC] Add LYNX_STOP_UNDEFINED = 4: "the selected profile does not define
the next operation; the machine cannot continue under this profile". In
Phase 2 only profile none reaches it (Decision A). PC stays on the
offending opcode; repeated calls return it again with consumed 0; the trap
is stateless (re-derived from profile + next opcode), so no new state field.
- Why not reuse HALT: evidence clarity. A manifest line
  "stop_reason = undefined" and "stop_reason = halt" are different
  findings; hosts and the claim reader must not confuse "this program
  depends on the model" with "the CPU stopped".
- LibFuturo-style adapters map it to their core-halted stop [INF].
- Option kept open for the CPU lens: a named profile may mark an entry
  "unverified-stop" so a run stops instead of guessing (for example an
  unsourced reserved-NOP timing). Not recommended as default (it would
  stop most programs on decimal ADC if decimal timing is unsourced), but
  the stop reason supports it without API change.
[REC] LYNX_STOP_HALT stays in the enum, documented as reserved again:
"no Phase 2 profile can stop the CPU; WAI and STP opcodes are NOPs on both
Lynx profiles per the lynx-tests cpu suite as reported in research [SO via
PITFALLS P6, recheck]". Phase 4 (CPUSLEEP, EVD-03 no-run control) may give
it meaning.
[REC] No WAI waiting state in Phase 2 for the same reason. If the CPU lens
finds a profile where $CB waits, model "waiting" as time passing to the
deadline exactly (no overshoot), not as a halt.

Tests whose halt cases move to UNDEFINED (deliberate re-baseline):
- api.advance_budget halt image -> a profile-none image whose first
  differing opcode is known, for example 0x07 (RMB0 on Lynx II, 1-byte NOP
  on Lynx I per Test 7 [SO via research]).
- runner.cli: the UINT64_MAX budget case uses one-byte content 07 under
  the default profile; manifest must read stop_reason = undefined. A
  UINT64_MAX budget on a non-terminating program under lynx1/lynx2 never
  returns (Phase 1 lesson): every max-budget test must use a profile-none
  trap image.
- det.manifest_fields halting case: same change.


======================================================================
4. Decision D: register exposure and the test state seam
======================================================================

[REC] Public read ids (append-only; LYNX_REG_COUNT is documented as
"grows; read ids 0..COUNT-1 of the build you link"):
  existing PC, A, X, Y, S, P (values unchanged in meaning)
  LYNX_REG_CPU_STATE = 6: a documented bitfield of the CPU's latent state
  that crosses an instruction boundary (for example: IRQ line level as last
  sampled; "take IRQ before next opcode" latch; any CLI/SEI delay latch the
  CPU lens needs). New latches add bits, not ids.
- Invariant [REC]: every field in the snapshot payload is readable through
  the public inspect API. Then the observation digest (obs.regs.hex loops
  over COUNT) covers all CPU state, det.sensitivity_state can flip every
  bit, and a future debugger needs no new read path.
- Define and test the P read convention: [REC] P reads with bit 5 = 1 and
  bit 4 = 1 (the value PHP pushes; lynx-tests Test 4 expects pushed P to
  have 0x30 [SO via research]); the CPU lens confirms against the vector
  format. Snapshot restore refuses a P without bits 4 and 5.
- Name lynx_read_memory explicitly "raw RAM view" in the header now.
  Phase 3 adds a CPU-mapped, side-effect-free view (MEM-01); naming now
  avoids a semantic change of an existing function later.
- No public IRQ input: IRQs come from Mikey in Phase 4, never from the
  host.

Setting state for vector runs ("load initial state, run one instruction,
compare final state and bus cycles"). Options:

 S1. Internal white-box seam. [REC]
     The vector runner is a test executable that links lynx_core (the
     sanitized library, like lynx-test-api) and includes core/internal.h
     plus a core-internal CPU header. It sets registers and sparse RAM by
     writing struct fields (or one internal helper
     lynx__cpu_set_state(m, const lynx__cpu_regs *)), drives one
     instruction with an internal lynx__cpu_step(m, log) that returns
     ticks, and reads the bus-cycle log from a CALLER-OWNED buffer passed
     in (null in production). Test-only IRQ line control is an internal
     setter too.
     + zero public surface; nothing for hosts to misuse; no ABI
       commitment while the CPU is still being validated.
     + matches the Phase 1 precedent (tests already include internal.h and
       poke fields); api.abi_symbols already allows lynx__ names.
     + fast: sparse set and sparse restore of only the addresses a vector
       touches (no 64 KiB memset per vector).
     - lynx__ symbols are linkable from the static library by anyone; they
       are undocumented and outside the contract (state this in the
       header comment of internal.h).
     - tests can create states a real run cannot reach; acceptable for a
       vector oracle; keep the snapshot value checks independent of it.
 S2. Public debug mutators (lynx_write_register, lynx_write_memory).
     - widens the contract before HOST-03 (v2); every mutator needs
       refusal semantics, determinism story (pokes are input that the
       manifest does not record), snapshot interplay and tests.
       Anti-pattern now. Reject for Phase 2.
 S3. Separate library target with test hooks (lynx_core_testhooks).
     + production library carries no poke helpers.
     - more CMake; ODR hazard if a test TU defines a lynx__ symbol the core
       also defines (two definitions, one silently wins with static libs).
       Only worth it if poke helpers grow large. Not now.
 S4. Set state through snapshot restore, step with lynx_advance(m, 1).
     + public API only; exercises restore validation heavily.
     - cannot observe the bus-cycle log (CPU-03 needs it); 64 KiB payload
       hash per vector is slow at vector scale; couples vectors to the
       snapshot format.
     [REC] Use S4 only as a small seam-agreement test (for example one
       vector per opcode): public restore + advance(1) must produce the
       same final state as the internal step. Cheap proof that the public
       path and the vector path are the same CPU.

[REC] ODR/test-bus rule: never substitute the production bus by link-time
replacement (a test TU defining lynx__bus_read). Select test cost models by
data (an internal cost-table selector or a cost parameter in the step call)
so one core library serves production and tests. No function pointers from
the host during advance (research boundary rule); a caller-owned log buffer
is data, not a callback.


======================================================================
5. Decision E: snapshot format 2
======================================================================

[REC] Bump LYNX__SNAPSHOT_FORMAT to 2 in the same commit that changes the
payload. Header layout unchanged (profile is already in it and already
checked). Payload (CPU lens finalizes the latch set):
  pc u16, a u8, x u8, y u8, s u8, p u8, cpu_state u8 (the same bitfield
  as LYNX_REG_CPU_STATE), reserved/pad bytes zero, guest_ticks u64,
  ram 65536.
  Drop "halted" (no halt state in Phase 2). No mid-instruction state
  (Decision B3 guarantees snapshots at instruction boundaries).
Old blobs: refuse. A format-1 blob returns LYNX_ERR_SNAPSHOT_VERSION (the
version check runs before the build-id check today); a format-2 blob from
another build returns MISMATCH. No migration, consistent with "same build
only" (Phase 1 decision, research AD-12).

[REC] Value validation changes:
- Remove the "PC inside the loaded content" check: it encodes the skeleton
  rule; a real CPU's PC may be anywhere (vectors start anywhere).
- Add: p has bits 4 and 5 set (if that is the representation); cpu_state
  has no undefined bits; reserved bytes zero.
- Keep: exact length, magic, header length, build id, identity, payload
  length, payload hash, then values, then a no-fail apply.

[REC] Version bump enforcement (cheap, compile-time): pin length to format
in core/snapshot.c, for example
  _Static_assert(LYNX__SNAPSHOT_FORMAT != 2 || LYNX__SNAPSHOT_PAYLOAD_LEN
                 == <n>, "payload layout changed: bump the format");
so changing the layout without a bump fails to compile. Policy text: bump
on any header or payload layout change or any change in a field's meaning;
behaviour-only changes need no bump (the build id already differs).

[REC] Snapshot coverage test (catches a forgotten field, the classic
save-state bug): white-box set every CPU field and latch to a non-default
value, save, restore into a fresh instance with the same identity, and
memcmp the whole instance buffers. Then continue both by the same deadline
and memcmp again. Extends api.snapshot_roundtrip.

[REC] New refusal tests (api.snapshot_refusal):
- cross-profile: blob saved under lynx1 restored into lynx2 and into none:
  MISMATCH, buffer byte-identical; all three pairs.
- format 1 and format 3: VERSION.
- bad p bits, undefined cpu_state bit, nonzero reserved byte (each with a
  recomputed payload hash so the value check, not the hash, refuses).
- remove the "PC outside content" case; add a positive case: a PC outside
  the content range now restores.
- the header-byte-flip sweep stays (header layout unchanged).


======================================================================
6. Decision F: determinism evidence, re-baselining and fixtures
======================================================================

6.1 The tracer image under a real CPU [INF, hand-decoded from standard
65C02 encodings; the CPU lens should confirm]
fixtures/skeleton/skeleton-image.hex at 0x0200:
  03          reserved opcode, 1-byte NOP (if both tables agree)
  0a          ASL A
  11 18       ORA ($18),Y   pointer from seeded zero page
  18          CLC
  1f 26 2d    BBR1 $26,+$2d on Lynx II; on Lynx I a 1-byte NOP (Test 7
              shows the bit-0 forms only; extending to $1F is inference)
  26 2d       ROL $2d (Lynx I path continues here)
  ...
  6c 73 7a    JMP ($7A73)   jumps through seeded RAM
Consequences:
- Under profile none the image stops with UNDEFINED at PC 0x0204 after
  four instructions. Under lynx1 and lynx2 it diverges at 0x0204 and soon
  executes seeded random RAM. Expected end states after thousands of ticks
  are not independently derivable without a second CPU model.
- [REC] Retire it as the golden and as the api.skeleton workload. Keep it
  as content for identity, loader and snapshot-identity tests (its SHA-256
  is pinned in api.identity and the claim).
- [REC] Reuse it as a cheap profile-divergence test with independently
  derivable values: run to the stop under none (4 instructions; A equals
  the seeded byte at the pointer, computed in the test with the test's own
  independent SplitMix64; ticks from cited cycle counts times the default
  access cost), then single-step the fifth instruction under lynx1 (PC
  0x0205) and under lynx2 (PC 0x0207 or 0x0234 depending on bit 1 of the
  seeded byte at $26, again computed independently). Same bytes, three
  documented outcomes: a visible demonstration of per-instance profiles.

6.2 New fixtures [REC]
- fixtures/cpu/churn: a small hand-assembled, never-terminating 65C02
  program using only opcodes both profiles must agree on (loads/stores in
  zp, abs, indexed and indirect modes; ADC/SBC binary and decimal; shifts;
  PHA/PLA/JSR/RTS; taken and untaken branches; BRA if common). It reads
  seeded RAM (keeps det.sensitivity's seed control meaningful) and writes
  RAM pages and stack (keeps the RAM digest meaningful). It replaces the
  tracer as the golden workload and as the program the snapshot,
  instances and inspect tests advance. Manifest: profile none, evidence R.
  Content-flip sensitivity must flip a DATA byte (flipping an opcode may
  trap and make the run vacuous).
- fixtures/cpu/sum (name illustrative): a short terminating program
  (for example sum 1..10 into $0300, then a tight BRA-to-self loop or a
  profile-none trap opcode as the end marker) whose final registers, RAM
  bytes and consumed ticks are derived BY HAND in a committed trace table
  (per instruction: bytes, cycles with citation, accesses, cumulative
  ticks under the default cost model). Replaces api.skeleton's exact
  expectations with an independent oracle that also checks tick
  accounting end to end.
- Assembly cross-check [REC]: commit the .s source next to the hex and add
  a test that assembles it with the pinned ca65 (already built by
  check.sh) and compares bytes with the committed hex. Independent check of
  hand assembly with no new dependency and no Python. ca65 checks
  encoding only, never timing.
- Expected values: hand-derived and cited in comments; an optional
  throwaway script in the session scratch directory may cross-check them
  but is never committed and never in the check path (Phase 1 precedent:
  SHA-256 vectors confirmed with shasum, SplitMix64 re-implemented in the
  test from the published algorithm).

6.3 Re-baseline protocol for the golden [REC]
- Order: vectors pass for both profiles, the hand-derived program test
  passes, then regenerate the golden in that same commit series with the
  reason in the commit message (rule already written in the golden file).
  The golden is a tripwire (R), never evidence of correctness; correctness
  comes from vectors and hand-derived values.
- Rename the file to tests/golden/cpu-churn.digest (or keep the name and
  rewrite its header); update workload text, regenerate command (add
  --profile none) and the rule.
- Cross-compiler equality (Apple clang local, gcc on CI) of the new golden
  is a meaningful check for the evaluation-order footgun in 7.2.
- det.golden.self_test unchanged.
- New det.profile_agreement [REC]: the churn workload's observation digest
  is equal under none, lynx1 and lynx2 (the observation files exclude the
  manifest, so profile does not enter the digest [SO bundle.c]). Proves the
  intersection property on a real workload and catches a profile table
  edit that silently changes common behaviour.
- det.sensitivity_state: add flips of every new state field
  (cpu_state bits); add "profile" as an input sensitivity on the tracer
  image (none vs lynx1 must differ, since the tracer depends on the model).
- docs/claims/skeleton-repeatability.claim: re-point to the churn workload
  (content_sha256, evidence names), profile stays none, inventory hash
  updated after re-running evidence. Consider renaming the claim file.
- tests/inventory/fast.txt: rename api.skeleton to api.cpu_program (the
  test's meaning changed; keeping the name would mislead), add the new
  tests. Inventory hash changes once.

6.4 Tests that change (inventory of deliberate re-baselines)
  api.skeleton            -> api.cpu_program (hand-derived oracle)
  api.advance_budget      -> B3 semantics, UNDEFINED stop, chunk property
  api.init_refusal        +  profile 3 and 0xFFFFFFFF refused; 1, 2 accepted
  api.instances_independent + lynx1 and lynx2 instances interleaved, each
                              equal to its solo run
  api.inspect_pure        +  LYNX_REG_CPU_STATE read is pure
  api.reset_power_on      +  declared CPU power-on values; profile kept
  api.snapshot_roundtrip  +  coverage test (5)
  api.snapshot_refusal    +  section 5 cases
  api.header_selfcontained + new enum values asserted
  api.abi_symbols         +  lynx_profile_name declared and defined
  det.golden, det.two_runs, det.two_instances, det.sensitivity,
  det.sensitivity_state, det.manifest_fields, runner.cli: new workload,
  profile flag, UNDEFINED stop text
  new: cpu vectors per profile (CPU lens), det.profile_agreement,
  cpu fixture assembly check, seam agreement (S4), profile divergence
  on the tracer.


======================================================================
7. Adversarial pass
======================================================================

7.1 API misuse
- Profile switch mid-run: impossible through the API (no setter, config
  copied). White-box pokes of cfg.profile are test-only. Snapshot identity
  binds profile, so a blob cannot carry a run across profiles.
- Snapshot cross-profile: MISMATCH (header identity) before any payload
  check; tests in section 5. A forged header claiming the instance's
  profile with a payload from another profile restores, and that is fine:
  the payload holds no profile-specific fields, and the build id binds the
  rest.
- Register write while stopped: no public write path exists. Internal
  setters are documented as test-only.
- Reset under UNDEFINED: reset clears content (Phase 1 semantics); advance
  then refuses with STATE until a load. Test it.
- Budget arithmetic: hosts computing D - ticks without the clamp get a
  wrap after overshoot. Documented helper plus a test.
- Profile none surprise: a host runs an unknown program under the default
  and gets UNDEFINED. That is the intended loud failure; make the message
  actionable (section 8).
- Silent fallback: never map an unknown profile to none. Refuse.

7.2 Determinism hazards in an opcode switch (C17, -Wconversion -Werror,
UBSan on)
- Unspecified evaluation order: two bus accesses in one expression, for
  example addr = read(pc+1) | (read(pc+2) << 8), leave the access order
  unspecified. gcc and clang may differ, so the bus-cycle log and any
  side-effecting MMIO read (Phase 3+) diverge across compilers. Rule:
  exactly one bus access per full statement, assigned to a local in
  program order. The vectors' cycle-order comparison on both compilers
  (local clang, CI gcc) and the cross-compiler golden catch it.
- Signed overflow: keep all CPU arithmetic unsigned. V flag as
  (~(a ^ m) & (a ^ r) & 0x80u) on unsigned values; never compute through
  int8_t.
- Branch offsets: (int8_t)u for u >= 0x80 is implementation-defined before
  C23. Use unsigned: target = (uint16_t)(pc + off - ((off & 0x80u) ?
  0x100u : 0u)).
- Integer promotion: uint8_t and uint16_t promote to int. A product of two
  uint16_t values can overflow int (UB): cast to uint32_t first. Not an
  issue for the CPU, but the tick math (cycles times cost) and the Phase 6
  Suzy 16x16 multiply hit it. Do all tick math in uint64_t.
- Shifts: shift unsigned values only; cast before shifting into bit 31
  ((uint32_t)b << 24), as get_u32 already does.
- -Wconversion: GCC warns on uint8_t compound assignment (a += m). Write
  a = (uint8_t)(a + m) (the skeleton already does). Expect a CI-only
  failure class if the local compiler is quieter (Phase 1 lesson).
- Plain char signedness differs between platforms. Never use plain char
  for byte data in the core.
- Decimal mode: invalid BCD digits have defined hardware results; follow
  the cited algorithm in unsigned arithmetic; vectors decide.
- Address wrap: zero-page indexed wraps in 8 bits, absolute in 16 bits;
  write the casts explicitly.
- Full opcode coverage: all 256 opcodes must have an entry in both
  profiles; static-assert the table length; a test iterates 0..255 under
  each profile and requires a defined outcome (execute or UNDEFINED for
  none). The default label of the switch must be unreachable and fail
  loudly in tests.
- Dispatch by switch, not by a function-pointer table (nm gate on PIC
  Linux; also keeps the instance free of pointers).

7.3 ABI, ODR and header self-containment
- New public items: LYNX_PROFILE_LYNX1/2, LYNX_STOP_UNDEFINED,
  LYNX_REG_CPU_STATE, lynx_profile_name. All additive; lynx_config and
  lynx_identity layouts unchanged (struct_size unchanged).
- lynx_machine_size grows; hosts already query it. Tests use sizeof via
  internal.h.
- LYNX_REG_COUNT changes value: an anti-pattern for a stable ABI; accepted
  pre-1.0 and documented as "grows".
- internal headers never included from lynxplosion.h; header test keeps the
  public header compiling alone.
- No test TU defines a lynx__ name (ODR with static libraries).

7.4 Performance sanity
- Real time needs on the order of 1 million instructions per second (16 MHz
  master clock, roughly 4 MHz CPU-equivalent) [INF]. A switch interpreter
  is far faster. No optimisation in Phase 2 (owner rule: measure first).
- The cost that matters is the vector suite under ASan+UBSan in the fast
  check: SingleStepTests ships about 10,000 vectors per opcode [INF from
  research description]; at full size that is millions of steps plus
  parsing. Measure; use a fixed, inventoried per-opcode subset in the fast
  gate if needed, with the full set visible as a declared separate lane or
  skip (CPU/verification lenses own the numbers). Sparse RAM set/restore
  per vector, not a 64 KiB memset.
- Phase 1 "no JSON in the check path" decision was about manifests; how
  vector JSON reaches the C runner is for the vector lens to decide
  (CMake string(JSON) is available but slow at this scale [INF]).


======================================================================
8. Product and developer experience
======================================================================

Runner CLI [REC]:
- --profile none|lynx1|lynx2, default none (matches lynx_config_default).
- Unknown value: exit 2, "lynx-run: --profile must be none, lynx1 or
  lynx2: <value>" plus the usage line (same style as --ram-policy).
- run.manifest: profile line from lynx_profile_name; no new keys needed
  (keeps det.manifest_fields' key list stable); stop_reason vocabulary adds
  "undefined".
- On an UNDEFINED stop the runner still writes the bundle and exits 0 (a
  stop is a recorded outcome, not a runtime error), and prints one stderr
  note: "lynx-run: note: profile none stopped at $0204: opcode $1f differs
  between lynx1 and lynx2; choose --profile". The note is composed by the
  runner from public reads (PC, read_memory); the core holds no message
  strings. Keep stdout's two digest lines unchanged (runner.cli parses
  them).

Claims and manifests [REC]:
- check-claims.sh ALLOWED_PROFILES stays "none" (owner rule, O1 open).
  Phase 2 writes no lynx1/lynx2 claim; per-profile vector results are test
  results with exact inventories, not claims.
- Fixture manifests may say lynx1 or lynx2 (for example the CPU-05
  bit-operation fixtures); they are not claims. Optional: have
  check-manifests.sh validate profile against none|lynx1|lynx2 to catch
  typos (cheap; add a seeded typo control if added).
- Claim template comment: say what profile none means now ("agreement of
  the lynx1 and lynx2 tables; a run that completes under none depends on
  neither model").

Documentation [REC]:
- Header: rewrite the PROVISIONAL banner (the CPU is real but unqualified;
  the raw RAM loader is still provisional until Phase 3); document the
  three profiles, the UNDEFINED stop, the new advance contract, the
  deadline helper pattern, P bit convention, CPU_STATE bits, raw-RAM naming
  of lynx_read_memory, and declared power-on CPU values.
- README: a short "Profiles" section (what none/lynx1/lynx2 mean, that
  neither named profile is a baseline until the owner decides O1, how to
  select with --profile).
- One profile provenance document beside the data (CPU lens owns content):
  every table entry that differs between profiles or is not covered by
  vectors cites its source or says "unverified" (ROADMAP Phase 2 criterion
  4).
- SKELETON.md is a Phase 1 record; do not rewrite it, add a line pointing
  to the Phase 2 CONTEXT.

How Phase 3 builds on this without rework [INF]:
- Model profiles (not CPU profiles) take MAPCTL and other model data in
  Phase 3 under the same ids.
- All CPU accesses already go through one bus function (CPU-03): the
  memory map decoder and the API-08 write watch hook in there.
- lynx_read_memory is already named raw RAM; Phase 3 adds the CPU-mapped
  view as a new inspect call.
- Startup tier A replaces the raw loader and sets a cited CPU preset; the
  declared power-on values from Phase 2 are the "left at power-on reset"
  fallback MEM-02 needs.
- DET-03 (snapshot mid-diagnostic) is easy because snapshots are always at
  instruction boundaries (B3) and the coverage test exists.
- Phase 3 adds startup tier and container identity to the snapshot header:
  expect format 3, by the same bump rule.
- The consumer (CON-01) needs only public reads and lynx_profile_name.


======================================================================
9. Unknowns
======================================================================
[UNK] Whether the two Lynx profiles agree on every non-bit-op opcode's
      length, cycles and flags (decides how much real code runs under none).
[UNK] Whether $CB/$DB are NOPs on both models with the same length and
      cycles (research reports Test 8; recheck in the CPU lens).
[UNK] Default per-access cost on the Phase 2 flat bus (4 vs 5 ticks,
      page-mode stream state): affects every hand-derived tick expectation.
[UNK] The exact latch set for IRQ sampling, CLI/SEI latency and IRQ entry
      (defines CPU_STATE bits and payload bytes).
[UNK] P bit 4/5 convention used by the chosen vector set.
[UNK] Declared power-on S and the other values hardware leaves undefined.
[UNK] Overshoot size once Suzy bus holds can occur inside an instruction
      (Phase 7); may require resumability at the bus-grant point.
[UNK] Vector suite runtime under sanitizers at the chosen subset size.


======================================================================
10. One-shot recommendations (for the synthesis)
======================================================================
 1. Profile ids 0 none, 1 lynx1, 2 lynx2; append-only; model profiles, not
    CPU profiles; names lowercase.
 2. none = intersection: execute only operations both tables define
    identically, computed at run time; otherwise stop with
    LYNX_STOP_UNDEFINED, consume 0, unchanged state. Default stays none.
 3. Profile fixed at init per instance; no setter; reset keeps it;
    unknown id refused with ARGUMENT and no write; no pointer in the
    instance.
 4. Add lynx_profile_name(id) (switch over literals).
 5. lynx_advance: instruction-boundary stops, start while ticks < deadline,
    report overshoot; budget 1 = one instruction; chunk invariance via
    absolute deadlines with a clamped helper; guest-time clamp kept.
 6. LYNX_STOP_UNDEFINED = 4 added; HALT reserved again (WAI/STP are NOPs on
    both Lynx profiles per research).
 7. Registers: keep PC..P; add LYNX_REG_CPU_STATE bitfield; invariant
    "every snapshot field is publicly readable"; P reads with bits 4/5 set
    (confirm with vectors); document lynx_read_memory as raw RAM.
 8. Test seam: internal white-box (internal.h, lynx__cpu_step with
    caller-owned bus log, sparse state set); no public mutators; plus a
    small public-path seam-agreement test (restore + advance(1)).
 9. Snapshot format 2: payload adds cpu_state and reserved bytes, drops
    halted; format 1 refused (VERSION); PC-in-content check removed;
    compile-time length-to-format pin; coverage test; cross-profile and
    new value refusal tests.
10. Retire the tracer as golden and api.skeleton workload; keep it for
    identity/loader tests and a short profile-divergence test with
    independently computed values.
11. New fixtures: a never-terminating profile-common churn program (golden,
    snapshot, instance tests) and a short terminating program with a
    hand-derived, cited trace table; ca65 assembly cross-check in the
    check; no committed scripts in the check path.
12. Re-baseline order: vectors green, hand-derived test green, then
    regenerate the golden with the reason in the commit; rename
    api.skeleton to api.cpu_program; add det.profile_agreement; update the
    claim and inventory hash once.
13. Runner: --profile flag, manifest profile from the core, stop text
    "undefined", bundle still written, actionable stderr note; claims stay
    profile none.
14. Coding rules for the CPU: one bus access per statement, unsigned
    arithmetic only, no int8_t casts, explicit uint8_t casts, switch
    dispatch, 256-entry tables static-asserted, no plain char.
