/* Lynxplosion public C API.

   One machine instance lives in memory the caller provides. The core never
   allocates, reads a clock, touches a file or starts a thread. Every function
   that can refuse input checks all of it first and leaves the machine
   byte-identical when it refuses.

   The machine runs a cycle-stepped 65C02-class CPU. It is real code but
   unqualified for the Lynx: its instruction sets and cycle counts come from
   other vendors' datasheets, a hardware-informed test and software-model
   vectors, and every cycle costs a PLACEHOLDER five master ticks. No profile
   is a baseline. Normal content starts from a strict BLL tier-A direct load.
   CPU accesses use MAPCTL's four overlays; mapped device, ROM, vector and
   MAPCTL-read routes without an implemented target stop as unavailable. */
#ifndef LYNXPLOSION_H
#define LYNXPLOSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status of every call that can fail. LYNX_OK is zero. */
typedef uint32_t lynx_status;
enum {
    LYNX_OK = 0,
    LYNX_ERR_ARGUMENT = 1,          /* NULL pointer, zero length or an invalid field */
    LYNX_ERR_SIZE = 2,              /* buffer smaller than lynx_machine_size() */
    LYNX_ERR_ALIGNMENT = 3,         /* buffer not aligned to lynx_machine_align() */
    LYNX_ERR_RANGE = 4,             /* register id or address range out of bounds */
    LYNX_ERR_STATE = 5,             /* machine not initialised, or no content loaded */
    LYNX_ERR_SNAPSHOT_INVALID = 6,  /* malformed, truncated or corrupted snapshot */
    LYNX_ERR_SNAPSHOT_VERSION = 7,  /* snapshot format version not supported */
    LYNX_ERR_SNAPSHOT_MISMATCH = 8, /* snapshot from another build, config or content */
    LYNX_ERR_CAPACITY = 9,          /* output buffer smaller than the snapshot */
    LYNX_ERR_UNAVAILABLE = 10       /* requested mapped observation is unavailable */
};

/* Why lynx_advance returned. FRAME and HALT remain reserved in this build.
   WATCH reports a completed matching CPU write. UNAVAILABLE leaves the
   pending CPU request uncompleted at its grant. */
typedef uint32_t lynx_stop_reason;
enum {
    LYNX_STOP_BUDGET = 0,   /* the tick budget was used up */
    LYNX_STOP_FRAME = 1,    /* reserved */
    LYNX_STOP_WATCH = 2,    /* reserved */
    LYNX_STOP_HALT = 3,     /* reserved */
    LYNX_STOP_UNDEFINED = 4, /* profile NONE reached an opcode the Lynx profiles disagree on */
    LYNX_STOP_UNAVAILABLE = 5 /* the pending CPU bus route has no implemented target */
};

/* Hardware profile, fixed per instance at init. Ids are append-only and are
   never renumbered. NONE (the default) runs only what the LYNX1 and LYNX2
   instruction tables agree on and stops with LYNX_STOP_UNDEFINED before any
   other opcode. LYNX1 follows the 65SC02 instruction set ($x7/$xF are NOPs),
   LYNX2 the Rockwell R65C02 set (adds RMB, SMB, BBR, BBS). Neither is a
   baseline (owner decision O1 is open). */
enum { LYNX_PROFILE_NONE = 0, LYNX_PROFILE_LYNX1 = 1, LYNX_PROFILE_LYNX2 = 2 };

/* How RAM is filled at power-on and at reset. ZERO fills every byte with
   0x00 and is the default everywhere. SPLITMIX64 fills RAM from the
   SplitMix64 generator (public domain) started at ram_seed: eight bytes per
   output, least significant byte first, from address 0. */
enum { LYNX_RAM_POLICY_ZERO = 0, LYNX_RAM_POLICY_SPLITMIX64 = 1 };

/* Register ids for lynx_read_register. P reads with bits 4 and 5 set.
   CPU_STATE packs the CPU's position inside an instruction:
     bits 0-7   interrupt and sequence latches
     bits 8-15  index of the pending cycle inside the instruction (0 at a boundary)
     bits 16-23 opcode of the instruction in progress (0 at a boundary)
     bits 24-31 remaining ticks in the currently priced bus cycle (ownership
       hold contributes no extra ticks)
   CPU_LATCH packs the CPU's working values:
     bits 0-15  effective-address latch
     bits 16-23 temporary data latch
     bits 24-31 cycle cost model id (0 is the PLACEHOLDER flat model)
   MAPCTL reports the instance-owned byte written by guest code; it is host
   latch observation, not guest readback. Together the nine registers expose
   every non-RAM machine field a snapshot holds except guest time
   (lynx_guest_ticks). */
