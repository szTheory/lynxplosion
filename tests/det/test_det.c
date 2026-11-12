/* Determinism tests that need two machines in one process.
   One executable, one case per CTest entry: `lynx-test-det <case> <args>`.
   An unknown case exits 2, so a typo in tests.cmake fails instead of passing.

   Host side: it uses stdio and POSIX mkdir, like the runner. */
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bundle.h"
#include "check.h"
#include "internal.h"
#include "lynxplosion.h"

#define LOAD_ADDR 0x0200u
#define SEED 0x0123456789abcdefu
#define CHUNK 512u
#define CHUNKS 8u
#define BUDGET (CHUNK * CHUNKS)
#define CHECK_NAMED(cond, name)                                                                     \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "ASSERT %s failed\n", (name));                                     \
            check_failures++;                                                                      \
        }                                                                                          \
    } while (0)

static const char *const bundle_files[] = {"obs.ram.hex", "obs.regs.hex", "obs.state.hex",
                                           "obs.time.hex", "run.manifest", "digests.txt"};

/* Reads the digest from the last non-comment, non-empty line of the golden. */
static int read_golden(const char *path, char out[65])
{
    char line[256];
    int found = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 1;
    }
    while (fgets(line, (int)sizeof line, f) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n > 0 && line[0] != '#') {
            if (n != 64) {
                fclose(f);
                return 1;
            }
            memcpy(out, line, 65);
            found = 1;
        }
    }
    fclose(f);
    return found ? 0 : 1;
}

/* Reads a whole file; the caller frees *data. */
static int read_file(const char *path, uint8_t **data, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    size_t n;
    if (f == NULL) {
        return 1;
    }
    buf = malloc(1u << 16);
    if (buf == NULL) {
        fclose(f);
        return 1;
    }
    n = fread(buf, 1, 1u << 16, f);
    fclose(f);
    *data = buf;
    *len = n;
    return 0;
}

/* A fresh, empty bundle directory: the known bundle files of an earlier run
   are removed, then the directory is created if it is missing. */
static int fresh_dir(const char *dir)
{
    char path[4096];
    size_t i;
    for (i = 0; i < sizeof bundle_files / sizeof bundle_files[0]; i++) {
        int n = snprintf(path, sizeof path, "%s/%s", dir, bundle_files[i]);
        if (n < 0 || (size_t)n >= sizeof path) {
            return 1;
        }
        (void)remove(path);
    }
    if (mkdir(dir, 0777) != 0) {
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return 1;
        }
    }
    return 0;
}

static int ensure_dir(const char *dir)
{
    struct stat st;
    if (mkdir(dir, 0777) == 0) {
        return 0;
    }
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : 1;
}

/* One machine in a caller buffer, built the way the runner builds it. */
typedef struct instance {
    void *mem;
    lynx_machine *m;
    uint64_t consumed;
    int all_budget_stops;
} instance;

static int instance_make_p(instance *in, uint32_t profile, const uint8_t *content, size_t len)
{
    lynx_config cfg;
    uint8_t *object;
    size_t object_len;
    lynx_status load_status;
    lynx_config_default(&cfg);
    cfg.profile = profile;
    cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
    cfg.ram_seed = SEED;
    memset(in, 0, sizeof *in);
    in->all_budget_stops = 1;
    in->mem = malloc(lynx_machine_size());
    if (in->mem == NULL || (uintptr_t)in->mem % lynx_machine_align() != 0) {
        return 1;
    }
    if (lynx_machine_init(in->mem, lynx_machine_size(), &cfg, &in->m) != LYNX_OK) {
        return 1;
    }
    if (content == NULL || len == 0u || len > UINT16_MAX - 10u) {
        return 1;
    }
    object_len = len + 10u;
    object = malloc(object_len);
    if (object == NULL) {
        return 1;
    }
    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(LOAD_ADDR >> 8);
    object[3] = (uint8_t)LOAD_ADDR;
    object[4] = (uint8_t)(object_len >> 8);
    object[5] = (uint8_t)object_len;
    object[6] = 'B';
    object[7] = 'S';
    object[8] = '9';
    object[9] = '3';
    memcpy(object + 10u, content, len);
    load_status = lynx_load_bll(in->m, object, object_len);
    free(object);
    return load_status == LYNX_OK ? 0 : 1;
}

static void digest_hex(const uint8_t digest[32], char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 32u; i++) {
        hex[2u * i] = digits[digest[i] >> 4];
        hex[2u * i + 1u] = digits[digest[i] & 0x0Fu];
    }
    hex[64] = '\0';
}

static int read_manifest(const char *dir, char *buf, size_t cap)
{
    char path[4096];
    FILE *f;
    size_t n;
    int extra;
    int failed;
    int written = snprintf(path, sizeof path, "%s/run.manifest", dir);
    if (written < 0 || (size_t)written >= sizeof path || cap < 2u) {
        return 1;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return 1;
    }
    n = fread(buf, 1u, cap - 1u, f);
    extra = fgetc(f);
    failed = ferror(f) || extra != EOF;
    if (fclose(f) != 0) {
        failed = 1;
    }
    if (failed) {
        return 1;
    }
    buf[n] = '\0';
    return 0;
}

static int instance_make(instance *in, const uint8_t *content, size_t len)
{
    return instance_make_p(in, LYNX_PROFILE_NONE, content, len);
}

static void instance_advance(instance *in)
{
    uint64_t used = 0;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(in->m, CHUNK, &used, &stop), LYNX_OK);
    if (stop != LYNX_STOP_BUDGET) {
        in->all_budget_stops = 0;
    }
    in->consumed += used;
}

static bundle_run instance_run(const instance *in)
{
    bundle_run run;
    run.content_name = NULL;
    run.tick_budget = BUDGET;
    run.ticks_consumed = in->consumed;
    run.stop = LYNX_STOP_BUDGET;
    return run;
}

