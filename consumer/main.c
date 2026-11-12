/* Public-only host consumer for the project-authored Phase 3 diagnostic. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#elif defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fs.h>
#endif

#include "lynxplosion.h"

enum {
    BLL_LIMIT = 65535,
    SYMBOL_LIMIT = 16384,
    SYMBOL_LINE_LIMIT = 255,
    RESULT_BYTES = 8,
    PIXEL_BYTES = 32,
    SNAPSHOT_TICK = 5000,
    ADVANCE_SLICE = 137,
    PPM_CAP = 4096,
    METADATA_CAP = 1024,
    WAV_DATA_BYTES = 1600,
    WAV_BYTES = 44 + WAV_DATA_BYTES,
    TIMELINE_LIMIT = 128 * 1024,
    TIMELINE_RECORD_LIMIT = 4096,
    FRAME_PPM_LIMIT = 50000,
    FRAME_MANIFEST_CAP = 2048
};

typedef struct input_event { uint64_t tick; uint32_t mask; } input_event;
typedef struct input_timeline { input_event *events; size_t count; } input_timeline;

/* Static host-local copy of the project-owned FIPS 180-4 helper. The consumer
   includes only the public emulator header and hashes canonical script bytes. */
typedef struct host_sha256 {
    uint32_t h[8]; uint8_t block[64]; uint32_t used; uint64_t total;
} host_sha256;
static const uint32_t host_sha_k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t host_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }
static void host_sha_compress(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (uint32_t i = 0u; i < 16u; i++)
        w[i] = ((uint32_t)block[4u*i] << 24) | ((uint32_t)block[4u*i+1u] << 16) |
               ((uint32_t)block[4u*i+2u] << 8) | (uint32_t)block[4u*i+3u];
    for (uint32_t i = 16u; i < 64u; i++) {
        uint32_t s0 = host_rotr(w[i-15u],7u) ^ host_rotr(w[i-15u],18u) ^ (w[i-15u] >> 3);
        uint32_t s1 = host_rotr(w[i-2u],17u) ^ host_rotr(w[i-2u],19u) ^ (w[i-2u] >> 10);
        w[i] = w[i-16u] + s0 + w[i-7u] + s1;
    }
    uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], q=h[7];
    for (uint32_t i = 0u; i < 64u; i++) {
        uint32_t s1=host_rotr(e,6u)^host_rotr(e,11u)^host_rotr(e,25u);
        uint32_t ch=(e&f)^(~e&g), t1=q+s1+ch+host_sha_k[i]+w[i];
        uint32_t s0=host_rotr(a,2u)^host_rotr(a,13u)^host_rotr(a,22u);
        uint32_t maj=(a&b)^(a&c)^(b&c), t2=s0+maj;
        q=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=q;
}
static void host_sha_init(host_sha256 *s)
{
    static const uint32_t initial[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    memcpy(s->h,initial,sizeof initial); s->used=0u; s->total=0u;
}
static void host_sha_update(host_sha256 *s, const void *data, size_t len)
{
    const uint8_t *p=(const uint8_t *)data; s->total+=(uint64_t)len;
    while(len>0u) {
        size_t take=64u-(size_t)s->used; if(take>len) take=len;
        memcpy(s->block+s->used,p,take); s->used+=(uint32_t)take; p+=take; len-=take;
        if(s->used==64u) { host_sha_compress(s->h,s->block); s->used=0u; }
    }
}
static void host_sha_final(host_sha256 *s, uint8_t out[32])
{
    uint64_t bits=s->total*8u; s->block[s->used++]=0x80u;
    if(s->used>56u) { memset(s->block+s->used,0,64u-(size_t)s->used); host_sha_compress(s->h,s->block); s->used=0u; }
    memset(s->block+s->used,0,56u-(size_t)s->used);
    for(uint32_t i=0u;i<8u;i++) s->block[56u+i]=(uint8_t)(bits>>(56u-8u*i));
    host_sha_compress(s->h,s->block);
    for(uint32_t i=0u;i<8u;i++) { out[4u*i]=(uint8_t)(s->h[i]>>24); out[4u*i+1u]=(uint8_t)(s->h[i]>>16);
        out[4u*i+2u]=(uint8_t)(s->h[i]>>8); out[4u*i+3u]=(uint8_t)s->h[i]; }
}
static void host_sha_digest(const void *bytes,size_t len,uint8_t out[32])
{ host_sha256 s; host_sha_init(&s); host_sha_update(&s,bytes,len); host_sha_final(&s,out); }
static void host_sha_hex(const uint8_t digest[32], char out[65])
{
    static const char digits[]="0123456789abcdef";
    for(uint32_t i=0u;i<32u;i++){out[2u*i]=digits[digest[i]>>4];out[2u*i+1u]=digits[digest[i]&0x0fu];}
    out[64]='\0';
}

typedef struct symbols {
    uint32_t result;
    uint32_t pixels;
    uint32_t pixels_end;
} symbols;

typedef struct machine_storage {
    void *allocation;
    lynx_machine *machine;
} machine_storage;

static const uint8_t expected_pixels[PIXEL_BYTES] = {
    0x01u, 0x23u, 0x45u, 0x67u, 0x23u, 0x45u, 0x67u, 0x89u,
    0x45u, 0x67u, 0x89u, 0xABu, 0x67u, 0x89u, 0xABu, 0xCDu,
    0x89u, 0xABu, 0xCDu, 0xEFu, 0xABu, 0xCDu, 0xEFu, 0x01u,
    0xCDu, 0xEFu, 0x01u, 0x23u, 0xEFu, 0x01u, 0x23u, 0x45u
};

/* The diagnostic-owned palette is not a Lynx palette or display model. */
static const uint8_t diagnostic_palette[16][3] = {
    {0x00u, 0xFFu, 0x00u}, {0x11u, 0xEEu, 0x33u}, {0x22u, 0xDDu, 0x66u},
    {0x33u, 0xCCu, 0x99u}, {0x44u, 0xBBu, 0xCCu}, {0x55u, 0xAAu, 0xFFu},
    {0x66u, 0x99u, 0x22u}, {0x77u, 0x88u, 0x55u}, {0x88u, 0x77u, 0x88u},
    {0x99u, 0x66u, 0xBBu}, {0xAAu, 0x55u, 0xEEu}, {0xBBu, 0x44u, 0x11u},
    {0xCCu, 0x33u, 0x44u}, {0xDDu, 0x22u, 0x77u}, {0xEEu, 0x11u, 0xAAu},
    {0xFFu, 0x00u, 0xDDu}
};

static int report_error(const char *message)
{
    fprintf(stderr, "lynx-consume: %s\n", message);
    return 1;
}

static int usage_error(const char *reason)
{
    fprintf(stderr, "lynx-consume: %s\n"
                    "Usage: lynx-consume --content BLL --symbols LABELS --out NEW-DIRECTORY [--profile none|lynx1|lynx2]\n"
                    "   or: lynx-consume --mode frames --content BLL --symbols LABELS --timeline FILE --out NEW-DIRECTORY [--profile none|lynx1|lynx2]\n"
                    "Provide a complete BS93 BLL and the matching ld65 -Ln symbol file.\n",
            reason);
    return 2;
}

static int read_bounded(const char *path, size_t limit, uint8_t **out, size_t *length,
                        const char *what)
{
    FILE *file = fopen(path, "rb");
    uint8_t *bytes;
    size_t got;
    int extra;
    int failed;
    if (file == NULL) {
        fprintf(stderr, "lynx-consume: cannot open %s; check that the supplied path names a readable file\n", what);
        return 1;
    }
    bytes = malloc(limit == 0u ? 1u : limit);
    if (bytes == NULL) {
        (void)fclose(file);
        fprintf(stderr, "lynx-consume: cannot allocate the bounded %s buffer; reduce the input size\n", what);
        return 1;
    }
    got = fread(bytes, 1u, limit, file);
    extra = fgetc(file);
    failed = ferror(file) != 0;
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed != 0 || extra != EOF) {
        free(bytes);
        fprintf(stderr, "lynx-consume: %s exceeds its %zu-byte bound or could not be read; provide a complete bounded file\n",
                what, limit);
        return 1;
    }
    *out = bytes;
    *length = got;
    return 0;
}