enum {
    LYNX_REG_PC = 0,
    LYNX_REG_A = 1,
    LYNX_REG_X = 2,
    LYNX_REG_Y = 3,
    LYNX_REG_S = 4,
    LYNX_REG_P = 5,
    LYNX_REG_CPU_STATE = 6,
    LYNX_REG_CPU_LATCH = 7,
    LYNX_REG_MAPCTL = 8,
    LYNX_REG_COUNT = 9
};

/* Size of the guest address space in bytes. */
#define LYNX_ADDRESS_SPACE 0x10000u

typedef struct lynx_config {
    uint32_t struct_size; /* must equal sizeof(lynx_config) */
    uint32_t profile;     /* LYNX_PROFILE_*; fixed for the life of the instance */
    uint32_t ram_policy;  /* LYNX_RAM_POLICY_* */
    uint32_t reserved;    /* must be zero */
    uint64_t ram_seed;    /* SplitMix64 start state; unused by the ZERO policy */
} lynx_config;

/* Fills cfg with the defaults: profile NONE, RAM policy ZERO (fixed 0x00),
   seed 0. A NULL cfg is ignored. */
void lynx_config_default(lynx_config *cfg);

/* The stable name of a profile id ("none", "lynx1", "lynx2"), or NULL for an
   id this build does not define. The string is static; never free it. */
const char *lynx_profile_name(uint32_t profile);

/* Opaque machine instance. */
typedef struct lynx_machine lynx_machine;

/* Size and alignment of the memory lynx_machine_init needs. Both are
   constant for a given build. */
size_t lynx_machine_size(void);
size_t lynx_machine_align(void);

/* Builds a machine in place in mem, which must hold at least
   lynx_machine_size() bytes and be aligned to lynx_machine_align(). On
   success *out points at the machine (the same address as mem). A refusal
   returns a status and writes nothing to mem or *out; an unknown profile id
   is refused with LYNX_ERR_ARGUMENT. The config is copied: changing *cfg
   afterwards has no effect. Instances share no mutable state. */
lynx_status lynx_machine_init(void *mem, size_t size, const lynx_config *cfg,
                              lynx_machine **out);

/* Marks the machine unusable. A NULL or already deinitialised machine is
   ignored. The caller still owns the memory. */
void lynx_machine_deinit(lynx_machine *m);

/* Returns the machine to the declared power-on state: RAM filled by the
   configured policy and seed, the CPU at its declared power-on values (A, X,
   Y zero, S 0xFD, P 0x34; unverified for the Lynx), guest time zero, no
   content loaded. The profile is kept. The result is byte-identical to a
   fresh lynx_machine_init with the same config. The host reloads content
   after a reset. A NULL or
   deinitialised machine returns a status and changes nothing. This is a
   Phase 1 choice: reset clears loaded content, and a later phase may add a
   startup-state reset beside it. */
lynx_status lynx_reset(lynx_machine *m);

/* Content format and startup strategy recorded in lynx_identity. Value 1 is
   reserved for the private CPU-vector test lane and is not publicly loadable. */
enum {
    LYNX_CONTENT_NONE = 0,
    LYNX_CONTENT_BS93 = 2
};
enum { LYNX_STARTUP_NONE = 0, LYNX_STARTUP_A = 1 };

/* Load a complete BS93/BLL tier-A object. The strict accepted subset has a
   10-byte header: 80 08, big-endian load address, big-endian total object
   length including the header, and BS93. The payload must be non-empty, its
   declared and supplied lengths must match exactly, and its address range
   must fit in 64 KiB. The CPU starts at the payload address after the declared
   power-on state is applied. The core hashes the complete object and copies
   only its payload into owned RAM. Every refusal leaves the full instance
   byte-identical. */
lynx_status lynx_load_bll(lynx_machine *m, const uint8_t *bytes, size_t len);