static void require_same_public_guest(const char *path, const lynx_machine *reference,
                                     const lynx_machine *candidate)
{
    static uint8_t ram_reference[LYNX_ADDRESS_SPACE];
    static uint8_t ram_candidate[LYNX_ADDRESS_SPACE];
    uint32_t regs_reference[LYNX_REG_COUNT];
    uint32_t regs_candidate[LYNX_REG_COUNT];
    uint64_t ticks_reference = 0u;
    uint64_t ticks_candidate = 0u;
    lynx_identity id_reference;
    lynx_identity id_candidate;
    uint8_t peek_reference = 0xA5u;
    uint8_t peek_candidate = 0x5Au;
    size_t size_reference = 0u;
    size_t size_candidate = 0u;
    size_t written_reference = 0u;
    size_t written_candidate = 0u;
    uint8_t *snapshot_reference = NULL;
    uint8_t *snapshot_candidate = NULL;
    uint32_t reg;

    CHECK_EQ_U64(lynx_read_memory(reference, 0u, ram_reference, sizeof ram_reference), LYNX_OK);
    CHECK_EQ_U64(lynx_read_memory(candidate, 0u, ram_candidate, sizeof ram_candidate), LYNX_OK);
    if (memcmp(ram_reference, ram_candidate, sizeof ram_reference) != 0) {
        fprintf(stderr, "watch_snapshot: %s raw RAM differs from uninterrupted execution\n", path);
        check_failures++;
    }
    for (reg = 0u; reg < LYNX_REG_COUNT; reg++) {
        CHECK_EQ_U64(lynx_read_register(reference, reg, &regs_reference[reg]), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(candidate, reg, &regs_candidate[reg]), LYNX_OK);
    }
    if (memcmp(regs_reference, regs_candidate, sizeof regs_reference) != 0) {
        fprintf(stderr, "watch_snapshot: %s public registers differ from uninterrupted execution\n",
                path);
        check_failures++;
    }
    CHECK_EQ_U64(lynx_guest_ticks(reference, &ticks_reference), LYNX_OK);
    CHECK_EQ_U64(lynx_guest_ticks(candidate, &ticks_candidate), LYNX_OK);
    CHECK_EQ_U64(ticks_candidate, ticks_reference);
    CHECK_EQ_U64(lynx_get_identity(reference, &id_reference), LYNX_OK);
    CHECK_EQ_U64(lynx_get_identity(candidate, &id_candidate), LYNX_OK);
    if (id_reference.profile != id_candidate.profile ||
        id_reference.ram_policy != id_candidate.ram_policy ||
        id_reference.ram_seed != id_candidate.ram_seed ||
        id_reference.has_content != id_candidate.has_content ||
        id_reference.content_addr != id_candidate.content_addr ||
        id_reference.content_len != id_candidate.content_len ||
        id_reference.content_format != id_candidate.content_format ||
        id_reference.startup_tier != id_candidate.startup_tier ||
        id_reference.entry_addr != id_candidate.entry_addr ||
        id_reference.object_len != id_candidate.object_len ||
        memcmp(id_reference.content_sha256, id_candidate.content_sha256,
               sizeof id_reference.content_sha256) != 0) {
        fprintf(stderr, "watch_snapshot: %s identity differs from uninterrupted execution\n", path);
        check_failures++;
    }
    CHECK_EQ_U64(lynx_read_cpu_memory(reference, 0xFFF9u, &peek_reference, 1u),
                 LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_U64(lynx_read_cpu_memory(candidate, 0xFFF9u, &peek_candidate, 1u),
                 LYNX_ERR_UNAVAILABLE);
    CHECK_EQ_HEX(peek_reference, 0xA5u);
    CHECK_EQ_HEX(peek_candidate, 0x5Au);
    CHECK_EQ_U64(lynx_snapshot_size(reference, &size_reference), LYNX_OK);
    CHECK_EQ_U64(lynx_snapshot_size(candidate, &size_candidate), LYNX_OK);
    CHECK_EQ_U64(size_candidate, size_reference);
    snapshot_reference = malloc(size_reference);
    snapshot_candidate = malloc(size_candidate);
    CHECK(snapshot_reference != NULL);
    CHECK(snapshot_candidate != NULL);
    if (snapshot_reference != NULL && snapshot_candidate != NULL) {
        CHECK_EQ_U64(lynx_snapshot_save(reference, snapshot_reference, size_reference,
                                        &written_reference), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_save(candidate, snapshot_candidate, size_candidate,
                                        &written_candidate), LYNX_OK);
        CHECK_EQ_U64(written_reference, size_reference);
        CHECK_EQ_U64(written_candidate, size_candidate);
        if (memcmp(snapshot_reference, snapshot_candidate, size_reference) != 0) {
            fprintf(stderr, "watch_snapshot: %s guest snapshots differ from uninterrupted execution\n",
                    path);
            check_failures++;
        }
    }
    free(snapshot_reference);
    free(snapshot_candidate);
}

/* The watch proof deliberately uses only common CPU opcodes and a real RAM
   store. Every compared bundle_run describes the aggregate path to the same
   final tick, so intermediate WATCH returns never enter guest equality. */
static void watch_snapshot_agreement(void)
{
    static const uint8_t program[] = {
        0xA9u, 0x5Au,             /* LDA #$5A */
        0x8Du, 0x00u, 0x04u,      /* STA $0400 */
        0x4Cu, 0x00u, 0x02u       /* JMP $0200 */
    };
    static const uint32_t profiles[] = {
        LYNX_PROFILE_NONE, LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2
    };
    static const uint32_t cost_models[] = {LYNX__COST_FLAT5, LYNX__COST_TEST_SKEW};
    static const uint64_t slices[] = {1u, 127u, 512u, 2048u};
    enum { BASELINE = 0, SLICED = 1, WATCHED = 2, RESTORED = 3, SOURCE = 4, INSTANCE_COUNT = 5 };
    const uint64_t final_tick = 4096u;
    size_t profile_index;
    size_t cost_index;

    for (profile_index = 0u; profile_index < sizeof profiles / sizeof profiles[0]; profile_index++) {
        for (cost_index = 0u; cost_index < sizeof cost_models / sizeof cost_models[0]; cost_index++) {
            instance machines[INSTANCE_COUNT] = {{0}};
            uint64_t requested[4] = {0u, 0u, 0u, 0u};
            uint64_t ticks[4] = {0u, 0u, 0u, 0u};
            char digests[4][65];
            bundle_run runs[4];
            uint8_t *snapshot = NULL;
            size_t snapshot_size = 0u;
            size_t machine_index;
            uint64_t used = 0u;
            uint64_t elapsed = 0u;
            lynx_stop_reason stop = LYNX_STOP_HALT;
            lynx_status status = LYNX_OK;
            unsigned watch_hits = 0u;
            uint64_t checkpoint_requested = 0u;
            uint64_t checkpoint_tick = 0u;

            for (machine_index = 0u; machine_index < INSTANCE_COUNT; machine_index++) {
                if (instance_make_p(&machines[machine_index], profiles[profile_index], program,
                                    sizeof program) != 0 || machines[machine_index].m == NULL) {
                    fprintf(stderr, "watch_snapshot: failed to create profile %s instance\n",
                            lynx_profile_name(profiles[profile_index]));
                    check_failures++;
                    goto combo_cleanup;
                }
                CHECK_EQ_U64(lynx__machine_set_cost_model(machines[machine_index].m,
                                                          cost_models[cost_index]), LYNX_OK);
            }

            status = lynx_advance(machines[BASELINE].m, final_tick, &used, &stop);
            CHECK_EQ_U64(status, LYNX_OK);
            CHECK_EQ_U64(used, final_tick);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
            if (status != LYNX_OK || used != final_tick || stop != LYNX_STOP_BUDGET) {
                goto combo_cleanup;
            }
            ticks[BASELINE] = used;
            requested[BASELINE] = final_tick;

            elapsed = 0u;
            machine_index = 0u;
            while (elapsed < final_tick) {
                const uint64_t remaining = final_tick - elapsed;
                uint64_t budget = slices[machine_index % (sizeof slices / sizeof slices[0])];
                if (budget > remaining) {
                    budget = remaining;
                }
                machine_index++;
                used = 0u;
                stop = LYNX_STOP_HALT;
                requested[SLICED] += budget;
                status = lynx_advance(machines[SLICED].m, budget, &used, &stop);
                CHECK_EQ_U64(status, LYNX_OK);
                CHECK_EQ_U64(used, budget);
                CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
                if (status != LYNX_OK || used != budget || stop != LYNX_STOP_BUDGET) {
                    goto combo_cleanup;
                }
                elapsed += used;
            }
            ticks[SLICED] = elapsed;

            CHECK_EQ_U64(lynx_watch_configure(machines[WATCHED].m, 0x0400u, 1u), LYNX_OK);
            elapsed = 0u;
            while (elapsed < final_tick) {
                const uint64_t remaining = final_tick - elapsed;
                uint64_t watch_used = 0u;
                stop = LYNX_STOP_HALT;
                requested[WATCHED] += remaining;
                status = lynx_advance(machines[WATCHED].m, remaining, &watch_used, &stop);
                CHECK_EQ_U64(status, LYNX_OK);
                CHECK(watch_used > 0u && watch_used <= remaining);
                if (status != LYNX_OK || watch_used == 0u || watch_used > remaining) {
                    goto combo_cleanup;
                }
                elapsed += watch_used;
                if (stop == LYNX_STOP_WATCH) {
                    lynx_watch_event event;
                    memset(&event, 0, sizeof event);
                    CHECK_EQ_U64(lynx_watch_get(machines[WATCHED].m, &event), LYNX_OK);
                    CHECK_EQ_U64(event.hit, 1u);
                    CHECK_EQ_U64(event.address, 0x0400u);
                    CHECK_EQ_U64(event.value, 0x5Au);
                    CHECK_EQ_U64(event.return_tick, event.hit_tick + 1u);
                    watch_hits++;
                    if (elapsed < final_tick) {
                        CHECK_EQ_U64(lynx_watch_configure(machines[WATCHED].m, 0x0400u, 1u),
                                     LYNX_OK);
                    } else {
                        uint64_t zero_used = 1u;
                        lynx_stop_reason final_stop = LYNX_STOP_HALT;
                        CHECK_EQ_U64(lynx_advance(machines[WATCHED].m, 0u, &zero_used,
                                                  &final_stop), LYNX_OK);
                        CHECK_EQ_U64(zero_used, 0u);
                        CHECK_EQ_U64(final_stop, LYNX_STOP_BUDGET);
                    }
                } else {
                    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
                    CHECK_EQ_U64(elapsed, final_tick);
                    if (stop != LYNX_STOP_BUDGET || elapsed != final_tick) {
                        goto combo_cleanup;
                    }
                }
            }
            CHECK(watch_hits > 1u);
            ticks[WATCHED] = elapsed;

            /* Stop at a reachable mid-instruction, in-cycle point, then restore
               into a freshly loaded equal-identity destination with a host watch armed. */
            used = 0u;
            stop = LYNX_STOP_HALT;
            status = lynx_advance(machines[SOURCE].m, 1024u, &used, &stop);
            CHECK_EQ_U64(status, LYNX_OK);
            CHECK_EQ_U64(used, 1024u);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
            if (status != LYNX_OK || used != 1024u || stop != LYNX_STOP_BUDGET) {
                goto combo_cleanup;
            }
            checkpoint_requested = 1024u;
            for (unsigned probe = 0u;
                 !(machines[SOURCE].m->cpu.step != 0u &&
                   machines[SOURCE].m->cpu_ready_at > machines[SOURCE].m->guest_ticks);
                 probe++) {
                if (probe >= 512u) {
                    fprintf(stderr, "watch_snapshot: no mid-instruction/in-cycle checkpoint\n");
                    check_failures++;
                    goto combo_cleanup;
                }
                used = 0u;
                stop = LYNX_STOP_HALT;
                status = lynx_advance(machines[SOURCE].m, 1u, &used, &stop);
                CHECK_EQ_U64(status, LYNX_OK);
                CHECK_EQ_U64(used, 1u);
                CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
                if (status != LYNX_OK || used != 1u || stop != LYNX_STOP_BUDGET) {
                    goto combo_cleanup;
                }
                checkpoint_requested++;
            }
            CHECK(machines[SOURCE].m->cpu.step != 0u);
            CHECK(machines[SOURCE].m->cpu_ready_at > machines[SOURCE].m->guest_ticks);
            checkpoint_tick = machines[SOURCE].m->guest_ticks;
            CHECK(checkpoint_tick < final_tick);
            if (checkpoint_tick >= final_tick) {
                goto combo_cleanup;
            }
            CHECK_EQ_U64(lynx_snapshot_size(machines[SOURCE].m, &snapshot_size), LYNX_OK);
            snapshot = malloc(snapshot_size);
            CHECK(snapshot != NULL);
            if (snapshot == NULL) {
                goto combo_cleanup;
            }
            {
                size_t written = 0u;
                CHECK_EQ_U64(lynx_snapshot_save(machines[SOURCE].m, snapshot, snapshot_size,
                                                &written), LYNX_OK);
                CHECK_EQ_U64(written, snapshot_size);
            }
            CHECK_EQ_U64(lynx_watch_configure(machines[RESTORED].m, 0x0400u, 1u), LYNX_OK);
            CHECK_EQ_U64(lynx_snapshot_restore(machines[RESTORED].m, snapshot, snapshot_size),
                         LYNX_OK);
            {
                lynx_watch_event event;
                memset(&event, 0xA5, sizeof event);
                CHECK_EQ_U64(lynx_watch_get(machines[RESTORED].m, &event), LYNX_OK);
                CHECK_EQ_U64(event.hit, 0u);
                CHECK_EQ_U64(event.address, 0u);
            }
            used = 0u;
            stop = LYNX_STOP_HALT;
            status = lynx_advance(machines[RESTORED].m, final_tick - checkpoint_tick, &used, &stop);
            CHECK_EQ_U64(status, LYNX_OK);
            CHECK_EQ_U64(used, final_tick - checkpoint_tick);
            CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
            if (status != LYNX_OK || used != final_tick - checkpoint_tick ||
                stop != LYNX_STOP_BUDGET) {
                goto combo_cleanup;
            }
            ticks[RESTORED] = checkpoint_tick + used;
            requested[RESTORED] = checkpoint_requested + (final_tick - checkpoint_tick);

            runs[BASELINE] = (bundle_run){NULL, requested[BASELINE], ticks[BASELINE],
                                          LYNX_STOP_BUDGET};
            runs[SLICED] = (bundle_run){NULL, requested[SLICED], ticks[SLICED],
                                        LYNX_STOP_BUDGET};
            runs[WATCHED] = (bundle_run){NULL, requested[WATCHED], ticks[WATCHED],
                                         LYNX_STOP_BUDGET};
            runs[RESTORED] = (bundle_run){NULL, requested[RESTORED], ticks[RESTORED],
                                          LYNX_STOP_BUDGET};
            {
                lynx_machine *paths[4] = {machines[BASELINE].m, machines[SLICED].m,
                                          machines[WATCHED].m, machines[RESTORED].m};
                const char *names[4] = {"uninterrupted", "split-budget", "watched/rearmed",
                                        "restored"};
                for (machine_index = 0u; machine_index < 4u; machine_index++) {
                    CHECK_EQ_U64(ticks[machine_index], final_tick);
                    CHECK_EQ_U64(bundle_observe(paths[machine_index], &runs[machine_index],
                                                digests[machine_index]), 0);
                    require_same_public_guest(names[machine_index], machines[BASELINE].m,
                                              paths[machine_index]);
                    if (machine_index != BASELINE &&
                        strcmp(digests[machine_index], digests[BASELINE]) != 0) {
                        fprintf(stderr, "watch_snapshot: %s digest differs for profile %s cost %u\n",
                                names[machine_index], lynx_profile_name(profiles[profile_index]),
                                cost_models[cost_index]);
                        check_failures++;
                    }
                }
            }
            if (check_failures == 0) {
                printf("watch_snapshot: profile=%s cost=%u tick=%llu digest=%s hits=%u\n",
                       lynx_profile_name(profiles[profile_index]), cost_models[cost_index],
                       (unsigned long long)final_tick, digests[BASELINE], watch_hits);
            }

combo_cleanup:
            free(snapshot);
            for (machine_index = 0u; machine_index < INSTANCE_COUNT; machine_index++) {
                if (machines[machine_index].m != NULL) {
                    lynx_machine_deinit(machines[machine_index].m);
                }
                free(machines[machine_index].mem);
            }
        }
    }
}

/* Two instances in one process, advanced interleaved in 512-tick chunks, each
   written as a whole bundle into its own fresh directory. DET-02. */
static void case_two_instances(char **args)
{
    const char *content_path = args[0];
    const char *golden_path = args[1];
    const char *work = args[2];
    uint8_t *content = NULL;
    size_t content_len = 0;
    char golden[65];
    char dir_a[4096];
    char dir_b[4096];
    char obs_a[65];
    char obs_b[65];
    char obs_w_a[65];
    char obs_w_b[65];
    char bundle_a[65];
    char bundle_b[65];
    char manifest[1024];
    char identity_sha[65];
    char want_line[128];
    lynx_identity identity;
    instance a;
    instance b;
    bundle_run run_a;
    bundle_run run_b;
    uint8_t *text_a = NULL;
    uint8_t *text_b = NULL;
    size_t len_a = 0;
    size_t len_b = 0;
    unsigned i;

    CHECK_EQ_U64(bundle_read_hex(content_path, &content, &content_len), 0);
    CHECK_EQ_U64(read_golden(golden_path, golden), 0);
    if (content == NULL || check_failures != 0) {
        free(content);
        return;
    }
    CHECK_EQ_U64(instance_make(&a, content, content_len), 0);
    CHECK_EQ_U64(instance_make(&b, content, content_len), 0);
    free(content);
    if (a.m == NULL || b.m == NULL) {
        free(a.mem);
        free(b.mem);
        return;
    }

    for (i = 0; i < CHUNKS; i++) {
        instance_advance(&a);
        instance_advance(&b);
    }
    CHECK_EQ_U64(a.consumed, BUDGET);
    CHECK_EQ_U64(b.consumed, BUDGET);
    CHECK(a.all_budget_stops);
    CHECK(b.all_budget_stops);

    run_a = instance_run(&a);
    run_b = instance_run(&b);
    CHECK_EQ_U64(bundle_observe(a.m, &run_a, obs_a), 0);
    CHECK_EQ_U64(bundle_observe(b.m, &run_b, obs_b), 0);
    CHECK(strcmp(obs_a, obs_b) == 0);
    CHECK(strcmp(obs_a, golden) == 0);

    CHECK_EQ_U64(ensure_dir(work), 0);
    CHECK_EQ_U64(snprintf(dir_a, sizeof dir_a, "%s/a", work) > 0, 1);
    CHECK_EQ_U64(snprintf(dir_b, sizeof dir_b, "%s/b", work) > 0, 1);
    CHECK_EQ_U64(fresh_dir(dir_a), 0);
    CHECK_EQ_U64(fresh_dir(dir_b), 0);
    CHECK_EQ_U64(bundle_write(dir_a, a.m, &run_a, obs_w_a, bundle_a), 0);
    CHECK_EQ_U64(bundle_write(dir_b, b.m, &run_b, obs_w_b, bundle_b), 0);
    CHECK_EQ_U64(lynx_get_identity(a.m, &identity), LYNX_OK);
    CHECK_EQ_U64(identity.content_format, LYNX_CONTENT_BS93);
    CHECK_EQ_U64(identity.startup_tier, LYNX_STARTUP_A);
    CHECK_EQ_U64(identity.content_addr, LOAD_ADDR);
    CHECK_EQ_U64(identity.entry_addr, LOAD_ADDR);
    CHECK_EQ_U64(identity.content_len, content_len);
    CHECK_EQ_U64(identity.object_len, content_len + 10u);
    digest_hex(identity.content_sha256, identity_sha);
    CHECK_EQ_U64(read_manifest(dir_a, manifest, sizeof manifest), 0);
    CHECK(strstr(manifest, "content_format = bs93\n") != NULL);
    CHECK(strstr(manifest, "startup = A\n") != NULL);
    CHECK(strstr(manifest, "startup_tier = A\n") != NULL);
    (void)snprintf(want_line, sizeof want_line, "content_sha256 = %s\n", identity_sha);
    CHECK(strstr(manifest, want_line) != NULL);
    (void)snprintf(want_line, sizeof want_line, "entry_addr = 0x%04x\n", LOAD_ADDR);
    CHECK(strstr(manifest, want_line) != NULL);
    (void)snprintf(want_line, sizeof want_line, "object_len = %zu\n", content_len + 10u);
    CHECK(strstr(manifest, want_line) != NULL);
    (void)snprintf(want_line, sizeof want_line, "payload_addr = 0x%04x\n", LOAD_ADDR);
    CHECK(strstr(manifest, want_line) != NULL);
    (void)snprintf(want_line, sizeof want_line, "payload_len = %zu\n", content_len);
    CHECK(strstr(manifest, want_line) != NULL);
    /* A missing run or machine is a refusal, not a crash (the header promises it). */
    CHECK(bundle_write(dir_a, a.m, NULL, obs_w_a, bundle_a) != 0);
    CHECK(bundle_write(dir_a, NULL, &run_a, obs_w_a, bundle_a) != 0);
    CHECK(bundle_observe(a.m, NULL, obs_a) != 0);
    CHECK(strcmp(obs_w_a, obs_a) == 0);
    CHECK(strcmp(obs_w_b, obs_b) == 0);
    CHECK(strcmp(bundle_a, bundle_b) == 0);

    /* Whole-bundle equality: every file is byte-identical, not only digests.txt. */
    for (i = 0; i < sizeof bundle_files / sizeof bundle_files[0]; i++) {
        char path_a[4096];
        char path_b[4096];
        CHECK_EQ_U64(snprintf(path_a, sizeof path_a, "%s/%s", dir_a, bundle_files[i]) > 0, 1);
        CHECK_EQ_U64(snprintf(path_b, sizeof path_b, "%s/%s", dir_b, bundle_files[i]) > 0, 1);
        CHECK_EQ_U64(read_file(path_a, &text_a, &len_a), 0);
        CHECK_EQ_U64(read_file(path_b, &text_b, &len_b), 0);
        if (text_a != NULL && text_b != NULL) {
            CHECK(len_a > 0);
            CHECK_EQ_U64(len_a, len_b);
            CHECK(len_a == len_b && memcmp(text_a, text_b, len_a) == 0);
        }
        free(text_a);
        free(text_b);
        text_a = NULL;
        text_b = NULL;
    }
    watch_snapshot_agreement();
    if (check_failures == 0) {
        printf("two_instances: ok observation %s bundle %s\n", obs_a, bundle_a);
    }
    lynx_machine_deinit(a.m);
    lynx_machine_deinit(b.m);
    free(a.mem);
    free(b.mem);
}

/* The observation digest of a machine after the fixed workload (4096 ticks in
   one call), for the state-flip cases below. */
static int digest_of(const lynx_machine *m, char out[65])
{
    bundle_run run;
    run.content_name = NULL;
    run.tick_budget = BUDGET;
    run.ticks_consumed = BUDGET;
    run.stop = LYNX_STOP_BUDGET;
    return bundle_observe(m, &run, out);
}

/* A byte for byte copy of a machine into fresh memory. The instance holds no
   pointers, so a memcpy of the caller buffer is a valid instance. */
static lynx_machine *copy_machine(const lynx_machine *src)
{
    void *mem = malloc(lynx_machine_size());
    if (mem == NULL || (uintptr_t)mem % lynx_machine_align() != 0) {
        free(mem);
        return NULL;
    }
    memcpy(mem, src, lynx_machine_size());
    return (lynx_machine *)mem;
}

/* ---- det.sensitivity_state ---- */

/* Each flip changes one piece of machine state in a copy of the machine, then
   brings the CPU's derived request back in line with it (lynx__cpu_repost), the
   way a restore does. The flip is checked to leave a state real execution can
   reach, so the digest changes through the public read path (registers,
   including ids 6 and 7, RAM and guest time) and not through a refusal. */
typedef struct flip {
    const char *name;
    void (*apply)(lynx_machine *m);
} flip;

static void flip_pc(lynx_machine *m) { m->cpu.pc ^= 1u; }
static void flip_a(lynx_machine *m) { m->cpu.a ^= 1u; }
static void flip_x(lynx_machine *m) { m->cpu.x ^= 1u; }
static void flip_y(lynx_machine *m) { m->cpu.y ^= 1u; }
static void flip_s(lynx_machine *m) { m->cpu.s ^= 1u; }
/* Bit 0, the carry: bits 4 and 5 always read as set, so they cannot show a flip. */
static void flip_p(lynx_machine *m) { m->cpu.p ^= 1u; }
/* A defined latch bit that is clear in the base state. */
static void flip_flags(lynx_machine *m) { m->cpu.flags ^= LYNX__CS_NMI_PREV; }
/* Another cycle of the same instruction (PLA is mid-instruction at 4096 ticks). */
static void flip_step(lynx_machine *m) { m->cpu.step = m->cpu.step == 1u ? 2u : 1u; }
/* Another opcode of the same addressing mode: PLA becomes PLY (both pull). */
static void flip_opcode(lynx_machine *m) { m->cpu.opcode = 0x7Au; }
static void flip_ea(lynx_machine *m) { m->cpu.ea ^= 1u; }
static void flip_tmp(lynx_machine *m) { m->cpu.tmp ^= 1u; }
/* Within the allowed range [guest_ticks, guest_ticks + 255]. */
static void flip_ready_at(lynx_machine *m) { m->cpu_ready_at = m->guest_ticks + 2u; }
static void flip_cost_model(lynx_machine *m) { m->cost_model = LYNX__COST_TEST_SKEW; }
static void flip_guest_ticks(lynx_machine *m)
{
    m->guest_ticks ^= 1u;
    m->cpu_ready_at ^= 1u; /* keeps cpu_ready_at - guest_ticks and the range as they were */
}
static void flip_ram(lynx_machine *m) { m->ram[0x0100u] ^= 1u; }

/* A digest that ignores state would make every equality test vacuous. Every
   piece of state the machine holds, one bit or one value at a time, must change
   it: registers, the CPU's position inside the instruction, its latches, the
   cost model, guest time and RAM. An unmodified copy must not change it. */
static void case_state_sensitivity(char **args)
{
    static const flip flips[] = {
        {"cpu.pc", flip_pc},
        {"register a", flip_a},
        {"register x", flip_x},
        {"register y", flip_y},
        {"register s", flip_s},
        {"register p (carry)", flip_p},
        {"cpu latch flags", flip_flags},
        {"cpu step", flip_step},
        {"cpu opcode", flip_opcode},
        {"cpu ea", flip_ea},
        {"cpu tmp", flip_tmp},
        {"cpu_ready_at", flip_ready_at},
        {"cost model", flip_cost_model},
        {"guest time", flip_guest_ticks},
        {"ram byte 0x0100", flip_ram},
    };
    enum { NFLIPS = sizeof flips / sizeof flips[0] };
    static char digests[NFLIPS][65];
    uint8_t *content = NULL;
    size_t content_len = 0;
    instance base;
    lynx_machine *copy;
    char want[65];
    char got[65];
    size_t i;
    size_t j;

    CHECK_EQ_U64(bundle_read_hex(args[0], &content, &content_len), 0);
    if (content == NULL) {
        return;
    }
    CHECK_EQ_U64(instance_make(&base, content, content_len), 0);
    free(content);
    if (base.m == NULL) {
        free(base.mem);
        return;
    }
    {
        uint64_t used = 0;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(base.m, BUDGET, &used, &stop), LYNX_OK);
        CHECK_EQ_U64(used, BUDGET);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    }
    CHECK_EQ_U64(digest_of(base.m, want), 0);
    /* The base state is mid-instruction (PLA, step 2), the position the step and
       opcode flips rely on. */
    CHECK_EQ_HEX(base.m->cpu.opcode, 0x68u);
    CHECK_EQ_HEX(base.m->cpu.step, 2u);
    CHECK(lynx__cpu_state_valid(&base.m->cpu, lynx__cpu_table(base.m->cpu.profile)));

    copy = copy_machine(base.m);
    CHECK(copy != NULL);
    if (copy != NULL) {
        CHECK_EQ_U64(digest_of(copy, got), 0);
        if (strcmp(got, want) != 0) {
            fprintf(stderr, "sensitivity_state: unmodified copy: digest changed without a change\n");
            check_failures++;
        }
        free(copy);
    }
    for (i = 0; i < NFLIPS; i++) {
        copy = copy_machine(base.m);
        CHECK(copy != NULL);
        if (copy == NULL) {
            continue;
        }
        flips[i].apply(copy);
        lynx__cpu_repost(&copy->cpu, lynx__cpu_table(copy->cpu.profile));
        if (!lynx__cpu_state_valid(&copy->cpu, lynx__cpu_table(copy->cpu.profile)) ||
            copy->cpu_ready_at < copy->guest_ticks || copy->cpu_ready_at - copy->guest_ticks > 255u) {
            fprintf(stderr, "sensitivity_state: %s: the flip left a state execution cannot reach\n",
                    flips[i].name);
            check_failures++;
        }
        CHECK_EQ_U64(digest_of(copy, digests[i]), 0);
        if (strcmp(digests[i], want) == 0) {
            fprintf(stderr, "sensitivity_state: %s: digest unchanged\n", flips[i].name);
            check_failures++;
        } else {
            printf("sensitivity_state: %s: digest differs (%s)\n", flips[i].name, digests[i]);
        }
        free(copy);
    }
    /* No two flips collapse to one digest. */
    for (i = 0; i < NFLIPS; i++) {
        for (j = i + 1u; j < NFLIPS; j++) {
            if (strcmp(digests[i], digests[j]) == 0) {
                fprintf(stderr, "sensitivity_state: %s and %s give the same digest\n", flips[i].name,
                        flips[j].name);
                check_failures++;
            }
        }
    }
    printf("sensitivity_state: base digest %s\n", want);
    free(base.mem);
}

