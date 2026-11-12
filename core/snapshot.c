/* Snapshot save and restore, format version 5.

   A snapshot is a header followed by a payload. Every integer is
   little-endian and is written and read one field at a time, never by copying
   a struct, so padding bytes and host byte order never reach the blob.

   Header (256 bytes), in order:
     0    magic          8 bytes  "LYNXSNAP"
     8    format_version u32      4
     12   header_len     u32      252
     16   build_id       128      lynx__build_id, NUL-padded to 128
     144  profile        u32
     148  ram_policy     u32
     152  ram_seed       u64
     160  content_sha256 32
     192  content_addr   u32
     196  content_len    u32
     200  content_format u32
     204  startup_tier   u32
     208  entry_addr     u32
     212  object_len     u32
     216  payload_len    u32      65604
     220  payload_sha256 32       SHA-256 of the payload
     252  timing_policy  u32
   Payload (77948 bytes), offsets relative to its start:
     0  pc u16        2  a      3  x      4  y      5  s
     6  p (bits 4 and 5 set)    7  cpu latches (lynx__cpu.flags)
     8  opcode        9  step (cycle index inside the instruction)
     10 ea u16        12 tmp    13 cost_model     14 mapctl u8  15 reserved u8, zero
     16 guest_ticks u64         24 cpu_ready_at u64
     32 ram 65536 bytes
     65568 eight timer records (backup, count, CTLA, CTLB) in timer order
     65600 timer pending IRQ bits
     65601 fixed timer phase policy
     65602 event-already-applied-at-guest-tick (0 or 1)
     65603 bus ownership hold (0 or 1)
     65604 video/input runtime header and bounded raw capture (format 5)
   The profile is not in the payload: it is part of the header identity. The
   CPU request and wake_pending are derived state and are never stored; restore
   recomputes them from CPU state and timer pending bits. Divider phase and
   candidate deadlines derive from guest_ticks and timer controls. The event
   cursor stores only whether an event at the current tick was already applied;
   its tick is reconstructed from guest_ticks. Older cursor timestamps are
   incidental bus history and are normalized away as guest time advances.

   Restore accepts a blob only for an instance with the identical build id,
   profile, RAM policy, seed and loaded content: the identity comes from the
   instance, so a blob cannot assert its own compatibility. It validates in a
   fixed order, all before the first write, and then applies in one step that
   cannot fail. A refused restore leaves the instance byte-identical. After
   that guest-state apply succeeds, host-only watch configuration and hit
   reporting are cleared; callers must explicitly rearm after restore.
   Formats 1 through 4 are refused as version errors.

   Reversibility: restore needs the same build, so no old snapshot is ever
   accepted and the format may change in any later build without migration. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"
#include "sha256.h"

/* Header field offsets. */
enum {
    OFF_MAGIC = 0,
    OFF_FORMAT = 8,
    OFF_HEADER_LEN = 12,
    OFF_BUILD_ID = 16,
    OFF_PROFILE = OFF_BUILD_ID + (int)LYNX__BUILD_ID_FIELD,
    OFF_POLICY = OFF_PROFILE + 4,
    OFF_SEED = OFF_POLICY + 4,
    OFF_CONTENT_SHA = OFF_SEED + 8,
    OFF_CONTENT_ADDR = OFF_CONTENT_SHA + 32,
    OFF_CONTENT_LEN = OFF_CONTENT_ADDR + 4,
    OFF_CONTENT_FORMAT = OFF_CONTENT_LEN + 4,
    OFF_STARTUP_TIER = OFF_CONTENT_FORMAT + 4,
    OFF_ENTRY_ADDR = OFF_STARTUP_TIER + 4,
    OFF_OBJECT_LEN = OFF_ENTRY_ADDR + 4,
    OFF_PAYLOAD_LEN = OFF_OBJECT_LEN + 4,
    OFF_PAYLOAD_SHA = OFF_PAYLOAD_LEN + 4,
    OFF_TIMING_POLICY = OFF_PAYLOAD_SHA + 32,
    OFF_END = OFF_TIMING_POLICY + 4
};

