/* Lynxplosion public C API.

   One machine instance lives in memory the caller provides. The core never
   allocates, reads a clock, touches a file or starts a thread. Every function
   that can refuse input checks all of it first and leaves the machine
   byte-identical when it refuses.

   PROVISIONAL: the machine behind this API is a skeleton. It is not a CPU and
   makes no accuracy claim. The raw RAM image loader below is replaced by the
   real direct-load format in Phase 3. */
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
    LYNX_ERR_CAPACITY = 9           /* output buffer smaller than the snapshot */
};

/* Why lynx_advance returned. FRAME and WATCH are reserved: later phases
   report them, Phase 1 never does. */
typedef uint32_t lynx_stop_reason;
enum {
    LYNX_STOP_BUDGET = 0, /* the tick budget was used up */
    LYNX_STOP_FRAME = 1,  /* reserved */
    LYNX_STOP_WATCH = 2,  /* reserved */
    LYNX_STOP_HALT = 3    /* the machine halted */
};

/* Hardware profile. No Lynx I or Lynx II profile is named yet (owner
   decision O1 is open), so the only value is NONE. */
enum { LYNX_PROFILE_NONE = 0 };

/* How RAM is filled at power-on and at reset. ZERO fills every byte with
   0x00 and is the default everywhere. SPLITMIX64 fills RAM from the
   SplitMix64 generator (public domain) started at ram_seed: eight bytes per
   output, least significant byte first, from address 0. */
enum { LYNX_RAM_POLICY_ZERO = 0, LYNX_RAM_POLICY_SPLITMIX64 = 1 };

/* Register ids for lynx_read_register. */
enum {
    LYNX_REG_PC = 0,
    LYNX_REG_A = 1,
    LYNX_REG_X = 2,
    LYNX_REG_Y = 3,
    LYNX_REG_S = 4,
    LYNX_REG_P = 5,
    LYNX_REG_COUNT = 6
};

/* Size of the guest address space in bytes. */
#define LYNX_ADDRESS_SPACE 0x10000u

typedef struct lynx_config {
    uint32_t struct_size; /* must equal sizeof(lynx_config) */
    uint32_t profile;     /* LYNX_PROFILE_* */
    uint32_t ram_policy;  /* LYNX_RAM_POLICY_* */
    uint32_t reserved;    /* must be zero */
    uint64_t ram_seed;    /* SplitMix64 start state; unused by the ZERO policy */
} lynx_config;

/* Fills cfg with the defaults: no profile, RAM policy ZERO (fixed 0x00),
   seed 0. A NULL cfg is ignored. */
void lynx_config_default(lynx_config *cfg);

/* Opaque machine instance. */
typedef struct lynx_machine lynx_machine;

/* Size and alignment of the memory lynx_machine_init needs. Both are
   constant for a given build. */
size_t lynx_machine_size(void);
size_t lynx_machine_align(void);

/* Builds a machine in place in mem, which must hold at least
   lynx_machine_size() bytes and be aligned to lynx_machine_align(). On
   success *out points at the machine (the same address as mem). A refusal
   returns a status and writes nothing to mem or *out. Instances share no
   mutable state. */
lynx_status lynx_machine_init(void *mem, size_t size, const lynx_config *cfg,
                              lynx_machine **out);

/* Marks the machine unusable. A NULL or already deinitialised machine is
   ignored. The caller still owns the memory. */
void lynx_machine_deinit(lynx_machine *m);

/* Returns the machine to the declared power-on state: RAM filled by the
   configured policy and seed, registers and guest time zero, no content
   loaded. The result is byte-identical to a fresh lynx_machine_init with the
   same config. The host reloads content after a reset. A NULL or
   deinitialised machine returns a status and changes nothing. This is a
   Phase 1 choice: reset clears loaded content, and a later phase may add a
   startup-state reset beside it. */
lynx_status lynx_reset(lynx_machine *m);

/* PROVISIONAL raw RAM image loader, replaced in Phase 3. Re-applies the
   power-on RAM policy, copies len bytes of bytes to load_addr and sets the
   program counter to load_addr. Every check happens before the first write,
   so a refused load leaves the machine byte-identical. Refused: NULL bytes,
   len of zero, len above LYNX_ADDRESS_SPACE, load_addr at or above
   LYNX_ADDRESS_SPACE, and an image that does not end inside the address
   space. */
lynx_status lynx_load_ram_image(lynx_machine *m, const uint8_t *bytes, size_t len,
                                uint32_t load_addr);

/* Runs the machine for at most budget master ticks (16 MHz). *consumed is the
   number of ticks actually used and never exceeds budget. *stop says why the
   call returned. A budget smaller than one machine step consumes nothing and
   reports LYNX_STOP_BUDGET. Guest time never wraps: the budget is clamped so
   that it stays below the 64-bit limit. Needs loaded content. */
lynx_status lynx_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                         lynx_stop_reason *stop);

/* Inspect functions never change machine state or guest time. */
lynx_status lynx_read_register(const lynx_machine *m, uint32_t reg, uint32_t *value);
lynx_status lynx_read_memory(const lynx_machine *m, uint32_t addr, uint8_t *out, size_t len);

/* Guest time in master ticks since power-on or the last load. */
lynx_status lynx_guest_ticks(const lynx_machine *m, uint64_t *ticks);

/* Run identity: what this instance was configured with and what it holds.
   content_sha256 is computed inside the core from the bytes the loader
   accepted; the host never supplies it. Before any load, has_content is 0 and
   content_addr, content_len and content_sha256 are all zero. reserved is
   always zero. */
typedef struct lynx_identity {
    uint32_t profile;    /* LYNX_PROFILE_* */
    uint32_t ram_policy; /* LYNX_RAM_POLICY_* */
    uint64_t ram_seed;
    uint32_t has_content;
    uint32_t content_addr;
    uint32_t content_len;
    uint32_t reserved;
    uint8_t content_sha256[32];
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
   SHA-256 of that state. All integers in it are little-endian. The format is
   version 1 and is documented beside its code in core/snapshot.c.

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
   version, LYNX_ERR_SNAPSHOT_MISMATCH another build or identity. */
lynx_status lynx_snapshot_size(const lynx_machine *m, size_t *size);
lynx_status lynx_snapshot_save(const lynx_machine *m, uint8_t *buf, size_t cap, size_t *written);
lynx_status lynx_snapshot_restore(lynx_machine *m, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LYNXPLOSION_H */
