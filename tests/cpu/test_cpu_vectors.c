/* lynx-test-cpu-vectors: the per-opcode vector runner and the reader self-test.

   Subcommands:
     parse-check FILE        parse a sample container strictly and print its counts
     self-test-reader        feed malformed inputs to the reader and check each rejection
     run --profile none|lynx1|lynx2 --sample FILE --manifest FILE --expect-plan FILE
         check the sample hash, derive the plan from the sample and the profile
         table, compare it byte for byte with the committed inventory, then run
         every non-excluded vector through the same CPU the machine uses
     run --profile P --sample FILE --print-plan
         print the derived plan (used once to generate the committed inventories)
     complete --profile P --dir DIR --variant V
         run every vector of the 256 whole upstream files DIR/V/v1/<op>.json (10,000
         per file) through the same CPU, with the exclusions and, under none, the
         undefined rows that the profile table gives; no committed plan applies
     Options of run and complete: --results FILE (one line per run row, op, passed,
     failed, ascending), --partial (sample: any ascending subset of sections;
     complete: files that are absent are skipped, at least one must exist) and
     --fault no-step|stray-write|mutant-69 (named faults used only by the controls).
     seam-agreement --profile none|lynx1|lynx2 --sample FILE --manifest FILE
         the machine against the vector seam: for every run row the first vector of
         the opcode is written into a format-2 snapshot (hash recomputed), restored
         into a machine through the public restore call and advanced by the seam's
         cycle count at 5 ticks; registers and the whole RAM must equal the internal
         seam step. Under none every vector of the undefined rows must stop with
         UNDEFINED, nothing consumed and the instance byte-identical.

   Asserted per vector: final pc, s, a, x, y, p under mask 0xCF, every listed final
   ram value, and no write outside the union of the initial and final ram address
   lists. Not asserted: cycle count, bus trace, unlisted reads, ticks. Unlisted
   reads return an address-hashed poison, never a constant.

   Exit codes: 0 pass, 1 assertion failure, 2 usage, 3 data or parse error, 4 inventory
   mismatch. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "cpu/flat_bus.h"
#include "cpu/sst_reader.h"
#include "internal.h"
#include "lynxplosion.h"
#include "sha256.h"

enum { EXIT_PASS = 0, EXIT_ASSERT = 1, EXIT_USAGE = 2, EXIT_DATA = 3, EXIT_INVENTORY = 4 };

/* Named faults the controls inject; each is built here at run time, never committed. */
enum { FAULT_NONE = 0, FAULT_NO_STEP = 1, FAULT_STRAY_WRITE = 2, FAULT_MUTANT_69 = 3 };

/* ---- files --------------------------------------------------------------- */