static int parse_hex_address(const char *text, size_t length, uint32_t *out)
{
    size_t index;
    uint32_t value = 0u;
    if (length == 0u || length > 6u) {
        return 1;
    }
    for (index = 0u; index < length; index++) {
        unsigned digit;
        unsigned char ch = (unsigned char)text[index];
        if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
            digit = (unsigned)(ch - (unsigned char)'0');
        } else if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') {
            digit = (unsigned)(ch - (unsigned char)'a') + 10u;
        } else if (ch >= (unsigned char)'A' && ch <= (unsigned char)'F') {
            digit = (unsigned)(ch - (unsigned char)'A') + 10u;
        } else {
            return 1;
        }
        value = (value << 4) | (uint32_t)digit;
    }
    if (value > LYNX_ADDRESS_SPACE) {
        return 1;
    }
    *out = value;
    return 0;
}

static int target_symbol(const char *text, size_t length, size_t *which)
{
    static const char *const names[] = {"diag_result", "diag_pixels", "diag_pixels_end"};
    size_t index;
    if (length > 0u && text[0] == '.') {
        text++;
        length--;
    }
    for (index = 0u; index < sizeof names / sizeof names[0]; index++) {
        if (strlen(names[index]) == length && memcmp(text, names[index], length) == 0) {
            *which = index;
            return 1;
        }
    }
    return 0;
}

static int parse_linked_symbols(const uint8_t *text, size_t length, symbols *out,
                                char *reason, size_t reason_cap)
{
    uint32_t address[3] = {0u, 0u, 0u};
    unsigned found[3] = {0u, 0u, 0u};
    size_t offset = 0u;
    while (offset < length) {
        char line[SYMBOL_LINE_LIMIT + 1];
        size_t line_len = 0u;
        size_t starts[4] = {0u, 0u, 0u, 0u};
        size_t lengths[4] = {0u, 0u, 0u, 0u};
        size_t token_count = 0u;
        size_t cursor;
        size_t target = 0u;
        int relevant = 0;
        while (offset + line_len < length && text[offset + line_len] != (uint8_t)'\n') {
            line_len++;
        }
        if (line_len > SYMBOL_LINE_LIMIT || memchr(text + offset, '\0', line_len) != NULL ||
            memchr(text + offset, '\r', line_len) != NULL) {
            (void)snprintf(reason, reason_cap, "symbol line is overlong or contains a forbidden control byte");
            return 1;
        }
        memcpy(line, text + offset, line_len);
        line[line_len] = '\0';
        offset += line_len;
        if (offset < length && text[offset] == (uint8_t)'\n') {
            offset++;
        }
        cursor = 0u;
        while (cursor < line_len) {
            size_t start;
            while (cursor < line_len && isspace((unsigned char)line[cursor]) != 0) {
                cursor++;
            }
            if (cursor == line_len) {
                break;
            }
            start = cursor;
            while (cursor < line_len && isspace((unsigned char)line[cursor]) == 0) {
                cursor++;
            }
            if (token_count < 4u) {
                starts[token_count] = start;
                lengths[token_count] = cursor - start;
            }
            token_count++;
        }
        for (cursor = 0u; cursor < token_count && cursor < 4u; cursor++) {
            if (target_symbol(line + starts[cursor], lengths[cursor], &target) != 0) {
                relevant = 1;
            }
        }
        if (relevant == 0) {
            continue;
        }
        if (token_count != 3u || lengths[0] != 2u || memcmp(line + starts[0], "al", 2u) != 0 ||
            target_symbol(line + starts[2], lengths[2], &target) == 0 ||
            parse_hex_address(line + starts[1], lengths[1], &address[target]) != 0) {
            (void)snprintf(reason, reason_cap, "linked diagnostic symbol row is malformed");
            return 1;
        }
        if (found[target] != 0u) {
            (void)snprintf(reason, reason_cap, "linked diagnostic symbol is duplicated");
            return 1;
        }
        found[target] = 1u;
    }
    if (found[0] == 0u || found[1] == 0u || found[2] == 0u) {
        (void)snprintf(reason, reason_cap, "linked symbol file is missing diag_result, diag_pixels, or diag_pixels_end");
        return 1;
    }
    if (address[0] > LYNX_ADDRESS_SPACE - RESULT_BYTES ||
        address[1] > LYNX_ADDRESS_SPACE - PIXEL_BYTES ||
        address[2] > LYNX_ADDRESS_SPACE || address[2] < address[1] ||
        address[2] - address[1] != PIXEL_BYTES) {
        (void)snprintf(reason, reason_cap, "linked result/pixel symbol extent is outside 64 KiB or has the wrong size");
        return 1;
    }
    if (!(address[0] + RESULT_BYTES <= address[1] || address[2] <= address[0])) {
        (void)snprintf(reason, reason_cap, "linked result and pixel symbol ranges overlap");
        return 1;
    }
    out->result = address[0];
    out->pixels = address[1];
    out->pixels_end = address[2];
    reason[0] = '\0';
    return 0;
}

static int parse_decimal(const uint8_t *text, size_t length, uint64_t maximum, uint64_t *out)
{
    uint64_t value = 0u;
    if (length == 0u || (length > 1u && text[0] == (uint8_t)'0')) return 1;
    for (size_t i = 0u; i < length; i++) {
        if (text[i] < (uint8_t)'0' || text[i] > (uint8_t)'9') return 1;
        const uint32_t digit = (uint32_t)(text[i] - (uint8_t)'0');
        if (value > (maximum - digit) / 10u) return 1;
        value = value * 10u + digit;
    }
    *out = value;
    return 0;
}

static int parse_input_timeline(const uint8_t *bytes, size_t length, input_timeline *out)
{
    static const uint8_t header[] = "LYNX-INPUT 1\n";
    size_t offset = sizeof header - 1u;
    uint64_t previous = 0u;
    bool has_previous = false;
    memset(out, 0, sizeof *out);
    if (length < sizeof header - 1u || memcmp(bytes, header, sizeof header - 1u) != 0) return 1;
    out->events = calloc(TIMELINE_RECORD_LIMIT, sizeof *out->events);
    if (out->events == NULL) return 1;
    while (offset < length) {
        size_t end = offset, space = SIZE_MAX;
        uint64_t tick = 0u, mask = 0u;
        while (end < length && bytes[end] != (uint8_t)'\n') {
            if (bytes[end] == (uint8_t)'\r' || bytes[end] == 0u) goto invalid;
            if (bytes[end] == (uint8_t)' ') {
                if (space != SIZE_MAX) goto invalid;
                space = end;
            }
            end++;
        }
        if (end == length || end == offset || space == SIZE_MAX || space == offset || space + 1u == end ||
            out->count >= TIMELINE_RECORD_LIMIT ||
            parse_decimal(bytes + offset, space - offset, UINT64_MAX - UINT64_C(65536), &tick) != 0 ||
            parse_decimal(bytes + space + 1u, end - space - 1u, LYNX_INPUT_MASK, &mask) != 0 ||
            (has_previous && tick <= previous)) goto invalid;
        out->events[out->count].tick = tick;
        out->events[out->count].mask = (uint32_t)mask;
        out->count++;
        previous = tick;
        has_previous = true;
        offset = end + 1u;
    }
    return 0;
invalid:
    free(out->events);
    memset(out, 0, sizeof *out);
    return 1;
}

