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

static const char *const bundle_files[] = {"obs.ram.hex", "obs.regs.hex", "obs.time.hex",
                                           "run.manifest", "digests.txt"};

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

static const struct {
    const char *name;
    int args; /* arguments after the case name */
    void (*run)(char **args);
} cases[] = {
    {"profile_agreement", 2, case_profile_agreement},
    {"state_sensitivity", 1, case_state_sensitivity},
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