/* ---- det.profile_agreement ---- */

/* The golden workload (churn-loop, seeded RAM, 4096 ticks) uses only opcodes the
   lynx1 and lynx2 tables define identically, so profile none, lynx1 and lynx2
   must give the same observation: the same eight registers, the same RAM, the
   same guest time and therefore the same observation digest (the one the golden
   file commits). The manifest's "profile = " line differs by design and is not
   part of the observation digest, so it is excluded from this comparison. */
static void case_profile_agreement(char **args)
{
    static const uint32_t profiles[3] = {LYNX_PROFILE_NONE, LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    uint8_t *content = NULL;
    size_t content_len = 0;
    instance in[3];
    char golden[65];
    char digest[3][65];
    static uint8_t ram[3][LYNX_ADDRESS_SPACE];
    uint32_t regs[3][LYNX_REG_COUNT];
    uint64_t ticks[3];
    size_t i;
    uint32_t r;

    CHECK_EQ_U64(bundle_read_hex(args[0], &content, &content_len), 0);
    CHECK_EQ_U64(read_golden(args[1], golden), 0);
    if (content == NULL || check_failures != 0) {
        free(content);
        return;
    }
    for (i = 0; i < 3u; i++) {
        unsigned chunk;
        CHECK_EQ_U64(instance_make_p(&in[i], profiles[i], content, content_len), 0);
        if (in[i].m == NULL) {
            free(content);
            return;
        }
        for (chunk = 0; chunk < CHUNKS; chunk++) {
            instance_advance(&in[i]);
        }
        CHECK_EQ_U64(in[i].consumed, BUDGET);
        CHECK(in[i].all_budget_stops);
        /* The instance holds the profile the host chose. */
        {
            lynx_identity id;
            CHECK_EQ_U64(lynx_get_identity(in[i].m, &id), LYNX_OK);
            CHECK_EQ_U64(id.profile, profiles[i]);
        }
        CHECK_EQ_U64(lynx_read_memory(in[i].m, 0, ram[i], LYNX_ADDRESS_SPACE), LYNX_OK);
        for (r = 0; r < LYNX_REG_COUNT; r++) {
            CHECK_EQ_U64(lynx_read_register(in[i].m, r, &regs[i][r]), LYNX_OK);
        }
        CHECK_EQ_U64(lynx_guest_ticks(in[i].m, &ticks[i]), LYNX_OK);
        CHECK_EQ_U64(digest_of(in[i].m, digest[i]), 0);
        /* Every profile reproduces the committed golden. */
        if (strcmp(digest[i], golden) != 0) {
            fprintf(stderr, "profile_agreement: profile %s digest %s, golden %s\n",
                    lynx_profile_name(profiles[i]), digest[i], golden);
            check_failures++;
        }
    }
    for (i = 1; i < 3u; i++) {
        CHECK(memcmp(ram[0], ram[i], LYNX_ADDRESS_SPACE) == 0);
        CHECK(memcmp(regs[0], regs[i], sizeof regs[0]) == 0);
        CHECK_EQ_U64(ticks[0], ticks[i]);
        CHECK(strcmp(digest[0], digest[i]) == 0);
    }
    if (check_failures == 0) {
        printf("profile_agreement: none, lynx1 and lynx2 agree, observation %s\n", digest[0]);
    }
    for (i = 0; i < 3u; i++) {
        if (in[i].m != NULL) {
            lynx_machine_deinit(in[i].m);
        }
        free(in[i].mem);
    }
    free(content);
}

/* A project-authored guest used by the public scheduler-watch checks. SEI
   keeps the latched timer IRQ observable without fetching an absent vector. Its
   RAM store grants at tick 80 after timer-bank and linked-borrow work there. */
static const uint8_t timer_paths_program[] = {
    0x78u, 0xE6u, 0x10u, 0xADu, 0x00u, 0x04u, 0xA9u, 0xA5u,
    0x8Du, 0x00u, 0x04u, 0x4Cu, 0x0Bu, 0x02u
};

static void timer_paths_configure(lynx_machine *m)
{
    m->timer_bank.timers[0] = (lynx__timer){0u, 4u, 0x98u, 0u};
    m->timer_bank.timers[1] = (lynx__timer){0u, 4u, 0x88u, 0u};
    m->timer_bank.timers[2] = (lynx__timer){0u, 0u, 0x8Fu, 0u};
}

static void timer_paths_advance_to(lynx_machine *m, uint64_t target, uint8_t mode)
{
    static const uint64_t irregular[] = {3u, 1u, 7u, 2u, 5u, 11u};
    size_t irregular_index = 0u;
    uint64_t ticks = 0u;
    CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    while (ticks < target) {
        const uint64_t remaining = target - ticks;
        uint64_t budget = mode == 0u ? remaining : (mode == 1u ? 1u :
                             irregular[irregular_index++ %
                                       (sizeof irregular / sizeof irregular[0])]);
        if (budget > remaining) {
            budget = remaining;
        }
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(m, budget, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(consumed, budget);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        if (stop != LYNX_STOP_BUDGET || consumed != budget) {
            break;
        }
        CHECK_EQ_U64(lynx_guest_ticks(m, &ticks), LYNX_OK);
    }
    CHECK_EQ_U64(ticks, target);
}

static void timer_paths_equal(const lynx_machine *reference, const lynx_machine *candidate,
                              const char *path)
{
    static uint8_t reference_ram[LYNX_ADDRESS_SPACE];
    static uint8_t candidate_ram[LYNX_ADDRESS_SPACE];
    uint32_t reference_regs[LYNX_REG_COUNT];
    uint32_t candidate_regs[LYNX_REG_COUNT];
    uint8_t reference_timers[32];
    uint8_t candidate_timers[32];
    uint8_t reference_irq[2];
    uint8_t candidate_irq[2];
    uint64_t reference_ticks = 0u;
    uint64_t candidate_ticks = 0u;
    size_t snapshot_size = 0u;
    size_t written_a = 0u;
    size_t written_b = 0u;
    uint8_t *snapshot_a = NULL;
    uint8_t *snapshot_b = NULL;

    CHECK_EQ_U64(lynx_guest_ticks(reference, &reference_ticks), LYNX_OK);
    CHECK_EQ_U64(lynx_guest_ticks(candidate, &candidate_ticks), LYNX_OK);
    CHECK_EQ_U64(reference_ticks, candidate_ticks);
    CHECK_EQ_U64(lynx_read_memory(reference, 0u, reference_ram, sizeof reference_ram), LYNX_OK);
    CHECK_EQ_U64(lynx_read_memory(candidate, 0u, candidate_ram, sizeof candidate_ram), LYNX_OK);
    if (memcmp(reference_ram, candidate_ram, sizeof reference_ram) != 0) {
        fprintf(stderr, "timer_paths: %s raw RAM differs at common tick %llu\n", path,
                (unsigned long long)reference_ticks);
        check_failures++;
    }
    CHECK_EQ_U64(lynx_read_cpu_memory(reference, 0xFD00u, reference_timers,
                                      sizeof reference_timers), LYNX_OK);
    CHECK_EQ_U64(lynx_read_cpu_memory(candidate, 0xFD00u, candidate_timers,
                                      sizeof candidate_timers), LYNX_OK);
    CHECK(memcmp(reference_timers, candidate_timers, sizeof reference_timers) == 0);
    CHECK_EQ_U64(lynx_read_cpu_memory(reference, 0xFD80u, reference_irq,
                                      sizeof reference_irq), LYNX_OK);
    CHECK_EQ_U64(lynx_read_cpu_memory(candidate, 0xFD80u, candidate_irq,
                                      sizeof candidate_irq), LYNX_OK);
    CHECK(memcmp(reference_irq, candidate_irq, sizeof reference_irq) == 0);
    for (uint32_t reg = 0u; reg < LYNX_REG_COUNT; reg++) {
        CHECK_EQ_U64(lynx_read_register(reference, reg, &reference_regs[reg]), LYNX_OK);
        CHECK_EQ_U64(lynx_read_register(candidate, reg, &candidate_regs[reg]), LYNX_OK);
    }
    CHECK(memcmp(reference_regs, candidate_regs, sizeof reference_regs) == 0);
    CHECK_EQ_U64(reference->cpu.req_addr, candidate->cpu.req_addr);
    CHECK_EQ_U64(reference->cpu.req_kind, candidate->cpu.req_kind);
    CHECK_EQ_U64(reference->cpu.req_flags, candidate->cpu.req_flags);
    CHECK_EQ_U64(reference->cpu.req_data, candidate->cpu.req_data);
    CHECK_EQ_U64(reference->timer_bank.pending, candidate->timer_bank.pending);
    CHECK_EQ_U64(reference->wake_pending, candidate->wake_pending);
    CHECK_EQ_U64(reference->bus_hold, candidate->bus_hold);

    CHECK_EQ_U64(lynx_snapshot_size(reference, &snapshot_size), LYNX_OK);
    snapshot_a = malloc(snapshot_size);
    snapshot_b = malloc(snapshot_size);
    CHECK(snapshot_a != NULL && snapshot_b != NULL);
    if (snapshot_a != NULL && snapshot_b != NULL) {
        CHECK_EQ_U64(lynx_snapshot_save(reference, snapshot_a, snapshot_size, &written_a), LYNX_OK);
        CHECK_EQ_U64(lynx_snapshot_save(candidate, snapshot_b, snapshot_size, &written_b), LYNX_OK);
        CHECK_EQ_U64(written_a, snapshot_size);
        CHECK_EQ_U64(written_b, snapshot_size);
        if (memcmp(snapshot_a, snapshot_b, snapshot_size) != 0) {
            fprintf(stderr, "timer_paths: %s canonical snapshot differs at common tick %llu\n",
                    path, (unsigned long long)reference_ticks);
            check_failures++;
        }
    }
    free(snapshot_a);
    free(snapshot_b);
}

static void case_timer_paths(char **args)
{
    enum { COARSE = 0, ONE_TICK, IRREGULAR, WATCHED, RESTORE_SOURCE, RESTORED, PATH_COUNT };
    static const uint32_t profiles[] = {LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    const uint64_t common_tick = 257u;

    (void)args;
    for (size_t p = 0u; p < sizeof profiles / sizeof profiles[0]; p++) {
        instance paths[PATH_COUNT] = {{0}};
        for (size_t i = 0u; i < PATH_COUNT; i++) {
            CHECK_EQ_U64(instance_make_p(&paths[i], profiles[p], timer_paths_program,
                                         sizeof timer_paths_program), 0u);
            if (paths[i].m == NULL) {
                goto profile_cleanup;
            }
            timer_paths_configure(paths[i].m);
        }

        timer_paths_advance_to(paths[COARSE].m, common_tick, 0u);
        timer_paths_advance_to(paths[ONE_TICK].m, common_tick, 1u);
        timer_paths_advance_to(paths[IRREGULAR].m, common_tick, 2u);

        CHECK_EQ_U64(lynx_watch_configure(paths[WATCHED].m, 0x0400u, 1u), LYNX_OK);
        {
            static const uint64_t watch_chunks[] = {3u, 1u, 7u, 2u, 5u, 11u};
            uint64_t consumed = UINT64_MAX;
            lynx_stop_reason stop = LYNX_STOP_BUDGET;
            lynx_watch_event event;
            size_t i = 0u;
            for (; i < 128u && stop != LYNX_STOP_WATCH; i++) {
                CHECK_EQ_U64(lynx_advance(paths[WATCHED].m,
                                          watch_chunks[i % (sizeof watch_chunks /
                                                            sizeof watch_chunks[0])],
                                          &consumed, &stop), LYNX_OK);
                CHECK(consumed != 0u);
                CHECK(stop == LYNX_STOP_BUDGET || stop == LYNX_STOP_WATCH);
                if (stop != LYNX_STOP_BUDGET && stop != LYNX_STOP_WATCH) {
                    break;
                }
            }
            CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
            CHECK_EQ_U64(lynx_watch_get(paths[WATCHED].m, &event), LYNX_OK);
            CHECK_EQ_U64(event.hit, 1u);
            CHECK_EQ_U64(event.address, 0x0400u);
            CHECK_EQ_U64(event.value, 0xA5u);
            CHECK_EQ_U64(event.hit_tick, 80u);
            CHECK_EQ_U64(event.return_tick, 81u);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.pending, 0x07u);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[0].count, 0u);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[1].count, 0u);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[2].count, 0u);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[0].control_b, 0x0Du);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[1].control_b, 0x0Du);
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[2].control_b, 0x0Fu);
        }
        timer_paths_advance_to(paths[WATCHED].m, common_tick, 2u);

        timer_paths_advance_to(paths[RESTORE_SOURCE].m, 73u, 2u);
        {
            size_t snapshot_size = 0u;
            size_t written = 0u;
            uint8_t *snapshot = NULL;
            CHECK_EQ_U64(lynx_snapshot_size(paths[RESTORE_SOURCE].m, &snapshot_size), LYNX_OK);
            snapshot = malloc(snapshot_size);
            CHECK(snapshot != NULL);
            if (snapshot != NULL) {
                CHECK_EQ_U64(lynx_snapshot_save(paths[RESTORE_SOURCE].m, snapshot,
                                                snapshot_size, &written), LYNX_OK);
                CHECK_EQ_U64(written, snapshot_size);
                CHECK_EQ_U64(lynx_snapshot_restore(paths[RESTORED].m, snapshot,
                                                   snapshot_size), LYNX_OK);
                timer_paths_equal(paths[RESTORE_SOURCE].m, paths[RESTORED].m,
                                  "restore checkpoint");
                free(snapshot);
            }
        }
        timer_paths_advance_to(paths[RESTORED].m, common_tick, 2u);

        timer_paths_equal(paths[COARSE].m, paths[ONE_TICK].m, "one-tick slicing");
        timer_paths_equal(paths[COARSE].m, paths[IRREGULAR].m, "irregular slicing");
        timer_paths_equal(paths[COARSE].m, paths[WATCHED].m, "watched grant with timer cascade");
        timer_paths_equal(paths[COARSE].m, paths[RESTORED].m, "snapshot continuation");
        CHECK_EQ_U64(paths[COARSE].m->guest_ticks, common_tick);
        CHECK_EQ_U64(paths[COARSE].m->ram[0x0400u], 0xA5u);
        printf("timer_paths: profile=%s common_tick=%llu paths=5 watch_grant=80 cascade_pending=%02x\n",
               lynx_profile_name(profiles[p]), (unsigned long long)common_tick,
               (unsigned)paths[WATCHED].m->timer_bank.pending);

