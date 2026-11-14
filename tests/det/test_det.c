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
#include "sha256.h"

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

static void digest_hex(const uint8_t digest[32], char hex[65]);

/* One machine in a caller buffer, built the way the runner builds it. */
typedef struct instance {
    void *mem;
    lynx_machine *m;
    uint64_t consumed;
    int all_budget_stops;
} instance;

static void sha256_hex(const uint8_t *bytes, size_t length, char out[65])
{
    lynx__sha256 hash;
    uint8_t digest[32];
    lynx__sha256_init(&hash);
    lynx__sha256_update(&hash, bytes, length);
    lynx__sha256_final(&hash, digest);
    digest_hex(digest, out);
}

static int read_manifest_value(const char *path, const char *key,
                               char *out, size_t capacity)
{
    char line[256];
    const size_t key_length = strlen(key);
    unsigned found = 0u;
    FILE *file = fopen(path, "rb");
    if (file == NULL || capacity == 0u) {
        if (file != NULL) fclose(file);
        return 1;
    }
    while (fgets(line, (int)sizeof line, file) != NULL) {
        size_t length = strlen(line);
        while (length > 0u && (line[length - 1u] == '\n' || line[length - 1u] == '\r')) {
            line[--length] = '\0';
        }
        if (length >= key_length + 3u && memcmp(line, key, key_length) == 0 &&
            line[key_length] == ' ' && line[key_length + 1u] == '=' &&
            line[key_length + 2u] == ' ') {
            const char *value = line + key_length + 3u;
            const size_t value_length = strlen(value);
            if (found != 0u || value_length == 0u || value_length >= capacity) {
                fclose(file);
                return 1;
            }
            memcpy(out, value, value_length + 1u);
            found++;
        }
    }
    int failed = ferror(file);
    if (fclose(file) != 0) failed = 1;
    return failed || found != 1u ? 1 : 0;
}

static int parse_hex_address_token(const char *text, size_t length, uint32_t *out)
{
    uint32_t value = 0u;
    if (length == 0u || length > 8u) return 1;
    for (size_t i = 0u; i < length; i++) {
        const unsigned char ch = (unsigned char)text[i];
        uint32_t digit;
        if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
            digit = (uint32_t)(ch - (unsigned char)'0');
        } else if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') {
            digit = (uint32_t)(ch - (unsigned char)'a') + 10u;
        } else if (ch >= (unsigned char)'A' && ch <= (unsigned char)'F') {
            digit = (uint32_t)(ch - (unsigned char)'A') + 10u;
        } else {
            return 1;
        }
        value = (value << 4u) | digit;
    }
    *out = value;
    return 0;
}

static int read_linked_symbol(const char *path, const char *name, uint32_t *address)
{
    char line[256];
    unsigned found = 0u;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 1;
    while (fgets(line, (int)sizeof line, file) != NULL) {
        char kind[16], hex[16], symbol[128], extra;
        uint32_t candidate = 0u;
        if (sscanf(line, "%15s %15s %127s %c", kind, hex, symbol, &extra) != 3 ||
            strcmp(kind, "al") != 0) continue;
        const char *symbol_name = symbol[0] == '.' ? symbol + 1u : symbol;
        if (strcmp(symbol_name, name) != 0) continue;
        if (parse_hex_address_token(hex, strlen(hex), &candidate) != 0 || found != 0u) {
            fclose(file);
            return 1;
        }
        *address = candidate;
        found++;
    }
    int failed = ferror(file);
    if (fclose(file) != 0) failed = 1;
    return failed || found != 1u ? 1 : 0;
}

typedef struct av_fixture {
    uint8_t *bll;
    size_t bll_length;
    uint32_t probe_result;
} av_fixture;

static int av_fixture_load(const char *bll_path, const char *labels_path,
                           const char *manifest_path, av_fixture *fixture)
{
    char expected_hash[80];
    char expected_size[32];
    char actual_hash[65];
    char *end = NULL;
    unsigned long expected_length;
    memset(fixture, 0, sizeof *fixture);
    if (read_file(bll_path, &fixture->bll, &fixture->bll_length) != 0 ||
        read_manifest_value(manifest_path, "av_guest_sha256", expected_hash,
                            sizeof expected_hash) != 0 ||
        read_manifest_value(manifest_path, "av_guest_bytes", expected_size,
                            sizeof expected_size) != 0 ||
        read_linked_symbol(labels_path, "probe_result", &fixture->probe_result) != 0) {
        free(fixture->bll);
        memset(fixture, 0, sizeof *fixture);
        return 1;
    }
    expected_length = strtoul(expected_size, &end, 10);
    sha256_hex(fixture->bll, fixture->bll_length, actual_hash);
    if (end == expected_size || *end != '\0' || expected_length != fixture->bll_length ||
        strcmp(actual_hash, expected_hash) != 0 || fixture->bll_length != 220u ||
        fixture->probe_result > LYNX_ADDRESS_SPACE - 9u) {
        free(fixture->bll);
        memset(fixture, 0, sizeof *fixture);
        return 1;
    }
    return 0;
}

static void av_fixture_free(av_fixture *fixture)
{
    free(fixture->bll);
    memset(fixture, 0, sizeof *fixture);
}

static int av_instance_make(instance *in, const av_fixture *fixture)
{
    lynx_config config;
    const size_t size = lynx_machine_size();
    const size_t alignment = lynx_machine_align();
    uintptr_t base;
    size_t offset;
    void *aligned;
    lynx_identity identity;
    memset(in, 0, sizeof *in);
    in->all_budget_stops = 1;
    if (size == 0u || alignment == 0u || size > SIZE_MAX - (alignment - 1u)) return 1;
    in->mem = malloc(size + alignment - 1u);
    if (in->mem == NULL) return 1;
    base = (uintptr_t)in->mem;
    offset = (size_t)((alignment - (base % alignment)) % alignment);
    aligned = (uint8_t *)in->mem + offset;
    lynx_config_default(&config);
    config.profile = LYNX_PROFILE_NONE;
    config.ram_policy = LYNX_RAM_POLICY_ZERO;
    config.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    if (lynx_machine_init(aligned, size, &config, &in->m) != LYNX_OK ||
        lynx_load_bll(in->m, fixture->bll, fixture->bll_length) != LYNX_OK ||
        lynx_get_identity(in->m, &identity) != LYNX_OK || identity.profile != LYNX_PROFILE_NONE ||
        identity.ram_policy != LYNX_RAM_POLICY_ZERO ||
        identity.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        identity.audio_policy != LYNX_AUDIO_POLICY_DIGITAL_1 ||
        identity.audio_rate_num != 40000u || identity.audio_rate_den != 1u ||
        identity.audio_channels != 2u || identity.audio_bits != 16u) {
        if (in->m != NULL) lynx_machine_deinit(in->m);
        free(in->mem);
        memset(in, 0, sizeof *in);
        return 1;
    }
    return 0;
}

