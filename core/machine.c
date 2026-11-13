/* The machine: instance setup in caller memory, the bus loop that runs the
   CPU one granted, priced and tick-stamped bus cycle at a time within a
   budget, and side-effect-free inspection.

   Boundary rules for everything under core/: no allocation, no file-scope
   writable data, no host clock, file, thread or network access, and integer
   arithmetic only. The only libc calls are memcpy and memset. */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "internal.h"
#include "sha256.h"

/* Reserved for the private CPU-vector machine-loop seam. This value is not a
   public content format and can only be selected by the test adapter below. */
#define LYNX__CONTENT_TEST_FLAT 1u

/* SplitMix64, written in 2015 by S. Vigna, public domain. The state is held
   by the caller (here: a local of the power-on function) so there is no
   writable global. */
static uint64_t splitmix64_next(uint64_t *x)
{
    *x += 0x9e3779b97f4a7c15u;
    uint64_t z = *x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

void lynx_config_default(lynx_config *cfg)
{
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof *cfg);
    cfg->struct_size = (uint32_t)sizeof *cfg;
    cfg->profile = LYNX_PROFILE_NONE;
    cfg->ram_policy = LYNX_RAM_POLICY_ZERO;
    cfg->reserved = 0u;
    cfg->ram_seed = 0u;
    cfg->timing_policy = LYNX_TIMING_POLICY_LEGACY_FLAT5;
}

/* A switch over string literals: a table of pointers would land in
   relocated read-only data, which the nm gate reports as writable on Linux. */
const char *lynx_profile_name(uint32_t profile)
{
    switch (profile) {
    case LYNX_PROFILE_NONE:
        return "none";
    case LYNX_PROFILE_LYNX1:
        return "lynx1";
    case LYNX_PROFILE_LYNX2:
        return "lynx2";
    default:
        return NULL;
    }
}

static bool profile_valid(uint32_t profile)
{
    return profile == LYNX_PROFILE_NONE || profile == LYNX_PROFILE_LYNX1 ||
           profile == LYNX_PROFILE_LYNX2;
}

size_t lynx_machine_size(void)
{
    return sizeof(struct lynx_machine);
}

size_t lynx_machine_align(void)
{
    return _Alignof(struct lynx_machine);
}

static bool machine_valid(const lynx_machine *m)
{
    return m->magic == LYNX__MAGIC;
}

/* Guest time is monotonic. Once it passes the end of a priced bus cycle,
   readiness becomes the current tick; ownership hold remains a separate gate. */
static void machine_set_guest_tick(struct lynx_machine *m, uint64_t tick)
{
    if (tick > m->guest_ticks && m->timer_bank.last_event_valid != 0u &&
        tick > m->timer_bank.last_event_tick) {
        m->timer_bank.last_event_valid = 0u;
        m->timer_bank.last_event_tick = 0u;
    }
    if (tick > m->guest_ticks) {
        lynx__timer_audio_expire_status(&m->timer_bank, &m->audio);
        m->audio.same_tick_device_cursor = 0u;
        m->audio.same_tick_sample_cursor = 0u;
        if (m->audio.event_cursor_valid != 0u && tick > m->audio.event_tick) {
            m->audio.event_cursor_valid = 0u;
            m->audio.event_sequence = 0u;
            m->audio.event_tick = 0u;
        }
    }
    m->guest_ticks = tick;
    if (m->cpu_ready_at < tick) {
        m->cpu_ready_at = tick;
    }
}

/* timer_audio_apply stages and normalizes the complete device state before it
   commits. This companion advances only the clock after that transaction. */
static void machine_set_guest_tick_after_device(struct lynx_machine *m, uint64_t tick)
{
    m->guest_ticks = tick;
    if (m->cpu_ready_at < tick) m->cpu_ready_at = tick;
}

static void machine_update_wake_request(struct lynx_machine *m)
{
    m->wake_pending = m->timer_bank.pending != 0u ? 1u : 0u;
}

void lynx__power_on(lynx_machine *m)
{
    if (m->cfg.ram_policy == LYNX_RAM_POLICY_SPLITMIX64) {
        uint64_t state = m->cfg.ram_seed;
        for (uint32_t i = 0u; i < LYNX_ADDRESS_SPACE; i += 8u) {
            uint64_t v = splitmix64_next(&state);
            for (uint32_t k = 0u; k < 8u; k++) {
                m->ram[i + k] = (uint8_t)(v >> (8u * k));
            }
        }
    } else {
        memset(m->ram, 0, sizeof m->ram);
    }
    lynx__cpu_init(&m->cpu, m->cfg.profile);
    m->cost_model = (uint8_t)LYNX__COST_FLAT5;
    m->cpu_page_latch = 0u;
    m->has_content = 0u;
    m->content_addr = 0u;
    m->content_len = 0u;
    m->content_format = LYNX_CONTENT_NONE;
    m->startup_tier = LYNX_STARTUP_NONE;
    m->entry_addr = 0u;
    m->object_len = 0u;
    memset(m->content_sha256, 0, sizeof m->content_sha256);
    m->mapctl = 0u;
    m->bus_hold = 0u;
    m->wake_pending = 0u;
    m->guest_ticks = 0u;
    m->cpu_ready_at = m->guest_ticks;
    lynx__timer_init(&m->timer_bank);
    lynx__audio_init(&m->audio);
    lynx__video_init(&m->video, m->cfg.timing_policy);
    memset(&m->host_observation, 0, sizeof m->host_observation);
}

/* The one power-on path, used by init and by reset. cfg may point into m, so
   it is copied before the struct is cleared. Clearing the whole struct first
   means no padding byte or stale field survives. */
static void machine_setup(struct lynx_machine *m, const lynx_config *cfg)
{
    lynx_config keep = *cfg;
    memset(m, 0, sizeof *m);
    m->cfg = keep;
    m->magic = LYNX__MAGIC;
    lynx__power_on(m);
}