profile_cleanup:
        for (size_t i = 0u; i < PATH_COUNT; i++) {
            if (paths[i].m != NULL) {
                lynx_machine_deinit(paths[i].m);
            }
            free(paths[i].mem);
        }
    }
}

typedef struct video_packet {
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_result result;
    uint64_t total_ticks;
    unsigned empty_frames;
    unsigned watch_hits;
    int complete;
} video_packet;

static size_t make_video_guest(uint8_t *program)
{
    size_t n = 0u;
    program[n++] = 0xA9u; program[n++] = 0x0Cu; /* MAPCTL: expose devices */
    program[n++] = 0x8Du; program[n++] = 0xF9u; program[n++] = 0xFFu;
    program[n++] = 0xA9u; program[n++] = 0x01u; /* enable video DMA */
    program[n++] = 0x8Du; program[n++] = 0x92u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x00u;
    program[n++] = 0x8Du; program[n++] = 0x94u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x04u;
    program[n++] = 0x8Du; program[n++] = 0x95u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x00u; /* right-hand orientation */
    program[n++] = 0x8Du; program[n++] = 0x92u; program[n++] = 0xFCu;
    program[n++] = 0xA9u; program[n++] = 0x1Fu; /* Timer 0 reload/count */
    program[n++] = 0x8Du; program[n++] = 0x00u; program[n++] = 0xFDu;
    program[n++] = 0x8Du; program[n++] = 0x02u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x68u; /* Timer 2 reload/count */
    program[n++] = 0x8Du; program[n++] = 0x08u; program[n++] = 0xFDu;
    program[n++] = 0x8Du; program[n++] = 0x0Au; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x1Fu; /* Timer 2 linked mode */
    program[n++] = 0x8Du; program[n++] = 0x09u; program[n++] = 0xFDu;
    program[n++] = 0xA9u; program[n++] = 0x18u; /* start Timer 0 last */
    program[n++] = 0x8Du; program[n++] = 0x01u; program[n++] = 0xFDu;
    const uint16_t loop = (uint16_t)(LOAD_ADDR + (uint16_t)n);
    program[n++] = 0xADu; program[n++] = 0xB0u; program[n++] = 0xFCu;
    program[n++] = 0x8Du; program[n++] = 0x00u; program[n++] = 0x04u;
    program[n++] = 0x4Cu;
    program[n++] = (uint8_t)loop;
    program[n++] = (uint8_t)(loop >> 8);
    return n;
}

