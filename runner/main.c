/* lynx-run: the headless runner.

   lynx-run --content FILE.hex --load-addr HEX --ticks N --out DIR
            [--ram-policy zero|splitmix64] [--ram-seed HEX]
            [--profile none|lynx1|lynx2]

   Builds one machine through the public API, loads the hex content at the
   load address, advances it by N master ticks and writes an artifact bundle
   into DIR (see bundle.h). Prints two lines on standard output:

     observation_digest <64 hex> (repeatability (R), not accuracy)
     bundle_digest <64 hex> (repeatability (R), not accuracy)

   Without --ram-policy and --ram-seed the RAM power-on policy is fixed 0x00
   and the seed is 0. Without --profile the profile is none (neither Lynx
   profile is ever a default). A run that stops at an opcode profile none does
   not define still writes its bundle and exits 0; a note on standard error
   names the opcode and the program counter. Exit status: 0 success, 1 runtime refusal (message
   "lynx-run: <reason>" on standard error), 2 usage error. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bundle.h"
#include "lynxplosion.h"

#define EXIT_RUNTIME 1
#define EXIT_USAGE 2

static const char usage_line[] =
    "usage: lynx-run --content FILE.hex --load-addr HEX --ticks N --out DIR "
    "[--ram-policy zero|splitmix64] [--ram-seed HEX] [--profile none|lynx1|lynx2]";

static int usage_error(const char *reason, const char *what)
{
    fprintf(stderr, "lynx-run: %s%s%s\n%s\n", reason, what != NULL ? ": " : "",
            what != NULL ? what : "", usage_line);
    return EXIT_USAGE;
}

static int runtime_error(const char *reason)
{
    fprintf(stderr, "lynx-run: %s\n", reason);
    return EXIT_RUNTIME;
}

/* Decimal digits only, the full uint64_t range, overflow refused. */
static int parse_dec(const char *s, uint64_t *out)
{
    uint64_t v = 0;
    if (*s == '\0') {
        return 1;
    }
    for (; *s != '\0'; s++) {
        unsigned digit;
        if (*s < '0' || *s > '9') {
            return 1;
        }
        digit = (unsigned)(*s - '0');
        if (v > (UINT64_MAX - digit) / 10u) {
            return 1;
        }
        v = v * 10u + digit;
    }
    *out = v;
    return 0;
}

/* 1 to max_digits hex digits, no prefix, no sign. */
static int parse_hex(const char *s, size_t max_digits, uint64_t *out)
{
    uint64_t v = 0;
    size_t n = 0;
    for (; *s != '\0'; s++, n++) {
        unsigned digit;
        if (*s >= '0' && *s <= '9') {
            digit = (unsigned)(*s - '0');
        } else if (*s >= 'a' && *s <= 'f') {
            digit = (unsigned)(*s - 'a') + 10u;
        } else if (*s >= 'A' && *s <= 'F') {
            digit = (unsigned)(*s - 'A') + 10u;
        } else {
            return 1;
        }
        if (n >= max_digits) {
            return 1;
        }
        v = (v << 4) | digit;
    }
    if (n == 0) {
        return 1;
    }
    *out = v;
    return 0;
}

/* Creates one directory level; an existing directory is reused. */
static int make_dir(const char *path)
{
    struct stat st;
    if (mkdir(path, 0777) == 0) {
        return 0;
    }
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    return 1;
}

/* The core holds no message text: the note is built here from public reads
   only (the program counter and the byte at it, which is the opcode the stop
   is before). A failed read prints nothing; the stop itself is in the bundle. */
static void undefined_note(const lynx_machine *m)
{
    uint32_t pc = 0;
    uint8_t opcode = 0;
    const char *name = lynx_profile_name(LYNX_PROFILE_NONE);
    lynx_identity id;
    if (lynx_read_register(m, LYNX_REG_PC, &pc) != LYNX_OK ||
        lynx_read_memory(m, pc, &opcode, 1u) != LYNX_OK) {
        return;
    }
    if (lynx_get_identity(m, &id) == LYNX_OK && lynx_profile_name(id.profile) != NULL) {
        name = lynx_profile_name(id.profile);
    }
    fprintf(stderr,
            "lynx-run: stopped at undefined opcode $%02X at PC $%04X under profile %s; "
            "the two Lynx profiles disagree here; "
            "select one with --profile lynx1 or --profile lynx2\n",
            (unsigned)opcode, (unsigned)pc, name);
}