static int parse_named_symbol(const uint8_t *text, size_t length, const char *name,
                              uint32_t *address)
{
    size_t offset = 0u;
    unsigned found = 0u;
    while (offset < length) {
        size_t end = offset, starts[3] = {0u,0u,0u}, lengths[3] = {0u,0u,0u};
        size_t tokens = 0u, cursor;
        uint32_t candidate = 0u;
        while (end < length && text[end] != (uint8_t)'\n') end++;
        if (end - offset > SYMBOL_LINE_LIMIT || memchr(text + offset, 0, end - offset) != NULL ||
            memchr(text + offset, '\r', end - offset) != NULL) return 1;
        cursor = offset;
        while (cursor < end) {
            size_t start;
            while (cursor < end && isspace((unsigned char)text[cursor]) != 0) cursor++;
            if (cursor == end) break;
            start = cursor;
            while (cursor < end && isspace((unsigned char)text[cursor]) == 0) cursor++;
            if (tokens < 3u) { starts[tokens] = start; lengths[tokens] = cursor - start; }
            tokens++;
        }
        if (tokens >= 3u) {
            size_t symbol_index = 2u;
            const uint8_t *symbol = text + starts[symbol_index];
            size_t symbol_len = lengths[symbol_index];
            if (symbol_len > 0u && symbol[0] == (uint8_t)'.') { symbol++; symbol_len--; }
            if (symbol_len == strlen(name) && memcmp(symbol, name, symbol_len) == 0) {
                if (tokens != 3u || lengths[0] != 2u || memcmp(text + starts[0], "al", 2u) != 0 ||
                    parse_hex_address((const char *)text + starts[1], lengths[1], &candidate) != 0 ||
                    found != 0u) return 1;
                *address = candidate;
                found = 1u;
            }
        }
        offset = end < length ? end + 1u : end;
    }
    return found == 1u ? 0 : 1;
}

static int create_machine(const uint8_t *bll, size_t bll_len, uint32_t profile,
                          uint32_t timing_policy,
                          machine_storage *storage, lynx_identity *identity)
{
    lynx_config config;
    size_t size = lynx_machine_size();
    size_t alignment = lynx_machine_align();
    uintptr_t base_address;
    size_t offset;
    void *aligned;
    memset(storage, 0, sizeof *storage);
    if (size == 0u || alignment == 0u || size > SIZE_MAX - (alignment - 1u)) {
        return report_error("public machine size/alignment is invalid; rebuild against the supported C API");
    }
    storage->allocation = malloc(size + alignment - 1u);
    if (storage->allocation == NULL) {
        return report_error("cannot allocate public machine storage; close other work or reduce memory pressure");
    }
    base_address = (uintptr_t)storage->allocation;
    offset = (size_t)((alignment - (base_address % alignment)) % alignment);
    aligned = (uint8_t *)storage->allocation + offset;
    lynx_config_default(&config);
    config.profile = profile;
    config.ram_policy = LYNX_RAM_POLICY_ZERO;
    config.timing_policy = timing_policy;
    if (lynx_machine_init(aligned, size, &config, &storage->machine) != LYNX_OK) {
        free(storage->allocation);
        memset(storage, 0, sizeof *storage);
        return report_error("public machine initialization refused; use a supported profile and valid aligned storage");
    }
    if (lynx_load_bll(storage->machine, bll, bll_len) != LYNX_OK) {
        lynx_machine_deinit(storage->machine);
        free(storage->allocation);
        memset(storage, 0, sizeof *storage);
        return report_error("strict BS93 load refused; provide one complete exact-length tier-A BLL object");
    }
    if (lynx_get_identity(storage->machine, identity) != LYNX_OK ||
        identity->has_content != 1u || identity->content_format != LYNX_CONTENT_BS93 ||
        identity->startup_tier != LYNX_STARTUP_A || identity->profile != profile ||
        identity->ram_policy != LYNX_RAM_POLICY_ZERO || identity->timing_policy != timing_policy ||
        identity->object_len != bll_len ||
        identity->entry_addr != identity->content_addr) {
        lynx_machine_deinit(storage->machine);
        free(storage->allocation);
        memset(storage, 0, sizeof *storage);
        return report_error("public BLL identity disagrees with the accepted object; verify the core/API build");
    }
    return 0;
}

static void destroy_machine(machine_storage *storage)
{
    if (storage->machine != NULL) {
        lynx_machine_deinit(storage->machine);
    }
    free(storage->allocation);
    memset(storage, 0, sizeof *storage);
}

static const char *stop_name(lynx_stop_reason stop)
{
    switch (stop) {
    case LYNX_STOP_BUDGET: return "budget";
    case LYNX_STOP_FRAME: return "frame";
    case LYNX_STOP_WATCH: return "watch";
    case LYNX_STOP_HALT: return "halt";
    case LYNX_STOP_UNDEFINED: return "undefined opcode";
    case LYNX_STOP_UNAVAILABLE: return "unavailable CPU route";
    default: return "unknown stop";
    }
}

static int advance_diagnostic(lynx_machine *machine, const symbols *where,
                              uint64_t *final_tick)
{
    uint64_t ticks = 0u;
    uint8_t result[RESULT_BYTES];
    while (ticks < SNAPSHOT_TICK) {
        uint64_t budget = SNAPSHOT_TICK - ticks;
        uint64_t consumed = 0u;
        uint64_t after = 0u;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        if (budget > ADVANCE_SLICE) {
            budget = ADVANCE_SLICE;
        }
        if (lynx_advance(machine, budget, &consumed, &stop) != LYNX_OK || consumed > budget ||
            lynx_guest_ticks(machine, &after) != LYNX_OK) {
            return report_error("public advance or guest-time read failed; inspect the accepted BLL and core build");
        }
        if (stop != LYNX_STOP_BUDGET) {
            fprintf(stderr, "lynx-consume: guest stopped at %s before tick %u; use the original diagnostic with a compatible profile/map\n",
                    stop_name(stop), SNAPSHOT_TICK);
            return 1;
        }
        if (after <= ticks || after - ticks != consumed || after > SNAPSHOT_TICK) {
            return report_error("guest made no bounded time progress; check the diagnostic input and profile");
        }
        ticks = after;
        if (lynx_read_memory(machine, where->result, result, sizeof result) != LYNX_OK) {
            return report_error("public raw-RAM result read failed; verify linked result symbol bounds");
        }
        if (memcmp(result, "LXD3", 4u) == 0 &&
            (result[4] != 1u || result[5] > 3u || result[6] > 1u ||
             (result[7] == 0xA5u && result[5] != 3u))) {
            return report_error("guest result schema/progress/error/done bytes are invalid; rebuild the diagnostic fixture");
        }
    }
    if (ticks != SNAPSHOT_TICK) {
        return report_error("guest did not reach the fixed snapshot tick; use the supported tick-aligned diagnostic contract");
    }
    *final_tick = ticks;
    return 0;
}

