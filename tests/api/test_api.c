/* API tests. One executable, one case per CTest entry: `lynx-test-api <case>`.
   An unknown case exits 2, so a typo in tests.cmake fails instead of passing.

   The cases use the public header. Where a case also needs the layout of the
   instance it says so and includes core/internal.h for sizeof and _Alignof
   only (a white-box check of lynx_machine_size and lynx_machine_align). */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "cpu/flat_bus.h"
#include "internal.h"
#include "lynxplosion.h"
#include "bundle.h"
#include "sha256.h"

#define CHECK_NAMED(cond, name)                                                                     \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "ASSERT %s failed\n", (name));                                     \
            check_failures++;                                                                      \
        }                                                                                          \
    } while (0)

/* The retained Phase 1 tracer image: byte i = (7*i + 3) mod 256 for i =
   0..31. As 65C02 code it reaches the profile-dependent opcode $1F, so it is
   used only by the identity and loader cases. */
static const uint8_t tracer_image[32] = {
    0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49,
    0x50, 0x57, 0x5e, 0x65, 0x6c, 0x73, 0x7a, 0x81, 0x88, 0x8f, 0x96,
    0x9d, 0xa4, 0xab, 0xb2, 0xb9, 0xc0, 0xc7, 0xce, 0xd5, 0xdc,
};

/* fixtures/cpu/sum-trap.s as assembled by the pinned ca65 (load $0200). The
   case "cpu_program" checks the SHA-256 of these bytes against the fixture
   manifest value below, so the array cannot drift from the committed fixture.
   Runs only under profile none: it ends at the bit-op slot $07. */
#define SUM_TRAP_SHA256 "6ae9bada8fb004f5756821c0e756267d2a2e44e6f68928fe27984906acc00f33"
static const uint8_t sum_trap[35] = {
    0xa2, 0x05, 0xa9, 0x00, 0x86, 0x10, 0x18, 0x65, 0x10, 0xca, 0xd0, 0xf8,
    0x8d, 0x00, 0x03, 0x20, 0x20, 0x02, 0xa2, 0xff, 0xbd, 0x01, 0x02, 0x48,
    0xa9, 0x00, 0x68, 0x07, 0xea, 0xea, 0xea, 0xea, 0xe6, 0x11, 0x60,
};

/* fixtures/cpu/churn-loop.s (load $0200): a never-ending loop of opcodes both
   Lynx profiles define identically, reading seeded RAM. 70-tick prelude, then
   255 ticks per iteration at flat5 (hand-derived in the fixture source). */
#define CHURN_LOOP_SHA256 "dcee7216a88ff2ffaf3e4a2794c3369676fc47493d710f26cd8bbadd19745b77"
static const uint8_t churn_loop[46] = {
    0xa2, 0x00, 0xa0, 0x08, 0xa9, 0x00, 0x85, 0x40, 0xa9, 0x05, 0x85, 0x41,
    0xbd, 0x00, 0x04, 0x45, 0x30, 0x71, 0x40, 0x9d, 0x00, 0x06, 0x2a, 0x48,
    0x20, 0x2b, 0x02, 0x68, 0x85, 0x30, 0xe8, 0xd0, 0xeb, 0xe6, 0x31, 0x80,
    0xe7, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0x04, 0x32, 0x60,
};

/* Four NOPs at $FFFA-$FFFD, then JMP $FFFA at $FFFE. Its low operand is
   $FFFF and the high operand comes from $0000 after PC wraps. This private
   snapshot fixture explicitly supplies the vector-window RAM mapping. */
static const uint8_t wrap_image[6] = {0xea, 0xea, 0xea, 0xea, 0x4c, 0xfa};

/* One byte: the bit-op slot $07, so profile none stops before it. */
static const uint8_t trap_image[1] = {0x07};

/* A 10-byte BS93 header and a 13-byte tracer payload at $0200. */
static const uint8_t bll_tracer[] = {
    0x80, 0x08, 0x02, 0x00, 0x00, 0x17, 'B', 'S', '9', '3',
    0xA9, 0x0C, 0x8D, 0xF9, 0xFF, 0xA9, 0xA5, 0x8D, 0x00, 0x04, 0x4C, 0x0A, 0x02,
};

/* Two stores to one watched location, separated by a load. The first store
   must finish before WATCH; rearming then observes the second value. */
static const uint8_t watch_program[] = {
    0xA9u, 0x5Au, 0x8Du, 0x00u, 0x04u,
    0xA9u, 0xA6u, 0x8Du, 0x00u, 0x04u,
    0x4Cu, 0x00u, 0x02u
};

/* Read the watched start, write just before the range, write start, end-1,
   then just after it. The three writes make the half-open contract visible. */
static const uint8_t watch_range_program[] = {
    0xADu, 0x00u, 0x04u,             /* LDA $0400 (reads never hit) */
    0xA9u, 0x11u, 0x8Du, 0xFFu, 0x03u, /* STA $03FF: just before */
    0xA9u, 0x22u, 0x8Du, 0x00u, 0x04u, /* STA $0400: range start */
    0xA9u, 0x33u, 0x8Du, 0x01u, 0x04u, /* STA $0401: end-1 */
    0xA9u, 0x44u, 0x8Du, 0x02u, 0x04u, /* STA $0402: just after */
    0x4Cu, 0x00u, 0x02u
};

/* The counter changes only after the watched store has completed. Replaying
   the same store on resume therefore leaves the independent counter at zero. */
static const uint8_t watch_count_program[] = {
    0xA9u, 0x5Au, 0x8Du, 0x00u, 0x04u, /* LDA #$5A; STA $0400 */
    0xEEu, 0x01u, 0x04u,             /* INC $0401 */
    0x4Cu, 0x00u, 0x02u              /* JMP $0200 */
};

/* Project-authored BLL guest: program timer 0, poll its DONE bit, then store
   the current count and pending timer-0 IRQ bit in ordinary RAM. At flat5 the
   timer enable write is granted at tick 75, the shared-phase edges occur at
   80 and 96, and the two result stores complete at ticks 200 and 240. */
static const uint8_t timer_tracer_program[] = {
    0xA9u, 0x01u,                         /* LDA #1 */
    0x8Du, 0x00u, 0xFDu,                 /* STA $FD00: backup */
    0x8Du, 0x02u, 0xFDu,                 /* STA $FD02: count */
    0xA9u, 0x88u,                         /* LDA #$88: count + IRQ enable */
    0x8Du, 0x01u, 0xFDu,                 /* STA $FD01: start */
    0xADu, 0x03u, 0xFDu,                 /* loop: LDA $FD03 */
    0x29u, 0x08u,                         /* AND #$08: timer done */
    0xF0u, 0xF9u,                         /* BEQ loop */
    0xADu, 0x02u, 0xFDu,                 /* LDA $FD02: current count */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 */
    0xADu, 0x81u, 0xFDu,                 /* LDA $FD81: pending interrupts */
    0x8Du, 0x01u, 0x04u,                 /* STA $0401 */
    0x4Cu, 0x0Du, 0x02u                  /* JMP loop */
};

/* A repeating timer-0 source gives snapshot continuation another event after
   the first IRQ latch. The host holds the real CPU request after CTLA starts
   the timer, so the timer and guest clock continue independently. */
static const uint8_t timer_snapshot_program[] = {
    0x78u,                               /* SEI: keep IRQ pending observable */
    0xA9u, 0x01u,                         /* LDA #1 */
    0x8Du, 0x00u, 0xFDu,                 /* STA timer 0 backup */
    0x8Du, 0x02u, 0xFDu,                 /* STA timer 0 count */
    0xA9u, 0x98u,                         /* LDA #IRQ|RELOAD|COUNT */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA */
    0x4Cu, 0x0Eu, 0x02u                  /* JMP to itself */
};

/* After enabling a repeating timer, ten NOPs and a five-cycle LDA/branch pair
   align the real FD91 store request with the timer edge at tick 240. The request
   returns at that same tick after timer-priority work has already run. */
static const uint8_t timer_cursor_refusal_program[] = {
    0x78u,                               /* SEI */
    0xA9u, 0x01u,                         /* LDA #1 */
    0x8Du, 0x00u, 0xFDu,                 /* STA timer 0 backup */
    0x8Du, 0x02u, 0xFDu,                 /* STA timer 0 count */
    0xA9u, 0x98u,                         /* LDA #IRQ|RELOAD|COUNT */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA at tick 80 */
    0xEAu, 0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xEAu, 0xEAu, 0xEAu, 0xEAu, 0xEAu, /* delay before the request */
    0xA9u, 0x01u, 0xD0u, 0x00u,         /* 2-cycle LDA + taken zero-offset BNE */
    0xA9u, 0x5Au,                         /* LDA #$5A */
    0x8Du, 0x91u, 0xFDu                  /* STA unavailable $FD91 */
};

/* Original guest for the mapped IRQ path. It maps the RAM vector window,
   installs an IRQ vector and handler, checks FD80/FD81 force/ack semantics,
   then waits for a timer-0 IRQ while I is set. It disables that source after
   the pending bit is latched, clears I, and returns through RTI. The handler
   stores a marker before acknowledging so the host can inspect the live stack. */
static const uint8_t irq_guest_program[0x100u + 23u] = {
    0xA9u, 0x08u,                         /* LDA #$08 */
    0x8Du, 0xF9u, 0xFFu,                 /* STA MAPCTL: RAM vectors */
    0xA9u, 0x00u,                         /* LDA #$00 */
    0x8Du, 0xFEu, 0xFFu,                 /* STA IRQ vector low */
    0xA9u, 0x03u,                         /* LDA #$03 */
    0x8Du, 0xFFu, 0xFFu,                 /* STA IRQ vector high -> $0300 */
    0x78u,                               /* SEI: pending does not enter yet */
    0xA9u, 0x03u,                         /* LDA #$03 */
    0x8Du, 0x81u, 0xFDu,                 /* STA INTSET: force T0 and T1 */
    0xADu, 0x80u, 0xFDu,                 /* LDA INTRST: same pending view */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 */
    0xADu, 0x81u, 0xFDu,                 /* LDA INTSET: pure alias read */
    0x8Du, 0x01u, 0x04u,                 /* STA $0401 */
    0xA9u, 0x00u,                         /* LDA #0: zero acknowledgement */
    0x8Du, 0x80u, 0xFDu,                 /* STA INTRST */
    0xADu, 0x81u, 0xFDu,                 /* LDA INTSET */
    0x8Du, 0x02u, 0x04u,                 /* STA $0402 */
    0xA9u, 0x01u,                         /* LDA #1 */
    0x8Du, 0x80u, 0xFDu,                 /* STA INTRST: clear only T0 */
    0xADu, 0x81u, 0xFDu,                 /* LDA INTSET */
    0x8Du, 0x03u, 0x04u,                 /* STA $0403: T1 remains */
    0xA9u, 0x02u,                         /* LDA #2 */
    0x8Du, 0x80u, 0xFDu,                 /* STA INTRST: clear T1 */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x00u, 0xFDu,                 /* STA timer 0 backup */
    0x8Du, 0x02u, 0xFDu,                 /* STA timer 0 count */
    0xA9u, 0x88u,                         /* LDA #IRQ|COUNT */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA */
    0xADu, 0x81u, 0xFDu,                 /* wait: LDA pending */
    0x29u, 0x01u,                         /* AND #T0 */
    0xF0u, 0xF9u,                         /* BEQ wait */
    0xA9u, 0x08u,                         /* LDA #COUNT: disable IRQ source */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA */
    0xADu, 0x81u, 0xFDu,                 /* pending must remain after disable */
    0x8Du, 0x04u, 0x04u,                 /* STA $0404: host stop before CLI */
    0x58u,                               /* CLI */
    0xEAu,                               /* NOP: existing profile POLL takes IRQ */
    0x4Cu, 0x5Du, 0x02u,                 /* JMP $025D after RTI */
    [0x100u] = 0xA9u, 0xA5u,             /* IRQ handler: LDA #$A5 */
    0x8Du, 0x05u, 0x04u,                 /* STA $0405: live-stack watch */
    0xADu, 0x81u, 0xFDu,                 /* LDA pending */
    0x8Du, 0x06u, 0x04u,                 /* STA $0406 */
    0xA9u, 0x01u,                         /* LDA #T0 */
    0x8Du, 0x80u, 0xFDu,                 /* STA INTRST */
    0xADu, 0x80u, 0xFDu,                 /* LDA pending after acknowledgement */
    0x8Du, 0x07u, 0x04u,                 /* STA $0407 */
    0x40u                                /* RTI */
};

/* A masked source still counts and sets DONE, but cannot create a new pending
   interrupt. The guest waits on mapped CTLB, records FD81, and stops on that
   nonempty result write. */
static const uint8_t irq_disabled_source_program[] = {
    0x78u,                               /* SEI */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x02u, 0xFDu,                 /* STA timer 0 count */
    0xA9u, 0x08u,                         /* LDA #COUNT, IRQ disabled */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA */
    0xADu, 0x03u, 0xFDu,                 /* wait: LDA CTLB */
    0x29u, 0x08u,                         /* AND #DONE */
    0xF0u, 0xF9u,                         /* BEQ wait */
    0xADu, 0x81u, 0xFDu,                 /* LDA pending */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 */
    0x4Cu, 0x0Bu, 0x02u                  /* JMP wait */
};

/* The seventh NOP's POLL is at tick 75. Timer 0 expires at 80 while the
   eighth NOP has its opcode cycle in flight; its POLL at 85 must see IRQ. */
static const uint8_t irq_poll_boundary_program[0x100u + 5u] = {
    0x58u,                               /* CLI */
    0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xEAu, 0xEAu, 0xEAu, 0xEAu,         /* eight NOPs */
    0x4Cu, 0x09u, 0x02u,                 /* JMP $0209 if no IRQ */
    [0x100u] = 0xA9u, 0xA5u,             /* handler at $0300 */
    0x8Du, 0x00u, 0x04u                  /* STA $0400 */
};

/* Thirteen cycles precede STA; its final bus cycle/POLL is at tick 80, when
   the timer event is also due. INTRST must clear the newly latched bit before
   that CPU cycle samples the line. */
static const uint8_t irq_poll_ack_program[] = {
    0x58u,                               /* CLI */
    0xE6u, 0x10u,                         /* INC $10: five cycles */
    0xADu, 0x00u, 0x04u,                 /* LDA $0400: four cycles */
    0xA9u, 0x01u,                         /* LDA #T0 */
    0x8Du, 0x80u, 0xFDu,                 /* STA INTRST at tick 80 */
    0x4Cu, 0x0Bu, 0x02u                  /* JMP $020B if no IRQ */
};

/* The store's final poll is exactly at tick 80, colliding with T0/T1 expiry
   and the linked T2 borrow. A watch returns only after the whole prefix and
   CPU completion have become observable. */
static const uint8_t scheduler_watch_program[] = {
    0x58u,                               /* CLI: allow the event to be sampled */
    0xE6u, 0x10u,                         /* INC $10: five cycles */
    0xADu, 0x00u, 0x04u,                 /* LDA $0400: four cycles */
    0xA9u, 0xA5u,                         /* LDA #$A5: two cycles */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 at tick 80 */
    0x4Cu, 0x0Bu, 0x02u                  /* JMP $020B if no watch */
};

/* Project-authored guest reproducing pinned lynx-tests/timers Test 1's
   CTLB borrow input: count $80, write DONE+borrow-in ($0A), then record the
   mapped count and CTLB value. */
static const uint8_t timer_done_borrow_program[] = {
    0xA9u, 0x80u,                         /* LDA #$80 */
    0x8Du, 0x0Eu, 0xFDu,                 /* STA timer 3 count */
    0xA9u, 0x0Au,                         /* LDA #$0A: DONE + borrow-in */
    0x8Du, 0x0Fu, 0xFDu,                 /* STA timer 3 CTLB */
    0xADu, 0x0Eu, 0xFDu,                 /* LDA timer 3 count */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 */
    0xADu, 0x0Fu, 0xFDu,                 /* LDA timer 3 CTLB */
    0x8Du, 0x01u, 0x04u                  /* STA $0401 */
};

/* RESET-DONE stays asserted across many timer source edges, then the guest
   releases it. The test watches both CTLA writes and advances the next edge
   with only the timer bank active. */
static const uint8_t timer_reset_done_program[] = {
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x0Eu, 0xFDu,                 /* STA timer 3 count */
    0xA9u, 0xC8u,                         /* LDA #IRQ|COUNT|RESET-DONE */
    0x8Du, 0x0Du, 0xFDu,                 /* STA timer 3 CTLA */
    0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xEAu, 0xEAu, 0xEAu, 0xEAu,
    0xA9u, 0x88u,                         /* LDA #COUNT */
    0x8Du, 0x0Du, 0xFDu                  /* STA timer 3 CTLA */
};

/* T3 reloads at the first shared-divider edge and clocks linked T5. The loop
   reads T5 CTLB on every pass so the test can hold its mapped read request
   until the precise timer tick and verify timer-before-CPU ordering. */
static const uint8_t timer_link_tracer_program[] = {
    0x78u,                               /* SEI: observe pending without IRQ entry */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x14u, 0xFDu,                 /* STA timer 5 backup */
    0x8Du, 0x16u, 0xFDu,                 /* STA timer 5 count */
    0xA9u, 0xCFu,                         /* LDA #IRQ|RESET-DONE|COUNT|LINK */
    0x8Du, 0x15u, 0xFDu,                 /* STA timer 5 CTLA */
    0xA9u, 0x01u,                         /* LDA #1 */
    0x8Du, 0x0Cu, 0xFDu,                 /* STA timer 3 backup */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x0Eu, 0xFDu,                 /* STA timer 3 count */
    0xA9u, 0x9Eu,                         /* LDA #IRQ|RELOAD|COUNT|source 6 */
    0x8Du, 0x0Du, 0xFDu,                 /* STA timer 3 CTLA */
    0xADu, 0x17u, 0xFDu,                 /* loop: LDA timer 5 CTLB */
    0x8Du, 0x00u, 0x04u,                 /* STA $0400 */
    0xADu, 0x81u, 0xFDu,                 /* LDA pending status */
    0x8Du, 0x01u, 0x04u,                 /* STA $0401 */
    0x4Cu, 0x1Du, 0x02u                  /* JMP loop at $021D */
};

/* Start a zero-count timer-0 one-shot and post a CTLB clear write. The test
   aligns that guest write before, at, or after the tick-1024 expiry. */
static const uint8_t timer_ctlb_boundary_program[] = {
    0x78u,                               /* SEI */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x00u, 0xFDu,                 /* STA timer 0 backup */
    0x8Du, 0x02u, 0xFDu,                 /* STA timer 0 count */
    0xA9u, 0x8Eu,                         /* LDA #IRQ|COUNT|source 6 */
    0x8Du, 0x01u, 0xFDu,                 /* STA timer 0 CTLA */
    0xA9u, 0x00u,                         /* LDA #0 */
    0x8Du, 0x03u, 0xFDu,                 /* STA timer 0 CTLB */
    0x4Cu, 0x13u, 0x02u                  /* JMP to itself */
};

/* MAPCTL is set by guest code before a watch on the last address. */
static const uint8_t watch_last_address_program[] = {
    0xA9u, 0x08u, 0x8Du, 0xF9u, 0xFFu, /* LDA #$08; STA $FFF9 */
    0xA9u, 0xC7u, 0x8Du, 0xFFu, 0xFFu, /* LDA #$C7; STA $FFFF */
    0x4Cu, 0x00u, 0x02u
};

static void make_bll_header(uint8_t *object, size_t len, uint16_t load_addr);
static lynx_status load_bll_at(lynx_machine *m, const uint8_t *payload, size_t payload_len,
                               uint32_t load_addr);

/* Instance memory for the tests: aligned for any type, sized from the core's
   own layout. */
static _Alignas(max_align_t) unsigned char machine_buf[sizeof(struct lynx_machine)];

enum {
    API_FAULT_NONE = 0,
    API_FAULT_WRONG_MAP_BIT,
    API_FAULT_SPECIAL_ADDRESS_ORDER,
    API_FAULT_RAW_AS_MAPPED,
    API_FAULT_ROM_FALLBACK,
    API_FAULT_STOP_BEFORE_STORE,
    API_FAULT_REPEAT_STORE,
    API_FAULT_WATCH_IN_DIGEST
};
static int g_api_fault;

static int is_power_of_two(size_t v)
{
    return v != 0u && (v & (v - 1u)) == 0u;
}

/* The four standard SHA-256 vectors (FIPS 180-4 examples plus the million-a
   message), each confirmed independently with shasum before being written here. */
static void expect_sha256(const void *data, size_t len, const char *want_hex)
{
    lynx__sha256 s;
    uint8_t digest[32];
    char hex[65];
    lynx__sha256_init(&s);
    lynx__sha256_update(&s, data, len);
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
    if (strcmp(hex, want_hex) != 0) {
        fprintf(stderr, "%s:%d: sha256 mismatch: got %s want %s\n", __FILE__, __LINE__, hex,
                want_hex);
        check_failures++;
    }
}

static void case_sha256(void)
{
    expect_sha256("", 0u, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    expect_sha256("abc", 3u, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *two_block = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_EQ_U64(strlen(two_block), 56u);
    expect_sha256(two_block, 56u,
                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    /* One million 'a' bytes fed in 1000-byte chunks. */
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof chunk);
    lynx__sha256 s;
    uint8_t digest[32];
    char hex[65];
    lynx__sha256_init(&s);
    for (int i = 0; i < 1000; i++) {
        lynx__sha256_update(&s, chunk, sizeof chunk);
    }
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

static void case_identity(void)
{
    static const uint8_t zero_hash[32] = {0};
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 0x0123456789abcdefu;
    lynx_machine *m = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, lynx_machine_size(), &cfg, &m), LYNX_OK);
    if (m == NULL) {
        return;
    }

    lynx_identity id;
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, 0u);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_HEX(id.ram_seed, 0x0123456789abcdefu);
    CHECK_EQ_U64(id.has_content, 0u);
    CHECK_EQ_U64(id.content_addr, 0u);
    CHECK_EQ_U64(id.content_len, 0u);
    CHECK_EQ_U64(id.reserved, 0u);
    CHECK(memcmp(id.content_sha256, zero_hash, 32u) == 0);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_NONE);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_NONE);
    CHECK_EQ_U64(id.entry_addr, 0u);
    CHECK_EQ_U64(id.object_len, 0u);
    CHECK_EQ_U64(id.timing_policy, LYNX_TIMING_POLICY_LEGACY_FLAT5);
    CHECK_EQ_U64(id.audio_policy, LYNX_AUDIO_POLICY_DIGITAL_1);
    CHECK_EQ_U64(id.audio_rate_num, LYNX_AUDIO_RATE_NUM);
    CHECK_EQ_U64(id.audio_rate_den, LYNX_AUDIO_RATE_DEN);
    CHECK_EQ_U64(id.audio_channels, LYNX_AUDIO_PCM_CHANNELS);
    CHECK_EQ_U64(id.audio_bits, LYNX_AUDIO_PCM_BITS);

    CHECK_EQ_U64(load_bll_at(m, tracer_image, sizeof tracer_image, 0x0200u), LYNX_OK);
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, 0u);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_HEX(id.ram_seed, 0x0123456789abcdefu);
    CHECK_EQ_U64(id.has_content, 1u);
    CHECK_EQ_HEX(id.content_addr, 0x0200u);
    CHECK_EQ_U64(id.content_len, 32u);
    CHECK_EQ_U64(id.reserved, 0u);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_BS93);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_A);
    CHECK_EQ_U64(id.entry_addr, 0x0200u);
    CHECK_EQ_U64(id.object_len, 42u);
    CHECK_EQ_U64(id.timing_policy, LYNX_TIMING_POLICY_LEGACY_FLAT5);
    CHECK_EQ_U64(id.audio_policy, LYNX_AUDIO_POLICY_DIGITAL_1);
    CHECK_EQ_U64(id.audio_rate_num, LYNX_AUDIO_RATE_NUM);
    CHECK_EQ_U64(id.audio_rate_den, LYNX_AUDIO_RATE_DEN);
    CHECK_EQ_U64(id.audio_channels, LYNX_AUDIO_PCM_CHANNELS);
    CHECK_EQ_U64(id.audio_bits, LYNX_AUDIO_PCM_BITS);
    char hex[65];
    lynx__sha256_hex(id.content_sha256, hex);
    CHECK(strcmp(hex, "bab6f7ea8d9ba129db7e7a776f61a73162ccd67f2348d3a17a31ae7dda31da4e") == 0);

    /* A refused load leaves the identity as it was. */
    uint8_t malformed[sizeof bll_tracer];
    memcpy(malformed, bll_tracer, sizeof malformed);
    malformed[5]--;
    CHECK_EQ_U64(lynx_load_bll(m, malformed, sizeof malformed), LYNX_ERR_SIZE);
    lynx_identity again;
    CHECK_EQ_U64(lynx_get_identity(m, &again), LYNX_OK);
    CHECK(memcmp(&again, &id, sizeof id) == 0);

    /* A different image gives a different hash: the hash follows the bytes. */
    uint8_t other[32];
    memcpy(other, tracer_image, sizeof other);
    other[31] ^= 1u;
    CHECK_EQ_U64(load_bll_at(m, other, sizeof other, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_get_identity(m, &again), LYNX_OK);
    CHECK(memcmp(again.content_sha256, id.content_sha256, 32u) != 0);

    CHECK_EQ_U64(lynx_get_identity(NULL, &id), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_get_identity(m, NULL), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_ERR_STATE);
}

static void case_build_id(void)
{
    const char *id = lynx_build_id();
    CHECK(id != NULL);
    if (id == NULL) {
        return;
    }
    size_t len = strlen(id);
    CHECK(len > 0u);
    CHECK(len < 128u);
    CHECK(strncmp(id, "git=", 4u) == 0);
    CHECK(strstr(id, " cc=") != NULL);
    CHECK(strstr(id, " sys=") != NULL);
    /* No path, date or clock time: none of these characters can appear. */
    CHECK(strchr(id, '/') == NULL);
    CHECK(strchr(id, '~') == NULL);
    CHECK(strchr(id, ':') == NULL);
    /* Fixed for the life of the build: same pointer, same bytes. */
    const char *again = lynx_build_id();
    CHECK(again == id);
    CHECK(strcmp(again, id) == 0);
    CHECK_EQ_U64(strlen(lynx__build_id), len);
}

static lynx_machine *init_in(void *mem, const lynx_config *cfg);

/* ---- Shared helpers for the contract cases (plan 01-06) ------------------

   Every refusal below is proven the same way: copy the whole instance buffer,
   make the call, and memcmp the buffer against the copy. A refusal that
   touches one byte, including padding, fails. The accepted-call controls show
   the comparison can see a change at all. */

#define MSIZE (sizeof(struct lynx_machine))

static _Alignas(max_align_t) unsigned char buf_b[sizeof(struct lynx_machine)];
static unsigned char snap_a[sizeof(struct lynx_machine)];
static unsigned char snap_b[sizeof(struct lynx_machine)];
/* Exact-touch API input starts immediately after this instance. */
static _Alignas(max_align_t) unsigned char bll_adjacent_arena[sizeof(struct lynx_machine) +
                                                              sizeof bll_tracer];
/* Room for a deliberately misaligned start. */
static _Alignas(max_align_t) unsigned char over_buf[sizeof(struct lynx_machine) + 16];
/* One byte more than the address space, for the over-long load refusal. */
static unsigned char big_image[LYNX_ADDRESS_SPACE + 1];
static unsigned char mem_out[LYNX_ADDRESS_SPACE];

static int all_bytes_equal(const unsigned char *p, size_t len, unsigned char v)
{
    for (size_t i = 0u; i < len; i++) {
        if (p[i] != v) {
            return 0;
        }
    }
    return 1;
}

/* Initialises mem with cfg and returns the machine, or NULL after a failed check. */
static lynx_machine *init_in(void *mem, const lynx_config *cfg)
{
    lynx_machine *m = NULL;
    lynx_status st = lynx_machine_init(mem, lynx_machine_size(), cfg, &m);
    CHECK_EQ_U64(st, LYNX_OK);
    if (st != LYNX_OK || m == NULL) {
        return NULL;
    }
    CHECK(m == mem);
    return m;
}

/* The expected whole-buffer comparison: buffer == copy. */
static void check_unchanged(const void *buf, const unsigned char *copy, int line)
{
    if (memcmp(buf, copy, MSIZE) != 0) {
        fprintf(stderr, "%s:%d: instance buffer changed by a call that must not change it\n",
                __FILE__, line);
        check_failures++;
    }
}
#define CHECK_UNCHANGED(buf, copy) check_unchanged((buf), (copy), __LINE__)

/* ---- bounded advance (API-03) ---- */

/* Advances one call and checks what it reports. */
static void advance_expect(lynx_machine *m, uint64_t budget, uint64_t want_consumed,
                           lynx_stop_reason want_stop, int line)
{
    uint64_t consumed = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    lynx_status st = lynx_advance(m, budget, &consumed, &stop);
    if (st != LYNX_OK || consumed != want_consumed || stop != want_stop || consumed > budget) {
        fprintf(stderr,
                "%s:%d: advance(budget %llu): status %llu consumed %llu stop %llu, want consumed "
                "%llu stop %llu\n",
                __FILE__, line, (unsigned long long)budget, (unsigned long long)st,
                (unsigned long long)consumed, (unsigned long long)stop,
                (unsigned long long)want_consumed, (unsigned long long)want_stop);
        check_failures++;
    }
}
#define ADVANCE_EXPECT(m, b, c, s) advance_expect((m), (b), (c), (s), __LINE__)

static uint64_t ticks_of(const lynx_machine *m)
{
    uint64_t t = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &t), LYNX_OK);
    return t;
}

static uint32_t reg_of(const lynx_machine *m, uint32_t reg)
{
    uint32_t v = 0u;
    CHECK_EQ_U64(lynx_read_register(m, reg, &v), LYNX_OK);
    return v;
}

static void expect_bll_refusal(lynx_machine *m, const uint8_t *bytes, size_t len,
                               lynx_status want)
{
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, bytes, len), want);
    CHECK_UNCHANGED(m, snap_a);
}

static void make_bll_header(uint8_t *object, size_t len, uint16_t load_addr)
{
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(load_addr >> 8);
    object[3] = (uint8_t)load_addr;
    object[4] = (uint8_t)(len >> 8);
    object[5] = (uint8_t)len;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
}

/* Package an in-memory payload in the canonical strict BLL object used by
   all normal API workloads. The CPU-vector machine-loop proof has its own
   private adapter in test_cpu_vectors.c. */
static lynx_status load_bll_at(lynx_machine *m, const uint8_t *payload, size_t payload_len,
                               uint32_t load_addr)
{
    if (payload == NULL || payload_len == 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    if (load_addr > UINT16_MAX || payload_len > (size_t)UINT16_MAX - 10u ||
        payload_len > (size_t)LYNX_ADDRESS_SPACE - load_addr) {
        return LYNX_ERR_RANGE;
    }
    const size_t object_len = payload_len + 10u;
    uint8_t *object = malloc(object_len);
    if (object == NULL) {
        fprintf(stderr, "%s:%d: cannot allocate temporary BLL object\n", __FILE__, __LINE__);
        check_failures++;
        return LYNX_ERR_SIZE;
    }
    make_bll_header(object, object_len, (uint16_t)load_addr);
    memcpy(object + 10u, payload, payload_len);
    const lynx_status result = lynx_load_bll(m, object, object_len);
    free(object);
    return result;
}

/* CPU-only reads and writes use a BLL program that selects MAPCTL through the
   guest bus before touching one address. The oracle below is hand-derived
   from the four independent Epyx overlay windows, with the implemented Mikey
   timer registers checked as device routes and unimplemented targets stopping
   before the CPU request completes. */
static int map_target_is_ram(uint8_t mapctl, uint16_t addr)
{
    /* Persistent negative controls deliberately corrupt one expected route.
       The public CPU-request oracle must reject each wrong-map interpretation. */
    if (g_api_fault == API_FAULT_WRONG_MAP_BIT && mapctl == 0u && addr == 0xFC00u) {
        return 1;
    }
    if (g_api_fault == API_FAULT_SPECIAL_ADDRESS_ORDER && mapctl == 0u && addr == 0xFFF8u) {
        return 0;
    }
    if (addr < 0xFC00u || addr == 0xFFF8u) {
        return 1;
    }
    if (addr == 0xFFF9u) {
        return 0; /* MAPCTL is write-only in this provisional guest policy. */
    }
    if (addr <= 0xFCFFu) {
        return (mapctl & 0x01u) != 0u;
    }
    if (addr <= 0xFDFFu) {
        return (mapctl & 0x02u) != 0u;
    }
    if (addr <= 0xFFF7u) {
        return (mapctl & 0x04u) != 0u;
    }
    return (mapctl & 0x08u) != 0u;
}

static int map_target_is_mikey(uint8_t mapctl, uint16_t addr)
{
    return (mapctl & 0x02u) == 0u && lynx__timer_available(addr);
}

static void map_read_one(uint8_t mapctl, uint16_t addr)
{
    const uint8_t program[] = {
        0xA9u, mapctl, 0x8Du, 0xF9u, 0xFFu,       /* LDA #mapctl; STA $FFF9 */
        0xADu, (uint8_t)addr, (uint8_t)(addr >> 8), /* LDA $addr */
        0x8Du, 0x00u, 0x03u, 0x4Cu, 0x0Bu, 0x02u /* STA $0300; JMP $020B */
    };
    const uint8_t sentinel = (uint8_t)(0xA5u ^ (uint8_t)addr ^ mapctl);
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;
    m->ram[0x0300u] = 0u;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    if (map_target_is_ram(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[0x0300u], sentinel);
        return;
    }
    if (map_target_is_mikey(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[0x0300u], lynx__timer_peek(&m->timer_bank, addr));
        return;
    }

    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK(consumed < 512u);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->ram[addr], sentinel);
    memcpy(snap_a, m, MSIZE);
    consumed = UINT64_MAX;
    stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_UNCHANGED(m, snap_a);
}

static void map_write_one(uint8_t mapctl, uint16_t addr)
{
    const uint8_t value = (uint8_t)(0x5Au ^ (uint8_t)addr ^ mapctl);
    const uint8_t sentinel = (uint8_t)~value;
    const uint8_t program[] = {
        0xA9u, mapctl, 0x8Du, 0xF9u, 0xFFu,       /* LDA #mapctl; STA $FFF9 */
        0xA9u, value, 0x8Du, (uint8_t)addr, (uint8_t)(addr >> 8),
        0x4Cu, 0x0Au, 0x02u /* JMP $020A */
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    if (map_target_is_ram(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[addr], value);
        return;
    }
    if (map_target_is_mikey(mapctl, addr)) {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 512u);
        CHECK_EQ_U64(m->ram[addr], sentinel);
        if (addr == 0xFD00u || addr == 0xFD80u || addr == 0xFD81u) {
            CHECK_EQ_U64(lynx__timer_peek(&m->timer_bank, addr),
                         addr == 0xFD80u ? 0u : value);
        }
        return;
    }

    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK(consumed < 512u);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->ram[addr], sentinel);
    memcpy(snap_a, m, MSIZE);
    consumed = UINT64_MAX;
    stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_UNCHANGED(m, snap_a);
}

static void map_latch_one(uint8_t value)
{
    const uint8_t raw_sentinel = (uint8_t)(value ^ 0xC3u);
    const uint8_t program[] = {
        0xA9u, value, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x05u, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[0xFFF9u] = raw_sentinel;
    ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, value);
    CHECK_EQ_U64(m->ram[0xFFF9u], raw_sentinel);
}

static void case_map_matrix(void)
{
    static const uint16_t boundaries[] = {
        0xFBFFu, 0xFC00u, 0xFCFFu, 0xFD00u, 0xFD1Fu, 0xFD20u,
        0xFD7Fu, 0xFD80u, 0xFD81u, 0xFD82u, 0xFDFFu, 0xFE00u,
        0xFFF7u, 0xFFF8u, 0xFFF9u, 0xFFFAu, 0xFFFFu
    };
    static const uint16_t window_writes[] = {
        0xFC00u, 0xFCFFu, 0xFD00u, 0xFD1Fu, 0xFD20u, 0xFD7Fu,
        0xFD80u, 0xFD81u, 0xFD82u, 0xFDFFu, 0xFE00u, 0xFFF7u,
        0xFFF8u, 0xFFFAu, 0xFFFFu
    };
    for (uint32_t mapctl = 0u; mapctl < 16u; mapctl++) {
        for (size_t i = 0u; i < sizeof boundaries / sizeof boundaries[0]; i++) {
            const int before = check_failures;
            map_read_one((uint8_t)mapctl, boundaries[i]);
            if (check_failures != before) {
                return;
            }
        }
        for (size_t i = 0u; i < sizeof window_writes / sizeof window_writes[0]; i++) {
            const int before = check_failures;
            map_write_one((uint8_t)mapctl, window_writes[i]);
            if (check_failures != before) {
                return;
            }
        }
    }
    for (uint32_t value = 0u; value <= UINT8_MAX; value++) {
        const int before = check_failures;
        map_latch_one((uint8_t)value);
        if (check_failures != before) {
            return;
        }
    }
    /* Mapped absent vectors must win over profile-none's opcode disagreement. */
    const uint8_t undefined_vector[] = {0x07u};
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, undefined_vector, sizeof undefined_vector, 0xFFFAu), LYNX_OK);
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_advance(m, 50u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK_EQ_U64(consumed, 0u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFFFAu);
    }
}

static void case_bll_load(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);

    uint8_t short_object[10];
    make_bll_header(short_object, sizeof short_object, 0x0200u);
    expect_bll_refusal(m, short_object, sizeof short_object, LYNX_ERR_SIZE);

    /* The shortest accepted object has exactly one payload byte. Its load
       ends at address-space boundary 0x10000, and its source is freed after
       loading to prove the core copied the byte. */
    uint8_t min_object[11];
    make_bll_header(min_object, sizeof min_object, 0xFFFFu);
    min_object[10] = 0xA5u;
    uint8_t *source = malloc(sizeof min_object);
    CHECK(source != NULL);
    if (source == NULL) {
        goto done;
    }
    memcpy(source, min_object, sizeof min_object);
    CHECK_EQ_U64(lynx_load_bll(m, source, sizeof min_object), LYNX_OK);
    memset(source, 0, sizeof min_object);
    free(source);
    uint8_t end_byte = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, &end_byte, 1u), LYNX_OK);
    CHECK_EQ_U64(end_byte, 0xA5u);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_addr, 0xFFFFu);
    CHECK_EQ_U64(id.entry_addr, 0xFFFFu);
    CHECK_EQ_U64(id.content_len, 1u);
    CHECK_EQ_U64(id.object_len, sizeof min_object);

    /* Largest representable total length fits exactly when the payload starts
       at 11: 65535 - 10 payload bytes end at 65536 without wrapping. */
    uint8_t *max_object = malloc(UINT16_MAX);
    CHECK(max_object != NULL);
    if (max_object == NULL) {
        goto done;
    }
    make_bll_header(max_object, UINT16_MAX, 11u);
    memset(max_object + 10u, 0x5Cu, UINT16_MAX - 10u);
    CHECK_EQ_U64(lynx_load_bll(m, max_object, UINT16_MAX), LYNX_OK);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_addr, 11u);
    CHECK_EQ_U64(id.content_len, UINT16_MAX - 10u);
    CHECK_EQ_U64(id.object_len, UINT16_MAX);
    CHECK_EQ_U64(lynx_read_memory(m, 11u, mem_out, UINT16_MAX - 10u), LYNX_OK);
    CHECK(memcmp(mem_out, max_object + 10u, UINT16_MAX - 10u) == 0);
    free(max_object);

    uint8_t past_end[12];
    make_bll_header(past_end, sizeof past_end, 0xFFFFu);
    past_end[10] = 0xAAu;
    past_end[11] = 0xBBu;
    expect_bll_refusal(m, past_end, sizeof past_end, LYNX_ERR_RANGE);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_load\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_bll_alias(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    memset(bll_adjacent_arena, 0xA5, sizeof bll_adjacent_arena);
    lynx_machine *m = init_in(bll_adjacent_arena, &cfg);
    if (m == NULL) {
        goto done;
    }
    uint8_t *const adjacent = bll_adjacent_arena + MSIZE;
    memcpy(adjacent, bll_tracer, sizeof bll_tracer);
    memcpy(snap_a, bll_adjacent_arena, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, adjacent, sizeof bll_tracer), LYNX_OK);
    CHECK(memcmp(snap_a, bll_adjacent_arena, MSIZE) != 0); /* accepted control */
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);

    /* Every overlap with the complete instance, including header, RAM, the
       last byte and a one-byte partial overlap, is refused unchanged. */
    expect_bll_refusal(m, (const uint8_t *)m, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, m->ram + 0x0200u, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, (const uint8_t *)m + MSIZE - 1u, sizeof bll_tracer,
                       LYNX_ERR_ARGUMENT);
    /* Exact touch after the instance is disjoint. A huge size is rejected by
       the length bound without reading the claimed bytes or changing state. */
    expect_bll_refusal(m, adjacent, SIZE_MAX, LYNX_ERR_RANGE);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.bll_alias\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_bll_tracer(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    lynx_identity id;
    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_NONE);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_NONE);
    CHECK_EQ_U64(id.entry_addr, 0u);
    CHECK_EQ_U64(id.object_len, 0u);

    uint8_t source[sizeof bll_tracer];
    memcpy(source, bll_tracer, sizeof source);
    CHECK_EQ_U64(lynx_load_bll(m, source, sizeof source), LYNX_OK);
    CHECK_EQ_U64(reg_of(m, LYNX_REG_PC), 0x0200u);
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    memset(&id, 0xA5, sizeof id);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.content_format, LYNX_CONTENT_BS93);
    CHECK_EQ_U64(id.startup_tier, LYNX_STARTUP_A);
    CHECK_EQ_U64(id.content_addr, 0x0200u);
    CHECK_EQ_U64(id.content_len, 13u);
    CHECK_EQ_U64(id.entry_addr, 0x0200u);
    CHECK_EQ_U64(id.object_len, sizeof bll_tracer);
    char hash_hex[65];
    lynx__sha256_hex(id.content_sha256, hash_hex);
    CHECK(strcmp(hash_hex, "50ddb4511395952841333f53031b264adde66d40953df60a2a7c4bcdaf52d279") == 0);

    /* The accepted payload is copied into RAM; changing the caller's object
       cannot alter guest code or the core-computed object identity. */
    source[10] = 0xEAu;
    lynx_identity after_source_change;
    CHECK_EQ_U64(lynx_get_identity(m, &after_source_change), LYNX_OK);
    CHECK(memcmp(after_source_change.content_sha256, id.content_sha256, 32u) == 0);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 300u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 300u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    uint8_t result = 0xA5u;
    CHECK_EQ_U64(lynx_read_memory(m, 0x0400u, &result, 1u), LYNX_OK);
    CHECK_EQ_U64(result, 0xA5u);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFF9u, &result, 1u), LYNX_OK);
    CHECK_EQ_U64(result, 0u); /* raw RAM remains distinct from the latch */

    /* Every malformed object is rejected before power-on and leaves the
       already-running instance byte-identical. */
    expect_bll_refusal(m, NULL, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, bll_tracer, 0u, LYNX_ERR_ARGUMENT);
    uint8_t malformed[sizeof bll_tracer + 1u];
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[0] = 0x81u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[1] = 0x00u; /* unsupported/cart-shaped type, not a BS93 load */
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[6] = 'X';
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[5] = 0x16u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_SIZE);
    expect_bll_refusal(m, bll_tracer, sizeof bll_tracer - 1u, LYNX_ERR_SIZE);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[sizeof bll_tracer] = 0u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer + 1u, LYNX_ERR_SIZE);
    const uint8_t empty_payload[10] = {0x80u, 0x08u, 0x02u, 0x00u,
                                       0x00u, 0x0Au, 'B', 'S', '9', '3'};
    expect_bll_refusal(m, empty_payload, sizeof empty_payload, LYNX_ERR_SIZE);
    memcpy(malformed, bll_tracer, sizeof bll_tracer);
    malformed[2] = 0xFFu;
    malformed[3] = 0xF8u;
    expect_bll_refusal(m, malformed, sizeof bll_tracer, LYNX_ERR_RANGE);
    expect_bll_refusal(m, (const uint8_t *)m, sizeof *m, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, big_image, LYNX_ADDRESS_SPACE, LYNX_ERR_RANGE);

    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_tracer\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* ---- init refusal (API-01, API-02) ---- */

static void init_refusal_one(int line, void *mem, size_t size, const lynx_config *cfg,
                             int pass_out, lynx_status want)
{
    /* Both buffers hold a known pattern; a refusal must leave it alone. */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    memset(over_buf, 0xA5, sizeof over_buf);
    lynx_machine *const sentinel = (lynx_machine *)(uintptr_t)0x5A5A5A58u;
    lynx_machine *out = sentinel;
    lynx_status st = lynx_machine_init(mem, size, cfg, pass_out ? &out : NULL);
    if (st != want) {
        fprintf(stderr, "%s:%d: init status %llu, want %llu\n", __FILE__, line,
                (unsigned long long)st, (unsigned long long)want);
        check_failures++;
    }
    if (out != sentinel) {
        fprintf(stderr, "%s:%d: init refusal wrote *out\n", __FILE__, line);
        check_failures++;
    }
    if (!all_bytes_equal(machine_buf, sizeof machine_buf, 0xA5) ||
        !all_bytes_equal(over_buf, sizeof over_buf, 0xA5)) {
        fprintf(stderr, "%s:%d: init refusal wrote to the buffer\n", __FILE__, line);
        check_failures++;
    }
}

static void case_init_refusal(void)
{
    lynx_config good;
    lynx_config_default(&good);
    size_t size = lynx_machine_size();
    lynx_config bad;

    init_refusal_one(__LINE__, NULL, size, &good, 1, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size, NULL, 1, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size, &good, 0, LYNX_ERR_ARGUMENT);
    init_refusal_one(__LINE__, machine_buf, size - 1u, &good, 1, LYNX_ERR_SIZE);
    init_refusal_one(__LINE__, machine_buf, 0u, &good, 1, LYNX_ERR_SIZE);
    /* Misaligned start with enough room: alignment is the refusal reason. */
    init_refusal_one(__LINE__, over_buf + 1, size + 8u, &good, 1, LYNX_ERR_ALIGNMENT);
    /* Misaligned and too small: the size check comes first. */
    init_refusal_one(__LINE__, machine_buf + 1, size - 1u, &good, 1, LYNX_ERR_SIZE);

    bad = good;
    bad.struct_size += 1u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.struct_size = 0u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.profile = 3u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.profile = 0xFFFFFFFFu;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.ram_policy = 2u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);
    bad = good;
    bad.reserved = 1u;
    init_refusal_one(__LINE__, machine_buf, size, &bad, 1, LYNX_ERR_ARGUMENT);

    /* Positive control: the same call with good input writes the buffer and *out. */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    lynx_machine *out = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, size, &good, &out), LYNX_OK);
    CHECK(out == (lynx_machine *)machine_buf);
    CHECK(!all_bytes_equal(machine_buf, sizeof machine_buf, 0xA5));

    /* Profiles 1 and 2 are accepted and reported. */
    for (uint32_t p = LYNX_PROFILE_LYNX1; p <= LYNX_PROFILE_LYNX2; p++) {
        lynx_config c = good;
        c.profile = p;
        out = NULL;
        CHECK_EQ_U64(lynx_machine_init(machine_buf, size, &c, &out), LYNX_OK);
        lynx_identity id;
        CHECK_EQ_U64(lynx_get_identity(out, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, p);
    }

    /* The config is copied: changing the caller's struct after init changes
       nothing, and a reset still uses the values given at init. */
    lynx_config mine = good;
    mine.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    mine.ram_seed = 5u;
    lynx_machine *m = init_in(machine_buf, &mine);
    lynx_machine *twin = init_in(buf_b, &mine);
    if (m == NULL || twin == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    mine.profile = LYNX_PROFILE_LYNX2;
    mine.ram_seed = 6u;
    mine.ram_policy = LYNX_RAM_POLICY_ZERO;
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, LYNX_PROFILE_NONE);
    CHECK_EQ_U64(id.ram_seed, 5u);
}

/* ---- instance independence and init/deinit idempotency (API-01) ---- */

static _Alignas(max_align_t) unsigned char buf_c[sizeof(struct lynx_machine)];
static unsigned char solo_runs[3][sizeof(struct lynx_machine)];

static void case_instances_independent(void)
{
    lynx_config cfg_a;
    lynx_config cfg_b;
    lynx_config_default(&cfg_a);
    lynx_config_default(&cfg_b);
    cfg_a.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg_a.ram_seed = 1u;
    cfg_b.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg_b.ram_seed = 2u;

    lynx_machine *a = init_in(machine_buf, &cfg_a);
    lynx_machine *b = init_in(buf_b, &cfg_b);
    if (a == NULL || b == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(a, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(load_bll_at(b, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);

    /* Advancing A leaves B's whole buffer alone. */
    memcpy(snap_b, buf_b, MSIZE);
    memcpy(snap_a, machine_buf, MSIZE);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(a, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    CHECK_UNCHANGED(buf_b, snap_b);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0); /* control: A did change */

    /* Loading into A leaves B alone. */
    uint8_t other[16];
    memset(other, 0x11, sizeof other);
    CHECK_EQ_U64(load_bll_at(a, other, sizeof other, 0x4000u), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Resetting A leaves B alone. */
    CHECK_EQ_U64(lynx_reset(a), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Deinitialising A leaves B alone, and B still runs. */
    lynx_machine_deinit(a);
    CHECK_UNCHANGED(buf_b, snap_b);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(b, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_seed, 2u);
    CHECK_EQ_U64(lynx_advance(b, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);

    /* Deinit twice, and deinit of NULL, are no-ops. */
    lynx_machine_deinit(a);
    memcpy(snap_a, machine_buf, MSIZE);
    lynx_machine_deinit(a);
    lynx_machine_deinit(NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Init twice on one buffer with one config gives identical bytes, whatever
       the buffer held before (so no stale byte or padding reaches the result). */
    memset(machine_buf, 0xA5, sizeof machine_buf);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    memcpy(snap_a, machine_buf, MSIZE);
    memset(machine_buf, 0x5A, sizeof machine_buf);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK(init_in(machine_buf, &cfg_a) != NULL);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* One instance per profile, advanced interleaved in 100-tick slices, each
       ends byte-identical to its own solo run of the same 3000 ticks. */
    static const uint32_t profiles[3] = {LYNX_PROFILE_NONE, LYNX_PROFILE_LYNX1,
                                         LYNX_PROFILE_LYNX2};
    unsigned char *bufs[3] = {machine_buf, buf_b, buf_c};
    lynx_machine *ms[3] = {NULL, NULL, NULL};
    for (size_t i = 0u; i < 3u; i++) {
        lynx_config c = cfg_a;
        c.profile = profiles[i];
        lynx_machine *solo = init_in(bufs[i], &c);
        if (solo == NULL) {
            return;
        }
        CHECK_EQ_U64(load_bll_at(solo, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(solo, 3000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 3000u);
        memcpy(solo_runs[i], bufs[i], MSIZE);
        ms[i] = init_in(bufs[i], &c);
        if (ms[i] == NULL) {
            return;
        }
        CHECK_EQ_U64(load_bll_at(ms[i], churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    }
    for (uint32_t slice = 0u; slice < 30u; slice++) {
        for (size_t i = 0u; i < 3u; i++) {
            CHECK_EQ_U64(lynx_advance(ms[i], 100u, &consumed, &stop), LYNX_OK);
            CHECK_EQ_U64(consumed, 100u);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        }
    }
    for (size_t i = 0u; i < 3u; i++) {
        CHECK_UNCHANGED(bufs[i], solo_runs[i]);
        lynx_identity pid;
        CHECK_EQ_U64(lynx_get_identity(ms[i], &pid), LYNX_OK);
        CHECK_EQ_U64(pid.profile, profiles[i]);
    }
    /* The loop uses only opcodes both Lynx tables agree on, so the three
       profiles reach the same registers and RAM (the instances differ only in
       the stored profile id). */
    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v0 = 0u;
        uint32_t v1 = 1u;
        uint32_t v2 = 2u;
        CHECK_EQ_U64(lynx_read_register(ms[0], r, &v0), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(ms[1], r, &v1), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(ms[2], r, &v2), LYNX_OK);
        CHECK_EQ_HEX(v1, v0);
        CHECK_EQ_HEX(v2, v0);
    }
    CHECK(memcmp(((struct lynx_machine *)machine_buf)->ram, ((struct lynx_machine *)buf_b)->ram,
                 LYNX_ADDRESS_SPACE) == 0);
    CHECK(memcmp(((struct lynx_machine *)machine_buf)->ram, ((struct lynx_machine *)buf_c)->ram,
                 LYNX_ADDRESS_SPACE) == 0);
}

/* ---- load refusal (API-02) ---- */

static void case_load_refusal_unchanged(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 7u;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    memcpy(snap_a, machine_buf, MSIZE);

    /* BLL malformed shape, marker, exact-length and range refusals are
       checked against the entire instance after a prior run. */
    expect_bll_refusal(m, NULL, sizeof bll_tracer, LYNX_ERR_ARGUMENT);
    expect_bll_refusal(m, bll_tracer, 0u, LYNX_ERR_ARGUMENT);
    uint8_t short_object[10];
    make_bll_header(short_object, sizeof short_object, 0x0200u);
    expect_bll_refusal(m, short_object, sizeof short_object, LYNX_ERR_SIZE);
    uint8_t wrong_format[sizeof bll_tracer];
    memcpy(wrong_format, bll_tracer, sizeof wrong_format);
    wrong_format[6] = 'X';
    expect_bll_refusal(m, wrong_format, sizeof wrong_format, LYNX_ERR_ARGUMENT);
    uint8_t wrong_length[sizeof bll_tracer];
    memcpy(wrong_length, bll_tracer, sizeof wrong_length);
    wrong_length[5]++;
    expect_bll_refusal(m, wrong_length, sizeof wrong_length, LYNX_ERR_SIZE);
    uint8_t past_end[12];
    make_bll_header(past_end, sizeof past_end, 0xFFFFu);
    past_end[10] = 0xAAu;
    past_end[11] = 0xBBu;
    expect_bll_refusal(m, past_end, sizeof past_end, LYNX_ERR_RANGE);
    expect_bll_refusal(m, big_image, (size_t)UINT16_MAX + 1u, LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_load_bll(NULL, bll_tracer, sizeof bll_tracer), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Control: an accepted canonical object is visible to the comparison. */
    CHECK_EQ_U64(load_bll_at(m, tracer_image, 16u, 0x0300u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);

    /* BLL may end exactly at 0x10000, but has no raw whole-space form. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(load_bll_at(m, big_image, 16u, 0xFFF0u), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(load_bll_at(m, big_image, 1u, 0xFFFFu), LYNX_OK);

    /* A deinitialised machine refuses a load and stays as it was. */
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_ERR_STATE);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

/* ---- inspect purity (API-05) ---- */

static void case_inspect_pure(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = 11u;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4096u);
    memcpy(snap_a, machine_buf, MSIZE);

    uint64_t ticks_before = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_before), LYNX_OK);
    CHECK_EQ_U64(ticks_before, 4096u);

    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v = 0u;
        CHECK_EQ_U64(lynx_read_register(m, r, &v), LYNX_OK);
    }
    /* The two CPU registers carry the documented encoding of the instance's
       fields (white-box), and P reads with bits 4 and 5 set. 4096 ticks end
       inside PLA (cycle 40 of the sixteenth iteration, granted at 4095). */
    const struct lynx_machine *w = (const struct lynx_machine *)machine_buf;
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE),
                 (uint32_t)w->cpu.flags | ((uint32_t)w->cpu.step << 8) |
                     ((uint32_t)w->cpu.opcode << 16) |
                     ((uint32_t)(w->cpu_ready_at - w->guest_ticks) << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH),
                 (uint32_t)w->cpu.ea | ((uint32_t)w->cpu.tmp << 16) |
                     ((uint32_t)w->cost_model << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 16, 0x0468u); /* 4 ticks left, opcode PLA */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_P) & 0x30u, 0x30u);
    uint32_t v = 0xA5A5A5A5u;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_COUNT, &v), LYNX_ERR_RANGE);
    CHECK_EQ_HEX(v, 0xA5A5A5A5u);
    CHECK_EQ_U64(lynx_read_register(m, 0xFFFFFFFFu, &v), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_PC, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_register(NULL, LYNX_REG_PC, &v), LYNX_ERR_ARGUMENT);

    memset(mem_out, 0xA5, sizeof mem_out);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE), LYNX_OK);
    CHECK(memcmp(mem_out, ((struct lynx_machine *)machine_buf)->ram, LYNX_ADDRESS_SPACE) == 0);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, &m->ram[0x0100u], 1u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Keep later checks independent when proving the pre-fix implementation
       incorrectly accepts and writes this aliased output. */
    memcpy(machine_buf, snap_a, MSIZE);

    /* A caller-owned output immediately after the machine is disjoint and is
       accepted, including when the two ranges share a boundary. */
    lynx_machine *adjacent = init_in(bll_adjacent_arena, &cfg);
    if (adjacent != NULL) {
        CHECK_EQ_U64(load_bll_at(adjacent, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
        uint8_t *adjacent_out = bll_adjacent_arena + MSIZE;
        CHECK_EQ_U64(lynx_read_memory(adjacent, 0x0200u, adjacent_out, 4u), LYNX_OK);
        CHECK(memcmp(adjacent_out, adjacent->ram + 0x0200u, 4u) == 0);
    }
    memset(mem_out, 0xA5, sizeof mem_out);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, mem_out, 2u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0x10000u, mem_out, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFFFFFu, mem_out, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE + 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, SIZE_MAX), LYNX_ERR_RANGE);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0xA5)); /* RANGE writes nothing */
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, mem_out, 0u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(m, 0x0200u, NULL, 4u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(NULL, 0x0200u, mem_out, 4u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFFFu, mem_out, 1u), LYNX_OK);

    uint64_t ticks_after = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_after), LYNX_OK);
    CHECK_EQ_U64(ticks_after, ticks_before);
    CHECK_EQ_U64(lynx_guest_ticks(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_guest_ticks(NULL, &ticks_after), LYNX_ERR_ARGUMENT);

    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.has_content, 1u);
    CHECK_EQ_U64(lynx_get_identity(m, NULL), LYNX_ERR_ARGUMENT);

    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A deinitialised machine: every inspect refuses with STATE, buffer untouched. */
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_PC, &v), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, 4u), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks_after), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_ERR_STATE);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

static void capture_inspection_snapshot(const lynx_machine *m, uint8_t *out, size_t expected_size)
{
    size_t size = 0u;
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, expected_size);
    CHECK_EQ_U64(lynx_snapshot_save(m, out, expected_size, &written), LYNX_OK);
    CHECK_EQ_U64(written, expected_size);
}

static void case_memory_peek_pure(void)
{
    enum { INSPECT_SNAPSHOT_SIZE = LYNX__SNAPSHOT_HEADER_LEN + LYNX__SNAPSHOT_PAYLOAD_LEN };
    uint8_t before_snapshot[INSPECT_SNAPSHOT_SIZE];
    uint8_t after_snapshot[INSPECT_SNAPSHOT_SIZE];
    const uint8_t select_all_ram[] = {
        0xA9u, 0x8Fu, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x05u, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, select_all_ram, sizeof select_all_ram, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
    uint8_t low_ram = 0u;
    lynx_status low_status = lynx_read_cpu_memory(m, 0x0200u, &low_ram, 1u);
    if (low_status != LYNX_OK) {
        CHECK_EQ_U64(low_status, LYNX_OK);
        return;
    }
    CHECK_EQ_HEX(low_ram, 0xA9u);
    uint32_t mapctl = 0u;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_MAPCTL, &mapctl), LYNX_OK);
    CHECK_EQ_HEX(mapctl, 0x8Fu);

    m->ram[0xFC00u] = 0xC1u;
    m->ram[0xFD00u] = 0xD2u;
    m->ram[0xFE00u] = 0xE3u;
    m->ram[0xFFFAu] = 0xF4u;
    m->ram[0xFFF8u] = 0x38u;
    m->ram[0xFFF9u] = 0x59u;
    static const struct {
        uint16_t addr;
        uint8_t value;
    } mapped_ram[] = {
        {0xFC00u, 0xC1u}, {0xFD00u, 0xD2u}, {0xFE00u, 0xE3u}, {0xFFFAu, 0xF4u}, {0xFFF8u, 0x38u}
    };
    for (size_t i = 0u; i < sizeof mapped_ram / sizeof mapped_ram[0]; i++) {
        uint8_t out = 0xA5u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, mapped_ram[i].addr, &out, 1u), LYNX_OK);
        CHECK_EQ_U64(out, mapped_ram[i].value);
    }
    uint8_t raw_mapctl_byte = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFFF9u, &raw_mapctl_byte, 1u), LYNX_OK);
    CHECK_EQ_HEX(raw_mapctl_byte, 0x59u);

    /* The latch survives snapshots, including bit 7, but mapped guest readback
       remains unavailable. */
    capture_inspection_snapshot(m, before_snapshot, sizeof before_snapshot);
    lynx_machine *m2 = init_in(buf_b, &cfg);
    if (m2 == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m2, select_all_ram, sizeof select_all_ram, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, before_snapshot, sizeof before_snapshot), LYNX_OK);
    CHECK_EQ_U64(lynx_read_register(m2, LYNX_REG_MAPCTL, &mapctl), LYNX_OK);
    CHECK_EQ_HEX(mapctl, 0x8Fu);

    m->mapctl = 0u;
    m->ram[0xFBFFu] = 0xA1u;
    m->ram[0xFC00u] = 0xB2u;
    uint8_t raw_suzy = 0u;
    const lynx_status raw_status =
        g_api_fault == API_FAULT_RAW_AS_MAPPED
            ? lynx_read_cpu_memory(m, 0xFC00u, &raw_suzy, 1u)
            : lynx_read_memory(m, 0xFC00u, &raw_suzy, 1u);
    CHECK_EQ_U64(raw_status, LYNX_OK);
    CHECK_EQ_HEX(raw_suzy, 0xB2u);
    m->ram[0xFD20u] = 0xA7u;
    CHECK_NAMED(lynx__audio_write(&m->audio, 0xFD20u, 0x7Fu, m->guest_ticks, NULL),
                "memory.peek.audio-write-fixture");
    uint8_t mapped_audio_volume = 0xCCu;
    const uint64_t audio_peek_ticks = ticks_of(m);
    memcpy(snap_a, m, MSIZE);
    const lynx_status audio_peek_status =
        lynx_read_cpu_memory(m, 0xFD20u, &mapped_audio_volume, 1u);
    CHECK_NAMED(audio_peek_status == LYNX_OK, "memory.peek.audio-mapped-route");
    CHECK_NAMED(mapped_audio_volume == 0x7Fu, "memory.peek.audio-mapped-volume");
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(ticks_of(m), audio_peek_ticks);
    uint8_t raw_audio_volume = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFD20u, &raw_audio_volume, 1u), LYNX_OK);
    CHECK_EQ_HEX(raw_audio_volume, 0xA7u);
    uint64_t ticks_before = ticks_of(m);
    memcpy(snap_a, m, MSIZE);
    capture_inspection_snapshot(m, before_snapshot, sizeof before_snapshot);
    uint8_t crossing[2] = {0x5Au, 0xA5u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFBFFu, crossing, sizeof crossing),
                 LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_HEX(crossing[0], 0x5Au);
    CHECK_EQ_HEX(crossing[1], 0xA5u);
    uint8_t unavailable = 0xCCu;
    uint8_t timer_backup = 0xCCu;
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, &timer_backup, 1u), LYNX_OK);
    CHECK_EQ_U64(timer_backup, 0u); /* the timer view is separate from raw RAM */
    static const uint16_t unavailable_addresses[] = {
        0xFC00u, 0xFE00u, 0xFFF9u, 0xFFFAu};
    for (size_t i = 0u; i < sizeof unavailable_addresses / sizeof unavailable_addresses[0]; i++) {
        unavailable = 0xCCu;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, unavailable_addresses[i], &unavailable, 1u),
                     LYNX_ERR_UNAVAILABLE);
        CHECK_EQ_HEX(unavailable, 0xCCu);
    }
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0x0200u, &m->ram[0x0100u], 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFFFFu, crossing, 2u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0x10000u, crossing, 1u), LYNX_ERR_RANGE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0u, crossing, 0u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0u, NULL, 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_read_cpu_memory(NULL, 0u, crossing, 1u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(ticks_of(m), ticks_before);
    capture_inspection_snapshot(m, after_snapshot, sizeof after_snapshot);
    CHECK(memcmp(before_snapshot, after_snapshot, sizeof before_snapshot) == 0);

    lynx_machine_deinit(m2);
}

static void rom_read_request(int ram_selected, uint16_t addr, uint8_t map_bit, uint8_t sentinel)
{
    const uint8_t program[] = {
        0xA9u, ram_selected != 0 ? map_bit : 0u, 0x8Du, 0xF9u, 0xFFu,
        0xADu, (uint8_t)addr, (uint8_t)(addr >> 8), 0x8Du, 0x00u, 0x03u,
        0x4Cu, 0x0Bu, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[addr] = sentinel;
    m->ram[0x0300u] = 0u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        if (g_api_fault == API_FAULT_ROM_FALLBACK) {
            /* Counterfactual fallback mutant: it reads the backing RAM and
               completes instead of returning the public unavailable stop. */
            stop = LYNX_STOP_BUDGET;
            consumed = 128u;
            m->ram[0x0300u] = sentinel;
        }
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, addr);
        CHECK_EQ_HEX(m->ram[addr], sentinel);
        CHECK_EQ_HEX(m->ram[0x0300u], 0u);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0x0300u], sentinel);
    }
    lynx_machine_deinit(m);
}

static void rom_write_request(int ram_selected)
{
    const uint8_t program[] = {
        0xA9u, ram_selected != 0 ? 0x04u : 0x00u, 0x8Du, 0xF9u, 0xFFu,
        0xA9u, 0xA5u, 0x8Du, 0x00u, 0xFEu, 0x4Cu, 0x0Au, 0x02u
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
        CHECK_EQ_HEX(m->ram[0xFE00u], 0x3Cu);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0xFE00u], 0xA5u);
    }
    lynx_machine_deinit(m);
}

static void rom_opcode_request(int ram_selected)
{
    const uint8_t boot_without_map[] = {0x4Cu, 0x00u, 0xFEu};
    const uint8_t boot_with_map[] = {
        0xA9u, 0x04u, 0x8Du, 0xF9u, 0xFFu, 0x4Cu, 0x00u, 0xFEu
    };
    static const uint8_t target_code[] = {
        0xA9u, 0x5Au, 0x8Du, 0x01u, 0x03u, 0x4Cu, 0x00u, 0xFEu
    };
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    const uint8_t *boot = ram_selected != 0 ? boot_with_map : boot_without_map;
    const size_t boot_len = ram_selected != 0 ? sizeof boot_with_map : sizeof boot_without_map;
    CHECK_EQ_U64(load_bll_at(m, boot, boot_len, 0x0200u), LYNX_OK);
    memcpy(&m->ram[0xFE00u], target_code, sizeof target_code);
    m->ram[0x0301u] = 0u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    if (ram_selected == 0) {
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK(consumed < 128u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
        CHECK_EQ_HEX(m->ram[0x0301u], 0u);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 128u);
        CHECK_EQ_HEX(m->ram[0x0301u], 0x5Au);
    }
    lynx_machine_deinit(m);
}

static void case_rom_unavailable(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, bll_tracer + 10u, 13u, 0x0200u), LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    uint8_t out = 0xA5u;
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFE00u, &out, 1u), LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_HEX(out, 0xA5u);
    CHECK_UNCHANGED(m, snap_a);
    uint8_t raw = 0u;
    CHECK_EQ_U64(lynx_read_memory(m, 0xFE00u, &raw, 1u), LYNX_OK);
    CHECK_EQ_HEX(raw, 0x3Cu);
    lynx_machine_deinit(m);

    /* The real CPU path refuses mapped absent-ROM reads and writes, vector
       window reads, and ROM opcode fetches before request completion. Each
       corresponding MAPCTL-selected RAM case executes and leaves a marker. */
    rom_read_request(0, 0xFE00u, 0x04u, 0x6Du);
    rom_read_request(1, 0xFE00u, 0x04u, 0x6Du);
    rom_write_request(0);
    rom_write_request(1);
    rom_read_request(0, 0xFFFAu, 0x08u, 0xC7u);
    rom_read_request(1, 0xFFFAu, 0x08u, 0xC7u);
    rom_opcode_request(0);
    rom_opcode_request(1);
}

static void case_watch_post_write(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    const lynx_status configured = lynx_watch_configure(m, 0x0400u, 1u);
    if (configured != LYNX_OK) {
        CHECK_EQ_U64(configured, LYNX_OK);
        return;
    }

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 26u);
    if (g_api_fault == API_FAULT_STOP_BEFORE_STORE) {
        /* Named local mutant: present the state a pre-store stop would expose. */
        m->ram[0x0400u] = 0u;
    }
    if (m->ram[0x0400u] != 0x5Au) {
        fprintf(stderr, "watch returned before store committed\n");
        check_failures++;
    }
    CHECK(consumed <= 26u);
    CHECK_EQ_U64(ticks_of(m), 26u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0205u); /* next LDA opcode: STA completed */
    CHECK_EQ_U64(m->cpu_ready_at, 30u);    /* in-cycle duration remains */

    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0x5Au);
    CHECK_EQ_U64(event.hit_tick, 25u);
    CHECK_EQ_U64(event.return_tick, 26u);

    /* The second store to the same address proves the first request was
       completed, the one-shot was disarmed, and resumption did not replay it. */
    CHECK_EQ_U64(lynx_advance(m, 40u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 40u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0xA6u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.value, 0x5Au);
    CHECK_EQ_U64(event.hit_tick, 25u);
    CHECK_EQ_U64(event.return_tick, 26u);
    lynx_machine_deinit(m);
}

static void case_watch_range_edges(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;

    /* The initial read and preceding $03FF store cannot trigger [0400,0402). */
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x03FFu], 0x11u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x22u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0x22u);

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x0401u], 0x33u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0401u);
    CHECK_EQ_U64(event.value, 0x33u);

    /* Exactly 30 more guest ticks include the $0402 store, outside the range. */
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 2u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0402u], 0x44u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);
}

static void case_watch_counting_store(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->ram[0x0401u], 0u);

    if (g_api_fault == API_FAULT_REPEAT_STORE) {
        /* A bounded white-box mutant rewinds to the just-completed STA before
           rearming. Correct continuation proceeds to INC $0401 instead. */
        m->cpu.pc = 0x0202u;
        m->cpu.opcode = 0u;
        m->cpu.step = 0u;
        m->cpu.ea = 0u;
        m->cpu.tmp = 0u;
        m->cpu.flags = 0u;
        lynx__cpu_repost(&m->cpu, lynx__cpu_table(m->cpu.profile));
        CHECK(lynx__cpu_state_valid(&m->cpu, lynx__cpu_table(m->cpu.profile)));
    }

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    const uint64_t continuation_budget =
        g_api_fault == API_FAULT_REPEAT_STORE ? 25u : 30u;
    CHECK_EQ_U64(lynx_advance(m, continuation_budget, &consumed, &stop), LYNX_OK);
    if (stop != LYNX_STOP_BUDGET || m->ram[0x0401u] != 1u) {
        fprintf(stderr, "watch continuation replayed first store before counter increment\n");
        check_failures++;
    }
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0401u], 1u);
    lynx_machine_deinit(m);

    /* A real store to another address changes RAM without producing a hit. */
    m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0402u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 56u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(m->ram[0x0401u], 1u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);
}

static void case_watch_last_address(const lynx_config *cfg)
{
    lynx_machine *m = init_in(machine_buf, cfg);
    if (m == NULL) {
        return;
    }
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    CHECK_EQ_U64(load_bll_at(m, watch_last_address_program,
                             sizeof watch_last_address_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->mapctl, 0x08u);
    CHECK_EQ_U64(m->ram[0xFFFFu], 0xC7u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0xFFFFu);
    CHECK_EQ_U64(event.value, 0xC7u);
    lynx_machine_deinit(m);
}

static void case_watch_controls(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_configure(NULL, 0x0400u, 1u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_clear(NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_get(NULL, &event), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(event.hit, UINT32_C(0xA5A5A5A5));
    CHECK_EQ_U64(lynx_watch_get(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    const uint64_t config_tick = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_WATCH;
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), config_tick);
    CHECK_EQ_HEX(m->ram[0x0400u], 0u);

    /* The first matching store is due exactly at target=25 and must not run. */
    CHECK_EQ_U64(lynx_advance(m, 25u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 25u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(m->ram[0x0400u], 0u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 2u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x10000u, 1u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 0u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_watch_get(m, (lynx_watch_event *)(void *)m), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);

    /* Grant 25 is inside the one-tick half-open budget: WATCH wins over BUDGET. */
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(ticks_of(m), 26u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0x5Au);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.value, 0x5Au);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFFFFu, 2u), LYNX_ERR_RANGE);
    CHECK_UNCHANGED(m, snap_a);
    const uint64_t before_rearm = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), before_rearm);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);
    CHECK_EQ_U64(lynx_advance(m, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(ticks_of(m), 56u);
    CHECK_EQ_HEX(m->ram[0x0400u], 0xA6u);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0xA6u);
    CHECK_EQ_U64(event.hit_tick, 55u);
    CHECK_EQ_U64(event.return_tick, 56u);

    const uint64_t before_clear = ticks_of(m);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), before_clear);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);

    /* Outside-range writes advance normally and leave the event empty. */
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0402u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 100u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);

    /* A due write whose mapped target is unavailable is refused before the
       watch can publish an event or expose hidden backing RAM. */
    static const uint8_t unavailable_write[] = {
        0xA9u, 0x00u, 0x8Du, 0xF9u, 0xFFu, 0xA9u, 0x96u,
        0x8Du, 0x00u, 0xFEu, 0x4Cu, 0x0Au, 0x02u
    };
    m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, unavailable_write, sizeof unavailable_write, 0x0200u),
                 LYNX_OK);
    m->ram[0xFE00u] = 0x3Cu;
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFE00u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_EQ_U64(m->cpu.req_addr, 0xFE00u);
    CHECK_EQ_HEX(m->ram[0xFE00u], 0x3Cu);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    lynx_machine_deinit(m);

    /* Successful reset and load clear host-only configuration and hit data. */
    m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);

    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(ticks_of(m), 0u);
    lynx_machine_deinit(m);
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_ERR_STATE);
    CHECK_EQ_U64(event.hit, UINT32_C(0xA5A5A5A5));

    case_watch_range_edges(&cfg);
    case_watch_counting_store(&cfg);
    case_watch_last_address(&cfg);
}

static void case_watch_independent(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *a = init_in(machine_buf, &cfg);
    lynx_machine *b = init_in(buf_b, &cfg);
    if (a == NULL || b == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(a, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(load_bll_at(b, watch_range_program, sizeof watch_range_program, 0x0200u),
                 LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(a, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(b, 0x0401u, 1u), LYNX_OK);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event_a;
    lynx_watch_event event_b;

    CHECK_EQ_U64(lynx_advance(a, 80u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_advance(b, 90u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_watch_get(b, &event_b), LYNX_OK);
    CHECK_EQ_U64(event_b.hit, 0u);
    CHECK_EQ_U64(lynx_advance(a, 10u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_advance(b, 20u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(lynx_watch_get(a, &event_a), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_get(b, &event_b), LYNX_OK);
    CHECK_EQ_U64(event_a.address, 0x0400u);
    CHECK_EQ_U64(event_a.value, 0x22u);
    CHECK_EQ_U64(event_b.address, 0x0401u);
    CHECK_EQ_U64(event_b.value, 0x33u);
    CHECK_EQ_U64(lynx_watch_get(a, &event_a), LYNX_OK);
    CHECK_EQ_U64(event_a.address, 0x0400u);
    CHECK_EQ_U64(event_a.value, 0x22u);
    lynx_machine_deinit(a);
    lynx_machine_deinit(b);
}

/* CPU_STATE as documented in the public header. */
static uint32_t cpu_state_word(uint32_t flags, uint32_t step, uint32_t opcode, uint32_t delta)
{
    return flags | (step << 8) | (opcode << 16) | (delta << 24);
}

static unsigned char sweep_ref[sizeof(struct lynx_machine)];

/* Loads image at addr into an initialised machine (a fresh power-on state). */
static void load_at(lynx_machine *m, const uint8_t *image, size_t len, uint32_t addr)
{
    CHECK_EQ_U64(load_bll_at(m, image, len, addr), LYNX_OK);
}

static void case_advance_budget(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf; /* white-box pokes below */

    /* Before any load: STATE, outputs zeroed, buffer untouched. */
    uint64_t consumed = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_ERR_STATE);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);

    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 4096u, NULL, &stop), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_advance(NULL, 4096u, &consumed, &stop), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A budget of 0 consumes nothing and changes nothing. */
    ADVANCE_EXPECT(m, 0u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), 0u);

    /* advance(1) performs one cycle: the opcode fetch of LDX #$00 granted at
       tick 0. PC is past the opcode, the operand read (cycle 1) is pending,
       and the 5-tick cycle has 4 ticks left. */
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), 1u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 1u, 0xA2u, 4u));
    /* The rest of that cycle's duration starts nothing new. */
    ADVANCE_EXPECT(m, 4u, 4u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 1u, 0xA2u, 0u));
    /* Half-open: the operand cycle is due exactly at tick 5, the target of a
       0-tick budget, so it is not started. */
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, 0u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* One more tick grants it: LDX #$00 completes at a boundary. */
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0202u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 0u, 0u, 4u));

    /* consumed equals the budget for every budget 1..600, and any two-way split
       of 600 ticks gives the same bytes as one call; so do 600 calls of one. */
    lynx_machine *ref = init_in(buf_b, &cfg);
    if (ref == NULL) {
        return;
    }
    load_at(ref, churn_loop, sizeof churn_loop, 0x0200u);
    ADVANCE_EXPECT(ref, 600u, 600u, LYNX_STOP_BUDGET);
    memcpy(sweep_ref, buf_b, MSIZE);
    for (uint64_t b = 1u; b <= 600u; b++) {
        load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
        ADVANCE_EXPECT(m, b, b, LYNX_STOP_BUDGET);
        ADVANCE_EXPECT(m, 600u - b, 600u - b, LYNX_STOP_BUDGET);
        if (memcmp(machine_buf, sweep_ref, MSIZE) != 0) {
            fprintf(stderr, "%s:%d: split %llu + %llu differs from one 600-tick call\n", __FILE__,
                    __LINE__, (unsigned long long)b, (unsigned long long)(600u - b));
            check_failures++;
        }
    }
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    for (uint32_t i = 0u; i < 600u; i++) {
        ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    }
    CHECK_UNCHANGED(machine_buf, sweep_ref);

    /* The trap image with the largest budget: UNDEFINED before the first
       opcode, nothing consumed or changed, the same again on a repeat. */
    load_at(m, trap_image, sizeof trap_image, 0x0200u);
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), 0u);
    /* NOP then the trap: one 2-cycle instruction (10 ticks), then UNDEFINED. */
    static const uint8_t nop_trap[2] = {0xEAu, 0x07u};
    load_at(m, nop_trap, sizeof nop_trap, 0x0200u);
    ADVANCE_EXPECT(m, UINT64_MAX, 10u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0201u);
    CHECK_EQ_U64(ticks_of(m), 10u);

    /* White-box CPU boundary construction only: select RAM for $FFFA-$FFFF
       and seed the JMP high operand at $0000. FFF8/FFF9 keep their fixed routes. */
    load_at(m, wrap_image, sizeof wrap_image, 0xFFFAu);
    m->mapctl = 0x08u;
    m->ram[0x0000u] = 0xFFu;
    ADVANCE_EXPECT(m, 44u, 44u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu);
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFFu); /* half-open budget, operand not granted */
    ADVANCE_EXPECT(m, 5u, 5u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0000u); /* low operand fetch wrapped PC */
    ADVANCE_EXPECT(m, 5u, 5u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFAu); /* high operand fetched after wrap */
    for (uint32_t pass = 2u; pass <= 5u; pass++) {
        ADVANCE_EXPECT(m, 55u, 55u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0xFFFAu);
        CHECK_EQ_U64(ticks_of(m), 55u * pass);
    }

    /* Guest time near the 64-bit limit (white-box): with less than the headroom
       left nothing runs, nothing is consumed and the clock never wraps. */
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    w->guest_ticks = UINT64_MAX - 5u;
    w->cpu_ready_at = UINT64_MAX - 5u;
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_BUDGET);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - 5u);
    /* Seven ticks of room below the headroom: the clamp gives exactly those 7
       (cycles at +0 and +5), then nothing. */
    load_at(m, churn_loop, sizeof churn_loop, 0x0200u);
    w->guest_ticks = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM - 7u;
    w->cpu_ready_at = w->guest_ticks;
    ADVANCE_EXPECT(m, UINT64_MAX, 7u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(ticks_of(m), UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0202u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), cpu_state_word(0u, 0u, 0u, 3u));
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_BUDGET);
}

/* ---- reset and power-on state (API-04) ---- */

/* Independent SplitMix64 (Vigna, public domain), written here from the
   published algorithm so the test does not borrow the core's code. */
static uint64_t ref_splitmix64(uint64_t *state)
{
    *state += 0x9e3779b97f4a7c15u;
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

static void read_ram(const lynx_machine *m, unsigned char *dst)
{
    CHECK_EQ_U64(lynx_read_memory(m, 0u, dst, LYNX_ADDRESS_SPACE), LYNX_OK);
}

/* The declared power-on CPU values (unverified for the Lynx). */
static void check_power_on_regs(const lynx_machine *m, int line)
{
    static const uint32_t want[LYNX_REG_COUNT] = {0x0000u, 0x00u, 0x00u, 0x00u,
                                                  0xFDu,   0x34u, 0u,    0u};
    for (uint32_t r = 0u; r < LYNX_REG_COUNT; r++) {
        uint32_t v = 0xA5A5A5A5u;
        lynx_status st = lynx_read_register(m, r, &v);
        if (st != LYNX_OK || v != want[r]) {
            fprintf(stderr, "%s:%d: power-on register %u = 0x%x (status %u), want 0x%x\n", __FILE__,
                    line, (unsigned)r, (unsigned)v, (unsigned)st, (unsigned)want[r]);
            check_failures++;
        }
    }
}

static void case_reset_power_on(void)
{
    /* The default path is fixed 0x00. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    CHECK_EQ_U64(cfg.ram_policy, LYNX_RAM_POLICY_ZERO);
    CHECK_EQ_U64(cfg.ram_seed, 0u);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    read_ram(m, mem_out);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0x00));
    check_power_on_regs(m, __LINE__);
    lynx_identity id;
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_policy, 0u);
    CHECK_EQ_U64(id.ram_seed, 0u);
    CHECK_EQ_U64(id.has_content, 0u);

    /* Reset after load and advance equals a fresh init, byte for byte. */
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    /* The load starts the CPU at the load address with the power-on registers. */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0200u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_S), 0xFDu);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_P), 0x34u);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0); /* control: the buffer did move */
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    read_ram(m, mem_out);
    CHECK(all_bytes_equal(mem_out, sizeof mem_out, 0x00));
    CHECK_EQ_U64(ticks_of(m), 0u);
    check_power_on_regs(m, __LINE__);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.has_content, 0u);
    /* After a reset there is no content, so advance refuses until a reload. */
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* An UNDEFINED stop is cleared by reset like any other state. */
    CHECK_EQ_U64(load_bll_at(m, trap_image, sizeof trap_image, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 4096u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_ERR_STATE);

    /* The cost model (internal seam) returns to flat5 on reset. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    memcpy(snap_b, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_MODEL_COUNT), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 0xFFFFFFFFu), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(NULL, LYNX__COST_TEST_SKEW), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH) >> 24, LYNX__COST_TEST_SKEW);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH), 0u);

    /* Bad magic and NULL: a status and no change. */
    CHECK_EQ_U64(lynx_reset(NULL), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_reset(m), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* Reset keeps the profile: a lynx2 instance resets to a fresh lynx2 init. */
    lynx_config l2;
    lynx_config_default(&l2);
    l2.profile = LYNX_PROFILE_LYNX2;
    m = init_in(machine_buf, &l2);
    if (m == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    check_power_on_regs(m, __LINE__);
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.profile, LYNX_PROFILE_LYNX2);

    /* SplitMix64 policy with a seed. */
    const uint64_t seed = 0x0123456789abcdefu;
    lynx_config sm;
    lynx_config_default(&sm);
    sm.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    sm.ram_seed = seed;
    m = init_in(machine_buf, &sm);
    lynx_machine *m2 = init_in(buf_b, &sm);
    if (m == NULL || m2 == NULL) {
        return;
    }
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0); /* two inits, one config */
    memcpy(snap_a, machine_buf, MSIZE);

    /* The first 8 bytes are the first output, little-endian; the next 8 the second. */
    read_ram(m, mem_out);
    uint64_t state = seed;
    for (uint32_t block = 0u; block < 4u; block++) {
        uint64_t want = ref_splitmix64(&state);
        for (uint32_t k = 0u; k < 8u; k++) {
            CHECK_EQ_HEX(mem_out[block * 8u + k], (want >> (8u * k)) & 0xFFu);
        }
    }
    /* Not all zero: the policy really fills. */
    CHECK(!all_bytes_equal(mem_out, sizeof mem_out, 0x00));

    /* Reset after load and advance equals the fresh init; identity reports policy and seed. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 4096u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(machine_buf, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
    CHECK_EQ_U64(id.ram_policy, LYNX_RAM_POLICY_SPLITMIX64);
    CHECK_EQ_U64(id.ram_seed, seed);
    CHECK_EQ_U64(id.has_content, 0u);

    /* A different seed gives different RAM. */
    sm.ram_seed = seed + 1u;
    lynx_machine *m3 = init_in(buf_b, &sm);
    if (m3 == NULL) {
        return;
    }
    read_ram(m, mem_out);
    static unsigned char ram_other[LYNX_ADDRESS_SPACE];
    read_ram(m3, ram_other);
    CHECK(memcmp(mem_out, ram_other, LYNX_ADDRESS_SPACE) != 0);

    /* The reset instance reports guest time 0. */
    CHECK_EQ_U64(ticks_of(m), 0u);
}

/* ---- snapshots (API-07) ---- */

#define SNAP_TOTAL ((size_t)LYNX__SNAPSHOT_HEADER_LEN + (size_t)LYNX__SNAPSHOT_PAYLOAD_LEN)
#define SNAP_OFF_PAYLOAD_SHA 220u
#define SNAP_SEED 0x0123456789abcdefu

/* Payload offsets of format 6 (white-box, from core/snapshot.c). */
enum {
    PAY_PC = 0, PAY_P = 6, PAY_FLAGS = 7, PAY_OPCODE = 8, PAY_STEP = 9, PAY_EA = 10, PAY_TMP = 12,
    PAY_COST_MODEL = 13, PAY_MAPCTL = 14, PAY_PAGE_LATCH = 15, PAY_TICKS = 16, PAY_READY_AT = 24,
    PAY_TIMERS = 32 + (int)LYNX_ADDRESS_SPACE,
    PAY_TIMER_PENDING = PAY_TIMERS + (int)(LYNX__TIMER_COUNT * 4u),
    PAY_TIMER_PHASE = PAY_TIMER_PENDING + 1,
    PAY_TIMER_EVENT_APPLIED = PAY_TIMER_PHASE + 1,
    PAY_BUS_HOLD = PAY_TIMER_EVENT_APPLIED + 1,
    PAY_VIDEO = PAY_BUS_HOLD + 1,
    PAY_AUDIO = PAY_VIDEO + (int)LYNX__VIDEO_SNAPSHOT_LEN
};

enum {
    VIDEO_DISPCTL = 0,
    VIDEO_FRAME_SYNCED = 2,
    VIDEO_LINE_ORDINAL = 3,
    VIDEO_ROW_ACTIVE = 4,
    VIDEO_CAPTURE_ROWS = 5,
    VIDEO_DMA_CURSOR = 6,
    VIDEO_CAPTURE_PARTIAL = 7,
    VIDEO_INPUT_MASK = 12,
    VIDEO_LEFTHAND = 14,
    VIDEO_DMA_NEXT = 16,
    VIDEO_DMA_BUSY = 24,
    VIDEO_REFRESH_NEXT = 32,
    VIDEO_REFRESH_BUSY = 40,
    VIDEO_FRAME_START = 48,
    VIDEO_FRAME_SEQUENCE = 64,
    VIDEO_PALETTE = 72,
    VIDEO_PENS = VIDEO_PALETTE + 32,
    VIDEO_ROW_PALETTES = VIDEO_PENS + (int)LYNX__VIDEO_CAPTURE_PENS,
    VIDEO_LINE_TICKS = VIDEO_ROW_PALETTES + (int)LYNX__VIDEO_CAPTURE_PALETTES
};

/* Blobs carry one extra byte of room for the trailing-byte case. */
static unsigned char blob_v[SNAP_TOTAL + 1];
static unsigned char blob_bad[SNAP_TOTAL + 1];
static unsigned char blob_c[SNAP_TOTAL + 1];
static unsigned char at_top[sizeof(struct lynx_machine)];

static void inject_watch_digest(char digest_hex[65], const lynx_watch_event *event)
{
    char material[160];
    const int length = snprintf(material, sizeof material, "%s:%u:%u:%u:%llu:%llu",
                                digest_hex, event->hit, event->address, event->value,
                                (unsigned long long)event->hit_tick,
                                (unsigned long long)event->return_tick);
    if (length > 0 && (size_t)length < sizeof material) {
        lynx__sha256 sha;
        uint8_t bytes[32];
        lynx__sha256_init(&sha);
        lynx__sha256_update(&sha, material, (size_t)length);
        lynx__sha256_final(&sha, bytes);
        lynx__sha256_hex(bytes, digest_hex);
    } else {
        CHECK(0 && "watch digest mutant input must fit");
    }
}

/* Saves m into dst and checks the documented postconditions. Returns 0 on success. */
static int save_into(const lynx_machine *m, unsigned char *dst)
{
    size_t size = 0u;
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, SNAP_TOTAL);
    lynx_status st = lynx_snapshot_save(m, dst, SNAP_TOTAL, &written);
    CHECK_EQ_U64(st, LYNX_OK);
    CHECK_EQ_U64(written, SNAP_TOTAL);
    return st == LYNX_OK ? 0 : 1;
}

/* An instance with the cases' shared identity: the given profile, SplitMix64
   and the given seed, the image loaded at addr. Returns NULL after a failed
   check. */
static lynx_machine *snap_instance_p(void *mem, uint32_t profile, uint64_t seed,
                                     const unsigned char *image, size_t len, uint32_t addr)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = seed;
    lynx_machine *m = init_in(mem, &cfg);
    if (m == NULL) {
        return NULL;
    }
    CHECK_EQ_U64(load_bll_at(m, image, len, addr), LYNX_OK);
    return m;
}

static lynx_machine *snap_instance(void *mem, uint64_t seed, const unsigned char *image,
                                   size_t len, uint32_t addr)
{
    return snap_instance_p(mem, LYNX_PROFILE_NONE, seed, image, len, addr);
}

static void advance_ticks(lynx_machine *m, uint64_t ticks)
{
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, ticks, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, ticks);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
}

static void put_le(unsigned char *p, uint64_t v, size_t n)
{
    for (size_t i = 0u; i < n; i++) {
        p[i] = (unsigned char)(v >> (8u * i));
    }
}

static uint64_t get_le(const unsigned char *p, size_t n)
{
    uint64_t v = 0u;
    for (size_t i = 0u; i < n; i++) {
        v |= (uint64_t)p[i] << (8u * i);
    }
    return v;
}

/* Every snapshot field set to a non-default value, restored into a fresh
   instance: the whole buffer must match, and so must the continuation. */
static void snapshot_coverage(void)
{
    lynx_machine *m = snap_instance_p(machine_buf, LYNX_PROFILE_LYNX2, SNAP_SEED, churn_loop,
                                      sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf;
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    /* Run, one tick at a time, to a point inside an instruction and inside a
       cycle's duration. */
    advance_ticks(m, 1000u);
    for (uint32_t i = 0u; i < 1000u && !(w->cpu.step != 0u && w->cpu_ready_at > w->guest_ticks);
         i++) {
        advance_ticks(m, 1u);
    }
    /* White-box pokes that keep the state one lynx__cpu_state_valid accepts;
       the pending request is reposted, as a restore does. */
    w->cpu.a = 0x5Au;
    w->cpu.x = 0xC3u;
    w->cpu.y = 0x3Cu;
    w->cpu.s = 0x80u;
    w->cpu.p = 0xF7u;
    w->cpu.flags = (uint8_t)LYNX__CS_NMI_PREV;
    w->cpu.ea = 0xBEEFu;
    w->cpu.tmp = 0xA7u;
    const lynx__cpu_decode *table = lynx__cpu_table(w->cpu.profile);
    lynx__cpu_repost(&w->cpu, table);
    CHECK(lynx__cpu_state_valid(&w->cpu, table));
    /* Every field differs from its power-on or boundary default. */
    CHECK(w->cpu.pc != 0x0200u);
    CHECK(w->cpu.opcode != 0u);
    CHECK(w->cpu.step != 0u);
    CHECK(w->cost_model != LYNX__COST_FLAT5);
    CHECK(w->guest_ticks != 0u);
    CHECK(w->cpu_ready_at > w->guest_ticks);
    uint32_t latch = reg_of(m, LYNX_REG_CPU_LATCH);
    CHECK_EQ_HEX(latch, 0xBEEFu | (0xA7u << 16) | ((uint32_t)LYNX__COST_TEST_SKEW << 24));
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) & 0xFFu, LYNX__CS_NMI_PREV);

    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    lynx_machine *m2 = snap_instance_p(buf_b, LYNX_PROFILE_LYNX2, SNAP_SEED, churn_loop,
                                       sizeof churn_loop, 0x0200u);
    if (m2 == NULL) {
        return;
    }
    /* The cost model is identity: a flat5 instance refuses the blob. */
    memcpy(snap_b, buf_b, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_ERR_SNAPSHOT_INVALID);
    CHECK_UNCHANGED(buf_b, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m2, LYNX__COST_TEST_SKEW), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    advance_ticks(m, 3001u);
    advance_ticks(m2, 3001u);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
}

static void case_snapshot_continue(void)
{
    lynx_machine *watched =
        snap_instance(machine_buf, SNAP_SEED, watch_program, sizeof watch_program, 0x0200u);
    lynx_machine *plain =
        snap_instance(buf_b, SNAP_SEED, watch_program, sizeof watch_program, 0x0200u);
    if (watched == NULL || plain == NULL) {
        return;
    }

    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(save_into(watched, blob_v), 0u);
    CHECK_EQ_U64(save_into(plain, blob_c), 0u);
    CHECK(memcmp(blob_v, blob_c, SNAP_TOTAL) == 0); /* armed configuration is host-only */
    CHECK_EQ_U64(lynx_advance(watched, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 26u);
    CHECK_EQ_U64(lynx_advance(plain, 26u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 26u);
    CHECK_EQ_U64(ticks_of(watched), 26u);
    CHECK_EQ_U64(ticks_of(plain), 26u);

    /* The runner's observation digest describes the same guest path and final
       tick; watch state is host-only and is deliberately absent. */
    const bundle_run final_run = {NULL, 26u, 26u, LYNX_STOP_BUDGET};
    char watched_digest[65] = {0};
    char plain_digest[65] = {0};
    CHECK_EQ_U64(bundle_observe(watched, &final_run, watched_digest), 0u);
    CHECK_EQ_U64(bundle_observe(plain, &final_run, plain_digest), 0u);
    if (g_api_fault == API_FAULT_WATCH_IN_DIGEST) {
        lynx_watch_event hit;
        CHECK_EQ_U64(lynx_watch_get(watched, &hit), LYNX_OK);
        inject_watch_digest(watched_digest, &hit);
    }
    if (strcmp(watched_digest, plain_digest) != 0) {
        fprintf(stderr, "host watch state leaked into bundle observation digest\n");
        check_failures++;
    }

    /* A hit is host observation only: the two guest states serialize alike. */
    CHECK_EQ_U64(save_into(watched, blob_v), 0u);
    CHECK_EQ_U64(save_into(plain, blob_c), 0u);
    CHECK(memcmp(blob_v, blob_c, SNAP_TOTAL) == 0);

    /* Restoring a matching guest state clears both a prior hit and a newly
       armed watch, while preserving the established post-write tick contract. */
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_c, SNAP_TOTAL), LYNX_OK);
    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(watched, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    CHECK_EQ_U64(event.address, 0u);
    CHECK_EQ_U64(event.value, 0u);
    CHECK_EQ_U64(event.hit_tick, 0u);
    CHECK_EQ_U64(event.return_tick, 0u);

    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(watched, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(ticks_of(watched), 56u);
    CHECK_EQ_HEX(watched->ram[0x0400u], 0xA6u);

    /* A refusal leaves the active observer configuration byte-for-byte intact. */
    CHECK_EQ_U64(lynx_watch_configure(watched, 0x0410u, 1u), LYNX_OK);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    blob_bad[0] ^= 1u;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(watched, blob_bad, SNAP_TOTAL),
                 LYNX_ERR_SNAPSHOT_INVALID);
    CHECK_UNCHANGED(machine_buf, snap_a);
}

static void compare_snapshot_guest(const lynx_machine *reference,
                                   const lynx_machine *candidate)
{
    static uint8_t reference_ram[LYNX_ADDRESS_SPACE];
    static uint8_t candidate_ram[LYNX_ADDRESS_SPACE];
    uint32_t reference_regs[LYNX_REG_COUNT];
    uint32_t candidate_regs[LYNX_REG_COUNT];
    uint8_t reference_mapped[6];
    uint8_t candidate_mapped[6];
    uint64_t reference_ticks = 0u;
    uint64_t candidate_ticks = 0u;

    CHECK_EQ_U64(lynx_guest_ticks(reference, &reference_ticks), LYNX_OK);
    CHECK_EQ_U64(lynx_guest_ticks(candidate, &candidate_ticks), LYNX_OK);
    CHECK_EQ_U64(reference_ticks, candidate_ticks);
    CHECK_EQ_U64(lynx_read_memory(reference, 0u, reference_ram, sizeof reference_ram), LYNX_OK);
    CHECK_EQ_U64(lynx_read_memory(candidate, 0u, candidate_ram, sizeof candidate_ram), LYNX_OK);
    CHECK(memcmp(reference_ram, candidate_ram, sizeof reference_ram) == 0);
    CHECK_EQ_U64(lynx_read_cpu_memory(reference, 0xFD00u, reference_mapped,
                                      sizeof reference_mapped), LYNX_OK);
    CHECK_EQ_U64(lynx_read_cpu_memory(candidate, 0xFD00u, candidate_mapped,
                                      sizeof candidate_mapped), LYNX_OK);
    CHECK(memcmp(reference_mapped, candidate_mapped, sizeof reference_mapped) == 0);
    for (uint32_t reg = 0u; reg < LYNX_REG_COUNT; reg++) {
        CHECK_EQ_U64(lynx_read_register(reference, reg, &reference_regs[reg]), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(candidate, reg, &candidate_regs[reg]), LYNX_OK);
    }
    CHECK(memcmp(reference_regs, candidate_regs, sizeof reference_regs) == 0);
    CHECK_EQ_U64(reference->cpu.req_addr, candidate->cpu.req_addr);
    CHECK_EQ_U64(reference->cpu.req_kind, candidate->cpu.req_kind);
    CHECK_EQ_U64(reference->cpu.req_flags, candidate->cpu.req_flags);
    CHECK_EQ_U64(reference->cpu.req_data, candidate->cpu.req_data);
}

static void check_snapshot_cursor_refusal(uint32_t profile)
{
    lynx_machine *source = snap_instance_p(machine_buf, profile, SNAP_SEED,
                                           timer_cursor_refusal_program,
                                           sizeof timer_cursor_refusal_program, 0x0200u);
    lynx_machine *restored = snap_instance_p(buf_b, profile, SNAP_SEED,
                                             timer_cursor_refusal_program,
                                             sizeof timer_cursor_refusal_program, 0x0200u);
    if (source == NULL || restored == NULL) {
        return;
    }
    source->ram[0xFD91u] = 0xE7u;
    restored->ram[0xFD91u] = 0xE7u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(source, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_EQ_U64(consumed, 240u);
    CHECK_EQ_U64(source->guest_ticks, 240u);
    CHECK_EQ_U64(source->cpu.req_addr, 0xFD91u);
    CHECK_EQ_U64(source->timer_bank.pending, 1u);
    CHECK_EQ_U64(source->timer_bank.last_event_valid, 1u);
    CHECK_EQ_U64(source->timer_bank.last_event_tick, source->guest_ticks);
    CHECK_EQ_U64(source->timer_bank.timers[0].count, 1u);
    CHECK_EQ_U64(save_into(source, blob_v), 0u);
    CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK(memcmp(source, restored, MSIZE) == 0);

    /* Retrying the refused request must not dispatch the same timer edge a
       second time after restore. */
    for (size_t i = 0u; i < 2u; i++) {
        lynx_machine *machine = i == 0u ? source : restored;
        CHECK_EQ_U64(lynx_advance(machine, 1000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK_EQ_U64(consumed, 0u);
    }
    CHECK(memcmp(source, restored, MSIZE) == 0);
}

static void case_timer_snapshot(void)
{
    static const uint32_t profiles[] = {LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    const int failures_before = check_failures;

    for (size_t profile_index = 0u;
         profile_index < sizeof profiles / sizeof profiles[0]; profile_index++) {
        const uint32_t profile = profiles[profile_index];
        lynx_machine *source = snap_instance_p(machine_buf, profile, SNAP_SEED,
                                               timer_snapshot_program,
                                               sizeof timer_snapshot_program, 0x0200u);
        lynx_machine *restored = snap_instance_p(buf_b, profile, SNAP_SEED,
                                                 timer_snapshot_program,
                                                 sizeof timer_snapshot_program, 0x0200u);
        if (source == NULL || restored == NULL) {
            return;
        }

        /* Raw RAM at a timer address remains independent of the mapped bank. */
        source->ram[0xFD00u] = 0xA5u;
        restored->ram[0xFD00u] = 0xA5u;
        CHECK_EQ_U64(lynx_watch_configure(source, 0xFD01u, 1u), LYNX_OK);
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_advance(source, 1000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK(consumed != 0u && consumed < 1000u);
        CHECK_EQ_U64(source->timer_bank.timers[0].control_a, 0x98u);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 1u), LYNX_OK);

        /* Keep the posted request held until a real timer edge latches IRQ.
           Stop one tick after the edge, before the next divider edge. */
        for (uint32_t i = 0u; i < 256u && source->timer_bank.pending == 0u; i++) {
            CHECK_EQ_U64(lynx_advance(source, 1u, &consumed, &stop), LYNX_OK);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
            CHECK_EQ_U64(consumed, 1u);
        }
        CHECK(source->timer_bank.pending != 0u);
        CHECK_EQ_U64(source->wake_pending, 1u);
        CHECK_EQ_U64(source->bus_hold, 1u);
        CHECK(source->timer_bank.timers[0].count != 0u);
        CHECK_EQ_U64(source->ram[0xFD00u], 0xA5u);
        uint8_t raw_timer = 0u;
        uint8_t mapped_timer = 0u;
        CHECK_EQ_U64(lynx_read_memory(source, 0xFD00u, &raw_timer, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_read_cpu_memory(source, 0xFD00u, &mapped_timer, 1u), LYNX_OK);
        CHECK_EQ_U64(raw_timer, 0xA5u);
        CHECK_EQ_U64(mapped_timer, 1u);

        /* A successful restore clears its destination's host watch, while
           preserving the guest timer, pending IRQ, hold and request. */
        CHECK_EQ_U64(lynx_watch_configure(source, 0x0400u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_watch_configure(restored, 0x0400u, 1u), LYNX_OK);
        CHECK_EQ_U64(save_into(source, blob_v), 0u);
        memcpy(blob_bad, blob_v, SNAP_TOTAL);
        blob_bad[0] ^= 1u;
        memcpy(snap_b, restored, MSIZE);
        CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_bad, SNAP_TOTAL),
                     LYNX_ERR_SNAPSHOT_INVALID);
        CHECK_UNCHANGED(buf_b, snap_b);
        CHECK_EQ_U64(restored->host_observation.watch_armed, 1u);
        CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_v, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_U64(restored->timer_bank.pending, source->timer_bank.pending);
        CHECK_EQ_U64(restored->timer_bank.timers[0].count,
                     source->timer_bank.timers[0].count);
        CHECK_EQ_U64(restored->timer_bank.last_event_valid,
                     source->timer_bank.last_event_valid);
        CHECK_EQ_U64(restored->timer_bank.last_event_tick,
                     source->timer_bank.last_event_tick);
        CHECK_EQ_U64(restored->wake_pending, source->wake_pending);
        CHECK_EQ_U64(restored->bus_hold, source->bus_hold);
        CHECK_EQ_U64(restored->host_observation.watch_armed, 0u);
        compare_snapshot_guest(source, restored);

        /* The common held prefix includes the next event boundary and its
           following tick, which consumes the event exactly once. */
        const uint64_t next_edge = (source->guest_ticks / 16u + 1u) * 16u;
        advance_ticks(source, next_edge - source->guest_ticks);
        advance_ticks(restored, next_edge - restored->guest_ticks);
        CHECK_EQ_U64(source->timer_bank.pending, restored->timer_bank.pending);
        CHECK_EQ_U64(source->timer_bank.timers[0].count,
                     restored->timer_bank.timers[0].count);
        advance_ticks(source, 1u);
        advance_ticks(restored, 1u);
        CHECK_EQ_U64(source->timer_bank.pending, restored->timer_bank.pending);
        CHECK_EQ_U64(source->timer_bank.timers[0].count,
                     restored->timer_bank.timers[0].count);
        compare_snapshot_guest(source, restored);

        CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 0u), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(restored, 0u), LYNX_OK);
        advance_ticks(source, 32u);
        advance_ticks(restored, 32u);
        compare_snapshot_guest(source, restored);
        CHECK_EQ_U64(save_into(source, blob_v), 0u);
        CHECK_EQ_U64(save_into(restored, blob_c), 0u);
        CHECK(memcmp(blob_v, blob_c, SNAP_TOTAL) == 0);

        check_snapshot_cursor_refusal(profile);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.timer_snapshot\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_av_lattice(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) return;
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);

    uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_pcm_frame pcm_first[2], pcm_second[2];
    lynx_audio_event events_first[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_audio_event events_second[LYNX_AV_MIN_EVENT_CAPACITY];
    lynx_av_buffers buffers = {0};
    lynx_av_result result = {0}, result_before;
    uint64_t consumed = 0u, ticks = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    memset(pens, 0xA5, sizeof pens); memset(palettes, 0xA5, sizeof palettes);
    memset(line_ticks, 0xA5, sizeof line_ticks);
    memset(pcm_first, 0xA5, sizeof pcm_first); memset(pcm_second, 0xA5, sizeof pcm_second);
    memset(events_first, 0xA5, sizeof events_first); memset(events_second, 0xA5, sizeof events_second);
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.frame.struct_size = (uint32_t)sizeof buffers.frame;
    buffers.frame.pens = pens; buffers.frame.pens_capacity = sizeof pens;
    buffers.frame.palettes = palettes; buffers.frame.palettes_capacity = sizeof palettes;
    buffers.frame.line_ticks = line_ticks; buffers.frame.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
    buffers.pcm = pcm_first; buffers.pcm_capacity = 2u;
    buffers.events = events_first; buffers.event_capacity = LYNX_AV_MIN_EVENT_CAPACITY;
    result.struct_size = (uint32_t)sizeof result;

    memcpy(snap_a, machine_buf, MSIZE);
    result.pcm_count = 0xA5A5A5A5u;
    result_before = result;
    buffers.pcm_capacity = 0u;
    CHECK_EQ_U64(lynx_advance_av(m, 400u, &buffers, &result), LYNX_ERR_CAPACITY);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK(memcmp(&result, &result_before, sizeof result) == 0);
    CHECK(all_bytes_equal(pens, sizeof pens, 0xA5u));
    CHECK(all_bytes_equal(palettes, sizeof palettes, 0xA5u));
    CHECK(all_bytes_equal((const uint8_t *)line_ticks, sizeof line_ticks, 0xA5u));
    CHECK(all_bytes_equal((const uint8_t *)pcm_first, sizeof pcm_first, 0xA5u));
    buffers.pcm_capacity = 2u;

    /* Tick 400 is excluded from [0,400); the next call observes ordinal 0. */
    result.struct_size = (uint32_t)sizeof result;
    CHECK_EQ_U64(lynx_advance_av(m, 400u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(result.ticks_consumed, 400u);
    CHECK_EQ_U64(result.first_sample_ordinal, 0u);
    CHECK_EQ_U64(result.next_sample_ordinal, 0u);
    CHECK_EQ_U64(result.pcm_count, 0u);
    CHECK_EQ_U64(result.event_count, 0u);
    CHECK(all_bytes_equal(pens, sizeof pens, 0xA5u));
    CHECK_EQ_U64(lynx_advance_av(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.first_sample_ordinal, 0u);
    CHECK_EQ_U64(result.next_sample_ordinal, 1u);
    CHECK_EQ_U64(result.pcm_count, 1u);
    CHECK_EQ_U64(pcm_first[0].left, 0u);
    CHECK_EQ_U64(pcm_first[0].right, 0u);

    /* Repointing to new caller spans writes the next ordinal only there. */
    buffers.pcm = pcm_second; buffers.events = events_second;
    CHECK_EQ_U64(lynx_advance_av(m, 399u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.first_sample_ordinal, 1u);
    CHECK_EQ_U64(result.next_sample_ordinal, 1u);
    CHECK_EQ_U64(result.pcm_count, 0u);
    CHECK_EQ_U64(lynx_advance_av(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.first_sample_ordinal, 1u);
    CHECK_EQ_U64(result.next_sample_ordinal, 2u);
    CHECK_EQ_U64(result.pcm_count, 1u);
    CHECK_EQ_U64(pcm_second[0].left, 0u);
    CHECK_EQ_U64(pcm_second[0].right, 0u);
    CHECK_EQ_U64(pcm_first[0].left, 0u);
    CHECK_EQ_U64(pcm_first[0].right, 0u);

    /* Legacy advance keeps the sample phase without requesting observations. */
    CHECK_EQ_U64(lynx_advance(m, 399u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 399u);
    CHECK_EQ_U64(m->audio.next_sample_ordinal, 2u);
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, 1200u);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->audio.next_sample_ordinal, 3u);

    /* A complete-tick reservation stops before the next transition; resuming
       with a fresh span captures it once rather than losing it. */
    const uint64_t setup_tick = m->guest_ticks;
    CHECK(lynx__audio_write(&m->audio, 0xFD20u, 1u, setup_tick, NULL));
    CHECK(lynx__audio_write(&m->audio, 0xFD21u, 1u, setup_tick, NULL));
    CHECK(lynx__audio_write(&m->audio, 0xFD24u, 0u, setup_tick, NULL));
    CHECK(lynx__audio_write(&m->audio, 0xFD25u, 0x18u, setup_tick, NULL));
    memcpy(snap_a, machine_buf, MSIZE);
    {
        static const uint16_t addresses[] = {0xFD20u, 0xFD21u, 0xFD24u, 0xFD25u};
        static const uint8_t expected[] = {1u, 1u, 0u, 0x18u};
        for (size_t i = 0u; i < sizeof addresses / sizeof addresses[0]; i++) {
            uint8_t value = 0xA5u;
            CHECK_EQ_U64(lynx_read_cpu_memory(m, addresses[i], &value, 1u), LYNX_OK);
            CHECK_EQ_U64(value, expected[i]);
        }
    }
    CHECK_UNCHANGED(machine_buf, snap_a);
    buffers.pcm = pcm_first; buffers.events = events_first;
    CHECK_EQ_U64(lynx_advance_av(m, 40u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.stop, LYNX_STOP_OUTPUT);
    CHECK_EQ_U64(result.event_count, 1u);
    CHECK_EQ_U64(events_first[0].tick, 1216u);
    CHECK_EQ_U64(events_first[0].sequence, 0u);
    CHECK_EQ_U64(events_first[0].channel, 0u);
    CHECK_EQ_U64(events_first[0].cause, LYNX_AUDIO_EVENT_CLOCK);
    CHECK(events_first[0].before_output == -1);
    CHECK_EQ_U64(events_first[0].after_output, 1u);
    buffers.events = events_second;
    CHECK_EQ_U64(lynx_advance_av(m, 16u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(result.event_count, 1u);
    CHECK_EQ_U64(events_second[0].tick, 1232u);
    CHECK_EQ_U64(events_second[0].sequence, 0u);
    CHECK_EQ_U64(events_second[0].channel, 0u);
    CHECK_EQ_U64(events_second[0].cause, LYNX_AUDIO_EVENT_CLOCK);
    CHECK_EQ_U64(events_second[0].before_output, 1u);
    CHECK(events_second[0].after_output == -1);
    lynx_machine_deinit(m);
}

static void case_snapshot_roundtrip(void)
{
    case_av_lattice();
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    /* 2063 = 70 + 7 * 255 + 208: cycle 41 of the eighth iteration (the third
       cycle of PLA) was granted at 2060 and lasts until 2065, so the state is
       inside an instruction and inside a cycle. */
    advance_ticks(m, 2063u);
    uint32_t cs = reg_of(m, LYNX_REG_CPU_STATE);
    CHECK_EQ_HEX((cs >> 16) & 0xFFu, 0x68u);
    CHECK((cs >> 8) & 0xFFu);
    CHECK_EQ_HEX(cs >> 24, 2u);

    /* Size and save leave the instance byte-identical. */
    memcpy(snap_a, machine_buf, MSIZE); /* at_save */
    size_t size = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(m, &size), LYNX_OK);
    CHECK_EQ_U64(size, SNAP_TOTAL);
    CHECK_UNCHANGED(machine_buf, snap_a);
    memset(blob_v, 0xA5, sizeof blob_v);
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_v, size, &written), LYNX_OK);
    CHECK_EQ_U64(written, size);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(blob_v[SNAP_TOTAL], 0xA5u); /* nothing written past the end */
    CHECK(memcmp(blob_v, "LYNXSNAP", 8u) == 0);
    CHECK_EQ_U64(get_le(blob_v + 8u, 4u), 6u); /* format 6 */
    CHECK_EQ_U64(get_le(blob_v + 12u, 4u), LYNX__SNAPSHOT_HEADER_LEN);
    CHECK_EQ_U64(get_le(blob_v + 200u, 4u), LYNX_CONTENT_BS93);
    CHECK_EQ_U64(get_le(blob_v + 204u, 4u), LYNX_STARTUP_A);
    CHECK_EQ_U64(get_le(blob_v + 208u, 4u), 0x0200u);
    CHECK_EQ_U64(get_le(blob_v + 212u, 4u), sizeof churn_loop + 10u);
    CHECK_EQ_U64(get_le(blob_v + 216u, 4u), LYNX__SNAPSHOT_PAYLOAD_LEN);
    CHECK_EQ_U64(PAY_AUDIO, 77948);
    CHECK(all_bytes_equal(blob_v + LYNX__SNAPSHOT_HEADER_LEN + PAY_AUDIO, 40u, 0u));
    CHECK_EQ_U64(get_le(blob_v + LYNX__SNAPSHOT_HEADER_LEN + PAY_AUDIO + 66u, 8u), 2400u);
    CHECK_EQ_U64(get_le(blob_v + LYNX__SNAPSHOT_HEADER_LEN + PAY_AUDIO + 74u, 8u), 5u);

    /* Saving twice, over different garbage, gives the same bytes. */
    memset(blob_bad, 0x5A, sizeof blob_bad);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, size, &written), LYNX_OK);
    CHECK(memcmp(blob_v, blob_bad, SNAP_TOTAL) == 0);

    advance_ticks(m, 2048u);
    memcpy(snap_b, machine_buf, MSIZE); /* final */
    CHECK(memcmp(snap_a, snap_b, MSIZE) != 0);

    /* Same instance: restore gives the buffer at save time, then the same continuation. */
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, size), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    advance_ticks(m, 2048u);
    CHECK_UNCHANGED(machine_buf, snap_b);

    /* A second fresh instance, same config and content: restore and continue. */
    lynx_machine *m2 = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m2 == NULL) {
        return;
    }
    CHECK(memcmp(buf_b, snap_a, MSIZE) != 0);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, size), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_a);
    advance_ticks(m2, 2048u);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Capacity: one byte short is refused, nothing written, nothing reported. */
    memset(blob_bad, 0xA5, sizeof blob_bad);
    memcpy(snap_a, machine_buf, MSIZE);
    written = 77u;
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, size - 1u, &written), LYNX_ERR_CAPACITY);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(written, 77u);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, 0u, &written), LYNX_ERR_CAPACITY);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));

    /* Argument refusals write nothing. */
    CHECK_EQ_U64(lynx_snapshot_size(NULL, &size), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_size(m, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(NULL, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(m, NULL, SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_save(m, blob_bad, SNAP_TOTAL, NULL), LYNX_ERR_ARGUMENT);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(lynx_snapshot_restore(NULL, blob_v, SNAP_TOTAL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_snapshot_restore(m, NULL, SNAP_TOTAL), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A buffer that overlaps the instance is refused by both calls: inside it,
       reaching into its start, and reaching into its end. A refusal reads and
       writes none of it, so the odd addresses are never dereferenced. */
    unsigned char *inside = machine_buf + 16;
    unsigned char *before = (unsigned char *)(uintptr_t)((uintptr_t)machine_buf - 100u);
    unsigned char *after = machine_buf + MSIZE - 100u;
    unsigned char *overlaps[3] = {inside, before, after};
    for (size_t i = 0u; i < 3u; i++) {
        CHECK_EQ_U64(lynx_snapshot_save(m, overlaps[i], SNAP_TOTAL, &written), LYNX_ERR_ARGUMENT);
        CHECK_UNCHANGED(machine_buf, snap_a);
        CHECK_EQ_U64(lynx_snapshot_restore(m, overlaps[i], SNAP_TOTAL), LYNX_ERR_ARGUMENT);
        CHECK_UNCHANGED(machine_buf, snap_a);
    }

    /* No content loaded: save and restore are refused with STATE; size still answers. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *fresh = init_in(buf_b, &cfg);
    if (fresh == NULL) {
        return;
    }
    memcpy(snap_b, buf_b, MSIZE);
    memset(blob_bad, 0xA5, sizeof blob_bad);
    CHECK_EQ_U64(lynx_snapshot_size(fresh, &size), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_save(fresh, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_STATE);
    CHECK(all_bytes_equal(blob_bad, sizeof blob_bad, 0xA5));
    CHECK_EQ_U64(lynx_snapshot_restore(fresh, blob_v, SNAP_TOTAL), LYNX_ERR_STATE);
    CHECK_UNCHANGED(buf_b, snap_b);
    /* A deinitialised machine: STATE for all three. */
    lynx_machine_deinit(fresh);
    memcpy(snap_b, buf_b, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_size(fresh, &size), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_snapshot_save(fresh, blob_bad, SNAP_TOTAL, &written), LYNX_ERR_STATE);
    CHECK_EQ_U64(lynx_snapshot_restore(fresh, blob_v, SNAP_TOTAL), LYNX_ERR_STATE);
    CHECK_UNCHANGED(buf_b, snap_b);

    /* Top of the address space: a white-box vector-window mapping plus a raw
       $0000 high operand lets the real CPU fetch the JMP operand across wrap. */
    m = snap_instance(machine_buf, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFFAu);
    m2 = snap_instance(buf_b, SNAP_SEED, wrap_image, sizeof wrap_image, 0xFFFAu);
    if (m == NULL || m2 == NULL) {
        return;
    }
    m->mapctl = 0x08u;
    m->ram[0x0000u] = 0xFFu;
    advance_ticks(m, 50u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0000u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0000u);
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    advance_ticks(m, 300u);
    memcpy(at_top, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);
    advance_ticks(m, 300u);
    CHECK_UNCHANGED(machine_buf, at_top);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(buf_b, snap_a);
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0x0000u);
    advance_ticks(m2, 5u);
    CHECK_EQ_HEX(reg_of(m2, LYNX_REG_PC), 0xFFFAu); /* fetched through wrap and jumped */
    advance_ticks(m2, 295u);
    CHECK_UNCHANGED(buf_b, at_top);

    /* An UNDEFINED stop survives a snapshot: the restored instance stops at the
       same place with nothing consumed. */
    m = snap_instance(machine_buf, SNAP_SEED, sum_trap, sizeof sum_trap, 0x0200u);
    m2 = snap_instance(buf_b, SNAP_SEED, sum_trap, sizeof sum_trap, 0x0200u);
    if (m == NULL || m2 == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 100000u, 525u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    CHECK_EQ_U64(lynx_snapshot_restore(m2, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK(memcmp(machine_buf, buf_b, MSIZE) == 0);
    memcpy(snap_b, buf_b, MSIZE);
    ADVANCE_EXPECT(m2, 4096u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(buf_b, snap_b);

    snapshot_coverage();
}

static void case_bll_identity(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    lynx_machine *m2 = init_in(buf_b, &cfg);
    if (m2 != NULL) {
        uint8_t relocated[sizeof bll_tracer];
        lynx_identity original_id;
        lynx_identity relocated_id;
        memcpy(relocated, bll_tracer, sizeof relocated);
        relocated[3] = 0x01u; /* header-only change: same payload at $0201 */
        CHECK_EQ_U64(lynx_get_identity(m, &original_id), LYNX_OK);
        CHECK_EQ_U64(lynx_load_bll(m2, relocated, sizeof relocated), LYNX_OK);
        CHECK_EQ_U64(lynx_get_identity(m2, &relocated_id), LYNX_OK);
        CHECK_EQ_U64(relocated_id.content_addr, 0x0201u);
        CHECK_EQ_U64(relocated_id.content_len, original_id.content_len);
        CHECK(memcmp(relocated_id.content_sha256, original_id.content_sha256, 32u) != 0);
        CHECK(memcmp(m->ram + 0x0200u, m2->ram + 0x0201u, 13u) == 0);
        lynx_machine_deinit(m2);
    }
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);
    CHECK_EQ_U64(save_into(m, blob_v), 0u);

    /* Format 6 carries the complete object and timing identity in the LE header and the
       guest-written MAPCTL latch in the payload. */
    CHECK_EQ_U64(get_le(blob_v + 8u, 4u), 6u);
    CHECK_EQ_U64(get_le(blob_v + 200u, 4u), LYNX_CONTENT_BS93);
    CHECK_EQ_U64(get_le(blob_v + 204u, 4u), LYNX_STARTUP_A);
    CHECK_EQ_U64(get_le(blob_v + 208u, 4u), 0x0200u);
    CHECK_EQ_U64(get_le(blob_v + 212u, 4u), sizeof bll_tracer);
    CHECK_EQ_U64(get_le(blob_v + 216u, 4u), LYNX__SNAPSHOT_PAYLOAD_LEN);
    CHECK_EQ_U64(get_le(blob_v + 252u, 4u), LYNX_TIMING_POLICY_LEGACY_FLAT5);
    CHECK_EQ_U64(blob_v[LYNX__SNAPSHOT_HEADER_LEN + 14u], 0x0Cu);
    CHECK_EQ_U64(blob_v[LYNX__SNAPSHOT_HEADER_LEN + 15u], 0u);

    m->mapctl = 0u;
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0x0Cu);

    /* The provisional latch retains all eight guest-written bits. */
    uint8_t all_bits_bll[sizeof bll_tracer];
    memcpy(all_bits_bll, bll_tracer, sizeof all_bits_bll);
    all_bits_bll[11] = 0xFFu; /* LDA #$FF */
    CHECK_EQ_U64(lynx_load_bll(m, all_bits_bll, sizeof all_bits_bll), LYNX_OK);
    ADVANCE_EXPECT(m, 300u, 300u, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->mapctl, 0xFFu);
    CHECK_EQ_U64(save_into(m, blob_c), 0u);
    CHECK_EQ_U64(blob_c[LYNX__SNAPSHOT_HEADER_LEN + PAY_MAPCTL], 0xFFu);
    m->mapctl = 0u;
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(m->mapctl, 0xFFu);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.bll_identity\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* Recomputes the payload hash of a blob in place, so a test can corrupt a
   value and still pass the hash check. White-box on the layout. */
static void rehash_payload(unsigned char *blob)
{
    lynx__sha256 hash;
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, blob + LYNX__SNAPSHOT_HEADER_LEN, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&hash, blob + SNAP_OFF_PAYLOAD_SHA);
}

/* The status a one-bit flip at header offset i must give. */
static lynx_status header_flip_status(size_t i)
{
    if (i < 8u) {
        return LYNX_ERR_SNAPSHOT_INVALID; /* magic */
    }
    if (i < 12u) {
        return LYNX_ERR_SNAPSHOT_VERSION; /* format_version */
    }
    if (i < 16u) {
        return LYNX_ERR_SNAPSHOT_INVALID; /* header_len */
    }
    if (i < 216u) {
        return LYNX_ERR_SNAPSHOT_MISMATCH; /* build, profile, policy, seed, full content identity */
    }
    if (i >= 252u) {
        return LYNX_ERR_SNAPSHOT_MISMATCH; /* timing-policy identity */
    }
    return LYNX_ERR_SNAPSHOT_INVALID; /* payload_len, payload_sha256 */
}

static void restore_expect(lynx_machine *m, const unsigned char *blob, size_t len,
                           lynx_status want, const unsigned char *ref, int line)
{
    lynx_status st = lynx_snapshot_restore(m, blob, len);
    if (st != want) {
        fprintf(stderr, "%s:%d: snapshot restore status %llu, want %llu\n", __FILE__, line,
                (unsigned long long)st, (unsigned long long)want);
        check_failures++;
    }
    check_unchanged(m, ref, line);
}
#define RESTORE_EXPECT(m, blob, len, want, ref) \
    restore_expect((m), (blob), (len), (want), (ref), __LINE__)

/* Copies src into blob_bad, writes n little-endian bytes of v at payload
   offset off, recomputes the payload hash, and expects the refusal. */
static void value_refused(lynx_machine *m, const unsigned char *src, size_t off, uint64_t v,
                          size_t n, const unsigned char *ref, int line)
{
    memcpy(blob_bad, src, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + off, v, n);
    rehash_payload(blob_bad);
    restore_expect(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, ref, line);
}
#define VALUE_REFUSED(m, src, off, v, n, ref) value_refused((m), (src), (off), (v), (n), (ref), __LINE__)

/* Forged CPU fields in a snapshot whose payload hash is recomputed, so only the
   CPU reachability check can refuse (WR-01, WR-02): a take latch no completion
   leaves set, and a pending micro-operation real execution skips. The control
   blob is a real state of a taken BNE that crosses a page, with the
   page-cross cycle pending; it must restore. */
static void snapshot_forged_cpu_states(void)
{
    static const unsigned char bne_cross[2] = {0xD0u, 0x10u}; /* at $02FD: target $030F */
    lynx_machine *src = snap_instance(machine_buf, SNAP_SEED, bne_cross, sizeof bne_cross, 0x02FDu);
    lynx_machine *dst = snap_instance(buf_b, SNAP_SEED, bne_cross, sizeof bne_cross, 0x02FDu);
    if (src == NULL || dst == NULL) {
        return;
    }
    struct lynx_machine *w = (struct lynx_machine *)machine_buf;
    for (uint32_t i = 0u; i < 100u && w->cpu.step != 3u; i++) {
        advance_ticks(src, 1u);
    }
    CHECK_EQ_U64(w->cpu.opcode, 0xD0u);
    CHECK_EQ_U64(w->cpu.step, 3u);
    CHECK_EQ_HEX(w->cpu.ea, 0x0110u); /* offset with BR_CROSSED */
    CHECK_EQ_U64(save_into(src, blob_c), 0u);
    memcpy(snap_b, buf_b, MSIZE);
    const unsigned char *pay = blob_c + LYNX__SNAPSHOT_HEADER_LEN;
    uint64_t p = pay[PAY_P];
    CHECK((p & 0x02u) == 0u); /* Z clear: the branch is taken */
    /* WR-02: the cross cycle belongs to a taken, crossing branch only. */
    VALUE_REFUSED(dst, blob_c, PAY_P, p | 0x02u, 1u, snap_b);    /* Z set: not taken */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0010u, 2u, snap_b);     /* crossing never recorded */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0210u, 2u, snap_b);     /* BR_SAME_PAGE: cycle skipped */
    VALUE_REFUSED(dst, blob_c, PAY_EA, 0x0310u, 2u, snap_b);
    /* WR-01 on the same state. */
    VALUE_REFUSED(dst, blob_c, PAY_FLAGS, 0x01u, 1u, snap_b);
    VALUE_REFUSED(dst, blob_c, PAY_FLAGS, 0x02u, 1u, snap_b);
    /* Control: the unedited blob restores, so every refusal above was its edit. */
    CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_c, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_U64(((struct lynx_machine *)buf_b)->cpu.step, 3u);
}

/* The guest-time limit through a snapshot (WR-03): a state at the limit is
   reachable and restores with a flat5 cycle ending at most four ticks past
   it. lynx_advance then returns LYNX_OK, LYNX_STOP_BUDGET and consumed 0 (the
   documented signal for the end of guest time). One tick above the limit is
   refused, instance untouched. */
static void snapshot_tick_limit(void)
{
    const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    lynx_machine *src = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    lynx_machine *dst = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (src == NULL || dst == NULL) {
        return;
    }
    advance_ticks(src, 2063u); /* inside an instruction and inside a cycle */
    CHECK_EQ_U64(save_into(src, blob_c), 0u);
    memcpy(snap_b, buf_b, MSIZE);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit + 1u, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + 1u, 8u);
    rehash_payload(blob_bad);
    RESTORE_EXPECT(dst, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
    /* At the limit, with the maximum flat5 public-return residual. */
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + 4u, 8u);
    const uint64_t next_ordinal = limit / UINT64_C(400);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_AUDIO + 74u, next_ordinal, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_AUDIO + 66u,
           (next_ordinal + 1u) * UINT64_C(400), 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_bad, SNAP_TOTAL), LYNX_OK);
    uint64_t ticks = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(dst, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, limit);
    uint64_t consumed = 99u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(dst, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_guest_ticks(dst, &ticks), LYNX_OK);
    CHECK_EQ_U64(ticks, limit);
}

static void case_snapshot_refusal(void)
{
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m == NULL) {
        return;
    }
    advance_ticks(m, 2063u); /* inside PLA and inside a cycle (see snapshot_roundtrip) */
    CHECK_EQ_U64(save_into(m, blob_v), 0u);
    memcpy(at_top, machine_buf, MSIZE); /* the instance as saved */
    advance_ticks(m, 64u); /* the instance now differs from the blob, so a wrongly applied blob shows */
    memcpy(snap_a, machine_buf, MSIZE);

    /* Every header byte: flip bit 0, restore, expect the field's status, instance unchanged. */
    memcpy(blob_bad, blob_v, SNAP_TOTAL);
    for (size_t i = 0u; i < LYNX__SNAPSHOT_HEADER_LEN; i++) {
        blob_bad[i] ^= 1u;
        lynx_status st = lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL);
        if (st != header_flip_status(i) || st == LYNX_OK) {
            fprintf(stderr, "%s:%d: header byte %zu flip gave status %llu, want %llu\n", __FILE__,
                    __LINE__, i, (unsigned long long)st,
                    (unsigned long long)header_flip_status(i));
            check_failures++;
        }
        CHECK_UNCHANGED(machine_buf, snap_a);
        blob_bad[i] ^= 1u;
    }

    /* Payload bytes: the first, the last and every 251st. The payload hash makes
       any single change detectable, so a stride sample is enough. */
    for (size_t k = 0u; k < LYNX__SNAPSHOT_PAYLOAD_LEN; k += 251u) {
        size_t i = (size_t)LYNX__SNAPSHOT_HEADER_LEN + k;
        blob_bad[i] ^= 1u;
        RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
        blob_bad[i] ^= 1u;
    }
    blob_bad[SNAP_TOTAL - 1u] ^= 1u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    blob_bad[SNAP_TOTAL - 1u] ^= 1u;
    CHECK(memcmp(blob_bad, blob_v, SNAP_TOTAL) == 0); /* every flip was undone */

    /* Lengths: truncated, one extra byte, zero. */
    RESTORE_EXPECT(m, blob_v, SNAP_TOTAL - 1u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    blob_bad[SNAP_TOTAL] = 0u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL + 1u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    RESTORE_EXPECT(m, blob_v, 0u, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
    RESTORE_EXPECT(m, blob_v, LYNX__SNAPSHOT_HEADER_LEN, LYNX_ERR_SNAPSHOT_INVALID, snap_a);

    /* Formats 1 through 5 are unsupported; format 6 is the current positive case. */
    CHECK_EQ_U64(blob_bad[8], 6u);
    CHECK_EQ_U64(blob_bad[9] | blob_bad[10] | blob_bad[11], 0u);
    blob_bad[8] = 1u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 2u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 3u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 4u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 5u;
    RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_VERSION, snap_a);
    blob_bad[8] = 6u;

    /* Another identity: each refused as a MISMATCH, whatever the blob holds. */
    unsigned char other_image[sizeof churn_loop];
    memcpy(other_image, churn_loop, sizeof other_image);
    other_image[5] ^= 0x01u;
    lynx_config zero;
    lynx_config_default(&zero);
    for (int variant = 0; variant < 5; variant++) {
        lynx_machine *o = NULL;
        switch (variant) {
        case 0: /* different seed */
            o = snap_instance(buf_b, SNAP_SEED + 1u, churn_loop, sizeof churn_loop, 0x0200u);
            break;
        case 1: /* one different content byte */
            o = snap_instance(buf_b, SNAP_SEED, other_image, sizeof other_image, 0x0200u);
            break;
        case 2: /* different RAM policy (and so seed) */
            o = init_in(buf_b, &zero);
            if (o != NULL) {
                CHECK_EQ_U64(load_bll_at(o, churn_loop, sizeof churn_loop, 0x0200u),
                             LYNX_OK);
            }
            break;
        case 3: /* same bytes at another address */
            o = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0300u);
            break;
        default: /* same bytes, shorter */
            o = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop - 1u, 0x0200u);
            break;
        }
        if (o == NULL) {
            return;
        }
        memcpy(snap_b, buf_b, MSIZE);
        restore_expect(o, blob_v, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_MISMATCH, snap_b, __LINE__);
    }

    /* Every cross-profile pair is an identity mismatch; the same profile is
       accepted (control). */
    for (uint32_t from = 0u; from < 3u; from++) {
        lynx_machine *src = snap_instance_p(buf_b, from, SNAP_SEED, churn_loop, sizeof churn_loop,
                                            0x0200u);
        if (src == NULL) {
            return;
        }
        advance_ticks(src, 500u);
        CHECK_EQ_U64(save_into(src, blob_c), 0u);
        for (uint32_t to = 0u; to < 3u; to++) {
            lynx_machine *dst = snap_instance_p(buf_c, to, SNAP_SEED, churn_loop,
                                                sizeof churn_loop, 0x0200u);
            if (dst == NULL) {
                return;
            }
            memcpy(snap_b, buf_c, MSIZE);
            if (to == from) {
                CHECK_EQ_U64(lynx_snapshot_restore(dst, blob_c, SNAP_TOTAL), LYNX_OK);
                CHECK(memcmp(buf_c, buf_b, MSIZE) == 0);
            } else {
                restore_expect(dst, blob_c, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_MISMATCH, snap_b,
                               __LINE__);
            }
        }
    }

    /* A valid hash is not enough: values are checked after the hash and before
       the apply. Each edit below recomputes the hash so the value check, not
       the hash, refuses. */
    const unsigned char *pay = blob_v + LYNX__SNAPSHOT_HEADER_LEN;
    VALUE_REFUSED(m, blob_v, PAY_P, pay[PAY_P] & ~0x10u, 1u, snap_a);   /* P bit 4 clear */
    VALUE_REFUSED(m, blob_v, PAY_P, pay[PAY_P] & ~0x20u, 1u, snap_a);   /* P bit 5 clear */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x40u, 1u, snap_a);             /* undefined latch bit */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x80u, 1u, snap_a);             /* undefined latch bit */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x01u, 1u, snap_a);             /* IRQ_TAKE: never left set */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x02u, 1u, snap_a);             /* NMI_TAKE: never left set */
    VALUE_REFUSED(m, blob_v, PAY_FLAGS, 0x03u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_PAGE_LATCH, 0x01u, 1u, snap_a);       /* legacy has no page latch */
    VALUE_REFUSED(m, blob_v, PAY_PAGE_LATCH, 0x80u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_STEP, 0xFFu, 1u, snap_a);              /* step past the script */
    VALUE_REFUSED(m, blob_v, PAY_STEP, 0x20u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_OPCODE, 0x07u, 1u, snap_a);            /* undefined under none */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, LYNX__COST_TEST_SKEW, 1u, snap_a); /* not this instance's */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, LYNX__COST_MODEL_COUNT, 1u, snap_a); /* undefined */
    VALUE_REFUSED(m, blob_v, PAY_COST_MODEL, 0xFFu, 1u, snap_a);
    uint64_t ticks = get_le(pay + PAY_TICKS, 8u);
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, ticks + 5u, 8u, snap_a);     /* flat5 max is +4 */
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, ticks + 255u, 8u, snap_a);  /* inherited gap */
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, ticks - 1u, 8u, snap_a);     /* before guest time */
    VALUE_REFUSED(m, blob_v, PAY_READY_AT, UINT64_MAX, 8u, snap_a);
    /* Guest time above the lynx_advance limit is a state no run reaches (WR-03):
       the span check alone passes these (ready == ticks), the limit check refuses. */
    {
        const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
        static const uint64_t past_limit[] = {1u, 2u, 255u, 0x10000u};
        for (size_t i = 0u; i < sizeof past_limit / sizeof past_limit[0]; i++) {
            memcpy(blob_bad, blob_v, SNAP_TOTAL);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit + past_limit[i], 8u);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + past_limit[i], 8u);
            rehash_payload(blob_bad);
            RESTORE_EXPECT(m, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_a);
        }
    }

    /* The reverse cost-model case: a test_skew instance refuses a flat5 blob. */
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    memcpy(snap_b, machine_buf, MSIZE);
    RESTORE_EXPECT(m, blob_v, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_FLAT5), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* A boundary state (70 ticks: the prelude done, PC at the loop head): the
       canonical boundary zeros are checked too. */
    lynx_machine *bnd = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (bnd == NULL) {
        return;
    }
    advance_ticks(bnd, 70u);
    CHECK_EQ_HEX(reg_of(bnd, LYNX_REG_CPU_STATE), 0u);
    CHECK_EQ_U64(save_into(bnd, blob_c), 0u);
    VALUE_REFUSED(m, blob_c, PAY_OPCODE, 0xBDu, 1u, snap_a); /* opcode held at a boundary */
    VALUE_REFUSED(m, blob_c, PAY_EA, 0x0001u, 2u, snap_a);
    VALUE_REFUSED(m, blob_c, PAY_TMP, 0x01u, 1u, snap_a);

    /* Positive cases, each a control that the rehash is right and the apply is
       seen: a PC outside the loaded content restores (the CPU may run
       anywhere), as does the largest allowed cpu_ready_at. */
    static const uint16_t outside_pcs[] = {0x0000u, 0x01FFu, 0x0300u, 0xFFFFu};
    for (size_t i = 0u; i < sizeof outside_pcs / sizeof outside_pcs[0]; i++) {
        memcpy(blob_bad, blob_c, SNAP_TOTAL);
        put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_PC, outside_pcs[i], 2u);
        rehash_payload(blob_bad);
        CHECK_EQ_U64(lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), outside_pcs[i]);
    }
    memcpy(blob_bad, blob_v, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, ticks + 4u, 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_bad, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 4u);

    /* The test-skew table's 14-tick vector cycle allows at most 13 positive
       remaining ticks when a public advance returns at least one tick later. */
    lynx_machine *skew = snap_instance(buf_b, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (skew == NULL) {
        return;
    }
    CHECK_EQ_U64(lynx__machine_set_cost_model(skew, LYNX__COST_TEST_SKEW), LYNX_OK);
    advance_ticks(skew, 1u);
    CHECK_EQ_U64(save_into(skew, blob_c), 0u);
    const uint64_t skew_ticks = get_le(blob_c + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, 8u);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, skew_ticks + 13u, 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_snapshot_restore(skew, blob_bad, SNAP_TOTAL), LYNX_OK);
    CHECK_EQ_HEX(reg_of(skew, LYNX_REG_CPU_STATE) >> 24, 13u);
    memcpy(blob_bad, blob_c, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, skew_ticks + 14u, 8u);
    rehash_payload(blob_bad);
    memcpy(snap_b, skew, MSIZE);
    RESTORE_EXPECT(skew, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);

    /* Timer controls are validated as a complete candidate before apply. */
    memcpy(snap_a, m, MSIZE);
    VALUE_REFUSED(m, blob_v, PAY_TIMERS + 2u, 0x8Fu, 1u, snap_a); /* unsupported T0 link */
    VALUE_REFUSED(m, blob_v, PAY_TIMERS + 3u, 0x10u, 1u, snap_a); /* reserved CTLB bits */
    VALUE_REFUSED(m, blob_v, PAY_TIMER_PHASE, 0xFFu, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_TIMER_EVENT_APPLIED, 2u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_TIMER_EVENT_APPLIED, 1u, 1u, snap_a); /* no due edge */
    VALUE_REFUSED(m, blob_v, PAY_BUS_HOLD, 2u, 1u, snap_a);

    /* Audio format-6 fields are validated before any candidate is applied. */
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 41u, LYNX__AUDIO_RING_NODE_COUNT, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 42u, LYNX__AUDIO_RING_DEBT_LIMIT + 1u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 50u, 2u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 51u, 2u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 52u, 2u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 53u, 1u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 54u, LYNX_AV_MIN_EVENT_CAPACITY + 1u, 4u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 58u, 1u, 8u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 66u, 2399u, 8u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 74u, 6u, 8u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 82u, 1u, 1u, snap_a);
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 4u, 0x10u, 1u, snap_a); /* CTLB reserved bits */
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 6u, 0x1000u, 2u, snap_a); /* shifter wider than 12 bits */
    VALUE_REFUSED(m, blob_v, PAY_AUDIO + 3u, 0x0Fu, 1u, snap_a); /* unsupported linked count */

    /* Control: the untouched blob is accepted and gives the instance as saved,
       so every refusal above was the edit. */
    CHECK_EQ_U64(lynx_snapshot_restore(m, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_UNCHANGED(machine_buf, at_top);

    snapshot_forged_cpu_states();
    snapshot_tick_limit();
}

static void timer_snapshot_control_refusal(lynx_machine *target, size_t offset,
                                           uint64_t value, size_t width,
                                           const unsigned char *source)
{
    CHECK_EQ_U64(lynx_watch_configure(target, 0x0400u, 1u), LYNX_OK);
    memcpy(snap_b, target, MSIZE);
    memcpy(blob_bad, source, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + offset, value, width);
    rehash_payload(blob_bad);
    RESTORE_EXPECT(target, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
    CHECK_EQ_U64(target->host_observation.watch_armed, 1u);
}

static void case_timer_snapshot_controls(void)
{
    static const uint32_t profiles[] = {LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    const int failures_before = check_failures;

    for (size_t profile_index = 0u;
         profile_index < sizeof profiles / sizeof profiles[0]; profile_index++) {
        const uint32_t profile = profiles[profile_index];
        lynx_machine *source = snap_instance_p(machine_buf, profile, SNAP_SEED,
                                               timer_snapshot_program,
                                               sizeof timer_snapshot_program, 0x0200u);
        lynx_machine *restored = snap_instance_p(buf_b, profile, SNAP_SEED,
                                                 timer_snapshot_program,
                                                 sizeof timer_snapshot_program, 0x0200u);
        lynx_machine *target = snap_instance_p(buf_c, profile, SNAP_SEED,
                                               timer_snapshot_program,
                                               sizeof timer_snapshot_program, 0x0200u);
        if (source == NULL || restored == NULL || target == NULL) {
            return;
        }

        /* SEI (2 cycles), LDA #1 (2), two absolute stores (4 each), then
           LDA #$98 (2) put the final CTLA write cycle at cycle index 17:
           grant 85 at flat5, with the watch returning at grant + 1 == 86.
           Holding the posted request immediately afterward captures a real
           positive ready span. */
        uint64_t consumed = 0u;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        CHECK_EQ_U64(lynx_watch_configure(source, 0xFD01u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(source, 1000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK_EQ_U64(source->guest_ticks, 86u);
        CHECK_EQ_U64(source->timer_bank.timers[0].control_a, 0x98u);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 1u), LYNX_OK);
        CHECK_EQ_HEX(reg_of(source, LYNX_REG_CPU_STATE) >> 24, 4u);
        CHECK_EQ_U64(save_into(source, blob_v), 0u);

        CHECK_EQ_U64(lynx_watch_configure(restored, 0x0400u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_v, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_U64(restored->host_observation.watch_armed, 0u);
        CHECK_EQ_HEX(reg_of(restored, LYNX_REG_CPU_STATE) >> 24, 4u);
        compare_snapshot_guest(source, restored);

        /* Let both machines remain held across several divider/borrow/IRQ
           edges. Expired readiness is canonicalized to the common guest tick. */
        advance_ticks(source, 176u);
        advance_ticks(restored, 176u);
        CHECK(source->timer_bank.pending != 0u);
        CHECK_EQ_U64(source->bus_hold, 1u);
        CHECK_EQ_U64(source->cpu_ready_at, source->guest_ticks);
        compare_snapshot_guest(source, restored);
        CHECK_EQ_U64(save_into(source, blob_c), 0u);
        CHECK_EQ_U64(lynx_watch_configure(restored, 0x0400u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_c, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_U64(restored->host_observation.watch_armed, 0u);
        CHECK_EQ_U64(restored->cpu_ready_at, restored->guest_ticks);
        CHECK_EQ_HEX(reg_of(restored, LYNX_REG_CPU_STATE) >> 24, 0u);
        compare_snapshot_guest(source, restored);

        /* Every hostile blob has a valid recomputed hash. Each refusal is
           checked against the entire target instance with a live watch armed. */
        CHECK_EQ_U64(lynx__machine_set_bus_hold(target, 1u), LYNX_OK);
        advance_ticks(target, 9u);
        timer_snapshot_control_refusal(target, PAY_READY_AT,
                                       get_le(blob_c + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, 8u) + 5u,
                                       8u, blob_c); /* flat5 span exceeds reachable +4 */
        timer_snapshot_control_refusal(target, PAY_READY_AT,
                                       get_le(blob_c + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, 8u) - 1u,
                                       8u, blob_c); /* stale behind-tick readiness */
        timer_snapshot_control_refusal(target, PAY_TIMERS + 2u, 0x8Fu, 1u,
                                       blob_c); /* enabled T0 incoming link is unsupported */
        timer_snapshot_control_refusal(target, PAY_TIMERS + 4u + 2u, 0x8Fu, 1u,
                                       blob_c); /* enabled T1 incoming audio link is absent */
        timer_snapshot_control_refusal(target, PAY_TIMERS + 6u * 4u + 2u, 0x8Fu, 1u,
                                       blob_c); /* T6 has no supported predecessor */
        timer_snapshot_control_refusal(target, PAY_TIMERS + 3u, 0x02u, 1u,
                                       blob_c); /* BORROW IN without LAST CLOCK */
        timer_snapshot_control_refusal(target, PAY_TIMERS + 3u, 0x10u, 1u,
                                       blob_c); /* reserved CTLB bits */
        timer_snapshot_control_refusal(target, PAY_TIMER_PHASE, 0xFFu, 1u, blob_c);
        timer_snapshot_control_refusal(target, PAY_TIMER_EVENT_APPLIED, 2u, 1u, blob_c);
        timer_snapshot_control_refusal(target, PAY_TIMER_EVENT_APPLIED, 1u, 1u,
                                       blob_c); /* current public tick has no unapplied edge */
        timer_snapshot_control_refusal(target, PAY_BUS_HOLD, 2u, 1u, blob_c);
        {
            const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
            memcpy(blob_bad, blob_c, SNAP_TOTAL);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TICKS, limit + 1u, 8u);
            put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_READY_AT, limit + 1u, 8u);
            rehash_payload(blob_bad);
            CHECK_EQ_U64(lynx_watch_configure(target, 0x0400u, 1u), LYNX_OK);
            memcpy(snap_b, target, MSIZE);
            RESTORE_EXPECT(target, blob_bad, SNAP_TOTAL, LYNX_ERR_SNAPSHOT_INVALID, snap_b);
            CHECK_EQ_U64(target->host_observation.watch_armed, 1u);
        }

        /* T2 has a defined T0 predecessor. This rehashed positive control
           distinguishes supported link input from the three refusals above. */
        memcpy(blob_bad, blob_c, SNAP_TOTAL);
        put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_TIMERS + 2u * 4u + 2u,
               0x8Fu, 1u);
        rehash_payload(blob_bad);
        CHECK_EQ_U64(lynx_watch_configure(target, 0x0400u, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_restore(target, blob_bad, SNAP_TOTAL), LYNX_OK);
        CHECK_EQ_U64(target->timer_bank.timers[2].control_a, 0x8Fu);
        CHECK_EQ_U64(target->host_observation.watch_armed, 0u);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.timer_snapshot_controls\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* ---- the CPU programs with hand-derived values (API-03, CPU-02, CPU-03) ---- */

/* Registers PC, A, X, Y, S, P against the hand-derived values. */
static void check_regs(const lynx_machine *m, uint32_t pc, uint32_t a, uint32_t x, uint32_t y,
                       uint32_t s, uint32_t p, int line)
{
    const uint32_t want[6] = {pc, a, x, y, s, p};
    static const char *const names[6] = {"PC", "A", "X", "Y", "S", "P"};
    for (uint32_t r = 0u; r < 6u; r++) {
        uint32_t v = 0xA5A5A5A5u;
        lynx_status st = lynx_read_register(m, r, &v);
        if (st != LYNX_OK || v != want[r]) {
            fprintf(stderr, "%s:%d: %s = 0x%x (status %u), hand-derived 0x%x\n", __FILE__, line,
                    names[r], (unsigned)v, (unsigned)st, (unsigned)want[r]);
            check_failures++;
        }
    }
}
#define CHECK_REGS(m, pc, a, x, y, s, p) check_regs((m), (pc), (a), (x), (y), (s), (p), __LINE__)

static unsigned char want_ram[LYNX_ADDRESS_SPACE];

/* The whole RAM against want_ram, reporting the first difference. */
static void check_ram(const lynx_machine *m, int line)
{
    CHECK_EQ_U64(lynx_read_memory(m, 0u, mem_out, LYNX_ADDRESS_SPACE), LYNX_OK);
    for (uint32_t a = 0u; a < LYNX_ADDRESS_SPACE; a++) {
        if (mem_out[a] != want_ram[a]) {
            fprintf(stderr, "%s:%d: RAM $%04x = %02x, hand-derived %02x\n", __FILE__, line,
                    (unsigned)a, (unsigned)mem_out[a], (unsigned)want_ram[a]);
            check_failures++;
            return;
        }
    }
}

static void case_cpu_program(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    CHECK_EQ_U64(cfg.struct_size, sizeof(lynx_config));
    CHECK_EQ_U64(cfg.profile, LYNX_PROFILE_NONE);
    CHECK_EQ_U64(cfg.ram_policy, LYNX_RAM_POLICY_ZERO);
    CHECK_EQ_U64(cfg.ram_seed, 0u);
    CHECK_EQ_U64(cfg.reserved, 0u);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_NONE), "none") == 0);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_LYNX1), "lynx1") == 0);
    CHECK(strcmp(lynx_profile_name(LYNX_PROFILE_LYNX2), "lynx2") == 0);
    CHECK(lynx_profile_name(3u) == NULL);
    CHECK(lynx_profile_name(0xFFFFFFFFu) == NULL);

    size_t size = lynx_machine_size();
    size_t align = lynx_machine_align();
    CHECK(size > 65536u);
    CHECK(is_power_of_two(align));
    CHECK(align <= _Alignof(max_align_t));
    CHECK_EQ_U64(lynx_machine_size(), size);
    CHECK_EQ_U64(lynx_machine_align(), align);
    /* White-box: the two queries must report exactly the real layout. */
    CHECK_EQ_U64(size, sizeof(struct lynx_machine));
    CHECK_EQ_U64(align, _Alignof(struct lynx_machine));
    if (size > sizeof machine_buf) {
        return; /* the next step would overrun the buffer; the checks above already failed */
    }
    CHECK_EQ_U64((uintptr_t)machine_buf % align, 0u);

    /* The embedded programs are the committed fixtures, byte for byte. */
    expect_sha256(sum_trap, sizeof sum_trap, SUM_TRAP_SHA256);
    expect_sha256(churn_loop, sizeof churn_loop, CHURN_LOOP_SHA256);

    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(ticks_of(m), 0u);

    /* sum-trap under none, RAM policy zero. Expected values are the trace table
       in fixtures/cpu/sum-trap.s (datasheet cycle counts, 5 ticks per cycle):
       105 cycles, then the $07 at $021B stops the run before it is fetched. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), 0u);
    ADVANCE_EXPECT(m, 100000u, 525u, LYNX_STOP_UNDEFINED);
    CHECK_EQ_U64(ticks_of(m), 525u);
    CHECK_REGS(m, 0x021Bu, 0x0Fu, 0xFFu, 0x00u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u); /* boundary, nothing in flight */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_LATCH), 0u);
    memset(want_ram, 0, sizeof want_ram);
    memcpy(&want_ram[0x0200], sum_trap, sizeof sum_trap);
    want_ram[0x0010] = 0x01u; /* STX $10 of the last iteration */
    want_ram[0x0011] = 0x01u; /* INC $11 */
    want_ram[0x0300] = 0x0Fu; /* STA $0300 */
    want_ram[0x01FC] = 0x11u; /* JSR return address low byte */
    want_ram[0x01FD] = 0x0Fu; /* the JSR high byte $02, overwritten by PHA */
    check_ram(m, __LINE__);
    /* Repeated: the same stop, nothing consumed, the buffer byte-identical. */
    memcpy(snap_a, machine_buf, MSIZE);
    ADVANCE_EXPECT(m, 100000u, 0u, LYNX_STOP_UNDEFINED);
    ADVANCE_EXPECT(m, UINT64_MAX, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* advance(524): all 105 cycles are granted (the last, PLA's pull, at 520),
       the registers are already final and one cycle is in flight until 525. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 524u, 524u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x0Fu, 0xFFu, 0x00u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 1u << 24);
    ADVANCE_EXPECT(m, 1u, 1u, LYNX_STOP_BUDGET);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
    ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);
    /* Exactly 525 in one call: guest time reaches the target at the boundary,
       so BUDGET wins and the next call reports UNDEFINED. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 525u, 525u, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(machine_buf, snap_a);
    ADVANCE_EXPECT(m, 1u, 0u, LYNX_STOP_UNDEFINED);
    /* advance(520) stops before the last PLA cycle: A is still 0, S still $FC,
       PC already $021B. The next call performs it and then stops. */
    CHECK_EQ_U64(load_bll_at(m, sum_trap, sizeof sum_trap, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 520u, 520u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x021Bu, 0x00u, 0xFFu, 0x00u, 0xFCu, 0x36u);
    ADVANCE_EXPECT(m, 100u, 5u, LYNX_STOP_UNDEFINED);
    CHECK_UNCHANGED(machine_buf, snap_a);

    /* churn-loop under none, RAM policy zero, after the 70-tick prelude and one
       255-tick iteration (fixtures/cpu/churn-loop.s). */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 325u, 325u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x020Cu, 0x00u, 0x01u, 0x08u, 0xFDu, 0x34u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
    memset(want_ram, 0, sizeof want_ram);
    memcpy(&want_ram[0x0200], churn_loop, sizeof churn_loop);
    want_ram[0x0041] = 0x05u; /* pointer high byte; $0040 = $00 */
    want_ram[0x01FC] = 0x02u; /* JSR return address $021A */
    want_ram[0x01FB] = 0x1Au; /* ($01FD = $00 from PHA, $0600 = $00 from STA) */
    check_ram(m, __LINE__);
    /* After 70 + 255k ticks: PC at the loop head and X = k. */
    CHECK_EQ_U64(load_bll_at(m, churn_loop, sizeof churn_loop, 0x0200u), LYNX_OK);
    ADVANCE_EXPECT(m, 70u, 70u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x020Cu, 0x05u, 0x00u, 0x08u, 0xFDu, 0x34u); /* last flag write: LDA #$05 */
    for (uint32_t k = 1u; k <= 20u; k++) {
        ADVANCE_EXPECT(m, 255u, 255u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x020Cu);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_X), k);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_Y), 0x08u);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_S), 0xFDu);
    }

    lynx_machine_deinit(m);
}


/* ---- profile selection and divergence (CPU-02, CPU-05) ---- */

/* The Rockwell R65C02 meaning of the 32 profile-dependent opcodes, written from
   the opcode bits alone (a second statement of the rule, not a read of the
   core's tables): low nibble 7 is the zero-page bit operation, F the bit
   branch; bits 4-6 give the bit number; bit 7 chooses RMB or BBR (0) against
   SMB or BBS (1). */
static int slot_defined(uint8_t op)
{
    return (op & 0x0Fu) == 0x07u || (op & 0x0Fu) == 0x0Fu;
}
static int slot_is_branch(uint8_t op)
{
    return (op & 0x0Fu) == 0x0Fu;
}
static uint8_t slot_mask(uint8_t op)
{
    return (uint8_t)(1u << ((op >> 4) & 7u));
}
static uint8_t slot_result(uint8_t op, uint8_t v)
{
    return (op & 0x80u) != 0u ? (uint8_t)(v | slot_mask(op)) : (uint8_t)(v & (uint8_t)~slot_mask(op));
}
/* BBR branches when the bit is clear, BBS when it is set. */
static int slot_taken(uint8_t op, uint8_t v)
{
    int bit_set = (v & slot_mask(op)) != 0u;
    return (op & 0x80u) != 0u ? bit_set : !bit_set;
}

/* LDA #v; STA $10; then the slot with zero-page operand $10, followed by marker
   code that shows which way execution went:
     bit operation (2 bytes): LDA #$5A; STA $11; JMP self
     bit branch (3 bytes, relative offset 7):
       fall-through at $0207: LDA #$BB; STA $12; JMP self
       branch target $020E:   LDA #$AA; STA $12; JMP self
   Under profile none the slot is never fetched. Under lynx1 the slot is a NOP
   of 2 bytes (bit operations) or 3 bytes (bit branches), so the marker for the
   fall-through path appears and $10 is unchanged. */
static size_t slot_image(uint8_t *img, uint8_t op, uint8_t v)
{
    size_t n = 0u;
    img[n++] = 0xA9u; img[n++] = v;
    img[n++] = 0x85u; img[n++] = 0x10u;
    img[n++] = op;    img[n++] = 0x10u;
    if (slot_is_branch(op)) {
        static const uint8_t tail[] = {0x07u, 0xA9u, 0xBBu, 0x85u, 0x12u, 0x4Cu, 0x0Bu, 0x02u,
                                       0xA9u, 0xAAu, 0x85u, 0x12u, 0x4Cu, 0x12u, 0x02u};
        memcpy(img + n, tail, sizeof tail);
        n += sizeof tail;
    } else {
        static const uint8_t tail[] = {0xA9u, 0x5Au, 0x85u, 0x11u, 0x4Cu, 0x0Au, 0x02u};
        memcpy(img + n, tail, sizeof tail);
        n += sizeof tail;
    }
    return n;
}

static uint8_t ram_byte(const lynx_machine *m, uint32_t addr)
{
    uint8_t b = 0xA5u;
    CHECK_EQ_U64(lynx_read_memory(m, addr, &b, 1u), LYNX_OK);
    return b;
}

static lynx_machine *profile_instance(void *mem, uint32_t profile, const uint8_t *image,
                                      size_t len)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(mem, &cfg);
    if (m == NULL) {
        return NULL;
    }
    CHECK_EQ_U64(load_bll_at(m, image, len, 0x0200u), LYNX_OK);
    return m;
}

/* One slot under one profile with the zero-page cell starting at v. */
static void slot_run(uint32_t profile, uint8_t op, uint8_t v)
{
    uint8_t img[32];
    const size_t len = slot_image(img, op, v);
    const int branch = slot_is_branch(op);
    const uint32_t before = (uint32_t)check_failures;
    lynx_machine *m = profile_instance(machine_buf, profile, img, len);
    if (m == NULL) {
        return;
    }
    if (profile == LYNX_PROFILE_NONE) {
        /* LDA #v and STA $10 are 5 cycles = 25 ticks; the slot is not fetched. */
        ADVANCE_EXPECT(m, 1000u, 25u, LYNX_STOP_UNDEFINED);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0204u);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), v);
        CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
        memcpy(snap_a, machine_buf, MSIZE);
        ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
        CHECK_UNCHANGED(machine_buf, snap_a);
    } else {
        /* Long enough for the slot and the marker code, which then loops. */
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        const int lynx2 = profile == LYNX_PROFILE_LYNX2;
        const uint8_t want10 = lynx2 && !branch ? slot_result(op, v) : v;
        CHECK_EQ_HEX(ram_byte(m, 0x10u), want10);
        if (branch) {
            const uint8_t want12 = (lynx2 && slot_taken(op, v)) ? 0xAAu : 0xBBu;
            CHECK_EQ_HEX(ram_byte(m, 0x12u), want12);
            CHECK_EQ_HEX(ram_byte(m, 0x11u), 0u);
            CHECK_EQ_HEX(reg_of(m, LYNX_REG_A), want12);
        } else {
            CHECK_EQ_HEX(ram_byte(m, 0x11u), 0x5Au);
            CHECK_EQ_HEX(ram_byte(m, 0x12u), 0u);
            CHECK_EQ_HEX(reg_of(m, LYNX_REG_A), 0x5Au);
        }
        if (lynx2) {
            /* Rockwell datasheet counts at flat5 (5 ticks per cycle): RMB and SMB
               take 5 cycles, BBR and BBS 5 cycles plus 1 when taken (same
               page here). Stop exactly after the slot: PC is the next byte. */
            uint64_t slot_cycles = branch ? (slot_taken(op, v) ? 6u : 5u) : 5u;
            uint32_t want_pc = !branch ? 0x0206u : (slot_taken(op, v) ? 0x020Eu : 0x0207u);
            m = profile_instance(machine_buf, profile, img, len);
            if (m != NULL) {
                ADVANCE_EXPECT(m, 25u + 5u * slot_cycles, 25u + 5u * slot_cycles,
                               LYNX_STOP_BUDGET);
                CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), want_pc);
                CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE), 0u);
                CHECK_EQ_HEX(ram_byte(m, 0x10u), branch ? v : slot_result(op, v));
            }
        }
    }
    if ((uint32_t)check_failures != before) {
        fprintf(stderr, "  (profile %u, opcode $%02X, zero-page cell $%02X)\n", (unsigned)profile,
                (unsigned)op, (unsigned)v);
    }
}

static void case_profile_select(void)
{
    /* A host selects the profile at init; the instance reports it and keeps it
       across reset. */
    for (uint32_t profile = LYNX_PROFILE_NONE; profile <= LYNX_PROFILE_LYNX2; profile++) {
        lynx_config cfg;
        lynx_identity id;
        lynx_config_default(&cfg);
        cfg.profile = profile;
        lynx_machine *m = init_in(machine_buf, &cfg);
        if (m == NULL) {
            return;
        }
        CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, profile);
        CHECK(lynx_profile_name(id.profile) != NULL);
        CHECK(strcmp(lynx_profile_name(id.profile),
                     profile == 0u ? "none" : (profile == 1u ? "lynx1" : "lynx2")) == 0);
        CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
        CHECK_EQ_U64(lynx_get_identity(m, &id), LYNX_OK);
        CHECK_EQ_U64(id.profile, profile);
        lynx_machine_deinit(m);
    }

    /* The named case of the plan: RMB0 $10 on a cell holding $FF. */
    {
        static const uint8_t rmb0[] = {0xA9u, 0xFFu, 0x85u, 0x10u, 0x07u, 0x10u,
                                       0xA9u, 0x5Au, 0x85u, 0x11u, 0x4Cu, 0x0Au, 0x02u};
        lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_LYNX2, rmb0, sizeof rmb0);
        if (m == NULL) {
            return;
        }
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFEu); /* lynx2: RMB0 cleared bit 0 */
        /* Reset keeps the profile; a reload runs the same program the same way. */
        CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
        CHECK_EQ_U64(load_bll_at(m, rmb0, sizeof rmb0, 0x0200u), LYNX_OK);
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFEu);
        lynx_machine_deinit(m);
        m = profile_instance(machine_buf, LYNX_PROFILE_LYNX1, rmb0, sizeof rmb0);
        if (m == NULL) {
            return;
        }
        ADVANCE_EXPECT(m, 1000u, 1000u, LYNX_STOP_BUDGET);
        CHECK_EQ_HEX(ram_byte(m, 0x10u), 0xFFu); /* lynx1: a NOP of 2 bytes */
        CHECK_EQ_HEX(ram_byte(m, 0x11u), 0x5Au);
        lynx_machine_deinit(m);
    }

    /* All 32 slots, every profile, cell values that tell the bit numbers apart
       (all set, all clear, only bit n set, only bit n clear). */
    for (uint32_t op = 0u; op < 256u; op++) {
        if (!slot_defined((uint8_t)op)) {
            continue;
        }
        const uint8_t cells[4] = {0xFFu, 0x00u, slot_mask((uint8_t)op),
                                  (uint8_t)~slot_mask((uint8_t)op)};
        for (uint32_t profile = LYNX_PROFILE_NONE; profile <= LYNX_PROFILE_LYNX2; profile++) {
            for (size_t c = 0u; c < 4u; c++) {
                slot_run(profile, (uint8_t)op, cells[c]);
            }
        }
    }
}

/* The retained 32-byte tracer image (RESEARCH 7.3): one image, three outcomes.
   As code at $0200: 03 is a 1-cycle NOP, 0A is ASL A (2 cycles), 11 18 is ORA
   ($18),Y (5 cycles; RAM is zero, so the pointer is $0000, Y is 0 and the
   operand is 0): 8 cycles = 40 ticks at flat5. Then $1F, a bit-op slot, at $0204. */
static void case_profile_divergence(void)
{
    /* Zero RAM, A = X = Y = 0 and Z set by the ORA: P = $34 | Z = $36. */
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, tracer_image,
                                       sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 1000u, 40u, LYNX_STOP_UNDEFINED);
    CHECK_REGS(m, 0x0204u, 0u, 0u, 0u, 0xFDu, 0x36u);
    ADVANCE_EXPECT(m, 1000u, 0u, LYNX_STOP_UNDEFINED);
    lynx_machine_deinit(m);

    /* lynx1: $1F $26 $2D is a 3-byte NOP of 4 cycles (20 ticks): PC = $0207. */
    m = profile_instance(machine_buf, LYNX_PROFILE_LYNX1, tracer_image, sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 60u, 60u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x0204u + 3u, 0u, 0u, 0u, 0xFDu, 0x36u);
    lynx_machine_deinit(m);

    /* lynx2: $1F is BBR1 $26,+$2D. RAM[$26] is 0, so bit 1 is clear and the
       branch is taken: 6 cycles (30 ticks), PC = $0207 + $2D = $0234. */
    m = profile_instance(machine_buf, LYNX_PROFILE_LYNX2, tracer_image, sizeof tracer_image);
    if (m == NULL) {
        return;
    }
    ADVANCE_EXPECT(m, 70u, 70u, LYNX_STOP_BUDGET);
    CHECK_REGS(m, 0x0204u + 3u + tracer_image[6], 0u, 0u, 0u, 0xFDu, 0x36u);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_PC), 0x0234u);
    lynx_machine_deinit(m);

    /* Under none every one of the 32 bit-op opcodes placed first in an image
       stops with nothing consumed and the buffer unchanged; the other 224 run
       the whole budget, which also exercises every defined opcode under the
       sanitizers. The operand byte $80 points zero-page and absolute operands at
       $0080, so no instruction writes the zero bytes at $0000 (BRK, defined
       everywhere) that a BRK or a wild jump would execute; DEC $00 would turn
       one into $FF, a bit-op slot. */
    uint32_t stopped = 0u;
    uint32_t ran = 0u;
    m = profile_instance(machine_buf, LYNX_PROFILE_NONE, trap_image, sizeof trap_image);
    if (m == NULL) {
        return;
    }
    /* This exhaustive dispatch pass is a white-box CPU-table probe, not a
       tier-A startup test. Select vector-window RAM so a BRK reached after an
       opcode cannot turn the CPU proof into an unavailable-route stop. The
       zero vector then loops in ordinary RAM for the bounded run. */
    m->mapctl = 0x08u;
    for (uint32_t op = 0u; op < 256u; op++) {
        const uint8_t img[3] = {(uint8_t)op, 0x80u, 0u};
        CHECK_EQ_U64(load_bll_at(m, img, sizeof img, 0x0200u), LYNX_OK);
        /* Loading a fresh BLL reapplies the declared MAPCTL-zero state. */
        m->mapctl = 0x08u;
        memcpy(snap_a, machine_buf, MSIZE);
        if (slot_defined((uint8_t)op)) {
            ADVANCE_EXPECT(m, 100u, 0u, LYNX_STOP_UNDEFINED);
            CHECK_UNCHANGED(machine_buf, snap_a);
            stopped++;
        } else {
            const uint32_t before = (uint32_t)check_failures;
            ADVANCE_EXPECT(m, 100u, 100u, LYNX_STOP_BUDGET);
            if ((uint32_t)check_failures != before) {
                fprintf(stderr, "  (opcode $%02X first in the image)\n", (unsigned)op);
            }
            ran++;
        }
    }
    CHECK_EQ_U64(stopped, 32u);
    CHECK_EQ_U64(ran, 224u);
    lynx_machine_deinit(m);
}

/* ---- cost model and slicing (CPU-03, ROADMAP criterion 3) ---- */

/* Named-fault switch of the persistent control api.cost_model.self_test: the
   case runs on the flat5 model where it expects test_skew, which is what a
   timing function that ignores its cost table would produce. */
static int g_fault_constant;

/* The test's own copy of the test_skew prices, one per cycle kind in the order
   of enum lynx__ck (RESEARCH section 8). It is deliberately not read from the
   core: a wrong price in the core then shows up as a wrong total. */
static const uint8_t skew_copy[LYNX__CK_COUNT] = {
    [LYNX__CK_OPCODE] = 4,       [LYNX__CK_OPERAND] = 6,   [LYNX__CK_DUMMY_STREAM] = 3,
    [LYNX__CK_DUMMY] = 10,       [LYNX__CK_READ] = 7,      [LYNX__CK_WRITE] = 9,
    [LYNX__CK_RMW_READ] = 8,     [LYNX__CK_STACK_READ] = 11, [LYNX__CK_STACK_WRITE] = 12,
    [LYNX__CK_POINTER] = 13,     [LYNX__CK_VECTOR] = 14,
};

static flat_bus g_fb;
static uint8_t seam_kinds[1024];
static uint8_t seam_replay[LYNX_ADDRESS_SPACE];

typedef struct seam_totals {
    uint64_t cycles;
    uint64_t skew_ticks;      /* sum of the test_skew price over every cycle */
    long long skew_extra;     /* sum of (skew price - 5) over every cycle */
    uint32_t end_pc;
} seam_totals;

/* Runs a program on the flat test bus (the internal seam, profile none) until
   the next instruction is one profile none stops at, or until max_cycles cycles
   have been logged. Records the cycle kinds in order, the sums over the log and
   the replay of every logged write onto the initial RAM. */
static seam_totals seam_run(const uint8_t *image, size_t len, size_t max_cycles)
{
    seam_totals tot;
    const lynx__cpu_decode *t = lynx__cpu_table(LYNX_PROFILE_NONE);
    lynx__cpu cpu;
    memset(&tot, 0, sizeof tot);
    flat_bus_clear(&g_fb);
    flat_bus_load(&g_fb, 0x0200u, image, (unsigned)len);
    memcpy(seam_replay, g_fb.mem, sizeof seam_replay);
    lynx__cpu_init(&cpu, LYNX_PROFILE_NONE);
    lynx__cpu_start_at(&cpu, t, 0x0200u);
    while (tot.cycles < max_cycles && !slot_defined(g_fb.mem[cpu.pc])) {
        flat_bus_reset_log(&g_fb);
        if (!flat_bus_step_instruction(&g_fb, &cpu, t)) {
            fprintf(stderr, "%s: the seam overran its cycle bound\n", __FILE__);
            check_failures++;
            break;
        }
        for (unsigned i = 0u; i < g_fb.nlog; i++) {
            const flat_entry *e = &g_fb.log[i];
            if (tot.cycles < sizeof seam_kinds) {
                seam_kinds[tot.cycles] = e->kind;
            }
            tot.cycles++;
            tot.skew_ticks += skew_copy[e->kind];
            tot.skew_extra += (long long)skew_copy[e->kind] - 5;
            if (e->write != 0u) {
                seam_replay[e->addr] = e->data;
            }
        }
    }
    tot.end_pc = cpu.pc;
    return tot;
}

/* sum-trap on the machine under one cost model. Returns ticks consumed. */
static uint64_t cost_run(uint32_t model, unsigned char *regs_ram)
{
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, sum_trap, sizeof sum_trap);
    uint64_t consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    if (m == NULL) {
        return 0u;
    }
    /* Fault fault-constant: the machine stays on flat5 where the test expects
       test_skew, as a timing function that ignores its cost table would. */
    if (model != LYNX__COST_FLAT5 && !g_fault_constant) {
        CHECK_EQ_U64(lynx__machine_set_cost_model(m, model), LYNX_OK);
    }
    CHECK_EQ_U64(lynx_advance(m, 100000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNDEFINED);
    /* registers (six words), CPU_STATE, then RAM */
    for (uint32_t r = 0u; r < 7u; r++) {
        put_le(regs_ram + 4u * r, reg_of(m, r), 4u);
    }
    CHECK_EQ_U64(lynx_read_memory(m, 0u, regs_ram + 28u, LYNX_ADDRESS_SPACE), LYNX_OK);
    /* The cost-model byte of CPU_LATCH names the model that ran. */
    CHECK_EQ_U64(reg_of(m, LYNX_REG_CPU_LATCH) >> 24, g_fault_constant ? 0u : model);
    lynx_machine_deinit(m);
    return consumed;
}

static unsigned char cost_flat[28u + LYNX_ADDRESS_SPACE];
static unsigned char cost_skew[28u + LYNX_ADDRESS_SPACE];

static void case_cost_model(void)
{
    /* The seam over the same bytes: cycle kinds, the price sums and the writes. */
    const seam_totals tot = seam_run(sum_trap, sizeof sum_trap, 100000u);
    CHECK_EQ_U64(tot.cycles, 105u);                 /* RESEARCH 7.1: 105 cycles */
    CHECK_EQ_HEX(tot.end_pc, 0x021Bu);              /* the next opcode is the $07 */

    const uint64_t flat = cost_run(LYNX__COST_FLAT5, cost_flat);
    const uint64_t skew = cost_run(LYNX__COST_TEST_SKEW, cost_skew);
    CHECK_EQ_U64(flat, 525u);
    CHECK_EQ_U64(flat, tot.cycles * 5u);

    /* The cost model changes consumed ticks and nothing architectural: the same
       registers, the same CPU position and the same RAM. */
    CHECK(memcmp(cost_flat, cost_skew, sizeof cost_flat) == 0);
    /* RAM equals the replay of the logged writes onto the initial RAM, so no write
       reached memory without passing the bus. */
    CHECK(memcmp(cost_flat + 28u, seam_replay, LYNX_ADDRESS_SPACE) == 0);

    /* The consumed-tick difference is the sum over the log of (price - 5). */
    const long long want_diff = tot.skew_extra;
    const long long got_diff = (long long)skew - (long long)flat;
    CHECK(want_diff > 0);
    if (skew == flat && want_diff != 0) {
        fprintf(stderr,
                "%s:%d: cost model ignored: test_skew consumed %llu ticks, the same as flat5, "
                "but the seam log prices it at %llu\n",
                __FILE__, __LINE__, (unsigned long long)skew,
                (unsigned long long)tot.skew_ticks);
        check_failures++;
    } else if (got_diff != want_diff) {
        fprintf(stderr, "%s:%d: test_skew minus flat5 is %lld ticks, the seam log gives %lld\n",
                __FILE__, __LINE__, got_diff, want_diff);
        check_failures++;
    }
    CHECK_EQ_U64(skew, tot.skew_ticks);

    /* The setter refuses a model this build does not define and writes nothing. */
    lynx_machine *m = profile_instance(machine_buf, LYNX_PROFILE_NONE, sum_trap, sizeof sum_trap);
    if (m == NULL) {
        return;
    }
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 2u), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, 0xFFFFFFFFu), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    CHECK_EQ_U64(lynx__machine_set_cost_model(NULL, 1u), LYNX_ERR_ARGUMENT);
    lynx_machine_deinit(m);
}

static unsigned char slice_ref[sizeof(struct lynx_machine)];

/* Runs churn-loop (seeded RAM) on a fresh load, under the model, for total ticks
   in the given chunk sizes (the last chunk is the remainder). */
static lynx_machine *slice_start(uint32_t model)
{
    lynx_machine *m = snap_instance(machine_buf, SNAP_SEED, churn_loop, sizeof churn_loop, 0x0200u);
    if (m != NULL && model != LYNX__COST_FLAT5) {
        CHECK_EQ_U64(lynx__machine_set_cost_model(m, model), LYNX_OK);
    }
    return m;
}

static void slice_run(lynx_machine *m, uint64_t total, uint64_t chunk)
{
    uint64_t done = 0u;
    while (done < total) {
        const uint64_t n = total - done < chunk ? total - done : chunk;
        advance_ticks(m, n);
        done += n;
    }
}

static void slice_compare(const char *what, uint64_t arg, uint32_t model)
{
    if (memcmp(machine_buf, slice_ref, MSIZE) != 0) {
        fprintf(stderr, "%s: model %u: %s %llu differs from one 4096-tick call\n", __FILE__,
                (unsigned)model, what, (unsigned long long)arg);
        check_failures++;
    }
}

static void slice_one_model(uint32_t model)
{
    lynx_machine *m = slice_start(model);
    if (m == NULL) {
        return;
    }
    advance_ticks(m, 4096u);
    memcpy(slice_ref, machine_buf, MSIZE);
    CHECK_EQ_U64(ticks_of(m), 4096u);

    static const uint64_t chunks[] = {1u, 3u, 7u, 64u, 255u, 512u};
    for (size_t i = 0u; i < sizeof chunks / sizeof chunks[0]; i++) {
        m = slice_start(model);
        if (m == NULL) {
            return;
        }
        slice_run(m, 4096u, chunks[i]);
        slice_compare("chunk size", chunks[i], model);
    }
    /* Every two-chunk split from 1 to 600. */
    for (uint64_t split = 1u; split <= 600u; split++) {
        m = slice_start(model);
        if (m == NULL) {
            return;
        }
        advance_ticks(m, split);
        advance_ticks(m, 4096u - split);
        slice_compare("split at", split, model);
    }

    /* Grants chain: stepping one tick at a time, the cycle count is read from
       CPU_STATE (bits 24-31, ticks until the next cycle may start). A grant is a
       step after which that delta is one less than the cycle's price, which
       happens only at the tick where the previous cycle ended. g[i+1] must be
       g[i] + ticks[i], with ticks[i] the price of the i-th cycle of the seam
       log under this model. */
    const seam_totals tot = seam_run(churn_loop, sizeof churn_loop, 300u);
    CHECK(tot.cycles >= 300u);
    m = slice_start(model);
    if (m == NULL) {
        return;
    }
    uint64_t grants[320];
    size_t ngrants = 0u;
    uint32_t prev_delta = 0u; /* the first cycle is due at tick 0 */
    for (uint64_t tick = 0u; tick < 600u; tick++) {
        advance_ticks(m, 1u);
        const uint32_t delta = reg_of(m, LYNX_REG_CPU_STATE) >> 24;
        if (prev_delta == 0u) {
            /* The CPU was ready at this tick: a cycle was granted here. */
            if (ngrants < sizeof grants / sizeof grants[0]) {
                const uint8_t kind = seam_kinds[ngrants];
                const uint32_t price = model == LYNX__COST_FLAT5 ? 5u : skew_copy[kind];
                CHECK_EQ_U64(delta + 1u, price); /* the ready delta restarts at the price */
                grants[ngrants++] = tick;
            }
        } else {
            CHECK_EQ_U64(delta, prev_delta - 1u); /* no grant: the delta counts down */
        }
        prev_delta = delta;
    }
    CHECK(ngrants > 40u);
    CHECK_EQ_U64(grants[0], 0u);
    for (size_t i = 0u; i + 1u < ngrants; i++) {
        const uint32_t price = model == LYNX__COST_FLAT5 ? 5u : skew_copy[seam_kinds[i]];
        CHECK_EQ_U64(grants[i + 1u], grants[i] + price);
    }
    /* And the stepped run is the same instance as a single 600-tick call. */
    memcpy(slice_ref, machine_buf, MSIZE);
    m = slice_start(model);
    if (m != NULL) {
        advance_ticks(m, 600u);
        slice_compare("600 one-tick steps against a single call of", 600u, model);
    }
}

static void case_advance_slicing(void)
{
    slice_one_model(LYNX__COST_FLAT5);
    slice_one_model(LYNX__COST_TEST_SKEW);
}

static void case_timer_tracer(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(load_bll_at(m, timer_tracer_program, sizeof timer_tracer_program,
                             0x0200u), LYNX_OK);
    /* The raw RAM byte must remain independent from the Mikey register view. */
    m->ram[0xFD02u] = 0xE7u;
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFD01u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK_EQ_U64(consumed, 76u);
    CHECK_EQ_U64(ticks_of(m), 76u);
    uint8_t timer[4] = {0xA5u, 0xA5u, 0xA5u, 0xA5u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, timer, sizeof timer), LYNX_OK);
    CHECK_EQ_U64(timer[0], 1u);
    CHECK_EQ_U64(timer[1], 0x88u);
    CHECK_EQ_U64(timer[2], 1u);
    CHECK_EQ_U64(timer[3] & 0x08u, 0u);

    /* The first edge is at 80: target 80 leaves it pending, target 81
       consumes it before the same-tick CPU bus action. */
    CHECK_EQ_U64(lynx_advance(m, 3u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 3u);
    CHECK_EQ_U64(ticks_of(m), 79u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD02u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0], 1u);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 80u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD02u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0], 1u);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 81u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD02u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0], 0u);
    CHECK_EQ_U64(lynx_advance(m, 15u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 15u);
    CHECK_EQ_U64(ticks_of(m), 96u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0] & 0x08u, 0u);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 97u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD02u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0], 0u);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, timer, 1u), LYNX_OK);
    CHECK_EQ_U64(timer[0] & 0x08u, 0x08u);

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0401u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 144u);
    CHECK_EQ_U64(ticks_of(m), 241u);
    CHECK_EQ_U64(m->ram[0x0400u], 0u);
    CHECK_EQ_U64(m->ram[0x0401u], 1u);
    CHECK_EQ_U64(m->ram[0xFD02u], 0xE7u);

    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, timer, sizeof timer), LYNX_OK);
    CHECK_EQ_U64(timer[0], 1u);
    CHECK_EQ_U64(timer[1], 0x88u);
    CHECK_EQ_U64(timer[2], 0u);
    CHECK_EQ_U64(timer[3] & 0x08u, 0x08u);
    uint8_t irq[2] = {0xA5u, 0xA5u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD80u, irq, sizeof irq), LYNX_OK);
    CHECK_EQ_U64(irq[0], 1u);
    CHECK_EQ_U64(irq[1], 1u);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_UNCHANGED(m, snap_a);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_tracer\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void run_irq_guest_profile(uint32_t profile)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, irq_guest_program, sizeof irq_guest_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0404u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 5000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK(consumed != 0u && consumed < 5000u);
    CHECK_EQ_U64(ticks_of(m), consumed);
    CHECK_EQ_U64(m->mapctl, 0x08u);
    CHECK_EQ_U64(m->ram[0xFFFEu], 0x00u);
    CHECK_EQ_U64(m->ram[0xFFFFu], 0x03u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x03u);
    CHECK_EQ_U64(m->ram[0x0401u], 0x03u);
    CHECK_EQ_U64(m->ram[0x0402u], 0x03u);
    CHECK_EQ_U64(m->ram[0x0403u], 0x02u);
    CHECK_EQ_U64(m->ram[0x0404u], 0x01u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_a, 0x08u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);
    CHECK_EQ_U64(m->cpu.p & 0x04u, 0x04u); /* SEI still masks acceptance. */
    CHECK_EQ_U64(m->cpu.pc, 0x025Bu); /* Next instruction is CLI. */
    CHECK_EQ_U64(m->ram[0x01FBu], 0u);
    uint8_t pending[2] = {0xA5u, 0xA5u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD80u, pending, sizeof pending), LYNX_OK);
    CHECK_EQ_U64(pending[0], 0x01u);
    CHECK_EQ_U64(pending[1], 0x01u);

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0405u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK_EQ_U64(m->host_observation.watch_event.hit_tick + 1u, ticks_of(m));
    CHECK_EQ_U64(m->host_observation.watch_event.address, 0x0405u);
    CHECK_EQ_U64(m->ram[0x0405u], 0xA5u);
    CHECK_EQ_U64(m->cpu.pc, 0x0305u);
    CHECK_EQ_U64(m->cpu.s, 0xFAu);
    CHECK_EQ_U64(m->cpu.p & 0x04u, 0x04u); /* IRQ entry sets I after saving P. */
    CHECK_EQ_U64(m->ram[0x01FDu], 0x02u);
    CHECK_EQ_U64(m->ram[0x01FCu], 0x5Du); /* RTI returns to the guest JMP. */
    CHECK_EQ_U64(m->ram[0x01FBu] & 0x04u, 0u); /* Saved P had I clear after CLI. */
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);

    CHECK_EQ_U64(lynx_watch_configure(m, 0x0407u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK_EQ_U64(m->ram[0x0406u], 0x01u);
    CHECK_EQ_U64(m->ram[0x0407u], 0x00u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_a, 0x08u);
    CHECK_EQ_U64(lynx_advance(m, 30u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 30u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);
    CHECK_EQ_U64(m->cpu.pc, 0x025Du);
    if (check_failures != failures_before) {
        fprintf(stderr, "profile %s IRQ guest evidence failed\n", lynx_profile_name(profile));
    }

done:
    lynx_machine_deinit(m);
}

static void run_irq_disabled_source_profile(uint32_t profile)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, irq_disabled_source_program,
                             sizeof irq_disabled_source_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 3000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop == LYNX_STOP_WATCH) {
        CHECK(consumed != 0u && consumed < 3000u);
        CHECK_EQ_U64(m->ram[0x0400u], 0u);
        CHECK_EQ_U64(m->timer_bank.timers[0].control_a, 0x08u);
        CHECK_EQ_U64(m->timer_bank.timers[0].control_b & 0x08u, 0x08u);
        CHECK_EQ_U64(m->timer_bank.pending, 0u);
        CHECK_EQ_U64(m->cpu.s, 0xFDu);
        CHECK_EQ_U64(m->cpu.p & 0x04u, 0x04u);
        uint8_t pending[2] = {0xA5u, 0xA5u};
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD80u, pending, sizeof pending), LYNX_OK);
        CHECK_EQ_U64(pending[0], 0u);
        CHECK_EQ_U64(pending[1], 0u);
    }
    lynx_machine_deinit(m);
}

static void case_irq_pending(void)
{
    const int failures_before = check_failures;
    run_irq_guest_profile(LYNX_PROFILE_LYNX1);
    run_irq_guest_profile(LYNX_PROFILE_LYNX2);
    run_irq_disabled_source_profile(LYNX_PROFILE_LYNX1);
    run_irq_disabled_source_profile(LYNX_PROFILE_LYNX2);
    printf("TAP version 13\n1..1\n%s 1 - api.irq_pending\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void irq_timer_event_at_eighty(lynx_machine *m)
{
    m->timer_bank.timers[0].count = 4u;
    m->timer_bank.timers[0].control_a = 0x88u; /* IRQ + COUNT, source period 16. */
}

static void run_irq_poll_after_profile(uint32_t profile)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, irq_poll_boundary_program,
                             sizeof irq_poll_boundary_program, 0x0200u), LYNX_OK);
    m->mapctl = 0x08u;
    m->ram[0xFFFEu] = 0x00u;
    m->ram[0xFFFFu] = 0x03u;
    irq_timer_event_at_eighty(m);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 76u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 76u);
    CHECK_EQ_U64(ticks_of(m), 76u);
    CHECK_EQ_U64(m->cpu.pc, 0x0208u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0208u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);

    /* Target 80 is exclusive: neither the timer event nor the due CPU fetch
       is visible until the next call includes tick 80. */
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4u);
    CHECK_EQ_U64(ticks_of(m), 80u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0208u);

    /* At tick 80 the timer event precedes the in-flight NOP opcode fetch. The
       event is now pending, while the next cycle's POLL is still five ticks
       away; the seventh NOP's poll at 75 did not take this later event. */
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 81u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->cpu.pc, 0x0209u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0209u);
    CHECK_EQ_U64(m->cpu_ready_at, 85u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);

    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4u);
    CHECK_EQ_U64(ticks_of(m), 85u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0209u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 86u);
    CHECK_EQ_U64(m->cpu.req_kind, LYNX__CK_DUMMY_STREAM);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0209u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);

    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, 150u);
    CHECK_EQ_U64(m->host_observation.watch_event.return_tick, 151u);
    CHECK_EQ_U64(ticks_of(m), 151u);
    CHECK_EQ_U64(m->ram[0x0400u], 0xA5u);
    CHECK_EQ_U64(m->cpu.pc, 0x0305u);
    CHECK_EQ_U64(m->cpu.s, 0xFAu);
    CHECK_EQ_U64(m->ram[0x01FDu], 0x02u);
    CHECK_EQ_U64(m->ram[0x01FCu], 0x09u);
    CHECK_EQ_U64(m->ram[0x01FBu] & 0x04u, 0u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    lynx_machine_deinit(m);
}

static void run_irq_poll_same_tick_ack_profile(uint32_t profile)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, irq_poll_ack_program,
                             sizeof irq_poll_ack_program, 0x0200u), LYNX_OK);
    irq_timer_event_at_eighty(m);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 81u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, 81u);
    CHECK_EQ_U64(ticks_of(m), 81u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_b & 0x08u, 0x08u);
    CHECK_EQ_U64(m->cpu.pc, 0x020Bu);
    CHECK_EQ_U64(m->cpu.req_kind, LYNX__CK_OPCODE);
    CHECK_EQ_U64(m->cpu.req_addr, 0x020Bu);
    CHECK_EQ_U64(m->cpu_ready_at, 85u);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4u);
    CHECK_EQ_U64(ticks_of(m), 85u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->cpu.req_kind, LYNX__CK_OPCODE);
    CHECK_EQ_U64(m->cpu.s, 0xFDu);
    lynx_machine_deinit(m);
}

static void case_irq_poll(void)
{
    const int failures_before = check_failures;
    run_irq_poll_after_profile(LYNX_PROFILE_LYNX1);
    run_irq_poll_after_profile(LYNX_PROFILE_LYNX2);
    run_irq_poll_same_tick_ack_profile(LYNX_PROFILE_LYNX1);
    run_irq_poll_same_tick_ack_profile(LYNX_PROFILE_LYNX2);
    printf("TAP version 13\n1..1\n%s 1 - api.irq_poll\n",
           check_failures == failures_before ? "ok" : "not ok");
}

enum { SCHEDULER_EVENT_COUNT = 5, SCHEDULER_TIMER_SET_COUNT = 3 };

typedef struct scheduler_event_observation {
    uint64_t event_tick;
    uint8_t pending;
    uint8_t counts[SCHEDULER_TIMER_SET_COUNT];
    uint8_t ctlb[SCHEDULER_TIMER_SET_COUNT];
    uint8_t last_clock_mask;
} scheduler_event_observation;

typedef struct scheduler_test_candidate {
    lynx__machine_event event;
    uint8_t source;
} scheduler_test_candidate;

static bool check_event_candidate_permutation(const uint8_t order[9])
{
    scheduler_test_candidate candidates[9];
    uint8_t remaining[9];
    for (uint8_t timer = 0u; timer < LYNX__TIMER_COUNT; timer++) {
        candidates[timer].event.tick = 80u;
        candidates[timer].event.priority = (uint8_t)LYNX__MACHINE_EVENT_TIMER_PRIORITY;
        candidates[timer].event.sequence = timer;
        candidates[timer].source = timer;
    }
    candidates[8].event.tick = 80u;
    candidates[8].event.priority = (uint8_t)LYNX__MACHINE_EVENT_CPU_PRIORITY;
    candidates[8].event.sequence = (uint8_t)LYNX__MACHINE_EVENT_SEQUENCE;
    candidates[8].source = 8u;
    memcpy(remaining, order, sizeof remaining);

    for (size_t expected = 0u; expected < 9u; expected++) {
        size_t best = 0u;
        const size_t remaining_count = 9u - expected;
        for (size_t i = 1u; i < remaining_count; i++) {
            if (lynx__machine_event_before(candidates[remaining[i]].event,
                                           candidates[remaining[best]].event)) {
                best = i;
            }
        }
        const uint8_t selected = remaining[best];
        if (candidates[selected].source != expected) {
            CHECK_NAMED(false, "timer.order.timer-before-cpu-prefix");
            return false;
        }
        CHECK_EQ_U64(candidates[selected].event.tick, 80u);
        if (expected < LYNX__TIMER_COUNT) {
            CHECK_EQ_U64(candidates[selected].event.priority,
                         LYNX__MACHINE_EVENT_TIMER_PRIORITY);
            CHECK_EQ_U64(candidates[selected].event.sequence, expected);
        } else {
            CHECK_EQ_U64(candidates[selected].event.priority,
                         LYNX__MACHINE_EVENT_CPU_PRIORITY);
        }
        memmove(&remaining[best], &remaining[best + 1u],
                (remaining_count - best - 1u) * sizeof remaining[0]);
    }
    return true;
}

static void configure_scheduler_timer_set(lynx_machine *m, bool reverse)
{
    static const lynx__timer timers[SCHEDULER_TIMER_SET_COUNT] = {
        {0u, 4u, 0x98u, 0u}, /* T0: IRQ, reload, count, source 0. */
        {0u, 4u, 0x88u, 0u}, /* T1: IRQ, count, source 0. */
        {0u, 0u, 0x8Fu, 0u}, /* T2: IRQ, count, T0 link input. */
    };
    for (size_t n = 0u; n < SCHEDULER_TIMER_SET_COUNT; n++) {
        const size_t index = reverse ? SCHEDULER_TIMER_SET_COUNT - 1u - n : n;
        m->timer_bank.timers[index] = timers[index];
    }
}

static void read_scheduler_event_observation(const lynx_machine *m,
                                              scheduler_event_observation *observation,
                                              uint64_t event_tick)
{
    memset(observation, 0, sizeof *observation);
    observation->event_tick = event_tick;
    observation->pending = m->timer_bank.pending;
    for (size_t i = 0u; i < SCHEDULER_TIMER_SET_COUNT; i++) {
        observation->counts[i] = m->timer_bank.timers[i].count;
        observation->ctlb[i] = m->timer_bank.timers[i].control_b;
        if ((observation->ctlb[i] & 0x04u) != 0u) {
            observation->last_clock_mask =
                (uint8_t)(observation->last_clock_mask | (uint8_t)(1u << i));
        }
    }
}

static void run_scheduler_event_prefix(uint8_t chunk_mode, bool reverse_setup,
                                       scheduler_event_observation observations[SCHEDULER_EVENT_COUNT])
{
    static const uint8_t irregular_chunks[] = {3u, 1u, 7u, 2u, 5u};
    static const uint64_t event_ticks[SCHEDULER_EVENT_COUNT] = {16u, 32u, 48u, 64u, 80u};
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, scheduler_watch_program, sizeof scheduler_watch_program,
                             0x0200u), LYNX_OK);
    configure_scheduler_timer_set(m, reverse_setup);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    size_t irregular_at = 0u;
    for (size_t i = 0u; i < SCHEDULER_EVENT_COUNT; i++) {
        const uint64_t target = event_ticks[i] + 1u;
        while (ticks_of(m) < target) {
            const uint64_t remaining = target - ticks_of(m);
            uint64_t budget = remaining;
            if (chunk_mode == 1u) {
                budget = 1u;
            } else if (chunk_mode == 2u) {
                budget = irregular_chunks[irregular_at++ %
                                          (sizeof irregular_chunks / sizeof irregular_chunks[0])];
                if (budget > remaining) {
                    budget = remaining;
                }
            }
            CHECK_EQ_U64(lynx_advance(m, budget, &consumed, &stop), LYNX_OK);
            CHECK_EQ_U64(consumed, budget);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        }
        CHECK_EQ_U64(ticks_of(m), target);
        read_scheduler_event_observation(m, &observations[i], event_ticks[i]);
    }
    lynx_machine_deinit(m);
}

static void case_scheduler_order(void)
{
    static const uint8_t permutations[4][9] = {
        {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u},
        {8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u},
        {7u, 5u, 3u, 1u, 0u, 2u, 4u, 6u, 8u},
        {3u, 4u, 5u, 6u, 7u, 8u, 0u, 1u, 2u},
    };
    static const scheduler_event_observation expected[SCHEDULER_EVENT_COUNT] = {
        {16u, 0u, {3u, 3u, 0u}, {0x04u, 0x04u, 0u}, 0x03u},
        {32u, 0u, {2u, 2u, 0u}, {0x04u, 0x04u, 0u}, 0x03u},
        {48u, 0u, {1u, 1u, 0u}, {0x04u, 0x04u, 0u}, 0x03u},
        {64u, 0u, {0u, 0u, 0u}, {0x04u, 0x04u, 0u}, 0x03u},
        {80u, 0x07u, {0u, 0u, 0u}, {0x0Du, 0x0Du, 0x0Fu}, 0x07u},
    };
    const int failures_before = check_failures;
    scheduler_event_observation coarse[SCHEDULER_EVENT_COUNT];
    scheduler_event_observation one_tick[SCHEDULER_EVENT_COUNT];
    scheduler_event_observation irregular[SCHEDULER_EVENT_COUNT];

    for (size_t i = 0u; i < sizeof permutations / sizeof permutations[0]; i++) {
        if (!check_event_candidate_permutation(permutations[i])) {
            goto done;
        }
    }
    run_scheduler_event_prefix(0u, false, coarse);
    run_scheduler_event_prefix(1u, true, one_tick);
    run_scheduler_event_prefix(2u, false, irregular);
    CHECK(memcmp(coarse, expected, sizeof expected) == 0);
    CHECK(memcmp(one_tick, expected, sizeof expected) == 0);
    CHECK(memcmp(irregular, expected, sizeof expected) == 0);
    CHECK(memcmp(coarse, one_tick, sizeof coarse) == 0);
    CHECK(memcmp(coarse, irregular, sizeof coarse) == 0);
    CHECK_EQ_U64(SCHEDULER_EVENT_COUNT, 5u);
done:
    printf("TAP version 13\n1..1\n%s 1 - api.scheduler_order\n",
           check_failures == failures_before ? "ok" : "not ok");
}

typedef struct scheduler_watch_observation {
    uint64_t hit_tick;
    uint64_t return_tick;
    uint64_t guest_ticks;
    uint64_t cpu_ready_at;
    uint32_t watch_addr;
    uint32_t watch_value;
    uint16_t cpu_pc;
    uint16_t cpu_req_addr;
    uint8_t cpu_req_kind;
    uint8_t cpu_s;
    uint8_t cpu_p;
    uint8_t pending;
    uint8_t ram_value;
    uint8_t counts[SCHEDULER_TIMER_SET_COUNT];
    uint8_t ctlb[SCHEDULER_TIMER_SET_COUNT];
    uint8_t last_clock_mask;
} scheduler_watch_observation;

static void run_scheduler_watch(uint8_t chunk_mode, scheduler_watch_observation *observation)
{
    static const uint8_t irregular_chunks[] = {3u, 7u, 1u, 11u, 2u, 5u};
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, scheduler_watch_program, sizeof scheduler_watch_program,
                             0x0200u), LYNX_OK);
    configure_scheduler_timer_set(m, false);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    size_t irregular_at = 0u;
    for (size_t call = 0u; call < 200u && stop != LYNX_STOP_WATCH; call++) {
        uint64_t budget = chunk_mode == 0u ? 200u : 1u;
        if (chunk_mode == 2u) {
            budget = irregular_chunks[irregular_at++ %
                                      (sizeof irregular_chunks / sizeof irregular_chunks[0])];
        }
        CHECK_EQ_U64(lynx_advance(m, budget, &consumed, &stop), LYNX_OK);
        CHECK(consumed != 0u && consumed <= budget);
    }
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    memset(observation, 0, sizeof *observation);
    observation->hit_tick = m->host_observation.watch_event.hit_tick;
    observation->return_tick = m->host_observation.watch_event.return_tick;
    observation->guest_ticks = ticks_of(m);
    observation->cpu_ready_at = m->cpu_ready_at;
    observation->watch_addr = m->host_observation.watch_event.address;
    observation->watch_value = m->host_observation.watch_event.value;
    observation->cpu_pc = m->cpu.pc;
    observation->cpu_req_addr = m->cpu.req_addr;
    observation->cpu_req_kind = m->cpu.req_kind;
    observation->cpu_s = m->cpu.s;
    observation->cpu_p = m->cpu.p;
    observation->pending = m->timer_bank.pending;
    observation->ram_value = m->ram[0x0400u];
    for (size_t i = 0u; i < SCHEDULER_TIMER_SET_COUNT; i++) {
        observation->counts[i] = m->timer_bank.timers[i].count;
        observation->ctlb[i] = m->timer_bank.timers[i].control_b;
        if ((observation->ctlb[i] & 0x04u) != 0u) {
            observation->last_clock_mask =
                (uint8_t)(observation->last_clock_mask | (uint8_t)(1u << i));
        }
    }

    CHECK_EQ_U64(observation->hit_tick, 80u);
    CHECK_EQ_U64(observation->return_tick, 81u);
    CHECK_EQ_U64(observation->guest_ticks, 81u);
    CHECK_EQ_U64(observation->cpu_ready_at, 85u);
    CHECK_EQ_U64(observation->watch_addr, 0x0400u);
    CHECK_EQ_U64(observation->watch_value, 0xA5u);
    CHECK_EQ_U64(observation->cpu_pc, 0x020Bu);
    CHECK_EQ_U64(observation->cpu_req_addr, 0x020Bu);
    CHECK_EQ_U64(observation->cpu_req_kind, LYNX__CK_DUMMY_STREAM);
    CHECK_EQ_U64(observation->cpu_s, 0xFDu);
    CHECK_EQ_U64(observation->cpu_p & 0x04u, 0u);
    CHECK_EQ_U64(observation->pending, 0x07u);
    CHECK_EQ_U64(observation->ram_value, 0xA5u);
    CHECK_EQ_U64(observation->counts[0], 0u);
    CHECK_EQ_U64(observation->counts[1], 0u);
    CHECK_EQ_U64(observation->counts[2], 0u);
    CHECK_EQ_U64(observation->ctlb[0], 0x0Du);
    CHECK_EQ_U64(observation->ctlb[1], 0x0Du);
    CHECK_EQ_U64(observation->ctlb[2], 0x0Fu);
    CHECK_EQ_U64(observation->last_clock_mask, 0x07u);
    lynx_machine_deinit(m);
}

static void case_scheduler_watch(void)
{
    const int failures_before = check_failures;
    scheduler_watch_observation coarse;
    scheduler_watch_observation one_tick;
    scheduler_watch_observation irregular;
    run_scheduler_watch(0u, &coarse);
    run_scheduler_watch(1u, &one_tick);
    run_scheduler_watch(2u, &irregular);
    CHECK(memcmp(&coarse, &one_tick, sizeof coarse) == 0);
    CHECK(memcmp(&coarse, &irregular, sizeof coarse) == 0);
    printf("TAP version 13\n1..1\n%s 1 - api.scheduler_watch\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_scheduler_limits(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    m->timer_bank.timers[0].count = 0u;
    m->timer_bank.timers[0].control_a = 0x88u;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 15u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 15u);
    CHECK_EQ_U64(ticks_of(m), 15u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_UNCHANGED(m, snap_a);

    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 16u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u); /* event exactly at target is exclusive */
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(ticks_of(m), 17u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u); /* target + 1 includes tick 16 */
    CHECK_EQ_U64(m->timer_bank.timers[0].control_b, 0x0Du);

    const uint64_t idle_budget = UINT64_C(1) << 40;
    CHECK_EQ_U64(lynx_advance(m, idle_budget, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, idle_budget);
    CHECK_EQ_U64(ticks_of(m), idle_budget + 17u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);

    const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
    m->guest_ticks = limit - 7u;
    m->cpu_ready_at = m->guest_ticks;
    CHECK_EQ_U64(lynx_advance(m, UINT64_MAX, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 7u);
    CHECK_EQ_U64(ticks_of(m), limit);
    CHECK_EQ_U64(lynx_advance(m, UINT64_MAX, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(ticks_of(m), limit);
    lynx_machine_deinit(m);

    /* A real posted guest store is positioned at a near-limit grant. The
       timer edge is due at that grant; the watch returns at grant+1 without
       crossing the guest-time cap or overflowing the CPU-ready value. */
    m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 25u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 25u);
    CHECK_EQ_U64(m->cpu.req_addr, 0x0400u);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    const uint64_t grant = limit - 15u; /* limit is 15 mod the 16-tick period. */
    m->guest_ticks = grant;
    m->cpu_ready_at = grant;
    m->timer_bank.timers[0].count = 0u;
    m->timer_bank.timers[0].control_a = 0x88u;
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, grant);
    CHECK_EQ_U64(m->host_observation.watch_event.return_tick, grant + 1u);
    CHECK_EQ_U64(ticks_of(m), grant + 1u);
    CHECK(ticks_of(m) <= limit);
    CHECK_EQ_U64(m->cpu_ready_at, grant + 5u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    lynx_machine_deinit(m);

    printf("TAP version 13\n1..1\n%s 1 - api.scheduler_limits\n",
           check_failures == 0 ? "ok" : "not ok");
}

enum {
    TIMER_CTLB_CLEAR_BEFORE = 0u,
    TIMER_CTLB_CLEAR_ON = 1u,
    TIMER_CTLB_CLEAR_AFTER = 2u,
};

static void run_timer_ctlb_boundary_case(uint8_t relation)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        CHECK(m != NULL);
        return;
    }
    CHECK_EQ_U64(load_bll_at(m, timer_ctlb_boundary_program,
                             sizeof timer_ctlb_boundary_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFD03u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    bool found_clear = false;
    for (size_t i = 0u; i < 1024u && ticks_of(m) < 1024u; i++) {
        if (m->cpu.req_addr == 0xFD03u && m->cpu.req_data == 0u &&
            m->cpu_ready_at <= 1024u) {
            found_clear = true;
            break;
        }
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        if (stop != LYNX_STOP_BUDGET) {
            break;
        }
    }
    CHECK(found_clear);
    if (!found_clear) {
        goto done;
    }
    CHECK_EQ_U64(m->cpu.req_addr, 0xFD03u);
    CHECK_EQ_U64(m->cpu.req_data, 0u);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    uint64_t event_tick = 0u;
    CHECK(lynx__timer_next(&m->timer_bank, ticks_of(m), &event_tick));
    CHECK_EQ_U64(event_tick, 1024u);

    if (relation == TIMER_CTLB_CLEAR_BEFORE) {
        CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK(m->host_observation.watch_event.hit_tick < event_tick);
        uint8_t ctlb = 0xA5u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, &ctlb, 1u), LYNX_OK);
        CHECK_EQ_U64(ctlb, 0u);
        CHECK_EQ_U64(m->timer_bank.pending, 0u);

        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        const uint64_t now = ticks_of(m);
        CHECK_EQ_U64(lynx_advance(m, event_tick - now + 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(ticks_of(m), event_tick + 1u);
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, &ctlb, 1u), LYNX_OK);
        CHECK_EQ_U64(ctlb, 0x0Du);
        CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    } else if (relation == TIMER_CTLB_CLEAR_ON) {
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        const uint64_t now = ticks_of(m);
        CHECK_EQ_U64(lynx_advance(m, event_tick - now, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(ticks_of(m), event_tick);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, event_tick);
        CHECK_EQ_U64(m->host_observation.watch_event.return_tick, event_tick + 1u);
        uint8_t ctlb = 0xA5u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, &ctlb, 1u), LYNX_OK);
        CHECK_EQ_U64(ctlb, 0u);
        CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    } else {
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        const uint64_t now = ticks_of(m);
        CHECK_EQ_U64(lynx_advance(m, event_tick - now, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(ticks_of(m), event_tick);
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(ticks_of(m), event_tick + 1u);
        uint8_t ctlb = 0xA5u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, &ctlb, 1u), LYNX_OK);
        CHECK_EQ_U64(ctlb, 0x0Du);
        CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, event_tick + 1u);
        CHECK_EQ_U64(m->host_observation.watch_event.return_tick, event_tick + 2u);
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD03u, &ctlb, 1u), LYNX_OK);
        CHECK_EQ_U64(ctlb, 0u);
        CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    }

done:
    lynx_machine_deinit(m);
}

static void case_timer_done_borrow(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(load_bll_at(m, timer_done_borrow_program,
                             sizeof timer_done_borrow_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0401u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK(consumed != 0u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x7Fu);
    CHECK_EQ_U64(m->ram[0x0401u], 0x08u);
    if (check_failures != failures_before) {
        goto done;
    }
    lynx_machine_deinit(m);
    m = init_in(machine_buf, &cfg);
    CHECK(m != NULL);
    if (m == NULL) {
        goto done;
    }

    CHECK_EQ_U64(load_bll_at(m, timer_reset_done_program,
                             sizeof timer_reset_done_program, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFD0Du, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    uint8_t timer[4] = {0u, 0u, 0u, 0u};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD0Cu, timer, sizeof timer), LYNX_OK);
    CHECK_EQ_U64(timer[1], 0xC8u);
    CHECK_EQ_U64(timer[3] & 0x08u, 0u);

    /* The same watch now observes RESET-DONE being released after many
       source edges; DONE must still be clear until a later zero-count edge. */
    CHECK_EQ_U64(lynx_watch_configure(m, 0xFD0Du, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD0Cu, timer, sizeof timer), LYNX_OK);
    CHECK_EQ_U64(timer[1], 0x88u);
    CHECK_EQ_U64(timer[3] & 0x08u, 0u);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 17u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 17u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD0Cu, timer, sizeof timer), LYNX_OK);
    CHECK_EQ_U64(timer[3] & 0x08u, 0x08u);

    /* The same guest CTLB clear is observed immediately before, at, and just
       after the expiry tick; at-tick timer work precedes the CPU write. */
    lynx_machine_deinit(m);
    m = NULL;
    run_timer_ctlb_boundary_case(TIMER_CTLB_CLEAR_BEFORE);
    run_timer_ctlb_boundary_case(TIMER_CTLB_CLEAR_ON);
    run_timer_ctlb_boundary_case(TIMER_CTLB_CLEAR_AFTER);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_done_borrow\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_link_tracer(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(load_bll_at(m, timer_link_tracer_program,
                             sizeof timer_link_tracer_program, 0x0200u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    bool found_read = false;
    for (size_t i = 0u; i < 1024u && ticks_of(m) < 1024u; i++) {
        if (m->cpu.req_addr == 0xFD17u && m->cpu_ready_at <= 1024u) {
            found_read = true;
            break;
        }
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        if (stop != LYNX_STOP_BUDGET) {
            break;
        }
    }
    CHECK(found_read);
    if (!found_read) {
        goto done;
    }
    CHECK_EQ_U64(m->timer_bank.timers[3].count, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[5].count, 0u);

    /* Leave the mapped T5 CTLB read pending until tick 1024. The source
       timer event and CPU bus action then share a tick, with timer work first. */
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1024u - ticks_of(m), &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), 1024u);
    CHECK_EQ_U64(m->timer_bank.timers[3].count, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[5].control_b & 0x07u, 0u);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(m->cpu.a, 0x07u);
    CHECK_EQ_U64(m->timer_bank.timers[3].count, 1u);
    CHECK_EQ_U64(m->timer_bank.timers[3].control_b, 0x0Du);
    CHECK_EQ_U64(m->timer_bank.timers[5].count, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[5].control_b, 0x07u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x28u);

    CHECK_EQ_U64(lynx_advance(m, 100u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(m->ram[0x0400u], 0x07u);
    CHECK_EQ_U64(m->ram[0x0401u], 0x28u);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_link_tracer\n",
           check_failures == failures_before ? "ok" : "not ok");
}

typedef struct timer_link_setup {
    uint8_t index;
    uint8_t backup;
    uint8_t count;
    uint8_t control_a;
} timer_link_setup;

static size_t make_timer_link_guest(uint8_t *program, const timer_link_setup *setups,
                                    size_t setup_count)
{
    size_t length = 0u;
    program[length++] = 0x78u; /* SEI: keep source IRQ latches observable */
    for (size_t i = 0u; i < setup_count; i++) {
        const uint16_t base = (uint16_t)(0xFD00u + 4u * setups[i].index);
        program[length++] = 0xA9u;
        program[length++] = setups[i].backup;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)base;
        program[length++] = (uint8_t)(base >> 8);
        program[length++] = 0xA9u;
        program[length++] = setups[i].count;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)(base + 2u);
        program[length++] = (uint8_t)((base + 2u) >> 8);
        program[length++] = 0xA9u;
        program[length++] = setups[i].control_a;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)(base + 1u);
        program[length++] = (uint8_t)((base + 1u) >> 8);
    }
    const uint16_t loop = (uint16_t)(0x0200u + (uint16_t)length);
    program[length++] = 0x4Cu;
    program[length++] = (uint8_t)loop;
    program[length++] = (uint8_t)(loop >> 8);
    return length;
}

static void run_timer_link_scenario(const timer_link_setup *setups, size_t setup_count,
                                    uint8_t root, size_t event_count,
                                    uint8_t expected_mask, const uint8_t expected_counts[8],
                                    const uint8_t expected_ctlb[8], uint8_t expected_pending)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        CHECK(m != NULL);
        return;
    }
    uint8_t program[160];
    const size_t program_len = make_timer_link_guest(program, setups, setup_count);
    const uint16_t root_control = (uint16_t)(0xFD01u + 4u * root);
    CHECK_EQ_U64(load_bll_at(m, program, program_len, 0x0200u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(m, root_control, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 2000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }

    /* Apply exactly the requested number of source events while preserving
       the posted CPU request. Each event drains all links in its prefix. */
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    for (size_t event = 0u; event < event_count; event++) {
        uint64_t event_tick = 0u;
        const uint64_t now = ticks_of(m);
        const bool has_event = lynx__timer_next(&m->timer_bank, now, &event_tick);
        CHECK(has_event);
        if (!has_event) {
            break;
        }
        CHECK(event_tick >= now);
        if (event_tick < now) {
            break;
        }
        const uint64_t budget = event_tick - now + 1u;
        CHECK_EQ_U64(lynx_advance(m, budget, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, budget);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }

    uint8_t mapped[LYNX__TIMER_COUNT * 4u];
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, mapped, sizeof mapped), LYNX_OK);
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        if ((expected_mask & (uint8_t)(1u << i)) != 0u) {
            CHECK_EQ_U64(mapped[i * 4u + 2u], expected_counts[i]);
            CHECK_EQ_U64(mapped[i * 4u + 3u], expected_ctlb[i]);
        }
    }
    uint8_t pending[2] = {0xA5u, 0x5Au};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD80u, pending, sizeof pending), LYNX_OK);
    CHECK_EQ_U64(pending[0], expected_pending);
    CHECK_EQ_U64(pending[1], expected_pending);

done:
    lynx_machine_deinit(m);
}

static void case_timer_links(void)
{
    static const timer_link_setup link02[] = {{2u, 0u, 1u, 0x8Fu}, {0u, 1u, 0u, 0x98u}};
    static const timer_link_setup link24[] = {{4u, 0u, 1u, 0x8Fu}, {2u, 1u, 0u, 0x98u}};
    static const timer_link_setup chain024[] = {
        {4u, 0u, 0u, 0x8Fu}, {2u, 1u, 0u, 0x9Fu}, {0u, 1u, 0u, 0x98u}};
    static const timer_link_setup link13[] = {{3u, 0u, 1u, 0x8Fu}, {1u, 1u, 0u, 0x98u}};
    static const timer_link_setup link35[] = {{5u, 0u, 1u, 0x8Fu}, {3u, 1u, 0u, 0x98u}};
    static const timer_link_setup link57[] = {{7u, 0u, 1u, 0x8Fu}, {5u, 1u, 0u, 0x98u}};
    static const timer_link_setup chain1357[] = {
        {7u, 0u, 1u, 0x8Fu}, {5u, 1u, 0u, 0x9Fu},
        {3u, 1u, 0u, 0x9Fu}, {1u, 1u, 0u, 0x98u}};
    static const timer_link_setup simultaneous[] = {
        {2u, 0u, 1u, 0x8Fu}, {0u, 1u, 0u, 0x9Eu}, {1u, 0u, 0u, 0x8Eu}};
    static const timer_link_setup count_255[] = {{0u, 0xFFu, 0xFFu, 0x88u}};
    static const timer_link_setup reload_0_to_255[] = {{0u, 0xFFu, 0u, 0x98u}};
    static const uint8_t expected02_count[8] = {1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected02_ctlb[8] = {0x0Du, 0u, 0x06u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected24_count[8] = {0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected24_ctlb[8] = {0u, 0u, 0x0Du, 0u, 0x06u, 0u, 0u, 0u};
    static const uint8_t expected024_count[8] = {1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected024_ctlb[8] = {0x0Du, 0u, 0x0Fu, 0u, 0x0Fu, 0u, 0u, 0u};
    static const uint8_t expected13_count[8] = {0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected13_ctlb[8] = {0u, 0x0Du, 0u, 0x06u, 0u, 0u, 0u, 0u};
    static const uint8_t expected35_count[8] = {0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u};
    static const uint8_t expected35_ctlb[8] = {0u, 0u, 0u, 0x0Du, 0u, 0x06u, 0u, 0u};
    static const uint8_t expected57_count[8] = {0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u};
    static const uint8_t expected57_ctlb[8] = {0u, 0u, 0u, 0u, 0u, 0x0Du, 0u, 0x06u};
    static const uint8_t expected1357_count[8] = {0u, 1u, 0u, 1u, 0u, 1u, 0u, 0u};
    static const uint8_t expected1357_ctlb[8] = {0u, 0x0Du, 0u, 0x0Fu, 0u, 0x0Fu, 0u, 0x06u};
    static const uint8_t expected_simultaneous_count[8] = {1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected_simultaneous_ctlb[8] = {0x0Du, 0x0Du, 0x06u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected255_count[8] = {0xFEu, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected255_ctlb[8] = {0x04u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected_reload_count[8] = {0xFEu, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t expected_reload_ctlb[8] = {0x0Cu, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const int failures_before = check_failures;
    size_t scenarios_run = 0u;

    run_timer_link_scenario(link02, sizeof link02 / sizeof link02[0], 0u, 1u,
                            0x05u, expected02_count, expected02_ctlb, 0x01u);
    scenarios_run++;
    run_timer_link_scenario(link24, sizeof link24 / sizeof link24[0], 2u, 1u,
                            0x14u, expected24_count, expected24_ctlb, 0x04u);
    scenarios_run++;
    run_timer_link_scenario(chain024, sizeof chain024 / sizeof chain024[0], 0u, 1u,
                            0x15u, expected024_count, expected024_ctlb, 0x05u);
    scenarios_run++;
    run_timer_link_scenario(link13, sizeof link13 / sizeof link13[0], 1u, 1u,
                            0x0Au, expected13_count, expected13_ctlb, 0x02u);
    scenarios_run++;
    run_timer_link_scenario(link35, sizeof link35 / sizeof link35[0], 3u, 1u,
                            0x28u, expected35_count, expected35_ctlb, 0x08u);
    scenarios_run++;
    run_timer_link_scenario(link57, sizeof link57 / sizeof link57[0], 5u, 1u,
                            0xA0u, expected57_count, expected57_ctlb, 0x20u);
    scenarios_run++;
    run_timer_link_scenario(chain1357, sizeof chain1357 / sizeof chain1357[0], 1u, 1u,
                            0xAAu, expected1357_count, expected1357_ctlb, 0x2Au);
    scenarios_run++;
    run_timer_link_scenario(simultaneous, sizeof simultaneous / sizeof simultaneous[0],
                            1u, 1u, 0x07u, expected_simultaneous_count,
                            expected_simultaneous_ctlb, 0x03u);
    scenarios_run++;
    run_timer_link_scenario(count_255, sizeof count_255 / sizeof count_255[0],
                            0u, 1u, 0x01u, expected255_count, expected255_ctlb, 0u);
    scenarios_run++;
    run_timer_link_scenario(reload_0_to_255,
                            sizeof reload_0_to_255 / sizeof reload_0_to_255[0],
                            0u, 2u, 0x01u, expected_reload_count, expected_reload_ctlb, 0x01u);
    scenarios_run++;

    /* timer4 has no timer-expiry IRQ source even when enabled. */
    CHECK_EQ_U64(scenarios_run, 10u);
    printf("TAP version 13\n1..1\n%s 1 - api.timer_links\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static size_t make_unavailable_guest(uint8_t *program, uint16_t addr, uint8_t value,
                                     bool disabled_link_first)
{
    size_t length = 0u;
    program[length++] = 0xA9u;
    program[length++] = 0xA5u;
    program[length++] = 0x8Du;
    program[length++] = 0x00u;
    program[length++] = 0x04u;
    if (disabled_link_first) {
        program[length++] = 0xA9u;
        program[length++] = 0x07u;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)addr;
        program[length++] = (uint8_t)(addr >> 8);
    }
    program[length++] = 0xA9u;
    program[length++] = value;
    program[length++] = 0x8Du;
    program[length++] = (uint8_t)addr;
    program[length++] = (uint8_t)(addr >> 8);
    return length;
}

static void expect_unavailable_guest_write(uint16_t addr, uint8_t value,
                                           bool disabled_link_first,
                                           bool is_timer_control, uint8_t timer_index,
                                           uint8_t expected_control_a)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        CHECK(m != NULL);
        return;
    }
    uint8_t program[32];
    const size_t program_len = make_unavailable_guest(program, addr, value,
                                                      disabled_link_first);
    CHECK_EQ_U64(load_bll_at(m, program, program_len, 0x0200u), LYNX_OK);
    /* Poison hidden RAM so a mistaken fallback write is observable. */
    m->ram[addr] = 0xE7u;

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_EQ_U64(consumed, disabled_link_first ? 85u : 55u);
    CHECK_EQ_U64(m->ram[0x0400u], 0xA5u);
    CHECK_EQ_U64(m->ram[addr], 0xE7u);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->cpu.req_data, value);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    if (is_timer_control) {
        CHECK_EQ_U64(m->timer_bank.timers[timer_index].control_a, expected_control_a);
    }

    /* Retrying the refused request consumes no new prefix and leaves the
       action pending and unapplied. */
    CHECK_EQ_U64(lynx_advance(m, 10u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->cpu.req_data, value);
    CHECK_EQ_U64(m->ram[addr], 0xE7u);
    if (is_timer_control) {
        CHECK_EQ_U64(m->timer_bank.timers[timer_index].control_a, expected_control_a);
    }
    lynx_machine_deinit(m);
}

static void case_timer_unavailable(void)
{
    const int failures_before = check_failures;
    size_t cases_run = 0u;

    /* No documented incoming link exists for timer 0 or 6; timer 1's source
       would be absent audio 3. Enabled selections refuse before CTLA mutation. */
    expect_unavailable_guest_write(0xFD01u, 0x0Fu, false, true, 0u, 0u);
    cases_run++;
    expect_unavailable_guest_write(0xFD05u, 0x0Fu, false, true, 1u, 0u);
    cases_run++;
    expect_unavailable_guest_write(0xFD19u, 0x0Fu, false, true, 6u, 0u);
    cases_run++;

    /* A disabled selector is retained; enabling COUNT with that selector is
       still refused before the second write can change the register. */
    expect_unavailable_guest_write(0xFD01u, 0x0Fu, true, true, 0u, 0x07u);
    cases_run++;
    expect_unavailable_guest_write(0xFD19u, 0x0Fu, true, true, 6u, 0x07u);
    cases_run++;

    /* Unimplemented Mikey audio and Suzy addresses refuse through the guest
       bus, preserving both the pending store and hidden backing RAM. */
    expect_unavailable_guest_write(0xFD20u, 0x5Au, false, false, 0u, 0u);
    cases_run++;
    expect_unavailable_guest_write(0xFC00u, 0x5Au, false, false, 0u, 0u);
    cases_run++;

    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        uint8_t crossing[2] = {0xA5u, 0x5Au};
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD1Fu, crossing, sizeof crossing),
                     LYNX_ERR_UNAVAILABLE);
        CHECK_EQ_U64(crossing[0], 0xA5u);
        CHECK_EQ_U64(crossing[1], 0x5Au);
        lynx_machine_deinit(m);
    } else {
        CHECK(m != NULL);
    }

    CHECK_EQ_U64(cases_run, 7u);
    printf("TAP version 13\n1..1\n%s 1 - api.timer_unavailable\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static size_t make_timer_bank_guest(uint8_t *program)
{
    size_t length = 0u;
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        const uint8_t base = (uint8_t)(0x00u + 4u * (uint8_t)i);
        const uint8_t backup = (uint8_t)(0xF0u + (uint8_t)i);
        const uint8_t count = (uint8_t)(0xE0u + (uint8_t)i);
        const uint8_t source = i < 7u ? (uint8_t)i : 0u;
        program[length++] = 0xA9u;
        program[length++] = backup;
        program[length++] = 0x8Du;
        program[length++] = base;
        program[length++] = 0xFDu;
        program[length++] = 0xA9u;
        program[length++] = count;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)(base + 2u);
        program[length++] = 0xFDu;
        program[length++] = 0xA9u;
        program[length++] = source;
        program[length++] = 0x8Du;
        program[length++] = (uint8_t)(base + 1u);
        program[length++] = 0xFDu;
    }
    program[length++] = 0xA9u;
    program[length++] = 0x5Au;
    program[length++] = 0x8Du;
    program[length++] = 0x00u;
    program[length++] = 0x04u;
    return length;
}

static bool timer_write_required(lynx__timer_bank *bank, uint16_t addr, uint8_t value);

static void case_timer_standalone(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    uint8_t program[160];
    const size_t program_len = make_timer_bank_guest(program);
    CHECK_EQ_U64(load_bll_at(m, program, program_len, 0x0200u), LYNX_OK);
    for (size_t i = 0u; i < LYNX__TIMER_COUNT * 4u; i++) {
        m->ram[0xFD00u + i] = (uint8_t)(0x50u + (uint8_t)i);
    }
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    CHECK_EQ_U64(lynx_advance(m, 2000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    if (stop != LYNX_STOP_WATCH) {
        goto done;
    }
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK(consumed != 0u);
    uint8_t mapped[LYNX__TIMER_COUNT * 4u];
    memset(mapped, 0xA5, sizeof mapped);
    const uint64_t before_peek = ticks_of(m);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, mapped, sizeof mapped), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), before_peek);
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        const uint8_t source = i < 7u ? (uint8_t)i : 0u;
        CHECK_EQ_U64(mapped[i * 4u], 0xF0u + (uint8_t)i);
        CHECK_EQ_U64(mapped[i * 4u + 1u], source);
        CHECK_EQ_U64(mapped[i * 4u + 2u], 0xE0u + (uint8_t)i);
        CHECK_EQ_U64(mapped[i * 4u + 3u], 0u);
        CHECK_EQ_U64(m->ram[0xFD00u + i * 4u], 0x50u + (uint8_t)(i * 4u));
    }
    uint8_t crossing[2] = {0xA5u, 0x5Au};
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD1Fu, crossing, sizeof crossing),
                 LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_U64(crossing[0], 0xA5u);
    CHECK_EQ_U64(crossing[1], 0x5Au);

    /* Register state belongs to the instance and reset/load return it to the
       declared fixed-zero phase policy. */
    lynx_machine *other = init_in(buf_b, &cfg);
    if (other != NULL) {
        uint8_t other_timers[LYNX__TIMER_COUNT * 4u];
        CHECK_EQ_U64(lynx_read_cpu_memory(other, 0xFD00u, other_timers,
                                         sizeof other_timers), LYNX_OK);
        for (size_t i = 0u; i < sizeof other_timers; i++) {
            CHECK_EQ_U64(other_timers[i], 0u);
        }
        lynx_machine_deinit(other);
    }
    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    memset(mapped, 0xA5, sizeof mapped);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, mapped, sizeof mapped), LYNX_OK);
    for (size_t i = 0u; i < sizeof mapped; i++) {
        CHECK_EQ_U64(mapped[i], 0u);
    }
    CHECK_EQ_U64(m->timer_bank.phase_policy, LYNX__TIMER_PHASE_FIXED_ZERO_1);
    CHECK(timer_write_required(&m->timer_bank, 0xFD1Cu, 0xA5u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD81u, 0x80u));
    CHECK_EQ_U64(load_bll_at(m, timer_tracer_program, sizeof timer_tracer_program,
                             0x0200u), LYNX_OK);
    memset(mapped, 0xA5, sizeof mapped);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD00u, mapped, sizeof mapped), LYNX_OK);
    for (size_t i = 0u; i < sizeof mapped; i++) {
        CHECK_EQ_U64(mapped[i], 0u);
    }
    CHECK_EQ_U64(lynx__timer_peek(&m->timer_bank, 0xFD80u), 0u);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_standalone\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static uint16_t timer_register_addr(size_t timer_index, uint8_t reg)
{
    return (uint16_t)(0xFD00u + 4u * (uint16_t)timer_index + reg);
}

static bool timer_write_required(lynx__timer_bank *bank, uint16_t addr, uint8_t value)
{
    if (lynx__timer_write(bank, addr, value)) {
        return true;
    }
    fprintf(stderr, "timer bank rejected supported write at $%04X\n", (unsigned)addr);
    check_failures++;
    return false;
}

/* A held grant preserves a real BLL store while the shared timer clock keeps
   advancing. This is the RED tracer for expired readiness: cpu_ready_at is
   currently left behind by an idle hold, so the public remaining-cost byte
   underflows after the target passes it. */
static void case_request_hold(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    lynx_machine *other = NULL;
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(m->content_format, LYNX_CONTENT_BS93);

    other = init_in(buf_b, &cfg);
    if (other == NULL) {
        CHECK(other != NULL);
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(other, bll_tracer, sizeof bll_tracer), LYNX_OK);

    /* Timer 0 produces its IRQ on the fourth source edge (tick 64), after the
       guest has posted its store to $0400 and before ownership is released. */
    CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 3u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 3u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    for (uint32_t i = 0u; i < 100u &&
                           !(m->cpu.req_addr == 0x0400u &&
                             (m->cpu.req_flags & LYNX__CF_WRITE) != 0u);
         i++) {
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }
    CHECK_EQ_U64(m->cpu.req_addr, 0x0400u);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    CHECK(m->cpu_ready_at > m->guest_ticks);
    m->cpu.p = (uint8_t)(m->cpu.p | 0x04u); /* I masks acceptance, not timer wake. */
    const lynx__cpu request_before = m->cpu;
    const uint64_t target = m->guest_ticks + 40u;
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 40u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 40u);
    CHECK_EQ_U64(ticks_of(m), target);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->wake_pending, 1u);
    CHECK_EQ_U64(m->cpu.p & 0x04u, 0x04u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) == 0);
    CHECK_EQ_U64(m->ram[0x0400u], 0u);
    lynx_watch_event event;
    memset(&event, 0xA5, sizeof event);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 0u);
    uint32_t cpu_state = UINT32_MAX;
    CHECK_EQ_U64(lynx_read_register(m, LYNX_REG_CPU_STATE, &cpu_state), LYNX_OK);
    CHECK_EQ_HEX(cpu_state >> 24, 0u);
    CHECK_EQ_U64(m->cpu_ready_at, m->guest_ticks);

    CHECK_EQ_U64(other->guest_ticks, 0u);
    CHECK_EQ_U64(other->timer_bank.pending, 0u);
    CHECK_EQ_U64(other->bus_hold, 0u);
    CHECK_EQ_U64(other->ram[0x0400u], 0u);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->ram[0x0400u], 0xA5u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) != 0);
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_EQ_U64(event.hit, 1u);
    CHECK_EQ_U64(event.address, 0x0400u);
    CHECK_EQ_U64(event.value, 0xA5u);
    CHECK_EQ_U64(event.hit_tick, target);
    CHECK_EQ_U64(event.return_tick, target + 1u);
    CHECK_EQ_U64(lynx_advance(m, 4u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 4u);
    CHECK_EQ_U64(m->ram[0x0400u], 0xA5u);

done:
    if (other != NULL) {
        lynx_machine_deinit(other);
    }
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.request_hold\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void run_request_wake_case(bool mask_irq)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        CHECK(m != NULL);
        return;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 3u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 3u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    for (uint32_t i = 0u; i < 100u &&
                           !(m->cpu.req_addr == 0x0400u &&
                             (m->cpu.req_flags & LYNX__CF_WRITE) != 0u);
         i++) {
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }
    CHECK_EQ_U64(m->cpu.req_addr, 0x0400u);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_POLL, LYNX__CF_POLL);
    CHECK(m->guest_ticks < 64u);
    m->cpu.p = mask_irq ? (uint8_t)(m->cpu.p | 0x04u) :
                          (uint8_t)(m->cpu.p & (uint8_t)~0x04u);
    const lynx__cpu request_before = m->cpu;
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);

    /* The half-open call stops at 64 before the timer edge. Releasing and
       advancing one tick makes the timer event and CPU store contend at 64. */
    if (m->guest_ticks < 64u) {
        const uint64_t to_boundary = 64u - m->guest_ticks;
        CHECK_EQ_U64(lynx_advance(m, to_boundary, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, to_boundary);
    }
    CHECK_EQ_U64(m->guest_ticks, 64u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(m->wake_pending, 0u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) == 0);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->wake_pending, 1u);
    CHECK_EQ_U64(m->ram[0x0400u], 0xA5u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, 64u);
    CHECK_EQ_U64(m->host_observation.watch_event.return_tick, 65u);
    CHECK_EQ_U64(m->cpu.p & 0x04u, mask_irq ? 0x04u : 0u);
    CHECK_EQ_U64(m->cpu.flags & LYNX__CS_SEQ_MASK, mask_irq ? 0u : 0x10u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) != 0);

    lynx_machine_deinit(m);
}

static void case_request_wake(void)
{
    const int failures_before = check_failures;
    run_request_wake_case(true);
    run_request_wake_case(false);
    CHECK_EQ_U64(check_failures, failures_before);
    printf("TAP version 13\n1..1\n%s 1 - api.request_wake\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_request_boundary(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_cost_model(m, LYNX__COST_TEST_SKEW), LYNX_OK);
    const lynx__cpu opcode_request = m->cpu;
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK(memcmp(&opcode_request, &m->cpu, sizeof opcode_request) == 0);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->guest_ticks, 1u);
    CHECK_EQ_U64(m->cpu_ready_at, m->guest_ticks);
    CHECK(memcmp(&opcode_request, &m->cpu, sizeof opcode_request) == 0);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 0u);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(m->guest_ticks, 2u);
    CHECK_EQ_U64(m->cpu_ready_at, 5u); /* test_skew opcode cycle ends at tick 5 */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 3u);

    const lynx__cpu operand_request = m->cpu;
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx_advance(m, 10u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 10u);
    CHECK_EQ_U64(m->guest_ticks, 12u);
    CHECK_EQ_U64(m->cpu_ready_at, m->guest_ticks);
    CHECK(memcmp(&operand_request, &m->cpu, sizeof operand_request) == 0);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 0u);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(m->guest_ticks, 13u);
    CHECK_EQ_U64(m->cpu_ready_at, 18u); /* test_skew operand cycle ends at tick 18 */
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 5u);

    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));
    const lynx__cpu held_request = m->cpu;
    CHECK_EQ_U64(lynx_advance(m, 256u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 256u);
    CHECK_EQ_U64(m->guest_ticks, 269u);
    CHECK_EQ_U64(m->cpu_ready_at, m->guest_ticks);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->wake_pending, 1u);
    CHECK(memcmp(&held_request, &m->cpu, sizeof held_request) == 0);
    CHECK_EQ_HEX(reg_of(m, LYNX_REG_CPU_STATE) >> 24, 0u);

    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 2u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(NULL, 1u), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    _Alignas(max_align_t) uint8_t invalid_instance[sizeof(struct lynx_machine)] = {0};
    CHECK_EQ_U64(lynx__machine_set_bus_hold((lynx_machine *)(void *)invalid_instance, 1u),
                 LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);

    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_EQ_U64(m->bus_hold, 0u);
    CHECK_EQ_U64(m->wake_pending, 0u);
    CHECK_EQ_U64(m->guest_ticks, 0u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD81u, 0x01u));
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(m->wake_pending, 1u);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(m->bus_hold, 0u);
    CHECK_EQ_U64(m->wake_pending, 0u);
    CHECK_EQ_U64(m->timer_bank.pending, 0u);

    /* FD91 is in the unimplemented Mikey window. The unavailable write stays
       posted and does not mutate hidden backing RAM or the instance on retry. */
    static const uint8_t fd91_payload[] = {
        0xA9u, 0x0Cu, 0x8Du, 0xF9u, 0xFFu,
        0xA9u, 0x5Au, 0x8Du, 0x91u, 0xFDu
    };
    uint8_t fd91_bll[10u + sizeof fd91_payload];
    make_bll_header(fd91_bll, sizeof fd91_bll, 0x0200u);
    memcpy(fd91_bll + 10u, fd91_payload, sizeof fd91_payload);
    CHECK_EQ_U64(lynx_load_bll(m, fd91_bll, sizeof fd91_bll), LYNX_OK);
    m->ram[0xFD91u] = 0xE7u;
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK(consumed != 0u);
    CHECK_EQ_U64(m->cpu.req_addr, 0xFD91u);
    CHECK_EQ_U64(m->cpu.req_data, 0x5Au);
    CHECK_EQ_U64(m->cpu.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    CHECK_EQ_U64(m->ram[0xFD91u], 0xE7u);
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 1000u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 0u);
    CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
    CHECK_UNCHANGED(m, snap_a);

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.request_boundary\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_phase(void)
{
    const int failures_before = check_failures;
    lynx__timer_bank bank;
    uint64_t next = 0u;
    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        for (uint8_t source = 0u; source <= 6u; source++) {
            lynx__timer_init(&bank);
            const uint16_t backup_addr = timer_register_addr(i, 0u);
            const uint16_t control_addr = timer_register_addr(i, 1u);
            const uint16_t count_addr = timer_register_addr(i, 2u);
            CHECK(timer_write_required(&bank, backup_addr, 1u));
            if (check_failures != failures_before) {
                goto done;
            }
            CHECK(timer_write_required(&bank, count_addr, 1u));
            if (check_failures != failures_before) {
                goto done;
            }
            CHECK(timer_write_required(&bank, control_addr,
                                       (uint8_t)(0x88u | source)));
            if (check_failures != failures_before) {
                goto done;
            }
            const uint64_t period = UINT64_C(16) << source;
            CHECK(lynx__timer_next(&bank, period - 1u, &next));
            CHECK_EQ_U64(next, period);
            lynx__timer_apply(&bank, period);
            CHECK_EQ_U64(bank.timers[i].count, 0u);
            CHECK_EQ_U64(bank.timers[i].control_b & 0x08u, 0u);
            CHECK_EQ_U64(bank.pending, 0u);
            CHECK(lynx__timer_next(&bank, period, &next));
            CHECK_EQ_U64(next, period * 2u);
            lynx__timer_apply(&bank, period * 2u);
            CHECK_EQ_U64(bank.timers[i].count, 0u);
            CHECK_EQ_U64(bank.timers[i].control_b & 0x08u, 0x08u);
            CHECK_EQ_U64(bank.pending, i == 4u ? 0u : (uint8_t)(1u << i));
            CHECK(!lynx__timer_next(&bank, period * 2u, &next));
        }
    }

    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        lynx__timer_init(&bank);
        const uint8_t source = (uint8_t)(i % 7u);
        const uint64_t period = UINT64_C(16) << source;
        CHECK(timer_write_required(&bank, timer_register_addr(i, 0u), UINT8_MAX));
        CHECK(timer_write_required(&bank, timer_register_addr(i, 2u), UINT8_MAX));
        CHECK(timer_write_required(&bank, timer_register_addr(i, 1u),
                                   (uint8_t)(0x08u | source)));
        CHECK(lynx__timer_next(&bank, 0u, &next));
        CHECK_EQ_U64(next, period);
        lynx__timer_apply(&bank, period);
        CHECK_EQ_U64(bank.timers[i].count, UINT8_MAX - 1u);
        CHECK_EQ_U64(bank.timers[i].control_b & 0x08u, 0u);
    }

    /* DONE blocks a one-shot even if software writes a new count; CTLB zero
       clears it. A held reset-DONE level keeps the latch clear on later edges. */
    lynx__timer_init(&bank);
    CHECK(timer_write_required(&bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&bank, 0xFD01u, 0x88u));
    lynx__timer_apply(&bank, 16u);
    CHECK_EQ_U64(bank.timers[0].control_b & 0x08u, 0x08u);
    CHECK(timer_write_required(&bank, 0xFD02u, 3u));
    CHECK(!lynx__timer_next(&bank, 16u, &next));
    CHECK(timer_write_required(&bank, 0xFD03u, 0u));
    CHECK(lynx__timer_next(&bank, 16u, &next));
    CHECK_EQ_U64(next, 32u);

    lynx__timer_init(&bank);
    CHECK(timer_write_required(&bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&bank, 0xFD01u, 0xC8u));
    lynx__timer_apply(&bank, 16u);
    CHECK_EQ_U64(bank.timers[0].control_b & 0x08u, 0u);
    lynx__timer_apply(&bank, 32u);
    CHECK_EQ_U64(bank.timers[0].control_b & 0x08u, 0u);

    /* A reload interval uses backup + 1 source edges, and the shared divider
       remains aligned after count/source/enable writes at a nonzero phase. */
    lynx__timer_init(&bank);
    CHECK(timer_write_required(&bank, 0xFD00u, 2u));
    CHECK(timer_write_required(&bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&bank, 0xFD01u, 0x98u));
    CHECK(lynx__timer_next(&bank, 0u, &next));
    CHECK_EQ_U64(next, 16u);
    lynx__timer_apply(&bank, 16u);
    CHECK_EQ_U64(bank.timers[0].count, 2u);
    lynx__timer_apply(&bank, 32u);
    CHECK_EQ_U64(bank.timers[0].count, 1u);
    lynx__timer_apply(&bank, 48u);
    CHECK_EQ_U64(bank.timers[0].count, 0u);
    lynx__timer_apply(&bank, 64u);
    CHECK_EQ_U64(bank.timers[0].count, 2u);

    for (size_t i = 0u; i < LYNX__TIMER_COUNT; i++) {
        lynx__timer_init(&bank);
        const uint16_t base = timer_register_addr(i, 0u);
        CHECK(timer_write_required(&bank, base, 3u));
        CHECK(timer_write_required(&bank, (uint16_t)(base + 2u), 3u));
        CHECK(timer_write_required(&bank, (uint16_t)(base + 1u), 0x0Eu));
        lynx__timer_mark_tick(&bank, 37u);
        CHECK(lynx__timer_next(&bank, 37u, &next));
        CHECK_EQ_U64(next, 1024u);
        CHECK(timer_write_required(&bank, (uint16_t)(base + 2u), 2u));
        CHECK(lynx__timer_next(&bank, 37u, &next));
        CHECK_EQ_U64(next, 1024u);
        CHECK(timer_write_required(&bank, (uint16_t)(base + 1u), 0x08u));
        CHECK(lynx__timer_next(&bank, 37u, &next));
        CHECK_EQ_U64(next, 48u);
        CHECK(timer_write_required(&bank, (uint16_t)(base + 1u), 0x00u));
        CHECK(!lynx__timer_next(&bank, 37u, &next));
        CHECK(timer_write_required(&bank, (uint16_t)(base + 1u), 0x08u));
        CHECK(lynx__timer_next(&bank, 37u, &next));
        CHECK_EQ_U64(next, 48u);
    }

    /* Link mode is refused atomically; pending set/reset is write-one. */
    lynx__timer_init(&bank);
    CHECK(timer_write_required(&bank, 0xFD01u, 0x08u));
    lynx__timer_bank before_link;
    memcpy(&before_link, &bank, sizeof bank);
    CHECK(!lynx__timer_write(&bank, 0xFD01u, 0x0Fu));
    CHECK(memcmp(&before_link, &bank, sizeof bank) == 0);
    CHECK(timer_write_required(&bank, 0xFD81u, 0x21u));
    CHECK_EQ_U64(bank.pending, 0x21u);
    CHECK_EQ_U64(lynx__timer_peek(&bank, 0xFD80u), 0x21u);
    CHECK(timer_write_required(&bank, 0xFD80u, 0x01u));
    CHECK_EQ_U64(bank.pending, 0x20u);

    /* Timer 4 expiry belongs to UART; it never fabricates pending bit 4. */
    lynx__timer_init(&bank);
    CHECK(timer_write_required(&bank, 0xFD10u, 0u));
    CHECK(timer_write_required(&bank, 0xFD12u, 0u));
    CHECK(timer_write_required(&bank, 0xFD11u, 0x88u));
    lynx__timer_apply(&bank, 16u);
    CHECK_EQ_U64(bank.timers[4].control_b & 0x08u, 0x08u);
    CHECK_EQ_U64(bank.pending & 0x10u, 0u);

    /* A held CPU keeps its request while timers run, then the no-event tail
       jumps to the caller's target. Releasing the gate resumes that request. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program,
                             0x0200u), LYNX_OK);
    lynx__cpu request_before = m->cpu;
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 1u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 1u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x08u));
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 32u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 32u);
    CHECK_EQ_U64(ticks_of(m), 32u);
    CHECK_EQ_U64(m->timer_bank.timers[0].count, 0u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_b & 0x08u, 0u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) == 0);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), 33u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_b & 0x08u, 0x08u);
    CHECK_EQ_U64(lynx_advance(m, 23u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(ticks_of(m), 56u);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) == 0);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK(memcmp(&request_before, &m->cpu, sizeof request_before) != 0);
    lynx_machine_deinit(m);

    static const uint8_t refused_link[] = {
        0xA9u, 0x0Fu, 0x8Du, 0x01u, 0xFDu
    };
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, refused_link, sizeof refused_link, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_advance(m, 50u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNAVAILABLE);
        CHECK_EQ_U64(consumed, 25u);
        CHECK_EQ_U64(m->cpu.req_addr, 0xFD01u);
        CHECK_EQ_U64(m->timer_bank.timers[0].control_a, 0u);
        lynx_machine_deinit(m);
    }

done:
    printf("TAP version 13\n1..1\n%s 1 - api.timer_phase\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_done_gate(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 17u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 17u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_b & 0x08u, 0x08u);
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 3u));
    CHECK_EQ_U64(lynx_advance(m, 16u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 16u);
    CHECK_NAMED(m->timer_bank.timers[0].count == 3u, "timer.done-gates-counting");

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_done_gate\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_reset_done_held(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0xC8u));

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 33u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 33u);
    CHECK_NAMED((m->timer_bank.timers[0].control_b & 0x08u) == 0u,
                "timer.reset-done-held-clear");

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_reset_done_held\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_shared_phase(void)
{
    const int failures_before = check_failures;
    lynx__timer_bank bank;
    uint64_t next = 0u;
    lynx__timer_init(&bank);
    lynx__timer_mark_tick(&bank, 37u);
    CHECK(timer_write_required(&bank, 0xFD00u, 3u));
    CHECK(timer_write_required(&bank, 0xFD02u, 2u));
    CHECK(timer_write_required(&bank, 0xFD01u, 0x0Eu));
    const bool scheduled = lynx__timer_next(&bank, 37u, &next);
    CHECK_NAMED(scheduled && next == 1024u, "timer.phase.shared-divider-edge");
    printf("TAP version 13\n1..1\n%s 1 - api.timer_shared_phase\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* Test the documented Epyx Timer 0/2 presets against guest ticks. Timer 0's
   source is one microsecond (16 master ticks); Timer 2 receives Timer 0
   borrows and clocks once per configured line. */
static void configure_video_cadence(lynx_machine *m, uint8_t timer0_backup,
                                    uint8_t timer2_backup)
{
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD00u, timer0_backup));
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD02u, timer0_backup));
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD08u, timer2_backup));
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD0Au, timer2_backup));
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD09u, 0x1Fu)); /* count, reload, linked */
    CHECK(lynx__timer_write(&m->timer_bank, 0xFD01u, 0x18u)); /* count and reload */
}

/* The source presets are programmed through guest stores, so the assertions
   cover the real mapped register path and preserve timer phase. Stop the CPU
   before the shortest first underflow while device time remains live. */
static void configure_video_cadence_guest(lynx_machine *m, uint8_t timer0_backup,
                                          uint8_t timer2_backup)
{
    uint8_t program[48];
    size_t n = 0u;
    program[n++] = 0xA9u; program[n++] = 0x0Cu; /* MAPCTL keeps device overlays */
    program[n++] = 0x8Du; program[n++] = 0xF9u; program[n++] = 0xFFu;
    program[n++] = 0xA9u; program[n++] = 0x01u; /* enable video DMA */
    program[n++] = 0x8Du; program[n++] = 0x92u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = timer0_backup;
    program[n++] = 0x8Du; program[n++] = 0x00u; program[n++] = 0xFDu;
    program[n++] = 0x8Du; program[n++] = 0x02u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = timer2_backup;
    program[n++] = 0x8Du; program[n++] = 0x08u; program[n++] = 0xFDu;
    program[n++] = 0x8Du; program[n++] = 0x0Au; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x1Fu; /* count, reload, linked */
    program[n++] = 0x8Du; program[n++] = 0x09u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x18u; /* start Timer 0 last */
    program[n++] = 0x8Du; program[n++] = 0x01u; program[n++] = 0xFDu;
    const uint16_t loop = (uint16_t)(0x0200u + (uint16_t)n);
    program[n++] = 0x4Cu;
    program[n++] = (uint8_t)loop;
    program[n++] = (uint8_t)(loop >> 8);
    CHECK_EQ_U64(load_bll_at(m, program, n, 0x0200u), LYNX_OK);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
    CHECK_NAMED(consumed == 512u && stop == LYNX_STOP_BUDGET && ticks_of(m) == 512u,
                "video.guest-program-starts-cadence");
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
}

static void init_video_frame_buffers(lynx_frame_buffers *buffers, uint8_t *pens,
                                     uint8_t *palettes, uint64_t *line_ticks)
{
    memset(buffers, 0, sizeof *buffers);
    buffers->struct_size = (uint32_t)sizeof *buffers;
    buffers->pens = pens;
    buffers->pens_capacity = LYNX_FRAME_PENS_CAPACITY;
    buffers->palettes = palettes;
    buffers->palettes_capacity = LYNX_FRAME_PALETTE_CAPACITY;
    buffers->line_ticks = line_ticks;
    buffers->line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
}

static void init_video_frame_result(lynx_frame_result *result)
{
    memset(result, 0, sizeof *result);
    result->struct_size = (uint32_t)sizeof *result;
}

static void check_mapped_opcode_fetch(uint32_t profile, uint16_t address,
                                      uint32_t input_mask, uint8_t mapped_opcode,
                                      uint8_t ram_opcode, int expect_undefined,
                                      const char *case_name)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    lynx_machine *m = init_in(machine_buf, &cfg);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    if (m == NULL) return;

    if (load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u) != LYNX_OK) {
        CHECK_NAMED(false, "video.bus.opcode-fetch-load-bll");
        lynx_machine_deinit(m);
        return;
    }
    m->mapctl = 0u; /* FCxx selects Suzy; FDxx selects Mikey. */
    m->ram[address] = ram_opcode;
    if (address == 0xFCB0u) {
        CHECK_EQ_U64(lynx_set_input(m, input_mask), LYNX_OK);
        CHECK_EQ_U64(lynx__bus_decode(m->mapctl, address), LYNX__BUS_ROUTE_SUZY);
        CHECK_EQ_HEX(lynx__input_peek(&m->video, address), mapped_opcode);
    } else if (address == 0xFDA0u) {
        CHECK(lynx__video_write(&m->video, address, mapped_opcode));
        CHECK_EQ_U64(lynx__bus_decode(m->mapctl, address), LYNX__BUS_ROUTE_MIKEY);
        CHECK_EQ_HEX(lynx__video_peek(&m->video, address), mapped_opcode);
    } else if (address == 0xFD00u) {
        CHECK(lynx__timer_write(&m->timer_bank, address, mapped_opcode));
        CHECK_EQ_U64(lynx__bus_decode(m->mapctl, address), LYNX__BUS_ROUTE_MIKEY);
        CHECK_EQ_HEX(lynx__timer_peek(&m->timer_bank, address), mapped_opcode);
    } else {
        CHECK_NAMED(false, "video.bus.opcode-fetch-supported-device-address");
    }
    lynx__cpu_start_at(&m->cpu, lynx__cpu_table(m->cpu.profile), address);
    m->cpu_ready_at = m->guest_ticks;

    const uint64_t start = m->guest_ticks;
    const lynx__cpu posted = m->cpu;
    const uint16_t input_before = m->video.input_mask;
    memcpy(snap_a, machine_buf, MSIZE);
    CHECK_EQ_U64(lynx_advance(m, 5u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(m->video.input_mask, input_before);
    CHECK_EQ_U64(m->ram[address], ram_opcode);
    if (expect_undefined) {
        CHECK_NAMED(stop == LYNX_STOP_UNDEFINED && consumed == 0u, case_name);
        CHECK_EQ_U64(stop, LYNX_STOP_UNDEFINED);
        CHECK_EQ_U64(consumed, 0u);
        CHECK_EQ_U64(m->guest_ticks, start);
        CHECK(memcmp(&posted, &m->cpu, sizeof posted) == 0);
        CHECK_UNCHANGED(machine_buf, snap_a);

        /* Repeated refusal remains at the same boundary and does not mutate. */
        memcpy(snap_a, machine_buf, MSIZE);
        consumed = UINT64_MAX;
        stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(m, 5u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_UNDEFINED);
        CHECK_EQ_U64(consumed, 0u);
        CHECK_UNCHANGED(machine_buf, snap_a);
    } else {
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, 5u);
        CHECK_EQ_U64(m->guest_ticks, start + 5u);
        CHECK_EQ_U64(m->cpu.opcode, mapped_opcode);
        CHECK_EQ_U64(m->cpu.pc, (uint16_t)(address + 1u));
        CHECK_NAMED(m->cpu.opcode == mapped_opcode, case_name);
    }
    lynx_machine_deinit(m);
}

static void check_mapped_opcode_fetches(void)
{
    const uint32_t profiles[] = {
        LYNX_PROFILE_NONE, LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2
    };
    const uint32_t nop_mask = LYNX_INPUT_UP | LYNX_INPUT_DOWN | LYNX_INPUT_RIGHT |
                              LYNX_INPUT_OPTION1 | LYNX_INPUT_B;
    const uint32_t bit_opcode_mask = LYNX_INPUT_A | LYNX_INPUT_B | LYNX_INPUT_OPTION2;
    const uint16_t device_addresses[] = {0xFCB0u, 0xFDA0u, 0xFD00u};
    for (size_t i = 0u; i < sizeof profiles / sizeof profiles[0]; i++) {
        for (size_t route = 0u; route < sizeof device_addresses / sizeof device_addresses[0]; route++) {
            const uint16_t address = device_addresses[route];
            const uint32_t mask = address == 0xFCB0u ? nop_mask : 0u;
            const uint32_t undefined_mask = address == 0xFCB0u ? bit_opcode_mask : 0u;
            const char *route_name = address == 0xFCB0u ? "suzy" :
                                     address == 0xFDA0u ? "mikey-video" : "mikey-timer";
            check_mapped_opcode_fetch(profiles[i], address, mask, 0xEAu, 0x07u, 0,
                                      route_name);
            check_mapped_opcode_fetch(profiles[i], address, undefined_mask, 0x07u, 0xEAu,
                                      profiles[i] == LYNX_PROFILE_NONE, route_name);
        }
    }
}

static void case_video_bus(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    if (m == NULL) goto done;

    CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program, 0x0200u), LYNX_OK);
    bool found_store = false;
    for (uint32_t i = 0u; i < 200u; i++) {
        if (m->cpu.req_addr == 0x0400u &&
            (m->cpu.req_flags & LYNX__CF_WRITE) != 0u) {
            found_store = true;
            break;
        }
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 1u);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }
    CHECK_NAMED(found_store, "video.bus.real-guest-store-posted");
    if (!found_store) goto done;

    const uint64_t start = m->guest_ticks;
    const lynx__cpu posted = m->cpu;
    CHECK_EQ_U64(posted.req_addr, 0x0400u);
    CHECK_EQ_U64(posted.req_data, 0x5Au);
    CHECK_EQ_U64(posted.req_flags & LYNX__CF_WRITE, LYNX__CF_WRITE);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    m->cpu.p = (uint8_t)(m->cpu.p | 0x04u); /* Keep the timer IRQ pending without entering it. */

    CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
    m->video.row_active = 1u;
    m->video.capture_rows = 0u;
    m->video.dma_cursor = 70u; /* Ten one-byte grants keep the store pending. */
    m->video.dma_next_tick = start;
    m->video.dma_busy_until = start;
    m->video.refresh_next_tick = start;
    m->video.refresh_busy_until = start;
    CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));

    /* The last video grant lands at start+45 and releases at start+50. The
       equal-tick refresh request then owns through start+55; half-open calls
       prove the CPU store is not completed at either exact release boundary. */
    CHECK_EQ_U64(lynx_advance(m, 50u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 50u);
    CHECK_EQ_U64(m->guest_ticks, start + 50u);
    CHECK_EQ_U64(m->video.capture_rows, 1u);
    CHECK_EQ_U64(m->video.dma_busy_until, start + 50u);
    CHECK_EQ_U64(m->video.refresh_next_tick, start);
    CHECK_EQ_U64(m->video.refresh_busy_until, start);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->wake_pending, 1u);
    CHECK(memcmp(&posted, &m->cpu, sizeof posted) == 0);
    CHECK_EQ_U64(m->ram[0x0400u], 0u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit, 0u);

    CHECK_EQ_U64(lynx_advance(m, 5u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 5u);
    CHECK_EQ_U64(m->guest_ticks, start + 55u);
    CHECK_EQ_U64(m->video.refresh_busy_until, start + 55u);
    CHECK_EQ_U64(m->video.refresh_next_tick, start + 256u);
    CHECK(memcmp(&posted, &m->cpu, sizeof posted) == 0);
    CHECK_EQ_U64(m->ram[0x0400u], 0u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit, 0u);

    CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
    CHECK_EQ_U64(consumed, 1u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit, 1u);
    CHECK_EQ_U64(m->host_observation.watch_event.hit_tick, start + 55u);
    CHECK_EQ_U64(m->host_observation.watch_event.value, 0x5Au);
    CHECK_EQ_U64(m->host_observation.watch_event.return_tick, start + 56u);
    CHECK_EQ_U64(m->ram[0x0400u], 0x5Au);
    CHECK_NAMED(m->cpu.req_addr != 0x0400u, "video.bus.store-completed-once");

    /* Continue only until the following INC has committed. A replayed held
       store would leave the CPU at $0400 instead of reaching this increment. */
    CHECK_EQ_U64(lynx_watch_clear(m), LYNX_OK);
    for (uint32_t i = 0u; i < 50u && m->ram[0x0401u] == 0u; i++) {
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }
    CHECK_EQ_U64(m->ram[0x0401u], 1u);

    lynx_machine_deinit(m);
    m = NULL;
    check_mapped_opcode_fetches();

done:
    if (m != NULL) lynx_machine_deinit(m);
    printf("TAP version 13\n1..1\n%s 1 - api.video_bus\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_bus_edges(void)
{
    const int failures_before = check_failures;
    lynx__video_state video;
    uint8_t ram[LYNX_ADDRESS_SPACE];
    uint64_t next = UINT64_MAX;
    memset(ram, 0, sizeof ram);
    lynx__video_init(&video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    CHECK(lynx__video_write(&video, 0xFD92u, 1u));
    video.row_active = 1u;
    video.capture_rows = 0u;
    video.dma_cursor = 79u;
    video.dma_next_tick = 256u;
    video.refresh_next_tick = 256u;
    CHECK(lynx__video_next_dma(&video, 256u, &next));
    CHECK_EQ_U64(next, 256u);
    lynx__video_dma_apply(&video, ram, 256u);
    CHECK_EQ_U64(video.capture_rows, 1u);
    CHECK_EQ_U64(video.dma_busy_until, 261u);
    next = UINT64_MAX;
    CHECK(lynx__video_next_refresh(&video, 256u, &next));
    CHECK_NAMED(next == 261u, "video.bus.video-before-refresh-release");
    lynx__video_refresh_apply(&video, 261u);
    CHECK_EQ_U64(video.refresh_busy_until, 266u);
    CHECK_EQ_U64(video.refresh_next_tick, 512u);

    /* Disabling DMA cancels the unfinished row. Re-enabling does not revive
       its stale request; the next Timer 0 row edge starts a fresh row. */
    video.row_active = 1u;
    video.capture_rows = 0u;
    video.dma_cursor = 3u;
    video.dma_next_tick = 300u;
    video.frame_synced = 1u;
    video.line_ordinal = 2u;
    CHECK(lynx__video_write(&video, 0xFD92u, 0u));
    CHECK_EQ_U64(video.row_active, 0u);
    CHECK_EQ_U64(video.dma_cursor, 0u);
    CHECK_EQ_U64(video.dma_next_tick, 0u);
    CHECK_EQ_U64(video.capture_partial, 1u);
    CHECK(!lynx__video_next_dma(&video, 300u, &next));
    CHECK(lynx__video_write(&video, 0xFD92u, 1u));
    CHECK(!lynx__video_next_dma(&video, 300u, &next));
    (void)lynx__video_timer_apply(&video, 1u, 320u);
    CHECK_EQ_U64(video.row_active, 1u);
    CHECK_EQ_U64(video.dma_next_tick, 325u);

    /* A Timer 0 source reprogram made while video work is pending changes
       the next timer deadline without shifting the shared divider phase. The
       same-tick video grant still runs first, then refresh, before that edge. */
    lynx__timer_bank timers;
    lynx__video_state timed_video;
    lynx__timer_init(&timers);
    lynx__video_init(&timed_video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    CHECK(timer_write_required(&timers, 0xFD02u, 0u));
    CHECK(timer_write_required(&timers, 0xFD01u, 0x88u));
    CHECK(lynx__timer_next(&timers, 300u, &next));
    CHECK_EQ_U64(next, 304u);
    CHECK(timer_write_required(&timers, 0xFD01u, 0x89u));
    CHECK(lynx__timer_next(&timers, 300u, &next));
    CHECK_EQ_U64(next, 320u);
    CHECK(lynx__video_write(&timed_video, 0xFD92u, 1u));
    timed_video.row_active = 1u;
    timed_video.dma_cursor = 0u;
    timed_video.dma_next_tick = 304u;
    timed_video.refresh_next_tick = 304u;
    CHECK(lynx__video_next_dma(&timed_video, 300u, &next));
    CHECK_EQ_U64(next, 304u);
    lynx__video_dma_apply(&timed_video, ram, 304u);
    CHECK(lynx__video_next_refresh(&timed_video, 304u, &next));
    CHECK_EQ_U64(next, 309u);
    lynx__video_refresh_apply(&timed_video, 309u);
    CHECK_EQ_U64(timed_video.refresh_busy_until, 314u);
    CHECK(lynx__timer_next(&timers, 309u, &next));
    CHECK_EQ_U64(next, 320u);

    lynx__video_state idle;
    lynx__video_init(&idle, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    CHECK(!lynx__video_next_dma(&idle, 0u, &next));
    CHECK(lynx__video_next_refresh(&idle, 0u, &next));
    CHECK_EQ_U64(next, 256u);

    /* The scheduler grants work at the current tick, then stops at its
       headroom limit with a bounded, non-wrapping busy deadline. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        const uint64_t limit = UINT64_MAX - LYNX__TICK_HEADROOM;
        m->guest_ticks = limit - 2u;
        m->cpu_ready_at = m->guest_ticks;
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        m->video.dispctl = 1u;
        m->video.row_active = 1u;
        m->video.capture_rows = 0u;
        m->video.dma_cursor = 79u;
        m->video.dma_next_tick = m->guest_ticks;
        m->video.refresh_next_tick = 0u;
        CHECK_EQ_U64(lynx_advance(m, 10u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, 2u);
        CHECK_EQ_U64(m->guest_ticks, limit);
        CHECK_EQ_U64(m->video.dma_busy_until, limit + 3u);
        lynx_machine_deinit(m);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.video_bus_edges\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_dma_cost_fault(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(load_bll_at(m, watch_count_program, sizeof watch_count_program,
                                 0x0200u), LYNX_OK);
        for (unsigned i = 0u; i < 200u &&
                              !(m->cpu.req_addr == 0x0400u &&
                                (m->cpu.req_flags & LYNX__CF_WRITE) != 0u); i++) {
            CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        }
        const uint64_t start = m->guest_ticks;
        CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
        m->cpu.p = (uint8_t)(m->cpu.p | 0x04u);
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        m->video.row_active = 1u;
        m->video.capture_rows = 0u;
        m->video.dma_cursor = 70u;
        m->video.dma_next_tick = start;
        m->video.dma_busy_until = start;
        m->video.refresh_next_tick = start;
        m->video.refresh_busy_until = start;
        CHECK(timer_write_required(&m->timer_bank, 0xFD00u, 0u));
        CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
        CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));
        CHECK_EQ_U64(lynx_advance(m, 128u, &consumed, &stop), LYNX_OK);
        CHECK_NAMED(stop == LYNX_STOP_WATCH &&
                        m->host_observation.watch_event.hit_tick == start + 55u,
                    "video.fault.dma-cost-holds-cpu-until-release");
        lynx_machine_deinit(m);
    }

    /* The timing fault must not alter the bytes transferred by a complete
       DMA row. Hold the CPU so this check isolates content from arbitration. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        const uint64_t start = m->guest_ticks;
        m->video.row_active = 1u;
        m->video.capture_rows = 0u;
        m->video.dispaddr = 0xFCB0u;
        m->video.dma_next_tick = start;
        m->video.dma_busy_until = start;
        m->video.refresh_next_tick = 0u;
        m->video.refresh_busy_until = start;
        for (unsigned i = 0u; i < LYNX_FRAME_ROW_BYTES; i++) {
            m->ram[0xFCB0u + i] = (uint8_t)(0xA5u ^ (uint8_t)i);
        }
        CHECK_EQ_U64(lynx_advance(m, 401u, &consumed, &stop), LYNX_OK);
        uint8_t expected_pens[LYNX_FRAME_PENS_CAPACITY];
        uint8_t expected_palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint64_t expected_line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        memset(expected_pens, 0, sizeof expected_pens);
        memset(expected_palettes, 0, sizeof expected_palettes);
        memset(expected_line_ticks, 0, sizeof expected_line_ticks);
        for (unsigned i = 0u; i < LYNX_FRAME_ROW_BYTES; i++) {
            expected_pens[i] = m->ram[0xFCB0u + i];
        }
        const bool raw_frame_matches = stop == LYNX_STOP_BUDGET && consumed == 401u &&
            m->video.capture_rows == 1u &&
            memcmp(m->video.pens, expected_pens, sizeof expected_pens) == 0 &&
            memcmp(m->video.palettes, expected_palettes, sizeof expected_palettes) == 0 &&
            memcmp(m->video.line_ticks, expected_line_ticks, sizeof expected_line_ticks) == 0;
        CHECK_NAMED(raw_frame_matches,
                    "video.fault.dma-cost-preserves-full-raw-frame");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.video_dma_cost_fault\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_dma_request_fault(void)
{
    const int failures_before = check_failures;
    lynx__video_state idle;
    uint64_t next = UINT64_MAX;
    lynx__video_init(&idle, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    CHECK_NAMED(!lynx__video_next_dma(&idle, 0u, &next),
                "video.fault.inactive-dma-has-no-request");

    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        const uint64_t start = m->guest_ticks;
        m->ram[0xFCB0u] = 0xA6u;
        m->video.dispctl = 1u;
        m->video.row_active = 1u;
        m->video.capture_rows = 0u;
        m->video.dispaddr = 0xFCB0u;
        m->video.dma_next_tick = start;
        m->video.dma_busy_until = start;
        m->video.refresh_next_tick = 0u;
        m->video.refresh_busy_until = start;
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
        CHECK_NAMED(stop == LYNX_STOP_BUDGET && consumed == 1u &&
                        m->video.pens[0] == 0xA6u,
                    "video.fault.active-row-receives-video-ram-grant");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.video_dma_request_fault\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void video_page_expect_grant(lynx_machine *m, uint64_t grant_tick,
                                    uint16_t addr, uint8_t kind, uint8_t ticks,
                                    const char *assertion)
{
    CHECK_EQ_U64(m->cpu.req_addr, addr);
    CHECK_EQ_U64(m->cpu.req_kind, kind);
    CHECK(grant_tick >= m->guest_ticks);
    const uint64_t budget = grant_tick - m->guest_ticks + 1u;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, budget, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(consumed, budget);
    CHECK_EQ_U64(m->guest_ticks, grant_tick + 1u);
    CHECK_NAMED(m->cpu_ready_at == grant_tick + ticks, assertion);
}

static uint64_t video_page_expect_next(lynx_machine *m, uint16_t addr, uint8_t kind,
                                       uint8_t ticks, const char *assertion)
{
    const uint64_t grant_tick = m->cpu_ready_at > m->guest_ticks ?
                                    m->cpu_ready_at : m->guest_ticks;
    video_page_expect_grant(m, grant_tick, addr, kind, ticks, assertion);
    return m->cpu_ready_at;
}

static void case_video_page_mode(void)
{
    const int failures_before = check_failures;
    static const uint8_t program[] = {
        0xEAu,                               /* $0200 NOP: initial page access */
        0xEAu,                               /* $0201 NOP: eligible same-page opcode */
        0xADu, 0x00u, 0x02u,                 /* $0202 LDA $0200: opcode plus data */
        0xEAu,                               /* $0205 eligible opcode after data read */
        0x8Du, 0x01u, 0x03u,                 /* $0206 STA $0301: writes stay full-cycle */
        0xEAu, 0xEAu, 0xEAu,                 /* $0209-$020B same page */
        0xEAu                                /* $020C */
    };
    const uint32_t profiles[] = {LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    for (size_t profile_index = 0u;
         profile_index < sizeof profiles / sizeof profiles[0]; profile_index++) {
        lynx_config cfg;
        lynx_config_default(&cfg);
        cfg.profile = profiles[profile_index];
        cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
        lynx_machine *m = init_in(machine_buf, &cfg);
        lynx_machine *restored = NULL;
        if (m == NULL) continue;
        CHECK_EQ_U64(load_bll_at(m, program, sizeof program, 0x0200u), LYNX_OK);
        m->mapctl = 0u; /* MAPCTL reset enables sequential cycles; low overlays remain clear. */
        lynx__cpu_start_at(&m->cpu, lynx__cpu_table(m->cpu.profile), 0x0200u);
        m->cpu_ready_at = m->guest_ticks;

        video_page_expect_grant(m, 0u, 0x0200u, LYNX__CK_OPCODE, 5u,
                                "page-mode initial fetch uses normal five-tick cycle");
        video_page_expect_next(m, 0x0201u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        CHECK_EQ_U64(save_into(m, blob_c), 0u);
        restored = init_in(buf_b, &cfg);
        if (restored != NULL) {
            CHECK_EQ_U64(load_bll_at(restored, program, sizeof program, 0x0200u), LYNX_OK);
            CHECK_EQ_U64(lynx_snapshot_restore(restored, blob_c, SNAP_TOTAL), LYNX_OK);
            video_page_expect_next(restored, 0x0201u, LYNX__CK_OPCODE, 4u,
                                    "page-mode snapshot retains same-page continuation");
            restored->mapctl = 0x80u; /* B7 disables sequential cycles. */
            lynx__cpu_start_at(&restored->cpu, lynx__cpu_table(restored->cpu.profile), 0x0200u);
            restored->cpu_ready_at = restored->guest_ticks;
            video_page_expect_next(restored, 0x0200u, LYNX__CK_OPCODE, 5u,
                                   "page-mode disabled first fetch uses normal cycle");
            video_page_expect_next(restored, 0x0201u, LYNX__CK_DUMMY_STREAM, 5u,
                                   "page-mode disabled dummy access uses normal cycle");
            video_page_expect_next(restored, 0x0201u, LYNX__CK_OPCODE, 5u,
                                   "page-mode disabled same-page fetch uses normal cycle");
            lynx_machine_deinit(restored);
        }

        /* Exact microcycle trace: only NOP opcode fetches can use the narrow
           unverified subset. Every implied dummy, operand, data access, other
           opcode, and write remains full-cycle. */
        video_page_expect_next(m, 0x0201u, LYNX__CK_OPCODE, 4u,
                                "page-mode same-page NOP fetch costs four ticks");
        video_page_expect_next(m, 0x0202u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x0202u, LYNX__CK_OPCODE, 5u,
                                "page-mode conservative opcode exclusion costs five ticks");
        video_page_expect_next(m, 0x0203u, LYNX__CK_OPERAND, 5u,
                                "page-mode operand access costs five ticks");
        video_page_expect_next(m, 0x0204u, LYNX__CK_OPERAND, 5u,
                                "page-mode operand access costs five ticks");
        video_page_expect_next(m, 0x0200u, LYNX__CK_READ, 5u,
                                "page-mode data read costs five ticks");
        video_page_expect_next(m, 0x0205u, LYNX__CK_OPCODE, 4u,
                                "page-mode NOP after same-page data access costs four ticks");
        video_page_expect_next(m, 0x0206u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x0206u, LYNX__CK_OPCODE, 5u,
                                "page-mode conservative opcode exclusion costs five ticks");
        video_page_expect_next(m, 0x0207u, LYNX__CK_OPERAND, 5u,
                                "page-mode store operand costs five ticks");
        video_page_expect_next(m, 0x0208u, LYNX__CK_OPERAND, 5u,
                                "page-mode store operand costs five ticks");
        video_page_expect_next(m, 0x0301u, LYNX__CK_WRITE, 5u,
                                "page-mode data write costs five ticks");
        CHECK_EQ_U64(m->ram[0x0301u], 0xEAu);
        video_page_expect_next(m, 0x0209u, LYNX__CK_OPCODE, 5u,
                               "page-mode page crossing uses normal five-tick cycle");
        video_page_expect_next(m, 0x020Au, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x020Au, LYNX__CK_OPCODE, 4u,
                               "page-mode same-page stream resumes after CPU access");
        video_page_expect_next(m, 0x020Bu, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x020Bu, LYNX__CK_OPCODE, 4u,
                               "page-mode same-page stream remains eligible");
        video_page_expect_next(m, 0x020Cu, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");

        /* JMP $0300 leaves its high operand at $02FF, so the target opcode
           crosses a page without an intervening CPU access at $0300. */
        m->ram[0x02FDu] = 0x4Cu;
        m->ram[0x02FEu] = 0x00u;
        m->ram[0x02FFu] = 0x03u;
        m->ram[0x0300u] = 0xEAu;
        m->ram[0x0301u] = 0xEAu;
        m->ram[0x0302u] = 0xEAu;
        m->ram[0x0303u] = 0xEAu;
        m->ram[0x0304u] = 0xEAu;
        lynx__cpu_start_at(&m->cpu, lynx__cpu_table(m->cpu.profile), 0x02FDu);
        video_page_expect_next(m, 0x02FDu, LYNX__CK_OPCODE, 5u,
                               "page-mode conservative JMP opcode uses normal cycle");
        video_page_expect_next(m, 0x02FEu, LYNX__CK_OPERAND, 5u,
                               "page-mode JMP operand uses normal cycle");
        video_page_expect_next(m, 0x02FFu, LYNX__CK_OPERAND, 5u,
                               "page-mode JMP operand uses normal cycle");
        video_page_expect_next(m, 0x0300u, LYNX__CK_OPCODE, 5u,
                               "page-mode 256-byte crossing uses normal cycle");
        video_page_expect_next(m, 0x0301u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x0301u, LYNX__CK_OPCODE, 4u,
                               "page-mode same-page fetch after crossing is eligible");
        video_page_expect_next(m, 0x0302u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");

        /* A one-byte video row closes continuation before the pending CPU
           opcode, then a refresh grant independently closes the next stream. */
        const uint64_t video_tick = m->cpu_ready_at;
        m->video.dispctl = 1u;
        m->video.row_active = 1u;
        m->video.dma_cursor = 79u;
        m->video.dma_next_tick = video_tick;
        m->video.dma_busy_until = video_tick;
        m->video.refresh_next_tick = 0u;
        m->video.refresh_busy_until = 0u;
        video_page_expect_grant(m, video_tick + 5u, 0x0302u, LYNX__CK_OPCODE, 5u,
                                "page-mode video grant closes the CPU continuation");
        video_page_expect_next(m, 0x0303u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        const uint64_t refresh_tick = m->cpu_ready_at;
        m->video.refresh_next_tick = refresh_tick;
        video_page_expect_grant(m, refresh_tick + 5u, 0x0303u, LYNX__CK_OPCODE, 5u,
                                "page-mode refresh grant closes the CPU continuation");
        video_page_expect_next(m, 0x0304u, LYNX__CK_DUMMY_STREAM, 5u,
                               "page-mode implied dummy access uses normal cycle");
        video_page_expect_next(m, 0x0304u, LYNX__CK_OPCODE, 4u,
                                "page-mode CPU stream resumes after closed grant");
        lynx_machine_deinit(m);
    }

    /* The legacy flat5 control remains unchanged even with MAPCTL bit 7 set. */
    lynx_config legacy_cfg;
    lynx_config_default(&legacy_cfg);
    legacy_cfg.profile = LYNX_PROFILE_LYNX1;
    lynx_machine *legacy = init_in(machine_buf, &legacy_cfg);
    if (legacy != NULL) {
        CHECK_EQ_U64(load_bll_at(legacy, program, sizeof program, 0x0200u), LYNX_OK);
        legacy->mapctl = 0u;
        lynx__cpu_start_at(&legacy->cpu, lynx__cpu_table(legacy->cpu.profile), 0x0200u);
        legacy->cpu_ready_at = legacy->guest_ticks;
        video_page_expect_next(legacy, 0x0200u, LYNX__CK_OPCODE, 5u,
                               "legacy flat5 first opcode remains five ticks");
        video_page_expect_next(legacy, 0x0201u, LYNX__CK_DUMMY_STREAM, 5u,
                               "legacy flat5 dummy access remains five ticks");
        video_page_expect_next(legacy, 0x0201u, LYNX__CK_OPCODE, 5u,
                               "legacy flat5 same-page opcode remains five ticks");
        lynx_machine_deinit(legacy);
    }

    /* An invalid timing-policy id is rejected before touching caller storage. */
    lynx_config invalid;
    lynx_config_default(&invalid);
    invalid.timing_policy = 2u;
    memset(machine_buf, 0xA5, MSIZE);
    memcpy(snap_a, machine_buf, MSIZE);
    lynx_machine *invalid_out = NULL;
    CHECK_EQ_U64(lynx_machine_init(machine_buf, MSIZE, &invalid, &invalid_out), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(machine_buf, snap_a);
    if (invalid_out != NULL) lynx_machine_deinit(invalid_out);
    printf("TAP version 13\n1..1\n%s 1 - api.video_page_mode\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_cadence(void)
{
    const int failures_before = check_failures;
    static const struct {
        uint8_t timer0_backup;
        const char *name;
    } presets[] = {
        {190u, "video.cadence.50hz"},
        {158u, "video.cadence.60hz"},
        {126u, "video.cadence.75hz"},
    };
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;

    for (size_t preset = 0u; preset < sizeof presets / sizeof presets[0]; preset++) {
        lynx_config cfg;
        lynx_config_default(&cfg);
        cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
        lynx_machine *m = init_in(machine_buf, &cfg);
        if (m == NULL) {
            break;
        }
        configure_video_cadence_guest(m, presets[preset].timer0_backup, 104u);
        init_video_frame_buffers(&buffers, pens, palettes, line_ticks);

        const uint64_t line_ticks_expected =
            ((uint64_t)presets[preset].timer0_backup + 1u) * 16u;
        const uint64_t frame_ticks_expected = line_ticks_expected * 105u;
        memset(pens, 0xA5, sizeof pens);
        memset(palettes, 0xA5, sizeof palettes);
        memset(line_ticks, 0xA5, sizeof line_ticks);
        lynx_frame_result result;
        init_video_frame_result(&result);

        CHECK_EQ_U64(lynx_advance_frame(m, 0u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 0u &&
                        ticks_of(m) == 512u && pens[0] == 0xA5u && palettes[0] == 0xA5u &&
                        line_ticks[0] == UINT64_C(0xA5A5A5A5A5A5A5A5),
                    "video.zero-budget-preserves-progress-and-buffers");
        CHECK_EQ_U64(lynx_advance_frame(m, 2u * frame_ticks_expected, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u &&
                        result.frame_end_tick > ticks_of(m) - result.ticks_consumed,
                    "video.first-edge-policy-frame");
        const uint64_t first_frame_tick = result.frame_end_tick;
        memset(pens, 0xA5, sizeof pens);
        memset(palettes, 0xA5, sizeof palettes);
        memset(line_ticks, 0xA5, sizeof line_ticks);

        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected - 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET &&
                        ticks_of(m) == first_frame_tick + frame_ticks_expected - 1u,
                    "video.steady-frame-period-minus-one");
        CHECK_NAMED(pens[0] == 0xA5u && pens[sizeof pens - 1u] == 0xA5u &&
                        palettes[0] == 0xA5u && palettes[sizeof palettes - 1u] == 0xA5u &&
                        line_ticks[0] == UINT64_C(0xA5A5A5A5A5A5A5A5),
                    "video.no-frame-preserves-output-sentinels");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET &&
                        ticks_of(m) == first_frame_tick + frame_ticks_expected,
                    "video.steady-frame-exact-target");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 0u &&
                        result.frame_start_tick == first_frame_tick &&
                        result.frame_end_tick == first_frame_tick + frame_ticks_expected &&
                        result.sequence == 1u && result.row_count == LYNX_FRAME_MAX_ROWS &&
                        result.flags == 0u,
                    "video.source-preset-steady-period-and-rows");
        CHECK_NAMED(line_ticks[0] == first_frame_tick + 3u * line_ticks_expected &&
                        line_ticks[101] == first_frame_tick + 104u * line_ticks_expected,
                    "video.standard-row-start-ticks");
        lynx_machine_deinit(m);
    }

    /* A deliberately short Timer 0/2 period starts only partial rows. They
       are not counted as visible output rows, while their consumed DMA grants
       still set the explicit partial-frame flag. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        m->ram[0] = 0xABu;
        m->ram[1] = 0xCDu;
        configure_video_cadence(m, 0u, 4u);
        init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
        memset(pens, 0xA5, sizeof pens);
        memset(palettes, 0xA5, sizeof palettes);
        memset(line_ticks, 0xA5, sizeof line_ticks);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 80u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && ticks_of(m) == 80u,
                    "video.short-frame-sync-edge-half-open");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 0u,
                    "video.short-frame-sync-event");
        CHECK_EQ_U64(lynx_advance_frame(m, 81u, &buffers, &result), LYNX_OK);
        if (result.row_count != 0u || result.flags != LYNX_FRAME_PARTIAL || pens[0] != 0u) {
            fprintf(stderr, "OBSERVED short frame row_count=%u flags=%u pen0=%u\n",
                    (unsigned)result.row_count, (unsigned)result.flags, (unsigned)pens[0]);
        }
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.frame_start_tick == 80u &&
                        result.frame_end_tick == 160u && result.row_count == 0u &&
                        result.flags == LYNX_FRAME_PARTIAL && pens[0] == 0u &&
                        line_ticks[0] == 0u,
                    "video.incomplete-rows-omitted-from-short-frame");
        lynx_machine_deinit(m);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.video_cadence\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_frame_edges(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    configure_video_cadence(m, 158u, 104u);
    const uint64_t first_frame_tick = UINT64_C(159) * 16u * 105u;
    memset(pens, 0x5Au, sizeof pens);
    memset(palettes, 0x5Au, sizeof palettes);
    memset(line_ticks, 0x5Au, sizeof line_ticks);
    lynx_frame_result result;
    init_video_frame_result(&result);

    CHECK_EQ_U64(lynx_advance_frame(m, 0u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 0u &&
                    ticks_of(m) == 0u && pens[0] == 0x5Au,
                "video.zero-budget-does-no-work");
    CHECK_EQ_U64(lynx_advance_frame(m, first_frame_tick - 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET &&
                    result.ticks_consumed == first_frame_tick - 1u,
                "video.frame-minus-one");
    CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 1u &&
                    ticks_of(m) == first_frame_tick,
                "video.frame-event-at-exclusive-target");
    CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 0u &&
                    result.frame_end_tick == first_frame_tick,
                "video.frame-event-after-target");

    memset(pens, 0x5Au, sizeof pens);
    CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 1u &&
                    result.frame_end_tick == 0u && pens[0] == 0x5Au,
                "video.frame-event-consumed-once");
    lynx_machine_deinit(m);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.frame_edges\n",
           check_failures == failures_before ? "ok" : "not ok");
}

enum { VIDEO_TEST_GUARD = 16 };

static uint8_t video_test_green(uint8_t salt, unsigned entry)
{
    return (uint8_t)(0xA0u | ((entry * 3u + salt) & 0x0Fu));
}

static uint8_t video_test_blue_red(uint8_t salt, unsigned entry)
{
    return (uint8_t)(0xC0u | ((entry * 5u + salt * 3u + 1u) & 0x0Fu));
}

static void write_video_test_palette(lynx_machine *m, uint8_t salt)
{
    for (unsigned entry = 0u; entry < 16u; entry++) {
        CHECK(lynx__video_write(&m->video, (uint16_t)(0xFDA0u + entry),
                                video_test_green(salt, entry)));
        CHECK(lynx__video_write(&m->video, (uint16_t)(0xFDB0u + entry),
                                video_test_blue_red(salt, entry)));
    }
}

static void check_video_test_palette(const uint8_t *palettes, unsigned row, uint8_t salt,
                                     const char *name)
{
    bool matches = true;
    for (unsigned entry = 0u; entry < 16u; entry++) {
        matches = matches &&
                  palettes[row * 32u + entry] == video_test_green(salt, entry) &&
                  palettes[row * 32u + 16u + entry] == video_test_blue_red(salt, entry);
    }
    CHECK_NAMED(matches, name);
}

static lynx_machine *init_video_geometry_machine(void)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    }
    return m;
}

static void set_video_test_address(lynx_machine *m, uint16_t address)
{
    CHECK(lynx__video_write(&m->video, 0xFD94u, (uint8_t)address));
    CHECK(lynx__video_write(&m->video, 0xFD95u, (uint8_t)(address >> 8)));
}

static void case_video_geometry(void)
{
    const int failures_before = check_failures;
    const uint64_t line_ticks_expected = 32u * 16u;
    const uint64_t frame_ticks_expected = line_ticks_expected * 105u;
    const uint16_t requested_base = 0xFC03u;
    const uint16_t aligned_base = 0xFC00u;
    uint8_t pens_storage[VIDEO_TEST_GUARD + LYNX_FRAME_PENS_CAPACITY + VIDEO_TEST_GUARD];
    uint8_t palettes_storage[VIDEO_TEST_GUARD + LYNX_FRAME_PALETTE_CAPACITY + VIDEO_TEST_GUARD];
    uint64_t ticks_storage[VIDEO_TEST_GUARD / sizeof(uint64_t) +
                           LYNX_FRAME_LINE_TICKS_CAPACITY + VIDEO_TEST_GUARD / sizeof(uint64_t)];
    uint8_t *pens = pens_storage + VIDEO_TEST_GUARD;
    uint8_t *palettes = palettes_storage + VIDEO_TEST_GUARD;
    uint64_t *line_ticks = ticks_storage + VIDEO_TEST_GUARD / sizeof(uint64_t);
    lynx_frame_buffers buffers;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    memset(pens_storage, 0xC7, sizeof pens_storage);
    memset(palettes_storage, 0xC7, sizeof palettes_storage);
    memset(ticks_storage, 0xC7, sizeof ticks_storage);

    lynx_machine *m = init_video_geometry_machine();
    if (m != NULL) {
        /* Address selection and DMA use underlying RAM, even across device
           overlays and 16-bit wrap. This distinct byte proves MAPCTL still
           exposes the Mikey register to the CPU at the same address. */
        for (unsigned i = 0u; i < LYNX_FRAME_PENS_CAPACITY; i++) {
            const uint16_t addr = (uint16_t)(aligned_base + i);
            m->ram[addr] = ((i / LYNX_FRAME_ROW_BYTES + i % LYNX_FRAME_ROW_BYTES) & 1u) != 0u ?
                               0xF0u : 0x0Fu;
        }
        m->ram[requested_base] = 0xABu;
        m->ram[0xFD92u] = 0x3Cu;
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        CHECK(!lynx__video_write(&m->video, 0xFD92u, 3u));
        CHECK_NAMED(m->video.dispctl == 1u &&
                        lynx__video_peek(&m->video, 0xFD92u) == 1u,
                    "video.unsupported-flip-mode-refuses-atomically");
        CHECK(!lynx__video_write(&m->video, 0xFD92u, 0x0Du));
        CHECK_NAMED(m->video.dispctl == 1u &&
                        lynx__video_peek(&m->video, 0xFD92u) == 1u,
                    "video.unsupported-color-mode-refuses-atomically");
        set_video_test_address(m, requested_base);
        write_video_test_palette(m, 2u);
        configure_video_cadence(m, 31u, 104u);

        uint8_t cpu_overlay = 0u;
        CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFD92u, &cpu_overlay, 1u), LYNX_OK);
        CHECK_EQ_U64(cpu_overlay, 1u);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected + 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u &&
                        result.sequence == 0u,
                    "video.geometry-first-sync-is-empty");
        const uint64_t first_frame_tick = result.frame_end_tick;

        /* Event exactly at target is deferred. The start address is not
           transferred until Timer 0 reaches the third blank-line edge. */
        CHECK_EQ_U64(lynx_advance_frame(m, 2u * line_ticks_expected, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && m->video.dispaddr == 0u,
                    "video.geometry-address-before-third-blank");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(m->video.dispaddr == aligned_base,
                    "video.geometry-address-masked-at-third-blank");
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected -
                                               2u * line_ticks_expected + 1u,
                                         &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 102u &&
                        result.flags == 0u && result.frame_start_tick == first_frame_tick &&
                        result.frame_end_tick == first_frame_tick + frame_ticks_expected,
                    "video.geometry-102-visible-rows");
        bool bytes_match = true;
        for (unsigned i = 0u; i < LYNX_FRAME_PENS_CAPACITY; i++) {
            bytes_match = bytes_match && pens[i] == m->ram[(uint16_t)(aligned_base + i)];
        }
        CHECK_NAMED(bytes_match, "video.geometry-raw-nibbles-normal-traversal-and-wrap");
        CHECK_NAMED(pens[0] == 0x0Fu && pens[1] == 0xF0u &&
                        pens[LYNX_FRAME_ROW_BYTES] == 0xF0u &&
                        pens[101u * LYNX_FRAME_ROW_BYTES] == 0xF0u,
                    "video.geometry-row0-row1-row101-pen-order");
        CHECK_NAMED(pens[5u * LYNX_FRAME_ROW_BYTES + 2u] == 0x3Cu,
                    "video.geometry-dma-reads-underlying-ram-through-overlay");
        CHECK_NAMED(line_ticks[0] == first_frame_tick + 3u * line_ticks_expected &&
                        line_ticks[1] == first_frame_tick + 4u * line_ticks_expected &&
                        line_ticks[101] == first_frame_tick + 104u * line_ticks_expected,
                    "video.geometry-exact-row-start-ticks");
        check_video_test_palette(palettes, 0u, 2u, "video.geometry-row0-palette");
        check_video_test_palette(palettes, 1u, 2u, "video.geometry-row1-palette");
        check_video_test_palette(palettes, 101u, 2u, "video.geometry-row101-palette");
        bool guards_intact = true;
        for (unsigned i = 0u; i < VIDEO_TEST_GUARD; i++) {
            guards_intact = guards_intact && pens_storage[i] == 0xC7u &&
                            pens_storage[VIDEO_TEST_GUARD + LYNX_FRAME_PENS_CAPACITY + i] == 0xC7u &&
                            palettes_storage[i] == 0xC7u &&
                            palettes_storage[VIDEO_TEST_GUARD + LYNX_FRAME_PALETTE_CAPACITY + i] == 0xC7u;
        }
        for (unsigned i = 0u; i < VIDEO_TEST_GUARD / sizeof(uint64_t); i++) {
            guards_intact = guards_intact && ticks_storage[i] == UINT64_C(0xC7C7C7C7C7C7C7C7) &&
                            ticks_storage[VIDEO_TEST_GUARD / sizeof(uint64_t) +
                                          LYNX_FRAME_LINE_TICKS_CAPACITY + i] ==
                                UINT64_C(0xC7C7C7C7C7C7C7C7);
        }
        CHECK_NAMED(guards_intact, "video.geometry-row103-buffer-canaries");
        lynx_machine_deinit(m);
    }

    /* A disabled display produces a real zero-row frame rather than a
       synthesized black image. */
    m = init_video_geometry_machine();
    if (m != NULL) {
        CHECK(lynx__video_write(&m->video, 0xFD92u, 0u));
        configure_video_cadence(m, 31u, 104u);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected + 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u &&
                        result.flags == 0u,
                    "video.geometry-blank-frame-has-zero-rows");
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected + 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u &&
                        result.flags == 0u,
                    "video.geometry-next-blank-frame-remains-empty");
        lynx_machine_deinit(m);
    }

    /* A four-line programmed interval completes row 0 and reports exactly
       one actual row; row_count follows completed DMA, not nominal geometry. */
    m = init_video_geometry_machine();
    if (m != NULL) {
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        set_video_test_address(m, 0x2000u);
        for (unsigned i = 0u; i < LYNX_FRAME_ROW_BYTES; i++) m->ram[0x2000u + i] = 0x0Fu;
        configure_video_cadence(m, 31u, 3u);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 4u * line_ticks_expected + 1u,
                                        &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u,
                    "video.geometry-short-sync-empty");
        CHECK_EQ_U64(lynx_advance_frame(m, 4u * line_ticks_expected + 1u,
                                        &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 1u &&
                        result.flags == 0u && pens[0] == 0x0Fu && pens[79] == 0x0Fu,
                    "video.geometry-short-frame-one-actual-row");
        lynx_machine_deinit(m);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.video_geometry\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_palette_boundary(void)
{
    const int failures_before = check_failures;
    const uint64_t line_ticks_expected = 32u * 16u;
    const uint64_t frame_ticks_expected = line_ticks_expected * 105u;
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    lynx_machine *m = init_video_geometry_machine();
    if (m != NULL) {
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        set_video_test_address(m, 0x2000u);
        for (unsigned i = 0u; i < LYNX_FRAME_PENS_CAPACITY; i++) {
            m->ram[0x2000u + i] = ((i / LYNX_FRAME_ROW_BYTES + i % LYNX_FRAME_ROW_BYTES) & 1u) != 0u ?
                                      0xF0u : 0x0Fu;
        }
        write_video_test_palette(m, 1u);
        configure_video_cadence(m, 31u, 104u);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks_expected + 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u,
                    "video.palette-first-sync-empty");
        const uint64_t frame_start = result.frame_end_tick;

        /* A write made after reaching the half-open row boundary is visible
           to the timer event that is still deferred at that exact tick. */
        CHECK_EQ_U64(lynx_advance_frame(m, 3u * line_ticks_expected, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.palette-row0-boundary-deferred");
        write_video_test_palette(m, 2u);
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.palette-row0-snapshot-completed");
        write_video_test_palette(m, 3u);
        CHECK_EQ_U64(lynx_advance_frame(m, line_ticks_expected - 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.palette-row1-boundary-deferred");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.palette-row1-snapshot-completed");
        write_video_test_palette(m, 4u);
        const uint64_t to_frame_end = frame_ticks_expected - 4u * line_ticks_expected + 1u;
        CHECK_EQ_U64(lynx_advance_frame(m, to_frame_end, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 102u &&
                        result.frame_start_tick == frame_start,
                    "video.palette-capture-frame-complete");
        CHECK_NAMED(pens[0] == 0x0Fu && pens[1] == 0xF0u &&
                        pens[LYNX_FRAME_ROW_BYTES] == 0xF0u,
                    "video.palette-asymmetric-pen-nibbles");
        CHECK_NAMED(line_ticks[0] == frame_start + 3u * line_ticks_expected &&
                        line_ticks[1] == frame_start + 4u * line_ticks_expected &&
                        line_ticks[2] == frame_start + 5u * line_ticks_expected &&
                        line_ticks[101] == frame_start + 104u * line_ticks_expected,
                    "video.palette-selected-row-edges");
        check_video_test_palette(palettes, 0u, 2u, "video.palette-before-edge-selects-row0");
        check_video_test_palette(palettes, 1u, 3u, "video.palette-after-row0-selects-row1");
        check_video_test_palette(palettes, 2u, 4u, "video.palette-after-row1-selects-row2");
        check_video_test_palette(palettes, 101u, 4u, "video.palette-asymmetric-row101");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.palette_boundary\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_palette_fault(void)
{
    const int failures_before = check_failures;
    const uint64_t line_ticks = 32u * 16u;
    const uint64_t frame_ticks = line_ticks * 105u;
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks_out[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks_out);
    lynx_machine *m = init_video_geometry_machine();
    if (m != NULL) {
        CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
        set_video_test_address(m, 0x2000u);
        for (unsigned i = 0u; i < LYNX_FRAME_PENS_CAPACITY; i++) {
            m->ram[0x2000u + i] = (uint8_t)(0x3Cu ^ (uint8_t)i);
        }
        write_video_test_palette(m, 1u);
        configure_video_cadence(m, 31u, 104u);
        lynx_frame_result result;
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks + 1u, &buffers, &result),
                     LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 0u,
                    "video.fault.palette-sync-frame-is-empty");
        CHECK_EQ_U64(lynx_advance_frame(m, 3u * line_ticks, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.fault.palette-row-zero-edge-is-deferred");
        write_video_test_palette(m, 2u);
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET,
                    "video.fault.palette-row-zero-boundary-is-processed");
        write_video_test_palette(m, 3u);
        CHECK_EQ_U64(lynx_advance_frame(m, frame_ticks - 3u * line_ticks,
                                         &buffers, &result), LYNX_OK);
        bool row_zero_matches = true;
        for (unsigned entry = 0u; entry < 16u; entry++) {
            row_zero_matches = row_zero_matches &&
                               palettes[entry] == video_test_green(2u, entry) &&
                               palettes[16u + entry] == video_test_blue_red(2u, entry);
        }
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.row_count == 102u &&
                        row_zero_matches,
                    "video.fault.palette-row-boundary-late");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.video_palette_fault\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_timer_pending_after_disable(void)
{
    const int failures_before = check_failures;
    static const uint8_t irq_probe[] = {0x58u, 0x4Cu, 0x01u, 0x02u};
    lynx_config cfg;
    lynx_config_default(&cfg);
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) {
        goto done;
    }
    CHECK_EQ_U64(load_bll_at(m, irq_probe, sizeof irq_probe, 0x0200u), LYNX_OK);
    m->ram[0xFFFEu] = 0x00u;
    m->ram[0xFFFFu] = 0x03u;
    m->ram[0x0300u] = 0x4Cu;
    m->ram[0x0301u] = 0x00u;
    m->ram[0x0302u] = 0x03u;
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK(timer_write_required(&m->timer_bank, 0xFD02u, 0u));
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x88u));

    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 17u, &consumed, &stop), LYNX_OK);
    CHECK_EQ_U64(consumed, 17u);
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK(timer_write_required(&m->timer_bank, 0xFD01u, 0x08u));
    CHECK_EQ_U64(m->timer_bank.pending, 0x01u);
    CHECK_EQ_U64(m->timer_bank.timers[0].control_a, 0x08u);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_advance(m, 200u, &consumed, &stop), LYNX_OK);
    CHECK_NAMED(m->cpu.s == 0xFAu, "timer.irq.pending-independent-of-enable");

done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.timer_pending_after_disable\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void check_refused_frame(lynx_machine *machine, const lynx_frame_buffers *buffers,
                                lynx_frame_result *result, lynx_status expected,
                                const char *name, bool check_payloads)
{
    static uint8_t machine_before[sizeof(struct lynx_machine)];
    static uint8_t descriptor_before[sizeof(lynx_frame_buffers)];
    static uint8_t result_before[sizeof(lynx_frame_result)];
    static uint8_t pens_before[LYNX_FRAME_PENS_CAPACITY];
    static uint8_t palettes_before[LYNX_FRAME_PALETTE_CAPACITY];
    static uint8_t ticks_before[LYNX_FRAME_LINE_TICKS_CAPACITY * sizeof(uint64_t)];
    if (machine != NULL) memcpy(machine_before, machine, sizeof *machine);
    if (buffers != NULL) memcpy(descriptor_before, buffers, sizeof *buffers);
    if (result != NULL) memcpy(result_before, result, sizeof *result);
    if (check_payloads && buffers != NULL) {
        memcpy(pens_before, buffers->pens, sizeof pens_before);
        memcpy(palettes_before, buffers->palettes, sizeof palettes_before);
        memcpy(ticks_before, buffers->line_ticks, sizeof ticks_before);
    }

    CHECK_EQ_U64(lynx_advance_frame(machine, 1u, buffers, result), expected);
    if (machine != NULL) {
        CHECK_NAMED(memcmp(machine, machine_before, sizeof *machine) == 0,
                    name);
    }
    if (buffers != NULL) {
        CHECK_NAMED(memcmp(buffers, descriptor_before, sizeof *buffers) == 0,
                    name);
    }
    if (result != NULL) {
        CHECK_NAMED(memcmp(result, result_before, sizeof *result) == 0,
                    name);
    }
    if (check_payloads && buffers != NULL) {
        CHECK_NAMED(memcmp(buffers->pens, pens_before, sizeof pens_before) == 0,
                    name);
        CHECK_NAMED(memcmp(buffers->palettes, palettes_before, sizeof palettes_before) == 0,
                    name);
        CHECK_NAMED(memcmp(buffers->line_ticks, ticks_before, sizeof ticks_before) == 0,
                    name);
    }
}

static void case_frame_refusal(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result result;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    init_video_frame_result(&result);
    memset(pens, 0xA5, sizeof pens);
    memset(palettes, 0x5A, sizeof palettes);
    memset(line_ticks, 0xC3, sizeof line_ticks);
    static uint8_t null_case_pens_before[sizeof pens];
    static uint8_t null_case_palettes_before[sizeof palettes];
    static uint8_t null_case_ticks_before[sizeof line_ticks];
    static uint8_t null_case_buffers_before[sizeof buffers];
    static uint8_t null_case_result_before[sizeof result];
    memcpy(null_case_pens_before, pens, sizeof pens);
    memcpy(null_case_palettes_before, palettes, sizeof palettes);
    memcpy(null_case_ticks_before, line_ticks, sizeof line_ticks);
    memcpy(null_case_buffers_before, &buffers, sizeof buffers);
    result.stop = LYNX_STOP_HALT;
    result.ticks_consumed = 17u;
    result.frame_start_tick = 19u;
    result.frame_end_tick = 23u;
    result.sequence = 29u;
    result.row_count = 31u;
    result.flags = 37u;
    memcpy(null_case_result_before, &result, sizeof result);

    if (m == NULL) goto done;
    CHECK_EQ_U64(lynx_advance_frame(NULL, 1u, &buffers, &result), LYNX_ERR_ARGUMENT);
    CHECK_NAMED(memcmp(pens, null_case_pens_before, sizeof pens) == 0 &&
                    memcmp(palettes, null_case_palettes_before, sizeof palettes) == 0 &&
                    memcmp(line_ticks, null_case_ticks_before, sizeof line_ticks) == 0 &&
                    memcmp(&buffers, null_case_buffers_before, sizeof buffers) == 0 &&
                    memcmp(&result, null_case_result_before, sizeof result) == 0,
                "frame_refusal.null-machine-preserves-outputs");
    check_refused_frame(m, NULL, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.null-descriptor-transactional", true);
    CHECK_NAMED(memcmp(pens, null_case_pens_before, sizeof pens) == 0 &&
                    memcmp(palettes, null_case_palettes_before, sizeof palettes) == 0 &&
                    memcmp(line_ticks, null_case_ticks_before, sizeof line_ticks) == 0,
                "frame_refusal.null-descriptor-preserves-caller-payloads");
    check_refused_frame(m, &buffers, NULL, LYNX_ERR_ARGUMENT,
                        "frame_refusal.null-result-transactional", true);
    check_refused_frame(m, &buffers, &result, LYNX_ERR_STATE,
                        "frame_refusal.no-content-state-precedence", true);

    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    buffers.struct_size--;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.descriptor-size", true);
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.reserved = 1u;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.descriptor-reserved", true);
    buffers.reserved = 0u;
    result.struct_size--;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.result-size", true);
    result.struct_size = (uint32_t)sizeof result;

    uint8_t *saved_pens = buffers.pens;
    size_t saved_pens_capacity = buffers.pens_capacity;
    buffers.pens = NULL;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.null-pens", false);
    buffers.pens = saved_pens;
    buffers.pens_capacity = saved_pens_capacity;
    uint8_t *saved_palettes = buffers.palettes;
    buffers.palettes = NULL;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.null-palettes", false);
    buffers.palettes = saved_palettes;
    uint64_t *saved_ticks = buffers.line_ticks;
    buffers.line_ticks = NULL;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.null-line-ticks", false);
    buffers.line_ticks = saved_ticks;

    buffers.pens_capacity = LYNX_FRAME_PENS_CAPACITY - 1u;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_CAPACITY,
                        "frame_refusal.pen-capacity", true);
    buffers.pens_capacity = LYNX_FRAME_PENS_CAPACITY;
    buffers.palettes_capacity = LYNX_FRAME_PALETTE_CAPACITY - 1u;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_CAPACITY,
                        "frame_refusal.palette-capacity", true);
    buffers.palettes_capacity = LYNX_FRAME_PALETTE_CAPACITY;
    buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY - 1u;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_CAPACITY,
                        "frame_refusal.tick-capacity", true);
    buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;

    _Alignas(uint64_t) uint8_t misaligned_ticks[sizeof line_ticks + _Alignof(uint64_t)];
    buffers.line_ticks = (uint64_t *)(void *)(misaligned_ticks + 1u);
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ALIGNMENT,
                        "frame_refusal.tick-alignment", false);
    buffers.line_ticks = saved_ticks;

    buffers.pens = (uint8_t *)(uintptr_t)(UINTPTR_MAX -
                                          (LYNX_FRAME_PENS_CAPACITY - 1u));
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.pen-span-overflow", false);
    buffers.pens = saved_pens;
    buffers.palettes = (uint8_t *)(uintptr_t)(UINTPTR_MAX -
                                              (LYNX_FRAME_PALETTE_CAPACITY - 1u));
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.palette-span-overflow", false);
    buffers.palettes = saved_palettes;
    buffers.line_ticks = (uint64_t *)(uintptr_t)(UINTPTR_MAX -
                                                  (sizeof line_ticks - 1u));
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.tick-span-overflow", false);
    buffers.line_ticks = saved_ticks;
    buffers.line_ticks_capacity = SIZE_MAX / sizeof(uint64_t) + 1u;
    buffers.line_ticks = (uint64_t *)(uintptr_t)0x1000u;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.tick-size-overflow", false);
    buffers.line_ticks = saved_ticks;
    buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;

    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PENS_CAPACITY];
    } aliases;
    memset(&aliases, 0x6D, sizeof aliases);
    buffers.pens = aliases.bytes;
    buffers.palettes = aliases.bytes;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.pen-palette-overlap", true);
    buffers.pens = saved_pens;
    buffers.palettes = saved_palettes;
    buffers.pens = aliases.bytes;
    buffers.line_ticks = (uint64_t *)(void *)aliases.bytes;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.pen-ticks-overlap", true);
    buffers.pens = saved_pens;
    buffers.line_ticks = saved_ticks;
    buffers.palettes = aliases.bytes;
    buffers.line_ticks = (uint64_t *)(void *)aliases.bytes;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.palette-ticks-overlap", true);
    buffers.palettes = saved_palettes;
    buffers.line_ticks = saved_ticks;

    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PENS_CAPACITY];
        lynx_frame_buffers descriptor;
    } descriptor_alias;
    memset(&descriptor_alias, 0, sizeof descriptor_alias);
    init_video_frame_buffers(&descriptor_alias.descriptor, descriptor_alias.bytes,
                             palettes, line_ticks);
    check_refused_frame(m, &descriptor_alias.descriptor, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.descriptor-in-pens", true);

    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PENS_CAPACITY];
        lynx_frame_result result;
    } result_alias;
    memset(&result_alias, 0xA5, sizeof result_alias);
    init_video_frame_result(&result_alias.result);
    init_video_frame_buffers(&buffers, result_alias.bytes, palettes, line_ticks);
    check_refused_frame(m, &buffers, &result_alias.result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.result-in-pens", true);

    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PALETTE_CAPACITY];
        lynx_frame_buffers descriptor;
    } palette_descriptor_alias;
    memset(&palette_descriptor_alias, 0, sizeof palette_descriptor_alias);
    init_video_frame_buffers(&palette_descriptor_alias.descriptor, pens,
                             palette_descriptor_alias.bytes, line_ticks);
    check_refused_frame(m, &palette_descriptor_alias.descriptor, &result,
                        LYNX_ERR_ARGUMENT,
                        "frame_refusal.descriptor-in-palettes", true);

    union {
        max_align_t alignment;
        uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        lynx_frame_buffers descriptor;
    } ticks_descriptor_alias;
    memset(&ticks_descriptor_alias, 0, sizeof ticks_descriptor_alias);
    init_video_frame_buffers(&ticks_descriptor_alias.descriptor, pens, palettes,
                             ticks_descriptor_alias.ticks);
    check_refused_frame(m, &ticks_descriptor_alias.descriptor, &result,
                        LYNX_ERR_ARGUMENT, "frame_refusal.descriptor-in-ticks", true);

    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PALETTE_CAPACITY];
        lynx_frame_result result;
    } result_palette_alias;
    memset(&result_palette_alias, 0, sizeof result_palette_alias);
    init_video_frame_result(&result_palette_alias.result);
    init_video_frame_buffers(&buffers, pens, result_palette_alias.bytes, line_ticks);
    check_refused_frame(m, &buffers, &result_palette_alias.result,
                        LYNX_ERR_ARGUMENT, "frame_refusal.result-in-palettes", true);

    union {
        max_align_t alignment;
        uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        lynx_frame_result result;
    } result_ticks_alias;
    memset(&result_ticks_alias, 0, sizeof result_ticks_alias);
    init_video_frame_result(&result_ticks_alias.result);
    init_video_frame_buffers(&buffers, pens, palettes, result_ticks_alias.ticks);
    check_refused_frame(m, &buffers, &result_ticks_alias.result,
                        LYNX_ERR_ARGUMENT, "frame_refusal.result-in-ticks", true);

    struct frame_spare_view {
        uint8_t fixed_pens[LYNX_FRAME_PENS_CAPACITY];
        lynx_frame_result result;
    };
    union {
        max_align_t alignment;
        uint8_t bytes[LYNX_FRAME_PENS_CAPACITY + sizeof(lynx_frame_result)];
        struct frame_spare_view view;
    } spare_alias;
    _Static_assert(offsetof(struct frame_spare_view, result) == LYNX_FRAME_PENS_CAPACITY,
                   "pen capacity keeps the result aligned in the spare-capacity alias");
    memset(&spare_alias, 0x3C, sizeof spare_alias);
    init_video_frame_result(&spare_alias.view.result);
    init_video_frame_buffers(&buffers, spare_alias.bytes, palettes, line_ticks);
    buffers.pens_capacity = sizeof spare_alias.bytes;
    check_refused_frame(m, &buffers, &spare_alias.view.result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.result-in-spare-pen-capacity", false);

    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    check_refused_frame(m, (lynx_frame_buffers *)(void *)m, &result,
                        LYNX_ERR_ARGUMENT, "frame_refusal.descriptor-in-machine", false);
    check_refused_frame(m, &buffers, (lynx_frame_result *)(void *)m,
                        LYNX_ERR_ARGUMENT, "frame_refusal.result-in-machine", false);
    buffers.pens = (uint8_t *)(void *)m;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.pens-in-machine", false);
    buffers.pens = saved_pens;
    buffers.palettes = (uint8_t *)(void *)m;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.palettes-in-machine", false);
    buffers.palettes = saved_palettes;
    buffers.line_ticks = (uint64_t *)(void *)m;
    check_refused_frame(m, &buffers, &result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.ticks-in-machine", false);
    buffers.line_ticks = saved_ticks;

    union {
        max_align_t alignment;
        lynx_frame_buffers buffers;
        lynx_frame_result result;
    } descriptor_result_alias;
    memset(&descriptor_result_alias, 0xA5, sizeof descriptor_result_alias);
    check_refused_frame(m, &descriptor_result_alias.buffers,
                        &descriptor_result_alias.result, LYNX_ERR_ARGUMENT,
                        "frame_refusal.descriptor-result-overlap", false);

    lynx_config legacy_cfg;
    lynx_config_default(&legacy_cfg);
    _Alignas(max_align_t) uint8_t legacy_machine_storage[sizeof(struct lynx_machine)];
    lynx_machine *legacy = init_in(legacy_machine_storage, &legacy_cfg);
    if (legacy != NULL) {
        CHECK_EQ_U64(lynx_load_bll(legacy, bll_tracer, sizeof bll_tracer), LYNX_OK);
        init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
        memset(pens, 0xA5, sizeof pens);
        memset(palettes, 0x5A, sizeof palettes);
        memset(line_ticks, 0xC3, sizeof line_ticks);
        init_video_frame_result(&result);
        result.sequence = 29u;
        check_refused_frame(legacy, &buffers, &result, LYNX_ERR_UNAVAILABLE,
                            "frame_refusal.legacy-policy-unavailable", true);
        lynx_machine_deinit(legacy);
    }

    struct guarded_frame_ticks {
        uint64_t entries[LYNX_FRAME_LINE_TICKS_CAPACITY];
        uint64_t canary[2];
    } guarded_ticks;
    uint8_t accepted_pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t accepted_palettes[LYNX_FRAME_PALETTE_CAPACITY];
    lynx_frame_buffers accepted_buffers;
    lynx_frame_result accepted_result;
    init_video_frame_buffers(&accepted_buffers, accepted_pens, accepted_palettes,
                             guarded_ticks.entries);
    init_video_frame_result(&accepted_result);
    memset(accepted_pens, 0xA5, sizeof accepted_pens);
    memset(accepted_palettes, 0x5A, sizeof accepted_palettes);
    memset(&guarded_ticks, 0xC3, sizeof guarded_ticks);
    guarded_ticks.canary[0] = UINT64_C(0x123456789ABCDEF0);
    guarded_ticks.canary[1] = UINT64_C(0x0FEDCBA987654321);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    configure_video_cadence(m, 158u, 104u);
    const uint64_t first_frame_tick = UINT64_C(159) * 16u * 105u;
    CHECK_EQ_U64(lynx_advance_frame(m, first_frame_tick + 1u, &accepted_buffers,
                                   &accepted_result), LYNX_OK);
    CHECK_NAMED(accepted_result.stop == LYNX_STOP_FRAME &&
                    accepted_result.ticks_consumed == first_frame_tick &&
                    accepted_result.frame_end_tick == first_frame_tick &&
                    accepted_result.sequence == 0u && accepted_result.row_count == 0u,
                "frame_refusal.accepted-frame-progresses-and-publishes");
    bool accepted_payload_zero = true;
    for (size_t i = 0u; i < sizeof accepted_pens; i++) {
        if (accepted_pens[i] != 0u) accepted_payload_zero = false;
    }
    for (size_t i = 0u; i < sizeof accepted_palettes; i++) {
        if (accepted_palettes[i] != 0u) accepted_payload_zero = false;
    }
    for (size_t i = 0u; i < sizeof guarded_ticks.entries / sizeof guarded_ticks.entries[0]; i++) {
        if (guarded_ticks.entries[i] != 0u) accepted_payload_zero = false;
    }
    CHECK_NAMED(accepted_payload_zero,
                "frame_refusal.accepted-frame-publishes-zero-sync-payload");
    CHECK_NAMED(guarded_ticks.canary[0] == UINT64_C(0x123456789ABCDEF0) &&
                    guarded_ticks.canary[1] == UINT64_C(0x0FEDCBA987654321),
                "frame_refusal.tick-capacity-canaries");

    lynx_machine_deinit(m);
    m = NULL;

done:
    printf("TAP version 13\n1..1\n%s 1 - api.frame_refusal\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_refusal(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) goto done;

    static uint8_t before[sizeof(struct lynx_machine)];
    memcpy(before, m, sizeof *m);
    CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_MASK + 1u), LYNX_ERR_ARGUMENT);
    CHECK_NAMED(memcmp(before, m, sizeof *m) == 0,
                "input_refusal.unknown-bits-preserve-machine");
    CHECK_EQ_U64(lynx_set_input(NULL, LYNX_INPUT_A), LYNX_ERR_ARGUMENT);
    CHECK_NAMED(memcmp(before, m, sizeof *m) == 0,
                "input_refusal.null-machine-preserves-instance");

    CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_MASK), LYNX_OK);
    CHECK_EQ_U64(m->video.input_mask, LYNX_INPUT_MASK);
    CHECK_EQ_U64(ticks_of(m), 0u);
    memcpy(before, m, sizeof *m);
    CHECK_EQ_U64(lynx_set_input(m, 0x200u), LYNX_ERR_ARGUMENT);
    CHECK_NAMED(memcmp(before, m, sizeof *m) == 0,
                "input_refusal.invalid-after-valid-preserves-machine");

    lynx_machine_deinit(m);
    memcpy(before, m, sizeof *m);
    CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_A), LYNX_ERR_STATE);
    CHECK_NAMED(memcmp(before, m, sizeof *m) == 0,
                "input_refusal.deinitialized-preserves-machine");

done:
    printf("TAP version 13\n1..1\n%s 1 - api.input_refusal\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_state_digest(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    _Alignas(max_align_t) unsigned char storage_a[sizeof(struct lynx_machine)];
    _Alignas(max_align_t) unsigned char storage_b[sizeof(struct lynx_machine)];
    lynx_machine *a = init_in(storage_a, &cfg);
    lynx_machine *b = init_in(storage_b, &cfg);
    if (a == NULL || b == NULL) goto done;

    CHECK_EQ_U64(lynx_set_input(a, 0u), LYNX_OK);
    CHECK_EQ_U64(lynx_set_input(b, 0u), LYNX_OK);
    uint8_t state_a[32];
    uint8_t state_b[32];
    memset(state_a, 0xA5, sizeof state_a);
    memset(state_b, 0x5A, sizeof state_b);
    CHECK_EQ_U64(lynx_state_digest(a, state_a), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(b, state_b), LYNX_OK);
    CHECK_NAMED(memcmp(state_a, state_b, sizeof state_a) == 0,
                "state_digest.equal-initial-state-agrees");

    CHECK_EQ_U64(lynx_watch_configure(b, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(b, state_b), LYNX_OK);
    CHECK_NAMED(memcmp(state_a, state_b, sizeof state_a) == 0,
                "state_digest.host-watch-excluded");

    const bundle_run run = {"input-sensitivity", 0u, 0u, LYNX_STOP_BUDGET};
    char digest_a[65] = {0};
    char digest_b[65] = {0};
    CHECK_EQ_U64(bundle_observe(a, &run, digest_a), 0u);
    CHECK_EQ_U64(bundle_observe(b, &run, digest_b), 0u);
    CHECK_NAMED(strcmp(digest_a, digest_b) == 0,
                "state_digest.host-watch-excluded-from-runner-observation");

    CHECK_EQ_U64(lynx_set_input(b, LYNX_INPUT_A), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(b, state_b), LYNX_OK);
    CHECK_NAMED(memcmp(state_a, state_b, sizeof state_a) != 0,
                "state_digest.input-changes-canonical-digest");
    CHECK_EQ_U64(bundle_observe(a, &run, digest_a), 0u);
    CHECK_EQ_U64(bundle_observe(b, &run, digest_b), 0u);
    CHECK_NAMED(memcmp(digest_a, digest_b, 64u) != 0,
                "state_digest.input-changes-runner-observation");

    unsigned char before[sizeof(struct lynx_machine)];
    uint8_t output_canary[32];
    memcpy(before, a, sizeof before);
    memset(output_canary, 0xC3, sizeof output_canary);
    memset(state_b, 0xC3, sizeof state_b);
    CHECK_EQ_U64(lynx_state_digest(a, NULL), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_state_digest(NULL, state_b), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_state_digest(a, (uint8_t *)a), LYNX_ERR_ARGUMENT);
    CHECK_EQ_U64(lynx_state_digest(a,
                                  (uint8_t *)(uintptr_t)(UINTPTR_MAX - 15u)),
                 LYNX_ERR_ARGUMENT);
    CHECK_NAMED(memcmp(state_b, output_canary, sizeof state_b) == 0,
                "state_digest.refusals-preserve-output");
    CHECK_NAMED(memcmp(before, a, sizeof before) == 0,
                "state_digest.refusals-preserve-machine");

    lynx_machine_deinit(a);
    CHECK_EQ_U64(lynx_state_digest(a, state_b), LYNX_ERR_STATE);
    CHECK_NAMED(memcmp(state_b, output_canary, sizeof state_b) == 0,
                "state_digest.uninitialised-preserves-output");
    a = NULL;

done:
    if (a != NULL) lynx_machine_deinit(a);
    if (b != NULL) lynx_machine_deinit(b);
    printf("TAP version 13\n1..1\n%s 1 - api.state_digest\n",
           check_failures == failures_before ? "ok" : "not ok");
}

/* The guest configures MAPCTL and SPRSYS, then reads the real CPU-mapped
   SPRSYS/JOYSTICK/SWITCHES routes. It repeats the two input reads so the host
   can replace levels only while the next CPU data request is pending. */
static size_t make_input_read_program(uint8_t *program, uint8_t mapctl,
                                     uint8_t lefthand)
{
    size_t n = 0u;
    program[n++] = 0xA9u; program[n++] = mapctl;       /* LDA #mapctl */
    program[n++] = 0x8Du; program[n++] = 0xF9u; program[n++] = 0xFFu;
    program[n++] = 0xA9u; program[n++] = lefthand;     /* LDA #SPRSYS input bit */
    program[n++] = 0x8Du; program[n++] = 0x92u; program[n++] = 0xFCu;
    program[n++] = 0xADu; program[n++] = 0x92u; program[n++] = 0xFCu;
    const uint16_t loop = (uint16_t)(0x0200u + (uint16_t)n);
    program[n++] = 0xADu; program[n++] = 0xB0u; program[n++] = 0xFCu;
    program[n++] = 0xADu; program[n++] = 0xB1u; program[n++] = 0xFCu;
    program[n++] = 0x4Cu; program[n++] = (uint8_t)loop;
    program[n++] = (uint8_t)(loop >> 8);
    return n;
}

static lynx_status load_input_read_guest(lynx_machine *m, uint8_t mapctl,
                                         uint8_t lefthand)
{
    uint8_t program[32];
    uint8_t object[sizeof program + 10u];
    const size_t length = make_input_read_program(program, mapctl, lefthand);
    const size_t object_length = length + 10u;
    make_bll_header(object, object_length, 0x0200u);
    memcpy(object + 10u, program, length);
    return lynx_load_bll(m, object, object_length);
}

static int input_read_request(const lynx_machine *m, uint16_t address)
{
    return m->cpu.req_addr == address && m->cpu.req_kind == LYNX__CK_READ &&
           (m->cpu.req_flags & (uint8_t)LYNX__CF_WRITE) == 0u;
}

static void input_advance_one_tick(lynx_machine *m, const char *assertion)
{
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    const lynx_status status = lynx_advance(m, 1u, &consumed, &stop);
    CHECK_NAMED(status == LYNX_OK && consumed == 1u && stop == LYNX_STOP_BUDGET,
                assertion);
}

static void input_wait_for_read_request(lynx_machine *m, uint16_t address,
                                        const char *assertion)
{
    for (unsigned i = 0u; i < 512u && !input_read_request(m, address); i++) {
        input_advance_one_tick(m, assertion);
    }
    CHECK_NAMED(input_read_request(m, address), assertion);
}

/* Returns the tick at which the CPU request actually completed, or UINT64_MAX
   when it did not complete within the bounded guest-time window. */
static uint64_t input_complete_read(lynx_machine *m, uint16_t address,
                                    uint8_t expected, const char *assertion)
{
    for (unsigned i = 0u; i < 1024u && input_read_request(m, address); i++) {
        const uint64_t before = ticks_of(m);
        input_advance_one_tick(m, assertion);
        if (!input_read_request(m, address)) {
            CHECK_NAMED(m->cpu.a == expected, assertion);
            return before;
        }
    }
    CHECK_NAMED(!input_read_request(m, address), assertion);
    return UINT64_MAX;
}

static void input_advance_exclusive_to(lynx_machine *m, uint64_t target,
                                       const char *assertion)
{
    const uint64_t start = ticks_of(m);
    CHECK_NAMED(target >= start, assertion);
    if (target < start) return;
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    const lynx_status status = lynx_advance(m, target - start, &consumed, &stop);
    CHECK_NAMED(status == LYNX_OK && consumed == target - start &&
                    ticks_of(m) == target && stop == LYNX_STOP_BUDGET,
                assertion);
}

static uint64_t input_next_grant_tick(const lynx_machine *m)
{
    uint64_t grant = m->guest_ticks;
    if (grant < m->cpu_ready_at) grant = m->cpu_ready_at;
    if (grant < m->video.dma_busy_until) grant = m->video.dma_busy_until;
    if (grant < m->video.refresh_busy_until) grant = m->video.refresh_busy_until;
    return grant;
}

static void input_set_exact_mask(lynx_machine *m, uint32_t mask,
                                 const char *assertion)
{
    const uint64_t before_ticks = ticks_of(m);
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_set_input(m, mask), LYNX_OK);
    const size_t mask_offset = offsetof(struct lynx_machine, video) +
                               offsetof(lynx__video_state, input_mask);
    const uint16_t expected_mask = (uint16_t)mask;
    uint8_t want_mask[sizeof expected_mask];
    memcpy(want_mask, &expected_mask, sizeof want_mask);
    int only_mask_changed = 1;
    for (size_t i = 0u; i < MSIZE; i++) {
        const uint8_t expected = i >= mask_offset && i < mask_offset + sizeof want_mask
                                     ? want_mask[i - mask_offset] : snap_a[i];
        if (((const uint8_t *)(const void *)m)[i] != expected) {
            only_mask_changed = 0;
            break;
        }
    }
    CHECK_NAMED(only_mask_changed && ticks_of(m) == before_ticks, assertion);
}

/* Independently transcribed from the primary appendix and cc65 alias table.
   The expected side does not call production input helpers. */
static uint8_t expected_joystick(uint32_t mask, uint8_t lefthand)
{
    static const struct {
        uint32_t logical;
        uint8_t lefthand_on;
        uint8_t lefthand_off;
    } fields[] = {
        {LYNX_INPUT_UP,       0x80u, 0x40u},
        {LYNX_INPUT_DOWN,     0x40u, 0x80u},
        {LYNX_INPUT_LEFT,     0x20u, 0x10u},
        {LYNX_INPUT_RIGHT,    0x10u, 0x20u},
        {LYNX_INPUT_OPTION1,  0x08u, 0x08u},
        {LYNX_INPUT_OPTION2,  0x04u, 0x04u},
        {LYNX_INPUT_B,        0x02u, 0x02u},
        {LYNX_INPUT_A,        0x01u, 0x01u},
    };
    uint8_t value = 0u;
    for (size_t i = 0u; i < sizeof fields / sizeof fields[0]; i++) {
        if ((mask & fields[i].logical) != 0u) {
            value |= lefthand != 0u ? fields[i].lefthand_on : fields[i].lefthand_off;
        }
    }
    return value;
}

static uint8_t expected_switches(uint32_t mask)
{
    return (mask & LYNX_INPUT_PAUSE) != 0u ? 0x01u : 0u;
}

static void case_input_bits(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) goto done;

    for (uint8_t lefthand = 0u; lefthand <= 1u; lefthand++) {
        CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, lefthand != 0u ? 0x08u : 0u), LYNX_OK);
        input_wait_for_read_request(m, 0xFC92u, "input_bits.guest-sprsys-read-request");
        (void)input_complete_read(m, 0xFC92u, lefthand != 0u ? 0x08u : 0u,
                                  "input_bits.guest-sprsys-readback");
        for (uint32_t mask = 0u; mask <= LYNX_INPUT_MASK; mask++) {
            input_wait_for_read_request(m, 0xFCB0u, "input_bits.guest-joystick-read-request");
            input_set_exact_mask(m, mask, "input_bits.setter-copies-only-levels-without-time");
            (void)input_complete_read(m, 0xFCB0u,
                                      expected_joystick(mask, lefthand),
                                      "input_bits.independent-nine-bit-joystick-table");
            input_wait_for_read_request(m, 0xFCB1u, "input_bits.guest-switches-read-request");
            (void)input_complete_read(m, 0xFCB1u, expected_switches(mask),
                                      "input_bits.pause-and-disconnected-lines");
        }
    }

    /* MAPCTL 0F is a negative device-fixture control: CPU reads must see
       underlying RAM even when logical levels are pressed. */
    CHECK_EQ_U64(load_input_read_guest(m, 0x0Fu, 0x08u), LYNX_OK);
    m->ram[0xFCB0u] = 0xA5u;
    m->ram[0xFCB1u] = 0x3Cu;
    CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_MASK), LYNX_OK);
    input_wait_for_read_request(m, 0xFC92u, "input_bits.mapctl-0f-sprsys-ram-request");
    (void)input_complete_read(m, 0xFC92u, 0x08u, "input_bits.mapctl-0f-sprsys-write-routes-to-ram");
    input_wait_for_read_request(m, 0xFCB0u, "input_bits.mapctl-0f-joystick-ram-request");
    (void)input_complete_read(m, 0xFCB0u, 0xA5u, "input_bits.mapctl-0f-joystick-is-ram");
    input_wait_for_read_request(m, 0xFCB1u, "input_bits.mapctl-0f-switches-ram-request");
    (void)input_complete_read(m, 0xFCB1u, 0x3Cu, "input_bits.mapctl-0f-switches-is-ram");

    lynx_machine_deinit(m);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.input_bits\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_orientation(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) goto done;

    CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, 0x08u), LYNX_OK);
    input_wait_for_read_request(m, 0xFC92u, "input_orientation.sprsys-read-request");
    CHECK_NAMED(m->video.lefthand == 1u,
                "input_orientation.guest-write-selects-lefthand");

    /* Raw RAM inspection and the CPU device overlay are distinct pure views. */
    m->ram[0xFCB0u] = 0xA5u;
    m->ram[0xFCB1u] = 0x5Au;
    input_set_exact_mask(m, LYNX_INPUT_UP | LYNX_INPUT_A,
                         "input_orientation.inspection-levels-installed");
    static uint8_t before_inspection[sizeof(struct lynx_machine)];
    uint8_t raw[2] = {0u, 0u};
    uint8_t mapped[2] = {0u, 0u};
    memcpy(before_inspection, m, sizeof *m);
    CHECK_EQ_U64(lynx_read_memory(m, 0xFCB0u, raw, sizeof raw), LYNX_OK);
    CHECK_EQ_U64(lynx_read_cpu_memory(m, 0xFCB0u, mapped, sizeof mapped), LYNX_OK);
    CHECK_NAMED(raw[0] == 0xA5u && raw[1] == 0x5Au &&
                    mapped[0] == 0x81u && mapped[1] == 0u &&
                    memcmp(before_inspection, m, sizeof *m) == 0,
                "input_orientation.raw-and-mapped-inspection-are-distinct-and-pure");

    input_set_exact_mask(m, LYNX_INPUT_OPTION2 | LYNX_INPUT_PAUSE,
                         "input_orientation.option2-pause-keeps-guest-orientation");
    CHECK_NAMED(m->video.lefthand == 1u,
                "input_orientation.option2-pause-does-not-auto-rotate");
    (void)input_complete_read(m, 0xFC92u, 0x08u,
                              "input_orientation.readback-exposes-only-lefthand-bit");
    input_wait_for_read_request(m, 0xFCB0u, "input_orientation.chord-joystick-request");
    (void)input_complete_read(m, 0xFCB0u, 0x04u,
                              "input_orientation.chord-is-two-independent-levels");
    input_wait_for_read_request(m, 0xFCB1u, "input_orientation.chord-switches-request");
    (void)input_complete_read(m, 0xFCB1u, 0x01u,
                              "input_orientation.pause-remains-on-switches-bit-zero");

    /* A bit-3 clear is accepted. Engine/math bits fail at the write request
       without modifying the input-owned orientation state. */
    static const uint8_t unsupported_sprsys_write[] = {
        0xA9u, 0x0Cu, 0x8Du, 0xF9u, 0xFFu,
        0xA9u, 0x08u, 0x8Du, 0x92u, 0xFCu,
        0xA9u, 0x10u, 0x8Du, 0x92u, 0xFCu,
        0x4Cu, 0x0Au, 0x02u
    };
    CHECK_EQ_U64(load_bll_at(m, unsupported_sprsys_write,
                             sizeof unsupported_sprsys_write, 0x0200u), LYNX_OK);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    for (unsigned i = 0u; i < 128u && stop != LYNX_STOP_UNAVAILABLE; i++) {
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    }
    CHECK_NAMED(stop == LYNX_STOP_UNAVAILABLE && m->video.lefthand == 1u &&
                    m->cpu.req_addr == 0xFC92u && m->cpu.req_kind == LYNX__CK_WRITE &&
                    m->cpu.a == 0x10u,
                "input_orientation.unsupported-sprsys-control-refuses-before-device-mutation");

    static const uint8_t unsupported_suzy_read[] = {
        0xA9u, 0x0Cu, 0x8Du, 0xF9u, 0xFFu,
        0xADu, 0x90u, 0xFCu,
        0x4Cu, 0x05u, 0x02u
    };
    CHECK_EQ_U64(load_bll_at(m, unsupported_suzy_read,
                             sizeof unsupported_suzy_read, 0x0200u), LYNX_OK);
    consumed = UINT64_MAX;
    stop = LYNX_STOP_HALT;
    for (unsigned i = 0u; i < 64u && stop != LYNX_STOP_UNAVAILABLE; i++) {
        CHECK_EQ_U64(lynx_advance(m, 1u, &consumed, &stop), LYNX_OK);
    }
    CHECK_NAMED(stop == LYNX_STOP_UNAVAILABLE && m->cpu.req_addr == 0xFC90u &&
                    m->cpu.req_kind == LYNX__CK_READ,
                "input_orientation.other-suzy-registers-remain-unavailable");
    lynx_machine_deinit(m);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.input_orientation\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static lynx_machine *init_input_fault_guest(uint8_t lefthand)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, lefthand), LYNX_OK);
        input_wait_for_read_request(m, 0xFC92u,
                                    "input.fault.setup-sprsys-read-request");
        (void)input_complete_read(m, 0xFC92u, lefthand,
                                  "input.fault.setup-sprsys-readback");
    }
    return m;
}

static void case_input_fault_a_b(void)
{
    const int failures_before = check_failures;
    lynx_machine *m = init_input_fault_guest(0u);
    if (m != NULL) {
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.a-b-pressed-read-request");
        input_set_exact_mask(m, LYNX_INPUT_A,
                             "input.fault.a-b-press-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB0u, 0x01u,
                                  "input.fault.a-b-register-mismatch");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.a-b-switches-read-request");
        input_set_exact_mask(m, 0u,
                             "input.fault.a-b-release-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB1u, 0u,
                                  "input.fault.a-b-switches-after-release");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.a-b-released-read-request");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.a-b-release-observed");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.input_fault_a_b\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_fault_options(void)
{
    const int failures_before = check_failures;
    lynx_machine *m = init_input_fault_guest(0u);
    if (m != NULL) {
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.option-pressed-read-request");
        input_set_exact_mask(m, LYNX_INPUT_OPTION1,
                             "input.fault.option1-press-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB0u, 0x08u,
                                  "input.fault.option-register-mismatch");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.option-switches-read-request");
        input_set_exact_mask(m, 0u,
                             "input.fault.option-release-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB1u, 0u,
                                  "input.fault.option-switches-after-release");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.option-released-read-request");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.option-release-observed");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.input_fault_options\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_fault_pause(void)
{
    const int failures_before = check_failures;
    lynx_machine *m = init_input_fault_guest(0u);
    if (m != NULL) {
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.pause-joystick-read-request");
        input_set_exact_mask(m, LYNX_INPUT_PAUSE,
                             "input.fault.pause-press-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.pause-is-not-a-joystick-bit");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.pause-switches-read-request");
        (void)input_complete_read(m, 0xFCB1u, 0x01u,
                                  "input.fault.pause-register-mismatch");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.pause-release-joystick-request");
        input_set_exact_mask(m, 0u,
                             "input.fault.pause-release-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.pause-release-joystick-is-clear");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.pause-released-switches-request");
        (void)input_complete_read(m, 0xFCB1u, 0u,
                                  "input.fault.pause-release-observed");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.input_fault_pause\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_fault_orientation(void)
{
    const int failures_before = check_failures;
    lynx_machine *m = init_input_fault_guest(0x08u);
    if (m != NULL) {
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.lefthand-pressed-read-request");
        input_set_exact_mask(m, LYNX_INPUT_LEFT,
                             "input.fault.lefthand-left-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB0u, 0x20u,
                                  "input.fault.lefthand-direction-mismatch");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.lefthand-switches-read-request");
        input_set_exact_mask(m, 0u,
                             "input.fault.lefthand-release-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB1u, 0u,
                                  "input.fault.lefthand-switches-after-release");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.lefthand-released-read-request");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.lefthand-release-observed");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.input_fault_orientation\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_input_fault_edge_late(void)
{
    const int failures_before = check_failures;
    lynx_machine *m = init_input_fault_guest(0u);
    if (m != NULL) {
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.sample-pressed-read-request");
        const uint64_t sample_tick = input_next_grant_tick(m);
        input_advance_exclusive_to(m, sample_tick,
                                   "input.fault.sample-request-edge-is-reachable");
        input_set_exact_mask(m, LYNX_INPUT_A,
                             "input.fault.sample-press-installed-at-request-edge");
        (void)input_complete_read(m, 0xFCB0u, 0x01u,
                                  "input.fault.sample-value-observes-press");
        CHECK_NAMED(m->cpu_ready_at == sample_tick + 5u,
                    "input.fault.sample-edge-at-guest-tick");
        input_wait_for_read_request(m, 0xFCB1u,
                                    "input.fault.sample-switches-read-request");
        input_set_exact_mask(m, 0u,
                             "input.fault.sample-release-installed-at-read-edge");
        (void)input_complete_read(m, 0xFCB1u, 0u,
                                  "input.fault.sample-released-switches");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input.fault.sample-released-read-request");
        (void)input_complete_read(m, 0xFCB0u, 0u,
                                  "input.fault.sample-release-observed");
        lynx_machine_deinit(m);
    }
    printf("TAP version 13\n1..1\n%s 1 - api.input_fault_edge_late\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void fill_frame_test_outputs(uint8_t *pens, uint8_t *palettes,
                                    uint64_t *line_ticks, uint8_t sentinel)
{
    memset(pens, sentinel, LYNX_FRAME_PENS_CAPACITY);
    memset(palettes, sentinel, LYNX_FRAME_PALETTE_CAPACITY);
    memset(line_ticks, sentinel, LYNX_FRAME_LINE_TICKS_CAPACITY * sizeof *line_ticks);
}

static bool frame_test_outputs_equal(const uint8_t *pens, const uint8_t *palettes,
                                     const uint64_t *line_ticks, uint8_t sentinel)
{
    for (size_t i = 0u; i < LYNX_FRAME_PENS_CAPACITY; i++) {
        if (pens[i] != sentinel) return false;
    }
    for (size_t i = 0u; i < LYNX_FRAME_PALETTE_CAPACITY; i++) {
        if (palettes[i] != sentinel) return false;
    }
    const uint8_t *tick_bytes = (const uint8_t *)(const void *)line_ticks;
    for (size_t i = 0u; i < LYNX_FRAME_LINE_TICKS_CAPACITY * sizeof *line_ticks; i++) {
        if (tick_bytes[i] != sentinel) return false;
    }
    return true;
}

static bool frame_test_outputs_zero(const uint8_t *pens, const uint8_t *palettes,
                                    const uint64_t *line_ticks)
{
    return all_bytes_equal(pens, LYNX_FRAME_PENS_CAPACITY, 0u) &&
           all_bytes_equal(palettes, LYNX_FRAME_PALETTE_CAPACITY, 0u) &&
           all_bytes_equal((const uint8_t *)(const void *)line_ticks,
                           LYNX_FRAME_LINE_TICKS_CAPACITY * sizeof *line_ticks, 0u);
}

static void check_nonframe_metadata(const lynx_frame_result *result, lynx_stop_reason stop)
{
    CHECK_NAMED(result->stop == stop && result->frame_start_tick == 0u &&
                    result->frame_end_tick == 0u && result->sequence == 0u &&
                    result->row_count == 0u && result->flags == 0u,
                "frame_stops.nonframe-metadata-is-not-a-frame");
}

static void case_frame_stops(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) goto done;

    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result result;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    init_video_frame_result(&result);
    fill_frame_test_outputs(pens, palettes, line_ticks, 0xA5u);
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    configure_video_cadence(m, 158u, 104u);
    const uint64_t first_frame_tick = UINT64_C(159) * 16u * 105u;
    uint64_t ticks_before = ticks_of(m);

    CHECK_EQ_U64(lynx_advance_frame(m, 0u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 0u &&
                    ticks_of(m) == ticks_before,
                "frame_stops.zero-budget-does-no-work");
    check_nonframe_metadata(&result, LYNX_STOP_BUDGET);
    CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xA5u),
                "frame_stops.zero-budget-preserves-all-payloads");

    CHECK_EQ_U64(lynx_advance_frame(m, first_frame_tick - 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET &&
                    result.ticks_consumed == first_frame_tick - 1u,
                "frame_stops.budget-before-first-frame");
    check_nonframe_metadata(&result, LYNX_STOP_BUDGET);
    CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xA5u),
                "frame_stops.pre-frame-budget-preserves-payloads");
    CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 1u &&
                    ticks_of(m) == first_frame_tick,
                "frame_stops.exclusive-frame-target");
    check_nonframe_metadata(&result, LYNX_STOP_BUDGET);
    CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xA5u),
                "frame_stops.exact-target-preserves-payloads");
    CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 0u &&
                    result.frame_end_tick == first_frame_tick && result.sequence == 0u,
                "frame_stops.due-frame-after-target");
    CHECK_NAMED(frame_test_outputs_zero(pens, palettes, line_ticks),
                "frame_stops.sync-frame-publishes-cleared-payload");
    lynx_machine_deinit(m);

    /* The public CPU reaches an absent ROM mapping, which is a successful
       nonframe stop for frame advancement and must not publish stale bytes. */
    static const uint8_t unavailable_program[] = {
        0xA9u, 0x00u, 0x8Du, 0xF9u, 0xFFu, 0xA9u, 0xA5u,
        0x8Du, 0x00u, 0xFEu, 0x4Cu, 0x0Au, 0x02u
    };
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, unavailable_program, sizeof unavailable_program, 0x0200u),
                     LYNX_OK);
        fill_frame_test_outputs(pens, palettes, line_ticks, 0x6Du);
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 128u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_UNAVAILABLE &&
                        result.ticks_consumed < 128u,
                    "frame_stops.device-unavailable-stop");
        check_nonframe_metadata(&result, LYNX_STOP_UNAVAILABLE);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0x6Du),
                    "frame_stops.unavailable-preserves-payloads");
        lynx_machine_deinit(m);
    }

    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, trap_image, sizeof trap_image, 0x0200u), LYNX_OK);
        fill_frame_test_outputs(pens, palettes, line_ticks, 0x7Eu);
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 128u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_UNDEFINED,
                    "frame_stops.undefined-opcode-stop");
        check_nonframe_metadata(&result, LYNX_STOP_UNDEFINED);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0x7Eu),
                    "frame_stops.undefined-preserves-payloads");
        lynx_machine_deinit(m);
    }

    /* A watch just before a common frame edge returns at the edge tick; the
       due frame remains for the next positive call and is delivered once. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
        m->guest_ticks = 6u;
        m->cpu_ready_at = 6u;
        configure_video_cadence(m, 0u, 0u);
        fill_frame_test_outputs(pens, palettes, line_ticks, 0x91u);
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 11u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.frame_end_tick == 16u,
                    "frame_stops.prewatch-control-sync-frame");
        fill_frame_test_outputs(pens, palettes, line_ticks, 0x91u);
        CHECK_EQ_U64(lynx_advance_frame(m, 17u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_WATCH && result.ticks_consumed == 16u &&
                        ticks_of(m) == 32u,
                    "frame_stops.watch-before-frame-edge");
        check_nonframe_metadata(&result, LYNX_STOP_WATCH);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0x91u),
                    "frame_stops.watch-preserves-payloads");
        lynx_watch_event event;
        CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
        CHECK_NAMED(event.hit == 1u && event.hit_tick == 31u && event.return_tick == 32u,
                    "frame_stops.watch-return-at-frame-edge");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 0u &&
                        result.frame_start_tick == 16u && result.frame_end_tick == 32u &&
                        result.sequence == 1u,
                    "frame_stops.deferred-frame-exposed-once");
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 1u,
                    "frame_stops.no-duplicate-deferred-frame");
        lynx_machine_deinit(m);
    }

    /* At a frame/CPU-grant tie, frame completion wins. The write watch is
       observed on the next call at grant+1, leaving next-tick work deferred. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, watch_program, sizeof watch_program, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
        m->guest_ticks = 7u;
        m->cpu_ready_at = 7u;
        configure_video_cadence(m, 0u, 0u);
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xB2u);
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, 10u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.frame_end_tick == 16u,
                    "frame_stops.tie-control-sync-frame");
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xB2u);
        CHECK_EQ_U64(lynx_advance_frame(m, 17u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.ticks_consumed == 16u &&
                        result.frame_end_tick == 32u,
                    "frame_stops.frame-wins-common-grant-tick");
        lynx_watch_event event;
        CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
        CHECK_NAMED(event.hit == 0u && m->ram[0x0400u] == 0u,
                    "frame_stops.tied-watch-not-run-before-frame-return");
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xB2u);
        CHECK_EQ_U64(lynx_advance_frame(m, 5u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_WATCH && result.ticks_consumed == 1u &&
                        ticks_of(m) == 33u,
                    "frame_stops.tied-watch-returns-after-committed-frame");
        check_nonframe_metadata(&result, LYNX_STOP_WATCH);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xB2u),
                    "frame_stops.tied-watch-preserves-payloads");
        CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
        CHECK_NAMED(event.hit == 1u && event.hit_tick == 32u && event.return_tick == 33u &&
                        m->ram[0x0400u] == 0x5Au,
                    "frame_stops.watch-tick-metadata-after-store");
        const uint16_t next_request = m->cpu.req_addr;
        m->cpu_ready_at = 33u;
        CHECK_EQ_U64(lynx_advance_frame(m, 1u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 1u &&
                        m->cpu.req_addr != next_request,
                    "frame_stops.next-tick-work-resumes-next-call");
        lynx_machine_deinit(m);
    }

    /* Ordinary advancement drops four frame deliveries. The frame API sees
       only the next completion, with no stale completed-frame queue. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        configure_video_cadence(m, 0u, 0u);
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(m, 65u, &consumed, &stop), LYNX_OK);
        CHECK_NAMED(stop == LYNX_STOP_BUDGET && consumed == 65u,
                    "frame_stops.ordinary-advance-crosses-deliveries");
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xD4u);
        CHECK_EQ_U64(lynx_advance_frame(m, 16u, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.frame_start_tick == 64u &&
                        result.frame_end_tick == 80u && result.sequence == 4u,
                    "frame_stops.next-frame-after-ordinary-discard");
        lynx_machine_deinit(m);
    }

    /* The scheduler clamps before overflow and remains finite at the end of
       representable guest time, including repeated positive host budgets. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        const uint64_t limit = UINT64_MAX - (uint64_t)LYNX__TICK_HEADROOM;
        m->guest_ticks = limit - 7u;
        m->cpu_ready_at = m->guest_ticks;
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xE5u);
        init_video_frame_result(&result);
        CHECK_EQ_U64(lynx_advance_frame(m, UINT64_MAX, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 7u &&
                        ticks_of(m) == limit,
                    "frame_stops.headroom-clamp-positive-progress");
        check_nonframe_metadata(&result, LYNX_STOP_BUDGET);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xE5u),
                    "frame_stops.headroom-preserves-payloads");
        memcpy(snap_a, m, MSIZE);
        CHECK_EQ_U64(lynx_advance_frame(m, UINT64_MAX, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && result.ticks_consumed == 0u &&
                        ticks_of(m) == limit,
                    "frame_stops.headroom-repeated-zero-progress");
        CHECK_UNCHANGED(m, snap_a);
        CHECK_NAMED(frame_test_outputs_equal(pens, palettes, line_ticks, 0xE5u),
                    "frame_stops.repeated-headroom-preserves-payloads");
        lynx_machine_deinit(m);
    }

done:
    printf("TAP version 13\n1..1\n%s 1 - api.frame_stops\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void setup_video_session(lynx_machine *m, uint16_t input, uint8_t salt)
{
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_set_input(m, input), LYNX_OK);
    CHECK(lynx__video_write(&m->video, 0xFD92u, 1u));
    CHECK(lynx__video_write(&m->video, 0xFD94u, 0u));
    CHECK(lynx__video_write(&m->video, 0xFD95u, 0u));
    for (unsigned i = 0u; i < 16u; i++) {
        CHECK(lynx__video_write(&m->video, (uint16_t)(0xFDA0u + i),
                                (uint8_t)(salt + i)));
        CHECK(lynx__video_write(&m->video, (uint16_t)(0xFDB0u + i),
                                (uint8_t)(salt + 0x20u + i)));
    }
    for (unsigned i = 0u; i < LYNX_FRAME_ROW_BYTES; i++) {
        m->ram[i] = (uint8_t)(salt ^ i);
    }
    configure_video_cadence(m, 158u, 104u);
}

static void input_run_adjacent_edge(int edge_offset)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m == NULL) return;
    CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, 0x08u), LYNX_OK);
    CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_A), LYNX_OK);
    input_wait_for_read_request(m, 0xFCB0u, "input_edges.guest-read-posted-before-sample");
    CHECK_EQ_U64(m->bus_hold, 0u);
    const uint64_t sample_tick = input_next_grant_tick(m);
    CHECK_NAMED(sample_tick > ticks_of(m), "input_edges.future-grant-is-observable");

    const uint64_t before_zero = ticks_of(m);
    uint64_t consumed = UINT64_MAX;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(m, 0u, &consumed, &stop), LYNX_OK);
    CHECK_NAMED(consumed == 0u && ticks_of(m) == before_zero &&
                    stop == LYNX_STOP_BUDGET &&
                    input_read_request(m, 0xFCB0u),
                "input_edges.zero-consumed-does-not-move-pending-edge");

    if (edge_offset < 0) {
        input_advance_exclusive_to(m, sample_tick - 1u,
                                   "input_edges.minus-one-boundary-is-exclusive");
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.minus-one-setter-preserves-posted-request");
        input_advance_exclusive_to(m, sample_tick,
                                   "input_edges.minus-one-stops-at-sample-boundary");
        CHECK_NAMED(input_read_request(m, 0xFCB0u) && ticks_of(m) == sample_tick,
                    "input_edges.minus-one-does-not-read-at-exclusive-target");
        const uint64_t observed = input_complete_read(
            m, 0xFCB0u, 0x02u, "input_edges.minus-one-level-is-sampled-at-grant");
        CHECK_EQ_U64(observed, sample_tick);
    } else if (edge_offset == 0) {
        input_advance_exclusive_to(m, sample_tick,
                                   "input_edges.exact-boundary-is-exclusive");
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.exact-setter-preserves-posted-request");
        const uint64_t observed = input_complete_read(
            m, 0xFCB0u, 0x02u, "input_edges.exact-level-is-sampled-at-grant");
        CHECK_EQ_U64(observed, sample_tick);
    } else {
        input_advance_exclusive_to(m, sample_tick,
                                   "input_edges.plus-one-starts-after-exclusive-boundary");
        const uint64_t observed_old = input_complete_read(
            m, 0xFCB0u, 0x01u, "input_edges.plus-one-keeps-old-level-at-grant");
        CHECK_EQ_U64(observed_old, sample_tick);
        const uint64_t edge_tick = ticks_of(m);
        CHECK_EQ_U64(edge_tick, sample_tick + 1u);
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.plus-one-setter-preserves-next-request");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input_edges.plus-one-next-guest-read-posted");
        const uint64_t observed_new = input_complete_read(
            m, 0xFCB0u, 0x02u, "input_edges.plus-one-level-is-seen-on-next-read");
        CHECK_NAMED(observed_new > edge_tick,
                    "input_edges.plus-one-does-not-retroactively-change-sample");
    }
    lynx_machine_deinit(m);
}

static void case_input_edges(void)
{
    const int failures_before = check_failures;
    input_run_adjacent_edge(-1);
    input_run_adjacent_edge(0);
    input_run_adjacent_edge(1);

    /* Align one real pending guest read with a frame endpoint. The frame API
       publishes its captured row before the host applies the edge; after
       resume the same-tick CPU grant observes the new level. */
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, 0x08u), LYNX_OK);
        CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_A), LYNX_OK);
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input_edges.frame-tie-guest-read-posted");
        const uint64_t start = ticks_of(m);
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
        configure_video_cadence(m, 0u, 0u);
        uint64_t frame_tick = UINT64_MAX;
        CHECK(lynx__timer_next(&m->timer_bank, start, &frame_tick));
        CHECK_NAMED(frame_tick > start, "input_edges.frame-tie-event-is-future");
        m->cpu_ready_at = frame_tick;
        m->video.capture_rows = 1u;
        m->video.pens[0] = 0xA5u;
        m->video.palettes[0] = 0x5Au;
        m->video.line_ticks[0] = start;

        uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
        uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        lynx_frame_buffers buffers;
        lynx_frame_result result;
        init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
        init_video_frame_result(&result);
        fill_frame_test_outputs(pens, palettes, line_ticks, 0xC7u);
        const uint64_t budget = frame_tick - start + 1u;
        CHECK_EQ_U64(lynx_advance_frame(m, budget, &buffers, &result), LYNX_OK);
        CHECK_NAMED(result.stop == LYNX_STOP_FRAME &&
                        result.frame_end_tick == frame_tick &&
                        result.ticks_consumed == frame_tick - start &&
                        result.row_count == 1u && pens[0] == 0xA5u &&
                        palettes[0] == 0x5Au && line_ticks[0] == start,
                    "input_edges.frame-publishes-prior-captured-row-at-endpoint");
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.frame-endpoint-setter-preserves-completed-frame");
        CHECK_NAMED(pens[0] == 0xA5u && palettes[0] == 0x5Au &&
                        line_ticks[0] == start && ticks_of(m) == frame_tick,
                    "input_edges.frame-endpoint-level-does-not-rewrite-published-frame");
        CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 0u), LYNX_OK);
        const uint64_t observed = input_complete_read(
            m, 0xFCB0u, 0x02u,
            "input_edges.frame-endpoint-level-is-sampled-after-resume");
        CHECK_EQ_U64(observed, frame_tick);
        CHECK_NAMED(pens[0] == 0xA5u && palettes[0] == 0x5Au &&
                        line_ticks[0] == start,
                    "input_edges.same-tick-read-leaves-completed-output-unchanged");
        lynx_machine_deinit(m);
    }

    /* A real video DMA grant followed by refresh holds this posted input read
       while the host changes levels; sampling waits for the eventual release. */
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_input_read_guest(m, 0x0Cu, 0x08u), LYNX_OK);
        CHECK_EQ_U64(lynx_set_input(m, LYNX_INPUT_A), LYNX_OK);
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input_edges.hold-guest-read-posted");
        const uint64_t video_tick = input_next_grant_tick(m);
        CHECK_NAMED(video_tick > ticks_of(m), "input_edges.hold-start-is-future");
        m->video.dispctl = 1u;
        m->video.row_active = 1u;
        m->video.dma_cursor = LYNX_FRAME_ROW_BYTES - 1u;
        m->video.dma_next_tick = video_tick;
        m->video.refresh_next_tick = video_tick;
        input_advance_exclusive_to(m, video_tick + 1u,
                                   "input_edges.video-begins-hold-at-grant-tick");
        CHECK_NAMED(input_read_request(m, 0xFCB0u) &&
                        m->video.dma_busy_until == video_tick + 5u,
                    "input_edges.posted-read-remains-held-after-video-grant");
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.level-change-during-video-hold-preserves-request");
        const uint64_t held_sample = input_complete_read(
            m, 0xFCB0u, 0x02u,
            "input_edges.held-read-samples-latest-level-after-refresh");
        CHECK_NAMED(held_sample >= video_tick + 10u &&
                        held_sample == m->video.refresh_busy_until &&
                        m->video.dma_busy_until == video_tick + 5u,
                    "input_edges.sample-tick-follows-video-and-refresh-release");
        lynx_machine_deinit(m);
    }

    /* The edge is applied at a one-shot write-watch return boundary and stays
       there while the guest proceeds to its real input read. */
    static const uint8_t watch_then_input_guest[] = {
        0xA9u, 0x0Cu, 0x8Du, 0xF9u, 0xFFu,
        0xA9u, 0x5Au, 0x8Du, 0x00u, 0x04u,
        0xADu, 0xB0u, 0xFCu, 0x4Cu, 0x0Au, 0x02u
    };
    m = init_in(machine_buf, &cfg);
    if (m != NULL) {
        CHECK_EQ_U64(load_bll_at(m, watch_then_input_guest,
                                 sizeof watch_then_input_guest, 0x0200u), LYNX_OK);
        CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(m, 512u, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
        lynx_watch_event watch;
        CHECK_EQ_U64(lynx_watch_get(m, &watch), LYNX_OK);
        CHECK_NAMED(watch.hit == 1u && watch.address == 0x0400u &&
                        watch.return_tick == watch.hit_tick + 1u &&
                        ticks_of(m) == watch.return_tick,
                    "input_edges.watch-return-is-edge-adjacent-boundary");
        input_set_exact_mask(m, LYNX_INPUT_B,
                             "input_edges.watch-boundary-setter-preserves-watch-and-time");
        input_wait_for_read_request(m, 0xFCB0u,
                                    "input_edges.watch-following-guest-read-posted");
        const uint64_t observed = input_complete_read(
            m, 0xFCB0u, 0x02u,
            "input_edges.watch-boundary-level-is-sampled-by-guest");
        CHECK_NAMED(observed > watch.return_tick,
                    "input_edges.watch-boundary-does-not-move-sample-time");
        lynx_machine_deinit(m);
    }

    printf("TAP version 13\n1..1\n%s 1 - api.input_edges\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void advance_to_video_sync_frame(lynx_machine *m)
{
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result result;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    init_video_frame_result(&result);
    fill_frame_test_outputs(pens, palettes, line_ticks, 0xA5u);
    const uint64_t first_frame_tick = UINT64_C(159) * 16u * 105u;
    CHECK_EQ_U64(lynx_advance_frame(m, first_frame_tick + 1u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.sequence == 0u,
                "video_reset_load.initial-sync-frame");
}

static void advance_to_partial_video_row(lynx_machine *m)
{
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result result;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    init_video_frame_result(&result);
    CHECK_EQ_U64(lynx_advance_frame(m, 3u * UINT64_C(159) * 16u + 100u,
                                   &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_BUDGET && m->video.row_active != 0u &&
                    m->video.dma_cursor != 0u,
                "video_reset_load.partial-row-is-active");
}

static void check_first_sync_is_clean(lynx_machine *m)
{
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result result;
    lynx_watch_event event;
    init_video_frame_buffers(&buffers, pens, palettes, line_ticks);
    init_video_frame_result(&result);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(m, 1u), LYNX_OK);
    configure_video_cadence(m, 0u, 0u);
    fill_frame_test_outputs(pens, palettes, line_ticks, 0xA5u);
    CHECK_EQ_U64(lynx_advance_frame(m, 17u, &buffers, &result), LYNX_OK);
    CHECK_NAMED(result.stop == LYNX_STOP_FRAME && result.sequence == 0u &&
                    result.row_count == 0u,
                "video_reset_load.fresh-sync-frame");
    CHECK_NAMED(frame_test_outputs_zero(pens, palettes, line_ticks),
                "video_reset_load.no-stale-capture-after-transition");
    CHECK_EQ_U64(lynx_watch_get(m, &event), LYNX_OK);
    CHECK_NAMED(event.hit == 0u && event.address == 0u && event.value == 0u &&
                    event.hit_tick == 0u && event.return_tick == 0u,
                "video_reset_load.watch-state-is-cleared");
}

static void case_video_reset_load(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *m = init_in(machine_buf, &cfg);
    lynx_machine *other = init_in(buf_b, &cfg);
    if (m == NULL || other == NULL) goto done;

    setup_video_session(m, LYNX_INPUT_A, 0x31u);
    setup_video_session(other, LYNX_INPUT_B, 0xA7u);
    advance_to_video_sync_frame(m);
    advance_to_video_sync_frame(other);
    advance_to_partial_video_row(m);
    advance_to_partial_video_row(other);
    CHECK_EQ_U64(lynx_watch_configure(m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(other, 0x0401u, 1u), LYNX_OK);
    CHECK_NAMED(m->video.input_mask == LYNX_INPUT_A &&
                    other->video.input_mask == LYNX_INPUT_B,
                "video_reset_load.instance-input-masks-independent");
    CHECK_NAMED(m->video.pens[0] != other->video.pens[0] &&
                    m->video.palettes[0] == 0x31u && other->video.palettes[0] == 0xA7u,
                "video_reset_load.instance-capture-and-palette-independent");
    CHECK_NAMED(m->video.dma_busy_until != 0u && other->video.dma_busy_until != 0u &&
                    ticks_of(m) == ticks_of(other),
                "video_reset_load.interleaved-instance-progress");

    uint8_t malformed[sizeof bll_tracer];
    memcpy(malformed, bll_tracer, sizeof malformed);
    malformed[6] = 'X';
    memcpy(snap_a, m, MSIZE);
    CHECK_EQ_U64(lynx_load_bll(m, malformed, sizeof malformed), LYNX_ERR_ARGUMENT);
    CHECK_UNCHANGED(m, snap_a);
    CHECK_NAMED(m->video.row_active != 0u && m->video.dma_cursor != 0u &&
                    m->video.input_mask == LYNX_INPUT_A && m->host_observation.watch_armed == 1u,
                "video_reset_load.refused-load-preserves-partial-session");

    CHECK_EQ_U64(lynx_reset(m), LYNX_OK);
    CHECK_NAMED(m->cfg.timing_policy == LYNX_TIMING_POLICY_VIDEO_POLICY1 &&
                    m->has_content == 0u && ticks_of(m) == 0u &&
                    m->video.input_mask == 0u && m->video.row_active == 0u &&
                    m->video.capture_rows == 0u && m->video.dma_cursor == 0u &&
                    m->video.capture_partial == 0u && m->video.dispctl == 0u &&
                    m->video.backup_addr == 0u && m->video.dispaddr == 0u &&
                    m->video.dma_busy_until == 0u && m->video.refresh_busy_until == 0u &&
                    m->cpu_page_latch == 0u && m->bus_hold == 0u,
                "video_reset_load.reset-clears-input-capture-and-owners");
    CHECK_NAMED(all_bytes_equal(m->video.pens, sizeof m->video.pens, 0u) &&
                    all_bytes_equal(m->video.palettes, sizeof m->video.palettes, 0u) &&
                    all_bytes_equal((const uint8_t *)(const void *)m->video.line_ticks,
                                    sizeof m->video.line_ticks, 0u),
                "video_reset_load.reset-clears-all-staged-video-bytes");
    CHECK_EQ_U64(lynx_load_bll(m, bll_tracer, sizeof bll_tracer), LYNX_OK);
    check_first_sync_is_clean(m);

    /* Accepted load applies the same fresh-session policy to a second live
       instance that already has partial capture, input and an armed watch. */
    CHECK_NAMED(other->video.row_active != 0u && other->video.dma_cursor != 0u &&
                    other->video.input_mask == LYNX_INPUT_B &&
                    other->host_observation.watch_armed == 1u,
                "video_reset_load.load-control-has-pending-session");
    CHECK_EQ_U64(lynx_load_bll(other, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_NAMED(other->cfg.timing_policy == LYNX_TIMING_POLICY_VIDEO_POLICY1 &&
                    other->has_content == 1u && ticks_of(other) == 0u &&
                    other->video.input_mask == 0u && other->video.row_active == 0u &&
                    other->video.capture_rows == 0u && other->video.dma_cursor == 0u &&
                    other->video.capture_partial == 0u && other->video.dispctl == 0u &&
                    other->video.dma_busy_until == 0u && other->video.refresh_busy_until == 0u,
                "video_reset_load.accepted-load-clears-input-capture-and-owners");
    CHECK_NAMED(all_bytes_equal(other->video.pens, sizeof other->video.pens, 0u) &&
                    all_bytes_equal(other->video.palettes, sizeof other->video.palettes, 0u) &&
                    all_bytes_equal((const uint8_t *)(const void *)other->video.line_ticks,
                                    sizeof other->video.line_ticks, 0u),
                "video_reset_load.accepted-load-clears-all-staged-video-bytes");
    check_first_sync_is_clean(other);
    lynx_machine_deinit(m);
    lynx_machine_deinit(other);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.video_reset_load\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static bool video_snapshot_roundtrip(lynx_machine *source, lynx_machine *restored,
                                     const char *assertion)
{
    const int failures_before = check_failures;
    CHECK_EQ_U64(save_into(source, blob_v), 0u);
    const lynx_status st = lynx_snapshot_restore(restored, blob_v, SNAP_TOTAL);
    CHECK_NAMED(st == LYNX_OK, assertion);
    if (st == LYNX_OK) {
        CHECK_NAMED(memcmp(source, restored, MSIZE) == 0,
                    "video_snapshot.boundary-restore-is-byte-identical");
    }
    return check_failures == failures_before;
}

static void video_snapshot_value_refused(lynx_machine *target, const unsigned char *source,
                                         size_t offset, uint64_t value, size_t width,
                                         const char *assertion)
{
    memcpy(blob_bad, source, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + offset, value, width);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_watch_configure(target, 0x0400u, 1u), LYNX_OK);
    memcpy(snap_a, target, MSIZE);
    memcpy(blob_c, blob_bad, SNAP_TOTAL);
    const lynx_status st = lynx_snapshot_restore(target, blob_bad, SNAP_TOTAL);
    CHECK_NAMED(st == LYNX_ERR_SNAPSHOT_INVALID, assertion);
    CHECK_UNCHANGED(target, snap_a);
    CHECK_NAMED(target->host_observation.watch_armed == 1u,
                "video_snapshot_refusal.watch-preserved");
    CHECK_NAMED(memcmp(blob_bad, blob_c, SNAP_TOTAL) == 0,
                "video_snapshot_refusal.caller-blob-preserved");
}

static void case_video_snapshot(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *source = init_in(machine_buf, &cfg);
    lynx_machine *restored = init_in(buf_b, &cfg);
    if (source == NULL || restored == NULL) goto done;

    setup_video_session(source, LYNX_INPUT_A | LYNX_INPUT_PAUSE, 0x51u);
    setup_video_session(restored, LYNX_INPUT_A | LYNX_INPUT_PAUSE, 0x51u);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 0u), LYNX_OK);
    advance_ticks(source, 1u);
    CHECK_NAMED(source->cpu_page_latch == 3u,
                "video_snapshot.same-page-opcode-continuation-is-reached");
    CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 1u), LYNX_OK);
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.page-continuation-restores")) goto done;
    advance_to_video_sync_frame(source);
    CHECK(lynx__video_write(&source->video, 0xFD94u, 0xFDu));
    CHECK(lynx__video_write(&source->video, 0xFD95u, 0xFFu));
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.frame-boundary-restores")) goto done;

    const uint64_t line_period = UINT64_C(159) * 16u;
    const uint64_t row_start = source->video.frame_start_tick + 3u * line_period;
    advance_ticks(source, row_start - source->guest_ticks);
    CHECK_NAMED(source->guest_ticks == row_start && source->video.row_active == 0u &&
                    source->video.line_ordinal == 2u,
                "video_snapshot.before-visible-line-edge");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.before-line-edge-restores")) goto done;

    advance_ticks(source, 1u);
    CHECK_NAMED(source->video.row_active != 0u && source->video.dma_cursor == 0u &&
                    source->video.line_ticks[0] == row_start &&
                    source->video.dispaddr == 0xFFFCu,
                "video_snapshot.visible-line-edge-starts-aligned-wrap-stream");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.after-line-edge-restores")) goto done;

    advance_ticks(source, 4u);
    CHECK_NAMED(source->guest_ticks == row_start + 5u && source->video.dma_cursor == 0u,
                "video_snapshot.before-first-dma-grant");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.before-dma-grant-restores")) goto done;

    advance_ticks(source, 1u);
    CHECK_NAMED(source->video.dma_cursor == 1u &&
                    source->video.dma_busy_until > source->guest_ticks,
                "video_snapshot.inside-first-dma-grant");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.inside-dma-grant-restores")) goto done;

    advance_ticks(source, 15u);
    CHECK_NAMED(source->video.dma_cursor == 4u && source->video.dispaddr == 0u,
                "video_snapshot.display-address-wraps-after-four-bytes");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.after-dma-grant-restores")) goto done;

    /* A refresh whose phase falls inside the continuous visible-row stream
       remains queued while each five-tick DMA byte owns the bus. This is a
       reachable public boundary: refresh_next_tick may be behind guest time
       while the current video grant is still in flight. */
    bool found_pending_refresh = false;
    for (uint32_t i = 0u; i < 4096u; i++) {
        if (source->video.row_active != 0u &&
            source->video.refresh_next_tick < source->guest_ticks &&
            source->video.dma_busy_until > source->guest_ticks) {
            found_pending_refresh = true;
            break;
        }
        advance_ticks(source, 1u);
    }
    CHECK_NAMED(found_pending_refresh && source->video.row_active != 0u &&
                    source->video.refresh_next_tick < source->guest_ticks &&
                    source->video.dma_busy_until > source->guest_ticks,
                "video_snapshot.reachable-refresh-waits-behind-dma");
    if (!found_pending_refresh) goto done;

    CHECK_EQ_U64(save_into(source, blob_v), 0u);
    CHECK_NAMED(lynx_watch_configure(restored, 0x0400u, 1u) == LYNX_OK,
                "video_snapshot.target-watch-armed-before-apply");
    const lynx_status restore_status = lynx_snapshot_restore(restored, blob_v, SNAP_TOTAL);
    CHECK_NAMED(restore_status == LYNX_OK,
                "video_snapshot.reachable-pending-refresh-restores");
    if (restore_status != LYNX_OK) goto done;
    CHECK_EQ_U64(restored->host_observation.watch_armed, 0u);
    CHECK_NAMED(memcmp(source, restored, MSIZE) == 0,
                "video_snapshot.restore-applies-complete-guest-state");

    uint8_t source_pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t source_palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t source_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    uint8_t restored_pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t restored_palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t restored_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers source_buffers;
    lynx_frame_buffers restored_buffers;
    lynx_frame_result source_result;
    lynx_frame_result restored_result;
    init_video_frame_buffers(&source_buffers, source_pens, source_palettes, source_ticks);
    init_video_frame_buffers(&restored_buffers, restored_pens, restored_palettes, restored_ticks);
    init_video_frame_result(&source_result);
    init_video_frame_result(&restored_result);
    CHECK_EQ_U64(lynx_advance_frame(source, 300000u, &source_buffers, &source_result), LYNX_OK);
    CHECK_EQ_U64(lynx_advance_frame(restored, 300000u, &restored_buffers, &restored_result),
                 LYNX_OK);
    CHECK_NAMED(source_result.stop == LYNX_STOP_FRAME &&
                    restored_result.stop == LYNX_STOP_FRAME &&
                    source_result.ticks_consumed == restored_result.ticks_consumed &&
                    source_result.frame_start_tick == restored_result.frame_start_tick &&
                    source_result.frame_end_tick == restored_result.frame_end_tick &&
                    source_result.sequence == restored_result.sequence &&
                    source_result.row_count == restored_result.row_count &&
                    source_result.flags == restored_result.flags,
                "video_snapshot.next-frame-metadata-matches");
    CHECK_NAMED(memcmp(source_pens, restored_pens, sizeof source_pens) == 0 &&
                    memcmp(source_palettes, restored_palettes, sizeof source_palettes) == 0 &&
                    memcmp(source_ticks, restored_ticks, sizeof source_ticks) == 0,
                "video_snapshot.next-frame-raw-capture-matches");
    CHECK_NAMED(memcmp(source, restored, MSIZE) == 0,
                "video_snapshot.next-frame-guest-state-matches");
    if (!video_snapshot_roundtrip(source, restored,
                                  "video_snapshot.after-frame-boundary-restores")) goto done;
    lynx_machine_deinit(source);
    lynx_machine_deinit(restored);

done:
    printf("TAP version 13\n1..1\n%s 1 - api.video_snapshot\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static void case_video_snapshot_refusal(void)
{
    const int failures_before = check_failures;
    lynx_config cfg;
    lynx_config_default(&cfg);
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    lynx_machine *source = init_in(machine_buf, &cfg);
    lynx_machine *target = init_in(buf_b, &cfg);
    if (source == NULL || target == NULL) goto done;

    CHECK_EQ_U64(lynx_load_bll(source, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx_load_bll(target, bll_tracer, sizeof bll_tracer), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(source, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx__machine_set_bus_hold(target, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_set_input(source, LYNX_INPUT_A), LYNX_OK);
    CHECK_EQ_U64(lynx_set_input(target, LYNX_INPUT_B), LYNX_OK);
    CHECK_EQ_U64(save_into(source, blob_v), 0u);

    /* Video and refresh grants own one exclusive bus. These individually
       in-range release ticks overlap, so a correctly hashed candidate must
       be refused before changing guest or host observation state. */
    memcpy(blob_bad, blob_v, SNAP_TOTAL);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_VIDEO + 24u, 5u, 8u);
    put_le(blob_bad + LYNX__SNAPSHOT_HEADER_LEN + PAY_VIDEO + 40u, 5u, 8u);
    rehash_payload(blob_bad);
    CHECK_EQ_U64(lynx_watch_configure(target, 0x0400u, 1u), LYNX_OK);
    memcpy(snap_a, target, MSIZE);
    memcpy(blob_c, blob_bad, SNAP_TOTAL);
    const lynx_status st = lynx_snapshot_restore(target, blob_bad, SNAP_TOTAL);
    CHECK_NAMED(st == LYNX_ERR_SNAPSHOT_INVALID,
                "video_snapshot_refusal.overlapping-owners-refused");
    CHECK_UNCHANGED(target, snap_a);
    CHECK_NAMED(target->host_observation.watch_armed == 1u,
                "video_snapshot_refusal.watch-preserved");
    CHECK_NAMED(memcmp(blob_bad, blob_c, SNAP_TOTAL) == 0,
                "video_snapshot_refusal.caller-blob-preserved");

    /* These independent hostile fields carry a fresh payload hash, so the
       restore path must reject their semantics rather than the checksum. */
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DISPCTL,
                                 2u, 1u, "video_snapshot_refusal.reserved-display-mode");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_FRAME_SYNCED,
                                 2u, 1u, "video_snapshot_refusal.frame-sync-flag");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LINE_ORDINAL,
                                 105u, 1u, "video_snapshot_refusal.line-ordinal-range");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_ROW_ACTIVE,
                                 2u, 1u, "video_snapshot_refusal.row-active-flag");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_CAPTURE_ROWS,
                                 103u, 1u, "video_snapshot_refusal.capture-row-range");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DMA_CURSOR,
                                 LYNX_FRAME_ROW_BYTES, 1u,
                                 "video_snapshot_refusal.inactive-row-cursor");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_CAPTURE_PARTIAL,
                                 2u, 1u, "video_snapshot_refusal.partial-flag");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_INPUT_MASK,
                                 LYNX_INPUT_MASK + 1u, 2u,
                                 "video_snapshot_refusal.input-mask-width");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LEFTHAND,
                                 2u, 1u, "video_snapshot_refusal.input-orientation");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DMA_NEXT,
                                 5u, 8u, "video_snapshot_refusal.inactive-dma-deadline");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_REFRESH_NEXT,
                                 0u, 8u, "video_snapshot_refusal.missing-refresh-phase");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_REFRESH_NEXT,
                                 257u, 8u, "video_snapshot_refusal.refresh-phase-grid");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_REFRESH_NEXT,
                                 512u, 8u, "video_snapshot_refusal.refresh-phase-horizon");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_FRAME_START,
                                 1u, 8u, "video_snapshot_refusal.future-frame-start");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_FRAME_SEQUENCE,
                                 1u, 8u, "video_snapshot_refusal.future-frame-sequence");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LINE_TICKS,
                                 1u, 8u, "video_snapshot_refusal.unused-line-tick");
    video_snapshot_value_refused(target, blob_v, PAY_TIMER_EVENT_APPLIED,
                                 1u, 1u, "video_snapshot_refusal.historical-timer-cursor");

    /* A real ongoing DMA stream is accepted as a control before its valid
       cross-state relationships are independently corrupted below. */
    setup_video_session(source, LYNX_INPUT_A | LYNX_INPUT_PAUSE, 0x51u);
    advance_to_video_sync_frame(source);
    advance_to_partial_video_row(source);
    bool found_pending_refresh = false;
    for (uint32_t i = 0u; i < 4096u; i++) {
        if (source->video.row_active != 0u &&
            source->video.refresh_next_tick < source->guest_ticks &&
            source->video.dma_busy_until > source->guest_ticks) {
            found_pending_refresh = true;
            break;
        }
        advance_ticks(source, 1u);
    }
    CHECK_NAMED(found_pending_refresh,
                "video_snapshot_refusal.active-owner-control-is-reachable");
    if (!found_pending_refresh) goto done;
    CHECK_EQ_U64(save_into(source, blob_v), 0u);
    CHECK_EQ_U64(lynx_snapshot_restore(target, blob_v, SNAP_TOTAL), LYNX_OK);
    CHECK_NAMED(memcmp(source, target, MSIZE) == 0,
                "video_snapshot_refusal.active-owner-control-restores");

    const uint64_t current_tick = source->guest_ticks;
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DMA_BUSY,
                                 current_tick - 1u, 8u,
                                 "video_snapshot_refusal.stale-refresh-without-dma-blocker");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DMA_NEXT,
                                 current_tick - 1u, 8u,
                                 "video_snapshot_refusal.stale-dma-without-refresh-owner");
    video_snapshot_value_refused(target, blob_v, PAY_PAGE_LATCH,
                                 1u, 1u,
                                 "video_snapshot_refusal.page-latch-cleared-by-dma-owner");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_DMA_CURSOR,
                                 LYNX_FRAME_ROW_BYTES, 1u,
                                 "video_snapshot_refusal.active-cursor-range");
    video_snapshot_value_refused(target, blob_v,
                                 PAY_VIDEO + VIDEO_PENS + source->video.dma_cursor,
                                 0xA5u, 1u,
                                 "video_snapshot_refusal.active-row-tail-zero");
    video_snapshot_value_refused(target, blob_v,
                                 PAY_VIDEO + VIDEO_LINE_TICKS,
                                 current_tick + 1u, 8u,
                                 "video_snapshot_refusal.future-line-start");

    /* Once a completed row and its successor are staged, line timestamps and
       unused tails are a contiguous, monotone prefix rather than 816 slots. */
    const uint64_t next_line = source->video.frame_start_tick + 4u * UINT64_C(159) * 16u;
    advance_ticks(source, next_line + 1u - source->guest_ticks);
    CHECK_NAMED(source->video.capture_rows == 1u && source->video.row_active != 0u,
                "video_snapshot_refusal.completed-row-before-successor");
    if (source->video.capture_rows != 1u || source->video.row_active == 0u) goto done;
    if (!video_snapshot_roundtrip(source, target,
                                  "video_snapshot_refusal.two-row-state-restores")) goto done;
    CHECK_EQ_U64(save_into(source, blob_v), 0u);
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LINE_TICKS + 8u,
                                 source->video.line_ticks[0], 8u,
                                 "video_snapshot_refusal.nonmonotone-line-starts");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LINE_TICKS + 8u,
                                 source->guest_ticks + 1u, 8u,
                                 "video_snapshot_refusal.second-line-time-in-future");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_PENS +
                                 2u * LYNX_FRAME_ROW_BYTES,
                                 0xA5u, 1u,
                                 "video_snapshot_refusal.unused-pen-tail-zero");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_ROW_PALETTES + 64u,
                                 0xA5u, 1u,
                                 "video_snapshot_refusal.unused-row-palette-zero");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_LINE_TICKS + 16u,
                                 1u, 8u,
                                 "video_snapshot_refusal.unused-row-tick-zero");
    video_snapshot_value_refused(target, blob_v, PAY_VIDEO + VIDEO_CAPTURE_ROWS,
                                 2u, 1u,
                                 "video_snapshot_refusal.rows-exceed-visible-ordinal");

done:
    if (source != NULL) lynx_machine_deinit(source);
    if (target != NULL) lynx_machine_deinit(target);
    printf("TAP version 13\n1..1\n%s 1 - api.video_snapshot_refusal\n",
           check_failures == failures_before ? "ok" : "not ok");
}

static const struct {
    const char *name;
    void (*run)(void);
} cases[] = {
    {"advance_budget", case_advance_budget},
    {"advance_slicing", case_advance_slicing},
    {"bll_alias", case_bll_alias},
    {"bll_identity", case_bll_identity},
    {"bll_load", case_bll_load},
    {"bll_tracer", case_bll_tracer},
    {"build_id", case_build_id},
    {"cost_model", case_cost_model},
    {"frame_edges", case_frame_edges},
    {"frame_refusal", case_frame_refusal},
    {"frame_stops", case_frame_stops},
    {"input_bits", case_input_bits},
    {"input_edges", case_input_edges},
    {"input_fault_a_b", case_input_fault_a_b},
    {"input_fault_edge_late", case_input_fault_edge_late},
    {"input_fault_options", case_input_fault_options},
    {"input_fault_orientation", case_input_fault_orientation},
    {"input_fault_pause", case_input_fault_pause},
    {"input_orientation", case_input_orientation},
    {"input_refusal", case_input_refusal},
    {"state_digest", case_state_digest},
    {"video_snapshot", case_video_snapshot},
    {"video_snapshot_refusal", case_video_snapshot_refusal},
    {"video_reset_load", case_video_reset_load},
    {"cpu_program", case_cpu_program},
    {"identity", case_identity},
    {"init_refusal", case_init_refusal},
    {"irq_pending", case_irq_pending},
    {"inspect_pure", case_inspect_pure},
    {"instances_independent", case_instances_independent},
    {"map_matrix", case_map_matrix},
    {"irq_poll", case_irq_poll},
    {"scheduler_limits", case_scheduler_limits},
    {"scheduler_order", case_scheduler_order},
    {"scheduler_watch", case_scheduler_watch},
    {"memory_peek_pure", case_memory_peek_pure},
    {"load_refusal_unchanged", case_load_refusal_unchanged},
    {"profile_divergence", case_profile_divergence},
    {"profile_select", case_profile_select},
    {"palette_boundary", case_palette_boundary},
    {"request_boundary", case_request_boundary},
    {"rom_unavailable", case_rom_unavailable},
    {"request_hold", case_request_hold},
    {"request_wake", case_request_wake},
    {"reset_power_on", case_reset_power_on},
    {"sha256", case_sha256},
    {"snapshot_refusal", case_snapshot_refusal},
    {"snapshot_roundtrip", case_snapshot_roundtrip},
    {"snapshot_continue", case_snapshot_continue},
    {"timer_snapshot", case_timer_snapshot},
    {"timer_snapshot_controls", case_timer_snapshot_controls},
    {"timer_tracer", case_timer_tracer},
    {"timer_done_borrow", case_timer_done_borrow},
    {"timer_link_tracer", case_timer_link_tracer},
    {"timer_links", case_timer_links},
    {"timer_unavailable", case_timer_unavailable},
    {"timer_standalone", case_timer_standalone},
    {"timer_phase", case_timer_phase},
    {"timer_done_gate", case_timer_done_gate},
    {"timer_reset_done_held", case_timer_reset_done_held},
    {"timer_shared_phase", case_timer_shared_phase},
    {"video_bus", case_video_bus},
    {"video_bus_edges", case_video_bus_edges},
    {"video_dma_cost_fault", case_video_dma_cost_fault},
    {"video_dma_request_fault", case_video_dma_request_fault},
    {"video_palette_fault", case_video_palette_fault},
    {"video_page_mode", case_video_page_mode},
    {"video_cadence", case_video_cadence},
    {"video_geometry", case_video_geometry},
    {"timer_pending_after_disable", case_timer_pending_after_disable},
    {"watch_controls", case_watch_controls},
    {"watch_independent", case_watch_independent},
    {"watch_post_write", case_watch_post_write},
};

int main(int argc, char **argv)
{
    /* Each persistent control dispatches a named, semantically targeted
       mutation. Unknown faults remain usage errors, never passing controls. */
    if (argc == 3) {
        if (strcmp(argv[1], "cost_model") == 0 && strcmp(argv[2], "fault-constant") == 0) {
            g_fault_constant = 1;
        } else if (strcmp(argv[1], "map_matrix") == 0 &&
                   strcmp(argv[2], "fault-wrong-map-bit") == 0) {
            g_api_fault = API_FAULT_WRONG_MAP_BIT;
        } else if (strcmp(argv[1], "map_matrix") == 0 &&
                   strcmp(argv[2], "fault-special-address-order") == 0) {
            g_api_fault = API_FAULT_SPECIAL_ADDRESS_ORDER;
        } else if (strcmp(argv[1], "memory_peek_pure") == 0 &&
                   strcmp(argv[2], "fault-raw-as-mapped") == 0) {
            g_api_fault = API_FAULT_RAW_AS_MAPPED;
        } else if (strcmp(argv[1], "rom_unavailable") == 0 &&
                   strcmp(argv[2], "fault-fallback-to-ram") == 0) {
            g_api_fault = API_FAULT_ROM_FALLBACK;
        } else if (strcmp(argv[1], "watch_post_write") == 0 &&
                   strcmp(argv[2], "fault-stop-before-store") == 0) {
            g_api_fault = API_FAULT_STOP_BEFORE_STORE;
        } else if (strcmp(argv[1], "watch_controls") == 0 &&
                   strcmp(argv[2], "fault-repeat-store") == 0) {
            g_api_fault = API_FAULT_REPEAT_STORE;
        } else if (strcmp(argv[1], "snapshot_continue") == 0 &&
                   strcmp(argv[2], "fault-watch-in-digest") == 0) {
            g_api_fault = API_FAULT_WATCH_IN_DIGEST;
        } else {
            fprintf(stderr, "usage: lynx-test-api <case> [named-fault]\n");
            return 2;
        }
    } else if (argc != 2) {
        fprintf(stderr, "usage: lynx-test-api <case> [named-fault]\n");
        return 2;
    }
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            cases[i].run();
            CHECK_DONE();
        }
    }
    fprintf(stderr, "lynx-test-api: unknown case %s\n", argv[1]);
    return 2;
}