static void instance_destroy(instance *in)
{
    if (in->m != NULL) lynx_machine_deinit(in->m);
    free(in->mem);
    memset(in, 0, sizeof *in);
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

static int suzy_state_digest(const lynx_machine *machine, char output[65])
{
    uint8_t digest[32];
    if (lynx_state_digest(machine, digest) != LYNX_OK) return 1;
    digest_hex(digest, output);
    return 0;
}

static void prepare_suzy_multiply(lynx_machine *machine)
{
    const uint64_t tick = machine->guest_ticks;
    CHECK(lynx__suzy_write(&machine->suzy, 0xFC52u, 0xFFu, tick));
    CHECK(lynx__suzy_write(&machine->suzy, 0xFC53u, 0x00u, tick));
    CHECK(lynx__suzy_write(&machine->suzy, 0xFC54u, 0x02u, tick));
    CHECK(lynx__suzy_write(&machine->suzy, 0xFC55u, 0x00u, tick));
    CHECK(lynx__suzy_valid(&machine->suzy, tick));
}

static void flip_suzy_sign_ab(lynx__suzy_state *suzy) { suzy->sign_ab ^= 1u; }
static void flip_suzy_sign_cd(lynx__suzy_state *suzy) { suzy->sign_cd ^= 1u; }
static void flip_suzy_mode(lynx__suzy_state *suzy) { suzy->mode = 0x80u; }
static void flip_suzy_warning(lynx__suzy_state *suzy) { suzy->warning ^= 1u; }
static void flip_suzy_carry(lynx__suzy_state *suzy) { suzy->carry ^= 1u; }
static void flip_suzy_unsafe(lynx__suzy_state *suzy) { suzy->unsafe ^= 1u; }
static void flip_suzy_latched_modes(lynx__suzy_state *suzy) { suzy->latched_modes ^= 0x80u; }
static void flip_suzy_latched_ab(lynx__suzy_state *suzy) { suzy->latched_ab ^= 1u; }
static void flip_suzy_latched_cd(lynx__suzy_state *suzy) { suzy->latched_cd ^= 1u; }
static void flip_suzy_latched_np(lynx__suzy_state *suzy) { suzy->latched_np ^= 1u; }
static void flip_suzy_latched_dividend(lynx__suzy_state *suzy) { suzy->latched_dividend ^= 1u; }
static void flip_suzy_latched_accumulator(lynx__suzy_state *suzy)
{
    suzy->latched_accumulator ^= 1u;
}
static void flip_suzy_latched_sign_ab(lynx__suzy_state *suzy)
{
    suzy->latched_sign_ab ^= 1u;
}
static void flip_suzy_latched_sign_cd(lynx__suzy_state *suzy)
{
    suzy->latched_sign_cd ^= 1u;
}
static void flip_suzy_operation(lynx__suzy_state *suzy) { suzy->operation = 2u; }
static void flip_suzy_start_tick(lynx__suzy_state *suzy) { suzy->start_tick ^= 1u; }
static void flip_suzy_deadline_tick(lynx__suzy_state *suzy) { suzy->deadline_tick ^= 1u; }

static void case_suzy_sensitivity(char **args)
{
    static const uint8_t loop[] = {0x4Cu, 0x00u, 0x02u}; /* JMP $0200 */
    (void)args;
    instance base;
    char base_digest[65];
    char changed_digest[65];
    CHECK_EQ_U64(instance_make(&base, loop, sizeof loop), 0u);
    if (base.m == NULL) {
        free(base.mem);
        return;
    }
    uint64_t used = 0u;
    lynx_stop_reason stop = LYNX_STOP_HALT;
    CHECK_EQ_U64(lynx_advance(base.m, BUDGET, &used, &stop), LYNX_OK);
    CHECK_EQ_U64(used, BUDGET);
    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(suzy_state_digest(base.m, base_digest), 0u);

    /* Every byte in the backing window participates in the canonical state. */
    for (size_t index = 0u; index < sizeof base.m->suzy.registers; index++) {
        lynx_machine *copy = copy_machine(base.m);
        CHECK(copy != NULL);
        if (copy == NULL) continue;
        copy->suzy.registers[index] ^= 1u;
        CHECK(lynx__suzy_valid(&copy->suzy, copy->guest_ticks));
        CHECK_EQ_U64(suzy_state_digest(copy, changed_digest), 0u);
        if (strcmp(changed_digest, base_digest) == 0) {
            fprintf(stderr, "suzy_sensitivity: backing byte %zu is absent from digest\n", index);
            check_failures++;
        }
        free(copy);
    }

    static const struct {
        const char *name;
        void (*flip)(lynx__suzy_state *);
    } idle_flips[] = {
        {"sign_ab", flip_suzy_sign_ab},
        {"sign_cd", flip_suzy_sign_cd},
        {"mode", flip_suzy_mode},
        {"warning", flip_suzy_warning},
        {"carry", flip_suzy_carry},
        {"unsafe", flip_suzy_unsafe}
    };
    for (size_t index = 0u; index < sizeof idle_flips / sizeof idle_flips[0]; index++) {
        lynx_machine *copy = copy_machine(base.m);
        CHECK(copy != NULL);
        if (copy == NULL) continue;
        idle_flips[index].flip(&copy->suzy);
        CHECK(lynx__suzy_valid(&copy->suzy, copy->guest_ticks));
        CHECK_EQ_U64(suzy_state_digest(copy, changed_digest), 0u);
        if (strcmp(changed_digest, base_digest) == 0) {
            fprintf(stderr, "suzy_sensitivity: %s is absent from digest\n",
                    idle_flips[index].name);
            check_failures++;
        }
        free(copy);
    }

    lynx_machine *active = copy_machine(base.m);
    CHECK(active != NULL);
    if (active != NULL) {
        prepare_suzy_multiply(active);
        char active_digest[65];
        CHECK_EQ_U64(suzy_state_digest(active, active_digest), 0u);
        static const struct {
            const char *name;
            void (*flip)(lynx__suzy_state *);
        } progress_flips[] = {
            {"latched modes", flip_suzy_latched_modes},
            {"latched AB", flip_suzy_latched_ab},
            {"latched CD", flip_suzy_latched_cd},
            {"latched NP", flip_suzy_latched_np},
            {"latched dividend", flip_suzy_latched_dividend},
            {"latched accumulator", flip_suzy_latched_accumulator},
            {"latched sign AB", flip_suzy_latched_sign_ab},
            {"latched sign CD", flip_suzy_latched_sign_cd},
            {"operation kind", flip_suzy_operation},
            {"start tick", flip_suzy_start_tick},
            {"deadline tick", flip_suzy_deadline_tick}
        };
        for (size_t index = 0u;
             index < sizeof progress_flips / sizeof progress_flips[0]; index++) {
            lynx_machine *copy = copy_machine(active);
            CHECK(copy != NULL);
            if (copy == NULL) continue;
            progress_flips[index].flip(&copy->suzy);
            CHECK_EQ_U64(suzy_state_digest(copy, changed_digest), 0u);
            if (strcmp(changed_digest, active_digest) == 0) {
                fprintf(stderr, "suzy_sensitivity: %s is absent from digest\n",
                        progress_flips[index].name);
                check_failures++;
            }
            free(copy);
        }
        free(active);
    }

    /* The host-only write watch is outside guest state and must not perturb it. */
    CHECK_EQ_U64(lynx_watch_configure(base.m, 0x0400u, 1u), LYNX_OK);
    CHECK_EQ_U64(suzy_state_digest(base.m, changed_digest), 0u);
    CHECK(strcmp(changed_digest, base_digest) == 0);
    printf("suzy_sensitivity: all 48 backing bytes, signs, mode, status, active progress, and host-watch exclusion passed (%s)\n",
           base_digest);
    free(base.mem);
}

static void suzy_program_byte(uint8_t *program, size_t *length, uint8_t value)
{
    program[(*length)++] = value;
}

static void suzy_program_write(uint8_t *program, size_t *length,
                               uint16_t address, uint8_t value)
{
    suzy_program_byte(program, length, 0xA9u); /* LDA #value */
    suzy_program_byte(program, length, value);
    suzy_program_byte(program, length, 0x8Du); /* STA address */
    suzy_program_byte(program, length, (uint8_t)address);
    suzy_program_byte(program, length, (uint8_t)(address >> 8));
}

static size_t make_suzy_path_program(uint8_t *program, size_t capacity,
                                     unsigned operation, uint16_t *command_address,
                                     uint64_t *deadline_delta,
                                     uint8_t *latched_modes, uint32_t *expected_result)
{
    (void)capacity;
    size_t length = 0u;
    const uint8_t signed_mode = operation == 1u ? 0x80u :
                                operation == 2u ? 0x40u : 0x00u;
    if (operation <= 2u) {
        const uint8_t ab_low = operation == 1u ? 0x05u :
                               operation == 2u ? 0x10u : 0x03u;
        const uint8_t ab_high = 0x00u;
        const uint8_t cd_low = operation == 1u ? 0xFDu :
                               operation == 2u ? 0x10u : 0x02u;
        const uint8_t cd_high = operation == 1u ? 0xFFu : 0x00u;
        *command_address = 0xFC55u;
        *deadline_delta = operation == 0u ? 44u : 54u;
        *latched_modes = signed_mode;
        *expected_result = operation == 1u ? UINT32_C(0xFFFFFFF1) :
                           operation == 2u ? UINT32_C(0x00000100) : 6u;
        if (signed_mode != 0u) suzy_program_write(program, &length, 0xFC92u, signed_mode);
        suzy_program_write(program, &length, 0xFC52u, cd_low);
        suzy_program_write(program, &length, 0xFC53u, cd_high);
        suzy_program_write(program, &length, 0xFC54u, ab_low);
        suzy_program_write(program, &length, 0xFC55u, ab_high);
    } else {
        const uint16_t divisor = 10u;
        *command_address = 0xFC63u;
        *deadline_delta = 176u + 14u * 12u;
        *latched_modes = 0u;
        *expected_result = UINT32_C(0x00001999);
        suzy_program_write(program, &length, 0xFC56u, (uint8_t)divisor);
        suzy_program_write(program, &length, 0xFC57u, (uint8_t)(divisor >> 8));
        suzy_program_write(program, &length, 0xFC60u, 0x00u);
        suzy_program_write(program, &length, 0xFC61u, 0x00u);
        suzy_program_write(program, &length, 0xFC62u, 0x01u);
        suzy_program_write(program, &length, 0xFC63u, 0x00u);
    }
    suzy_program_write(program, &length, 0xFC92u, 0x4Cu);
    const uint16_t loop = (uint16_t)(LOAD_ADDR + (uint16_t)length);
    suzy_program_byte(program, &length, 0x4Cu);
    suzy_program_byte(program, &length, (uint8_t)loop);
    suzy_program_byte(program, &length, (uint8_t)(loop >> 8));
    if (length > capacity) return 0u;
    return length;
}

static void suzy_advance_path(lynx_machine *machine, uint64_t target,
                              const uint64_t *slices, size_t slice_count)
{
    size_t index = 0u;
    while (machine->guest_ticks < target) {
        const uint64_t remaining = target - machine->guest_ticks;
        uint64_t budget = slices == NULL ? remaining : slices[index++ % slice_count];
        if (budget > remaining) budget = remaining;
        uint64_t consumed = UINT64_MAX;
        lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
        const lynx_status status = lynx_advance(machine, budget, &consumed, &stop);
        CHECK_EQ_U64(status, LYNX_OK);
        CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
        CHECK_EQ_U64(consumed, budget);
        if (status != LYNX_OK || consumed != budget || budget == 0u) return;
    }
}

static int suzy_fields_equal(const lynx__suzy_state *a, const lynx__suzy_state *b)
{
    return memcmp(a->registers, b->registers, sizeof a->registers) == 0 &&
           a->sign_ab == b->sign_ab && a->sign_cd == b->sign_cd && a->mode == b->mode &&
           a->warning == b->warning && a->carry == b->carry && a->unsafe == b->unsafe &&
           a->operation == b->operation && a->latched_modes == b->latched_modes &&
           a->latched_ab == b->latched_ab && a->latched_cd == b->latched_cd &&
           a->latched_np == b->latched_np && a->latched_dividend == b->latched_dividend &&
           a->latched_accumulator == b->latched_accumulator &&
           a->latched_sign_ab == b->latched_sign_ab &&
           a->latched_sign_cd == b->latched_sign_cd &&
           a->start_tick == b->start_tick && a->deadline_tick == b->deadline_tick;
}

static void case_suzy_paths(char **args)
{
    static const uint32_t profiles[] = {LYNX_PROFILE_LYNX1, LYNX_PROFILE_LYNX2};
    static const uint64_t irregular[] = {1u, 7u, 29u, 113u, 3u};
    static const uint64_t interleave_a[] = {31u, 1u, 127u, 13u};
    static const uint64_t interleave_b[] = {2u, 89u, 5u, 53u};
    enum { FINAL_TICK = 1024u };
    (void)args;
    for (unsigned operation = 0u; operation < 4u; operation++) {
        for (size_t p = 0u; p < sizeof profiles / sizeof profiles[0]; p++) {
            uint8_t program[128];
            uint16_t command_address = 0u;
            uint64_t deadline_delta = 0u;
            uint8_t expected_modes = 0u;
            uint32_t expected_result = 0u;
            const size_t program_length = make_suzy_path_program(
                program, sizeof program, operation, &command_address, &deadline_delta,
                &expected_modes, &expected_result);
            instance path[7] = {{0}};
            for (size_t i = 0u; i < 7u; i++) {
                CHECK_EQ_U64(instance_make_p(&path[i], profiles[p], program, program_length), 0u);
                if (path[i].m == NULL) goto operation_cleanup;
            }

            suzy_advance_path(path[0].m, FINAL_TICK, NULL, 0u);
            suzy_advance_path(path[1].m, FINAL_TICK, (const uint64_t[]){1u}, 1u);
            suzy_advance_path(path[2].m, FINAL_TICK, irregular,
                              sizeof irregular / sizeof irregular[0]);

            uint64_t tick_a = 0u;
            uint64_t tick_b = 0u;
            size_t slice_a = 0u;
            size_t slice_b = 0u;
            while (tick_a < FINAL_TICK || tick_b < FINAL_TICK) {
                const uint64_t budget_a = interleave_a[slice_a++ %
                    (sizeof interleave_a / sizeof interleave_a[0])];
                const uint64_t a = budget_a > FINAL_TICK - tick_a ?
                                   FINAL_TICK - tick_a : budget_a;
                uint64_t consumed = UINT64_MAX;
                lynx_stop_reason stop = LYNX_STOP_UNAVAILABLE;
                CHECK_EQ_U64(lynx_advance(path[5].m, a, &consumed, &stop), LYNX_OK);
                CHECK_EQ_U64(consumed, a);
                CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
                tick_a += consumed;
                if (tick_b < FINAL_TICK) {
                    const uint64_t budget_b = interleave_b[slice_b++ %
                        (sizeof interleave_b / sizeof interleave_b[0])];
                    const uint64_t b = budget_b > FINAL_TICK - tick_b ?
                                       FINAL_TICK - tick_b : budget_b;
                    consumed = UINT64_MAX;
                    stop = LYNX_STOP_UNAVAILABLE;
                    CHECK_EQ_U64(lynx_advance(path[6].m, b, &consumed, &stop), LYNX_OK);
                    CHECK_EQ_U64(consumed, b);
                    CHECK_EQ_U64(stop, LYNX_STOP_BUDGET);
                    tick_b += consumed;
                }
            }

            CHECK_EQ_U64(lynx_watch_configure(path[3].m, command_address, 1u), LYNX_OK);
            {
                uint64_t consumed = 0u;
                lynx_stop_reason stop = LYNX_STOP_BUDGET;
                CHECK_EQ_U64(lynx_advance(path[3].m, 400u, &consumed, &stop), LYNX_OK);
                CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
                CHECK_EQ_U64(lynx_watch_configure(path[3].m, 0xFC92u, 1u), LYNX_OK);
                CHECK_EQ_U64(lynx_advance(path[3].m, 200u, &consumed, &stop), LYNX_OK);
                CHECK_EQ_U64(stop, LYNX_STOP_WATCH);
                CHECK_EQ_U64(path[3].m->suzy.mode, 0x40u);
                CHECK_EQ_U64(path[3].m->suzy.latched_modes, expected_modes);
                CHECK_EQ_U64(path[3].m->video.lefthand, 1u);
                CHECK_EQ_U64(path[3].m->suzy.operation,
                             operation == 3u ? 2u : 1u);
            }
            size_t snapshot_size = 0u;
            size_t written = 0u;
            uint8_t *snapshot = NULL;
            CHECK_EQ_U64(lynx_snapshot_size(path[3].m, &snapshot_size), LYNX_OK);
            snapshot = malloc(snapshot_size);
            CHECK(snapshot != NULL);
            if (snapshot != NULL) {
                CHECK_EQ_U64(lynx_snapshot_save(path[3].m, snapshot, snapshot_size, &written),
                             LYNX_OK);
                CHECK_EQ_U64(written, snapshot_size);
                CHECK_EQ_U64(lynx_snapshot_restore(path[4].m, snapshot, snapshot_size), LYNX_OK);
                free(snapshot);
            }
            suzy_advance_path(path[3].m, FINAL_TICK, irregular,
                              sizeof irregular / sizeof irregular[0]);
            suzy_advance_path(path[4].m, FINAL_TICK, NULL, 0u);

            for (size_t candidate = 1u; candidate < 7u; candidate++) {
                char label[96];
                const int written_label = snprintf(label, sizeof label,
                    "suzy_paths operation=%u profile=%s path=%zu",
                    operation, lynx_profile_name(profiles[p]), candidate);
                CHECK(written_label > 0 && (size_t)written_label < sizeof label);
                require_same_public_guest(label, path[0].m, path[candidate].m);
                CHECK_NAMED(suzy_fields_equal(&path[0].m->suzy, &path[candidate].m->suzy),
                            "det.suzy_paths.fieldwise-suffix");
            }
            CHECK_EQ_U64(path[0].m->guest_ticks, FINAL_TICK);
            CHECK_EQ_U64(path[0].m->suzy.mode, 0x40u);
            CHECK_EQ_U64(path[0].m->video.lefthand, 1u);
            if (operation == 0u || operation == 1u || operation == 2u) {
                const uint32_t product = (uint32_t)path[0].m->suzy.registers[0x20u] |
                    ((uint32_t)path[0].m->suzy.registers[0x21u] << 8) |
                    ((uint32_t)path[0].m->suzy.registers[0x22u] << 16) |
                    ((uint32_t)path[0].m->suzy.registers[0x23u] << 24);
                CHECK_EQ_U64(product, expected_result);
            } else {
                const uint32_t quotient = (uint32_t)path[0].m->suzy.registers[0x12u] |
                    ((uint32_t)path[0].m->suzy.registers[0x13u] << 8) |
                    ((uint32_t)path[0].m->suzy.registers[0x14u] << 16) |
                    ((uint32_t)path[0].m->suzy.registers[0x15u] << 24);
                CHECK_EQ_U64(quotient, expected_result);
            }
            printf("det.suzy_paths: operation=%u profile=%s delta=%llu matched coarse/sliced/interleaved/restore\n",
                   operation, lynx_profile_name(profiles[p]),
                   (unsigned long long)deadline_delta);

operation_cleanup:
            for (size_t i = 0u; i < 7u; i++) instance_destroy(&path[i]);
            if (check_failures != 0) return;
        }
    }
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
            /* Watch returns at 81: the ring T1 borrow pulse at 80 has
               expired, while DONE/IRQ and divider LAST_CLOCK remain. */
            CHECK_EQ_U64(paths[WATCHED].m->timer_bank.timers[1].control_b, 0x0Cu);
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
    video_packet packets[PATH_COUNT] = {0};
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
    enum { INPUT, PALETTE, PEN, DMA_OWNER, PAGE_LATCH, AUDIO_ROUTE, RING_DEBT,
           HOST_WATCH, FLIP_COUNT };
    static const char *const names[FLIP_COUNT] = {
        "input mask", "palette component", "partial pen byte", "DMA owner deadline",
        "CPU page latch", "audio FD50 route", "audio ring debt", "host watch"
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
        case AUDIO_ROUTE:
            CHECK(lynx__audio_write(&copy->audio, 0xFD50u,
                                    (uint8_t)(copy->audio.stereo_disable ^ 1u),
                                    copy->guest_ticks, NULL));
            break;
        case RING_DEBT:
            copy->audio.ring_debt[0] = 1u;
            copy->audio.ring_defer_valid = 1u;
            copy->audio.ring_defer_tick = copy->guest_ticks;
            CHECK_NAMED(lynx__audio_ring_state_valid(&copy->audio, copy->guest_ticks),
                        "video_sensitivity.reachable-ring-debt-shape");
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
    printf("video_sensitivity: input,palette,partial-pen,DMA-owner,page-latch,audio-route,ring-debt sensitive; host-watch excluded\n");
}

enum {
    AV_TRACE_FRAME_LIMIT = 4,
    AV_TRACE_PCM_LIMIT = 512,
    AV_TRACE_EVENT_LIMIT = 16384
};

typedef struct av_trace {
    video_packet frames[AV_TRACE_FRAME_LIMIT];
    size_t frame_count;
    lynx_pcm_frame pcm[AV_TRACE_PCM_LIMIT];
    uint64_t sample_ordinals[AV_TRACE_PCM_LIMIT];
    size_t pcm_count;
    lynx_audio_event events[AV_TRACE_EVENT_LIMIT];
    size_t event_count;
    uint64_t input_ticks[4];
    uint32_t input_masks[4];
    size_t input_count;
    uint64_t final_tick;
    uint64_t first_sample_ordinal;
    uint64_t next_sample_ordinal;
    unsigned watch_hits;
    size_t calls;
} av_trace;

typedef struct av_trace_scratch {
    video_packet frame;
    lynx_pcm_frame pcm[1];
    lynx_audio_event events[LYNX_AV_MIN_EVENT_CAPACITY];
} av_trace_scratch;

typedef struct av_input_edge {
    uint64_t tick;
    uint32_t mask;
} av_input_edge;

enum av_slice_mode { AV_WHOLE, AV_COARSE, AV_ONE_TICK, AV_IRREGULAR };

static const av_input_edge av_probe_edges[] = {
    {UINT64_C(109321), LYNX_INPUT_A}
};
static const av_input_edge av_sensitivity_edges[] = {
    {UINT64_C(109321), LYNX_INPUT_A},
    {UINT64_C(130000), 0u}
};

static void av_trace_record_input(const char *name, av_trace *trace,
                                  const av_input_edge *edge)
{
    if (trace->input_count >= sizeof trace->input_ticks / sizeof trace->input_ticks[0]) {
        fprintf(stderr, "ASSERT %s exceeded the logical-input transcript bound\n", name);
        check_failures++;
        return;
    }
    trace->input_ticks[trace->input_count] = edge->tick;
    trace->input_masks[trace->input_count] = edge->mask;
    trace->input_count++;
}

static int av_trace_has_sequence(const av_trace *trace, uint64_t sequence)
{
    for (size_t i = 0u; i < trace->frame_count; i++) {
        if (trace->frames[i].result.sequence == sequence) return 1;
    }
    return 0;
}

static void av_trace_record_frame(const char *name, av_trace *trace,
                                  const av_trace_scratch *scratch,
                                  const lynx_av_result *result)
{
    if (trace->frame_count >= AV_TRACE_FRAME_LIMIT) {
        fprintf(stderr, "ASSERT %s exceeded the complete-frame bound\n", name);
        check_failures++;
        return;
    }
    video_packet *out = &trace->frames[trace->frame_count++];
    memcpy(out->pens, scratch->frame.pens, sizeof out->pens);
    memcpy(out->palettes, scratch->frame.palettes, sizeof out->palettes);
    memcpy(out->line_ticks, scratch->frame.line_ticks, sizeof out->line_ticks);
    memset(&out->result, 0, sizeof out->result);
    out->result.struct_size = (uint32_t)sizeof out->result;
    out->result.stop = result->stop;
    out->result.ticks_consumed = result->ticks_consumed;
    out->result.frame_start_tick = result->frame_start_tick;
    out->result.frame_end_tick = result->frame_end_tick;
    out->result.sequence = result->frame_sequence;
    out->result.row_count = result->row_count;
    out->result.flags = result->frame_flags;
    out->total_ticks = result->ticks_consumed;
    out->complete = result->row_count != 0u;
    out->empty_frames = result->row_count == 0u ? 1u : 0u;
}

static int av_trace_step(const char *name, instance *in, av_trace *trace,
                         av_trace_scratch *scratch, uint64_t target_tick,
                         uint64_t max_budget, enum av_slice_mode mode,
                         const av_input_edge *edges, size_t edge_count,
                         size_t *edge_cursor)
{
    static const uint64_t irregular[] = {1u, 17u, 257u, 5u, 61u, 509u, 3u};
    uint64_t now = in->m->guest_ticks;
    uint64_t limit;
    uint64_t budget;
    lynx_av_buffers buffers;
    lynx_av_result result;
    if (now >= target_tick) return 0;
    if (*edge_cursor < edge_count && edges[*edge_cursor].tick < now) {
        fprintf(stderr, "ASSERT %s skipped a host input edge at tick %llu\n", name,
                (unsigned long long)edges[*edge_cursor].tick);
        check_failures++;
        return 1;
    }
    if (*edge_cursor < edge_count && edges[*edge_cursor].tick == now) {
        CHECK_EQ_U64(lynx_set_input(in->m, edges[*edge_cursor].mask), LYNX_OK);
        av_trace_record_input(name, trace, &edges[*edge_cursor]);
        (*edge_cursor)++;
    }
    limit = target_tick;
    if (*edge_cursor < edge_count && edges[*edge_cursor].tick > now &&
        edges[*edge_cursor].tick < limit) limit = edges[*edge_cursor].tick;
    budget = limit - now;
    switch (mode) {
    case AV_WHOLE:
        break;
    case AV_COARSE:
        if (budget > 4096u) budget = 4096u;
        break;
    case AV_ONE_TICK:
        if (budget > 1u) budget = 1u;
        break;
    case AV_IRREGULAR: {
        const uint64_t piece = irregular[trace->calls %
                                          (sizeof irregular / sizeof irregular[0])];
        if (budget > piece) budget = piece;
        break;
    }
    }
    if (max_budget != 0u && budget > max_budget) budget = max_budget;
    if (budget == 0u) return 0;

    memset(&scratch->frame, 0xA5, sizeof scratch->frame);
    memset(&buffers, 0, sizeof buffers);
    buffers.struct_size = (uint32_t)sizeof buffers;
    frame_buffers_init(&buffers.frame, &scratch->frame);
    buffers.pcm = scratch->pcm;
    buffers.pcm_capacity = sizeof scratch->pcm / sizeof scratch->pcm[0];
    buffers.events = scratch->events;
    buffers.event_capacity = sizeof scratch->events / sizeof scratch->events[0];
    memset(&result, 0, sizeof result);
    result.struct_size = (uint32_t)sizeof result;
    CHECK_EQ_U64(lynx_advance_av(in->m, budget, &buffers, &result), LYNX_OK);
    trace->final_tick = in->m->guest_ticks;
    trace->calls++;
    CHECK(result.ticks_consumed <= budget);
    if (result.pcm_count > 0u) {
        const size_t produced = (size_t)result.pcm_count;
        CHECK(produced <= buffers.pcm_capacity);
        CHECK(produced <= AV_TRACE_PCM_LIMIT - trace->pcm_count);
        CHECK(result.next_sample_ordinal >= result.first_sample_ordinal);
        CHECK_EQ_U64(result.next_sample_ordinal - result.first_sample_ordinal, produced);
        if (trace->pcm_count == 0u) {
            trace->first_sample_ordinal = result.first_sample_ordinal;
        } else {
            CHECK_EQ_U64(result.first_sample_ordinal, trace->next_sample_ordinal);
        }
        if (produced <= AV_TRACE_PCM_LIMIT - trace->pcm_count) {
            for (size_t i = 0u; i < produced; i++) {
                trace->pcm[trace->pcm_count + i] = scratch->pcm[i];
                trace->sample_ordinals[trace->pcm_count + i] =
                    result.first_sample_ordinal + (uint64_t)i;
            }
            trace->pcm_count += produced;
        }
    }
    if ((size_t)result.event_count > buffers.event_capacity ||
        (size_t)result.event_count > AV_TRACE_EVENT_LIMIT - trace->event_count) {
        fprintf(stderr, "ASSERT %s exceeded the bounded event transcript\n", name);
        check_failures++;
    } else {
        for (size_t i = 0u; i < (size_t)result.event_count; i++) {
            trace->events[trace->event_count + i] = scratch->events[i];
        }
        trace->event_count += (size_t)result.event_count;
    }
    trace->next_sample_ordinal = result.next_sample_ordinal;
    if (result.stop == LYNX_STOP_FRAME) {
        av_trace_record_frame(name, trace, scratch, &result);
    } else if (result.stop == LYNX_STOP_WATCH) {
        lynx_watch_event event;
        memset(&event, 0, sizeof event);
        CHECK_EQ_U64(lynx_watch_get(in->m, &event), LYNX_OK);
        CHECK_EQ_U64(event.hit, 1u);
        trace->watch_hits++;
        CHECK_EQ_U64(lynx_watch_clear(in->m), LYNX_OK);
    } else {
        CHECK(result.stop == LYNX_STOP_BUDGET || result.stop == LYNX_STOP_OUTPUT);
    }
    if (result.ticks_consumed == 0u && result.pcm_count == 0u &&
        result.event_count == 0u && result.stop != LYNX_STOP_OUTPUT &&
        result.stop != LYNX_STOP_FRAME && result.stop != LYNX_STOP_WATCH) {
        fprintf(stderr, "ASSERT %s made no progress before its target\n", name);
        check_failures++;
        return 1;
    }
    return 0;
}

static int av_trace_to_tick(const char *name, instance *in, av_trace *trace,
                            av_trace_scratch *scratch, uint64_t target_tick,
                            enum av_slice_mode mode,
                            const av_input_edge *edges, size_t edge_count,
                            size_t *edge_cursor)
{
    const size_t max_calls = (size_t)(target_tick - in->m->guest_ticks) * 4u + 10000u;
    size_t start_calls = trace->calls;
    while (in->m->guest_ticks < target_tick && trace->calls - start_calls < max_calls) {
        if (av_trace_step(name, in, trace, scratch, target_tick, 0u, mode,
                          edges, edge_count, edge_cursor) != 0) return 1;
    }
    if (in->m->guest_ticks != target_tick) {
        fprintf(stderr, "ASSERT %s stopped at %llu instead of common tick %llu\n", name,
                (unsigned long long)in->m->guest_ticks, (unsigned long long)target_tick);
        check_failures++;
        return 1;
    }
    trace->final_tick = in->m->guest_ticks;
    return 0;
}

static int av_trace_to_frame_sequence(const char *name, instance *in, av_trace *trace,
                                      av_trace_scratch *scratch, uint64_t hard_limit,
                                      enum av_slice_mode mode,
                                      const av_input_edge *edges, size_t edge_count,
                                      size_t *edge_cursor, uint64_t sequence)
{
    const size_t max_calls = (size_t)hard_limit * 4u + 10000u;
    size_t start_calls = trace->calls;
    while (!av_trace_has_sequence(trace, sequence) && in->m->guest_ticks < hard_limit &&
           trace->calls - start_calls < max_calls) {
        if (av_trace_step(name, in, trace, scratch, hard_limit, 0u, mode,
                          edges, edge_count, edge_cursor) != 0) return 1;
    }
    if (!av_trace_has_sequence(trace, sequence)) {
        fprintf(stderr, "ASSERT %s did not produce frame sequence %llu by tick %llu\n", name,
                (unsigned long long)sequence, (unsigned long long)hard_limit);
        check_failures++;
        return 1;
    }
    return 0;
}

static int av_trace_equal(const char *name, const av_trace *a, const av_trace *b)
{
    CHECK_NAMED(a->frame_count == b->frame_count, name);
    CHECK_NAMED(a->pcm_count == b->pcm_count, name);
    CHECK_NAMED(a->event_count == b->event_count, name);
    CHECK_NAMED(a->input_count == b->input_count, name);
    CHECK_EQ_U64(a->final_tick, b->final_tick);
    CHECK_EQ_U64(a->first_sample_ordinal, b->first_sample_ordinal);
    CHECK_EQ_U64(a->next_sample_ordinal, b->next_sample_ordinal);
    if (a->frame_count != b->frame_count || a->pcm_count != b->pcm_count ||
        a->event_count != b->event_count || a->input_count != b->input_count) return 1;
    for (size_t i = 0u; i < a->input_count; i++) {
        CHECK_EQ_U64(a->input_ticks[i], b->input_ticks[i]);
        CHECK_EQ_U64(a->input_masks[i], b->input_masks[i]);
    }
    for (size_t i = 0u; i < a->frame_count; i++) {
        CHECK_NAMED(video_packet_equal(&a->frames[i], &b->frames[i]), name);
    }
    for (size_t i = 0u; i < a->pcm_count; i++) {
        CHECK_EQ_U64(a->sample_ordinals[i], b->sample_ordinals[i]);
        CHECK_EQ_U64((uint16_t)a->pcm[i].left, (uint16_t)b->pcm[i].left);
        CHECK_EQ_U64((uint16_t)a->pcm[i].right, (uint16_t)b->pcm[i].right);
    }
    for (size_t i = 0u; i < a->event_count; i++) {
        const lynx_audio_event *left = &a->events[i];
        const lynx_audio_event *right = &b->events[i];
        CHECK_EQ_U64(left->tick, right->tick);
        CHECK_EQ_U64(left->sequence, right->sequence);
        CHECK_EQ_U64(left->channel, right->channel);
        CHECK_EQ_U64(left->cause, right->cause);
        CHECK_EQ_U64((uint8_t)left->before_output, (uint8_t)right->before_output);
        CHECK_EQ_U64((uint8_t)left->after_output, (uint8_t)right->after_output);
    }
    return 0;
}

static void av_guest_state_equal(const char *name, const instance *reference,
                                 const instance *candidate, const av_fixture *fixture)
{
    uint64_t ticks_a = 0u, ticks_b = 0u;
    uint8_t digest_a[32], digest_b[32];
    uint8_t probe_a[9], probe_b[9];
    CHECK_EQ_U64(lynx_guest_ticks(reference->m, &ticks_a), LYNX_OK);
    CHECK_EQ_U64(lynx_guest_ticks(candidate->m, &ticks_b), LYNX_OK);
    CHECK_EQ_U64(ticks_a, ticks_b);
    CHECK_EQ_U64(lynx_read_memory(reference->m, fixture->probe_result, probe_a,
                                  sizeof probe_a), LYNX_OK);
    CHECK_EQ_U64(lynx_read_memory(candidate->m, fixture->probe_result, probe_b,
                                  sizeof probe_b), LYNX_OK);
    CHECK_NAMED(memcmp(probe_a, probe_b, sizeof probe_a) == 0, name);
    CHECK_EQ_U64(reference->m->video.input_mask, candidate->m->video.input_mask);
    CHECK_EQ_U64(lynx_state_digest(reference->m, digest_a), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(candidate->m, digest_b), LYNX_OK);
    CHECK_NAMED(memcmp(digest_a, digest_b, sizeof digest_a) == 0, name);
}

static int av_apply_edge_now(const char *name, instance *in, av_trace *trace,
                             const av_input_edge *edges, size_t edge_count,
                             size_t *edge_cursor)
{
    if (*edge_cursor >= edge_count || edges[*edge_cursor].tick != in->m->guest_ticks) {
        fprintf(stderr, "ASSERT %s has no host input edge at the current guest tick\n", name);
        check_failures++;
        return 1;
    }
    CHECK_EQ_U64(lynx_set_input(in->m, edges[*edge_cursor].mask), LYNX_OK);
    av_trace_record_input(name, trace, &edges[*edge_cursor]);
    (*edge_cursor)++;
    return 0;
}

static int av_snapshot(const instance *source, instance *target, uint8_t **snapshot,
                       size_t *snapshot_size)
{
    size_t written = 0u;
    CHECK_EQ_U64(lynx_snapshot_size(source->m, snapshot_size), LYNX_OK);
    *snapshot = malloc(*snapshot_size);
    CHECK(*snapshot != NULL);
    if (*snapshot == NULL) return 1;
    CHECK_EQ_U64(lynx_snapshot_save(source->m, *snapshot, *snapshot_size, &written), LYNX_OK);
    CHECK_EQ_U64(written, *snapshot_size);
    CHECK_EQ_U64(lynx_snapshot_restore(target->m, *snapshot, *snapshot_size), LYNX_OK);
    return 0;
}

static int av_checkpoint_path(const char *name, const av_fixture *fixture,
                              uint64_t checkpoint_tick, int apply_edge_before_save,
                              uint64_t final_tick, const av_trace *reference_trace,
                              const instance *reference_machine)
{
    instance source = {0}, restored = {0};
    av_trace *source_trace = calloc(1u, sizeof *source_trace);
    av_trace *restored_trace = calloc(1u, sizeof *restored_trace);
    av_trace_scratch scratch;
    size_t source_cursor = 0u, restored_cursor = 0u;
    uint8_t *snapshot = NULL;
    size_t snapshot_size = 0u;
    const size_t edge_count = sizeof av_probe_edges / sizeof av_probe_edges[0];
    int failed = 0;

    CHECK(source_trace != NULL && restored_trace != NULL);
    if (source_trace == NULL || restored_trace == NULL ||
        av_instance_make(&source, fixture) != 0 || av_instance_make(&restored, fixture) != 0) {
        fprintf(stderr, "ASSERT %s failed to allocate checkpoint instances\n", name);
        check_failures++;
        failed = 1;
        goto done;
    }
    memset(&scratch, 0, sizeof scratch);
    if (av_trace_to_tick(name, &source, source_trace, &scratch, checkpoint_tick,
                         AV_WHOLE, av_probe_edges, edge_count, &source_cursor) != 0) {
        failed = 1;
        goto done;
    }
    if (apply_edge_before_save != 0 &&
        av_apply_edge_now(name, &source, source_trace, av_probe_edges, edge_count,
                          &source_cursor) != 0) {
        failed = 1;
        goto done;
    }
    if (av_snapshot(&source, &restored, &snapshot, &snapshot_size) != 0) {
        failed = 1;
        goto done;
    }
    *restored_trace = *source_trace;
    restored_cursor = source_cursor;
    if (av_trace_to_tick(name, &restored, restored_trace, &scratch, final_tick,
                         AV_WHOLE, av_probe_edges, edge_count, &restored_cursor) != 0) {
        failed = 1;
        goto done;
    }
    av_trace_equal(name, reference_trace, restored_trace);
    av_guest_state_equal(name, reference_machine, &restored, fixture);
    CHECK_EQ_U64(restored_cursor, edge_count);
    CHECK_NAMED(av_trace_has_sequence(restored_trace, 2u), name);
done:
    free(snapshot);
    instance_destroy(&restored);
    instance_destroy(&source);
    free(restored_trace);
    free(source_trace);
    return failed;
}

static const video_packet *av_trace_frame(const av_trace *trace, uint64_t sequence)
{
    for (size_t i = 0u; i < trace->frame_count; i++) {
        if (trace->frames[i].result.sequence == sequence) return &trace->frames[i];
    }
    return NULL;
}

static int av_trace_has_nonzero_pcm(const av_trace *trace)
{
    for (size_t i = 0u; i < trace->pcm_count; i++) {
        if (trace->pcm[i].left != 0 || trace->pcm[i].right != 0) return 1;
    }
    return 0;
}

static void av_zero_suffix_restore(const av_fixture *fixture, const instance *source,
                                   const av_trace *trace)
{
    instance restored = {0};
    av_trace *zero_suffix_trace = malloc(sizeof *zero_suffix_trace);
    av_trace_scratch scratch;
    size_t snapshot_size = 0u;
    uint8_t *snapshot = NULL;
    lynx_av_buffers buffers;
    lynx_av_result result;
    uint8_t digest_source[32], digest_restored[32];

    CHECK(zero_suffix_trace != NULL);
    if (zero_suffix_trace == NULL) goto done;
    *zero_suffix_trace = *trace;
    CHECK_EQ_U64(av_instance_make(&restored, fixture), 0u);
    if (restored.m == NULL) goto done;
    if (av_snapshot(source, &restored, &snapshot, &snapshot_size) != 0) goto done;
    memset(&scratch, 0xA5, sizeof scratch);
    memset(&buffers, 0, sizeof buffers);
    buffers.struct_size = (uint32_t)sizeof buffers;
    frame_buffers_init(&buffers.frame, &scratch.frame);
    buffers.pcm = scratch.pcm;
    buffers.pcm_capacity = sizeof scratch.pcm / sizeof scratch.pcm[0];
    buffers.events = scratch.events;
    buffers.event_capacity = sizeof scratch.events / sizeof scratch.events[0];
    memset(&result, 0, sizeof result);
    result.struct_size = (uint32_t)sizeof result;
    CHECK_EQ_U64(lynx_advance_av(restored.m, 0u, &buffers, &result), LYNX_OK);
    CHECK_EQ_U64(result.stop, LYNX_STOP_BUDGET);
    CHECK_EQ_U64(result.ticks_consumed, 0u);
    CHECK_EQ_U64(result.pcm_count, 0u);
    CHECK_EQ_U64(result.event_count, 0u);
    CHECK_EQ_U64(result.next_sample_ordinal, trace->next_sample_ordinal);
    CHECK_NAMED(scratch.frame.pens[0] == 0xA5u && scratch.frame.palettes[0] == 0xA5u,
                "audio_paths.zero-suffix-no-frame-write");
    CHECK_NAMED(zero_suffix_trace->frame_count == trace->frame_count &&
                    zero_suffix_trace->pcm_count == trace->pcm_count &&
                    zero_suffix_trace->event_count == trace->event_count,
                "audio_paths.zero-suffix-no-transcript-append");
    CHECK_EQ_U64(lynx_state_digest(source->m, digest_source), LYNX_OK);
    CHECK_EQ_U64(lynx_state_digest(restored.m, digest_restored), LYNX_OK);
    CHECK_NAMED(memcmp(digest_source, digest_restored, sizeof digest_source) == 0,
                "audio_paths.zero-suffix-state");
    CHECK_EQ_U64(av_trace_equal("audio_paths.zero-suffix-complete-prefix",
                                trace, zero_suffix_trace), 0u);
done:
    free(snapshot);
    instance_destroy(&restored);
    free(zero_suffix_trace);
}

static int av_single_sample_suffix(const av_fixture *fixture)
{
    instance source = {0}, restored = {0};
    av_trace *source_trace = calloc(1u, sizeof *source_trace);
    av_trace *restored_trace = calloc(1u, sizeof *restored_trace);
    av_trace_scratch scratch;
    size_t source_cursor = 0u, restored_cursor = 0u;
    uint8_t *snapshot = NULL;
    size_t snapshot_size = 0u;
    const size_t edge_count = sizeof av_probe_edges / sizeof av_probe_edges[0];
    int failed = 0;

    CHECK(source_trace != NULL && restored_trace != NULL);
    if (source_trace == NULL || restored_trace == NULL ||
        av_instance_make(&source, fixture) != 0 || av_instance_make(&restored, fixture) != 0) {
        fprintf(stderr, "ASSERT audio_paths.single-sample-suffix allocation failed\n");
        check_failures++;
        failed = 1;
        goto done;
    }
    memset(&scratch, 0, sizeof scratch);
    if (av_trace_to_tick("audio_paths.single-sample-prefix", &source, source_trace,
                         &scratch, 399u, AV_WHOLE, av_probe_edges, edge_count,
                         &source_cursor) != 0 ||
        av_snapshot(&source, &restored, &snapshot, &snapshot_size) != 0) {
        failed = 1;
        goto done;
    }
    *restored_trace = *source_trace;
    restored_cursor = source_cursor;
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.single-sample-source", &source,
                  source_trace, &scratch, 401u, AV_WHOLE, av_probe_edges,
                  edge_count, &source_cursor), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.single-sample-restored", &restored,
                  restored_trace, &scratch, 401u, AV_WHOLE, av_probe_edges,
                  edge_count, &restored_cursor), 0u);
    CHECK_EQ_U64(source_trace->pcm_count, 1u);
    CHECK_EQ_U64(restored_trace->pcm_count, 1u);
    CHECK_EQ_U64(source_trace->sample_ordinals[0], 0u);
    CHECK_EQ_U64(source_trace->next_sample_ordinal, 1u);
    CHECK_EQ_U64(av_trace_equal("audio_paths.single-sample-suffix-equal",
                                source_trace, restored_trace), 0u);
    av_guest_state_equal("audio_paths.single-sample-common-state", &source,
                         &restored, fixture);
    CHECK_EQ_U64(source_cursor, 0u);
    CHECK_EQ_U64(restored_cursor, source_cursor);