int main(int argc, char **argv)
{
    static const char *const flags[] = {"--content",    "--load-addr",  "--ticks",
                                        "--out",        "--ram-policy", "--ram-seed",
                                        "--profile"};
    enum { F_CONTENT, F_LOAD, F_TICKS, F_OUT, F_POLICY, F_SEED, F_PROFILE, F_COUNT };
    const char *value[F_COUNT] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    uint64_t ticks = 0;
    uint64_t load_addr = 0;
    uint64_t seed = 0;
    lynx_config cfg;
    bundle_run run;
    uint8_t *content = NULL;
    size_t content_len = 0;
    void *mem = NULL;
    lynx_machine *m = NULL;
    char obs_hex[65];
    char bundle_hex[65];
    int rc;
    int i;

    for (i = 1; i < argc; i += 2) {
        int f;
        for (f = 0; f < F_COUNT; f++) {
            if (strcmp(argv[i], flags[f]) == 0) {
                break;
            }
        }
        if (f == F_COUNT) {
            return usage_error("unknown argument", argv[i]);
        }
        if (i + 1 >= argc) {
            return usage_error("missing value for", argv[i]);
        }
        if (value[f] != NULL) {
            return usage_error("repeated flag", argv[i]);
        }
        value[f] = argv[i + 1];
    }
    if (value[F_CONTENT] == NULL || value[F_LOAD] == NULL || value[F_TICKS] == NULL ||
        value[F_OUT] == NULL) {
        return usage_error("--content, --load-addr, --ticks and --out are required", NULL);
    }
    if (parse_dec(value[F_TICKS], &ticks) != 0) {
        return usage_error("--ticks must be a decimal number from 0 to 18446744073709551615",
                           value[F_TICKS]);
    }
    if (parse_hex(value[F_LOAD], 8, &load_addr) != 0) {
        return usage_error("--load-addr must be 1 to 8 hex digits", value[F_LOAD]);
    }
    lynx_config_default(&cfg);
    if (value[F_POLICY] != NULL) {
        if (strcmp(value[F_POLICY], "zero") == 0) {
            cfg.ram_policy = LYNX_RAM_POLICY_ZERO;
        } else if (strcmp(value[F_POLICY], "splitmix64") == 0) {
            cfg.ram_policy = LYNX_RAM_POLICY_SPLITMIX64;
        } else {
            return usage_error("--ram-policy must be zero or splitmix64", value[F_POLICY]);
        }
    }
    if (value[F_PROFILE] != NULL) {
        /* A closed set: anything else is a usage error and the default stays none. */
        if (strcmp(value[F_PROFILE], "none") == 0) {
            cfg.profile = LYNX_PROFILE_NONE;
        } else if (strcmp(value[F_PROFILE], "lynx1") == 0) {
            cfg.profile = LYNX_PROFILE_LYNX1;
        } else if (strcmp(value[F_PROFILE], "lynx2") == 0) {
            cfg.profile = LYNX_PROFILE_LYNX2;
        } else {
            return usage_error("--profile must be none, lynx1 or lynx2", value[F_PROFILE]);
        }
    }
    if (value[F_SEED] != NULL) {
        if (parse_hex(value[F_SEED], 16, &seed) != 0) {
            return usage_error("--ram-seed must be 1 to 16 hex digits", value[F_SEED]);
        }
        cfg.ram_seed = seed;
    }

    /* Everything below is a runtime outcome: one exit path frees what is held. */
    rc = EXIT_RUNTIME;
    if (bundle_read_hex(value[F_CONTENT], &content, &content_len) != 0) {
        runtime_error("cannot read content as hex text");
        goto done;
    }
    mem = malloc(lynx_machine_size());
    if (mem == NULL || (size_t)((uintptr_t)mem % lynx_machine_align()) != 0) {
        runtime_error("cannot allocate a machine instance");
        goto done;
    }
    if (lynx_machine_init(mem, lynx_machine_size(), &cfg, &m) != LYNX_OK) {
        runtime_error("the core refused the configuration");
        goto done;
    }
    if (load_addr > UINT32_MAX ||
        lynx_load_ram_image(m, content, content_len, (uint32_t)load_addr) != LYNX_OK) {
        runtime_error("the core refused the content load");
        goto done;
    }

    run.content_name = value[F_CONTENT];
    run.tick_budget = ticks;
    run.ticks_consumed = 0;
    run.stop = LYNX_STOP_BUDGET;
    if (lynx_advance(m, ticks, &run.ticks_consumed, &run.stop) != LYNX_OK) {
        runtime_error("the core refused to advance");
        goto done;
    }
    if (run.stop == LYNX_STOP_UNDEFINED) {
        undefined_note(m);
    }
    if (make_dir(value[F_OUT]) != 0) {
        runtime_error("cannot create the output directory");
        goto done;
    }
    if (bundle_write(value[F_OUT], m, &run, obs_hex, bundle_hex) != 0) {
        runtime_error("cannot write the artifact bundle");
        goto done;
    }
    if (printf("observation_digest %s (repeatability (R), not accuracy)\n"
               "bundle_digest %s (repeatability (R), not accuracy)\n",
               obs_hex, bundle_hex) < 0 ||
        fflush(stdout) != 0) {
        runtime_error("cannot write to standard output");
        goto done;
    }
    rc = 0;
done:
    if (m != NULL) {
        lynx_machine_deinit(m);
    }
    free(mem);
    free(content);
    return rc;
}