/* Runs the machine for at most budget master ticks (16 MHz). *consumed is the
   number of ticks actually used and never exceeds budget. *stop says why the
   call returned. Needs loaded content.

   Budgets are half-open intervals of guest time: advancing to T = start +
   budget performs every bus cycle that can start before T. A cycle that
   starts before T is performed even if its duration extends past T; the next
   call resumes when it ends. A cycle due exactly at T is not started.
   *consumed equals budget unless the run stops with LYNX_STOP_UNDEFINED or
   the guest-time limit is reached, so a BUDGET stop may fall in the middle
   of an instruction; the registers then show the state after the last
   performed cycle (PC is the next byte to fetch, not the instruction start).
   Splitting a run into any sequence of budgets gives the same state at the
   same guest time. A budget of 0 changes nothing.

   Under profile NONE the run stops with LYNX_STOP_UNDEFINED at an instruction
   boundary whose opcode the Lynx profiles define differently, once the
   previous cycle has ended, without fetching it: nothing further is consumed
   or changed, and a repeated call stops at the same place with *consumed 0.

   A public BLL machine stops with LYNX_STOP_UNAVAILABLE when a due CPU request
   maps to an unimplemented Suzy/Mikey target, absent ROM/vector content, or a
   MAPCTL read. Only writes to the MAPCTL latch are implemented there. The
   unavailable request is not completed: its CPU request and backing bytes are
   preserved at the grant. *consumed includes prior completed cycles in the
   call; a repeated non-zero-budget call at the same request returns
   LYNX_OK/LYNX_STOP_UNAVAILABLE with *consumed 0. This stop takes precedence
   over LYNX_STOP_UNDEFINED for an unavailable opcode fetch. It is a project
   limitation, not a claim about physical readback or bus electrical values.

   Guest time never wraps: no cycle is started whose end could come within
   65,536 ticks of the 64-bit limit (guest time never exceeds UINT64_MAX -
   65,536). At that limit lynx_advance is not an error and has no stop reason
   of its own: it returns LYNX_OK with LYNX_STOP_BUDGET and a *consumed below
   budget, and *consumed is 0 once the limit is reached. A host loop that
   subtracts *consumed from a remaining budget must therefore treat a
   LYNX_STOP_BUDGET return with *consumed 0 and a non-zero budget as the end
   of guest time, not as progress to retry. */
lynx_status lynx_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                         lynx_stop_reason *stop);

/* A one-shot write-watch event. hit is 1 when a CPU bus write matched the
   configured range; otherwise all fields are zero. hit_tick is the access
   sample tick (currently the grant tick), and return_tick is one tick later. */
typedef struct lynx_watch_event {
    uint32_t hit;
    uint32_t address;
    uint32_t value; /* low 8 bits contain the written byte */
    uint64_t hit_tick;
    uint64_t return_tick;
} lynx_watch_event;

/* Configure one bounded, non-empty, half-open watched range [addr, addr+len),
   clear the prior event, and arm it for the next matching actual CPU write.
   A hit returns LYNX_STOP_WATCH only after the bus write, CPU request
   completion and next-ready tick are committed. The return tick is grant+1,
   even when the cycle's remaining duration is later; consumed never exceeds
   the half-open budget. A request due exactly at the budget target is not
   started. A hit disarms the watch but preserves its event until reconfigure
   or clear. Configure and clear do not advance guest time.

   Calls on one instance must be caller-serialized. Watches are per-instance
   host observation: they are not guest state, guest digests or snapshots. A
   successful reset or load clears watch state; a refused operation preserves
   it. There are no callbacks and no caller pointers are retained. */
lynx_status lynx_watch_configure(lynx_machine *m, uint32_t addr, size_t len);
lynx_status lynx_watch_clear(lynx_machine *m);
/* Copy the last event, or an all-zero event when no hit is pending. out must
   not overlap any byte of the machine instance. */
lynx_status lynx_watch_get(const lynx_machine *m, lynx_watch_event *out);

/* Inspect functions never change machine state or guest time. Output ranges
   must not overlap any byte of the machine instance. lynx_read_memory reads
   the underlying raw 64 KiB RAM, separately from the CPU bus view. */
lynx_status lynx_read_register(const lynx_machine *m, uint32_t reg, uint32_t *value);
lynx_status lynx_read_memory(const lynx_machine *m, uint32_t addr, uint8_t *out, size_t len);