done:
    free(snapshot);
    instance_destroy(&restored);
    instance_destroy(&source);
    free(restored_trace);
    free(source_trace);
    return failed;
}

static int av_single_event_suffix(const av_fixture *fixture,
                                  const av_trace *reference_trace)
{
    uint64_t event_tick = UINT64_MAX;
    for (size_t i = 0u; i < reference_trace->event_count; i++) {
        const uint64_t tick = reference_trace->events[i].tick;
        size_t at_tick = 0u;
        for (size_t j = 0u; j < reference_trace->event_count; j++) {
            if (reference_trace->events[j].tick == tick) at_tick++;
        }
        if (at_tick == 1u && tick > av_probe_edges[0].tick) {
            event_tick = tick;
            break;
        }
    }
    CHECK_NAMED(event_tick != UINT64_MAX,
                "audio_paths.single-event-reference-has-isolated-record");
    if (event_tick == UINT64_MAX) return 1;

    instance source = {0}, restored = {0};
    av_trace *source_trace = calloc(1u, sizeof *source_trace);
    av_trace *restored_trace = calloc(1u, sizeof *restored_trace);
    av_trace_scratch scratch;
    size_t source_cursor = 0u, restored_cursor = 0u;
    size_t prefix_events = 0u;
    uint8_t *snapshot = NULL;
    size_t snapshot_size = 0u;
    const size_t edge_count = sizeof av_probe_edges / sizeof av_probe_edges[0];
    int failed = 0;

    CHECK(source_trace != NULL && restored_trace != NULL);
    if (source_trace == NULL || restored_trace == NULL ||
        av_instance_make(&source, fixture) != 0 || av_instance_make(&restored, fixture) != 0) {
        fprintf(stderr, "ASSERT audio_paths.single-event-suffix allocation failed\n");
        check_failures++;
        failed = 1;
        goto done;
    }
    memset(&scratch, 0, sizeof scratch);
    if (av_trace_to_tick("audio_paths.single-event-prefix", &source, source_trace,
                         &scratch, event_tick, AV_WHOLE, av_probe_edges, edge_count,
                         &source_cursor) != 0 ||
        av_snapshot(&source, &restored, &snapshot, &snapshot_size) != 0) {
        failed = 1;
        goto done;
    }
    prefix_events = source_trace->event_count;
    *restored_trace = *source_trace;
    restored_cursor = source_cursor;
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.single-event-source", &source,
                  source_trace, &scratch, event_tick + 1u, AV_WHOLE,
                  av_probe_edges, edge_count, &source_cursor), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.single-event-restored", &restored,
                  restored_trace, &scratch, event_tick + 1u, AV_WHOLE,
                  av_probe_edges, edge_count, &restored_cursor), 0u);
    CHECK_EQ_U64(source_trace->event_count - prefix_events, 1u);
    CHECK_EQ_U64(source_trace->events[prefix_events].tick, event_tick);
    CHECK_EQ_U64(av_trace_equal("audio_paths.single-event-suffix-equal",
                                source_trace, restored_trace), 0u);
    av_guest_state_equal("audio_paths.single-event-common-state", &source,
                         &restored, fixture);
    CHECK_EQ_U64(restored_cursor, source_cursor);