/* Reads a whole file into a fresh buffer; NULL after printing why on failure. */
static char *read_whole_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    size_t got;
    if (f == NULL) {
        fprintf(stderr, "lynx-test-cpu-vectors: cannot open %s\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "lynx-test-cpu-vectors: cannot size %s\n", path);
        (void)fclose(f);
        return NULL;
    }
    buf = malloc((size_t)n + 1u);
    if (buf == NULL) {
        fprintf(stderr, "lynx-test-cpu-vectors: out of memory reading %s\n", path);
        (void)fclose(f);
        return NULL;
    }
    got = fread(buf, 1u, (size_t)n, f);
    (void)fclose(f);
    if (got != (size_t)n) {
        fprintf(stderr, "lynx-test-cpu-vectors: short read of %s\n", path);
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    *len_out = (size_t)n;
    return buf;
}

/* ---- parse-check --------------------------------------------------------- */

static int cmd_parse_check(const char *path)
{
    size_t len = 0;
    char *buf = read_whole_file(path, &len);
    static sst_reader r;
    static sst_vector v;
    int rc;
    if (buf == NULL) {
        return EXIT_DATA;
    }
    if (sst_open_sample(&r, buf, len, path, 0) != 0) {
        fprintf(stderr, "%s\n", r.msg);
        free(buf);
        return EXIT_DATA;
    }
    while ((rc = sst_next(&r, &v)) > 0) {
    }
    if (rc < 0) {
        fprintf(stderr, "%s\n", r.msg);
        free(buf);
        return EXIT_DATA;
    }
    printf("sections %u vectors %u\n", r.sections, r.total);
    free(buf);
    return EXIT_PASS;
}

/* ---- reader self-test ---------------------------------------------------- */


/* snprintf with the length returned; the buffers here are sized for their text. */
static size_t spf(char *dst, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static size_t spf(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap) {
        fprintf(stderr, "self-test: buffer too small\n");
        exit(EXIT_USAGE);
    }
    return (size_t)n;
}

/* One valid object for opcode ea; the cases below change one thing in it. */
static const char k_obj[] =
    "{ \"name\": \"ea 00\", \"initial\": { \"pc\": 4096, \"s\": 253, \"a\": 1, \"x\": 2, \"y\": 3, \"p\": 36, "
    "\"ram\": [ [4096, 234]]}, \"final\": { \"pc\": 4097, \"s\": 253, \"a\": 1, \"x\": 2, \"y\": 3, \"p\": 36, "
    "\"ram\": [ [4096, 234]]}, \"cycles\": [ [4096, 234, \"read\"], [4097, 0, \"read\"]]}";

/* Big enough for 256 sections of 32 objects and for one 10000-object file. */
static char g_buf[4u * 1024u * 1024u];
static char g_obj[4096];

static const char k_sample_head[] =
    "#sst65x02-sample v1\n"
    "#source https://github.com/SingleStepTests/65x02 0123456789abcdef0123456789abcdef01234567 synthetic65c02/v1\n"
    "#rule first 32 objects of each file, upstream bytes verbatim, files in opcode order\n";

/* Writes the first occurrence of from replaced by to into dst; from must exist. */
static int replace_first(char *dst, size_t cap, const char *src, const char *from, const char *to)
{
    const char *at = strstr(src, from);
    size_t pre;
    if (at == NULL) {
        fprintf(stderr, "self-test: pattern not found: %s\n", from);
        return -1;
    }
    pre = (size_t)(at - src);
    if (pre + strlen(to) + strlen(at + strlen(from)) + 1u > cap) {
        return -1;
    }
    memcpy(dst, src, pre);
    memcpy(dst + pre, to, strlen(to));
    strcpy(dst + pre + strlen(to), at + strlen(from));
    return 0;
}

/* Section text for opcode op: header, "[", 32 objects with the first one given. */
static size_t put_section(char *dst, size_t pos, unsigned op, const char *header_lines, const char *first,
                          const char *other)
{
    unsigned i;
    pos += spf(dst + pos, sizeof g_buf - pos, "#file %02x.json lines %s\n[\n", op, header_lines);
    for (i = 0; i < 32u; i++) {
        const char *o = i == 0u ? first : other;
        pos += spf(dst + pos, sizeof g_buf - pos, "%s,\n", o);
    }
    return pos;
}

static int g_selftest_failures;

/* The reader must stop with an error whose message contains want. */
static void expect_reject(const char *name, const char *buf, size_t len, int partial, const char *want)
{
    static sst_reader r;
    static sst_vector v;
    int rc;
    if (sst_open_sample(&r, buf, len, name, partial) != 0) {
        rc = -1;
    } else {
        while ((rc = sst_next(&r, &v)) > 0) {
        }
    }
    if (rc >= 0) {
        fprintf(stderr, "FAIL %s: accepted (%u vectors)\n", name, r.total);
        g_selftest_failures++;
        return;
    }
    if (strstr(r.msg, want) == NULL) {
        fprintf(stderr, "FAIL %s: message '%s' lacks '%s'\n", name, r.msg, want);
        g_selftest_failures++;
        return;
    }
    printf("ok %s: %s\n", name, r.msg);
}

/* One partial-mode section of opcode ea whose first object is changed. */
static void case_object(const char *name, const char *from, const char *to, const char *want)
{
    size_t len;
    if (replace_first(g_obj, sizeof g_obj, k_obj, from, to) != 0) {
        g_selftest_failures++;
        return;
    }
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len = put_section(g_buf, len, 0xEAu, "33", g_obj, k_obj);
    expect_reject(name, g_buf, len, 1, want);
}

static void case_ram_overflow(void)
{
    char big[1200];
    size_t n = 0;
    unsigned i;
    size_t len;
    n += spf(big + n, sizeof big - n, "\"ram\": [ [4096, 234]");
    for (i = 0; i < 64u; i++) {
        n += spf(big + n, sizeof big - n, ", [%u, 1]", 100u + i);
    }
    n += spf(big + n, sizeof big - n, "]}, \"final\"");
    if (replace_first(g_obj, sizeof g_obj, k_obj, "\"ram\": [ [4096, 234]]}, \"final\"", big) != 0) {
        g_selftest_failures++;
        return;
    }
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len = put_section(g_buf, len, 0xEAu, "33", g_obj, k_obj);
    expect_reject("65 ram entries", g_buf, len, 1, "more than 64 ram entries");
}

static void case_cycle_overflow(void)
{
    char big[1200];
    size_t n = 0;
    unsigned i;
    size_t len;
    n += spf(big + n, sizeof big - n, "\"cycles\": [ [4096, 234, \"read\"]");
    for (i = 0; i < 16u; i++) {
        n += spf(big + n, sizeof big - n, ", [%u, 1, \"read\"]", 100u + i);
    }
    n += spf(big + n, sizeof big - n, "]}");
    if (replace_first(g_obj, sizeof g_obj, k_obj, "\"cycles\": [ [4096, 234, \"read\"], [4097, 0, \"read\"]]}", big) !=
        0) {
        g_selftest_failures++;
        return;
    }
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len = put_section(g_buf, len, 0xEAu, "33", g_obj, k_obj);
    expect_reject("17 cycles", g_buf, len, 1, "more than 16 cycles");
}

static void case_header_lines(void)
{
    size_t len;
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len = put_section(g_buf, len, 0xEAu, "34", k_obj, k_obj);
    expect_reject("section shorter than its lines header", g_buf, len, 1, "shorter than its 'lines' header");
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len = put_section(g_buf, len, 0xEAu, "32", k_obj, k_obj);
    expect_reject("section longer than its lines header", g_buf, len, 1, "different object count than 32: 31");
    len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    len += spf(g_buf + len, sizeof g_buf - len, "#file ea.json lines 1\n[\n");
    expect_reject("section with zero vectors", g_buf, len, 1, "no vectors after '['");
}

static size_t put_all_sections(void)
{
    unsigned op;
    size_t len = spf(g_buf, sizeof g_buf, "%s", k_sample_head);
    char tmp[4096];
    for (op = 0; op < 256u; op++) {
        char name[8];
        (void)spf(name, sizeof name, "\"%02x 00\"", op);
        {
            char byte[8];
            char from[40];
            char to[40];
            (void)spf(byte, sizeof byte, "%u", op);
            (void)spf(from, sizeof from, "\"ea 00\"");
            if (replace_first(tmp, sizeof tmp, k_obj, from, name) != 0) {
                return 0;
            }
            (void)spf(from, sizeof from, "[ [4096, 234]]}, \"final\"");
            (void)spf(to, sizeof to, "[ [4096, %u]]}, \"final\"", op);
            if (replace_first(g_obj, sizeof g_obj, tmp, from, to) != 0) {
                return 0;
            }
        }
        len = put_section(g_buf, len, op, "33", g_obj, g_obj);
    }
    return len;
}

static void case_container(void)
{
    static sst_reader r;
    static sst_vector v;
    size_t len = put_all_sections();
    int rc;
    if (len == 0u) {
        g_selftest_failures++;
        return;
    }
    if (sst_open_sample(&r, g_buf, len, "synthetic", 0) != 0) {
        fprintf(stderr, "FAIL synthetic container: %s\n", r.msg);
        g_selftest_failures++;
        return;
    }
    while ((rc = sst_next(&r, &v)) > 0) {
    }
    if (rc < 0 || r.sections != 256u || r.total != 8192u) {
        fprintf(stderr, "FAIL synthetic container: rc %d sections %u vectors %u\n", rc, r.sections, r.total);
        g_selftest_failures++;
    } else {
        printf("ok synthetic container: sections 256 vectors 8192\n");
    }
    memcpy(g_buf + len, "x\n", 3u);
    expect_reject("trailing bytes after the last section", g_buf, len + 2u, 0, "trailing bytes after the last section");
    expect_reject("truncated sample", g_buf, len - 1u, 0, "shorter than its 'lines' header");
}

/* One complete-mode file: count objects of the valid object, closing bracket. */
static void case_file(unsigned count, int expect_ok)
{
    static sst_reader r;
    static sst_vector v;
    size_t len = 0;
    unsigned i;
    int rc;
    len += spf(g_buf + len, sizeof g_buf - len, "[\n");
    for (i = 0; i < count; i++) {
        len += spf(g_buf + len, sizeof g_buf - len, "%s%s", k_obj, i + 1u == count ? "\n" : ",\n");
    }
    len += spf(g_buf + len, sizeof g_buf - len, "]");
    sst_open_file(&r, g_buf, len, 0xEAu, "synthetic file");
    while ((rc = sst_next(&r, &v)) > 0) {
    }
    if (expect_ok != 0) {
        if (rc != 0 || r.total != count) {
            fprintf(stderr, "FAIL complete file of %u: rc %d (%s)\n", count, rc, r.msg);
            g_selftest_failures++;
        } else {
            printf("ok complete file of %u objects accepted\n", count);
        }
    } else if (rc >= 0 || strstr(r.msg, "different object count than 10000") == NULL) {
        fprintf(stderr, "FAIL complete file of %u accepted or wrong message '%s'\n", count, r.msg);
        g_selftest_failures++;
    } else {
        printf("ok complete file of %u objects rejected: %s\n", count, r.msg);
    }
}

static int cmd_self_test_reader(void)
{
    case_object("truncated object", "[4097, 0, \"read\"]]}", "[4097, 0, \"read\"]]", "opcode ea vector 0");
    case_object("unknown key", "\"final\"", "\"finale\"", "unknown, missing or duplicate key, expected final");
    case_object("duplicate key", "\"pc\": 4096, \"s\"", "\"pc\": 4096, \"pc\": 4096, \"s\"",
                "unknown, missing or duplicate key, expected s");
    case_object("missing final",
                "\"final\": { \"pc\": 4097, \"s\": 253, \"a\": 1, \"x\": 2, \"y\": 3, \"p\": 36, \"ram\": [ [4096, 234]]}, ",
                "", "expected final");
    case_object("pc 70000", "\"pc\": 4096", "\"pc\": 70000", "number out of range for pc");
    case_object("register 256", "\"a\": 1,", "\"a\": 256,", "number out of range for a");
    case_object("negative number", "\"x\": 2,", "\"x\": -2,", "expected unsigned number for x");
    case_object("leading zero", "\"y\": 3,", "\"y\": 03,", "leading zero in y");
    case_object("ram byte at pc not the opcode", "[ [4096, 234]]}, \"final\"", "[ [4096, 235]]}, \"final\"",
                "initial.ram byte at pc is not the file opcode");
    case_object("ram without a byte at pc", "[ [4096, 234]]}, \"final\"", "[ [4095, 234]]}, \"final\"",
                "initial.ram has no byte at pc");
    case_object("name token is not the opcode", "\"ea 00\"", "\"eb 00\"", "first token is not the file opcode");
    case_object("duplicate initial.ram address", "[ [4096, 234]]}, \"final\"", "[ [4096, 234], [4096, 234]]}, \"final\"",
                "duplicate initial.ram address");
    case_object("direction other than read or write", "[4097, 0, \"read\"]", "[4097, 0, \"peek\"]",
                "cycle direction is neither");
    case_object("object split across lines", "\"initial\": {", "\"initial\":\n{", "opcode ea");
    case_ram_overflow();
    case_cycle_overflow();
    case_header_lines();
    case_container();
    case_file(10000u, 1);
    case_file(9999u, 0);
    if (g_selftest_failures != 0) {
        fprintf(stderr, "self-test-reader: %d failures\n", g_selftest_failures);
        return EXIT_ASSERT;
    }
    printf("self-test-reader: all rejections confirmed\n");
    return EXIT_PASS;
}

/* ---- run: identity, plan, execution -------------------------------------- */

enum { ST_RUN = 0, ST_EXCLUDED = 1, ST_UNDEFINED = 2 };

typedef struct rowinfo {
    int status;
    const char *tier;
    const char *reason;
} rowinfo;

/* Status, tier and reason of one opcode, derived only from the profile table
   (its vec and flag columns) and, under none, from the defined-under-none rule. */
static rowinfo classify(unsigned profile, unsigned op, const lynx__cpu_decode *t)
{
    rowinfo ri;
    const lynx__cpu_decode *row = &t[op];
    ri.status = ST_RUN;
    ri.tier = row->vec == LYNX__V_TIER_STATE ? "state" : "full";
    ri.reason = "-";
    if (profile == 0u && !lynx__cpu_opcode_defined(0u, (uint8_t)op)) {
        ri.status = ST_UNDEFINED;
        ri.tier = "-";
        ri.reason = "profile-none-stops-undefined";
        return ri;
    }
    if (row->vec == LYNX__V_EXCL_HW) {
        ri.status = ST_EXCLUDED;
        ri.tier = "-";
        ri.reason = "source-disagrees-with-hardware-informed-test";
    } else if ((row->flags & LYNX__F_UNV_SHAPE) != 0u) {
        ri.reason = "shape-unsourced-for-lynx1";
    } else if ((row->flags & LYNX__F_DEC_EXTRA) != 0u) {
        ri.reason = "cycles-unsourced-decimal";
    } else if (row->vec == LYNX__V_TIER_STATE) {
        ri.reason = "cycles-conflict-sources";
    }
    return ri;
}

static uint8_t poison(uint16_t a)
{
    return (uint8_t)(((uint32_t)a * 167u + 13u) >> 3);
}

/* The derived plan. */
static char g_plan[65536];
static size_t g_planlen;

static void plan_add(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void plan_add(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(g_plan + g_planlen, sizeof g_plan - g_planlen, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof g_plan - g_planlen) {
        fprintf(stderr, "lynx-test-cpu-vectors: plan buffer too small\n");
        exit(EXIT_DATA);
    }
    g_planlen += (size_t)n;
}

typedef struct run_opts {
    const char *profile_name;
    unsigned profile;
    const char *sample;
    const char *manifest;
    const char *expect_plan;
    const char *dir;     /* complete: directory holding <variant>/v1/<op>.json */
    const char *variant; /* complete: synertek65c02 or rockwell65c02 */
    const char *results; /* optional results file */
    int print_plan;
    int partial;
    int fault;
} run_opts;

/* Per-opcode counts from the parse pass. */
typedef struct counts {
    unsigned vectors[256];
    unsigned decimal[256];
} counts;

static counts g_counts;
static sst_reader g_reader;
static sst_vector g_vec;
static flat_bus g_bus;

/* The expected sample hash: the manifest's "sha256 = " line. 0 on success. */
static int manifest_sha256(const char *path, char out[65])
{
    size_t len = 0;
    char *buf = read_whole_file(path, &len);
    const char *p;
    size_t i;
    if (buf == NULL) {
        return -1;
    }
    p = buf;
    while (p != NULL && *p != '\0') {
        if (strncmp(p, "sha256 = ", 9u) == 0) {
            for (i = 0; i < 64u; i++) {
                char c = p[9u + i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                    fprintf(stderr, "lynx-test-cpu-vectors: %s: the sha256 value is not 64 lowercase hex digits\n", path);
                    free(buf);
                    return -1;
                }
                out[i] = c;
            }
            out[64] = '\0';
            if (p[73] != '\n' && p[73] != '\0') {
                fprintf(stderr, "lynx-test-cpu-vectors: %s: trailing text after the sha256 value\n", path);
                free(buf);
                return -1;
            }
            free(buf);
            return 0;
        }
        p = strchr(p, '\n');
        if (p != NULL) {
            p++;
        }
    }
    fprintf(stderr, "lynx-test-cpu-vectors: %s: no 'sha256 = ' line\n", path);
    free(buf);
    return -1;
}

/* Parses the whole sample once, filling the per-opcode counts. */
static int parse_pass(const char *buf, size_t len, const char *label, unsigned profile, int partial)
{
    const lynx__cpu_decode *t = lynx__cpu_table(profile);
    int rc;
    memset(&g_counts, 0, sizeof g_counts);
    if (sst_open_sample(&g_reader, buf, len, label, partial) != 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        return -1;
    }
    while ((rc = sst_next(&g_reader, &g_vec)) > 0) {
        unsigned op = g_vec.opcode;
        g_counts.vectors[op]++;
        if ((t[op].kind == LYNX__K_ADC || t[op].kind == LYNX__K_SBC) && (g_vec.initial.p & 8u) != 0u) {
            g_counts.decimal[op]++;
        }
    }
    if (rc < 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        return -1;
    }
    if ((partial == 0 && g_reader.sections != 256u) || g_reader.total == 0u) {
        fprintf(stderr, "%s: parsed %u sections and %u vectors, expected 256 sections\n", label, g_reader.sections,
                g_reader.total);
        return -1;
    }
    return 0;
}

/* Totals shared by the plan and the report. */
typedef struct totals {
    unsigned run, excluded, undefined;
    unsigned vectors, asserted, excluded_vectors, undefined_vectors;
    unsigned unv_cycles_rows, unv_cycles_vectors, unv_shape_rows, unv_shape_vectors;
} totals;

static totals g_totals;

static void build_plan(const run_opts *o, const char *sample_hex)
{
    const lynx__cpu_decode *t = lynx__cpu_table(o->profile);
    unsigned op;
    memset(&g_totals, 0, sizeof g_totals);
    g_planlen = 0;
    plan_add("# vector inventory: op<TAB>status<TAB>tier<TAB>vectors<TAB>decimal<TAB>reason\n");
    plan_add("# profile = %s\n", o->profile_name);
    plan_add("# source = SingleStepTests/65x02 %s %s%s\n", g_reader.commit, g_reader.variant,
             o->profile == 0u ? " (same data as lynx1; rows defined only where lynx1 and lynx2 agree)" : "");
    if (o->partial != 0) {
        plan_add("# sample = partial, first 32 objects of each of %u files, upstream bytes verbatim\n",
                 g_reader.sections);
    } else {
        plan_add("# sample = first 32 objects of each of 256 files, upstream bytes verbatim\n");
    }
    plan_add("# sample_sha256 = %s\n", sample_hex);
    plan_add("# asserted = pc s a x y, p & 0xcf, final listed ram, no write outside listed addresses\n");
    plan_add("# not_asserted = cycle count, bus trace, unlisted reads, ticks\n");
    plan_add("# not_covered = irq/nmi/reset entry, cli/sei latency, jmp (abs) at xxff, lynx memory map, hardware "
             "timing\n");
    plan_add("# evidence_tier = T1 (software-model vectors, not hardware)\n");
    plan_add("# poison = hash167 ((a*167+13)>>3)\n");
    plan_add("# tier = label only: full and state rows assert the same fields\n");
    for (op = 0; op < 256u; op++) {
        rowinfo ri = classify(o->profile, op, t);
        unsigned n = g_counts.vectors[op];
        if (o->partial != 0 && n == 0u) {
            continue; /* a partial sample has no section for this opcode */
        }
        g_totals.vectors += n;
        if (ri.status == ST_RUN) {
            g_totals.run++;
            g_totals.asserted += n;
            if ((t[op].flags & LYNX__F_UNV_CYCLES) != 0u) {
                g_totals.unv_cycles_rows++;
                g_totals.unv_cycles_vectors += n;
            }
            if ((t[op].flags & LYNX__F_UNV_SHAPE) != 0u) {
                g_totals.unv_shape_rows++;
                g_totals.unv_shape_vectors += n;
            }
        } else if (ri.status == ST_EXCLUDED) {
            g_totals.excluded++;
            g_totals.excluded_vectors += n;
        } else {
            g_totals.undefined++;
            g_totals.undefined_vectors += n;
        }
        plan_add("%02x\t%s\t%s\t%u\t%u\t%s\n", op,
                 ri.status == ST_RUN ? "run" : (ri.status == ST_EXCLUDED ? "excluded" : "undefined"), ri.tier, n,
                 g_counts.decimal[op], ri.reason);
    }
    if (o->profile == 0u) {
        plan_add("# totals = files %u run %u excluded %u undefined %u vectors %u asserted %u excluded_vectors %u "
                 "undefined_vectors %u\n",
                 g_reader.sections, g_totals.run, g_totals.excluded, g_totals.undefined, g_totals.vectors, g_totals.asserted,
                 g_totals.excluded_vectors, g_totals.undefined_vectors);
    } else {
        plan_add("# totals = files %u run %u excluded %u vectors %u asserted %u excluded_vectors %u\n",
                 g_reader.sections, g_totals.run, g_totals.excluded, g_totals.vectors, g_totals.asserted, g_totals.excluded_vectors);
    }
}

/* Compares the derived plan with the committed file byte for byte. 0 equal, -1
   after naming the first differing line. */
static int compare_plan(const char *path)
{
    size_t len = 0;
    char *want = read_whole_file(path, &len);
    size_t i = 0;
    size_t line_start = 0;
    unsigned line = 1;
    size_t n = len < g_planlen ? len : g_planlen;
    if (want == NULL) {
        return -2;
    }
    while (i < n && want[i] == g_plan[i]) {
        if (want[i] == '\n') {
            line++;
            line_start = i + 1u;
        }
        i++;
    }
    if (i == len && i == g_planlen) {
        free(want);
        return 0;
    }
    {
        size_t we = line_start;
        size_t ge = line_start;
        while (we < len && want[we] != '\n') {
            we++;
        }
        while (ge < g_planlen && g_plan[ge] != '\n') {
            ge++;
        }
        fprintf(stderr, "inventory mismatch: first difference at line %u\n  expected: %.*s\n  derived:  %.*s\n", line,
                (int)(we - line_start), want + line_start, (int)(ge - line_start), g_plan + line_start);
    }
    free(want);
    return -1;
}

static int addr_listed(const sst_state *a, const sst_state *b, uint16_t addr)
{
    unsigned i;
    for (i = 0; i < a->nram; i++) {
        if (a->ram_addr[i] == addr) {
            return 1;
        }
    }
    for (i = 0; i < b->nram; i++) {
        if (b->ram_addr[i] == addr) {
            return 1;
        }
    }
    return 0;
}

/* Final state equals the initial state apart from pc. */
static int is_identity(const sst_vector *v)
{
    unsigned i;
    unsigned j;
    if (v->final.s != v->initial.s || v->final.a != v->initial.a || v->final.x != v->initial.x ||
        v->final.y != v->initial.y || v->final.p != v->initial.p) {
        return 0;
    }
    for (i = 0; i < v->final.nram; i++) {
        int same = 0;
        for (j = 0; j < v->initial.nram; j++) {
            if (v->initial.ram_addr[j] == v->final.ram_addr[i] && v->initial.ram_val[j] == v->final.ram_val[i]) {
                same = 1;
            }
        }
        if (same == 0) {
            return 0;
        }
    }
    return 1;
}

typedef struct results {
    unsigned asserted, passed, failed, identity, unmoved;
    unsigned undefined_checked, undefined_stopped;
    unsigned printed;
    unsigned op_passed[256], op_failed[256];
} results;

static results g_res;
static rowinfo g_info[256];
static int g_fault;
static int g_stray_done;

static void fail_field(const sst_vector *v, const char *field, unsigned got, unsigned want)
{
    if (g_res.printed < 40u) {
        fprintf(stderr, "FAIL opcode %02x vector %u %s: got %u expected %u\n", v->opcode, v->ordinal, field, got,
                want);
    } else if (g_res.printed == 40u) {
        fprintf(stderr, "FAIL (further failures not printed)\n");
    }
    g_res.printed++;
}

static void load_initial(const sst_vector *v, lynx__cpu *cpu, const lynx__cpu_decode *t, unsigned profile)
{
    unsigned i;
    for (i = 0; i < v->initial.nram; i++) {
        g_bus.mem[v->initial.ram_addr[i]] = v->initial.ram_val[i];
    }
    lynx__cpu_init(cpu, profile);
    cpu->a = v->initial.a;
    cpu->x = v->initial.x;
    cpu->y = v->initial.y;
    cpu->s = v->initial.s;
    cpu->p = (uint8_t)(v->initial.p | 0x30u);
    lynx__cpu_start_at(cpu, t, v->initial.pc);
    flat_bus_reset_log(&g_bus);
}

static void restore_touched(const sst_vector *v)
{
    unsigned i;
    for (i = 0; i < v->initial.nram; i++) {
        g_bus.mem[v->initial.ram_addr[i]] = poison(v->initial.ram_addr[i]);
    }
    for (i = 0; i < v->final.nram; i++) {
        g_bus.mem[v->final.ram_addr[i]] = poison(v->final.ram_addr[i]);
    }
    for (i = 0; i < g_bus.nlog; i++) {
        if (g_bus.log[i].write != 0u) {
            g_bus.mem[g_bus.log[i].addr] = poison(g_bus.log[i].addr);
        }
    }
}

/* Fault stray-write: the bus performs and logs one more write, to an address the
   vector does not list, as if the CPU had written where it must not. */
static void inject_stray_write(const sst_vector *v)
{
    uint16_t addr = 0xFFF0u;
    while (addr_listed(&v->initial, &v->final, addr) != 0) {
        addr = (uint16_t)(addr - 1u);
    }
    if (g_bus.nlog < FLAT_LOG_MAX) {
        flat_entry *e = &g_bus.log[g_bus.nlog];
        memset(e, 0, sizeof *e);
        e->addr = addr;
        e->data = 0x5Au;
        e->write = 1u;
        g_bus.nlog++;
    }
    g_bus.mem[addr] = 0x5Au;
}

/* One executed vector: returns the number of failed fields. */
static unsigned run_vector(const sst_vector *v, unsigned profile, const lynx__cpu_decode *t)
{
    lynx__cpu cpu;
    unsigned failed = 0;
    unsigned i;
    load_initial(v, &cpu, t, profile);
    /* fault no-step: the state is loaded and the CPU is never called */
    if (g_fault != FAULT_NO_STEP && !flat_bus_step_instruction(&g_bus, &cpu, t)) {
        fail_field(v, "cycle-bound", 0u, 0u);
        failed++;
    }
    if (g_fault == FAULT_STRAY_WRITE && g_stray_done == 0) {
        inject_stray_write(v);
        g_stray_done = 1;
    }
    if (cpu.pc != v->final.pc) {
        fail_field(v, "pc", cpu.pc, v->final.pc);
        failed++;
    }
    if (cpu.s != v->final.s) {
        fail_field(v, "s", cpu.s, v->final.s);
        failed++;
    }
    if (cpu.a != v->final.a) {
        fail_field(v, "a", cpu.a, v->final.a);
        failed++;
    }
    if (cpu.x != v->final.x) {
        fail_field(v, "x", cpu.x, v->final.x);
        failed++;
    }
    if (cpu.y != v->final.y) {
        fail_field(v, "y", cpu.y, v->final.y);
        failed++;
    }
    if ((cpu.p & 0xCFu) != (v->final.p & 0xCFu)) {
        fail_field(v, "p", cpu.p & 0xCFu, v->final.p & 0xCFu);
        failed++;
    }
    for (i = 0; i < v->final.nram; i++) {
        if (g_bus.mem[v->final.ram_addr[i]] != v->final.ram_val[i]) {
            fail_field(v, "ram", g_bus.mem[v->final.ram_addr[i]], v->final.ram_val[i]);
            failed++;
        }
    }
    for (i = 0; i < g_bus.nlog; i++) {
        if (g_bus.log[i].write != 0u && addr_listed(&v->initial, &v->final, g_bus.log[i].addr) == 0) {
            fail_field(v, "nowrite", g_bus.log[i].addr, 0u);
            failed++;
        }
    }
    restore_touched(v);
    return failed;
}

/* Under none an undefined opcode is a machine-loop stop before the opcode: the
   seam checks the boundary rule the loop applies. Returns the failed fields. */
static unsigned probe_undefined(const sst_vector *v, unsigned profile, const lynx__cpu_decode *t)
{
    lynx__cpu cpu;
    lynx__cpu before;
    uint8_t touched_before[SST_MAX_RAM];
    unsigned failed = 0;
    unsigned i;
    load_initial(v, &cpu, t, profile);
    /* memcpy, not struct assignment: the copy keeps the padding bytes that the
       memcmp below reads, so equal states compare equal. */
    memcpy(&before, &cpu, sizeof cpu);
    for (i = 0; i < v->initial.nram; i++) {
        touched_before[i] = g_bus.mem[v->initial.ram_addr[i]];
    }
    if (!lynx__cpu_at_boundary(&cpu)) {
        fail_field(v, "undefined-at-boundary", 0u, 1u);
        failed++;
    }
    if (cpu.req_kind != LYNX__CK_OPCODE || cpu.req_addr != v->initial.pc || (cpu.req_flags & LYNX__CF_WRITE) != 0u) {
        fail_field(v, "undefined-posted-request", cpu.req_kind, LYNX__CK_OPCODE);
        failed++;
    }
    if (lynx__cpu_opcode_defined(0u, g_bus.mem[cpu.req_addr]) || g_bus.mem[cpu.req_addr] != (uint8_t)v->opcode) {
        fail_field(v, "undefined-opcode-peek", g_bus.mem[cpu.req_addr], v->opcode);
        failed++;
    }
    /* no lynx__cpu_complete call is made, so no cycle completes */
    if (g_bus.nlog != 0u || memcmp(&before, &cpu, sizeof cpu) != 0) {
        fail_field(v, "undefined-cpu-state", g_bus.nlog, 0u);
        failed++;
    }
    for (i = 0; i < v->initial.nram; i++) {
        if (g_bus.mem[v->initial.ram_addr[i]] != touched_before[i]) {
            fail_field(v, "undefined-memory", g_bus.mem[v->initial.ram_addr[i]], touched_before[i]);
            failed++;
        }
    }
    restore_touched(v);
    return failed;
}

/* One vector of a run or undefined row (excluded rows are only counted by the
   callers); counts it in g_res. */
static void exec_one(const sst_vector *v, const run_opts *o, const lynx__cpu_decode *t)
{
    unsigned op = v->opcode;
    unsigned failed;
    if (g_info[op].status == ST_RUN) {
        g_res.asserted++;
        if (is_identity(v) != 0) {
            g_res.identity++;
            if (v->final.pc == v->initial.pc) {
                g_res.unmoved++;
            }
        }
        failed = run_vector(v, o->profile, t);
        if (failed == 0u) {
            g_res.passed++;
            g_res.op_passed[op]++;
        } else {
            g_res.failed++;
            g_res.op_failed[op]++;
        }
    } else if (g_info[op].status == ST_UNDEFINED) {
        g_res.undefined_checked++;
        failed = probe_undefined(v, o->profile, t);
        if (failed == 0u) {
            g_res.undefined_stopped++;
        } else {
            g_res.failed++;
            g_res.op_failed[op]++;
        }
    }
}

/* The profile table the run uses; fault mutant-69 is a test-local copy whose $69
   row is the $E9 row, so $69 decodes as SBC with no change to the core. */
static lynx__cpu_decode g_mutant[256];

static const lynx__cpu_decode *table_for(const run_opts *o)
{
    if (o->fault == FAULT_MUTANT_69) {
        memcpy(g_mutant, lynx__cpu_table(o->profile), sizeof g_mutant);
        g_mutant[0x69] = g_mutant[0xE9];
        return g_mutant;
    }
    return lynx__cpu_table(o->profile);
}

static void begin_execution(const run_opts *o)
{
    const lynx__cpu_decode *t = lynx__cpu_table(o->profile);
    unsigned op;
    unsigned a;
    for (op = 0; op < 256u; op++) {
        g_info[op] = classify(o->profile, op, t);
    }
    for (a = 0; a < 65536u; a++) {
        g_bus.mem[a] = poison((uint16_t)a);
    }
    memset(&g_res, 0, sizeof g_res);
    g_fault = o->fault;
    g_stray_done = 0;
}

static const char *fault_name(int fault)
{
    switch (fault) {
    case FAULT_NO_STEP:
        return "no-step";
    case FAULT_STRAY_WRITE:
        return "stray-write";
    case FAULT_MUTANT_69:
        return "mutant-69";
    default:
        return "none";
    }
}

/* One line per run row that executed at least one vector: op, passed, failed. */
static int write_results(const char *path)
{
    FILE *f;
    unsigned op;
    if (path == NULL) {
        return 0;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "lynx-test-cpu-vectors: cannot write %s\n", path);
        return -1;
    }
    for (op = 0; op < 256u; op++) {
        if (g_info[op].status == ST_RUN && g_res.op_passed[op] + g_res.op_failed[op] > 0u) {
            (void)fprintf(f, "%02x\t%u\t%u\n", op, g_res.op_passed[op], g_res.op_failed[op]);
        }
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "lynx-test-cpu-vectors: cannot write %s\n", path);
        return -1;
    }
    return 0;
}

