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

static int instance_make(instance *in, const uint8_t *content, size_t len)
{
    lynx_config cfg;
    lynx_config_default(&cfg);
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

/* A digest that ignores state would make every equality test vacuous. One bit
   of a register, of guest time and of a RAM byte must each change it; an
   unmodified copy must not. */
static void case_state_sensitivity(char **args)
{
    uint8_t *content = NULL;
    size_t content_len = 0;
    instance base;
    lynx_machine *copy[4];
    char want[65];
    char got[65];
    size_t i;
    static const char *const name[4] = {"unmodified copy", "register a", "guest time",
                                        "ram byte 0x0100"};

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

    for (i = 0; i < 4; i++) {
        copy[i] = copy_machine(base.m);
        CHECK(copy[i] != NULL);
        if (copy[i] == NULL) {
            continue;
        }
        if (i == 1) {
            copy[i]->a ^= 1u;
        } else if (i == 2) {
            copy[i]->guest_ticks ^= 1u;
        } else if (i == 3) {
            copy[i]->ram[0x0100u] ^= 1u;
        }
        CHECK_EQ_U64(digest_of(copy[i], got), 0);
        if (i == 0) {
            if (strcmp(got, want) != 0) {
                fprintf(stderr, "sensitivity_state: %s: digest changed without a change\n",
                        name[i]);
                check_failures++;
            }
        } else if (strcmp(got, want) == 0) {
            fprintf(stderr, "sensitivity_state: %s: digest unchanged\n", name[i]);
            check_failures++;
        } else {
            printf("sensitivity_state: %s: digest differs (%s)\n", name[i], got);
        }
        free(copy[i]);
    }
    printf("sensitivity_state: base digest %s\n", want);
    free(base.mem);
}

static const struct {
    const char *name;
    int args; /* arguments after the case name */
    void (*run)(char **args);
} cases[] = {
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