/* Side-effect-free CPU-mapped RAM peek. The entire address range, output
   ownership and every mapped route are checked before any byte is copied.
   NULL pointers or zero length return ARGUMENT; an out-of-range request returns
   RANGE; if any byte maps to an unimplemented device, absent ROM/vector target
   or MAPCTL read, the whole request returns UNAVAILABLE. A refusal leaves the
   entire output range, machine and guest time unchanged. A successful request
   can read only mapped RAM and never performs a CPU bus cycle. */
lynx_status lynx_read_cpu_memory(const lynx_machine *m, uint32_t addr, uint8_t *out,
                                 size_t len);

/* Guest time in master ticks since power-on or the last load. */
lynx_status lynx_guest_ticks(const lynx_machine *m, uint64_t *ticks);

/* Run identity: what this instance was configured with and what it holds.
   content_sha256 is computed inside the core from the complete accepted input
   object; the host never supplies it. content_addr/content_len identify the
   payload in RAM, while object_len includes any container header. entry_addr
   is the first CPU address. Before any load, the content and startup fields
   are zero. reserved is always zero. */
typedef struct lynx_identity {
    uint32_t profile;    /* LYNX_PROFILE_* */
    uint32_t ram_policy; /* LYNX_RAM_POLICY_* */
    uint64_t ram_seed;
    uint32_t has_content;
    uint32_t content_addr;
    uint32_t content_len;
    uint32_t reserved;
    uint8_t content_sha256[32];
    uint32_t content_format; /* LYNX_CONTENT_* */
    uint32_t startup_tier;   /* LYNX_STARTUP_* */
    uint32_t entry_addr;
    uint32_t object_len;
} lynx_identity;

/* Side-effect-free inspect. NULL m or out gives LYNX_ERR_ARGUMENT; a
   machine that is not initialised gives LYNX_ERR_STATE. *out is written only
   on success. */
lynx_status lynx_get_identity(const lynx_machine *m, lynx_identity *out);

/* Identity of this core build, as a NUL-terminated string of the form
   "git=<rev>[+dirty] cc=<id>-<version> sys=<name>-<processor>" (git=nogit
   when the build had no git checkout). Fixed at configure time, shorter than
   128 bytes, with no path, date or clock time. Two calls return the same
   pointer. A snapshot restores only into an identical build id. */
const char *lynx_build_id(void);

/* Snapshots. A snapshot is a versioned blob: a header that names the build,
   profile, RAM policy, seed and loaded content, then the machine state with a
   SHA-256 of that state. All integers in it are little-endian. Format 3 adds
   content format, startup tier, entry, object length and MAPCTL. It is
   documented beside its code in core/snapshot.c; versions 1 and 2 are refused.

   A snapshot restores only into an instance of the same build (the full
   lynx_build_id string), with the same profile, RAM policy and seed and the
   same loaded content (same bytes at the same address). Anything else is
   refused, so no snapshot from an older or different build is ever accepted.

   lynx_snapshot_size gives the exact size of every snapshot of this build; it
   needs only an initialised machine. lynx_snapshot_save writes that many
   bytes to buf and sets *written to the count; it needs loaded content, a buf
   of at least cap bytes with cap at least the size, and a buf that does not
   overlap the machine. Neither call changes the machine. A refused save
   writes nothing to buf and nothing to *written.

   lynx_snapshot_restore checks everything before it writes anything: the
   exact length, the header, the identity against this instance, the payload
   hash, then every field value. Only then does it apply the state. A refused
   restore leaves the machine byte-identical. LYNX_ERR_SNAPSHOT_INVALID means
   malformed or corrupted, LYNX_ERR_SNAPSHOT_VERSION an unsupported format
   version, LYNX_ERR_SNAPSHOT_MISMATCH another build or identity. A state no
   run reaches is malformed: a CPU field combination real execution never
   produces, or a guest time above UINT64_MAX - 65,536 (the lynx_advance
   limit), is refused as LYNX_ERR_SNAPSHOT_INVALID. */
lynx_status lynx_snapshot_size(const lynx_machine *m, size_t *size);
lynx_status lynx_snapshot_save(const lynx_machine *m, uint8_t *buf, size_t cap, size_t *written);
lynx_status lynx_snapshot_restore(lynx_machine *m, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LYNXPLOSION_H */