done:
    free(snapshot);
    instance_destroy(&restored);
    instance_destroy(&source);
    free(restored_trace);
    free(source_trace);
    return failed;
}

static int av_seek_audio_write_request(const char *name, instance *in,
                                       av_trace *trace, av_trace_scratch *scratch,
                                       size_t *edge_cursor)
{
    const uint64_t hard_limit = av_probe_edges[0].tick + UINT64_C(12000);
    while (in->m->guest_ticks < hard_limit) {
        if ((in->m->cpu.req_flags & LYNX__CF_WRITE) != 0u &&
            in->m->cpu.req_addr == 0xFD20u) {
            CHECK_EQ_U64(in->m->video.input_mask, LYNX_INPUT_A);
            CHECK_EQ_U64(in->m->cpu.req_data, 1u);
            return 0;
        }
        if (av_trace_step(name, in, trace, scratch, in->m->guest_ticks + 1u,
                          0u, AV_WHOLE, av_probe_edges,
                          sizeof av_probe_edges / sizeof av_probe_edges[0],
                          edge_cursor) != 0) return 1;
    }
    fprintf(stderr, "ASSERT %s did not reach the input-triggered audio write request\n", name);
    check_failures++;
    return 1;
}

static uint64_t av_ring_debt_total(const lynx__audio_state *audio)
{
    uint64_t total = 0u;
    for (size_t i = 0u; i < LYNX__AUDIO_RING_NODE_COUNT; i++) {
        total += audio->ring_debt[i];
    }
    return total;
}

