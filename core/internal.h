/* Core-internal declarations. Not part of the public API.
   External names here start with lynx__ (two underscores). */
#ifndef LYNX_INTERNAL_H
#define LYNX_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus.h"
#include "cpu.h"
#include "lynxplosion.h"

/* Snapshot format version 5 (core/snapshot.c documents the layout). */
#define LYNX__SNAPSHOT_MAGIC "LYNXSNAP" /* 8 bytes, no terminating NUL in the blob */
#define LYNX__SNAPSHOT_MAGIC_LEN 8u
#define LYNX__SNAPSHOT_FORMAT 5u
#define LYNX__BUILD_ID_FIELD 128u
#define LYNX__SNAPSHOT_HEADER_LEN 256u  /* prior header + timing-policy identity */
#define LYNX__VIDEO_CAPTURE_PENS 8160u
#define LYNX__VIDEO_CAPTURE_PALETTES 3264u
#define LYNX__VIDEO_CAPTURE_TICKS 102u
#define LYNX__VIDEO_DMA_TICKS 5u
#define LYNX__VIDEO_REFRESH_PERIOD 256u
#define LYNX__VIDEO_SNAPSHOT_HEADER 72u
#define LYNX__VIDEO_SNAPSHOT_LEN (LYNX__VIDEO_SNAPSHOT_HEADER + 32u + LYNX__VIDEO_CAPTURE_PENS + LYNX__VIDEO_CAPTURE_PALETTES + 8u * LYNX__VIDEO_CAPTURE_TICKS)
#define LYNX__SNAPSHOT_PAYLOAD_LEN (65604u + LYNX__VIDEO_SNAPSHOT_LEN)

/* The payload layout belongs to the format number: a layout change without a
   format bump stops the build. */
_Static_assert(LYNX__SNAPSHOT_FORMAT != 5u || LYNX__SNAPSHOT_PAYLOAD_LEN == 77948u,
               "snapshot format 5 has a 77948-byte payload; bump the format to change it");

/* Build identity string baked at configure time (core/build_id.c.in). */
extern const char lynx__build_id[];

#define LYNX__MAGIC 0x4C594E58u

/* No bus cycle starts whose end could come within this many ticks of the
   64-bit guest-time limit. Larger than any cycle cost (costs are uint8_t). */
#define LYNX__TICK_HEADROOM 0x10000u

#define LYNX__TIMER_COUNT 8u
#define LYNX__TIMER_IMPLEMENTED_COUNT LYNX__TIMER_COUNT
#define LYNX__TIMER_PHASE_FIXED_ZERO_1 1u

typedef struct lynx__timer {
    uint8_t backup;
    uint8_t count;
    uint8_t control_a;
    uint8_t control_b;
} lynx__timer;

/* The event cursor is canonical only for the current guest tick. Advancing
   guest time clears it; snapshots store the current-tick bit and reconstruct
   its timestamp from guest_ticks instead of preserving historical CPU ticks. */
typedef struct lynx__timer_bank {
    lynx__timer timers[LYNX__TIMER_COUNT];
    uint8_t pending;
    uint8_t phase_policy;
    uint8_t last_event_valid;
    uint64_t last_event_tick;
} lynx__timer_bank;

/* Private scheduler ordering contract. The comparator is shared with the
   scheduler-order test so traversal permutations exercise production logic. */
typedef struct lynx__machine_event {
    uint64_t tick;
    uint8_t priority;
    uint8_t sequence;
} lynx__machine_event;

enum {
    LYNX__MACHINE_EVENT_TIMER_PRIORITY = 0u,
    LYNX__MACHINE_EVENT_VIDEO_PRIORITY = 1u,
    LYNX__MACHINE_EVENT_REFRESH_PRIORITY = 2u,
    LYNX__MACHINE_EVENT_CPU_PRIORITY = 3u,
    LYNX__MACHINE_EVENT_SEQUENCE = 0u,
};

typedef struct lynx__video_state {
    uint8_t dispctl, pbkup, frame_synced, line_ordinal;
    uint8_t row_active, capture_rows, dma_cursor, capture_partial;
    uint16_t dispaddr, backup_addr, input_mask;
    uint8_t lefthand, timing_policy;
    uint64_t dma_next_tick, dma_busy_until;
    uint64_t refresh_next_tick, refresh_busy_until;
    uint64_t frame_start_tick, frame_end_tick, frame_sequence;
    uint8_t palette[32];
    uint8_t pens[LYNX__VIDEO_CAPTURE_PENS];
    uint8_t palettes[LYNX__VIDEO_CAPTURE_PALETTES];
    uint64_t line_ticks[LYNX__VIDEO_CAPTURE_TICKS];
} lynx__video_state;

