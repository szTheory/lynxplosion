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
    m->has_content = 0u;
    m->content_addr = 0u;
    m->content_len = 0u;
    m->content_format = LYNX_CONTENT_NONE;
    m->startup_tier = LYNX_STARTUP_NONE;
    m->entry_addr = 0u;
    m->object_len = 0u;
    memset(m->content_sha256, 0, sizeof m->content_sha256);
    m->mapctl = 0u;
    m->guest_ticks = 0u;
    m->cpu_ready_at = m->guest_ticks;
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
        cfg->reserved != 0u) {
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

/* Read the instruction byte after the route precheck has established that the
   CPU sees RAM. This is not a public mapped peek and does not complete a bus
   request. */
static uint8_t bus_peek(const struct lynx_machine *m, uint16_t addr)
{
    return m->ram[addr];
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
    return route == LYNX__BUS_ROUTE_MAPCTL && (flags & LYNX__CF_WRITE) != 0u;
}

/* Performs one granted CPU bus cycle through the same MAPCTL decoder used by
   the opcode precheck. Unsupported targets never read or write hidden RAM. */
static bool bus_access(struct lynx_machine *m, uint16_t addr, uint8_t data, uint8_t flags,
                       uint64_t stamp, uint8_t *result)
{
    (void)stamp;
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
    case LYNX__BUS_ROUTE_SUZY:
    case LYNX__BUS_ROUTE_MIKEY:
    case LYNX__BUS_ROUTE_ROM:
    case LYNX__BUS_ROUTE_VECTOR:
        return false;
    }
    return false;
}

/* Half-open budget: every bus cycle that can be granted before target is
   granted, priced, performed and stamped; one that would be granted at or
   after target is not. A cycle granted before target is performed even when
   its duration ends past target; cpu_ready_at records when the next one may
   start, and the clock stops at target. */
lynx_status lynx_advance(lynx_machine *m, uint64_t budget, uint64_t *consumed,
                         lynx_stop_reason *stop)
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
        if (m->cpu_ready_at > m->guest_ticks) {
            /* Inside the previous cycle's duration: the next cycle can start
               only when it ends, and not at all in this call if that is at
               or after target. */
            if (m->cpu_ready_at >= target) {
                m->guest_ticks = target;
                break;
            }
            m->guest_ticks = m->cpu_ready_at;
        }
        const uint16_t addr = m->cpu.req_addr;
        const uint8_t flags = m->cpu.req_flags;
        if (!bus_request_available(m, addr, flags)) {
            *stop = LYNX_STOP_UNAVAILABLE;
            break;
        }
        if (lynx__cpu_at_boundary(&m->cpu) && m->cpu.req_kind == LYNX__CK_OPCODE &&
            !lynx__cpu_opcode_defined(m->cpu.profile, bus_peek(m, addr))) {
            *stop = LYNX_STOP_UNDEFINED;
            break;
        }
        const uint64_t grant = m->guest_ticks; /* no stalls before Phase 4 */
        const lynx__bus_timing price =
            lynx__bus_timing_of(m->cost_model, lynx__bus_region_of(addr), m->cpu.req_kind);
        uint8_t data = 0u;
        if (!bus_access(m, addr, m->cpu.req_data, flags, grant + price.sample, &data)) {
            *stop = LYNX_STOP_UNAVAILABLE;
            break;
        }
        lynx__cpu_complete(&m->cpu, table, data, 0u); /* no interrupt lines before Phase 4 */
        m->cpu_ready_at = grant + price.ticks;
    }
    *consumed = m->guest_ticks - start;
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
    case LYNX_REG_CPU_STATE:
        /* The delta fits 8 bits: guest_ticks <= cpu_ready_at <= guest_ticks +
           255 holds because every cycle costs at most 255 ticks. */
        *value = (uint32_t)m->cpu.flags | ((uint32_t)m->cpu.step << 8) |
                 ((uint32_t)m->cpu.opcode << 16) |
                 ((uint32_t)(m->cpu_ready_at - m->guest_ticks) << 24);
        break;
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
    /* First pass: the whole request must resolve to RAM. MAPCTL readback and
       every unimplemented mapped device/ROM/vector byte refuse atomically. */
    for (size_t i = 0u; i < len; i++) {
        const uint16_t current = (uint16_t)(addr + (uint32_t)i);
        if (lynx__bus_decode(m->mapctl, current) != LYNX__BUS_ROUTE_RAM) {
            return LYNX_ERR_UNAVAILABLE;
        }
    }
    /* Second pass: no mutating bus operation is used for inspection. */
    for (size_t i = 0u; i < len; i++) {
        const uint16_t current = (uint16_t)(addr + (uint32_t)i);
        out[i] = m->ram[current];
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
    out->profile = m->cfg.profile;
    out->ram_policy = m->cfg.ram_policy;
    out->ram_seed = m->cfg.ram_seed;
    out->has_content = m->has_content;
    out->content_addr = m->content_addr;
    out->content_len = m->content_len;
    out->reserved = 0u;
    memcpy(out->content_sha256, m->content_sha256, sizeof out->content_sha256);
    out->content_format = m->content_format;
    out->startup_tier = m->startup_tier;
    out->entry_addr = m->entry_addr;
    out->object_len = m->object_len;
    return LYNX_OK;
}

const char *lynx_build_id(void)
{
    return lynx__build_id;
}