static int av_capacity_ring_continuation(const av_fixture *fixture)
{
    instance source = {0}, restored = {0};
    av_trace *trace = calloc(1u, sizeof *trace);
    av_trace_scratch scratch;
    size_t edge_cursor = 0u, restored_cursor = 0u;
    uint8_t *snapshot = NULL;
    size_t snapshot_size = 0u;
    int failed = 0;

    CHECK(trace != NULL);
    if (trace == NULL || av_instance_make(&source, fixture) != 0 ||
        av_instance_make(&restored, fixture) != 0) {
        fprintf(stderr, "ASSERT audio_paths.capacity-ring allocation failed\n");
        check_failures++;
        failed = 1;
        goto done;
    }
    memset(&scratch, 0, sizeof scratch);
    if (av_seek_audio_write_request("audio_paths.capacity-ring-seek", &source,
                                    trace, &scratch, &edge_cursor) != 0) {
        failed = 1;
        goto done;
    }

    /* Keep the real fixture's posted FD20 write request, then place it at the
       same accepted scheduler boundary as the capacity/ring control. These
       synthetic held-device values are unreachable from the guest by design. */
    source.m->guest_ticks = 399u;
    source.m->cpu_ready_at = 400u;
    lynx__timer_init(&source.m->timer_bank);
    lynx__video_init(&source.m->video, LYNX_TIMING_POLICY_VIDEO_POLICY1);
    source.m->video.refresh_next_tick = 512u;
    CHECK_EQ_U64(lynx__input_set(&source.m->video, LYNX_INPUT_A), LYNX_OK);
    lynx__audio_init(&source.m->audio);
    for (uint8_t i = 0u; i < LYNX__AUDIO_CHANNEL_COUNT; i++) {
        lynx__audio_channel *channel = &source.m->audio.channels[i];
        channel->volume = i == 0u ? 5u : (uint8_t)(i + 1u);
        channel->feedback = 1u; /* alternating tap-0 feedback makes two records */
        channel->control_a = 0x5Fu; /* enabled, reload/reset-DONE, linked */
        channel->count = 0u;
        channel->backup = 0u;
    }
    for (uint8_t i = 4u; i < LYNX__AUDIO_RING_NODE_COUNT; i++) {
        source.m->audio.ring_debt[i] = 2u;
    }
    source.m->audio.ring_cursor = 4u;
    source.m->audio.ring_defer_valid = 1u;
    source.m->audio.ring_defer_tick = 400u;
    source.m->audio.same_tick_device_cursor = LYNX__AUDIO_RING_VISIT_LIMIT;
    memset(trace, 0, sizeof *trace);
    trace->input_count = 1u;
    trace->input_ticks[0] = av_probe_edges[0].tick;
    trace->input_masks[0] = av_probe_edges[0].mask;
    trace->final_tick = 399u;
    CHECK_EQ_U64(sizeof scratch.events / sizeof scratch.events[0],
                 LYNX_AV_MIN_EVENT_CAPACITY);
    CHECK_EQ_U64(sizeof scratch.pcm / sizeof scratch.pcm[0],
                 LYNX_AV_MIN_PCM_CAPACITY);
    CHECK_NAMED((source.m->cpu.req_flags & LYNX__CF_WRITE) != 0u &&
                    source.m->cpu.req_addr == 0xFD20u && source.m->cpu.req_data == 1u,
                "audio_paths.capacity-ring-real-guest-strobe");
    CHECK_NAMED(lynx__audio_ring_state_valid(&source.m->audio, 399u),
                "audio_paths.capacity-ring-valid-before-defer");

    CHECK_EQ_U64(av_trace_step("audio_paths.capacity-frontier-exclusive", &source,
                  trace, &scratch, 401u, 1u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &edge_cursor), 0u);
    CHECK_EQ_U64(source.m->guest_ticks, 400u);
    CHECK_EQ_U64(av_trace_step("audio_paths.capacity-frontier-device-sample", &source,
                  trace, &scratch, 401u, 1u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &edge_cursor), 0u);
    CHECK_EQ_U64(trace->event_count, 8u);
    CHECK_EQ_U64(LYNX_AV_MIN_EVENT_CAPACITY - trace->event_count, 5u);
    CHECK_EQ_U64(trace->pcm_count, 1u);
    CHECK_EQ_U64(trace->sample_ordinals[0], 0u);
    CHECK_EQ_U64(source.m->guest_ticks, 400u);
    CHECK_EQ_U64(source.m->cpu.req_addr, 0xFD20u);
    CHECK_EQ_U64(source.m->audio.channels[0].volume, 5u);
    CHECK_NAMED(av_ring_debt_total(&source.m->audio) != 0u &&
                    source.m->audio.ring_defer_valid == 1u &&
                    source.m->audio.ring_defer_tick == 401u &&
                    lynx__audio_ring_state_valid(&source.m->audio, 400u),
                "audio_paths.capacity-frontier-retains-deferred-ring");
    for (size_t i = 0u; i < trace->event_count; i++) {
        CHECK_EQ_U64(trace->events[i].tick, 400u);
        CHECK_EQ_U64(trace->events[i].sequence, i);
        CHECK_EQ_U64(trace->events[i].cause, LYNX_AUDIO_EVENT_CLOCK);
    }

    if (av_snapshot(&source, &restored, &snapshot, &snapshot_size) != 0) {
        failed = 1;
        goto done;
    }
    av_trace *restored_trace = calloc(1u, sizeof *restored_trace);
    if (restored_trace == NULL) {
        fprintf(stderr, "ASSERT audio_paths.capacity-ring trace allocation failed\n");
        check_failures++;
        failed = 1;
        goto done;
    }
    *restored_trace = *trace;
    restored_cursor = edge_cursor;
    CHECK_EQ_U64(lynx_watch_configure(source.m, 0xFD20u, 1u), LYNX_OK);
    CHECK_EQ_U64(lynx_watch_configure(restored.m, 0xFD20u, 1u), LYNX_OK);
    CHECK_EQ_U64(av_trace_step("audio_paths.capacity-frontier-cpu-write", &source,
                  trace, &scratch, 401u, 1u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &edge_cursor), 0u);
    CHECK_EQ_U64(av_trace_step("audio_paths.capacity-frontier-restored-write", &restored,
                  restored_trace, &scratch, 401u, 1u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &restored_cursor), 0u);
    CHECK_EQ_U64(trace->events[8].sequence, 8u);
    CHECK_EQ_U64(trace->events[8].cause, LYNX_AUDIO_EVENT_WRITE);
    CHECK_EQ_U64(trace->event_count, 9u);
    CHECK_EQ_U64(source.m->audio.channels[0].volume, 1u);
    CHECK_EQ_U64(source.m->guest_ticks, 401u);
    CHECK_EQ_U64(source.m->audio.next_sample_ordinal, 1u);
    CHECK_EQ_U64(source.m->audio.ring_defer_tick, 401u);
    CHECK_EQ_U64(trace->watch_hits, 1u);
    CHECK_EQ_U64(restored_trace->watch_hits, 1u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.deferred-ring-source", &source,
                  trace, &scratch, 402u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &edge_cursor), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.deferred-ring-restored", &restored,
                  restored_trace, &scratch, 402u, AV_WHOLE, av_probe_edges,
                  sizeof av_probe_edges / sizeof av_probe_edges[0], &restored_cursor), 0u);
    size_t continuation_slices = 1u;
    while (source.m->audio.ring_defer_valid != 0u &&
           source.m->guest_ticks < 410u && continuation_slices < 10u) {
        uint64_t target = source.m->audio.ring_defer_tick + 1u;
        if (target <= source.m->guest_ticks) target = source.m->guest_ticks + 1u;
        CHECK_EQ_U64(av_trace_to_tick("audio_paths.deferred-ring-source-next", &source,
                      trace, &scratch, target, AV_WHOLE, av_probe_edges,
                      sizeof av_probe_edges / sizeof av_probe_edges[0], &edge_cursor), 0u);
        CHECK_EQ_U64(av_trace_to_tick("audio_paths.deferred-ring-restored-next", &restored,
                      restored_trace, &scratch, target, AV_WHOLE, av_probe_edges,
                      sizeof av_probe_edges / sizeof av_probe_edges[0], &restored_cursor), 0u);
        continuation_slices++;
    }
    CHECK_NAMED(continuation_slices > 0u,
                "audio_paths.deferred-ring-resumed-after-stop");
    CHECK_NAMED(av_trace_equal("audio_paths.deferred-ring-streams", trace,
                               restored_trace) == 0,
                "audio_paths.deferred-ring-comparison-ran");
    av_guest_state_equal("audio_paths.deferred-ring-common-state", &source,
                         &restored, fixture);
    CHECK_NAMED(source.m->audio.ring_defer_valid == 0u,
                "audio_paths.deferred-ring-drained-without-loss");
    CHECK_EQ_U64(edge_cursor, restored_cursor);
    free(restored_trace);
