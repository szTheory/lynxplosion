/* The machine: instance setup in caller memory, the provisional skeleton step,
   bounded advance and side-effect-free inspection.

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
    m->pc = 0u;
    m->a = 0u;
    m->x = 0u;
    m->y = 0u;
    m->s = 0u;
    m->p = 0u;
    m->halted = 0u;
    m->has_content = 0u;
    m->content_addr = 0u;
    m->content_len = 0u;
    memset(m->content_sha256, 0, sizeof m->content_sha256);
    m->guest_ticks = 0u;
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
    if (cfg->struct_size != (uint32_t)sizeof(lynx_config) || cfg->profile != LYNX_PROFILE_NONE ||
        (cfg->ram_policy != LYNX_RAM_POLICY_ZERO && cfg->ram_policy != LYNX_RAM_POLICY_SPLITMIX64) ||
        cfg->reserved != 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    struct lynx_machine *m = (struct lynx_machine *)mem;
    memset(m, 0, sizeof *m); /* zero the whole struct: no padding byte is ever left over */
    m->cfg = *cfg;
    m->magic = LYNX__MAGIC;
    lynx__power_on(m);
    *out = m;
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
    m->pc = (uint16_t)load_addr;
    m->has_content = 1u;
    m->content_addr = load_addr;
    m->content_len = (uint32_t)len;
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, bytes, len);
    lynx__sha256_final(&hash, m->content_sha256);
    return LYNX_OK;
}

/* PROVISIONAL: replaced by the CPU in Phase 2. Not a CPU. It mixes the
   registers with the byte at the program counter and writes one byte into the
   stack page, so state depends on content, seed and ticks. */
void lynx__skeleton_step(lynx_machine *m)
{
    uint8_t b = m->ram[m->pc];
    if (b == LYNX__HALT_OPCODE) {
        m->halted = 1u;
        return;
    }
    m->a = (uint8_t)(m->a + b);
    m->x = (uint8_t)(m->x ^ m->a);
    m->y = (uint8_t)(m->y + 1u);
    m->ram[0x0100u + m->s] = m->a;
    m->s = (uint8_t)(m->s - 1u);
    /* 32-bit arithmetic: an image that ends exactly at 0x10000 must wrap to
       its start, and a 16-bit pc would wrap to 0 and leave the image. */
    uint32_t next = (uint32_t)m->pc + 1u;
    if (next >= m->content_addr + m->content_len) {
        next = m->content_addr;
    }
    m->pc = (uint16_t)next;
}

uint64_t lynx__budget_steps(uint64_t guest_ticks, uint64_t budget)
{
    uint64_t room = UINT64_MAX - guest_ticks;
    uint64_t usable = budget < room ? budget : room;
    return usable / LYNX__TICKS_PER_STEP;
}

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
    if (m->halted != 0u) {
        *stop = LYNX_STOP_HALT;
        return LYNX_OK;
    }
    uint64_t steps = lynx__budget_steps(m->guest_ticks, budget);
    uint64_t done = 0u;
    while (done < steps) {
        lynx__skeleton_step(m);
        if (m->halted != 0u) {
            *stop = LYNX_STOP_HALT; /* the halting check consumes no ticks */
            break;
        }
        m->guest_ticks += LYNX__TICKS_PER_STEP;
        done++;
    }
    *consumed = done * LYNX__TICKS_PER_STEP;
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
        *value = m->pc;
        break;
    case LYNX_REG_A:
        *value = m->a;
        break;
    case LYNX_REG_X:
        *value = m->x;
        break;
    case LYNX_REG_Y:
        *value = m->y;
        break;
    case LYNX_REG_S:
        *value = m->s;
        break;
    case LYNX_REG_P:
        *value = m->p;
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