lynx_status lynx_machine_init(void *mem, size_t size, const lynx_config *cfg, lynx_machine **out)
{
    if (mem == NULL || cfg == NULL || out == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (size < sizeof(struct lynx_machine)) {
        return LYNX_ERR_SIZE;
    }
    if (((uintptr_t)mem % _Alignof(struct lynx_machine)) != 0u) {
        return LYNX_ERR_ALIGNMENT;
    }
    if (cfg->struct_size != (uint32_t)sizeof(lynx_config) || !profile_valid(cfg->profile) ||
        (cfg->ram_policy != LYNX_RAM_POLICY_ZERO && cfg->ram_policy != LYNX_RAM_POLICY_SPLITMIX64) ||
        (cfg->timing_policy != LYNX_TIMING_POLICY_LEGACY_FLAT5 &&
         cfg->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1) || cfg->reserved != 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    struct lynx_machine *m = (struct lynx_machine *)mem;
    machine_setup(m, cfg);
    *out = m;
    return LYNX_OK;
}

lynx_status lynx_reset(lynx_machine *m)
{
    if (m == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    machine_setup(m, &m->cfg);
    return LYNX_OK;
}

void lynx_machine_deinit(lynx_machine *m)
{
    if (m != NULL && machine_valid(m)) {
        m->magic = 0u;
    }
}

/* Range test by subtraction: neither pointer plus length can wrap. */
static int ranges_overlap(const void *a, size_t alen, const void *b, size_t blen)
{
    const uintptr_t ua = (uintptr_t)a;
    const uintptr_t ub = (uintptr_t)b;
    if (ua >= ub) {
        return (ua - ub) < blen;
    }
    return (ub - ua) < alen;
}

lynx_status lynx_watch_configure(lynx_machine *m, uint32_t addr, size_t len)
{
    if (m == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    if (len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (addr >= LYNX_ADDRESS_SPACE || len > (size_t)LYNX_ADDRESS_SPACE - (size_t)addr) {
        return LYNX_ERR_RANGE;
    }
    memset(&m->host_observation, 0, sizeof m->host_observation);
    m->host_observation.watch_armed = 1u;
    m->host_observation.watch_addr = addr;
    m->host_observation.watch_len = (uint32_t)len;
    return LYNX_OK;
}

lynx_status lynx_watch_clear(lynx_machine *m)
{
    if (m == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    memset(&m->host_observation, 0, sizeof m->host_observation);
    return LYNX_OK;
}

lynx_status lynx_watch_get(const lynx_machine *m, lynx_watch_event *out)
{
    if (m == NULL || out == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    if (ranges_overlap(out, sizeof *out, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }
    *out = m->host_observation.watch_event;
    return LYNX_OK;
}

lynx_status lynx_load_bll(lynx_machine *m, const uint8_t *bytes, size_t len)
{
    if (m == NULL || bytes == NULL || len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    /* Reject aliasing before inspecting the input or resetting the instance.
       Include padding and every other instance-owned field in this range. */
    if (ranges_overlap(bytes, len, m, sizeof *m)) {
        return LYNX_ERR_ARGUMENT;
    }
    if (len > UINT16_MAX) {
        return LYNX_ERR_RANGE;
    }
    if (len < 11u) {
        return LYNX_ERR_SIZE;
    }

    /* Complete container validation precedes hashing and all state changes. */
    if (bytes[0] != 0x80u || bytes[1] != 0x08u || bytes[6] != 'B' ||
        bytes[7] != 'S' || bytes[8] != '9' || bytes[9] != '3') {
        return LYNX_ERR_ARGUMENT;
    }
    const uint32_t load_addr = ((uint32_t)bytes[2] << 8) | bytes[3];
    const uint32_t object_len = ((uint32_t)bytes[4] << 8) | bytes[5];
    if ((size_t)object_len != len) {
        return LYNX_ERR_SIZE;
    }
    const size_t payload_len = len - 10u;
    if (payload_len > (size_t)LYNX_ADDRESS_SPACE - load_addr) {
        return LYNX_ERR_RANGE;
    }

    uint8_t object_sha256[32];
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, bytes, len);
    lynx__sha256_final(&hash, object_sha256);

    lynx__power_on(m);
    memcpy(&m->ram[load_addr], bytes + 10u, payload_len);
    lynx__cpu_start_at(&m->cpu, lynx__cpu_table(m->cpu.profile), (uint16_t)load_addr);
    m->has_content = 1u;
    m->content_addr = load_addr;
    m->content_len = (uint32_t)payload_len;
    m->content_format = LYNX_CONTENT_BS93;
    m->startup_tier = LYNX_STARTUP_A;
    m->entry_addr = load_addr;
    m->object_len = object_len;
    memcpy(m->content_sha256, object_sha256, sizeof m->content_sha256);
    return LYNX_OK;
}

/* Private adapter for CPU vectors. Unlike lynx_load_bll, this deliberately
   marks the instance as using an all-RAM flat bus so high-address vectors
   still exercise the actual machine loop without claiming to model MAPCTL.
   The snapshot identity keeps this lane distinct from normal BLL startup. */
lynx_status lynx__load_flat_test_image(lynx_machine *m, const uint8_t *bytes, size_t len,
                                       uint32_t load_addr)
{
    if (m == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    if (bytes == NULL || len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (len > LYNX_ADDRESS_SPACE || load_addr >= LYNX_ADDRESS_SPACE ||
        len > LYNX_ADDRESS_SPACE - load_addr) {
        return LYNX_ERR_RANGE;
    }
    lynx__power_on(m);
    memcpy(&m->ram[load_addr], bytes, len);
    lynx__cpu_start_at(&m->cpu, lynx__cpu_table(m->cpu.profile), (uint16_t)load_addr);
    m->has_content = 1u;
    m->content_addr = load_addr;
    m->content_len = (uint32_t)len;
    m->content_format = LYNX__CONTENT_TEST_FLAT;
    m->startup_tier = LYNX_STARTUP_A;
    m->entry_addr = load_addr;
    m->object_len = (uint32_t)len;
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, bytes, len);
    lynx__sha256_final(&hash, m->content_sha256);
    return LYNX_OK;
}

lynx_status lynx__machine_set_cost_model(struct lynx_machine *m, uint32_t model)
{
    if (m == NULL || !machine_valid(m) || !lynx__bus_cost_model_valid(model)) {
        return LYNX_ERR_ARGUMENT;
    }
    m->cost_model = (uint8_t)model;
    return LYNX_OK;
}

lynx_status lynx__machine_set_bus_hold(struct lynx_machine *m, uint32_t hold)
{
    if (m == NULL || !machine_valid(m) || hold > 1u) {
        return LYNX_ERR_ARGUMENT;
    }
    m->bus_hold = (uint8_t)hold;
    machine_update_wake_request(m);
    return LYNX_OK;
}

/* Side-effect-free CPU-visible byte for the opcode gate and page candidate.
   It follows the same mapping as bus_access but does not complete a request.
   Private vector tests intentionally keep their separate flat RAM bus. */
static bool bus_opcode_peek(const struct lynx_machine *m, uint16_t addr,
                            uint8_t *result)
{
    if (result == NULL) return false;
    if (m->content_format == LYNX__CONTENT_TEST_FLAT) {
        *result = m->ram[addr];
        return true;
    }

    switch (lynx__bus_decode(m->mapctl, addr)) {
    case LYNX__BUS_ROUTE_RAM:
        *result = m->ram[addr];
        return true;
    case LYNX__BUS_ROUTE_MIKEY:
        if (lynx__video_available(addr)) {
            *result = lynx__video_peek(&m->video, addr);
            return true;
        }
        if (lynx__timer_available(addr)) {
            *result = lynx__timer_peek(&m->timer_bank, addr);
            return true;
        }
        if (lynx__audio_available(addr)) {
            *result = lynx__audio_peek(&m->audio, addr);
            return true;
        }
        return false;
    case LYNX__BUS_ROUTE_SUZY:
        if (!lynx__input_available(addr)) return false;
        *result = lynx__input_peek(&m->video, addr);
        return true;
    case LYNX__BUS_ROUTE_MAPCTL:
    case LYNX__BUS_ROUTE_ROM:
    case LYNX__BUS_ROUTE_VECTOR:
        return false;
    }
    return false;
}

/* Whether the pending CPU request has an implemented target. The private
   vector identity intentionally retains its separate flat RAM bus. */
static bool bus_request_available(const struct lynx_machine *m, uint16_t addr, uint8_t flags)
{
    if (m->content_format == LYNX__CONTENT_TEST_FLAT) {
        return true;
    }
    const lynx__bus_route route = lynx__bus_decode(m->mapctl, addr);
    if (route == LYNX__BUS_ROUTE_RAM) {
        return true;
    }
    if (route == LYNX__BUS_ROUTE_MIKEY) {
        return lynx__timer_available(addr) || lynx__video_available(addr) ||
               lynx__audio_available(addr);
    }
    if (route == LYNX__BUS_ROUTE_SUZY) return lynx__input_available(addr);
    return route == LYNX__BUS_ROUTE_MAPCTL && (flags & LYNX__CF_WRITE) != 0u;
}

/* Performs one granted CPU bus cycle through the same MAPCTL decoder used by
   the opcode precheck. Unsupported targets never read or write hidden RAM. */
static bool bus_access(struct lynx_machine *m, uint16_t addr, uint8_t data, uint8_t flags,
                       uint64_t stamp, uint8_t *result,
                       lynx__audio_event_sink *audio_events,
                       bool *output_full)
{
    if (output_full != NULL) *output_full = false;
    const bool write = (flags & LYNX__CF_WRITE) != 0u;
    if (m->content_format == LYNX__CONTENT_TEST_FLAT) {
        if (write) {
            m->ram[addr] = data;
            *result = data;
        } else {
            *result = m->ram[addr];
        }
        return true;
    }

    switch (lynx__bus_decode(m->mapctl, addr)) {
    case LYNX__BUS_ROUTE_RAM:
        if (write) {
            m->ram[addr] = data;
            *result = data;
        } else {
            *result = m->ram[addr];
        }
        return true;
    case LYNX__BUS_ROUTE_MAPCTL:
        if (!write) {
            return false;
        }
        /* Provisional policy: retain all eight written bits. */
        m->mapctl = data;
        *result = data;
        return true;
    case LYNX__BUS_ROUTE_MIKEY:
        if (!lynx__timer_available(addr) && !lynx__video_available(addr) &&
            !lynx__audio_available(addr)) {
            return false;
        }
        if (lynx__video_available(addr)) {
            if (write) {
                if (!lynx__video_write(&m->video, addr, data)) return false;
                *result = data;
            } else {
                *result = lynx__video_peek(&m->video, addr);
            }
            return true;
        }
        if (lynx__audio_available(addr)) {
            if (write) {
                const lynx__work_status work = lynx__timer_audio_write(
                    &m->timer_bank, &m->audio, addr, data, stamp, audio_events);
                if (work == LYNX__WORK_OUTPUT) {
                    if (output_full != NULL) *output_full = true;
                    return false;
                }
                if (work != LYNX__WORK_OK) return false;
                *result = data;
            } else {
                *result = lynx__audio_peek(&m->audio, addr);
            }
            return true;
        }
        if (write) {
            const lynx__work_status work = lynx__timer_audio_write(
                &m->timer_bank, &m->audio, addr, data, stamp, audio_events);
            if (work == LYNX__WORK_OUTPUT) {
                if (output_full != NULL) *output_full = true;
                return false;
            }
            if (work != LYNX__WORK_OK) {
                return false;
            }
            *result = data;
        } else {
            *result = lynx__timer_peek(&m->timer_bank, addr);
        }
        return true;
    case LYNX__BUS_ROUTE_SUZY:
        if (!lynx__input_available(addr)) return false;
        if (write) {
            if (!lynx__input_write(&m->video, addr, data)) return false;
            *result = data;
        } else {
            /* MUTATION_ANCHOR_INPUT_SAMPLE_EDGE: input is sampled only when
               this CPU read receives its actual bus grant (price.sample is 0). */
            *result = lynx__input_peek(&m->video, addr);
        }
        return true;
    case LYNX__BUS_ROUTE_ROM:
    case LYNX__BUS_ROUTE_VECTOR:
        return false;
    }
    return false;
}

bool lynx__machine_event_before(lynx__machine_event a, lynx__machine_event b)
{
    if (a.tick != b.tick) {
        return a.tick < b.tick;
    }
    if (a.priority != b.priority) {
        return a.priority < b.priority;
    }
    return a.sequence < b.sequence;
}

/* Half-open budget: every bus cycle that can be granted before target is
   granted, priced, performed and stamped; one that would be granted at or
   after target is not. A cycle granted before target is performed even when
   its duration ends past target; cpu_ready_at records when the next one may
   start, and the clock stops at target. */
typedef struct machine_av_capture {
    lynx_pcm_frame *pcm;
    size_t pcm_capacity;
    size_t pcm_count;
    lynx__audio_event_sink events;
} machine_av_capture;

static lynx_status machine_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                                   lynx_stop_reason *stop, bool stop_on_frame,
                                   machine_av_capture *capture)
{
    if (m == NULL || consumed == NULL || stop == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    *consumed = 0u;
    *stop = LYNX_STOP_BUDGET;
    if (!machine_valid(m) || m->has_content == 0u) {
        return LYNX_ERR_STATE;
    }
    const lynx__cpu_decode *table = lynx__cpu_table(m->cpu.profile);
    const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    const uint64_t start = m->guest_ticks;
    const uint64_t room = start < limit ? limit - start : 0u;
    const uint64_t target = start + (budget < room ? budget : room);

    while (m->guest_ticks < target) {
        lynx__machine_event next = {target, UINT8_MAX, UINT8_MAX};
        bool has_event = false;
        uint64_t timer_tick = 0u;
        if (lynx__timer_next(&m->timer_bank, m->guest_ticks, &timer_tick) &&
            timer_tick < target) {
            next.tick = timer_tick;
            next.priority = (uint8_t)LYNX__MACHINE_EVENT_TIMER_PRIORITY;
            next.sequence = (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE;
            has_event = true;
        }
        uint64_t audio_tick = 0u;
        if (lynx__audio_next(&m->audio, m->guest_ticks, &audio_tick) &&
            audio_tick < target) {
            const lynx__machine_event audio_event = {
                audio_tick, (uint8_t)LYNX__MACHINE_EVENT_AUDIO_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(audio_event, next)) {
                next = audio_event;
                has_event = true;
            }
        }
        uint64_t ring_tick = 0u;
        if (lynx__timer_audio_next(&m->audio, m->guest_ticks, &ring_tick) &&
            ring_tick < target) {
            const lynx__machine_event ring_event = {
                ring_tick, (uint8_t)LYNX__MACHINE_EVENT_AUDIO_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(ring_event, next)) {
                next = ring_event;
                has_event = true;
            }
        }
        uint64_t video_tick = 0u;
        if (lynx__video_next_dma(&m->video, m->guest_ticks, &video_tick) &&
            video_tick < target) {
            const lynx__machine_event video_event = {
                video_tick, (uint8_t)LYNX__MACHINE_EVENT_VIDEO_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(video_event, next)) {
                next = video_event;
                has_event = true;
            }
        }
        uint64_t refresh_tick = 0u;
        if (lynx__video_next_refresh(&m->video, m->guest_ticks, &refresh_tick) &&
            refresh_tick < target) {
            const lynx__machine_event refresh_event = {
                refresh_tick, (uint8_t)LYNX__MACHINE_EVENT_REFRESH_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(refresh_event, next)) {
                next = refresh_event;
                has_event = true;
            }
        }
        const bool cpu_available = m->bus_hold == 0u;
        uint64_t cpu_tick = m->cpu_ready_at > m->guest_ticks ?
                                m->cpu_ready_at : m->guest_ticks;
        if (m->video.dma_busy_until > cpu_tick) cpu_tick = m->video.dma_busy_until;
        if (m->video.refresh_busy_until > cpu_tick) cpu_tick = m->video.refresh_busy_until;
        if (cpu_available && cpu_tick < target) {
            const lynx__machine_event cpu_event = {
                cpu_tick, (uint8_t)LYNX__MACHINE_EVENT_CPU_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(cpu_event, next)) {
                next = cpu_event;
                has_event = true;
            }
        }
        if (capture == NULL) {
            /* No PCM span can observe sample values in this legacy call. Keep
               samples as ordered scheduler events when they coincide with
               real work, but advance a sample-only prefix arithmetically. */
            const uint64_t before_tick = has_event ? next.tick : target;
            const uint64_t sampled_through =
                lynx__audio_advance_sample_phase_before(&m->audio, before_tick);
            if (sampled_through != 0u) {
                if (sampled_through > m->guest_ticks) {
                    machine_set_guest_tick(m, sampled_through);
                }
                m->audio.same_tick_sample_cursor = 1u;
            }
        }
        if (m->audio.next_sample_tick < target) {
            const lynx__machine_event sample_event = {
                m->audio.next_sample_tick,
                (uint8_t)LYNX__MACHINE_EVENT_SAMPLE_PRIORITY,
                (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE};
            if (!has_event || lynx__machine_event_before(sample_event, next)) {
                next = sample_event;
                has_event = true;
            }
        }
        if (!has_event) {
            /* No eligible work before the exclusive target. This is also the
               bounded idle path while the private CPU grant gate is held. */
            machine_set_guest_tick(m, target);
            break;
        }
        if (next.priority <= (uint8_t)LYNX__MACHINE_EVENT_AUDIO_PRIORITY) {
            /* Device/link work can publish several ordered audio events and
               a same-tick sample follows it. Reserve the whole bounded
               device/link/sample group before mutating any of that state, so
               an undersized span cannot leave half of the tick visible. A
               later CPU-triggered audio write is a separate ordered group;
               its capacity check remains immediately before the bus write. */
            if (capture != NULL) {
                const bool sample_due_here =
                    m->audio.next_sample_tick == next.tick && next.tick < target;
                const bool event_span_short =
                    capture->events.count > capture->events.capacity ||
                    capture->events.capacity - capture->events.count <
                        LYNX_AV_MIN_EVENT_CAPACITY;
                const bool pcm_span_short = sample_due_here &&
                    capture->pcm_count >= capture->pcm_capacity;
                if (event_span_short || pcm_span_short) {
                    *stop = LYNX_STOP_OUTPUT;
                    break;
                }
            }
            const bool timer_due = timer_tick == next.tick && timer_tick < target;
            const bool audio_due = audio_tick == next.tick && audio_tick < target;
            lynx__audio_event_sink *sink = capture != NULL ? &capture->events : NULL;
            uint8_t borrow_mask = 0u;
            const lynx__work_status work = lynx__timer_audio_apply(
                &m->timer_bank, &m->audio, next.tick, m->guest_ticks,
                timer_due, audio_due, sink, &borrow_mask);
            if (work != LYNX__WORK_OK) {
                *stop = work == LYNX__WORK_OUTPUT ? LYNX_STOP_OUTPUT : LYNX_STOP_UNAVAILABLE;
                break;
            }
            machine_set_guest_tick_after_device(m, next.tick);
            const bool frame_complete = timer_due ?
                lynx__video_timer_apply(&m->video, borrow_mask, next.tick) : false;
            machine_update_wake_request(m);
            if (frame_complete) {
                if (stop_on_frame) {
                    *stop = LYNX_STOP_FRAME;
                    break;
                }
                lynx__video_discard_frame(&m->video, next.tick);
            }
            continue;
        }
        if (next.priority == (uint8_t)LYNX__MACHINE_EVENT_VIDEO_PRIORITY) {
            machine_set_guest_tick(m, next.tick);
            m->cpu_page_latch = 0u;
            lynx__video_dma_apply(&m->video, m->ram, next.tick);
            continue;
        }
        if (next.priority == (uint8_t)LYNX__MACHINE_EVENT_REFRESH_PRIORITY) {
            machine_set_guest_tick(m, next.tick);
            m->cpu_page_latch = 0u;
            lynx__video_refresh_apply(&m->video, next.tick);
            continue;
        }
        if (next.priority == (uint8_t)LYNX__MACHINE_EVENT_SAMPLE_PRIORITY) {
            if (capture != NULL && capture->pcm_count >= capture->pcm_capacity) {
                *stop = LYNX_STOP_OUTPUT;
                break;
            }
            machine_set_guest_tick(m, next.tick);
            lynx_pcm_frame sample;
            if (!lynx__audio_sample(&m->audio, next.tick, &sample)) {
                *stop = LYNX_STOP_UNAVAILABLE;
                break;
            }
            if (capture != NULL) capture->pcm[capture->pcm_count++] = sample;
            lynx__audio_mark_sample(&m->audio);
            continue;
        }
        if (next.tick > m->guest_ticks) {
            machine_set_guest_tick(m, next.tick);
        }
        const uint64_t grant = m->guest_ticks;
        const uint16_t addr = m->cpu.req_addr;
        const uint8_t flags = m->cpu.req_flags;
        if (!bus_request_available(m, addr, flags)) {
            *stop = LYNX_STOP_UNAVAILABLE;
            break;
        }
        if (lynx__cpu_at_boundary(&m->cpu) && m->cpu.req_kind == LYNX__CK_OPCODE) {
            uint8_t opcode = 0u;
            if (!bus_opcode_peek(m, addr, &opcode)) {
                *stop = LYNX_STOP_UNAVAILABLE;
                break;
            }
            if (!lynx__cpu_opcode_defined(m->cpu.profile, opcode)) {
                *stop = LYNX_STOP_UNDEFINED;
                break;
            }
        }
        lynx__bus_timing price;
        if (m->cfg.timing_policy == LYNX_TIMING_POLICY_VIDEO_POLICY1) {
            price.ticks = 5u;
            price.sample = 0u;
            uint8_t opcode = 0u;
            if (lynx__bus_decode(m->mapctl, addr) == LYNX__BUS_ROUTE_RAM &&
                bus_opcode_peek(m, addr, &opcode) &&
                lynx__bus_page_opcode_eligible(m->mapctl, m->cpu_page_latch,
                                               addr, m->cpu.req_kind, opcode,
                                               m->cpu.profile)) {
                price.ticks = 4u;
            }
        } else {
            price = lynx__bus_timing_of(m->cost_model, lynx__bus_region_of(addr),
                                        m->cpu.req_kind);
        }
        const uint8_t write_value = m->cpu.req_data;
        uint8_t data = 0u;
        if (capture != NULL && (flags & LYNX__CF_WRITE) != 0u &&
            lynx__bus_decode(m->mapctl, addr) == LYNX__BUS_ROUTE_MIKEY &&
            lynx__audio_available(addr) &&
            capture->events.capacity - capture->events.count < LYNX_AV_MIN_EVENT_CAPACITY) {
            *stop = LYNX_STOP_OUTPUT;
            break;
        }
        lynx__audio_event_sink *sink = capture != NULL ? &capture->events : NULL;
        bool output_full = false;
        if (!bus_access(m, addr, m->cpu.req_data, flags, grant + price.sample,
                        &data, sink, &output_full)) {
            *stop = output_full ? LYNX_STOP_OUTPUT : LYNX_STOP_UNAVAILABLE;
            break;
        }
        if (m->cfg.timing_policy == LYNX_TIMING_POLICY_VIDEO_POLICY1 &&
            (m->mapctl & 0x80u) == 0u &&
            lynx__bus_decode(m->mapctl, addr) == LYNX__BUS_ROUTE_RAM &&
            (addr >> 8) < UINT8_MAX) {
            m->cpu_page_latch = (uint8_t)((addr >> 8) + 1u);
        } else {
            m->cpu_page_latch = 0u;
        }
        machine_update_wake_request(m);
        /* The scan has already established that timer work at this exact tick
           is complete. Mark only after a successful bus action so refused
           requests preserve the whole instance byte-for-byte. */
        lynx__timer_mark_tick(&m->timer_bank, grant);
        /* CPU-side device writes (including explicit CTLB strobes handled by
           bus_access) precede any natural root newly enabled at this tick. */
        m->audio.same_tick_device_cursor = (uint8_t)(
            m->audio.same_tick_device_cursor | LYNX__AUDIO_DEVICE_APPLIED);
        const uint8_t lines = m->timer_bank.pending != 0u ? (uint8_t)LYNX__LINE_IRQ : 0u;
        lynx__cpu_complete(&m->cpu, table, data, lines);
        m->cpu_ready_at = grant + price.ticks;
        const uint32_t watched_addr = (uint32_t)addr;
        if ((flags & LYNX__CF_WRITE) != 0u && m->host_observation.watch_armed != 0u &&
            watched_addr >= m->host_observation.watch_addr &&
            watched_addr - m->host_observation.watch_addr < m->host_observation.watch_len) {
            /* The bus mutation, CPU continuation and next-ready tick are all
               committed before this host-only one-shot observation returns. */
            m->host_observation.watch_event.hit = 1u;
            m->host_observation.watch_event.address = watched_addr;
            m->host_observation.watch_event.value = write_value;
            m->host_observation.watch_event.hit_tick = grant;
            m->host_observation.watch_event.return_tick = grant + 1u;
            m->host_observation.watch_armed = 0u;
            machine_set_guest_tick(m, grant + 1u);
            *stop = LYNX_STOP_WATCH;
            break;
        }
    }
    *consumed = m->guest_ticks - start;
    return LYNX_OK;
}

lynx_status lynx_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                         lynx_stop_reason *stop)
{
    return machine_advance(m, budget, consumed, stop, false, NULL);
}

static bool span_valid(const void *ptr, size_t size)
{
    const uintptr_t begin = (uintptr_t)ptr;
    return size <= UINTPTR_MAX - begin;
}

static bool span_overlap(const void *a, size_t a_size, const void *b, size_t b_size)
{
    const uintptr_t a_begin = (uintptr_t)a;
    const uintptr_t b_begin = (uintptr_t)b;
    const uintptr_t a_end = a_begin + (uintptr_t)a_size;
    const uintptr_t b_end = b_begin + (uintptr_t)b_size;
    return a_begin < b_end && b_begin < a_end;
}

static bool frame_spans_disjoint(const lynx_machine *m, const lynx_frame_buffers *buffers,
                                 lynx_frame_result *result)
{
    const size_t descriptor_size = sizeof *buffers;
    const size_t result_size = sizeof *result;
    const size_t pens_size = buffers->pens_capacity;
    const size_t palettes_size = buffers->palettes_capacity;
    const size_t ticks_size = buffers->line_ticks_capacity * sizeof(uint64_t);
    if (buffers->line_ticks_capacity > SIZE_MAX / sizeof(uint64_t) ||
        !span_valid(buffers, descriptor_size) || !span_valid(result, result_size) ||
        !span_valid(buffers->pens, pens_size) || !span_valid(buffers->palettes, palettes_size) ||
        !span_valid(buffers->line_ticks, ticks_size)) return false;
    const struct { const void *ptr; size_t size; } spans[] = {
        {m, sizeof *m}, {buffers, descriptor_size}, {result, result_size},
        {buffers->pens, pens_size}, {buffers->palettes, palettes_size},
        {buffers->line_ticks, ticks_size}
    };
    for (size_t i = 0u; i < sizeof spans / sizeof spans[0]; i++) {
        for (size_t j = i + 1u; j < sizeof spans / sizeof spans[0]; j++) {
            if (span_overlap(spans[i].ptr, spans[i].size, spans[j].ptr, spans[j].size)) return false;
        }
    }
    return true;
}

lynx_status lynx_set_input(lynx_machine *m, uint32_t pressed_mask)
{
    if (m == NULL) return LYNX_ERR_ARGUMENT;
    if (!machine_valid(m)) return LYNX_ERR_STATE;
    return lynx__input_set(&m->video, pressed_mask);
}

lynx_status lynx_advance_frame(lynx_machine *m, uint64_t budget,
                               const lynx_frame_buffers *buffers,
                               lynx_frame_result *result)
{
    if (m == NULL || buffers == NULL || result == NULL) return LYNX_ERR_ARGUMENT;
    if (!machine_valid(m) || m->has_content == 0u) return LYNX_ERR_STATE;
    if (m->cfg.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1) return LYNX_ERR_UNAVAILABLE;
    if (!span_valid(buffers, sizeof *buffers) || !span_valid(result, sizeof *result) ||
        span_overlap(m, sizeof *m, buffers, sizeof *buffers) ||
        span_overlap(m, sizeof *m, result, sizeof *result) ||
        span_overlap(buffers, sizeof *buffers, result, sizeof *result)) return LYNX_ERR_ARGUMENT;
    if (buffers->struct_size != (uint32_t)sizeof *buffers || buffers->reserved != 0u ||
        result->struct_size != (uint32_t)sizeof *result || buffers->pens == NULL ||
        buffers->palettes == NULL || buffers->line_ticks == NULL) return LYNX_ERR_ARGUMENT;
    if (buffers->pens_capacity < LYNX_FRAME_PENS_CAPACITY ||
        buffers->palettes_capacity < LYNX_FRAME_PALETTE_CAPACITY ||
        buffers->line_ticks_capacity < LYNX_FRAME_LINE_TICKS_CAPACITY) return LYNX_ERR_CAPACITY;
    if (((uintptr_t)buffers->line_ticks % _Alignof(uint64_t)) != 0u) return LYNX_ERR_ALIGNMENT;
    if (!frame_spans_disjoint(m, buffers, result)) return LYNX_ERR_ARGUMENT;

    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    const lynx_status status = machine_advance(m, budget, &consumed, &stop, true, NULL);
    if (status != LYNX_OK) return status;
    const uint64_t frame_start = stop == LYNX_STOP_FRAME ? m->video.frame_start_tick : 0u;
    const uint64_t frame_end = stop == LYNX_STOP_FRAME ? m->video.frame_end_tick : 0u;
    const uint64_t sequence = stop == LYNX_STOP_FRAME ? m->video.frame_sequence : 0u;
    const uint32_t rows = stop == LYNX_STOP_FRAME ? m->video.capture_rows : 0u;
    const uint32_t flags = stop == LYNX_STOP_FRAME && m->video.capture_partial != 0u ?
                               LYNX_FRAME_PARTIAL : 0u;
    if (stop == LYNX_STOP_FRAME) {
        lynx__video_copy_frame(&m->video, buffers);
        lynx__video_discard_frame(&m->video, frame_end);
    }
    result->stop = stop;
    result->ticks_consumed = consumed;
    result->frame_start_tick = frame_start;
    result->frame_end_tick = frame_end;
    result->sequence = sequence;
    result->row_count = rows;
    result->flags = flags;
    return LYNX_OK;
}

static bool av_spans_disjoint(const lynx_machine *m, const lynx_av_buffers *buffers,
                              const lynx_av_result *result)
{
    if (buffers->frame.line_ticks_capacity > SIZE_MAX / sizeof(uint64_t) ||
        buffers->pcm_capacity > SIZE_MAX / sizeof(lynx_pcm_frame) ||
        buffers->event_capacity > SIZE_MAX / sizeof(lynx_audio_event)) return false;
    const struct { const void *ptr; size_t size; } spans[] = {
        {m, sizeof *m},
        {buffers, sizeof *buffers},
        {result, sizeof *result},
        {buffers->frame.pens, buffers->frame.pens_capacity},
        {buffers->frame.palettes, buffers->frame.palettes_capacity},
        {buffers->frame.line_ticks, buffers->frame.line_ticks_capacity * sizeof(uint64_t)},
        {buffers->pcm, buffers->pcm_capacity * sizeof(lynx_pcm_frame)},
        {buffers->events, buffers->event_capacity * sizeof(lynx_audio_event)}
    };
    for (size_t i = 0u; i < sizeof spans / sizeof spans[0]; i++) {
        if (!span_valid(spans[i].ptr, spans[i].size)) return false;
    }
    for (size_t i = 0u; i < sizeof spans / sizeof spans[0]; i++) {
        for (size_t j = i + 1u; j < sizeof spans / sizeof spans[0]; j++) {
            if (span_overlap(spans[i].ptr, spans[i].size, spans[j].ptr, spans[j].size)) return false;
        }
    }
    return true;
}

lynx_status lynx_advance_av(lynx_machine *m, uint64_t budget,
                            const lynx_av_buffers *buffers,
                            lynx_av_result *result)
{
    if (m == NULL || buffers == NULL || result == NULL) return LYNX_ERR_ARGUMENT;
    if (!machine_valid(m) || m->has_content == 0u) return LYNX_ERR_STATE;
    if (m->cfg.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1) return LYNX_ERR_UNAVAILABLE;
    if (!span_valid(buffers, sizeof *buffers) || !span_valid(result, sizeof *result) ||
        span_overlap(m, sizeof *m, buffers, sizeof *buffers) ||
        span_overlap(m, sizeof *m, result, sizeof *result) ||
        span_overlap(buffers, sizeof *buffers, result, sizeof *result)) return LYNX_ERR_ARGUMENT;
    if (buffers->struct_size != (uint32_t)sizeof *buffers || buffers->reserved != 0u ||
        buffers->frame.struct_size != (uint32_t)sizeof buffers->frame ||
        buffers->frame.reserved != 0u || result->struct_size != (uint32_t)sizeof *result ||
        buffers->frame.pens == NULL || buffers->frame.palettes == NULL ||
        buffers->frame.line_ticks == NULL || buffers->pcm == NULL || buffers->events == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (buffers->frame.pens_capacity < LYNX_FRAME_PENS_CAPACITY ||
        buffers->frame.palettes_capacity < LYNX_FRAME_PALETTE_CAPACITY ||
        buffers->frame.line_ticks_capacity < LYNX_FRAME_LINE_TICKS_CAPACITY ||
        buffers->pcm_capacity == 0u ||
        buffers->event_capacity < LYNX_AV_MIN_EVENT_CAPACITY) return LYNX_ERR_CAPACITY;
    if (((uintptr_t)buffers->frame.line_ticks % _Alignof(uint64_t)) != 0u ||
        ((uintptr_t)buffers->pcm % _Alignof(lynx_pcm_frame)) != 0u ||
        ((uintptr_t)buffers->events % _Alignof(lynx_audio_event)) != 0u) return LYNX_ERR_ALIGNMENT;
    if (!av_spans_disjoint(m, buffers, result)) return LYNX_ERR_ARGUMENT;

    const uint64_t first_sample = m->audio.next_sample_ordinal;
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    machine_av_capture capture = {
        buffers->pcm, buffers->pcm_capacity, 0u,
        {buffers->events, buffers->event_capacity, 0u}};
    const lynx_status status = machine_advance(m, budget, &consumed, &stop, true, &capture);
    if (status != LYNX_OK) return status;
    const uint64_t frame_start = stop == LYNX_STOP_FRAME ? m->video.frame_start_tick : 0u;
    const uint64_t frame_end = stop == LYNX_STOP_FRAME ? m->video.frame_end_tick : 0u;
    const uint64_t frame_sequence = stop == LYNX_STOP_FRAME ? m->video.frame_sequence : 0u;
    const uint32_t rows = stop == LYNX_STOP_FRAME ? m->video.capture_rows : 0u;
    const uint32_t flags = stop == LYNX_STOP_FRAME && m->video.capture_partial != 0u ?
                               LYNX_FRAME_PARTIAL : 0u;
    if (stop == LYNX_STOP_FRAME) {
        lynx__video_copy_frame(&m->video, &buffers->frame);
        lynx__video_discard_frame(&m->video, frame_end);
    }
    result->stop = stop;
    result->ticks_consumed = consumed;
    result->frame_start_tick = frame_start;
    result->frame_end_tick = frame_end;
    result->frame_sequence = frame_sequence;
    result->row_count = rows;
    result->frame_flags = flags;
    result->first_sample_ordinal = first_sample;
    result->next_sample_ordinal = m->audio.next_sample_ordinal;
    result->pcm_count = (uint32_t)capture.pcm_count;
    result->event_count = (uint32_t)capture.events.count;
    return LYNX_OK;
}

lynx_status lynx_read_register(const lynx_machine *m, uint32_t reg, uint32_t *value)
{
    if (m == NULL || value == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    switch (reg) {
    case LYNX_REG_PC:
        *value = m->cpu.pc;
        break;
    case LYNX_REG_A:
        *value = m->cpu.a;
        break;
    case LYNX_REG_X:
        *value = m->cpu.x;
        break;
    case LYNX_REG_Y:
        *value = m->cpu.y;
        break;
    case LYNX_REG_S:
        *value = m->cpu.s;
        break;
    case LYNX_REG_P:
        *value = (uint32_t)m->cpu.p | 0x30u;
        break;
    case LYNX_REG_CPU_STATE: {
        /* Readiness is normalized when guest time passes the priced-cycle end;
           keep observation defensive against a stale or forged earlier value. */
        const uint64_t remaining = m->cpu_ready_at > m->guest_ticks ?
                                       m->cpu_ready_at - m->guest_ticks : 0u;
        *value = (uint32_t)m->cpu.flags | ((uint32_t)m->cpu.step << 8) |
                 ((uint32_t)m->cpu.opcode << 16) |
                 ((uint32_t)(remaining > UINT8_MAX ? UINT8_MAX : remaining) << 24);
        break;
    }
    case LYNX_REG_CPU_LATCH:
        *value = (uint32_t)m->cpu.ea | ((uint32_t)m->cpu.tmp << 16) |
                 ((uint32_t)m->cost_model << 24);
        break;
    case LYNX_REG_MAPCTL:
        *value = m->mapctl;
        break;
    default:
        return LYNX_ERR_RANGE;
    }
    return LYNX_OK;
}

/* Inspection destinations must be wholly outside the caller-owned instance.
   Treat an integer-address range that wraps as an invalid output range too. */
static bool output_overlaps_machine(const struct lynx_machine *m, const void *out, size_t len)
{
    const uintptr_t machine_begin = (uintptr_t)(const void *)m;
    const uintptr_t output_begin = (uintptr_t)out;
    if (sizeof *m > UINTPTR_MAX - machine_begin || len > UINTPTR_MAX - output_begin) {
        return true;
    }
    const uintptr_t machine_end = machine_begin + sizeof *m;
    const uintptr_t output_end = output_begin + (uintptr_t)len;
    return output_begin < machine_end && machine_begin < output_end;
}

lynx_status lynx_read_memory(const lynx_machine *m, uint32_t addr, uint8_t *out, size_t len)
{
    if (m == NULL || out == NULL || len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    if (addr >= LYNX_ADDRESS_SPACE || len > LYNX_ADDRESS_SPACE - addr) {
        return LYNX_ERR_RANGE;
    }
    if (output_overlaps_machine(m, out, len)) {
        return LYNX_ERR_ARGUMENT;
    }
    memcpy(out, &m->ram[addr], len);
    return LYNX_OK;
}

lynx_status lynx_read_cpu_memory(const lynx_machine *m, uint32_t addr, uint8_t *out, size_t len)
{
    if (m == NULL || out == NULL || len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    if (addr >= LYNX_ADDRESS_SPACE || len > LYNX_ADDRESS_SPACE - addr) {
        return LYNX_ERR_RANGE;
    }
    if (output_overlaps_machine(m, out, len)) {
        return LYNX_ERR_ARGUMENT;
    }
    /* First pass: every byte must have an implemented, side-effect-free mapped
       view. Unsupported Mikey, Suzy, ROM, vector and MAPCTL routes refuse
       atomically before any output byte is written. */
    for (size_t i = 0u; i < len; i++) {
        const uint16_t current = (uint16_t)(addr + (uint32_t)i);
        const lynx__bus_route route = lynx__bus_decode(m->mapctl, current);
        if (route != LYNX__BUS_ROUTE_RAM &&
            !((route == LYNX__BUS_ROUTE_MIKEY &&
               (lynx__timer_available(current) || lynx__video_available(current) ||
                lynx__audio_available(current))) ||
              (route == LYNX__BUS_ROUTE_SUZY && lynx__input_available(current)))) {
            return LYNX_ERR_UNAVAILABLE;
        }
    }
    /* Second pass: peek device state without catching it up or doing bus I/O. */
    for (size_t i = 0u; i < len; i++) {
        const uint16_t current = (uint16_t)(addr + (uint32_t)i);
        if (lynx__bus_decode(m->mapctl, current) == LYNX__BUS_ROUTE_RAM) {
            out[i] = m->ram[current];
        } else if (lynx__bus_decode(m->mapctl, current) == LYNX__BUS_ROUTE_SUZY) {
            out[i] = lynx__input_peek(&m->video, current);
        } else if (lynx__video_available(current)) {
            out[i] = lynx__video_peek(&m->video, current);
        } else if (lynx__audio_available(current)) {
            out[i] = lynx__audio_peek(&m->audio, current);
        } else {
            out[i] = lynx__timer_peek(&m->timer_bank, current);
        }
    }
    return LYNX_OK;
}

lynx_status lynx_guest_ticks(const lynx_machine *m, uint64_t *ticks)
{
    if (m == NULL || ticks == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    *ticks = m->guest_ticks;
    return LYNX_OK;
}

lynx_status lynx_get_identity(const lynx_machine *m, lynx_identity *out)
{
    if (m == NULL || out == NULL) {
        return LYNX_ERR_ARGUMENT;
    }
    if (!machine_valid(m)) {
        return LYNX_ERR_STATE;
    }
    memset(out, 0, sizeof *out);
    out->profile = m->cfg.profile;
    out->ram_policy = m->cfg.ram_policy;
    out->ram_seed = m->cfg.ram_seed;
    out->timing_policy = m->cfg.timing_policy;
    out->has_content = m->has_content;
    out->content_addr = m->content_addr;
    out->content_len = m->content_len;
    out->reserved = 0u;
    memcpy(out->content_sha256, m->content_sha256, sizeof out->content_sha256);
    out->content_format = m->content_format;
    out->startup_tier = m->startup_tier;
    out->entry_addr = m->entry_addr;
    out->object_len = m->object_len;
    out->audio_policy = LYNX_AUDIO_POLICY_DIGITAL_1;
    out->audio_rate_num = LYNX_AUDIO_RATE_NUM;
    out->audio_rate_den = LYNX_AUDIO_RATE_DEN;
    out->audio_channels = LYNX_AUDIO_PCM_CHANNELS;
    out->audio_bits = LYNX_AUDIO_PCM_BITS;
    return LYNX_OK;
}

const char *lynx_build_id(void)
{
    return lynx__build_id;
}