/* Payload field offsets, relative to the start of the payload. */
enum {
    P_PC = 0,
    P_A = 2,
    P_X = 3,
    P_Y = 4,
    P_S = 5,
    P_P = 6,
    P_FLAGS = 7,
    P_OPCODE = 8,
    P_STEP = 9,
    P_EA = 10,
    P_TMP = 12,
    P_COST_MODEL = 13,
    P_MAPCTL = 14,
    P_RESERVED = 15,
    P_TICKS = 16,
    P_READY_AT = 24,
    P_RAM = 32,
    P_TIMERS = P_RAM + (int)LYNX_ADDRESS_SPACE,
    P_TIMER_PENDING = P_TIMERS + (int)(LYNX__TIMER_COUNT * 4u),
    P_TIMER_PHASE_POLICY = P_TIMER_PENDING + 1,
    P_TIMER_EVENT_APPLIED = P_TIMER_PHASE_POLICY + 1,
    P_BUS_HOLD = P_TIMER_EVENT_APPLIED + 1,
    P_VIDEO = P_BUS_HOLD + 1,
    P_END = P_VIDEO + (int)LYNX__VIDEO_SNAPSHOT_LEN
};

_Static_assert(OFF_END == (int)LYNX__SNAPSHOT_HEADER_LEN, "snapshot header length");
_Static_assert(P_END == (int)LYNX__SNAPSHOT_PAYLOAD_LEN, "snapshot payload length");
_Static_assert(P_RESERVED + 1 == P_TICKS, "snapshot byte fields meet the clock fields");
_Static_assert(P_TIMERS == 65568, "timer bank follows RAM without padding");

#define SNAPSHOT_TOTAL ((size_t)LYNX__SNAPSHOT_HEADER_LEN + (size_t)LYNX__SNAPSHOT_PAYLOAD_LEN)

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64(uint8_t *p, uint64_t v)
{
    put_u32(p, (uint32_t)v);
    put_u32(p + 4, (uint32_t)(v >> 32));
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}

/* The build id, NUL-padded to the 128-byte header field. The configure step
   refuses an id of 128 characters or more, and the copy is bounded here too. */
static void padded_build_id(uint8_t field[LYNX__BUILD_ID_FIELD])
{
    memset(field, 0, LYNX__BUILD_ID_FIELD);
    for (size_t i = 0u; i + 1u < LYNX__BUILD_ID_FIELD && lynx__build_id[i] != '\0'; i++) {
        field[i] = (uint8_t)lynx__build_id[i];
    }
}

/* True when [a, a+alen) and [b, b+blen) share no byte. Subtraction form, so
   no sum of an address and a length can wrap. */
static int ranges_disjoint(const void *a, size_t alen, const void *b, size_t blen)
{
    uintptr_t ua = (uintptr_t)a;
    uintptr_t ub = (uintptr_t)b;
    if (ua >= ub) {
        return (ua - ub) >= blen;
    }
    return (ub - ua) >= alen;
}

static int machine_ok(const lynx_machine *m)
{
    return m->magic == LYNX__MAGIC;
}

/* Timer-next only needs to know whether the current edge was already applied.
   Clear a local copy to derive whether a timer edge would otherwise be emitted
   at this exact guest tick; no deadline or historical bus tick is serialized. */
static int timer_edge_due_now(const lynx__timer_bank *bank, uint64_t now)
{
    lynx__timer_bank unmarked = *bank;
    uint64_t next = 0u;
    unmarked.last_event_valid = 0u;
    unmarked.last_event_tick = 0u;
    return lynx__timer_next(&unmarked, now, &next) && next == now;
}

static int timer_link_supported(uint8_t index)
{
    return index == 2u || index == 3u || index == 4u || index == 5u || index == 7u;
}