static int video_instance_make(instance *in, uint32_t input_mask)
{
    lynx_config cfg;
    uint8_t program[128];
    uint8_t object[sizeof program + 10u];
    const size_t payload_len = make_video_guest(program);
    const size_t object_len = payload_len + 10u;

    lynx_config_default(&cfg);
    cfg.profile = LYNX_PROFILE_NONE;
    cfg.ram_policy = LYNX_RAM_POLICY_ZERO;
    cfg.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    memset(in, 0, sizeof *in);
    in->all_budget_stops = 1;
    in->mem = malloc(lynx_machine_size());
    if (in->mem == NULL || (uintptr_t)in->mem % lynx_machine_align() != 0u) return 1;
    if (lynx_machine_init(in->mem, lynx_machine_size(), &cfg, &in->m) != LYNX_OK) return 1;

    object[0] = 0x80u;
    object[1] = 0x08u;
    object[2] = (uint8_t)(LOAD_ADDR >> 8);
    object[3] = (uint8_t)LOAD_ADDR;
    object[4] = (uint8_t)(object_len >> 8);
    object[5] = (uint8_t)object_len;
    object[6] = 'B'; object[7] = 'S'; object[8] = '9'; object[9] = '3';
    memcpy(object + 10u, program, payload_len);
    if (lynx_load_bll(in->m, object, object_len) != LYNX_OK ||
        lynx_set_input(in->m, input_mask) != LYNX_OK) {
        lynx_machine_deinit(in->m);
        free(in->mem);
        memset(in, 0, sizeof *in);
        return 1;
    }
    return 0;
}