static int validate_guest(const lynx_machine *machine, const symbols *where,
                          uint8_t pixels[PIXEL_BYTES], uint64_t *tick)
{
    uint8_t result[RESULT_BYTES];
    uint32_t mapctl = 0u;
    if (lynx_read_memory(machine, where->result, result, sizeof result) != LYNX_OK) {
        return report_error("cannot read the linked result block; verify diag_result in the -Ln file");
    }
    if (memcmp(result, "LXD3", 4u) != 0) {
        return report_error("guest result magic is missing; run the original tier-A diagnostic object");
    }
    if (result[4] != 1u || result[5] != 3u || result[6] != 0u || result[7] != 0xA5u) {
        return report_error("guest result is incomplete or reports an error; check the original diagnostic and MAPCTL guest initialization");
    }
    if (lynx_read_memory(machine, where->pixels, pixels, PIXEL_BYTES) != LYNX_OK) {
        return report_error("cannot read linked diagnostic pixels; verify diag_pixels extent in the -Ln file");
    }
    if (memcmp(pixels, expected_pixels, sizeof expected_pixels) != 0) {
        return report_error("guest packed pixels differ from the independent 8x8 contract; rebuild the original diagnostic fixture");
    }
    if (lynx_read_register(machine, LYNX_REG_MAPCTL, &mapctl) != LYNX_OK || mapctl != 0x0Cu) {
        return report_error("guest did not select RAM for ROM/vectors through its own MAPCTL write; check the diagnostic source");
    }
    if (lynx_guest_ticks(machine, tick) != LYNX_OK || *tick != SNAPSHOT_TICK) {
        return report_error("guest-time identity does not match the snapshot endpoint; rerun with the fixed 5000-tick bound");
    }
    return 0;
}

static void encode_hex(const uint8_t bytes[32], char text[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;
    for (index = 0u; index < 32u; index++) {
        text[index * 2u] = digits[bytes[index] >> 4];
        text[index * 2u + 1u] = digits[bytes[index] & 0x0Fu];
    }
    text[64] = '\0';
}

static int append_text(char *buffer, size_t cap, size_t *used, const char *format, ...)
{
    va_list args;
    int written;
    if (*used >= cap) {
        return 1;
    }
    va_start(args, format);
    written = vsnprintf(buffer + *used, cap - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= cap - *used) {
        return 1;
    }
    *used += (size_t)written;
    return 0;
}

static int build_ppm(const uint8_t pixels[PIXEL_BYTES], uint64_t tick,
                     const char *profile, const char hash[65], char out[PPM_CAP],
                     size_t *out_len)
{
    size_t used = 0u;
    unsigned y;
    if (append_text(out, PPM_CAP, &used,
        "P3\n"
        "# artifact = SNAPSHOT\n"
        "# guest_tick = %" PRIu64 "\n"
        "# startup = A\n"
        "# profile = %s\n"
        "# content_sha256 = %s\n"
        "# palette = diagnostic-palette-v1\n"
        "# interpretation = diagnostic-owned pixels; no canonical Lynx video claim\n"
        "8 8\n255\n", tick, profile, hash) != 0) {
        return report_error("P3 metadata exceeds its output bound; shorten the build identity or profile field");
    }
    for (y = 0u; y < 8u; y++) {
        unsigned x;
        for (x = 0u; x < 8u; x++) {
            size_t pixel_index = (size_t)y * 8u + x;
            uint8_t pair = pixels[pixel_index / 2u];
            unsigned pen = pixel_index % 2u == 0u ? (unsigned)(pair >> 4)
                                                   : (unsigned)(pair & 0x0Fu);
            const char *end = x == 7u ? "\n" : " ";
            if (append_text(out, PPM_CAP, &used, "%u %u %u%s",
                            (unsigned)diagnostic_palette[pen][0],
                            (unsigned)diagnostic_palette[pen][1],
                            (unsigned)diagnostic_palette[pen][2], end) != 0) {
                return report_error("P3 pixel data exceeds its fixed output bound; verify the 8x8 diagnostic extent");
            }
        }
    }
    *out_len = used;
    return 0;
}

static int build_manifest(uint64_t tick, const char *profile, const char hash[65],
                          char out[METADATA_CAP], size_t *out_len)
{
    int n = snprintf(out, METADATA_CAP,
        "artifact = SNAPSHOT\n"
        "image_format = P3\n"
        "width = 8\n"
        "height = 8\n"
        "guest_tick = %" PRIu64 "\n"
        "startup = A\n"
        "profile = %s\n"
        "content_sha256 = %s\n"
        "palette = diagnostic-palette-v1\n"
        "interpretation = diagnostic-owned pixels; no canonical Lynx video claim\n"
        "audio = placeholder silence pending Phase6\n"
        "wav = PCM mono 8000Hz signed16le; 800 zero samples\n",
        tick, profile, hash);
    if (n < 0 || (size_t)n >= METADATA_CAP) {
        return report_error("snapshot metadata exceeds its fixed field bound; use the supported profile names");
    }
    *out_len = (size_t)n;
    return 0;
}

static void store_u16le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8);
}

static void store_u32le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)(value >> 24);
}

static void build_silence_wav(uint8_t wav[WAV_BYTES])
{
    memset(wav, 0, WAV_BYTES);
    memcpy(wav, "RIFF", 4u);
    store_u32le(wav + 4u, (uint32_t)(WAV_BYTES - 8));
    memcpy(wav + 8u, "WAVEfmt ", 8u);
    store_u32le(wav + 16u, 16u);
    store_u16le(wav + 20u, 1u);
    store_u16le(wav + 22u, 1u);
    store_u32le(wav + 24u, 8000u);
    store_u32le(wav + 28u, 16000u);
    store_u16le(wav + 32u, 2u);
    store_u16le(wav + 34u, 16u);
    memcpy(wav + 36u, "data", 4u);
    store_u32le(wav + 40u, WAV_DATA_BYTES);
}

static int output_path(const char *directory, const char *name, char *path, size_t cap)
{
    int length = snprintf(path, cap, "%s/%s", directory, name);
    return length < 0 || (size_t)length >= cap ? 1 : 0;
}

static int write_exclusive(const char *path, const void *bytes, size_t length)
{
    FILE *file = fopen(path, "wbx");
    int failed;
#if defined(LYNX_CONSUMER_TESTING)
    const char *injected = getenv("LYNX_TEST_IO_FAILURE");
    int inject_write = injected != NULL && strcmp(injected, "write") == 0;
    int inject_close = injected != NULL && strcmp(injected, "close") == 0;
#endif
    if (file == NULL) {
        return 1;
    }
#if defined(LYNX_CONSUMER_TESTING)
    failed = inject_write != 0 || fwrite(bytes, 1u, length, file) != length;
#else
    failed = fwrite(bytes, 1u, length, file) != length;
#endif
    if (fclose(file) != 0) {
        failed = 1;
    }
#if defined(LYNX_CONSUMER_TESTING)
    if (inject_close != 0) {
        failed = 1;
    }
#endif
    if (failed != 0) {
        if (unlink(path) != 0 && errno != ENOENT) {
            return 2;
        }
        return 1;
    }
    return 0;
}

static int normalize_destination(const char *directory, char *normalized,
                                 size_t capacity)
{
    size_t length = strlen(directory);
    while (length > 1u && directory[length - 1u] == '/') {
        length--;
    }
    if (length == 0u || length >= capacity) {
        return 1;
    }
    memcpy(normalized, directory, length);
    normalized[length] = '\0';
    return 0;
}

static int destination_is_absent(const char *directory)
{
    struct stat info;
    if (lstat(directory, &info) == 0) {
        return report_error("output directory already exists; choose a new empty --out path (existing files were not changed)");
    }
    if (errno != ENOENT) {
        return report_error("cannot inspect the output path; check its parent path and permissions");
    }
    return 0;
}

static int publish_directory_exclusive(const char *staging, const char *directory)
{
#if defined(__APPLE__)
    return renamex_np(staging, directory, RENAME_EXCL);
#elif defined(__linux__)
    return renameat2(AT_FDCWD, staging, AT_FDCWD, directory, RENAME_NOREPLACE);
#else
    (void)staging;
    (void)directory;
    errno = ENOTSUP;
    return -1;
#endif
}

