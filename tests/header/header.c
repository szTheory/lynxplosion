/* The public header alone, compiled as C17 with warnings fatal. Including
   only "lynxplosion.h" proves the header is self-contained. The static
   assertions pin the sizes and values the snapshot format and the runner
   depend on, and the function-pointer table pins every public signature: a
   changed declaration stops this file compiling.

   Plans 01-06 and 01-07 must add lynx_reset and the snapshot functions to
   the table below when they declare them. */
#include "lynxplosion.h"

_Static_assert(sizeof(lynx_status) == 4, "status is 32 bits");
_Static_assert(sizeof(lynx_stop_reason) == 4, "stop reason is 32 bits");
_Static_assert(sizeof(lynx_config) == 24, "config layout");
_Static_assert(sizeof(lynx_identity) == 64, "identity layout");
_Static_assert(LYNX_STOP_HALT == 3, "stop reason HALT value");
_Static_assert(LYNX_REG_COUNT == 6, "register count");
_Static_assert(LYNX_ADDRESS_SPACE == 0x10000u, "address space size");

static void (*const config_default_fn)(lynx_config *) = lynx_config_default;
static size_t (*const machine_size_fn)(void) = lynx_machine_size;
static size_t (*const machine_align_fn)(void) = lynx_machine_align;
static lynx_status (*const machine_init_fn)(void *, size_t, const lynx_config *,
                                            lynx_machine **) = lynx_machine_init;
static void (*const machine_deinit_fn)(lynx_machine *) = lynx_machine_deinit;
static lynx_status (*const load_ram_image_fn)(lynx_machine *, const uint8_t *, size_t,
                                              uint32_t) = lynx_load_ram_image;
static lynx_status (*const advance_fn)(lynx_machine *, uint64_t, uint64_t *,
                                       lynx_stop_reason *) = lynx_advance;
static lynx_status (*const read_register_fn)(const lynx_machine *, uint32_t,
                                             uint32_t *) = lynx_read_register;
static lynx_status (*const read_memory_fn)(const lynx_machine *, uint32_t, uint8_t *,
                                           size_t) = lynx_read_memory;
static lynx_status (*const guest_ticks_fn)(const lynx_machine *, uint64_t *) = lynx_guest_ticks;
static lynx_status (*const get_identity_fn)(const lynx_machine *,
                                            lynx_identity *) = lynx_get_identity;
static const char *(*const build_id_fn)(void) = lynx_build_id;

int main(void);

int main(void)
{
    (void)config_default_fn;
    (void)machine_size_fn;
    (void)machine_align_fn;
    (void)machine_init_fn;
    (void)machine_deinit_fn;
    (void)load_ram_image_fn;
    (void)advance_fn;
    (void)read_register_fn;
    (void)read_memory_fn;
    (void)guest_ticks_fn;
    (void)get_identity_fn;
    (void)build_id_fn;
    return 0;
}