done:
    free(snapshot);
    free(trace);
    instance_destroy(&restored);
    instance_destroy(&source);
    return failed;
}

static int av_audio_record_prefix_equal(const av_trace *a, const av_trace *b,
                                        size_t frame_count, size_t pcm_count,
                                        size_t event_count, size_t input_count)
{
    for (size_t i = 0u; i < frame_count; i++) {
        if (!video_packet_equal(&a->frames[i], &b->frames[i])) return 0;
    }
    for (size_t i = 0u; i < pcm_count; i++) {
        if (a->sample_ordinals[i] != b->sample_ordinals[i] ||
            a->pcm[i].left != b->pcm[i].left || a->pcm[i].right != b->pcm[i].right) {
            return 0;
        }
    }
    for (size_t i = 0u; i < event_count; i++) {
        const lynx_audio_event *left = &a->events[i];
        const lynx_audio_event *right = &b->events[i];
        if (left->tick != right->tick || left->sequence != right->sequence ||
            left->channel != right->channel || left->cause != right->cause ||
            left->before_output != right->before_output ||
            left->after_output != right->after_output) return 0;
    }
    for (size_t i = 0u; i < input_count; i++) {
        if (a->input_ticks[i] != b->input_ticks[i] ||
            a->input_masks[i] != b->input_masks[i]) return 0;
    }
    return 1;
}