static void print_fault(const run_opts *o)
{
    if (o->fault != FAULT_NONE) {
        printf("fault %s\n", fault_name(o->fault));
        printf("unmoved %u\n", g_res.unmoved);
    }
}

static int execute_pass(const char *buf, size_t len, const char *label, const run_opts *o)
{
    const lynx__cpu_decode *t = table_for(o);
    int rc;
    begin_execution(o);
    if (sst_open_sample(&g_reader, buf, len, label, o->partial) != 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        return EXIT_DATA;
    }
    while ((rc = sst_next(&g_reader, &g_vec)) > 0) {
        exec_one(&g_vec, o, t);
    }
    if (rc < 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        return EXIT_DATA;
    }
    return EXIT_PASS;
}

static int cmd_run(const run_opts *o)
{
    size_t len = 0;
    char *buf;
    lynx__sha256 sha;
    uint8_t digest[32];
    char hex[65];
    int rc;

    buf = read_whole_file(o->sample, &len);
    if (buf == NULL) {
        return EXIT_DATA;
    }
    /* identity first: nothing is parsed before the hash matches the manifest */
    lynx__sha256_init(&sha);
    lynx__sha256_update(&sha, buf, len);
    lynx__sha256_final(&sha, digest);
    lynx__sha256_hex(digest, hex);
    if (o->manifest != NULL) {
        char want[65];
        if (manifest_sha256(o->manifest, want) != 0) {
            free(buf);
            return EXIT_DATA;
        }
        if (strcmp(want, hex) != 0) {
            fprintf(stderr, "lynx-test-cpu-vectors: sha256 mismatch: sample %s, manifest %s\n", hex, want);
            free(buf);
            return EXIT_DATA;
        }
    }
    if (parse_pass(buf, len, o->sample, o->profile, o->partial) != 0) {
        free(buf);
        return EXIT_DATA;
    }
    build_plan(o, hex);
    if (o->print_plan != 0) {
        (void)fwrite(g_plan, 1u, g_planlen, stdout);
        free(buf);
        return EXIT_PASS;
    }
    rc = compare_plan(o->expect_plan);
    if (rc != 0) {
        free(buf);
        return rc == -2 ? EXIT_DATA : EXIT_INVENTORY;
    }
    rc = execute_pass(buf, len, o->sample, o);
    free(buf);
    if (rc != EXIT_PASS) {
        return rc;
    }
    if (write_results(o->results) != 0) {
        return EXIT_DATA;
    }
    printf("profile %s\n", o->profile_name);
    printf("sample_sha256 %s\n", hex);
    printf("plan matches the committed inventory\n");
    if (o->profile == 0u) {
        printf("totals files %u run %u excluded %u undefined %u vectors %u\n", g_reader.sections, g_totals.run,
               g_totals.excluded, g_totals.undefined, g_totals.vectors);
    } else {
        printf("totals files %u run %u excluded %u vectors %u\n", g_reader.sections, g_totals.run, g_totals.excluded,
               g_totals.vectors);
    }
    printf("asserted %u passed %u failed %u identity %u\n", g_res.asserted, g_res.passed, g_res.failed,
           g_res.identity);
    print_fault(o);
    printf("excluded_vectors parsed %u asserted 0\n", g_totals.excluded_vectors);
    if (o->profile == 0u) {
        printf("undefined_vectors %u stopped %u\n", g_res.undefined_checked, g_res.undefined_stopped);
    }
    printf("unverified cycles rows %u vectors %u shape rows %u vectors %u\n", g_totals.unv_cycles_rows,
           g_totals.unv_cycles_vectors, g_totals.unv_shape_rows, g_totals.unv_shape_vectors);
    if (g_res.asserted != g_totals.asserted || g_res.undefined_checked != g_totals.undefined_vectors) {
        fprintf(stderr, "lynx-test-cpu-vectors: executed counts differ from the plan\n");
        return EXIT_ASSERT;
    }
    return g_res.failed == 0u ? EXIT_PASS : EXIT_ASSERT;
}