#if defined(LYNX_CONSUMER_TESTING)
static int publication_test_fd(const char *name, int *out)
{
    const char *text = getenv(name);
    char *end = NULL;
    long value;
    if (text == NULL || text[0] == '\0') {
        return 1;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value > INT_MAX) {
        return 1;
    }
    *out = (int)value;
    return 0;
}

static int publication_test_checkpoint(const char *name, uint32_t event)
{
    const char *requested = getenv("LYNX_TEST_PUBLICATION_STOP");
    struct pollfd resume_event;
    int ready_fd;
    int resume_fd;
    int poll_result;
    char release;
    ssize_t written;
    if (requested == NULL || strcmp(requested, name) != 0) {
        return 0;
    }
    if (publication_test_fd("LYNX_TEST_PUBLICATION_READY_FD", &ready_fd) != 0 ||
        publication_test_fd("LYNX_TEST_PUBLICATION_RESUME_FD", &resume_fd) != 0) {
        return report_error("test publication checkpoint has invalid synchronization descriptors");
    }
    do {
        written = write(ready_fd, &event, sizeof event);
    } while (written < 0 && errno == EINTR);
    if (written != (ssize_t)sizeof event) {
        return report_error("test publication checkpoint could not signal readiness");
    }
    resume_event.fd = resume_fd;
    resume_event.events = POLLIN;
    resume_event.revents = 0;
    do {
        poll_result = poll(&resume_event, 1u, 30000);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result == 0) {
        return report_error("test publication checkpoint timed out waiting for its parent");
    }
    if (poll_result < 0 || (resume_event.revents & POLLIN) == 0 ||
        read(resume_fd, &release, 1u) != 1 || release != 'r') {
        return report_error("test publication checkpoint received no valid resume command");
    }
    return 0;
}
#endif

static int publish_outputs(const char *directory, const char *ppm, size_t ppm_len,
                           const uint8_t wav[WAV_BYTES], const char *manifest,
                           size_t manifest_len)
{
    char normalized[4096];
    char staging[4096];
    char ppm_path[4096];
    char wav_path[4096];
    char manifest_path[4096];
    char stage_probe[4096];
    int stage_length;
    int made_staging = 0;
    int made_ppm = 0;
    int made_wav = 0;
    int made_manifest = 0;
    int write_result;

    if (normalize_destination(directory, normalized, sizeof normalized) != 0) {
        return report_error("output directory path exceeds its bound; choose a shorter --out path");
    }
    if (output_path(normalized, "snapshot.ppm", stage_probe, sizeof stage_probe) != 0 ||
        output_path(normalized, "silence.wav", stage_probe, sizeof stage_probe) != 0 ||
        output_path(normalized, "snapshot.manifest", stage_probe, sizeof stage_probe) != 0) {
        return report_error("output file path exceeds its bound; choose a shorter --out path");
    }
    if (destination_is_absent(normalized) != 0) {
        return 1;
    }
    stage_length = snprintf(staging, sizeof staging,
                            "%s.lynxplosion-stage-XXXXXX", normalized);
    if (stage_length < 0 || (size_t)stage_length >= sizeof staging ||
        output_path(staging, "snapshot.ppm", stage_probe, sizeof stage_probe) != 0 ||
        output_path(staging, "silence.wav", stage_probe, sizeof stage_probe) != 0 ||
        output_path(staging, "snapshot.manifest", stage_probe, sizeof stage_probe) != 0) {
        return report_error("output staging path exceeds its bound; choose a shorter --out path");
    }
    if (mkdtemp(staging) == NULL) {
        return report_error("cannot create a private sibling staging directory; create its parent and choose a writable --out path");
    }
    made_staging = 1;
    if (output_path(staging, "snapshot.ppm", ppm_path, sizeof ppm_path) != 0 ||
        output_path(staging, "silence.wav", wav_path, sizeof wav_path) != 0 ||
        output_path(staging, "snapshot.manifest", manifest_path, sizeof manifest_path) != 0) {
        (void)report_error("staged output path exceeds its bound; choose a shorter --out path");
        goto failed;
    }
    write_result = write_exclusive(ppm_path, ppm, ppm_len);
    if (write_result != 0) {
        (void)report_error("cannot write snapshot.ppm exclusively; check output permissions and use a new directory");
        goto failed;
    }
    made_ppm = 1;
    write_result = write_exclusive(wav_path, wav, WAV_BYTES);
    if (write_result != 0) {
        (void)report_error("cannot write silence.wav exclusively; check output permissions and use a new directory");
        goto failed;
    }
    made_wav = 1;
#if defined(LYNX_CONSUMER_TESTING)
    if (publication_test_checkpoint("before-manifest", 1u) != 0) {
        goto failed;
    }
#endif
    write_result = write_exclusive(manifest_path, manifest, manifest_len);
    if (write_result != 0) {
        (void)report_error("cannot write snapshot.manifest exclusively; check output permissions and use a new directory");
        goto failed;
    }
    made_manifest = 1;
#if defined(LYNX_CONSUMER_TESTING)
    if (publication_test_checkpoint("before-publish", 2u) != 0) {
        goto failed;
    }
#endif
    if (publish_directory_exclusive(staging, normalized) != 0) {
        if (errno == EEXIST) {
            (void)report_error("output directory already exists; choose a new empty --out path (existing files were not changed)");
        } else {
            (void)report_error("cannot publish the complete output directory without replacement; use a filesystem and platform that support exclusive directory rename");
        }
        goto failed;
    }
    made_staging = 0;
    return 0;
failed:
    {
        int cleanup_failed = 0;
        if (made_manifest != 0) {
            if (unlink(manifest_path) != 0 && errno != ENOENT) {
                cleanup_failed = 1;
            }
        }
        if (made_wav != 0) {
            if (unlink(wav_path) != 0 && errno != ENOENT) {
                cleanup_failed = 1;
            }
        }
        if (made_ppm != 0) {
            if (unlink(ppm_path) != 0 && errno != ENOENT) {
                cleanup_failed = 1;
            }
    }
        if (made_staging != 0 && rmdir(staging) != 0 && errno != ENOENT) {
            cleanup_failed = 1;
        }
        if (cleanup_failed != 0) {
            (void)report_error("could not remove this invocation's private staging files; inspect the destination parent and remove only the named staging path");
        }
    }
    return 1;
}

static int parse_profile(const char *text, uint32_t *profile)
{
    if (strcmp(text, "none") == 0) {
        *profile = LYNX_PROFILE_NONE;
        return 0;
    }
    if (strcmp(text, "lynx1") == 0) {
        *profile = LYNX_PROFILE_LYNX1;
        return 0;
    }
    if (strcmp(text, "lynx2") == 0) {
        *profile = LYNX_PROFILE_LYNX2;
        return 0;
    }
    return 1;
}