static void case_audio_sensitivity(char **args)
{
    enum { LFSR, INTEGRATOR, COUNTER, DEFERRED_PULSE, SAMPLE_PHASE, LOGICAL_INPUT,
           STATE_FLIP_COUNT };
    static const char *const state_names[STATE_FLIP_COUNT] = {
        "LFSR shift", "integrator", "channel counter", "deferred pulse",
        "next sample phase", "logical input"
    };
    const char *bll_path = args[0];
    const char *labels_path = args[1];
    const char *manifest_path = args[2];
    av_fixture fixture;
    instance state_base = {0};
    instance flips[STATE_FLIP_COUNT] = {{0}};
    uint8_t base_digest[32];
    uint8_t flip_digests[STATE_FLIP_COUNT][32];
    instance trace_machine = {0};
    av_trace *trace = calloc(1u, sizeof *trace);
    av_trace *variant = calloc(1u, sizeof *variant);
    av_trace_scratch scratch;
    size_t cursor = 0u;
    uint64_t final_tick = 0u;
    (void)args;

    CHECK_NAMED(av_fixture_load(bll_path, labels_path, manifest_path, &fixture) == 0,
                "audio_sensitivity.manifest-bound-source-fixture");
    if (fixture.bll == NULL) return;
    CHECK(trace != NULL && variant != NULL);
    if (trace == NULL || variant == NULL || av_instance_make(&state_base, &fixture) != 0 ||
        av_instance_make(&trace_machine, &fixture) != 0) {
        fprintf(stderr, "ASSERT audio_sensitivity allocation failed\n");
        check_failures++;
        goto cleanup;
    }
    CHECK_EQ_U64(lynx_state_digest(state_base.m, base_digest), LYNX_OK);
    for (size_t i = 0u; i < STATE_FLIP_COUNT; i++) {
        flips[i].mem = malloc(lynx_machine_size());
        CHECK(flips[i].mem != NULL);
        if (flips[i].mem == NULL) continue;
        memcpy(flips[i].mem, state_base.m, lynx_machine_size());
        flips[i].m = flips[i].mem;
        switch (i) {
        case LFSR:
            flips[i].m->audio.channels[0].shift ^= 1u;
            break;
        case INTEGRATOR:
            flips[i].m->audio.channels[0].integrator =
                flips[i].m->audio.channels[0].integrator == INT8_MAX ?
                    (int8_t)(INT8_MAX - 1) :
                    (int8_t)(flips[i].m->audio.channels[0].integrator + 1);
            break;
        case COUNTER:
            flips[i].m->audio.channels[0].count ^= 1u;
            break;
        case DEFERRED_PULSE:
            flips[i].m->audio.ring_debt[4] = 1u;
            flips[i].m->audio.ring_cursor = 4u;
            flips[i].m->audio.ring_defer_valid = 1u;
            flips[i].m->audio.ring_defer_tick = flips[i].m->guest_ticks;
            CHECK_NAMED(lynx__audio_ring_state_valid(&flips[i].m->audio,
                                                     flips[i].m->guest_ticks),
                        "audio_sensitivity.valid-deferred-pulse-shape");
            break;
        case SAMPLE_PHASE:
            flips[i].m->audio.next_sample_tick++;
            break;
        case LOGICAL_INPUT:
            CHECK_EQ_U64(lynx__input_set(&flips[i].m->video, LYNX_INPUT_A), LYNX_OK);
            break;
        }
        CHECK_EQ_U64(lynx_state_digest(flips[i].m, flip_digests[i]), LYNX_OK);
        CHECK_NAMED(memcmp(base_digest, flip_digests[i], sizeof base_digest) != 0,
                    state_names[i]);
        for (size_t j = 0u; j < i; j++) {
            if (flips[j].m != NULL) {
                CHECK_NAMED(memcmp(flip_digests[i], flip_digests[j],
                                   sizeof flip_digests[i]) != 0,
                            "audio_sensitivity.independent-state-fields");
            }
        }
    }

    memset(&scratch, 0, sizeof scratch);
    CHECK_EQ_U64(av_trace_to_frame_sequence("audio_sensitivity.real-guest",
                  &trace_machine, trace, &scratch, UINT64_C(200000), AV_WHOLE,
                  av_sensitivity_edges,
                  sizeof av_sensitivity_edges / sizeof av_sensitivity_edges[0],
                  &cursor, 2u), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_sensitivity.frame-boundary",
                  &trace_machine, trace, &scratch,
                  trace_machine.m->guest_ticks + 1u, AV_WHOLE,
                  av_sensitivity_edges,
                  sizeof av_sensitivity_edges / sizeof av_sensitivity_edges[0],
                  &cursor), 0u);
    final_tick = trace_machine.m->guest_ticks;
    CHECK_NAMED(trace->frame_count >= 3u && trace->pcm_count > 1u &&
                    trace->event_count > 1u && trace->input_count == 2u,
                "audio_sensitivity.nonempty-complete-observation-streams");
    CHECK_EQ_U64(trace->final_tick, final_tick);

    if (trace->frame_count >= 3u && trace->pcm_count > 1u &&
        trace->event_count > 1u && trace->input_count == 2u) {
        *variant = *trace;
        variant->frames[2].pens[0] ^= 1u;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, 2u,
                       trace->pcm_count, trace->event_count, trace->input_count),
                    "audio_sensitivity.raw-frame-unaffected-prefix");
        CHECK_NAMED(!video_packet_equal(&trace->frames[2], &variant->frames[2]),
                    "audio_sensitivity.raw-frame-record-detected");

        *variant = *trace;
        variant->frames[2].palettes[0] ^= 1u;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, 2u,
                       trace->pcm_count, trace->event_count, trace->input_count) &&
                        !video_packet_equal(&trace->frames[2], &variant->frames[2]),
                    "audio_sensitivity.palette-record-detected-after-equal-prefix");

        *variant = *trace;
        variant->frames[2].line_ticks[0]++;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, 2u,
                       trace->pcm_count, trace->event_count, trace->input_count) &&
                        !video_packet_equal(&trace->frames[2], &variant->frames[2]),
                    "audio_sensitivity.line-tick-record-detected-after-equal-prefix");

        *variant = *trace;
        variant->frames[2].result.frame_end_tick++;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, 2u,
                       trace->pcm_count, trace->event_count, trace->input_count) &&
                        !video_packet_equal(&trace->frames[2], &variant->frames[2]),
                    "audio_sensitivity.frame-metadata-detected-after-equal-prefix");

        *variant = *trace;
        variant->pcm[trace->pcm_count - 1u].left =
            trace->pcm[trace->pcm_count - 1u].left == INT16_MAX ?
                (int16_t)(INT16_MAX - 1) :
                (int16_t)(trace->pcm[trace->pcm_count - 1u].left + 1);
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count - 1u, trace->event_count, trace->input_count),
                    "audio_sensitivity.pcm-unaffected-prefix");
        CHECK_NAMED(trace->pcm[trace->pcm_count - 1u].left !=
                        variant->pcm[trace->pcm_count - 1u].left,
                    "audio_sensitivity.pcm-record-detected");

        *variant = *trace;
        variant->sample_ordinals[trace->pcm_count - 1u]++;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count - 1u, trace->event_count, trace->input_count) &&
                        variant->sample_ordinals[trace->pcm_count - 1u] !=
                            trace->sample_ordinals[trace->pcm_count - 1u],
                    "audio_sensitivity.sample-ordinal-detected-after-equal-prefix");

        *variant = *trace;
        variant->events[trace->event_count - 1u].after_output =
            trace->events[trace->event_count - 1u].after_output == INT8_MAX ?
                (int8_t)(INT8_MAX - 1) :
                (int8_t)(trace->events[trace->event_count - 1u].after_output + 1);
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count, trace->event_count - 1u, trace->input_count),
                    "audio_sensitivity.event-unaffected-prefix");
        CHECK_NAMED(variant->events[trace->event_count - 1u].after_output !=
                        trace->events[trace->event_count - 1u].after_output,
                    "audio_sensitivity.event-record-detected");

        *variant = *trace;
        variant->input_masks[1] = LYNX_INPUT_B;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count, trace->event_count, 1u),
                    "audio_sensitivity.logical-input-unaffected-prefix");
        CHECK_NAMED(variant->input_masks[1] != trace->input_masks[1],
                    "audio_sensitivity.logical-input-record-detected");

        *variant = *trace;
        variant->input_ticks[1]++;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count, trace->event_count, 1u) &&
                        variant->input_ticks[1] != trace->input_ticks[1],
                    "audio_sensitivity.logical-input-tick-detected-after-equal-prefix");

        *variant = *trace;
        variant->final_tick++;
        CHECK_NAMED(av_audio_record_prefix_equal(trace, variant, trace->frame_count,
                       trace->pcm_count, trace->event_count, trace->input_count),
                    "audio_sensitivity.progress-unaffected-streams");
        CHECK_NAMED(variant->final_tick != trace->final_tick,
                    "audio_sensitivity.progress-record-detected");
    }
    printf("audio_sensitivity: LFSR,integrator,counter,deferred-pulse,sample-phase,input,frame,PCM,event,progress independently sensitive\n");

cleanup:
    for (size_t i = 0u; i < STATE_FLIP_COUNT; i++) instance_destroy(&flips[i]);
    instance_destroy(&trace_machine);
    instance_destroy(&state_base);
    free(variant);
    free(trace);
    av_fixture_free(&fixture);
}

