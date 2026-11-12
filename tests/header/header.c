/* The public header alone, compiled as C17 with warnings fatal. Including
   only "lynxplosion.h" proves the header is self-contained. The static
   assertions pin the sizes and values the snapshot format and the runner
   depend on, and the function-pointer table pins every public signature: a
   changed declaration stops this file compiling. */
#include "lynxplosion.h"

_Static_assert(sizeof(lynx_status) == 4, "status is 32 bits");
_Static_assert(sizeof(lynx_stop_reason) == 4, "stop reason is 32 bits");
_Static_assert(sizeof(lynx_config) == 32, "config layout");
_Static_assert(sizeof(lynx_identity) == 88, "identity layout");
_Static_assert(sizeof(lynx_frame_buffers) == 56, "frame buffers layout");
_Static_assert(sizeof(lynx_frame_result) == 48, "frame result layout");
_Static_assert(sizeof(lynx_watch_event) == 32, "watch event layout");
_Static_assert(LYNX_CONTENT_BS93 == 2, "BS93 content format id");
_Static_assert(LYNX_STARTUP_A == 1, "tier A startup id");
_Static_assert(LYNX_STOP_HALT == 3, "stop reason HALT value");
_Static_assert(LYNX_STOP_WATCH == 2, "stop reason WATCH value");
_Static_assert(LYNX_STOP_UNDEFINED == 4, "stop reason UNDEFINED value");
_Static_assert(LYNX_STOP_UNAVAILABLE == 5, "stop reason UNAVAILABLE value");
_Static_assert(LYNX_ERR_UNAVAILABLE == 10, "status UNAVAILABLE value");
_Static_assert(LYNX_PROFILE_NONE == 0, "profile id none");
_Static_assert(LYNX_PROFILE_LYNX1 == 1, "profile id lynx1");
_Static_assert(LYNX_PROFILE_LYNX2 == 2, "profile id lynx2");
_Static_assert(LYNX_REG_CPU_STATE == 6, "register id CPU_STATE");
_Static_assert(LYNX_REG_CPU_LATCH == 7, "register id CPU_LATCH");
_Static_assert(LYNX_REG_MAPCTL == 8, "register id MAPCTL");
_Static_assert(LYNX_REG_COUNT == 9, "register count");
_Static_assert(LYNX_ADDRESS_SPACE == 0x10000u, "address space size");

static void (*const config_default_fn)(lynx_config *) = lynx_config_default;
static const char *(*const profile_name_fn)(uint32_t) = lynx_profile_name;
static size_t (*const machine_size_fn)(void) = lynx_machine_size;
static size_t (*const machine_align_fn)(void) = lynx_machine_align;
static lynx_status (*const machine_init_fn)(void *, size_t, const lynx_config *,
                                            lynx_machine **) = lynx_machine_init;
static void (*const machine_deinit_fn)(lynx_machine *) = lynx_machine_deinit;
static lynx_status (*const reset_fn)(lynx_machine *) = lynx_reset;
/* Normal content startup uses the complete BS93 container. */
static lynx_status (*const load_bll_fn)(lynx_machine *, const uint8_t *, size_t) = lynx_load_bll;
static lynx_status (*const advance_fn)(lynx_machine *, uint64_t, uint64_t *,
                                       lynx_stop_reason *) = lynx_advance;
static lynx_status (*const set_input_fn)(lynx_machine *, uint32_t) = lynx_set_input;
static lynx_status (*const advance_frame_fn)(lynx_machine *, uint64_t,
                                             const lynx_frame_buffers *,
                                             lynx_frame_result *) = lynx_advance_frame;
static lynx_status (*const watch_configure_fn)(lynx_machine *, uint32_t, size_t) =
    lynx_watch_configure;
static lynx_status (*const watch_clear_fn)(lynx_machine *) = lynx_watch_clear;
static lynx_status (*const watch_get_fn)(const lynx_machine *, lynx_watch_event *) =
    lynx_watch_get;
static lynx_status (*const read_register_fn)(const lynx_machine *, uint32_t,
                                             uint32_t *) = lynx_read_register;
static lynx_status (*const read_memory_fn)(const lynx_machine *, uint32_t, uint8_t *,
                                           size_t) = lynx_read_memory;
static lynx_status (*const read_cpu_memory_fn)(const lynx_machine *, uint32_t, uint8_t *,
                                               size_t) = lynx_read_cpu_memory;
static lynx_status (*const guest_ticks_fn)(const lynx_machine *, uint64_t *) = lynx_guest_ticks;
static lynx_status (*const get_identity_fn)(const lynx_machine *,
                                            lynx_identity *) = lynx_get_identity;
static lynx_status (*const state_digest_fn)(const lynx_machine *, uint8_t[32]) =
    lynx_state_digest;
static const char *(*const build_id_fn)(void) = lynx_build_id;
static lynx_status (*const snapshot_size_fn)(const lynx_machine *, size_t *) = lynx_snapshot_size;
static lynx_status (*const snapshot_save_fn)(const lynx_machine *, uint8_t *, size_t,
                                             size_t *) = lynx_snapshot_save;
static lynx_status (*const snapshot_restore_fn)(lynx_machine *, const uint8_t *,
                                                size_t) = lynx_snapshot_restore;

int main(void);

int main(void)
{
    (void)config_default_fn;
    (void)profile_name_fn;
    (void)machine_size_fn;
    (void)machine_align_fn;
    (void)machine_init_fn;
    (void)machine_deinit_fn;
    (void)reset_fn;
    (void)load_bll_fn;
    (void)advance_fn;
    (void)set_input_fn;
    (void)advance_frame_fn;
    (void)watch_configure_fn;
    (void)watch_clear_fn;
    (void)watch_get_fn;
    (void)read_register_fn;
    (void)read_memory_fn;
    (void)read_cpu_memory_fn;
    (void)guest_ticks_fn;
    (void)get_identity_fn;
    (void)state_digest_fn;
    (void)build_id_fn;
    (void)snapshot_size_fn;
    (void)snapshot_save_fn;
    (void)snapshot_restore_fn;
    return 0;
}