static int run_consumer(const char *content_path, const char *symbols_path,
                        const char *output_directory, uint32_t profile)
{
    uint8_t *bll = NULL;
    uint8_t *label_bytes = NULL;
    size_t bll_len = 0u;
    size_t label_len = 0u;
    char reason[160];
    symbols linked;
    machine_storage storage = {0};
    lynx_identity identity;
    uint8_t pixels[PIXEL_BYTES];
    uint64_t guest_tick = 0u;
    char hash[65];
    char ppm[PPM_CAP];
    size_t ppm_len = 0u;
    char manifest[METADATA_CAP];
    size_t manifest_len = 0u;
    uint8_t wav[WAV_BYTES];
    int result = 1;

    if (read_bounded(content_path, BLL_LIMIT, &bll, &bll_len, "BLL content") != 0) {
        goto done;
    }
    if (bll_len < 11u) {
        (void)report_error("BLL content is too short; provide a non-empty complete BS93 object with its 10-byte header");
        goto done;
    }
    if (read_bounded(symbols_path, SYMBOL_LIMIT, &label_bytes, &label_len,
                     "ld65 -Ln symbols") != 0) {
        goto done;
    }
    if (parse_linked_symbols(label_bytes, label_len, &linked, reason, sizeof reason) != 0) {
        fprintf(stderr, "lynx-consume: %s; rebuild the exact object with ld65 -Ln and pass its matching label file\n", reason);
        goto done;
    }
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_LEGACY_FLAT5,
                       &storage, &identity) != 0) {
        goto done;
    }
    if (advance_diagnostic(storage.machine, &linked, &guest_tick) != 0 ||
        validate_guest(storage.machine, &linked, pixels, &guest_tick) != 0) {
        goto done;
    }
    encode_hex(identity.content_sha256, hash);
    if (build_ppm(pixels, guest_tick, lynx_profile_name(profile), hash, ppm, &ppm_len) != 0 ||
        build_manifest(guest_tick, lynx_profile_name(profile), hash, manifest, &manifest_len) != 0) {
        goto done;
    }
    build_silence_wav(wav);
    if (publish_outputs(output_directory, ppm, ppm_len, wav, manifest, manifest_len) != 0) {
        goto done;
    }
    printf("lynx-consume: wrote diagnostic SNAPSHOT P3 and placeholder silence WAV to %s; silence is pending Phase6 audio\n",
           output_directory);
    result = 0;
done:
    free(bll);
    free(label_bytes);
    destroy_machine(&storage);
    return result;
}

static int frame_ppm(const uint8_t *pens, const uint8_t *palettes,
                     const lynx_frame_result *frame, uint32_t input_mask,
                     uint32_t sampled_a, const char *input_hash,
                     uint8_t **out, size_t *out_len)
{
    const size_t raster_len = (size_t)LYNX_FRAME_WIDTH * frame->row_count * 3u;
    char header[512];
    int header_len;
    uint8_t *bytes;
    size_t at;
    if (frame->row_count == 0u || frame->row_count > LYNX_FRAME_MAX_ROWS ||
        raster_len > FRAME_PPM_LIMIT - sizeof header) return report_error("frame geometry is outside the bounded PPM surface");
    header_len = snprintf(header, sizeof header,
        "P6\n# artifact = FRAME\n# sequence = %" PRIu64 "\n"
        "# frame_start_tick = %" PRIu64 "\n# frame_end_tick = %" PRIu64 "\n"
        "# rows = %u\n# input_mask = %u\n# sample_a = %u\n# input_sha256 = %s\n"
        "160 %u\n255\n", frame->sequence, frame->frame_start_tick,
        frame->frame_end_tick, frame->row_count, input_mask, sampled_a, input_hash,
        frame->row_count);
    if (header_len < 0 || (size_t)header_len >= sizeof header ||
        raster_len > SIZE_MAX - (size_t)header_len) return report_error("frame PPM header exceeds its fixed bound");
    bytes = malloc((size_t)header_len + raster_len);
    if (bytes == NULL) return report_error("cannot allocate the bounded raw frame PPM");
    memcpy(bytes, header, (size_t)header_len);
    at = (size_t)header_len;
    for (size_t y = 0u; y < frame->row_count; y++) {
        const uint8_t *row = pens + y * LYNX_FRAME_ROW_BYTES;
        const uint8_t *palette = palettes + y * 32u;
        for (size_t x = 0u; x < LYNX_FRAME_WIDTH; x++) {
            const uint8_t pair = row[x / 2u];
            const uint8_t pen = (x & 1u) == 0u ? (uint8_t)(pair >> 4) : (uint8_t)(pair & 0x0Fu);
            const uint8_t green = (uint8_t)(palette[pen] & 0x0Fu);
            const uint8_t blue_red = palette[16u + pen];
            bytes[at++] = (uint8_t)((blue_red & 0x0Fu) * 17u);
            bytes[at++] = (uint8_t)(green * 17u);
            bytes[at++] = (uint8_t)((blue_red >> 4) * 17u);
        }
    }
    *out = bytes;
    *out_len = at;
    return 0;
}

static int publish_frame_outputs(const char *directory, const uint8_t *released,
                                 size_t released_len, const uint8_t *pressed,
                                 size_t pressed_len, const char *manifest,
                                 size_t manifest_len)
{
    char normalized[4096], staging[4096], path[3][4096], probe[4096];
    const char *names[3] = {"released.ppm", "pressed.ppm", "frame-trace.manifest"};
    const void *data[3] = {released, pressed, manifest};
    size_t lengths[3] = {released_len, pressed_len, manifest_len};
    int n;
    int made = 0;
    if (normalize_destination(directory, normalized, sizeof normalized) != 0 ||
        destination_is_absent(normalized) != 0) return report_error("frame output path is unavailable; choose a new absent destination");
    for (size_t i = 0u; i < 3u; i++) {
        if (output_path(normalized, names[i], probe, sizeof probe) != 0) return report_error("frame output path exceeds its bound");
    }
    n = snprintf(staging, sizeof staging, "%s.lynxplosion-frame-stage-XXXXXX", normalized);
    if (n < 0 || (size_t)n >= sizeof staging || mkdtemp(staging) == NULL) return report_error("cannot create a private frame output staging directory");
    made = 1;
    for (size_t i = 0u; i < 3u; i++) {
        if (output_path(staging, names[i], path[i], sizeof path[i]) != 0 ||
            write_exclusive(path[i], data[i], lengths[i]) != 0) goto failed;
    }
    if (publish_directory_exclusive(staging, normalized) != 0) {
        (void)report_error("cannot publish the complete frame directory without replacement");
        goto failed;
    }
    made = 0;
    return 0;
failed:
    for (size_t i = 0u; i < 3u; i++) (void)unlink(path[i]);
    if (made != 0) (void)rmdir(staging);
    return report_error("cannot write the complete frame output; this invocation removed its private partial files");
}