/* The whole upstream files: DIR/<variant>/v1/<op>.json, 10,000 vectors each. The
   same classification as the sample run (exclusions, and under none the undefined
   rows) with no committed plan: the full lane compares totals, not a file. */
static int cmd_complete(const run_opts *o)
{
    const lynx__cpu_decode *t = table_for(o);
    unsigned op;
    unsigned files = 0;
    unsigned run_rows = 0;
    unsigned excluded_rows = 0;
    unsigned undefined_rows = 0;
    unsigned vectors = 0;
    unsigned excluded_vectors = 0;
    char path[1024];
    begin_execution(o);
    for (op = 0; op < 256u; op++) {
        size_t len = 0;
        char *buf;
        int rc;
        int n = snprintf(path, sizeof path, "%s/%s/v1/%02x.json", o->dir, o->variant, op);
        if (n < 0 || (size_t)n >= sizeof path) {
            fprintf(stderr, "lynx-test-cpu-vectors: path too long\n");
            return EXIT_USAGE;
        }
        if (o->partial != 0) {
            FILE *probe = fopen(path, "rb");
            if (probe == NULL) {
                continue;
            }
            (void)fclose(probe);
        }
        buf = read_whole_file(path, &len);
        if (buf == NULL) {
            return EXIT_DATA;
        }
        sst_open_file(&g_reader, buf, len, op, path);
        while ((rc = sst_next(&g_reader, &g_vec)) > 0) {
            vectors++;
            if (g_info[op].status == ST_EXCLUDED) {
                excluded_vectors++;
            } else {
                exec_one(&g_vec, o, t);
            }
        }
        if (rc < 0) {
            fprintf(stderr, "%s\n", g_reader.msg);
            free(buf);
            return EXIT_DATA;
        }
        free(buf);
        files++;
        if (g_info[op].status == ST_RUN) {
            run_rows++;
        } else if (g_info[op].status == ST_EXCLUDED) {
            excluded_rows++;
        } else {
            undefined_rows++;
        }
    }
    if (files == 0u) {
        fprintf(stderr, "lynx-test-cpu-vectors: no vector file found under %s/%s/v1\n", o->dir, o->variant);
        return EXIT_DATA;
    }
    if (write_results(o->results) != 0) {
        return EXIT_DATA;
    }
    printf("profile %s\n", o->profile_name);
    printf("variant %s\n", o->variant);
    printf("totals files %u run %u excluded %u undefined %u vectors %u\n", files, run_rows, excluded_rows,
           undefined_rows, vectors);
    printf("asserted %u passed %u failed %u identity %u\n", g_res.asserted, g_res.passed, g_res.failed,
           g_res.identity);
    print_fault(o);
    printf("excluded_vectors parsed %u asserted 0\n", excluded_vectors);
    if (o->profile == 0u) {
        printf("undefined_vectors %u stopped %u\n", g_res.undefined_checked, g_res.undefined_stopped);
    }
    if (g_res.asserted + excluded_vectors + g_res.undefined_checked != vectors) {
        fprintf(stderr, "lynx-test-cpu-vectors: executed counts differ from the parsed counts\n");
        return EXIT_ASSERT;
    }
    return g_res.failed == 0u ? EXIT_PASS : EXIT_ASSERT;
}

