/* A flat 64 KiB test bus for the CPU: it grants every posted request at once,
   performs it on plain memory, logs (address, data, read or write, kind, flags,
   lines) per cycle, and feeds the CPU a per-cycle interrupt line schedule.
   Header-only and static so every CPU test file can include it. The bus belongs
   to tests; the CPU never sees memory. */
#ifndef LYNX_TESTS_CPU_FLAT_BUS_H
#define LYNX_TESTS_CPU_FLAT_BUS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cpu.h"

enum { FLAT_LOG_MAX = 64, FLAT_CYCLE_BOUND = 16 };

typedef struct flat_entry {
    uint16_t addr;
    uint8_t data, write, kind, flags, lines;
} flat_entry;

typedef struct flat_bus {
    uint8_t mem[65536];
    flat_entry log[FLAT_LOG_MAX]; /* capacity 64 entries since the last reset */
    unsigned nlog;
    uint8_t sched[FLAT_LOG_MAX];  /* interrupt lines given with log entry i */
    bool overrun;                 /* an instruction exceeded the cycle bound */
} flat_bus;

static inline void flat_bus_clear(flat_bus *b)
{
    memset(b, 0, sizeof *b);
}

/* Forget the log; the line schedule restarts at entry 0. */
static inline void flat_bus_reset_log(flat_bus *b)
{
    b->nlog = 0u;
}

static inline void flat_bus_set_lines_all(flat_bus *b, uint8_t lines)
{
    memset(b->sched, lines, sizeof b->sched);
}

static inline void flat_bus_load(flat_bus *b, uint16_t addr, const uint8_t *bytes, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        b->mem[(uint16_t)(addr + i)] = bytes[i];
    }
}

/* Grant, perform and log one posted request, then complete it. */
static inline void flat_bus_cycle(flat_bus *b, lynx__cpu *c, const lynx__cpu_decode *t)
{
    uint8_t data;
    uint8_t lines;
    flat_entry *e;
    unsigned slot = b->nlog < FLAT_LOG_MAX ? b->nlog : FLAT_LOG_MAX - 1u;
    lines = b->sched[slot];
    if ((c->req_flags & LYNX__CF_WRITE) != 0u) {
        data = c->req_data;
        b->mem[c->req_addr] = data;
    } else {
        data = b->mem[c->req_addr];
    }
    if (b->nlog < FLAT_LOG_MAX) {
        e = &b->log[b->nlog];
        e->addr = c->req_addr;
        e->data = data;
        e->write = (c->req_flags & LYNX__CF_WRITE) != 0u ? 1u : 0u;
        e->kind = c->req_kind;
        e->flags = c->req_flags;
        e->lines = lines;
        b->nlog++;
    }
    lynx__cpu_complete(c, t, data, lines);
}

/* Run until the CPU is back at a boundary. A bound of 16 cycles fails instead of
   hanging; returns false and sets overrun then. */
static inline bool flat_bus_step_instruction(flat_bus *b, lynx__cpu *c, const lynx__cpu_decode *t)
{
    unsigned n = 0;
    do {
        if (n >= FLAT_CYCLE_BOUND) {
            b->overrun = true;
            return false;
        }
        flat_bus_cycle(b, c, t);
        n++;
    } while (!lynx__cpu_at_boundary(c));
    return true;
}

/* Run exactly n cycles (an interrupt entry may be cut off or crossed). */
static inline void flat_bus_run_cycles(flat_bus *b, lynx__cpu *c, const lynx__cpu_decode *t, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        flat_bus_cycle(b, c, t);
    }
}

#endif /* LYNX_TESTS_CPU_FLAT_BUS_H */