/* Host-side debugger observations are deliberately separate from guest
   machine state. Snapshot serialization and guest digests omit this object. */
typedef struct lynx__host_observation {
    uint32_t watch_armed;
    uint32_t watch_addr;
    uint32_t watch_len;
    lynx_watch_event watch_event;
} lynx__host_observation;

/* One machine instance. The caller's buffer is the only mutable state.
   Init zero-fills the whole struct, so padding bytes are always zero. */
struct lynx_machine {
    uint32_t magic;
    lynx_config cfg;
    uint8_t ram[LYNX_ADDRESS_SPACE];
    lynx__cpu cpu;
    uint8_t cost_model; /* LYNX__COST_*: prices every bus cycle; flat5 after power-on */
    uint32_t has_content, content_addr, content_len;
    uint32_t content_format, startup_tier, entry_addr, object_len;
    uint8_t content_sha256[32]; /* SHA-256 of the accepted image bytes; zero before a load */
    uint8_t mapctl; /* provisional byte latch; all written bits are retained */
    uint8_t cpu_page_latch; /* zero invalid; page+1 for the latest eligible RAM access */
    uint8_t bus_hold; /* private deterministic grant gate; no public sleep claim */
    uint8_t wake_pending; /* pending IRQ requests wake independently of CPU I */
    uint64_t guest_ticks;       /* the only clock */
    uint64_t cpu_ready_at;      /* end of the currently priced cycle, not an ownership delay */
    lynx__timer_bank timer_bank;
    lynx__video_state video;
    lynx__host_observation host_observation;
};

/* Applies the configured RAM power-on policy, puts the CPU in its declared
   power-on state, resets the cost model to flat5 and clears guest time and
   the content fields. Used by init and by load. */
void lynx__power_on(lynx_machine *m);

/* Test seam: select the cycle cost model of an initialised instance. An id
   this build does not define (or a NULL or uninitialised machine) is refused
   with LYNX_ERR_ARGUMENT and nothing is written. Reset and load return the
   model to flat5. */
lynx_status lynx__machine_set_cost_model(struct lynx_machine *m, uint32_t model);

/* Private deterministic ownership seam. Values other than 0 or 1, NULL, and
   uninitialised instances are refused without changing the machine. */
lynx_status lynx__machine_set_bus_hold(struct lynx_machine *m, uint32_t hold);

void lynx__timer_init(lynx__timer_bank *bank);
bool lynx__timer_next(const lynx__timer_bank *bank, uint64_t now, uint64_t *tick);
uint8_t lynx__timer_apply(lynx__timer_bank *bank, uint64_t tick);
void lynx__timer_mark_tick(lynx__timer_bank *bank, uint64_t tick);
bool lynx__timer_available(uint16_t addr);
uint8_t lynx__timer_peek(const lynx__timer_bank *bank, uint16_t addr);
bool lynx__timer_write(lynx__timer_bank *bank, uint16_t addr, uint8_t value);
bool lynx__machine_event_before(lynx__machine_event a, lynx__machine_event b);
void lynx__video_init(lynx__video_state *video, uint32_t timing_policy);
bool lynx__video_available(uint16_t addr);
uint8_t lynx__video_peek(const lynx__video_state *video, uint16_t addr);
bool lynx__video_write(lynx__video_state *video, uint16_t addr, uint8_t value);
bool lynx__video_next_dma(const lynx__video_state *video, uint64_t now, uint64_t *tick);
bool lynx__video_next_refresh(const lynx__video_state *video, uint64_t now, uint64_t *tick);
void lynx__video_dma_apply(lynx__video_state *video, const uint8_t *ram, uint64_t tick);
void lynx__video_refresh_apply(lynx__video_state *video, uint64_t tick);
bool lynx__video_timer_apply(lynx__video_state *video, uint8_t borrow_mask, uint64_t tick);
void lynx__video_copy_frame(const lynx__video_state *video, const lynx_frame_buffers *buffers);
void lynx__video_discard_frame(lynx__video_state *video, uint64_t next_start_tick);
lynx_status lynx__input_set(lynx__video_state *video, uint32_t mask);
bool lynx__input_available(uint16_t addr);
uint8_t lynx__input_peek(const lynx__video_state *video, uint16_t addr);
bool lynx__input_write(lynx__video_state *video, uint16_t addr, uint8_t value);

#endif /* LYNX_INTERNAL_H */