/* ---- seam-agreement: the shipping machine against the vector seam --------- */

static _Alignas(max_align_t) unsigned char g_mbuf[sizeof(struct lynx_machine)];
static unsigned char g_tmpl[LYNX__SNAPSHOT_HEADER_LEN + LYNX__SNAPSHOT_PAYLOAD_LEN];
static unsigned char g_blob[LYNX__SNAPSHOT_HEADER_LEN + LYNX__SNAPSHOT_PAYLOAD_LEN];
static unsigned char g_mref[sizeof(struct lynx_machine)];
static uint8_t g_poison[65536];
static uint8_t g_ram_read[65536];
static unsigned g_sa_printed;

static void sa_fail(const sst_vector *v, const char *what, unsigned got, unsigned want)
{
    if (g_sa_printed < 40u) {
        fprintf(stderr, "FAIL opcode %02x vector %u %s: machine %u, seam/expected %u\n", v->opcode, v->ordinal, what,
                got, want);
    } else if (g_sa_printed == 40u) {
        fprintf(stderr, "FAIL (further failures not printed)\n");
    }
    g_sa_printed++;
}

/* The vector's initial state as a format-2 payload on top of the template blob,
   the payload hash recomputed, restored through the public call. The CPU is at
   an instruction boundary at the vector's pc, the cost model is flat5 and guest
   time is 0 with the next cycle due at once. 0 on success. */
