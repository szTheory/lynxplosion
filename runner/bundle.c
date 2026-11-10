/* Artifact bundle writer: see bundle.h. Host side; the core never sees it. */
#include "bundle.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

#define BUNDLE_RAM_LEN LYNX_ADDRESS_SPACE
#define BUNDLE_REGS_LEN (4u * LYNX_REG_COUNT)
#define BUNDLE_TIME_LEN 20u
#define BUNDLE_HEX_PER_LINE 32u
#define BUNDLE_MAX_CONTENT (1u << 20)
#define BUNDLE_MAX_NAME 16u
#define BUNDLE_MANIFEST_CAP 1024u
#define BUNDLE_DIGESTS_CAP 512u

/* SHA-256 of the empty input timeline: Phase 1 runs have no input. */
static const char bundle_empty_input_digest[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

typedef struct entry {
    char name[BUNDLE_MAX_NAME];
    char hex[65];
} entry;

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void put_u64le(uint8_t *p, uint64_t v)
{
    put_u32le(p, (uint32_t)(v & 0xFFFFFFFFu));
    put_u32le(p + 4, (uint32_t)(v >> 32));
}

static void hash_hex(const void *data, size_t len, char hex[65])
{
    lynx__sha256 s;
    uint8_t digest[32];
    lynx__sha256_init(&s);
    lynx__sha256_update(&s, data, len);
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
}

static int set_entry(entry *e, const char *name, const void *data, size_t len)
{
    size_t n = strlen(name);
    if (n >= BUNDLE_MAX_NAME) {
        return 1;
    }
    memcpy(e->name, name, n + 1u);
    hash_hex(data, len, e->hex);
    return 0;
}

static int entry_cmp(const void *a, const void *b)
{
    return strcmp(((const entry *)a)->name, ((const entry *)b)->name);
}

/* The three canonical observation byte strings. */
static int collect(const lynx_machine *m, const bundle_run *run, uint8_t *ram, uint8_t regs[],
                   uint8_t tbuf[])
{
    uint64_t ticks = 0;
    uint32_t reg;
    if (m == NULL || run == NULL) {
        return 1;
    }
    if (lynx_read_memory(m, 0, ram, BUNDLE_RAM_LEN) != LYNX_OK) {
        return 1;
    }
    for (reg = 0; reg < LYNX_REG_COUNT; reg++) {
        uint32_t value = 0;
        if (lynx_read_register(m, reg, &value) != LYNX_OK) {
            return 1;
        }
        put_u32le(regs + 4u * reg, value);
    }
    if (lynx_guest_ticks(m, &ticks) != LYNX_OK) {
        return 1;
    }
    put_u64le(tbuf, ticks);
    put_u64le(tbuf + 8, run->ticks_consumed);
    put_u32le(tbuf + 16, run->stop);
    return 0;
}

/* Fills three entries (sorted by name) from the canonical bytes. */
static int obs_entries(const lynx_machine *m, const bundle_run *run, entry out[3])
{
    uint8_t regs[BUNDLE_REGS_LEN];
    uint8_t tbuf[BUNDLE_TIME_LEN];
    uint8_t *ram = malloc(BUNDLE_RAM_LEN);
    int rc = 1;
    if (ram == NULL) {
        return 1;
    }
    memset(regs, 0, sizeof regs);
    memset(tbuf, 0, sizeof tbuf);
    if (collect(m, run, ram, regs, tbuf) == 0 &&
        set_entry(&out[0], "obs.ram.hex", ram, BUNDLE_RAM_LEN) == 0 &&
        set_entry(&out[1], "obs.regs.hex", regs, sizeof regs) == 0 &&
        set_entry(&out[2], "obs.time.hex", tbuf, sizeof tbuf) == 0) {
        rc = 0;
    }
    free(ram);
    return rc;
}

/* Observation digest over the obs.* lines of already sorted entries. */
static void obs_digest(const entry *sorted, size_t count, char hex[65])
{
    lynx__sha256 s;
    uint8_t digest[32];
    size_t i;
    lynx__sha256_init(&s);
    for (i = 0; i < count; i++) {
        if (strncmp(sorted[i].name, "obs.", 4) == 0) {
            lynx__sha256_update(&s, sorted[i].name, strlen(sorted[i].name));
            lynx__sha256_update(&s, " ", 1);
            lynx__sha256_update(&s, sorted[i].hex, 64);
            lynx__sha256_update(&s, "\n", 1);
        }
    }
    lynx__sha256_final(&s, digest);
    lynx__sha256_hex(digest, hex);
}

int bundle_observe(const lynx_machine *m, const bundle_run *run, char obs_digest_hex[65])
{
    entry e[3];
    if (m == NULL || run == NULL || obs_digest_hex == NULL || obs_entries(m, run, e) != 0) {
        return 1;
    }
    qsort(e, 3, sizeof e[0], entry_cmp);
    obs_digest(e, 3, obs_digest_hex);
    return 0;
}

static const char *profile_text(uint32_t v)
{
    return lynx_profile_name(v);
}

static const char *policy_text(uint32_t v)
{
    if (v == LYNX_RAM_POLICY_ZERO) {
        return "zero";
    }
    return v == LYNX_RAM_POLICY_SPLITMIX64 ? "splitmix64" : NULL;
}

static const char *stop_text(lynx_stop_reason v)
{
    switch (v) {
    case LYNX_STOP_BUDGET:
        return "budget";
    case LYNX_STOP_FRAME:
        return "frame";
    case LYNX_STOP_WATCH:
        return "watch";
    case LYNX_STOP_HALT:
        return "halt";
    case LYNX_STOP_UNDEFINED:
        return "undefined";
    default:
        return NULL;
    }
}

/* Writes run.manifest text into buf. Returns its length, or 0 on a refusal. */
static size_t manifest_text(const lynx_machine *m, const bundle_run *run, char *buf, size_t cap)
{
    lynx_identity id;
    char sha[65];
    const char *profile;
    const char *policy;
    const char *stop;
    int n;
    if (lynx_get_identity(m, &id) != LYNX_OK || id.has_content == 0) {
        return 0;
    }
    profile = profile_text(id.profile);
    policy = policy_text(id.ram_policy);
    stop = stop_text(run->stop);
    if (profile == NULL || policy == NULL || stop == NULL) {
        return 0;
    }
    lynx__sha256_hex(id.content_sha256, sha);
    n = snprintf(buf, cap,
                 "profile = %s\n"
                 "startup_tier = A\n"
                 "content_sha256 = %s\n"
                 "content_load_addr = 0x%04x\n"
                 "input_digest = %s\n"
                 "tick_budget = %llu\n"
                 "ticks_consumed = %llu\n"
                 "stop_reason = %s\n"
                 "core_build = %s\n"
                 "ram_policy = %s\n"
                 "ram_seed = 0x%016llx\n"
                 "evidence_tier = R\n",
                 profile, sha, (unsigned)id.content_addr, bundle_empty_input_digest,
                 (unsigned long long)run->tick_budget, (unsigned long long)run->ticks_consumed,
                 stop, lynx_build_id(), policy, (unsigned long long)id.ram_seed);
    if (n < 0 || (size_t)n >= cap) {
        return 0;
    }
    return (size_t)n;
}

static int path_in(char *path, size_t cap, const char *dir, const char *name)
{
    int n = snprintf(path, cap, "%s/%s", dir, name);
    return (n < 0 || (size_t)n >= cap) ? 1 : 0;
}

/* Removes dir/name if it exists: a file left by an earlier bundle, or a symlink
   (the link itself goes, never its target). Returns 0 when it is gone. */
static int clear_file(const char *dir, const char *name)
{
    char path[4096];
    if (path_in(path, sizeof path, dir, name) != 0) {
        return 1;
    }
    if (remove(path) != 0 && errno != ENOENT) {
        return 1;
    }
    return 0;
}

/* Creates dir/name afresh: the old entry is removed and the new file is created
   exclusively ("x"), so a link planted in between is refused, not followed. */
static int write_file(const char *dir, const char *name, const void *data, size_t len)
{
    char path[4096];
    FILE *f;
    int ok;
    if (clear_file(dir, name) != 0 || path_in(path, sizeof path, dir, name) != 0) {
        return 1;
    }
    f = fopen(path, "wbx");
    if (f == NULL) {
        return 1;
    }
    ok = fwrite(data, 1, len, f) == len;
    if (fclose(f) != 0) {
        ok = 0;
    }
    return ok ? 0 : 1;
}

/* Lowercase hex text, 32 bytes per line, every line ending in a newline. */
static int write_hex_file(const char *dir, const char *name, const uint8_t *bytes, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    size_t lines = (len + BUNDLE_HEX_PER_LINE - 1u) / BUNDLE_HEX_PER_LINE;
    size_t cap = len * 2u + lines;
    char *text = malloc(cap);
    size_t i;
    size_t o = 0;
    int rc;
    if (text == NULL) {
        return 1;
    }
    for (i = 0; i < len; i++) {
        text[o++] = digits[bytes[i] >> 4];
        text[o++] = digits[bytes[i] & 0x0Fu];
        if ((i + 1u) % BUNDLE_HEX_PER_LINE == 0 || i + 1u == len) {
            text[o++] = '\n';
        }
    }
    rc = write_file(dir, name, text, o);
    free(text);
    return rc;
}

int bundle_write(const char *dir, const lynx_machine *m, const bundle_run *run,
                 char obs_digest_hex[65], char bundle_digest_hex[65])
{
    uint8_t regs[BUNDLE_REGS_LEN];
    uint8_t tbuf[BUNDLE_TIME_LEN];
    uint8_t *ram;
    char manifest[BUNDLE_MANIFEST_CAP];
    char digests[BUNDLE_DIGESTS_CAP];
    entry e[4];
    size_t manifest_len;
    size_t digests_len = 0;
    size_t i;
    int rc = 1;

    if (dir == NULL || m == NULL || run == NULL || obs_digest_hex == NULL ||
        bundle_digest_hex == NULL) {
        return 1;
    }
    manifest_len = manifest_text(m, run, manifest, sizeof manifest);
    if (manifest_len == 0) {
        return 1;
    }
    ram = malloc(BUNDLE_RAM_LEN);
    if (ram == NULL) {
        return 1;
    }
    memset(regs, 0, sizeof regs);
    memset(tbuf, 0, sizeof tbuf);
    if (collect(m, run, ram, regs, tbuf) != 0 ||
        set_entry(&e[0], "obs.ram.hex", ram, BUNDLE_RAM_LEN) != 0 ||
        set_entry(&e[1], "obs.regs.hex", regs, sizeof regs) != 0 ||
        set_entry(&e[2], "obs.time.hex", tbuf, sizeof tbuf) != 0 ||
        set_entry(&e[3], "run.manifest", manifest, manifest_len) != 0) {
        goto done;
    }
    /* Byte-order sort inside the runner: strcmp compares unsigned chars. */
    qsort(e, 4, sizeof e[0], entry_cmp);
    for (i = 0; i < 4; i++) {
        int n = snprintf(digests + digests_len, sizeof digests - digests_len, "%s %s\n",
                         e[i].name, e[i].hex);
        if (n < 0 || (size_t)n >= sizeof digests - digests_len) {
            goto done;
        }
        digests_len += (size_t)n;
    }
    obs_digest(e, 4, obs_digest_hex);
    hash_hex(digests, digests_len, bundle_digest_hex);
    /* digests.txt vouches for the other four files, so it and run.manifest go
       first: a write that fails part way leaves no digests.txt beside new files. */
    if (clear_file(dir, "digests.txt") != 0 || clear_file(dir, "run.manifest") != 0) {
        goto done;
    }
    if (write_hex_file(dir, "obs.ram.hex", ram, BUNDLE_RAM_LEN) != 0 ||
        write_hex_file(dir, "obs.regs.hex", regs, sizeof regs) != 0 ||
        write_hex_file(dir, "obs.time.hex", tbuf, sizeof tbuf) != 0 ||
        write_file(dir, "run.manifest", manifest, manifest_len) != 0 ||
        write_file(dir, "digests.txt", digests, digests_len) != 0) {
        goto done;
    }
    rc = 0;
done:
    free(ram);
    return rc;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int bundle_read_hex(const char *path, uint8_t **bytes, size_t *len)
{
    FILE *f;
    uint8_t *out;
    size_t n = 0;
    size_t cap = 256;
    int c;
    int in_comment = 0;
    int rc = 1;

    if (path == NULL || bytes == NULL || len == NULL) {
        return 1;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return 1;
    }
    out = malloc(cap);
    if (out == NULL) {
        fclose(f);
        return 1;
    }
    while ((c = fgetc(f)) != EOF) {
        int hi;
        int lo;
        if (in_comment) {
            if (c == '\n') {
                in_comment = 0;
            }
            continue;
        }
        if (c == '#') {
            in_comment = 1;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        hi = hex_value(c);
        if (hi < 0) {
            goto done;
        }
        lo = hex_value(fgetc(f));
        if (lo < 0) {
            goto done; /* odd digit count, split pair, or a stray character */
        }
        if (n == cap) {
            uint8_t *grown;
            if (cap >= BUNDLE_MAX_CONTENT) {
                goto done;
            }
            cap *= 2u;
            grown = realloc(out, cap);
            if (grown == NULL) {
                goto done;
            }
            out = grown;
        }
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    if (ferror(f) || n == 0) {
        goto done;
    }
    *bytes = out;
    *len = n;
    out = NULL;
    rc = 0;
done:
    free(out);
    fclose(f);
    return rc;
}