static int timer_bank_valid(const lynx__timer_bank *bank, uint64_t ticks,
                            uint8_t event_applied)
{
    if (bank->phase_policy != (uint8_t)LYNX__TIMER_PHASE_FIXED_ZERO_1 ||
        bank->last_event_valid > 1u || event_applied > 1u) {
        return 0;
    }
    for (uint8_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        const lynx__timer *timer = &bank->timers[i];
        const uint8_t source = (uint8_t)(timer->control_a & 0x07u);
        const uint8_t status = (uint8_t)(timer->control_b & 0x07u);
        if ((timer->control_b & 0xF0u) != 0u ||
            (status != 0u && status != 0x04u && status != 0x05u &&
             status != 0x06u && status != 0x07u) ||
            (source == 0x07u && (timer->control_a & 0x08u) != 0u &&
             !timer_link_supported(i))) {
            return 0;
        }
    }
    if (event_applied != 0u &&
        (ticks == 0u || bank->last_event_valid != 1u ||
         bank->last_event_tick != ticks || !timer_edge_due_now(bank, ticks))) {
        return 0;
    }
    if (event_applied == 0u && bank->last_event_valid != 0u) {
        return 0;
    }
    return 1;
}

static uint64_t max_public_ready_span(uint8_t cost_model)
{
    /* Every granted cycle returns after at least one tick has elapsed. The
       table's largest costs are flat5=5 and test_skew=14, so the largest
       positive remaining span is 4 or 13 respectively. */
    switch (cost_model) {
    case LYNX__COST_FLAT5:
        return 4u;
    case LYNX__COST_TEST_SKEW:
        return 13u;
    default:
        return 0u;
    }
}

static void video_snapshot_write(uint8_t *out, const lynx__video_state *video)
{
    out[0] = video->dispctl; out[1] = video->pbkup;
    out[2] = video->frame_synced; out[3] = video->line_ordinal;
    out[4] = video->row_active; out[5] = video->capture_rows;
    out[6] = video->dma_cursor; out[7] = video->capture_partial;
    put_u16(out + 8, video->dispaddr); put_u16(out + 10, video->backup_addr);
    put_u16(out + 12, video->input_mask);
    out[14] = video->lefthand; out[15] = video->timing_policy;
    put_u64(out + 16, video->dma_next_tick); put_u64(out + 24, video->dma_busy_until);
    put_u64(out + 32, video->refresh_next_tick); put_u64(out + 40, video->refresh_busy_until);
    put_u64(out + 48, video->frame_start_tick); put_u64(out + 56, video->frame_end_tick);
    put_u64(out + 64, video->frame_sequence);
    memcpy(out + LYNX__VIDEO_SNAPSHOT_HEADER, video->palette, sizeof video->palette);
    memcpy(out + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette,
           video->pens, sizeof video->pens);
    memcpy(out + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette + sizeof video->pens,
           video->palettes, sizeof video->palettes);
    uint8_t *ticks = out + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette +
                     sizeof video->pens + sizeof video->palettes;
    for (size_t i = 0u; i < LYNX__VIDEO_CAPTURE_TICKS; i++) put_u64(ticks + i * 8u, video->line_ticks[i]);
}