static int sa_restore(lynx_machine *m, const sst_vector *v, const uint8_t *ram)
{
    unsigned char *pay = g_blob + LYNX__SNAPSHOT_HEADER_LEN;
    lynx__sha256 h;
    memcpy(g_blob, g_tmpl, sizeof g_blob);
    memset(pay, 0, 32u); /* every CPU latch, cost model 0, guest time and ready time zero */
    pay[0] = (unsigned char)(v->initial.pc & 0xFFu);
    pay[1] = (unsigned char)(v->initial.pc >> 8);
    pay[2] = v->initial.a;
    pay[3] = v->initial.x;
    pay[4] = v->initial.y;
    pay[5] = v->initial.s;
    pay[6] = (unsigned char)(v->initial.p | 0x30u);
    memcpy(pay + 32, ram, 65536u);
    lynx__sha256_init(&h);
    lynx__sha256_update(&h, pay, LYNX__SNAPSHOT_PAYLOAD_LEN);
    lynx__sha256_final(&h, g_blob + (LYNX__SNAPSHOT_HEADER_LEN - 32u));
    return lynx_snapshot_restore(m, g_blob, sizeof g_blob) == LYNX_OK ? 0 : -1;
}

static unsigned sa_reg(const lynx_machine *m, uint32_t reg)
{
    uint32_t value = 0xA5A5A5A5u;
    if (lynx_read_register(m, reg, &value) != LYNX_OK) {
        return 0xA5A5A5A5u;
    }
    return value;
}

