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
    memset(m->content_sha256, 0, sizeof m->content_sha256);
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

lynx_status lynx_load_ram_image(lynx_machine *m, const uint8_t *bytes, size_t len,
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
    /* Subtraction form: addr + len could wrap, LYNX_ADDRESS_SPACE - addr cannot. */
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

/* The side-effect-free read the UNDEFINED check uses to look at the next
   opcode without fetching it. Raw RAM until Phase 3 adds the memory map. */
static uint8_t bus_peek(const struct lynx_machine *m, uint16_t addr)
{
    return m->ram[addr];
}

/* Performs one granted bus cycle on raw RAM: the only memory access of the
   CPU. Returns the byte read, or the byte written. stamp is the guest tick at
   which the access takes effect (the grant plus the sample offset); Phase 2
   has no device that observes it. */
static uint8_t bus_access(struct lynx_machine *m, uint16_t addr, uint8_t data, uint8_t flags,
                          uint64_t stamp)
{
    (void)stamp;
    if ((flags & LYNX__CF_WRITE) != 0u) {
        m->ram[addr] = data;
        return data;
    }
    return m->ram[addr];
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
        if (lynx__cpu_at_boundary(&m->cpu) && m->cpu.req_kind == LYNX__CK_OPCODE &&
            !lynx__cpu_opcode_defined(m->cpu.profile, bus_peek(m, m->cpu.req_addr))) {
            *stop = LYNX_STOP_UNDEFINED;
            break;
        }
        const uint64_t grant = m->guest_ticks; /* no stalls before Phase 4 */
        const uint16_t addr = m->cpu.req_addr;
        const lynx__bus_timing price =
            lynx__bus_timing_of(m->cost_model, lynx__bus_region_of(addr), m->cpu.req_kind);
        const uint8_t data =
            bus_access(m, addr, m->cpu.req_data, m->cpu.req_flags, grant + price.sample);
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
    default:
        return LYNX_ERR_RANGE;
    }
    return LYNX_OK;
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
    memcpy(out, &m->ram[addr], len);
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
    return LYNX_OK;
}

const char *lynx_build_id(void)
{
    return lynx__build_id;
}