static void video_snapshot_read(lynx__video_state *video, const uint8_t *in)
{
    memset(video, 0, sizeof *video);
    video->dispctl = in[0]; video->pbkup = in[1];
    video->frame_synced = in[2]; video->line_ordinal = in[3];
    video->row_active = in[4]; video->capture_rows = in[5];
    video->dma_cursor = in[6]; video->capture_partial = in[7];
    video->dispaddr = get_u16(in + 8); video->backup_addr = get_u16(in + 10);
    video->input_mask = get_u16(in + 12);
    video->lefthand = in[14]; video->timing_policy = in[15];
    video->dma_next_tick = get_u64(in + 16); video->dma_busy_until = get_u64(in + 24);
    video->refresh_next_tick = get_u64(in + 32); video->refresh_busy_until = get_u64(in + 40);
    video->frame_start_tick = get_u64(in + 48); video->frame_end_tick = get_u64(in + 56);
    video->frame_sequence = get_u64(in + 64);
    memcpy(video->palette, in + LYNX__VIDEO_SNAPSHOT_HEADER, sizeof video->palette);
    memcpy(video->pens, in + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette, sizeof video->pens);
    memcpy(video->palettes, in + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette +
           sizeof video->pens, sizeof video->palettes);
    const uint8_t *ticks = in + LYNX__VIDEO_SNAPSHOT_HEADER + sizeof video->palette +
                           sizeof video->pens + sizeof video->palettes;
    for (size_t i = 0u; i < LYNX__VIDEO_CAPTURE_TICKS; i++) video->line_ticks[i] = get_u64(ticks + i * 8u);
}

static int bytes_zero(const uint8_t *bytes, size_t count)
{
    for (size_t i = 0u; i < count; i++) if (bytes[i] != 0u) return 0;
    return 1;
}

static int video_state_valid(const lynx__video_state *video, uint64_t ticks, uint32_t policy)
{
    if (video->timing_policy != policy || video->input_mask > LYNX_INPUT_MASK ||
        video->lefthand > 1u || (video->dispctl & 0xFEu) != 0u ||
        video->frame_synced > 1u || video->line_ordinal > 104u ||
        video->row_active > 1u || video->capture_rows > LYNX_FRAME_MAX_ROWS ||
        video->capture_partial > 1u || video->frame_end_tick != 0u ||
        video->frame_start_tick > ticks ||
        video->dma_busy_until > ticks + (ticks <= UINT64_MAX - 5u ? 5u : 0u) ||
        video->refresh_busy_until > ticks + (ticks <= UINT64_MAX - 5u ? 5u : 0u)) return 0;
    if (policy == LYNX_TIMING_POLICY_LEGACY_FLAT5) {
        return video->row_active == 0u && video->capture_rows == 0u &&
               video->dma_next_tick == 0u && video->refresh_next_tick == 0u &&
               video->capture_partial == 0u &&
               bytes_zero(video->pens, sizeof video->pens) &&
               bytes_zero(video->palettes, sizeof video->palettes);
    }
    if (video->refresh_next_tick != 0u && video->refresh_next_tick < ticks) return 0;
    if (video->row_active != 0u) {
        if (video->frame_synced == 0u || video->dispctl == 0u ||
            video->capture_rows >= LYNX_FRAME_MAX_ROWS || video->dma_cursor >= LYNX_FRAME_ROW_BYTES ||
            video->dma_next_tick < ticks) return 0;
    } else if (video->dma_cursor != 0u || video->dma_next_tick != 0u) {
        return 0;
    }
    const size_t used_rows = (size_t)video->capture_rows + video->row_active;
    if (used_rows > LYNX_FRAME_MAX_ROWS) return 0;
    if (!bytes_zero(video->pens + used_rows * LYNX_FRAME_ROW_BYTES,
                    sizeof video->pens - used_rows * LYNX_FRAME_ROW_BYTES) ||
        !bytes_zero(video->palettes + used_rows * 32u,
                    sizeof video->palettes - used_rows * 32u)) return 0;
    for (size_t i = used_rows; i < LYNX__VIDEO_CAPTURE_TICKS; i++) {
        if (video->line_ticks[i] != 0u) return 0;
    }
    if (video->row_active != 0u) {
        const size_t tail = (size_t)video->capture_rows * LYNX_FRAME_ROW_BYTES + video->dma_cursor;
        if (!bytes_zero(video->pens + tail, LYNX_FRAME_ROW_BYTES - video->dma_cursor)) return 0;
    }
    return 1;
}

lynx_status lynx_snapshot_size(const lynx_machine *m, size_t *size)
{
    if (m == NULL || size == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m)) {
        return LYNX_ERR_STATE;
    }
    *size = SNAPSHOT_TOTAL;
    return LYNX_OK;
}

