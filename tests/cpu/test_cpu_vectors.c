/* lynx-test-cpu-vectors: the per-opcode vector runner and the reader self-test.

   Subcommands:
     parse-check FILE        parse a sample container strictly and print its counts
     self-test-reader        feed malformed inputs to the reader and check each rejection

   Exit codes: 0 pass, 1 assertion failure, 2 usage, 3 data or parse error, 4 inventory
   mismatch. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/sst_reader.h"

enum { EXIT_PASS = 0, EXIT_ASSERT = 1, EXIT_USAGE = 2, EXIT_DATA = 3, EXIT_INVENTORY = 4 };

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

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "parse-check") == 0) {
        return cmd_parse_check(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "self-test-reader") == 0) {
        return cmd_self_test_reader();
    }
    fprintf(stderr, "usage: lynx-test-cpu-vectors parse-check FILE | self-test-reader\n");
    return EXIT_USAGE;
}