static int host_sha_known_vector(void)
{
    static const char expected[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    uint8_t digest[32];
    char hex[65];
    host_sha_digest("abc", 3u, digest);
    host_sha_hex(digest, hex);
    return strcmp(hex, expected) == 0 ? 0 : 1;
}

static int advance_exact_tick(lynx_machine *machine, uint64_t target)
{
    uint64_t current = 0u;
    if (lynx_guest_ticks(machine, &current) != LYNX_OK || target < current) return 1;
    while (current < target) {
        uint64_t consumed = 0u, after = 0u;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        const uint64_t budget = target - current;
        if (lynx_advance(machine, budget, &consumed, &stop) != LYNX_OK || consumed != budget ||
            stop != LYNX_STOP_BUDGET || lynx_guest_ticks(machine, &after) != LYNX_OK || after != target) return 1;
        current = after;
    }
    return 0;
}

static int read_probe_result(lynx_machine *machine, uint32_t address,
                             uint32_t expected_a, uint8_t *framebuffer_byte)
{
    uint8_t result[8];
    if (lynx_read_memory(machine, address, result, sizeof result) != LYNX_OK ||
        memcmp(result, "LXI5", 4u) != 0 || result[4] != 2u || result[5] != expected_a ||
        lynx_read_memory(machine, 0x4000u, framebuffer_byte, 1u) != LYNX_OK) return 1;
    return 0;
}

static int run_frame_consumer(const char *content_path, const char *symbols_path,
                              const char *timeline_path, const char *output_directory,
                              uint32_t profile)
{
    uint8_t *bll = NULL, *label_bytes = NULL, *timeline_bytes = NULL;
    uint8_t *released_ppm = NULL, *pressed_ppm = NULL;
    size_t bll_len = 0u, label_len = 0u, timeline_len = 0u;
    size_t released_ppm_len = 0u, pressed_ppm_len = 0u;
    uint8_t released_pens[LYNX_FRAME_PENS_CAPACITY], pressed_pens[LYNX_FRAME_PENS_CAPACITY];
    uint8_t released_palettes[LYNX_FRAME_PALETTE_CAPACITY], pressed_palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t released_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], pressed_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    uint8_t replay_pens[LYNX_FRAME_PENS_CAPACITY], replay_palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t replay_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers;
    lynx_frame_result sync = {0}, released = {0}, replayed = {0}, pressed = {0};
    machine_storage storage = {0};
    lynx_identity identity;
    input_timeline timeline = {0};
    uint8_t *snapshot_blob = NULL, *snapshot_again = NULL;
    size_t snapshot_size = 0u;
    char content_hash[65], input_hash[65], manifest[FRAME_MANIFEST_CAP];
    size_t manifest_len = 0u;
    uint32_t probe_address = 0u;
    uint8_t result_sample = 0u;
    int result = 1;

    if (host_sha_known_vector() != 0) {
        (void)report_error("host SHA-256 helper failed its independent known-vector check");
        goto done;
    }
    if (read_bounded(timeline_path, TIMELINE_LIMIT, &timeline_bytes, &timeline_len,
                     "input timeline") != 0 ||
        parse_input_timeline(timeline_bytes, timeline_len, &timeline) != 0) {
        (void)report_error("timeline is not canonical LYNX-INPUT 1 ASCII; use strict increasing decimal tick/mask LF rows within the documented bounds");
        goto done;
    }
    {
        uint8_t digest[32];
        host_sha_digest(timeline_bytes, timeline_len, digest);
        host_sha_hex(digest, input_hash);
    }
    if (timeline.count != 1u || timeline.events[0].mask != LYNX_INPUT_A) {
        (void)report_error("frame tracer requires one recorded A press after the released frame");
        goto done;
    }
    if (read_bounded(content_path, BLL_LIMIT, &bll, &bll_len, "BLL content") != 0 || bll_len < 11u ||
        read_bounded(symbols_path, SYMBOL_LIMIT, &label_bytes, &label_len,
                     "ld65 -Ln symbols") != 0 ||
        parse_named_symbol(label_bytes, label_len, "probe_result", &probe_address) != 0 ||
        probe_address > LYNX_ADDRESS_SPACE - 8u) {
        (void)report_error("input-probe BLL or linked probe_result symbol is invalid; rebuild source with the pinned fixture recipe");
        goto done;
    }
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &storage, &identity) != 0) goto done;

    memset(&buffers, 0, sizeof buffers);
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.pens = released_pens;
    buffers.pens_capacity = sizeof released_pens;
    buffers.palettes = released_palettes;
    buffers.palettes_capacity = sizeof released_palettes;
    buffers.line_ticks = released_ticks;
    buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
    sync.struct_size = (uint32_t)sizeof sync;
    if (lynx_advance_frame(storage.machine, UINT64_C(60000), &buffers, &sync) != LYNX_OK ||
        sync.stop != LYNX_STOP_FRAME || sync.row_count != 0u || sync.sequence != 0u ||
        sync.frame_end_tick == 0u) {
        (void)report_error("first timer synchronization did not return its declared zero-row frame");
        goto done;
    }

    if (advance_exact_tick(storage.machine, sync.frame_end_tick + UINT64_C(1600)) != 0 ||
        lynx_snapshot_size(storage.machine, &snapshot_size) != LYNX_OK || snapshot_size == 0u) {
        (void)report_error("could not reach the in-progress row and size its public snapshot");
        goto done;
    }
    snapshot_blob = malloc(snapshot_size);
    snapshot_again = malloc(snapshot_size);
    if (snapshot_blob == NULL || snapshot_again == NULL) {
        (void)report_error("cannot allocate the bounded format-5 continuation snapshot");
        goto done;
    }
    {
        size_t written = 0u;
        if (lynx_snapshot_save(storage.machine, snapshot_blob, snapshot_size, &written) != LYNX_OK ||
            written != snapshot_size) {
            (void)report_error("public format-5 snapshot refused the in-progress display capture");
            goto done;
        }
    }
    buffers.pens = released_pens; buffers.palettes = released_palettes; buffers.line_ticks = released_ticks;
    released.struct_size = (uint32_t)sizeof released;
    if (lynx_advance_frame(storage.machine, UINT64_C(60000), &buffers, &released) != LYNX_OK ||
        released.stop != LYNX_STOP_FRAME || released.row_count != LYNX_FRAME_MAX_ROWS ||
        released.sequence != 1u || released.frame_start_tick != sync.frame_end_tick ||
        released.flags != 0u || read_probe_result(storage.machine, probe_address, 0u,
                                                    &result_sample) != 0 ||
        result_sample != 0x11u || released_pens[0] != 0x11u ||
        released_palettes[1] != 0x0Fu || released_palettes[17] != 0u ||
        released_palettes[2] != 0u || released_palettes[18] != 0x0Fu) {
        (void)report_error("released frame did not match the independent first-pen, palette, progress and timer expectations");
        goto done;
    }
    if (lynx_snapshot_restore(storage.machine, snapshot_blob, snapshot_size) != LYNX_OK) {
        (void)report_error("public format-5 restore refused its own captured in-progress video state");
        goto done;
    }
    {
        size_t written = 0u;
        if (lynx_snapshot_save(storage.machine, snapshot_again, snapshot_size, &written) != LYNX_OK ||
            written != snapshot_size || memcmp(snapshot_blob, snapshot_again, snapshot_size) != 0) {
            (void)report_error("format-5 restore did not reproduce its canonical in-progress capture state");
            goto done;
        }
    }
    buffers.pens = replay_pens; buffers.palettes = replay_palettes; buffers.line_ticks = replay_ticks;
    replayed.struct_size = (uint32_t)sizeof replayed;
    if (lynx_advance_frame(storage.machine, UINT64_C(60000), &buffers, &replayed) != LYNX_OK ||
        memcmp(&released, &replayed, sizeof released) != 0 ||
        memcmp(released_pens, replay_pens, sizeof released_pens) != 0 ||
        memcmp(released_palettes, replay_palettes, sizeof released_palettes) != 0 ||
        memcmp(released_ticks, replay_ticks, sizeof released_ticks) != 0) {
        (void)report_error("restored in-progress capture differs from uninterrupted raw frame output");
        goto done;
    }
    for (size_t row = 0u; row < released.row_count; row++) {
        if (released_ticks[row] < released.frame_start_tick || released_ticks[row] >= released.frame_end_tick ||
            (row > 0u && released_ticks[row] <= released_ticks[row - 1u])) {
            (void)report_error("released frame line ticks are not ordered within the timer-generated frame");
            goto done;
        }
    }
    if (timeline.events[0].tick != released.frame_end_tick + UINT64_C(777) ||
        timeline.events[0].tick == released.frame_end_tick) {
        (void)report_error("recorded A event is not the fixture's exact non-frame tick");
        goto done;
    }
    if (advance_exact_tick(storage.machine, timeline.events[0].tick) != 0 ||
        lynx_set_input(storage.machine, timeline.events[0].mask) != LYNX_OK) {
        (void)report_error("public input replay failed to reach and apply its recorded guest tick");
        goto done;
    }

    buffers.pens = pressed_pens; buffers.palettes = pressed_palettes; buffers.line_ticks = pressed_ticks;
    pressed.struct_size = (uint32_t)sizeof pressed;
    if (lynx_advance_frame(storage.machine, UINT64_C(60000), &buffers, &pressed) != LYNX_OK ||
        pressed.stop != LYNX_STOP_FRAME || pressed.row_count != LYNX_FRAME_MAX_ROWS ||
        pressed.sequence != 2u || pressed.frame_start_tick != released.frame_end_tick ||
        pressed.flags != 0u || pressed.frame_end_tick <= pressed.frame_start_tick ||
        pressed.frame_end_tick - pressed.frame_start_tick !=
            released.frame_end_tick - released.frame_start_tick ||
        read_probe_result(storage.machine, probe_address, 1u, &result_sample) != 0 ||
        result_sample != 0x22u || pressed_pens[0] != 0x22u ||
        pressed_palettes[1] != 0x0Fu || pressed_palettes[17] != 0u ||
        pressed_palettes[2] != 0u || pressed_palettes[18] != 0x0Fu) {
        (void)report_error("A-pressed frame did not match the independent pen, palette, input and progress expectations");
        goto done;
    }
    for (size_t row = 0u; row < pressed.row_count; row++) {
        if (pressed_ticks[row] < pressed.frame_start_tick || pressed_ticks[row] >= pressed.frame_end_tick ||
            (row > 0u && pressed_ticks[row] <= pressed_ticks[row - 1u])) {
            (void)report_error("pressed frame line ticks are not ordered within the timer-generated frame");
            goto done;
        }
    }
    encode_hex(identity.content_sha256, content_hash);
    if (frame_ppm(released_pens, released_palettes, &released, 0u, 0u, input_hash,
                  &released_ppm, &released_ppm_len) != 0 ||
        frame_ppm(pressed_pens, pressed_palettes, &pressed, LYNX_INPUT_A, 1u, input_hash,
                  &pressed_ppm, &pressed_ppm_len) != 0) goto done;
    {
        int n = snprintf(manifest, sizeof manifest,
            "artifact = FRAME_TRACE\nprofile = %s\nstartup = A\ntiming_policy = video-policy1-unverified\n"
            "content_sha256 = %s\ninput_sha256 = %s\nevent_count = %zu\nevent_tick = %" PRIu64 "\nevent_mask = %u\n"
            "sync_rows = %u\nsync_end_tick = %" PRIu64 "\n"
            "released_sequence = %" PRIu64 "\nreleased_start_tick = %" PRIu64 "\nreleased_end_tick = %" PRIu64 "\nreleased_rows = %u\nreleased_sample_a = 0\n"
            "pressed_sequence = %" PRIu64 "\npressed_start_tick = %" PRIu64 "\npressed_end_tick = %" PRIu64 "\npressed_rows = %u\npressed_sample_a = 1\n"
            "guest_progress = 2\npalette_policy = per-line GREEN then BLUERED raw nibbles; intra-line color accuracy unverified\n"
            "raw_pen_order = high nibble first; no host rotation\n",
            lynx_profile_name(profile), content_hash, input_hash, timeline.count,
            timeline.events[0].tick, timeline.events[0].mask, sync.row_count, sync.frame_end_tick,
            released.sequence, released.frame_start_tick, released.frame_end_tick, released.row_count,
            pressed.sequence, pressed.frame_start_tick, pressed.frame_end_tick, pressed.row_count);
        if (n < 0 || (size_t)n >= sizeof manifest) {
            (void)report_error("frame trace manifest exceeds its fixed bound");
            goto done;
        }
        manifest_len = (size_t)n;
    }
    if (publish_frame_outputs(output_directory, released_ppm, released_ppm_len,
                              pressed_ppm, pressed_ppm_len, manifest, manifest_len) != 0) goto done;
    printf("lynx-consume: wrote released and A-pressed raw-frame PPM observations to %s\n", output_directory);
    result = 0;