lynx_status lynx_snapshot_save(const lynx_machine *m, uint8_t *buf, size_t cap, size_t *written)
{
    if (m == NULL || buf == NULL || written == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m) || m->has_content == 0u) {
        return LYNX_ERR_STATE;
    }
    if (cap < SNAPSHOT_TOTAL) {
        return LYNX_ERR_CAPACITY;
    }
    if (!ranges_disjoint(buf, SNAPSHOT_TOTAL, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }

    uint8_t *payload = buf + LYNX__SNAPSHOT_HEADER_LEN;
    put_u16(payload + P_PC, m->cpu.pc);
    payload[P_A] = m->cpu.a;
    payload[P_X] = m->cpu.x;
    payload[P_Y] = m->cpu.y;
    payload[P_S] = m->cpu.s;
    payload[P_P] = (uint8_t)(m->cpu.p | 0x30u);
    payload[P_FLAGS] = m->cpu.flags;
    payload[P_OPCODE] = m->cpu.opcode;
    payload[P_STEP] = m->cpu.step;
    put_u16(payload + P_EA, m->cpu.ea);
    payload[P_TMP] = m->cpu.tmp;
    payload[P_COST_MODEL] = m->cost_model;
    payload[P_MAPCTL] = m->mapctl;
    payload[P_RESERVED] = 0u;
    put_u64(payload + P_TICKS, m->guest_ticks);
    put_u64(payload + P_READY_AT, m->cpu_ready_at);
    memcpy(payload + P_RAM, m->ram, LYNX_ADDRESS_SPACE);
    for (uint8_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        const lynx__timer *timer = &m->timer_bank.timers[i];
        uint8_t *record = payload + P_TIMERS + (size_t)i * 4u;
        record[0] = timer->backup;
        record[1] = timer->count;
        record[2] = timer->control_a;
        record[3] = timer->control_b;
    }
    payload[P_TIMER_PENDING] = m->timer_bank.pending;
    payload[P_TIMER_PHASE_POLICY] = m->timer_bank.phase_policy;
    payload[P_TIMER_EVENT_APPLIED] =
        (uint8_t)(m->timer_bank.last_event_valid == 1u &&
                  m->timer_bank.last_event_tick == m->guest_ticks &&
                  timer_edge_due_now(&m->timer_bank, m->guest_ticks));
    payload[P_BUS_HOLD] = m->bus_hold;
    video_snapshot_write(payload + P_VIDEO, &m->video);

    memcpy(buf + OFF_MAGIC, LYNX__SNAPSHOT_MAGIC, LYNX__SNAPSHOT_MAGIC_LEN);
    put_u32(buf + OFF_FORMAT, LYNX__SNAPSHOT_FORMAT);
    put_u32(buf + OFF_HEADER_LEN, LYNX__SNAPSHOT_HEADER_LEN);
    padded_build_id(buf + OFF_BUILD_ID);
    put_u32(buf + OFF_PROFILE, m->cfg.profile);
    put_u32(buf + OFF_POLICY, m->cfg.ram_policy);
    put_u64(buf + OFF_SEED, m->cfg.ram_seed);
    memcpy(buf + OFF_CONTENT_SHA, m->content_sha256, sizeof m->content_sha256);
    put_u32(buf + OFF_CONTENT_ADDR, m->content_addr);
    put_u32(buf + OFF_CONTENT_LEN, m->content_len);
    put_u32(buf + OFF_CONTENT_FORMAT, m->content_format);
    put_u32(buf + OFF_STARTUP_TIER, m->startup_tier);
    put_u32(buf + OFF_ENTRY_ADDR, m->entry_addr);
    put_u32(buf + OFF_OBJECT_LEN, m->object_len);
    put_u32(buf + OFF_PAYLOAD_LEN, LYNX__SNAPSHOT_PAYLOAD_LEN);
    put_u32(buf + OFF_TIMING_POLICY, m->cfg.timing_policy);

    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, payload, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, buf + OFF_PAYLOAD_SHA);

    *written = SNAPSHOT_TOTAL;
    return LYNX_OK;
}