static void frame_buffers_init(lynx_frame_buffers *buffers, video_packet *packet)
{
    memset(buffers, 0, sizeof *buffers);
    buffers->struct_size = (uint32_t)sizeof *buffers;
    buffers->pens = packet->pens;
    buffers->pens_capacity = sizeof packet->pens;
    buffers->palettes = packet->palettes;
    buffers->palettes_capacity = sizeof packet->palettes;
    buffers->line_ticks = packet->line_ticks;
    buffers->line_ticks_capacity = sizeof packet->line_ticks / sizeof packet->line_ticks[0];
}

static void video_first_sync(instance *in, uint64_t *tick)
{
    video_packet packet = {0};
    lynx_frame_buffers buffers;
    frame_buffers_init(&buffers, &packet);
    packet.result.struct_size = (uint32_t)sizeof packet.result;
    CHECK_EQ_U64(lynx_advance_frame(in->m, UINT64_C(1000000), &buffers,
                                   &packet.result), LYNX_OK);
    if (packet.result.stop != LYNX_STOP_FRAME) {
        fprintf(stderr, "video_first_sync: stop=%u tick=%llu request=$%04x kind=%u\n",
                packet.result.stop, (unsigned long long)in->m->guest_ticks,
                in->m->cpu.req_addr, in->m->cpu.req_kind);
    }
    CHECK_EQ_U64(packet.result.stop, LYNX_STOP_FRAME);
    CHECK_EQ_U64(packet.result.sequence, 0u);
    CHECK_EQ_U64(packet.result.row_count, 0u);
    CHECK_EQ_U64(lynx_guest_ticks(in->m, tick), LYNX_OK);
    CHECK_EQ_U64(*tick, packet.result.frame_end_tick);
}