/* One vector. A run row: the internal seam steps one instruction on the flat
   test bus and the machine advances by that many cycles at 5 ticks; they must
   agree on registers and on every RAM byte. An undefined row (profile none):
   the machine stops with nothing consumed and the instance unchanged. Returns
   the number of failed checks. */
static unsigned sa_one(lynx_machine *m, const sst_vector *v, unsigned profile, const lynx__cpu_decode *t,
                       int undefined)
{
    unsigned failed = 0;
    unsigned i;
    uint64_t consumed = 0xA5A5A5A5u;
    uint64_t ticks = 0xA5A5A5A5u;
    lynx_stop_reason stop = 0xA5A5A5A5u;
    lynx__cpu cpu;
    unsigned n;
    memcpy(g_bus.mem, g_poison, sizeof g_bus.mem);
    for (i = 0; i < v->initial.nram; i++) {
        g_bus.mem[v->initial.ram_addr[i]] = v->initial.ram_val[i];
    }
    if (sa_restore(m, v, g_bus.mem) != 0) {
        sa_fail(v, "restore", 1u, 0u);
        return 1u;
    }
    if (undefined != 0) {
        memcpy(g_mref, m, sizeof g_mref);
        if (lynx_advance(m, 1000u, &consumed, &stop) != LYNX_OK || consumed != 0u || stop != LYNX_STOP_UNDEFINED) {
            sa_fail(v, "undefined-stop", (unsigned)stop, LYNX_STOP_UNDEFINED);
            failed++;
        }
        if (memcmp(m, g_mref, sizeof g_mref) != 0) {
            sa_fail(v, "undefined-instance-changed", 1u, 0u);
            failed++;
        }
        return failed;
    }
    lynx__cpu_init(&cpu, profile);
    cpu.a = v->initial.a;
    cpu.x = v->initial.x;
    cpu.y = v->initial.y;
    cpu.s = v->initial.s;
    cpu.p = (uint8_t)(v->initial.p | 0x30u);
    lynx__cpu_start_at(&cpu, t, v->initial.pc);
    flat_bus_reset_log(&g_bus);
    if (!flat_bus_step_instruction(&g_bus, &cpu, t)) {
        sa_fail(v, "seam-cycle-bound", 0u, 0u);
        return 1u;
    }
    n = g_bus.nlog;
    if (lynx_advance(m, (uint64_t)n * 5u, &consumed, &stop) != LYNX_OK || consumed != (uint64_t)n * 5u ||
        stop != LYNX_STOP_BUDGET) {
        sa_fail(v, "advance", (unsigned)consumed, n * 5u);
        return 1u;
    }
    if (lynx_guest_ticks(m, &ticks) != LYNX_OK || ticks != (uint64_t)n * 5u) {
        sa_fail(v, "guest-ticks", (unsigned)ticks, n * 5u);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_PC) != cpu.pc) {
        sa_fail(v, "pc", sa_reg(m, LYNX_REG_PC), cpu.pc);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_S) != cpu.s) {
        sa_fail(v, "s", sa_reg(m, LYNX_REG_S), cpu.s);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_A) != cpu.a) {
        sa_fail(v, "a", sa_reg(m, LYNX_REG_A), cpu.a);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_X) != cpu.x) {
        sa_fail(v, "x", sa_reg(m, LYNX_REG_X), cpu.x);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_Y) != cpu.y) {
        sa_fail(v, "y", sa_reg(m, LYNX_REG_Y), cpu.y);
        failed++;
    }
    if (sa_reg(m, LYNX_REG_P) != ((unsigned)cpu.p | 0x30u)) {
        sa_fail(v, "p", sa_reg(m, LYNX_REG_P), (unsigned)cpu.p | 0x30u);
        failed++;
    }
    /* A boundary with nothing in flight: both words are zero (cost model 0). */
    if (sa_reg(m, LYNX_REG_CPU_STATE) != 0u || sa_reg(m, LYNX_REG_CPU_LATCH) != 0u) {
        sa_fail(v, "boundary", sa_reg(m, LYNX_REG_CPU_STATE) | sa_reg(m, LYNX_REG_CPU_LATCH), 0u);
        failed++;
    }
    if (lynx_read_memory(m, 0u, g_ram_read, 65536u) != LYNX_OK) {
        sa_fail(v, "read-memory", 1u, 0u);
        return failed + 1u;
    }
    if (memcmp(g_ram_read, g_bus.mem, 65536u) != 0) {
        unsigned a = 0;
        while (g_ram_read[a] == g_bus.mem[a]) {
            a++;
        }
        sa_fail(v, "ram", g_ram_read[a], g_bus.mem[a]);
        if (g_sa_printed <= 40u) {
            fprintf(stderr, "  first differing address $%04x\n", a);
        }
        failed++;
    }
    return failed;
}