done:
    free(bll); free(label_bytes); free(timeline_bytes); free(timeline.events);
    free(snapshot_blob); free(snapshot_again);
    free(released_ppm); free(pressed_ppm);
    destroy_machine(&storage);
    return result;
}

int main(int argc, char **argv)
{
    const char *content = NULL;
    const char *label_file = NULL;
    const char *output = NULL;
    const char *profile_text = "none";
    uint32_t profile = LYNX_PROFILE_NONE;
    if (argc > 1 && strcmp(argv[1], "--mode") == 0) {
        const char *frame_mode = NULL, *frame_content = NULL, *frame_symbols = NULL, *frame_timeline = NULL;
        const char *frame_output = NULL, *frame_profile_text = "none";
        uint32_t selected_profile = LYNX_PROFILE_NONE;
        unsigned seen = 0u;
        for (int i = 1; i < argc; i++) {
            const char *flag = argv[i], *value;
            unsigned bit;
            if (strcmp(flag, "--mode") == 0) bit = 1u;
            else if (strcmp(flag, "--content") == 0) bit = 2u;
            else if (strcmp(flag, "--symbols") == 0) bit = 4u;
            else if (strcmp(flag, "--timeline") == 0) bit = 8u;
            else if (strcmp(flag, "--out") == 0) bit = 16u;
            else if (strcmp(flag, "--profile") == 0) bit = 32u;
            else return usage_error("unknown frame-mode option");
            if ((seen & bit) != 0u || i + 1 >= argc) return usage_error("frame-mode option is repeated or missing its value");
            value = argv[++i];
            if (value[0] == '\0' || strlen(value) >= 4096u) return usage_error("frame-mode option value is empty or too long");
            if (bit == 1u) frame_mode = value;
            else if (bit == 2u) frame_content = value;
            else if (bit == 4u) frame_symbols = value;
            else if (bit == 8u) frame_timeline = value;
            else if (bit == 16u) frame_output = value;
            else frame_profile_text = value;
            seen |= bit;
        }
        if (frame_mode == NULL || strcmp(frame_mode, "frames") != 0 || frame_content == NULL || frame_symbols == NULL ||
            frame_timeline == NULL || frame_output == NULL || parse_profile(frame_profile_text, &selected_profile) != 0)
            return usage_error("frame mode requires --mode frames, BLL, linked symbols, a canonical timeline, output directory, and a supported profile");
        return run_frame_consumer(frame_content, frame_symbols, frame_timeline, frame_output, selected_profile);
    }
    unsigned seen = 0u;
    int index;
    for (index = 1; index < argc; index++) {
        const char *flag = argv[index];
        const char *value;
        unsigned bit;
        if (strcmp(flag, "--content") == 0) {
            bit = 1u;
        } else if (strcmp(flag, "--symbols") == 0) {
            bit = 2u;
        } else if (strcmp(flag, "--out") == 0) {
            bit = 4u;
        } else if (strcmp(flag, "--profile") == 0) {
            bit = 8u;
        } else {
            return usage_error("unknown option; use only --content, --symbols, --out, and optional --profile");
        }
        if ((seen & bit) != 0u) {
            return usage_error("option was repeated; provide each accepted option once");
        }
        if (index + 1 >= argc) {
            return usage_error("option is missing its value");
        }
        value = argv[++index];
        if (value[0] == '\0') {
            return usage_error("option value is empty");
        }
        if (strlen(value) >= 4096u) {
            return usage_error("path value exceeds the 4095-byte bound; provide a shorter path");
        }
        if (bit == 1u) {
            content = value;
        } else if (bit == 2u) {
            label_file = value;
        } else if (bit == 4u) {
            output = value;
        } else {
            profile_text = value;
        }
        seen |= bit;
    }
    if (content == NULL || label_file == NULL || output == NULL) {
        return usage_error("--content, --symbols, and --out are required");
    }
    if (parse_profile(profile_text, &profile) != 0 || lynx_profile_name(profile) == NULL) {
        return usage_error("profile must be exactly none, lynx1, or lynx2");
    }
    return run_consumer(content, label_file, output, profile);
}