lynx_status lynx_snapshot_restore(lynx_machine *m, const uint8_t *buf, size_t len)
{
    /* 1. Arguments, instance state, overlap. */
    if (m == NULL || buf == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_ok(m) || m->has_content == 0u) {
        return LYNX_ERR_STATE;
    }
    if (!ranges_disjoint(buf, len, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }
    /* 2. Exact length: a truncated blob or one with trailing bytes. */
    if (len != SNAPSHOT_TOTAL) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 3. Magic. */
    if (memcmp(buf + OFF_MAGIC, LYNX__SNAPSHOT_MAGIC, LYNX__SNAPSHOT_MAGIC_LEN) != 0) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 4. Format version. */
    if (get_u32(buf + OFF_FORMAT) != LYNX__SNAPSHOT_FORMAT) {
        return LYNX_ERR_SNAPSHOT_VERSION;
    }
    /* 5. Header length. */
    if (get_u32(buf + OFF_HEADER_LEN) != LYNX__SNAPSHOT_HEADER_LEN) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 6. The whole 128-byte build id field, padding included. */
    uint8_t build_field[LYNX__BUILD_ID_FIELD];
    padded_build_id(build_field);
    if (memcmp(buf + OFF_BUILD_ID, build_field, LYNX__BUILD_ID_FIELD) != 0) {
        return LYNX_ERR_SNAPSHOT_MISMATCH;
    }
    /* 7. Identity against this instance. */
    if (get_u32(buf + OFF_PROFILE) != m->cfg.profile ||
        get_u32(buf + OFF_POLICY) != m->cfg.ram_policy ||
        get_u64(buf + OFF_SEED) != m->cfg.ram_seed ||
        memcmp(buf + OFF_CONTENT_SHA, m->content_sha256, sizeof m->content_sha256) != 0 ||
        get_u32(buf + OFF_CONTENT_ADDR) != m->content_addr ||
        get_u32(buf + OFF_CONTENT_LEN) != m->content_len ||
        get_u32(buf + OFF_CONTENT_FORMAT) != m->content_format ||
        get_u32(buf + OFF_STARTUP_TIER) != m->startup_tier ||
        get_u32(buf + OFF_ENTRY_ADDR) != m->entry_addr ||
        get_u32(buf + OFF_OBJECT_LEN) != m->object_len ||
        get_u32(buf + OFF_TIMING_POLICY) != m->cfg.timing_policy) {
        return LYNX_ERR_SNAPSHOT_MISMATCH;
    }
    /* 8. Payload length. */
    if (get_u32(buf + OFF_PAYLOAD_LEN) != LYNX__SNAPSHOT_PAYLOAD_LEN) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 9. Payload hash. */
    const uint8_t *payload = buf + LYNX__SNAPSHOT_HEADER_LEN;
    uint8_t digest[32];
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, payload, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, digest);
    if (memcmp(digest, buf + OFF_PAYLOAD_SHA, sizeof digest) != 0) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* 10. Field values. The CPU fields are assembled into a candidate (a
       local, not the instance) and judged by the CPU's own reachability
       predicate under this instance's profile. Any PC is valid: the CPU may
       run anywhere in the address space. */
    lynx__cpu cand;
    memset(&cand, 0, sizeof cand);
    cand.pc = get_u16(payload + P_PC);
    cand.a = payload[P_A];
    cand.x = payload[P_X];
    cand.y = payload[P_Y];
    cand.s = payload[P_S];
    cand.p = payload[P_P];
    cand.flags = payload[P_FLAGS];
    cand.opcode = payload[P_OPCODE];
    cand.step = payload[P_STEP];
    cand.ea = get_u16(payload + P_EA);
    cand.tmp = payload[P_TMP];
    cand.profile = m->cpu.profile;
    const lynx__cpu_decode *table = lynx__cpu_table(cand.profile);
    if (!lynx__cpu_state_valid(&cand, table)) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    if (payload[P_RESERVED] != 0u) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* Every MAPCTL bit is stored as the deterministic provisional byte latch;
       reserved-bit electrical behavior remains unqualified. */
    /* The cost model is identity: it must be one this build defines and the
       one this instance uses. */
    if (!lynx__bus_cost_model_valid(payload[P_COST_MODEL]) ||
        payload[P_COST_MODEL] != m->cost_model) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* The largest possible positive remaining span is the maximum cycle
       price minus one tick already elapsed before a public return. */
    uint64_t ticks = get_u64(payload + P_TICKS);
    uint64_t ready = get_u64(payload + P_READY_AT);
    if (ready < ticks || ready - ticks > max_public_ready_span(payload[P_COST_MODEL])) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    /* Guest time never passes the limit lynx_advance enforces, so a state
       beyond it is one no run reaches: lynx_advance would report success with
       nothing consumed forever. ready may exceed the limit by the duration of
       the last cycle (a cycle granted just below it), which the span check
       above bounds; ticks itself may equal the limit. */
    if (ticks > UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }

    /* The candidate bank is canonical guest state. Pending request and wake
       are reconstructed from CPU state and the IRQ latch after validation. */
    lynx__timer_bank timer_cand;
    memset(&timer_cand, 0, sizeof timer_cand);
    for (uint8_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        const uint8_t *record = payload + P_TIMERS + (size_t)i * 4u;
        timer_cand.timers[i].backup = record[0];
        timer_cand.timers[i].count = record[1];
        timer_cand.timers[i].control_a = record[2];
        timer_cand.timers[i].control_b = record[3];
    }
    timer_cand.pending = payload[P_TIMER_PENDING];
    timer_cand.phase_policy = payload[P_TIMER_PHASE_POLICY];
    const uint8_t event_applied = payload[P_TIMER_EVENT_APPLIED];
    const uint8_t bus_hold = payload[P_BUS_HOLD];
    if (bus_hold > 1u) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    timer_cand.last_event_valid = event_applied;
    timer_cand.last_event_tick = event_applied != 0u ? ticks : 0u;
    if (!timer_bank_valid(&timer_cand, ticks, event_applied)) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }
    lynx__video_state video_cand;
    video_snapshot_read(&video_cand, payload + P_VIDEO);
    if (!video_state_valid(&video_cand, ticks, m->cfg.timing_policy)) {
        return LYNX_ERR_SNAPSHOT_INVALID;
    }

    /* Apply: one step, field by field, no failure path. The pending bus
       request is recomputed from the restored state. */
    m->cpu.pc = cand.pc;
    m->cpu.a = cand.a;
    m->cpu.x = cand.x;
    m->cpu.y = cand.y;
    m->cpu.s = cand.s;
    m->cpu.p = cand.p;
    m->cpu.flags = cand.flags;
    m->cpu.opcode = cand.opcode;
    m->cpu.step = cand.step;
    m->cpu.ea = cand.ea;
    m->cpu.tmp = cand.tmp;
    lynx__cpu_repost(&m->cpu, table);
    m->guest_ticks = ticks;
    m->cpu_ready_at = ready;
    m->mapctl = payload[P_MAPCTL];
    memcpy(m->ram, payload + P_RAM, LYNX_ADDRESS_SPACE);
    m->timer_bank = timer_cand;
    m->video = video_cand;
    m->bus_hold = bus_hold;
    m->wake_pending = timer_cand.pending != 0u ? 1u : 0u;
    /* Every refusal check is above. Successful restore resumes guest state
       without carrying the host's previous debugger observation forward. */
    (void)lynx_watch_clear(m);
    return LYNX_OK;
}