static int cmd_seam_agreement(const run_opts *o)
{
    const lynx__cpu_decode *t = lynx__cpu_table(o->profile);
    size_t len = 0;
    char *buf = read_whole_file(o->sample, &len);
    lynx__sha256 sha;
    uint8_t digest[32];
    char hex[65];
    char want[65];
    lynx_config cfg;
    lynx_machine *m = NULL;
    static const uint8_t content[1] = {0xEAu};
    size_t saved = 0;
    unsigned expect_run = 0;
    unsigned expect_undef_rows = 0;
    unsigned tested_run = 0;
    unsigned tested_undef = 0;
    unsigned failed = 0;
    unsigned op;
    unsigned a;
    int rc;

    if (buf == NULL) {
        return EXIT_DATA;
    }
    lynx__sha256_init(&sha);
    lynx__sha256_update(&sha, buf, len);
    lynx__sha256_final(&sha, digest);
    lynx__sha256_hex(digest, hex);
    if (manifest_sha256(o->manifest, want) != 0 || strcmp(want, hex) != 0) {
        fprintf(stderr, "lynx-test-cpu-vectors: the sample hash does not match the manifest\n");
        free(buf);
        return EXIT_DATA;
    }
    for (op = 0; op < 256u; op++) {
        rowinfo ri = classify(o->profile, op, t);
        g_info[op] = ri;
        expect_run += ri.status == ST_RUN ? 1u : 0u;
        expect_undef_rows += ri.status == ST_UNDEFINED ? 1u : 0u;
    }
    for (a = 0; a < 65536u; a++) {
        g_poison[a] = poison((uint16_t)a);
    }
    lynx_config_default(&cfg);
    cfg.profile = o->profile;
    if (lynx_machine_size() > sizeof g_mbuf ||
        lynx_machine_init(g_mbuf, lynx_machine_size(), &cfg, &m) != LYNX_OK ||
        lynx_load_ram_image(m, content, sizeof content, 0x0200u) != LYNX_OK ||
        lynx_snapshot_save(m, g_tmpl, sizeof g_tmpl, &saved) != LYNX_OK || saved != sizeof g_tmpl) {
        fprintf(stderr, "lynx-test-cpu-vectors: cannot build the machine\n");
        free(buf);
        return EXIT_DATA;
    }
    if (sst_open_sample(&g_reader, buf, len, o->sample, 0) != 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        free(buf);
        return EXIT_DATA;
    }
    while ((rc = sst_next(&g_reader, &g_vec)) > 0) {
        op = g_vec.opcode;
        if (g_info[op].status == ST_RUN && g_vec.ordinal == 0u) {
            failed += sa_one(m, &g_vec, o->profile, t, 0) != 0u ? 1u : 0u;
            tested_run++;
        } else if (g_info[op].status == ST_UNDEFINED) {
            failed += sa_one(m, &g_vec, o->profile, t, 1) != 0u ? 1u : 0u;
            tested_undef++;
        }
    }
    free(buf);
    lynx_machine_deinit(m);
    if (rc < 0) {
        fprintf(stderr, "%s\n", g_reader.msg);
        return EXIT_DATA;
    }
    printf("profile %s\n", o->profile_name);
    printf("seam_agreement run_rows %u undefined_vectors %u failed %u\n", tested_run, tested_undef, failed);
    if (tested_run != expect_run || tested_undef != expect_undef_rows * SST_SAMPLE_OBJECTS) {
        fprintf(stderr, "lynx-test-cpu-vectors: tested %u run rows and %u undefined vectors, expected %u and %u\n",
                tested_run, tested_undef, expect_run, expect_undef_rows * SST_SAMPLE_OBJECTS);
        return EXIT_ASSERT;
    }
    return failed == 0u ? EXIT_PASS : EXIT_ASSERT;
}

static int parse_profile(const char *name, unsigned *out)
{
    if (strcmp(name, "none") == 0) {
        *out = 0u;
    } else if (strcmp(name, "lynx1") == 0) {
        *out = 1u;
    } else if (strcmp(name, "lynx2") == 0) {
        *out = 2u;
    } else {
        return -1;
    }
    return 0;
}

static int parse_fault(const char *name, int *out)
{
    if (strcmp(name, "no-step") == 0) {
        *out = FAULT_NO_STEP;
    } else if (strcmp(name, "stray-write") == 0) {
        *out = FAULT_STRAY_WRITE;
    } else if (strcmp(name, "mutant-69") == 0) {
        *out = FAULT_MUTANT_69;
    } else {
        return -1;
    }
    return 0;
}

static int usage(void)
{
    fprintf(stderr,
            "usage: lynx-test-cpu-vectors parse-check FILE | self-test-reader\n"
            "       lynx-test-cpu-vectors run --profile none|lynx1|lynx2 --sample FILE --manifest FILE "
            "--expect-plan FILE\n"
            "       lynx-test-cpu-vectors run --profile P --sample FILE --print-plan\n"
            "       lynx-test-cpu-vectors complete --profile P --dir DIR --variant V\n"
            "       lynx-test-cpu-vectors seam-agreement --profile P --sample FILE --manifest FILE\n"
            "       run and complete also take: [--results FILE] [--partial] "
            "[--fault no-step|stray-write|mutant-69]\n");
    return EXIT_USAGE;
}

/* ---- main ---------------------------------------------------------------- */

/* Options shared by run and complete; 0 on success. */
static int parse_run_opts(int argc, char **argv, run_opts *o)
{
    int i;
    memset(o, 0, sizeof *o);
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--print-plan") == 0) {
            o->print_plan = 1;
        } else if (strcmp(argv[i], "--partial") == 0) {
            o->partial = 1;
        } else if (i + 1 < argc && strcmp(argv[i], "--profile") == 0) {
            o->profile_name = argv[++i];
            if (parse_profile(o->profile_name, &o->profile) != 0) {
                return -1;
            }
        } else if (i + 1 < argc && strcmp(argv[i], "--fault") == 0) {
            if (parse_fault(argv[++i], &o->fault) != 0) {
                return -1;
            }
        } else if (i + 1 < argc && strcmp(argv[i], "--sample") == 0) {
            o->sample = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--manifest") == 0) {
            o->manifest = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--expect-plan") == 0) {
            o->expect_plan = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--dir") == 0) {
            o->dir = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--variant") == 0) {
            o->variant = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--results") == 0) {
            o->results = argv[++i];
        } else {
            return -1;
        }
    }
    return o->profile_name == NULL ? -1 : 0;
}

int main(int argc, char **argv)
{
    run_opts o;
    if (argc == 3 && strcmp(argv[1], "parse-check") == 0) {
        return cmd_parse_check(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "self-test-reader") == 0) {
        return cmd_self_test_reader();
    }
    if (argc >= 2 && strcmp(argv[1], "run") == 0) {
        if (parse_run_opts(argc, argv, &o) != 0 || o.sample == NULL || o.dir != NULL || o.variant != NULL) {
            return usage();
        }
        if (o.print_plan == 0 && (o.manifest == NULL || o.expect_plan == NULL)) {
            return usage();
        }
        return cmd_run(&o);
    }
    if (argc >= 2 && strcmp(argv[1], "seam-agreement") == 0) {
        if (parse_run_opts(argc, argv, &o) != 0 || o.sample == NULL || o.manifest == NULL || o.dir != NULL ||
            o.variant != NULL || o.expect_plan != NULL || o.print_plan != 0 || o.fault != FAULT_NONE) {
            return usage();
        }
        return cmd_seam_agreement(&o);
    }
    if (argc >= 2 && strcmp(argv[1], "complete") == 0) {
        if (parse_run_opts(argc, argv, &o) != 0 || o.dir == NULL || o.variant == NULL || o.sample != NULL ||
            o.manifest != NULL || o.expect_plan != NULL || o.print_plan != 0) {
            return usage();
        }
        if (strcmp(o.variant, "synertek65c02") != 0 && strcmp(o.variant, "rockwell65c02") != 0) {
            fprintf(stderr, "lynx-test-cpu-vectors: the variant must be synertek65c02 or rockwell65c02\n");
            return EXIT_USAGE;
        }
        return cmd_complete(&o);
    }
    return usage();
}