static void video_advance_exact(instance *in, uint64_t ticks, int sliced)
{
    static const uint64_t pieces[] = {1u, 17u, 257u, 5u, 61u, 509u, 3u};
    uint64_t remaining = ticks;
    size_t piece = 0u;
    while (remaining != 0u) {
        uint64_t budget = sliced ? pieces[piece++ % (sizeof pieces / sizeof pieces[0])] : remaining;
        if (budget > remaining) budget = remaining;
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        CHECK_EQ_U64(lynx_advance(in->m, budget, &consumed, &stop), LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, budget);
        if (consumed == 0u || consumed > remaining) return;
        remaining -= consumed;
    }
}

static void video_frame_step(instance *in, video_packet *packet, uint64_t budget,
                             int watched)
{
    lynx_frame_buffers buffers;
    frame_buffers_init(&buffers, packet);
    memset(&packet->result, 0, sizeof packet->result);
    packet->result.struct_size = (uint32_t)sizeof packet->result;
    CHECK_EQ_U64(lynx_advance_frame(in->m, budget, &buffers, &packet->result), LYNX_OK);
    packet->total_ticks += packet->result.ticks_consumed;
    if (packet->result.stop == LYNX_STOP_WATCH) {
        lynx_watch_event event;
        CHECK(watched);
        CHECK_EQ_U64(lynx_watch_get(in->m, &event), LYNX_OK);
        CHECK_EQ_U64(event.hit, 1u);
        CHECK_EQ_U64(event.address, 0x0400u);
        packet->watch_hits++;
        if (packet->watch_hits < 4u) {
            CHECK_EQ_U64(lynx_watch_configure(in->m, 0x0400u, 1u), LYNX_OK);
        } else {
            CHECK_EQ_U64(lynx_watch_clear(in->m), LYNX_OK);
        }
    } else if (packet->result.stop == LYNX_STOP_FRAME && packet->result.row_count == 0u) {
        packet->empty_frames++;
    } else if (packet->result.stop == LYNX_STOP_FRAME) {
        packet->complete = 1;
    } else {
        CHECK(packet->result.stop == LYNX_STOP_BUDGET);
    }
}

static void video_collect_frame(instance *in, video_packet *packet, int sliced, int watched)
{
    static const uint64_t pieces[] = {1u, 13u, 255u, 7u, 509u, 31u, 1021u};
    memset(packet, 0, sizeof *packet);
    for (size_t i = 0u; i < 10000u && !packet->complete; i++) {
        const uint64_t budget = watched ? UINT64_C(1000000) :
            (sliced ? pieces[i % (sizeof pieces / sizeof pieces[0])] : UINT64_C(1000000));
        video_frame_step(in, packet, budget, watched);
    }
    CHECK(packet->complete);
    if (watched) CHECK(packet->watch_hits >= 4u);
}

static int video_packet_equal(const video_packet *a, const video_packet *b)
{
    return a->result.stop == LYNX_STOP_FRAME && b->result.stop == LYNX_STOP_FRAME &&
           a->result.frame_start_tick == b->result.frame_start_tick &&
           a->result.frame_end_tick == b->result.frame_end_tick &&
           a->result.sequence == b->result.sequence &&
           a->result.row_count == b->result.row_count && a->result.flags == b->result.flags &&
           memcmp(a->pens, b->pens, sizeof a->pens) == 0 &&
           memcmp(a->palettes, b->palettes, sizeof a->palettes) == 0 &&
           memcmp(a->line_ticks, b->line_ticks, sizeof a->line_ticks) == 0;
}