static void case_audio_paths(char **args)
{
    const char *bll_path = args[0];
    const char *labels_path = args[1];
    const char *manifest_path = args[2];
    const size_t edge_count = sizeof av_probe_edges / sizeof av_probe_edges[0];
    av_fixture fixture;
    instance whole = {0}, coarse = {0}, one_tick = {0}, irregular = {0};
    instance watched = {0}, no_input = {0};
    instance interleaved_press = {0}, interleaved_none = {0};
    av_trace *traces[8] = {0};
    av_trace_scratch scratch;
    size_t cursors[8] = {0};
    uint64_t final_tick = 0u;
    int initialized_fixture = 0;

    CHECK_NAMED(av_fixture_load(bll_path, labels_path, manifest_path, &fixture) == 0,
                "audio_paths.manifest-bound-source-fixture");
    if (fixture.bll == NULL) return;
    initialized_fixture = 1;
    for (size_t i = 0u; i < sizeof traces / sizeof traces[0]; i++) {
        traces[i] = calloc(1u, sizeof *traces[i]);
        CHECK(traces[i] != NULL);
        if (traces[i] == NULL) goto cleanup;
    }
    memset(&scratch, 0, sizeof scratch);

    CHECK_EQ_U64(av_instance_make(&whole, &fixture), 0u);
    if (whole.m == NULL) goto cleanup;
    CHECK_EQ_U64(av_trace_to_frame_sequence("audio_paths.whole-source", &whole,
                  traces[0], &scratch, UINT64_C(200000), AV_WHOLE,
                  av_probe_edges, edge_count, &cursors[0], 2u), 0u);
    /* A frame stop returns with the completed frame copied, while a tick
       target is exclusive. Advance one tick after that frame so every other
       path consumes the same boundary event before stream/state comparison. */
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.whole-boundary", &whole,
                  traces[0], &scratch, whole.m->guest_ticks + 1u, AV_WHOLE,
                  av_probe_edges, edge_count, &cursors[0]), 0u);
    final_tick = whole.m->guest_ticks;
    CHECK_NAMED(final_tick > av_probe_edges[0].tick, "audio_paths.common-tick-after-edge");
    CHECK_NAMED(traces[0]->frame_count >= 3u, "audio_paths.raw-frames-nonempty");
    CHECK_NAMED(traces[0]->pcm_count > 0u && traces[0]->event_count > 0u,
                "audio_paths.pcm-and-event-transcripts-nonempty");
    CHECK_NAMED(av_trace_has_nonzero_pcm(traces[0]), "audio_paths.input-audio-nonsilent");
    CHECK_EQ_U64(cursors[0], edge_count);
    const video_packet *release_frame = av_trace_frame(traces[0], 1u);
    const video_packet *pressed_frame = av_trace_frame(traces[0], 2u);
    CHECK_NAMED(release_frame != NULL && release_frame->result.row_count == LYNX_FRAME_MAX_ROWS,
                "audio_paths.release-frame-complete");
    CHECK_NAMED(pressed_frame != NULL && pressed_frame->result.row_count == LYNX_FRAME_MAX_ROWS,
                "audio_paths.input-frame-complete");

    CHECK_EQ_U64(av_instance_make(&coarse, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&one_tick, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&irregular, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&watched, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&no_input, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&interleaved_press, &fixture), 0u);
    CHECK_EQ_U64(av_instance_make(&interleaved_none, &fixture), 0u);
    if (coarse.m == NULL || one_tick.m == NULL || irregular.m == NULL || watched.m == NULL ||
        no_input.m == NULL || interleaved_press.m == NULL || interleaved_none.m == NULL) {
        goto cleanup;
    }
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.coarse", &coarse, traces[1], &scratch,
                  final_tick, AV_COARSE, av_probe_edges, edge_count, &cursors[1]), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.one-tick", &one_tick, traces[2], &scratch,
                  final_tick, AV_ONE_TICK, av_probe_edges, edge_count, &cursors[2]), 0u);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.irregular", &irregular, traces[3], &scratch,
                  final_tick, AV_IRREGULAR, av_probe_edges, edge_count, &cursors[3]), 0u);
    CHECK_EQ_U64(lynx_watch_configure(watched.m, 0x4000u, 1u), LYNX_OK);
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.watched-resumed", &watched, traces[4], &scratch,
                  final_tick, AV_WHOLE, av_probe_edges, edge_count, &cursors[4]), 0u);
    CHECK_NAMED(traces[4]->watch_hits > 0u, "audio_paths.watch-hit-then-resume");
    CHECK_EQ_U64(av_trace_to_tick("audio_paths.no-input", &no_input, traces[5], &scratch,
                  final_tick, AV_WHOLE, NULL, 0u, &cursors[5]), 0u);
    CHECK_NAMED(cursors[1] == edge_count && cursors[2] == edge_count &&
                    cursors[3] == edge_count && cursors[4] == edge_count,
                "audio_paths.host-edge-cursor-outside-snapshot");

    av_trace_equal("audio_paths.coarse-streams", traces[0], traces[1]);
    av_trace_equal("audio_paths.one-tick-streams", traces[0], traces[2]);
    av_trace_equal("audio_paths.irregular-streams", traces[0], traces[3]);
    av_trace_equal("audio_paths.watched-streams", traces[0], traces[4]);
    av_guest_state_equal("audio_paths.coarse-common-state", &whole, &coarse, &fixture);
    av_guest_state_equal("audio_paths.one-tick-common-state", &whole, &one_tick, &fixture);
    av_guest_state_equal("audio_paths.irregular-common-state", &whole, &irregular, &fixture);
    av_guest_state_equal("audio_paths.watched-common-state", &whole, &watched, &fixture);

    CHECK_NAMED(no_input.m->video.input_mask == 0u, "audio_paths.no-input-mask");
    CHECK_NAMED(whole.m->video.input_mask == LYNX_INPUT_A, "audio_paths.press-input-mask");
    CHECK_NAMED(traces[0]->event_count > traces[5]->event_count,
                "audio_paths.input-produces-channel-events");
    CHECK_NAMED(av_trace_has_nonzero_pcm(traces[0]) && !av_trace_has_nonzero_pcm(traces[5]),
                "audio_paths.input-produces-pcm");
    if (pressed_frame != NULL && av_trace_frame(traces[5], 2u) != NULL) {
        CHECK_NAMED(!video_packet_equal(pressed_frame, av_trace_frame(traces[5], 2u)),
                    "audio_paths.input-changes-raw-frame");
    }
    {
        uint8_t marker_pressed = 0u, marker_released = 0u;
        uint8_t probe_pressed = 0u, probe_released = 0u;
        CHECK_EQ_U64(lynx_read_memory(whole.m, 0x4000u, &marker_pressed, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_read_memory(no_input.m, 0x4000u, &marker_released, 1u), LYNX_OK);
        CHECK_EQ_HEX(marker_pressed, 0x22u);
        CHECK_EQ_HEX(marker_released, 0x11u);
        CHECK_EQ_U64(lynx_read_memory(whole.m, fixture.probe_result + 5u,
                                      &probe_pressed, 1u), LYNX_OK);
        CHECK_EQ_U64(lynx_read_memory(no_input.m, fixture.probe_result + 5u,
                                      &probe_released, 1u), LYNX_OK);
        CHECK_NAMED((probe_pressed & 1u) != 0u && probe_released == 0u,
                    "audio_paths.guest-input-marker");
    }

    /* Alternate two independent instances while reusing the same caller-owned
       PCM, event and frame spans; each transcript remains tied to its owner. */
    {
        size_t calls = 0u;
        while ((interleaved_press.m->guest_ticks < final_tick ||
                interleaved_none.m->guest_ticks < final_tick) && calls++ < 2000000u) {
            if (interleaved_press.m->guest_ticks < final_tick) {
                CHECK_EQ_U64(av_trace_step("audio_paths.interleaved-press",
                    &interleaved_press, traces[6], &scratch, final_tick, 257u, AV_WHOLE,
                    av_probe_edges, edge_count, &cursors[6]), 0u);
            }
            if (interleaved_none.m->guest_ticks < final_tick) {
                CHECK_EQ_U64(av_trace_step("audio_paths.interleaved-none",
                    &interleaved_none, traces[7], &scratch, final_tick, 61u, AV_WHOLE,
                    NULL, 0u, &cursors[7]), 0u);
            }
        }
        CHECK(interleaved_press.m->guest_ticks == final_tick &&
              interleaved_none.m->guest_ticks == final_tick);
    }
    CHECK_EQ_U64(cursors[6], edge_count);
    CHECK_EQ_U64(cursors[7], 0u);
    av_trace_equal("audio_paths.interleaved-press-streams", traces[0], traces[6]);
    av_trace_equal("audio_paths.interleaved-none-streams", traces[5], traces[7]);
    av_guest_state_equal("audio_paths.interleaved-press-state", &whole,
                         &interleaved_press, &fixture);
    av_guest_state_equal("audio_paths.interleaved-none-state", &no_input,
                         &interleaved_none, &fixture);

    /* Restore on both sides of the host input edge and immediately before,
       at, and after the 400-tick sample point. The host cursor is copied
       separately; it is never read from the snapshot. */
    CHECK_EQ_U64(av_checkpoint_path("audio_paths.sample-399", &fixture, 399u, 0,
                  final_tick, traces[0], &whole), 0u);
    CHECK_EQ_U64(av_checkpoint_path("audio_paths.sample-400", &fixture, 400u, 0,
                  final_tick, traces[0], &whole), 0u);
    CHECK_EQ_U64(av_checkpoint_path("audio_paths.sample-401", &fixture, 401u, 0,
                  final_tick, traces[0], &whole), 0u);
    CHECK_EQ_U64(av_checkpoint_path("audio_paths.pre-input-edge", &fixture,
                  av_probe_edges[0].tick, 0, final_tick, traces[0], &whole), 0u);
    CHECK_EQ_U64(av_checkpoint_path("audio_paths.post-input-edge", &fixture,
                  av_probe_edges[0].tick, 1, final_tick, traces[0], &whole), 0u);

    CHECK_EQ_U64(av_single_sample_suffix(&fixture), 0u);
    CHECK_EQ_U64(av_single_event_suffix(&fixture, traces[0]), 0u);
    CHECK_EQ_U64(av_capacity_ring_continuation(&fixture), 0u);
    av_zero_suffix_restore(&fixture, &whole, traces[0]);
    printf("audio_paths: tick=%llu frames=%zu pcm=%zu events=%zu variants=whole,coarse,one-tick,irregular,watched,interleaved,restore-edge,sample-boundaries,single-sample,event,zero-suffix\n",
           (unsigned long long)final_tick, traces[0]->frame_count,
           traces[0]->pcm_count, traces[0]->event_count);

cleanup:
    instance_destroy(&whole);
    instance_destroy(&coarse);
    instance_destroy(&one_tick);
    instance_destroy(&irregular);
    instance_destroy(&watched);
    instance_destroy(&no_input);
    instance_destroy(&interleaved_press);
    instance_destroy(&interleaved_none);
    for (size_t i = 0u; i < sizeof traces / sizeof traces[0]; i++) free(traces[i]);
    if (initialized_fixture != 0) av_fixture_free(&fixture);
}

static const struct {
    const char *name;
    int args; /* arguments after the case name */
    void (*run)(char **args);
} cases[] = {
    {"profile_agreement", 2, case_profile_agreement},
    {"suzy_paths", 0, case_suzy_paths},
    {"suzy_sensitivity", 0, case_suzy_sensitivity},
    {"state_sensitivity", 1, case_state_sensitivity},
    {"timer_paths", 0, case_timer_paths},
    {"video_paths", 0, case_video_paths},
    {"video_sensitivity", 0, case_video_sensitivity},
    {"audio_paths", 3, case_audio_paths},
    {"audio_sensitivity", 3, case_audio_sensitivity},
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
