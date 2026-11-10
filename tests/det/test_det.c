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
    return lynx_load_ram_image(in->m, content, len, LOAD_ADDR) == LYNX_OK ? 0 : 1;
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