static void check_video_common_state(const char *name, const lynx_machine *a,
                                     const lynx_machine *b)
{
    uint64_t ticks_a = 0u;
    uint64_t ticks_b = 0u;
    uint8_t sample_a = 0u;
    uint8_t sample_b = 0u;
    uint8_t digest_a[32];
    uint8_t digest_b[32];
    CHECK_EQ_U64(lynx_guest_ticks(a, &ticks_a), LYNX_OK);
    CHECK_EQ_U64(lynx_guest_ticks(b, &ticks_b), LYNX_OK);
    CHECK_NAMED(ticks_a == ticks_b, name);
    CHECK_EQ_U64(lynx_read_memory(a, 0x0400u, &sample_a, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_read_memory(b, 0x0400u, &sample_b, 1u), LYNX_OK);
    CHECK_NAMED(sample_a == sample_b, name);
    CHECK_EQ_U64(lynx_state_digest(a, digest_a), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(b, digest_b), LYNX_OK);
    CHECK_NAMED(memcmp(digest_a, digest_b, sizeof digest_a) == 0, name);
}

static void case_video_paths(char **args)
{
    (void)args;
    enum { PLAIN, SLICED, WATCHED, BLANK_SOURCE, ROW_SOURCE, PATH_COUNT };
    instance paths[PATH_COUNT] = {{0}};
    instance blank_restore = {0};
    instance row_restore = {0};
    video_packet packets[PATH_COUNT] = {{0}};
    video_packet blank_packet = {0};
    video_packet row_packet = {0};
    uint64_t sync_ticks[PATH_COUNT] = {0};
    const uint64_t line_ticks = 512u;
    uint8_t *snapshot = NULL;
    size_t snapshot_size = 0u;

    for (size_t i = 0u; i < PATH_COUNT; i++) {
        CHECK_EQ_U64(video_instance_make(&paths[i], 0u), 0u);
        if (paths[i].m == NULL) goto done;
        video_first_sync(&paths[i], &sync_ticks[i]);
        if (i != 0u) CHECK_NAMED(sync_ticks[i] == sync_ticks[PLAIN], "video_paths.common-first-sync");
        CHECK_EQ_U64(lynx_set_input(paths[i].m, LYNX_INPUT_A), LYNX_OK);
    }
    CHECK_EQ_U64(video_instance_make(&blank_restore, 0u), 0u);
    CHECK_EQ_U64(video_instance_make(&row_restore, 0u), 0u);
    if (blank_restore.m == NULL || row_restore.m == NULL) goto done;
    CHECK_EQ_U64(lynx_watch_configure(paths[WATCHED].m, 0x0400u, 1u), LYNX_OK);

    video_collect_frame(&paths[PLAIN], &packets[PLAIN], 0, 0);
    video_collect_frame(&paths[SLICED], &packets[SLICED], 1, 0);
    video_collect_frame(&paths[WATCHED], &packets[WATCHED], 0, 1);

    /* Save after two blank lines and halfway through the first visible row. */
    video_advance_exact(&paths[BLANK_SOURCE], 2u * line_ticks + 257u, 0);
    CHECK_EQ_U64(lynx_snapshot_size(paths[BLANK_SOURCE].m, &snapshot_size), LYNX_OK);
    snapshot = malloc(snapshot_size);
    CHECK(snapshot != NULL);
    if (snapshot != NULL) {
        size_t written = 0u;
        CHECK_EQ_U64(lynx_snapshot_save(paths[BLANK_SOURCE].m, snapshot,
                                        snapshot_size, &written), LYNX_OK);
        CHECK_EQ_U64(written, snapshot_size);
        CHECK_EQ_U64(lynx_snapshot_restore(blank_restore.m, snapshot, snapshot_size), LYNX_OK);
        check_video_common_state("video_paths.mid-blank-restore-state",
                                 paths[BLANK_SOURCE].m, blank_restore.m);
    }
    free(snapshot);
    snapshot = NULL;

    video_advance_exact(&paths[ROW_SOURCE], 3u * line_ticks + 257u, 1);
    CHECK_EQ_U64(lynx_snapshot_size(paths[ROW_SOURCE].m, &snapshot_size), LYNX_OK);
    snapshot = malloc(snapshot_size);
    CHECK(snapshot != NULL);
    if (snapshot != NULL) {
        size_t written = 0u;
        CHECK_EQ_U64(lynx_snapshot_save(paths[ROW_SOURCE].m, snapshot,
                                        snapshot_size, &written), LYNX_OK);
        CHECK_EQ_U64(written, snapshot_size);
        CHECK_EQ_U64(lynx_snapshot_restore(row_restore.m, snapshot, snapshot_size), LYNX_OK);
        check_video_common_state("video_paths.mid-row-restore-state",
                                 paths[ROW_SOURCE].m, row_restore.m);
    }
    free(snapshot);
    snapshot = NULL;

    video_collect_frame(&paths[BLANK_SOURCE], &packets[BLANK_SOURCE], 0, 0);
    video_collect_frame(&blank_restore, &blank_packet, 1, 0);
    video_collect_frame(&paths[ROW_SOURCE], &packets[ROW_SOURCE], 0, 0);
    video_collect_frame(&row_restore, &row_packet, 1, 0);

    for (size_t i = 1u; i < PATH_COUNT; i++) {
        CHECK_NAMED(video_packet_equal(&packets[PLAIN], &packets[i]),
                    "video_paths.complete-packet-equality");
        check_video_common_state("video_paths.guest-progress-input-and-digest",
                                 paths[PLAIN].m, paths[i].m);
    }
    CHECK_NAMED(video_packet_equal(&packets[PLAIN], &blank_packet),
                "video_paths.mid-blank-restored-packet");
    CHECK_NAMED(video_packet_equal(&packets[PLAIN], &row_packet),
                "video_paths.mid-row-restored-packet");
    check_video_common_state("video_paths.mid-blank-restored-final-state",
                             paths[PLAIN].m, blank_restore.m);
    check_video_common_state("video_paths.mid-row-restored-final-state",
                             paths[PLAIN].m, row_restore.m);

    /* Interleave distinct input streams and frame buffers. The guest writes
       the sampled joystick byte to the display's first byte and to RAM. */
    instance input_a = {0}, input_none = {0};
    instance ref_a = {0}, ref_none = {0};
    video_packet a_interleaved = {0}, none_interleaved = {0};
    video_packet a_reference = {0}, none_reference = {0};
    CHECK_EQ_U64(video_instance_make(&input_a, LYNX_INPUT_A), 0u);
    CHECK_EQ_U64(video_instance_make(&input_none, 0u), 0u);
    CHECK_EQ_U64(video_instance_make(&ref_a, LYNX_INPUT_A), 0u);
    CHECK_EQ_U64(video_instance_make(&ref_none, 0u), 0u);
    if (input_a.m != NULL && input_none.m != NULL && ref_a.m != NULL && ref_none.m != NULL) {
        uint64_t unused = 0u;
        video_first_sync(&input_a, &unused);
        video_first_sync(&input_none, &unused);
        video_first_sync(&ref_a, &unused);
        video_first_sync(&ref_none, &unused);
        video_collect_frame(&ref_a, &a_reference, 0, 0);
        video_collect_frame(&ref_none, &none_reference, 0, 0);
        memset(&a_interleaved, 0, sizeof a_interleaved);
        memset(&none_interleaved, 0, sizeof none_interleaved);
        for (size_t i = 0u; i < 10000u &&
             (!a_interleaved.complete || !none_interleaved.complete); i++) {
            if (!a_interleaved.complete)
                video_frame_step(&input_a, &a_interleaved, 257u, 0);
            if (!none_interleaved.complete)
                video_frame_step(&input_none, &none_interleaved, 61u, 0);
        }
        CHECK(a_interleaved.complete && none_interleaved.complete);
        CHECK_NAMED(video_packet_equal(&a_reference, &a_interleaved),
                    "video_paths.interleaved-action-frame-buffer");
        CHECK_NAMED(video_packet_equal(&none_reference, &none_interleaved),
                    "video_paths.interleaved-empty-frame-buffer");
        CHECK_NAMED(!video_packet_equal(&a_reference, &none_reference),
                    "video_paths.input-responsive-raw-pens");
        uint8_t sample_a = 0u, sample_none = 0u;
        CHECK_EQ_U64(lynx_read_memory(input_a.m, 0x0400u, &sample_a, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_read_memory(input_none.m, 0x0400u, &sample_none, 1u), LYNX_OK);
        CHECK_NAMED(sample_a != sample_none, "video_paths.separate-input-samples");
        check_video_common_state("video_paths.interleaved-action-final-state",
                                 ref_a.m, input_a.m);
        check_video_common_state("video_paths.interleaved-empty-final-state",
                                 ref_none.m, input_none.m);
    }
    if (input_a.m != NULL) lynx_machine_deinit(input_a.m);
    if (input_none.m != NULL) lynx_machine_deinit(input_none.m);
    if (ref_a.m != NULL) lynx_machine_deinit(ref_a.m);
    if (ref_none.m != NULL) lynx_machine_deinit(ref_none.m);
    free(input_a.mem); free(input_none.mem); free(ref_a.mem); free(ref_none.mem);

    printf("video_paths: sync=%llu rows=%u paths=unsliced,sliced,watched,mid-blank,mid-row interleaved-inputs=2\n",
           (unsigned long long)sync_ticks[PLAIN], packets[PLAIN].result.row_count);
done:
    free(snapshot);
    for (size_t i = 0u; i < PATH_COUNT; i++) {
        if (paths[i].m != NULL) lynx_machine_deinit(paths[i].m);
        free(paths[i].mem);
    }
    if (blank_restore.m != NULL) lynx_machine_deinit(blank_restore.m);
    if (row_restore.m != NULL) lynx_machine_deinit(row_restore.m);
    free(blank_restore.mem);
    free(row_restore.mem);
}

static void case_video_sensitivity(char **args)
{
    enum { INPUT, PALETTE, PEN, DMA_OWNER, PAGE_LATCH, HOST_WATCH, FLIP_COUNT };
    static const char *const names[FLIP_COUNT] = {
        "input mask", "palette component", "partial pen byte", "DMA owner deadline",
        "CPU page latch", "host watch"
    };
    (void)args;
    instance base = {0};
    CHECK_EQ_U64(video_instance_make(&base, 0u), 0u);
    if (base.m == NULL) {
        free(base.mem);
        return;
    }
    uint64_t sync_tick = 0u;
    video_first_sync(&base, &sync_tick);
    video_advance_exact(&base, 3u * 512u + 6u, 0);
    CHECK_NAMED(base.m->video.row_active == 1u && base.m->video.dma_cursor == 1u &&
                    base.m->video.dma_busy_until > base.m->guest_ticks,
                "video_sensitivity.fixture-is-partial-row-with-active-DMA");

    uint8_t expected[32];
    CHECK_EQ_U64(lynx_state_digest(base.m, expected), LYNX_OK);
    uint8_t flips[FLIP_COUNT][32];
    for (size_t i = 0u; i < FLIP_COUNT; i++) {
        lynx_machine *copy = malloc(lynx_machine_size());
        CHECK(copy != NULL);
        if (copy == NULL) continue;
        memcpy(copy, base.m, sizeof *copy);
        switch (i) {
        case INPUT:
            CHECK_EQ_U64(lynx_set_input(copy, LYNX_INPUT_A), LYNX_OK);
            break;
        case PALETTE:
            copy->video.palette[3] ^= 1u;
            break;
        case PEN:
            copy->video.pens[0] ^= 1u;
            break;
        case DMA_OWNER:
            copy->video.dma_busy_until++;
            break;
        case PAGE_LATCH:
            copy->cpu_page_latch = 1u;
            break;
        case HOST_WATCH:
            CHECK_EQ_U64(lynx_watch_configure(copy, 0x0400u, 1u), LYNX_OK);
            break;
        }
        CHECK_EQ_U64(lynx_state_digest(copy, flips[i]), LYNX_OK);
        if (i == HOST_WATCH) {
            CHECK_NAMED(memcmp(expected, flips[i], sizeof expected) == 0,
                        "video_sensitivity.host-watch-excluded");
        } else {
            CHECK_NAMED(memcmp(expected, flips[i], sizeof expected) != 0, names[i]);
        }
        if (i != HOST_WATCH) {
            for (size_t j = 0u; j < i; j++) {
                if (j != HOST_WATCH)
                    CHECK_NAMED(memcmp(flips[i], flips[j], sizeof flips[i]) != 0,
                                "video_sensitivity.independent-stream-flips");
            }
        }
        free(copy);
    }

    lynx_machine_deinit(base.m);
    free(base.mem);
    printf("video_sensitivity: input,palette,partial-pen,DMA-owner,page-latch sensitive; host-watch excluded\n");
}

static const struct {
    const char *name;
    int args; /* arguments after the case name */
    void (*run)(char **args);
} cases[] = {
    {"profile_agreement", 2, case_profile_agreement},
    {"state_sensitivity", 1, case_state_sensitivity},
    {"timer_paths", 0, case_timer_paths},
    {"video_paths", 0, case_video_paths},
    {"video_sensitivity", 0, case_video_sensitivity},
    {"two_instances", 3, case_two_instances},
};

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: lynx-test-det <case> <args>\n");
        return 2;
    }
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (strcmp(argv[1], cases[i].name) != 0) {
            continue;
        }
        if (argc != 2 + cases[i].args) {
            fprintf(stderr, "lynx-test-det: case %s takes %d arguments\n", cases[i].name,
                    cases[i].args);
            return 2;
        }
        cases[i].run(argv + 2);
        CHECK_DONE();
    }
    fprintf(stderr, "lynx-test-det: unknown case %s\n", argv[1]);
    return 2;
}
