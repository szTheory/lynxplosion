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
    FRAME_MANIFEST_CAP = 8192,
    FRAME_OUTPUT_LIMIT = 32,
    FRAME_RAW_BYTES_PER_ROW = 120,
    AV_PCM_CAPACITY = 3135,
    AV_EVENT_CAPACITY = 32768,
    AV_INPUT_APPLIED_CAPACITY = 512 * 1024,
    AV_RAW_HEADER_BYTES = 24,
    AV_WAV_HEADER_BYTES = 44
};

typedef struct input_event { uint64_t tick; uint32_t mask; size_t file_offset; } input_event;
typedef struct input_timeline { input_event *events; size_t count; } input_timeline;
typedef struct guest_probe {
    uint32_t joystick;
    uint32_t switches;
    uint32_t progress;
    uint32_t error;
    uint32_t done;
    uint32_t audio_started;
} guest_probe;
typedef struct frame_capture {
    lynx_frame_result result;
    uint32_t input_mask;
    guest_probe sample;
    uint32_t handedness;
    uint32_t display_marker;
    uint8_t *raw;
    size_t raw_len;
    uint8_t *ppm;
    size_t ppm_len;
    char raw_sha256[65];
    char rendered_sha256[65];
} frame_capture;
typedef enum checkpoint_phase { CHECKPOINT_PRE_EDGE = 1, CHECKPOINT_POST_EDGE = 2 } checkpoint_phase;
typedef struct replay_checkpoint {
    uint8_t script_digest[32];
    uint64_t tick;
    size_t cursor;
    uint32_t current_mask;
    checkpoint_phase phase;
} replay_checkpoint;
typedef struct av_replay_checkpoint {
    uint32_t schema;
    uint32_t profile;
    uint32_t timing_policy;
    uint32_t audio_policy;
    uint32_t audio_rate_num;
    uint32_t audio_rate_den;
    uint32_t audio_channels;
    uint32_t audio_bits;
    uint8_t content_digest[32];
    uint8_t script_digest[32];
    uint8_t build_digest[32];
    uint64_t guest_tick;
    uint64_t next_sample_ordinal;
    uint64_t frame_sequence;
    size_t script_length;
    size_t file_offset;
    size_t cursor;
    size_t input_applied_length;
    size_t input_applied_count;
    size_t pcm_count;
    size_t event_count;
    size_t frame_count;
    size_t progress_count;
    uint32_t current_mask;
    checkpoint_phase phase;
    uint8_t prefix_digest[32];
} av_replay_checkpoint;

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
                    "   or: lynx-consume --mode frames|av-replay --content BLL --symbols LABELS --timeline FILE --out NEW-DIRECTORY [--profile none|lynx1|lynx2]\n"
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
        out->events[out->count].file_offset = offset;
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
    const char *base = strrchr(path, '/');
    base = base == NULL ? path : base + 1;
    int inject_write = injected != NULL &&
        (strcmp(injected, "write") == 0 ||
         (strncmp(injected, "write:", 6u) == 0 && strcmp(injected + 6u, base) == 0));
    int inject_close = injected != NULL &&
        (strcmp(injected, "close") == 0 ||
         (strncmp(injected, "close:", 6u) == 0 && strcmp(injected + 6u, base) == 0));
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
    char absolute[4096], parent[4096];
    size_t length;
    if (directory[0] == '/') {
        if (snprintf(absolute, sizeof absolute, "%s", directory) >= (int)sizeof absolute)
            return report_error("output path exceeds its fixed bound");
    } else {
        char cwd[2048];
        if (getcwd(cwd, sizeof cwd) == NULL ||
            snprintf(absolute, sizeof absolute, "%s/%s", cwd, directory) >= (int)sizeof absolute)
            return report_error("cannot resolve the output path from the current directory");
    }
    length = strlen(absolute);
    while (length > 1u && absolute[length - 1u] == '/') absolute[--length] = '\0';
    for (char *cursor = absolute + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            struct stat component;
            *cursor = '\0';
            if (lstat(absolute, &component) == 0 && S_ISLNK(component.st_mode))
                return report_error("output path traverses a symbolic-link parent; use a direct directory path");
            *cursor = '/';
        }
    }
    if (snprintf(parent, sizeof parent, "%s", absolute) >= (int)sizeof parent)
        return report_error("output parent path exceeds its fixed bound");
    {
        char *slash = strrchr(parent, '/');
        if (slash == NULL) (void)snprintf(parent, sizeof parent, ".");
        else if (slash == parent) slash[1] = '\0';
        else *slash = '\0';
    }
    if (stat(parent, &info) != 0 || !S_ISDIR(info.st_mode))
        return report_error("output parent must be an existing directory");
    if (lstat(directory, &info) == 0) {
        return report_error("output directory already exists; choose a new empty --out path (existing files were not changed)");
    }
    if (errno != ENOENT) {
        return report_error("cannot inspect the output path; check its parent path and permissions");
    }
    return 0;
}

static int input_paths_are_distinct(const char *content, const char *symbols_path,
                                    const char *timeline, const char *error_message)
{
    const char *paths[3] = {content, symbols_path, timeline};
    struct stat info[3];
    int have[3] = {0, 0, 0};
    for (size_t index = 0u; index < 3u; index++)
        have[index] = stat(paths[index], &info[index]) == 0;
    for (size_t left = 0u; left < 3u; left++) {
        for (size_t right = left + 1u; right < 3u; right++) {
            if (have[left] && have[right] && info[left].st_dev == info[right].st_dev &&
                info[left].st_ino == info[right].st_ino)
                return report_error(error_message);
        }
    }
    return 0;
}

static int output_stage_is_owned(const char *staging, const struct stat *created)
{
    char owner[4096];
    char expected[4096];
    const char *base = strrchr(staging, '/');
    uint8_t *bytes = NULL;
    size_t length = 0u;
    struct stat current;
    int result;
    base = base == NULL ? staging : base + 1;
    if (lstat(staging, &current) != 0 || !S_ISDIR(current.st_mode) ||
        current.st_dev != created->st_dev || current.st_ino != created->st_ino ||
        output_path(staging, ".lynx-owner", owner, sizeof owner) != 0 ||
        snprintf(expected, sizeof expected, "%s\n", base) >= (int)sizeof expected)
        return 0;
    result = read_bounded(owner, 4096u, &bytes, &length, "output stage owner marker");
    if (result != 0) { free(bytes); return 0; }
    result = length == strlen(expected) && memcmp(bytes, expected, length) == 0;
    free(bytes);
    return result;
}

static int output_stage_same_inode(const char *staging, const struct stat *created)
{
    struct stat current;
    return lstat(staging, &current) == 0 && S_ISDIR(current.st_mode) &&
        current.st_dev == created->st_dev && current.st_ino == created->st_ino;
}

static int write_output_owner(const char *owner, const char *value, size_t length)
{
    int fd = open(owner, O_WRONLY | O_CREAT | O_EXCL, 0600);
    size_t used = 0u;
    int failed = 0;
    if (fd < 0) return 1;
    while (used < length) {
        ssize_t written = write(fd, value + used, length - used);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { failed = 1; break; }
        used += (size_t)written;
    }
    if (close(fd) != 0) failed = 1;
    if (failed != 0) (void)unlink(owner);
    return failed;
}

static int cleanup_output_stage(const char *staging, const struct stat *created,
                                int nonce_verified, char paths[][4096], size_t files)
{
    char owner[4096];
    int cleanup_failed = 0;
    if (nonce_verified == 0 && !output_stage_is_owned(staging, created)) return 1;
    if (nonce_verified != 0 && !output_stage_same_inode(staging, created)) return 1;
    for (size_t index = 0u; index < files; index++) {
        if (paths[index][0] != '\0' && unlink(paths[index]) != 0 && errno != ENOENT)
            cleanup_failed = 1;
    }
    if (output_path(staging, ".lynx-owner", owner, sizeof owner) != 0 ||
        (unlink(owner) != 0 && errno != ENOENT)) cleanup_failed = 1;
    if (rmdir(staging) != 0 && errno != ENOENT) cleanup_failed = 1;
    return cleanup_failed;
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

#if defined(LYNX_CONSUMER_TESTING)
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
#endif

static int publish_frame_outputs_generic(const char *directory, const frame_capture *frames,
                                         size_t frame_count, const char *manifest,
                                         size_t manifest_len)
{
    char normalized[4096], staging[4096], owner[4096], owner_value[4096];
    char paths[FRAME_OUTPUT_LIMIT * 2u + 1u][4096] = {{0}};
    char names[FRAME_OUTPUT_LIMIT * 2u + 1u][32], probe[4096];
    const void *data[FRAME_OUTPUT_LIMIT * 2u + 1u];
    size_t lengths[FRAME_OUTPUT_LIMIT * 2u + 1u];
    size_t files = 0u;
    int n;
    int made = 0;
    int nonce_verified = 0;
    struct stat stage_identity = {0};
    int stage_identity_valid = 0;
    if (frame_count > FRAME_OUTPUT_LIMIT || normalize_destination(directory, normalized, sizeof normalized) != 0 ||
        destination_is_absent(normalized) != 0)
        return report_error("frame output path is unavailable or the bounded frame count was exceeded");
    for (size_t i = 0u; i < frame_count; i++) {
        n = snprintf(names[files], sizeof names[files], "frame-%02zu.raw", i);
        if (n < 0 || (size_t)n >= sizeof names[files]) return report_error("raw frame name exceeds its fixed bound");
        data[files] = frames[i].raw; lengths[files] = frames[i].raw_len; files++;
        n = snprintf(names[files], sizeof names[files], "frame-%02zu.ppm", i);
        if (n < 0 || (size_t)n >= sizeof names[files]) return report_error("rendered frame name exceeds its fixed bound");
        data[files] = frames[i].ppm; lengths[files] = frames[i].ppm_len; files++;
    }
    (void)snprintf(names[files], sizeof names[files], "frame-trace.manifest");
    data[files] = manifest; lengths[files] = manifest_len; files++;
    for (size_t i = 0u; i < files; i++) {
        if (output_path(normalized, names[i], probe, sizeof probe) != 0)
            return report_error("frame output path exceeds its bound");
    }
    n = snprintf(staging, sizeof staging, "%s.lynxplosion-frame-stage-XXXXXX", normalized);
    if (n < 0 || (size_t)n >= sizeof staging || mkdtemp(staging) == NULL)
        return report_error("cannot create a private frame output staging directory");
    made = 1;
    if (lstat(staging, &stage_identity) != 0 || !S_ISDIR(stage_identity.st_mode)) goto failed;
    stage_identity_valid = 1;
    if (output_path(staging, ".lynx-owner", owner, sizeof owner) != 0) goto failed;
    {
        const char *base = strrchr(staging, '/');
        base = base == NULL ? staging : base + 1;
        n = snprintf(owner_value, sizeof owner_value, "%s\n", base);
    }
    if (n < 0 || (size_t)n >= sizeof owner_value ||
        write_output_owner(owner, owner_value, (size_t)n) != 0) goto failed;
    for (size_t i = 0u; i < files; i++) {
        if (output_path(staging, names[i], paths[i], sizeof paths[i]) != 0 ||
            write_exclusive(paths[i], data[i], lengths[i]) != 0) goto failed;
    }
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *abort_point = getenv("LYNX_TEST_FRAME_ABORT");
        if (abort_point != NULL && strcmp(abort_point, "before-publish") == 0) goto failed;
    }
#endif
    if (!output_stage_is_owned(staging, &stage_identity)) goto failed;
    nonce_verified = 1;
    if (unlink(owner) != 0) goto failed;
    if (publish_directory_exclusive(staging, normalized) != 0) {
        (void)report_error("cannot publish the complete frame directory without replacement");
        goto failed;
    }
    made = 0;
    return 0;
failed:
    if (made != 0 && stage_identity_valid != 0 && cleanup_output_stage(staging, &stage_identity, nonce_verified,
                                         paths, files) != 0)
        (void)report_error("frame stage cleanup refused because its invocation nonce or directory identity did not match");
    return report_error("cannot write the complete frame output; this invocation removed its private partial files");
}

#if defined(LYNX_CONSUMER_TESTING)
static int publish_frame_outputs(const char *directory, const uint8_t *released,
                                 size_t released_len, const uint8_t *pressed,
                                 size_t pressed_len, const char *manifest,
                                 size_t manifest_len)
{
    char normalized[4096], staging[4096], path[3][4096] = {{0}}, probe[4096];
    const char *names[3] = {"released.ppm", "pressed.ppm", "frame-trace.manifest"};
    const void *data[3] = {released, pressed, manifest};
    size_t lengths[3] = {released_len, pressed_len, manifest_len};
    int n;
    if (normalize_destination(directory, normalized, sizeof normalized) != 0 ||
        destination_is_absent(normalized) != 0) return report_error("frame output path is unavailable; choose a new absent destination");
    for (size_t i = 0u; i < 3u; i++)
        if (output_path(normalized, names[i], probe, sizeof probe) != 0) return report_error("frame output path exceeds its bound");
    n = snprintf(staging, sizeof staging, "%s.lynxplosion-frame-stage-XXXXXX", normalized);
    if (n < 0 || (size_t)n >= sizeof staging || mkdtemp(staging) == NULL) return report_error("cannot create a private frame output staging directory");
    for (size_t i = 0u; i < 3u; i++) {
        if (output_path(staging, names[i], path[i], sizeof path[i]) != 0 ||
            write_exclusive(path[i], data[i], lengths[i]) != 0) goto failed;
    }
    if (publish_directory_exclusive(staging, normalized) != 0) {
        (void)report_error("cannot publish the complete frame directory without replacement");
        goto failed;
    }
    return 0;
failed:
    for (size_t i = 0u; i < 3u; i++) if (path[i][0] != '\0') (void)unlink(path[i]);
    (void)rmdir(staging);
    return report_error("cannot write the complete frame output; this invocation removed its private partial files");
}
#endif

#if defined(LYNX_CONSUMER_TESTING)
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
        memcmp(result, "LXI5", 4u) != 0 || result[4] != 3u || (result[5] & 1u) != expected_a ||
        lynx_read_memory(machine, 0x4000u, framebuffer_byte, 1u) != LYNX_OK) return 1;
    return 0;
}
#endif

static int read_guest_probe(lynx_machine *machine, uint32_t address, guest_probe *out)
{
    uint8_t bytes[8];
    if (lynx_read_memory(machine, address, bytes, sizeof bytes) != LYNX_OK ||
        memcmp(bytes, "LXI5", 4u) != 0 || bytes[4] != 3u) return 1;
    out->joystick = bytes[5];
    out->switches = bytes[6];
    out->progress = (uint32_t)(bytes[7] & 0x3Fu);
    out->done = (uint32_t)((bytes[7] >> 6) & 1u);
    out->error = (uint32_t)((bytes[7] >> 7) & 1u);
    return 0;
}

#if defined(LYNX_CONSUMER_TESTING)
static int run_frame_consumer_legacy(const char *content_path, const char *symbols_path,
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
            released.frame_end_tick - released.frame_start_tick) {
        (void)report_error("A-pressed frame timing or sequence differs from the independent timer expectation");
        goto done;
    }
    if (read_probe_result(storage.machine, probe_address, 1u, &result_sample) != 0) {
        (void)report_error("A-pressed guest result block did not report the sampled A level");
        goto done;
    }
    if (result_sample != 0x22u || pressed_pens[0] != 0x22u ||
        pressed_palettes[1] != 0x0Fu || pressed_palettes[17] != 0u ||
        pressed_palettes[2] != 0u || pressed_palettes[18] != 0x0Fu) {
        char message[192];
        (void)snprintf(message, sizeof message,
            "A-pressed visual expectation differs (guest=%02x raw=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x)",
            result_sample, pressed_pens[0], pressed_pens[1], pressed_pens[2], pressed_pens[3],
            pressed_pens[4], pressed_pens[5], pressed_pens[6], pressed_pens[7], pressed_pens[8]);
        (void)report_error(message);
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
#endif

static int replay_sha_vectors(void)
{
    static const struct { const char *input; size_t length; const char *expected; } vectors[] = {
        {"", 0u, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", 3u, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"The quick brown fox jumps over the lazy dog", 43u,
         "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"}
    };
    uint8_t digest[32];
    char hex[65];
    for (size_t i = 0u; i < sizeof vectors / sizeof vectors[0]; i++) {
        host_sha_digest(vectors[i].input, vectors[i].length, digest);
        host_sha_hex(digest, hex);
        if (strcmp(hex, vectors[i].expected) != 0) return 1;
    }
    return 0;
}

static size_t replay_lower_bound(const input_timeline *timeline, uint64_t tick)
{
    size_t low = 0u, high = timeline->count;
    while (low < high) {
        const size_t mid = low + (high - low) / 2u;
        if (timeline->events[mid].tick < tick) low = mid + 1u; else high = mid;
    }
    return low;
}

static size_t replay_upper_bound(const input_timeline *timeline, uint64_t tick)
{
    size_t low = 0u, high = timeline->count;
    while (low < high) {
        const size_t mid = low + (high - low) / 2u;
        if (timeline->events[mid].tick <= tick) low = mid + 1u; else high = mid;
    }
    return low;
}

static uint32_t replay_mask_at_cursor(const input_timeline *timeline, size_t cursor)
{
    return cursor == 0u ? 0u : timeline->events[cursor - 1u].mask;
}

static int replay_checkpoint_valid(const replay_checkpoint *checkpoint,
                                   const input_timeline *timeline,
                                   const uint8_t script_digest[32], uint64_t tick,
                                   size_t cursor, uint32_t mask,
                                   checkpoint_phase phase)
{
    const size_t expected = phase == CHECKPOINT_PRE_EDGE ?
        replay_lower_bound(timeline, tick) : replay_upper_bound(timeline, tick);
    return checkpoint->phase == phase && checkpoint->tick == tick &&
        checkpoint->cursor == cursor && cursor == expected &&
        checkpoint->current_mask == mask && mask == replay_mask_at_cursor(timeline, expected) &&
        memcmp(checkpoint->script_digest, script_digest, 32u) == 0 ? 0 : 1;
}

static int replay_restore_checkpoint(lynx_machine *source, lynx_machine *target,
                                     const replay_checkpoint *checkpoint,
                                     const input_timeline *timeline,
                                     const uint8_t script_digest[32])
{
    uint8_t *snapshot = NULL, source_digest[32], target_digest[32];
    size_t size = 0u, written = 0u;
    uint64_t tick = 0u;
    if (replay_checkpoint_valid(checkpoint, timeline, script_digest,
                                checkpoint->tick, checkpoint->cursor,
                                checkpoint->current_mask, checkpoint->phase) != 0 ||
        lynx_guest_ticks(source, &tick) != LYNX_OK || tick != checkpoint->tick ||
        lynx_state_digest(source, source_digest) != LYNX_OK ||
        lynx_snapshot_size(source, &size) != LYNX_OK || size == 0u) return 1;
    snapshot = malloc(size);
    if (snapshot == NULL) return 1;
    if (lynx_snapshot_save(source, snapshot, size, &written) != LYNX_OK || written != size ||
        lynx_snapshot_restore(target, snapshot, size) != LYNX_OK ||
        lynx_guest_ticks(target, &tick) != LYNX_OK || tick != checkpoint->tick ||
        lynx_state_digest(target, target_digest) != LYNX_OK ||
        memcmp(source_digest, target_digest, sizeof source_digest) != 0) {
        free(snapshot);
        return 1;
    }
    free(snapshot);
    return 0;
}

static int replay_raw_bytes(const lynx_frame_result *frame, const uint8_t *pens,
                            const uint8_t *palettes, const uint64_t *ticks,
                            uint8_t **out, size_t *out_len)
{
    const size_t row_size = 120u;
    size_t length;
    uint8_t *bytes;
    if (frame->row_count == 0u || frame->row_count > LYNX_FRAME_MAX_ROWS) return 1;
    length = (size_t)frame->row_count * row_size;
    bytes = malloc(length);
    if (bytes == NULL) return 1;
    for (size_t row = 0u; row < frame->row_count; row++) {
        uint8_t *dst = bytes + row * row_size;
        memcpy(dst, pens + row * LYNX_FRAME_ROW_BYTES, LYNX_FRAME_ROW_BYTES);
        memcpy(dst + 80u, palettes + row * 32u, 32u);
        for (size_t byte = 0u; byte < 8u; byte++) dst[112u + byte] = (uint8_t)(ticks[row] >> (8u * byte));
        if (ticks[row] < frame->frame_start_tick || ticks[row] >= frame->frame_end_tick ||
            (row != 0u && ticks[row] <= ticks[row - 1u])) { free(bytes); return 1; }
    }
    *out = bytes; *out_len = length;
    return 0;
}

static uint32_t replay_expected_joystick(uint32_t mask, uint32_t lefthand)
{
    uint32_t value = (mask & LYNX_INPUT_A ? 0x01u : 0u) |
        (mask & LYNX_INPUT_B ? 0x02u : 0u) |
        (mask & LYNX_INPUT_OPTION2 ? 0x04u : 0u) |
        (mask & LYNX_INPUT_OPTION1 ? 0x08u : 0u);
    if (lefthand != 0u) {
        value |= (mask & LYNX_INPUT_UP ? 0x80u : 0u) |
            (mask & LYNX_INPUT_DOWN ? 0x40u : 0u) |
            (mask & LYNX_INPUT_LEFT ? 0x20u : 0u) |
            (mask & LYNX_INPUT_RIGHT ? 0x10u : 0u);
    } else {
        value |= (mask & LYNX_INPUT_UP ? 0x40u : 0u) |
            (mask & LYNX_INPUT_DOWN ? 0x80u : 0u) |
            (mask & LYNX_INPUT_LEFT ? 0x10u : 0u) |
            (mask & LYNX_INPUT_RIGHT ? 0x20u : 0u);
    }
    return value;
}

static int replay_guest_observation(lynx_machine *machine, uint32_t result_address,
                                   uint32_t handedness_address, uint32_t expected_mask,
                                   uint32_t *handedness_out, guest_probe *sample,
                                   const char **failure)
{
    static const uint32_t logical_bits[9] = {
        LYNX_INPUT_A, LYNX_INPUT_B, LYNX_INPUT_UP, LYNX_INPUT_DOWN,
        LYNX_INPUT_LEFT, LYNX_INPUT_RIGHT, LYNX_INPUT_OPTION1,
        LYNX_INPUT_OPTION2, LYNX_INPUT_PAUSE
    };
    uint8_t pattern[9];
    uint8_t handedness = 0u;
    if (lynx_read_memory(machine, handedness_address, &handedness, 1u) != LYNX_OK || handedness > 1u) {
        *failure = "source-linked homebrew handedness is outside its declared 0/1 range";
        return 1;
    }
    if (read_guest_probe(machine, result_address, sample) != 0) {
        *failure = "homebrew result block schema is invalid";
        return 1;
    }
    if (sample->progress < 2u) {
        *failure = "homebrew progress did not advance";
        return 1;
    }
    if (sample->error != 0u || sample->done != 1u) {
        *failure = "homebrew result block reports guest error or incomplete state";
        return 1;
    }
    if (sample->joystick != replay_expected_joystick(expected_mask, handedness) ||
        sample->switches != ((expected_mask & LYNX_INPUT_PAUSE) != 0u ? 1u : 0u) ||
        lynx_read_memory(machine, 0x4000u, pattern, 9u) != LYNX_OK) {
        *failure = "homebrew input response mismatch";
        return 1;
    }
    for (size_t index = 0u; index < 9u; index++) {
        const uint8_t wanted = (expected_mask & logical_bits[index]) != 0u ? 0x22u : 0x11u;
        if (pattern[index] != wanted) {
            *failure = "homebrew input response mismatch";
            return 1;
        }
    }
    *handedness_out = handedness;
    return 0;
}

static int replay_render_ppm(frame_capture *capture, const uint8_t *pens,
                             const uint8_t *palettes)
{
    const lynx_frame_result *frame = &capture->result;
    const size_t raster_len = (size_t)LYNX_FRAME_WIDTH * frame->row_count * 3u;
    char header[512];
    uint8_t digest[32];
    int header_len;
    size_t at;
    if (frame->row_count > LYNX_FRAME_MAX_ROWS || raster_len > FRAME_PPM_LIMIT - sizeof header) return 1;
    header_len = snprintf(header, sizeof header,
        "P6\n# artifact = FRAME\n# sequence = %" PRIu64 "\n# frame_start_tick = %" PRIu64 "\n"
        "# frame_end_tick = %" PRIu64 "\n# rows = %u\n# input_mask_at_end = %u\n"
        "# sample_a = %u\n# sample_joystick = %u\n# sample_switches = %u\n"
        "# poll_progress = %u\n# error = %u\n# done = %u\n# handedness = %u\n"
        "# raw_frame_sha256 = %s\n160 %u\n255\n",
        frame->sequence, frame->frame_start_tick, frame->frame_end_tick, frame->row_count,
        capture->input_mask, capture->sample.joystick & 1u, capture->sample.joystick,
        capture->sample.switches, capture->sample.progress, capture->sample.error,
        capture->sample.done, capture->handedness, capture->raw_sha256,
        frame->row_count);
    if (header_len < 0 || (size_t)header_len >= sizeof header ||
        (size_t)header_len + raster_len > FRAME_PPM_LIMIT) return 1;
    capture->ppm = malloc((size_t)header_len + raster_len);
    if (capture->ppm == NULL) return 1;
    memcpy(capture->ppm, header, (size_t)header_len); at = (size_t)header_len;
    for (size_t y = 0u; y < frame->row_count; y++) {
        const uint8_t *row = pens + y * LYNX_FRAME_ROW_BYTES;
        const uint8_t *palette = palettes + y * 32u;
        for (size_t x = 0u; x < LYNX_FRAME_WIDTH; x++) {
            const uint8_t packed = row[x / 2u];
            const uint8_t pen = (x & 1u) == 0u ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 15u);
            const uint8_t green = (uint8_t)(palette[pen] & 15u), blue_red = palette[16u + pen];
            capture->ppm[at++] = (uint8_t)((blue_red & 15u) * 17u);
            capture->ppm[at++] = (uint8_t)(green * 17u);
            capture->ppm[at++] = (uint8_t)((blue_red >> 4) * 17u);
        }
    }
    capture->ppm_len = at;
    host_sha_digest(capture->ppm, capture->ppm_len, digest);
    host_sha_hex(digest, capture->rendered_sha256);
    return 0;
}

static int replay_append(char *manifest, size_t cap, size_t *length,
                         const char *format, ...)
{
    va_list args;
    int n;
    if (*length >= cap) return 1;
    va_start(args, format);
    n = vsnprintf(manifest + *length, cap - *length, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= cap - *length) return 1;
    *length += (size_t)n;
    return 0;
}

static void replay_transcript_update(host_sha256 *transcript,
                                     const lynx_frame_result *frame,
                                     const uint8_t *pens,
                                     const uint8_t *palettes,
                                     const uint64_t *ticks)
{
    host_sha_update(transcript, &frame->frame_start_tick, sizeof frame->frame_start_tick);
    host_sha_update(transcript, &frame->frame_end_tick, sizeof frame->frame_end_tick);
    host_sha_update(transcript, &frame->sequence, sizeof frame->sequence);
    host_sha_update(transcript, &frame->row_count, sizeof frame->row_count);
    host_sha_update(transcript, &frame->flags, sizeof frame->flags);
    for (size_t row = 0u; row < frame->row_count; row++) {
        uint8_t encoded_tick[8];
        host_sha_update(transcript, pens + row * LYNX_FRAME_ROW_BYTES, LYNX_FRAME_ROW_BYTES);
        host_sha_update(transcript, palettes + row * 32u, 32u);
        for (size_t byte = 0u; byte < sizeof encoded_tick; byte++)
            encoded_tick[byte] = (uint8_t)(ticks[row] >> (8u * byte));
        host_sha_update(transcript, encoded_tick, sizeof encoded_tick);
    }
}

static int interleaved_step(lynx_machine *machine, uint64_t request,
                            host_sha256 *transcript, uint64_t *last_sequence,
                            uint64_t *last_end, uint64_t *now)
{
    uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
    uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
    lynx_frame_buffers buffers = {0};
    lynx_frame_result frame = {0};
    uint64_t after = 0u;
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.pens = pens; buffers.pens_capacity = sizeof pens;
    buffers.palettes = palettes; buffers.palettes_capacity = sizeof palettes;
    buffers.line_ticks = ticks; buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
    frame.struct_size = (uint32_t)sizeof frame;
    if (lynx_advance_frame(machine, request, &buffers, &frame) != LYNX_OK ||
        frame.ticks_consumed > request || lynx_guest_ticks(machine, &after) != LYNX_OK ||
        after < *now || after - *now != frame.ticks_consumed) return 1;
    if (frame.stop == LYNX_STOP_FRAME) {
        if (frame.frame_end_tick != after || frame.row_count > LYNX_FRAME_MAX_ROWS ||
            (*last_sequence != UINT64_MAX && frame.sequence <= *last_sequence) ||
            (*last_end != 0u && frame.frame_end_tick <= *last_end)) return 1;
        *last_sequence = frame.sequence; *last_end = frame.frame_end_tick;
        replay_transcript_update(transcript, &frame, pens, palettes, ticks);
    } else if (frame.stop == LYNX_STOP_BUDGET) {
        if (frame.ticks_consumed == 0u || frame.ticks_consumed != request) return 1;
    } else {
        return 1;
    }
    if (after == *now && frame.stop != LYNX_STOP_FRAME) return 1;
    *now = after;
    return 0;
}

static int interleaved_advance_to(machine_storage *first, machine_storage *second,
                                 uint64_t target, host_sha256 *first_transcript,
                                 host_sha256 *second_transcript, uint64_t *first_sequence,
                                 uint64_t *second_sequence, uint64_t *first_end,
                                 uint64_t *second_end, uint64_t *first_now,
                                 uint64_t *second_now)
{
    unsigned turn = 0u;
    if (lynx_guest_ticks(first->machine, first_now) != LYNX_OK ||
        lynx_guest_ticks(second->machine, second_now) != LYNX_OK ||
        *first_now != *second_now || target < *first_now) return 1;
    while (*first_now < target || *second_now < target) {
        uint64_t *now = turn == 0u ? first_now : second_now;
        const uint64_t request_limit = turn == 0u ? UINT64_C(60000) : UINT64_C(8191);
        if (*now < target) {
            const uint64_t request = target - *now < request_limit ? target - *now : request_limit;
            if (turn == 0u) {
                if (interleaved_step(first->machine, request, first_transcript,
                                     first_sequence, first_end, first_now) != 0) return 1;
            } else if (interleaved_step(second->machine, request, second_transcript,
                                        second_sequence, second_end, second_now) != 0) return 1;
        }
        turn ^= 1u;
    }
    if (*first_now != target || *second_now != target) return 1;
    {
        uint8_t first_digest[32], second_digest[32];
        if (lynx_state_digest(first->machine, first_digest) != LYNX_OK ||
            lynx_state_digest(second->machine, second_digest) != LYNX_OK ||
            memcmp(first_digest, second_digest, sizeof first_digest) != 0) return 1;
    }
    return 0;
}

static int verify_interleaved_replay(const uint8_t *bll, size_t bll_len,
                                     uint32_t profile, const input_timeline *timeline,
                                     uint64_t finish_tick,
                                     const uint8_t expected_transcript[32])
{
    machine_storage first = {0}, second = {0};
    lynx_identity first_identity, second_identity;
    host_sha256 first_transcript, second_transcript;
    uint64_t first_sequence = UINT64_MAX, second_sequence = UINT64_MAX;
    uint64_t first_end = 0u, second_end = 0u, first_now = 0u, second_now = 0u;
    size_t cursor = 0u;
    uint32_t mask = 0u;
    int result = 1;
    host_sha_init(&first_transcript); host_sha_init(&second_transcript);
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &first, &first_identity) != 0 ||
        create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &second, &second_identity) != 0) goto done;
    if (timeline->count != 0u && timeline->events[0].tick == 0u) {
        if (lynx_set_input(first.machine, timeline->events[0].mask) != LYNX_OK ||
            lynx_set_input(second.machine, timeline->events[0].mask) != LYNX_OK) goto done;
        mask = timeline->events[0].mask; cursor = 1u;
    }
    while (cursor < timeline->count) {
        const input_event *event = &timeline->events[cursor];
        if (interleaved_advance_to(&first, &second, event->tick,
                &first_transcript, &second_transcript, &first_sequence, &second_sequence,
                &first_end, &second_end, &first_now, &second_now) != 0 ||
            lynx_set_input(first.machine, event->mask) != LYNX_OK ||
            lynx_set_input(second.machine, event->mask) != LYNX_OK) goto done;
        mask = event->mask; cursor++;
        {
            uint8_t first_digest[32], second_digest[32];
            if (lynx_state_digest(first.machine, first_digest) != LYNX_OK ||
                lynx_state_digest(second.machine, second_digest) != LYNX_OK ||
                memcmp(first_digest, second_digest, sizeof first_digest) != 0) goto done;
        }
    }
    (void)mask;
    if (interleaved_advance_to(&first, &second, finish_tick,
            &first_transcript, &second_transcript, &first_sequence, &second_sequence,
            &first_end, &second_end, &first_now, &second_now) != 0) goto done;
    {
        uint8_t first_hash[32], second_hash[32];
        host_sha_final(&first_transcript, first_hash); host_sha_final(&second_transcript, second_hash);
        if (memcmp(first_hash, second_hash, sizeof first_hash) != 0 ||
            memcmp(first_hash, expected_transcript, sizeof first_hash) != 0) goto done;
    }
    result = 0;
done:
    destroy_machine(&first); destroy_machine(&second);
    return result;
}

static int run_frame_consumer(const char *content_path, const char *symbols_path,
                              const char *timeline_path, const char *output_directory,
                              uint32_t profile)
{
    uint8_t *bll = NULL, *labels = NULL, *timeline_bytes = NULL, script_digest[32];
    size_t bll_len = 0u, labels_len = 0u, timeline_len = 0u, cursor = 0u, frame_count = 0u;
    uint32_t probe_address = 0u, handedness_address = 0u, current_mask = 0u;
    uint32_t source_handedness = 0u;
    uint64_t previous_sequence = UINT64_MAX, previous_frame_end = 0u, finish_tick, last_tick;
    uint64_t pending_edge_tick = UINT64_MAX;
    uint32_t pending_prior_mask = 0u;
    host_sha256 active_transcript;
    uint8_t active_transcript_digest[32];
    char input_hash[65], content_hash[65];
    char *manifest = NULL;
    size_t manifest_len = 0u;
    input_timeline timeline = {0};
    frame_capture frames[FRAME_OUTPUT_LIMIT] = {0};
    machine_storage active = {0}, restore_target = {0};
    lynx_identity identity, restore_identity;
    int checkpoint_test = 0, target_created = 0, interleaved_test = 0, result = 1;
    host_sha_init(&active_transcript);
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *requested = getenv("LYNX_TEST_REPLAY_INTERLEAVED");
        interleaved_test = requested != NULL && strcmp(requested, "1") == 0;
    }
#endif
    if (replay_sha_vectors() != 0) { (void)report_error("independent SHA-256 vectors failed"); goto done; }
    if (input_paths_are_distinct(content_path, symbols_path, timeline_path,
            "frame input paths must refer to distinct files") != 0) goto done;
    if (read_bounded(timeline_path, TIMELINE_LIMIT, &timeline_bytes, &timeline_len, "input timeline") != 0 ||
        parse_input_timeline(timeline_bytes, timeline_len, &timeline) != 0) {
        (void)report_error("timeline is not canonical LYNX-INPUT 1 ASCII; use strict increasing decimal tick/mask LF rows within the documented bounds"); goto done;
    }
    host_sha_digest(timeline_bytes, timeline_len, script_digest); host_sha_hex(script_digest, input_hash);
    last_tick = timeline.count == 0u ? 0u : timeline.events[timeline.count - 1u].tick;
    if (last_tick > UINT64_MAX - UINT64_C(108545)) finish_tick = last_tick;
    else finish_tick = last_tick + UINT64_C(108545);
    if (finish_tick < UINT64_C(108545)) finish_tick = UINT64_C(108545);
    {
        char normalized[4096];
        if (normalize_destination(output_directory, normalized, sizeof normalized) != 0 || destination_is_absent(normalized) != 0) {
            (void)report_error("frame output must name a new absent destination"); goto done;
        }
    }
    if (read_bounded(content_path, BLL_LIMIT, &bll, &bll_len, "BLL content") != 0 || bll_len < 11u ||
        read_bounded(symbols_path, SYMBOL_LIMIT, &labels, &labels_len, "ld65 -Ln symbols") != 0 ||
        parse_named_symbol(labels, labels_len, "probe_result", &probe_address) != 0 ||
        probe_address > LYNX_ADDRESS_SPACE - 8u ||
        parse_named_symbol(labels, labels_len, "probe_lefthand", &handedness_address) != 0 ||
        handedness_address >= LYNX_ADDRESS_SPACE) {
        (void)report_error("input-probe BLL or linked probe_result symbol is invalid; rebuild source with the pinned fixture recipe"); goto done;
    }
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1, &active, &identity) != 0) goto done;
    {
        uint8_t handedness = 0u;
        if (lynx_read_memory(active.machine, handedness_address, &handedness, 1u) != LYNX_OK || handedness > 1u) {
            (void)report_error("source-linked input-probe handedness symbol is outside its declared 0/1 range"); goto done;
        }
        source_handedness = handedness;
    }
    encode_hex(identity.content_sha256, content_hash);
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *requested = getenv("LYNX_TEST_REPLAY_RESTORE");
        checkpoint_test = requested != NULL && strcmp(requested, "1") == 0;
    }
#endif
    if (checkpoint_test != 0 && (timeline.count != 1u || timeline.events[0].tick != UINT64_C(108544) || timeline.events[0].mask != LYNX_INPUT_A)) {
        (void)report_error("checkpoint probe requires one A edge at tick 108544"); goto done;
    }
    if (timeline.count != 0u && timeline.events[0].tick == 0u) {
        if (lynx_set_input(active.machine, timeline.events[0].mask) != LYNX_OK) { (void)report_error("public input API refused validated tick-zero input"); goto done; }
        current_mask = timeline.events[0].mask; cursor = 1u;
    }
    while (cursor < timeline.count) {
        const input_event *event = &timeline.events[cursor];
        uint64_t now = 0u;
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || event->tick < now) { (void)report_error("timeline event precedes current guest time"); goto done; }
        while (now < event->tick) {
            uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
            uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], after = 0u;
            lynx_frame_buffers buffers = {0}; lynx_frame_result frame = {0};
            const uint64_t budget = event->tick - now < UINT64_C(60000) ? event->tick - now : UINT64_C(60000);
            buffers.struct_size = (uint32_t)sizeof buffers; buffers.pens = pens; buffers.pens_capacity = sizeof pens;
            buffers.palettes = palettes; buffers.palettes_capacity = sizeof palettes;
            buffers.line_ticks = ticks; buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
            frame.struct_size = (uint32_t)sizeof frame;
            if (lynx_advance_frame(active.machine, budget, &buffers, &frame) != LYNX_OK || frame.ticks_consumed > budget ||
                lynx_guest_ticks(active.machine, &after) != LYNX_OK || after < now || after > event->tick || after - now != frame.ticks_consumed) {
                (void)report_error("frame advance violated its exact half-open event interval"); goto done;
            }
            if (frame.stop == LYNX_STOP_FRAME) {
                if ((previous_sequence != UINT64_MAX && frame.sequence <= previous_sequence) || frame.frame_end_tick != after ||
                    (previous_frame_end != 0u && frame.frame_end_tick <= previous_frame_end)) {
                    (void)report_error("duplicate or nonmonotone frame sequence/tick stopped replay"); goto done;
                }
                previous_sequence = frame.sequence; previous_frame_end = frame.frame_end_tick;
                replay_transcript_update(&active_transcript, &frame, pens, palettes, ticks);
                if (frame.row_count == 0u) {
                    if (frame.sequence != 0u) { (void)report_error("only sequence zero may synchronize without rows"); goto done; }
                } else {
                    uint8_t digest[32];
                    frame_capture *capture;
                    guest_probe sample = {0};
                    uint32_t handedness = 0u;
                    uint32_t frame_mask = current_mask;
                    if (frame.ticks_consumed == 0u && pending_edge_tick == frame.frame_end_tick) {
                        frame_mask = pending_prior_mask;
                        pending_edge_tick = UINT64_MAX;
                    } else if (pending_edge_tick != UINT64_MAX && frame.frame_end_tick > pending_edge_tick) {
                        pending_edge_tick = UINT64_MAX;
                    }
                    const char *observation_failure = "homebrew input response mismatch";
                    if (frame_count >= FRAME_OUTPUT_LIMIT || frame.row_count > LYNX_FRAME_MAX_ROWS) {
                        (void)report_error("frame output bound or row count exceeded its fixed limit"); goto done;
                    }
                    if (replay_guest_observation(active.machine, probe_address, handedness_address,
                            frame_mask, &handedness, &sample, &observation_failure) != 0 ||
                        handedness != source_handedness) {
                        (void)report_error(observation_failure); goto done;
                    }
                    capture = &frames[frame_count]; memset(capture, 0, sizeof *capture);
                    capture->result = frame; capture->input_mask = frame_mask;
                    capture->sample = sample; capture->handedness = handedness;
                    if (replay_raw_bytes(&frame, pens, palettes, ticks, &capture->raw, &capture->raw_len) != 0) {
                        (void)report_error("raw pen/palette/tick capture failed its exact bounds"); goto done;
                    }
                    host_sha_digest(capture->raw, capture->raw_len, digest); host_sha_hex(digest, capture->raw_sha256);
                    if (replay_render_ppm(capture, pens, palettes) != 0) { (void)report_error("raw frame rendering exceeded its bounded PPM surface"); goto done; }
                    frame_count++;
                }
            } else if (frame.stop == LYNX_STOP_BUDGET) {
                if (frame.ticks_consumed == 0u) { (void)report_error("guest-time limit reached: BUDGET consumed zero before the requested event tick"); goto done; }
                if (frame.ticks_consumed != budget && after < event->tick) { (void)report_error("guest-time limit stopped replay before the requested event tick"); goto done; }
            } else if (frame.stop == LYNX_STOP_WATCH) {
                (void)report_error("WATCH is unsupported in frame replay; no input edge was applied early"); goto done;
            } else {
                (void)report_error("UNDEFINED or UNAVAILABLE guest stop occurred before the requested input tick"); goto done;
            }
            if (after == now && frame.stop != LYNX_STOP_FRAME) { (void)report_error("frame replay made no progress before the event tick"); goto done; }
            now = after;
        }
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || now != event->tick) { (void)report_error("host did not reach exact event tick before setter"); goto done; }
        if (checkpoint_test != 0) {
            replay_checkpoint checkpoint = {{0}, event->tick, cursor, current_mask, CHECKPOINT_PRE_EDGE};
            if (!target_created) {
                if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1, &restore_target, &restore_identity) != 0) goto done;
                target_created = 1;
            }
            memcpy(checkpoint.script_digest, script_digest, sizeof script_digest);
#if defined(LYNX_CONSUMER_TESTING)
            if (getenv("LYNX_TEST_REPLAY_RESTORE_BAD_SCRIPT") != NULL &&
                strcmp(getenv("LYNX_TEST_REPLAY_RESTORE_BAD_SCRIPT"), "1") == 0)
                checkpoint.script_digest[0] ^= 1u;
#endif
            if (replay_restore_checkpoint(active.machine, restore_target.machine, &checkpoint, &timeline, script_digest) != 0) {
                (void)report_error("pre-edge checkpoint restore failed script digest, tick, lower-bound cursor, prior mask or state digest"); goto done;
            }
            { machine_storage swap = active; active = restore_target; restore_target = swap; }
        }
        const uint32_t prior_mask = current_mask;
        if (lynx_set_input(active.machine, event->mask) != LYNX_OK) { (void)report_error("public setter rejected a validated event mask at exact tick"); goto done; }
        pending_edge_tick = event->tick; pending_prior_mask = prior_mask;
        current_mask = event->mask; cursor++;
        if (checkpoint_test != 0) {
            replay_checkpoint checkpoint = {{0}, event->tick, cursor, current_mask, CHECKPOINT_POST_EDGE};
            memcpy(checkpoint.script_digest, script_digest, sizeof script_digest);
#if defined(LYNX_CONSUMER_TESTING)
            if (getenv("LYNX_TEST_REPLAY_RESTORE_BAD_SCRIPT") != NULL &&
                strcmp(getenv("LYNX_TEST_REPLAY_RESTORE_BAD_SCRIPT"), "1") == 0)
                checkpoint.script_digest[0] ^= 1u;
#endif
            if (replay_restore_checkpoint(active.machine, restore_target.machine, &checkpoint, &timeline, script_digest) != 0) {
                (void)report_error("post-edge checkpoint restore failed script digest, tick, upper-bound cursor, applied mask or state digest"); goto done;
            }
            { machine_storage swap = active; active = restore_target; restore_target = swap; }
        }
    }
    {
        uint64_t now = 0u;
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || finish_tick < now) { (void)report_error("bounded final observation tick precedes guest time"); goto done; }
        while (now < finish_tick) {
            uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
            uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], after = 0u;
            lynx_frame_buffers buffers = {0}; lynx_frame_result frame = {0};
            const uint64_t budget = finish_tick - now < UINT64_C(60000) ? finish_tick - now : UINT64_C(60000);
            buffers.struct_size = (uint32_t)sizeof buffers; buffers.pens = pens; buffers.pens_capacity = sizeof pens;
            buffers.palettes = palettes; buffers.palettes_capacity = sizeof palettes;
            buffers.line_ticks = ticks; buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
            frame.struct_size = (uint32_t)sizeof frame;
            if (lynx_advance_frame(active.machine, budget, &buffers, &frame) != LYNX_OK || frame.ticks_consumed > budget ||
                lynx_guest_ticks(active.machine, &after) != LYNX_OK || after < now || after > finish_tick || after - now != frame.ticks_consumed) {
                (void)report_error("final frame interval violated exact guest-time bounds"); goto done;
            }
            if (frame.stop == LYNX_STOP_FRAME) {
                if ((previous_sequence != UINT64_MAX && frame.sequence <= previous_sequence) || frame.frame_end_tick != after ||
                    (previous_frame_end != 0u && frame.frame_end_tick <= previous_frame_end)) { (void)report_error("final frame sequence or tick is not monotone"); goto done; }
                previous_sequence = frame.sequence; previous_frame_end = frame.frame_end_tick;
                replay_transcript_update(&active_transcript, &frame, pens, palettes, ticks);
                if (frame.row_count != 0u) {
                    uint8_t digest[32]; frame_capture *capture;
                    guest_probe sample = {0};
                    uint32_t handedness = 0u;
                    uint32_t frame_mask = current_mask;
                    if (frame.ticks_consumed == 0u && pending_edge_tick == frame.frame_end_tick) {
                        frame_mask = pending_prior_mask;
                        pending_edge_tick = UINT64_MAX;
                    } else if (pending_edge_tick != UINT64_MAX && frame.frame_end_tick > pending_edge_tick) {
                        pending_edge_tick = UINT64_MAX;
                    }
                    if (frame_count >= FRAME_OUTPUT_LIMIT || frame.row_count > LYNX_FRAME_MAX_ROWS) { (void)report_error("final frame exceeded the bounded output count or row count"); goto done; }
                    const char *observation_failure = "homebrew input response mismatch";
                    if (replay_guest_observation(active.machine, probe_address, handedness_address,
                            frame_mask, &handedness, &sample, &observation_failure) != 0 ||
                        handedness != source_handedness) {
                        (void)report_error(observation_failure); goto done;
                    }
                    capture = &frames[frame_count]; memset(capture, 0, sizeof *capture); capture->result = frame;
                    capture->input_mask = frame_mask; capture->sample = sample; capture->handedness = handedness;
                    if (replay_raw_bytes(&frame, pens, palettes, ticks, &capture->raw, &capture->raw_len) != 0) { (void)report_error("final raw capture failed bounds"); goto done; }
                    host_sha_digest(capture->raw, capture->raw_len, digest); host_sha_hex(digest, capture->raw_sha256);
                    if (replay_render_ppm(capture, pens, palettes) != 0) { (void)report_error("final raw frame rendering failed bounds"); goto done; }
                    frame_count++;
                } else if (frame.sequence != 0u) { (void)report_error("unexpected zero-row final frame"); goto done; }
            } else if (frame.stop == LYNX_STOP_BUDGET) {
                if (frame.ticks_consumed == 0u) { (void)report_error("guest-time limit reached during final frame window"); goto done; }
                if (frame.ticks_consumed != budget && after < finish_tick) { (void)report_error("guest-time limit stopped the final frame window early"); goto done; }
            } else { (void)report_error("unsupported guest stop during final frame window"); goto done; }
            now = after;
        }
    }
    host_sha_final(&active_transcript, active_transcript_digest);
    if (interleaved_test != 0 &&
        verify_interleaved_replay(bll, bll_len, profile, &timeline, finish_tick,
                                  active_transcript_digest) != 0) {
        (void)report_error("interleaved fresh-instance raw frame transcript differs from sequential replay at the common event ticks");
        goto done;
    }
    manifest = malloc(FRAME_MANIFEST_CAP);
    if (manifest == NULL) { (void)report_error("cannot allocate bounded replay manifest"); goto done; }
    if (replay_append(manifest, FRAME_MANIFEST_CAP, &manifest_len,
        "artifact = FRAME_TRACE\nprofile = %s\nstartup = A\ntiming_policy = video-policy1-unverified\nbuild_id = %s\n"
        "fixture_schema = LXI5-v3\nhandedness = %u\ncontent_sha256 = %s\ninput_sha256 = %s\n"
        "event_count = %zu\nfinish_tick = %" PRIu64 "\nframe_count = %zu\n"
        "repeatability = test harness compares raw and rendered digests across fresh and interleaved runs\n"
        "raw_pen_order = high nibble first; no host rotation\npalette_policy = per-line GREEN then BLUERED raw nibbles; intra-line color accuracy unverified\n",
        lynx_profile_name(profile), lynx_build_id(), source_handedness, content_hash, input_hash,
        timeline.count, finish_tick, frame_count) != 0) {
        (void)report_error("replay manifest exceeded its fixed bound"); goto done;
    }
    for (size_t i = 0u; i < frame_count; i++) {
        const lynx_frame_result *f = &frames[i].result;
        if (replay_append(manifest, FRAME_MANIFEST_CAP, &manifest_len,
            "frame[%zu] = sequence:%" PRIu64 ",start:%" PRIu64 ",end:%" PRIu64 ",rows:%u,flags:%u,mask:%u,sample_a:%u,joy:%u,switches:%u,progress:%u,error:%u,done:%u,handedness:%u,raw_sha256:%s,rendered_sha256:%s\n",
            i, f->sequence, f->frame_start_tick, f->frame_end_tick, f->row_count,
            f->flags, frames[i].input_mask, frames[i].sample.joystick & 1u,
            frames[i].sample.joystick, frames[i].sample.switches,
            frames[i].sample.progress, frames[i].sample.error,
            frames[i].sample.done, frames[i].handedness,
            frames[i].raw_sha256, frames[i].rendered_sha256) != 0) { (void)report_error("frame transcript exceeded its bound"); goto done; }
    }
    if (publish_frame_outputs_generic(output_directory, frames, frame_count, manifest, manifest_len) != 0) goto done;
#if defined(LYNX_CONSUMER_TESTING)
    if (interleaved_test != 0)
        puts("lynx-consume-test: interleaved replay verification passed");
    if (checkpoint_test != 0)
        puts("lynx-consume-test: pre/post-edge checkpoint restore checks passed");
#endif
    printf("lynx-consume: replayed %zu canonical input events and published %zu raw/rendered frames\n", timeline.count, frame_count);
    result = 0;
done:
    free(bll); free(labels); free(timeline_bytes); free(timeline.events); free(manifest);
    for (size_t i = 0u; i < FRAME_OUTPUT_LIMIT; i++) { free(frames[i].raw); free(frames[i].ppm); }
    destroy_machine(&active); destroy_machine(&restore_target);
    return result;
}

static void av_store_u16le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void av_store_u32le(uint8_t *out, uint32_t value)
{
    for (size_t i = 0u; i < 4u; i++) out[i] = (uint8_t)(value >> (8u * i));
}

static void av_store_u64le(uint8_t *out, uint64_t value)
{
    for (size_t i = 0u; i < 8u; i++) out[i] = (uint8_t)(value >> (8u * i));
}

static int av_build_raw(const uint8_t *pens, const uint8_t *palettes,
                        const uint64_t *ticks, const lynx_frame_result *frame,
                        uint8_t **out, size_t *out_len)
{
    if (frame->row_count == 0u || frame->row_count > LYNX_FRAME_MAX_ROWS) return 1;
    const size_t row_bytes = 80u + 32u + 8u;
    const size_t length = AV_RAW_HEADER_BYTES + (size_t)frame->row_count * row_bytes;
    uint8_t *raw = malloc(length);
    if (raw == NULL) return 1;
    memcpy(raw, "LXAVRAW1", 8u);
    av_store_u32le(raw + 8u, frame->row_count);
    av_store_u64le(raw + 12u, frame->sequence);
    av_store_u32le(raw + 20u, frame->flags);
    size_t at = AV_RAW_HEADER_BYTES;
    for (size_t row = 0u; row < frame->row_count; row++) {
        memcpy(raw + at, pens + row * LYNX_FRAME_ROW_BYTES, 80u); at += 80u;
        memcpy(raw + at, palettes + row * 32u, 32u); at += 32u;
        av_store_u64le(raw + at, ticks[row]); at += 8u;
    }
    *out = raw;
    *out_len = length;
    return 0;
}

static int av_build_pcm_bytes(const lynx_pcm_frame *pcm, size_t count,
                              uint8_t **out, size_t *out_len)
{
    if (count > AV_PCM_CAPACITY || count > (SIZE_MAX - AV_WAV_HEADER_BYTES) / 4u) return 1;
    const size_t length = count * 4u;
    uint8_t *bytes = malloc(length == 0u ? 1u : length);
    if (bytes == NULL) return 1;
    for (size_t i = 0u; i < count; i++) {
        av_store_u16le(bytes + i * 4u, (uint16_t)pcm[i].left);
        av_store_u16le(bytes + i * 4u + 2u, (uint16_t)pcm[i].right);
    }
    *out = bytes;
    *out_len = length;
    return 0;
}

static int av_build_wav(const uint8_t *pcm_bytes, size_t pcm_bytes_len,
                        uint8_t **out, size_t *out_len)
{
    if (pcm_bytes_len > UINT32_MAX || pcm_bytes_len % 4u != 0u ||
        pcm_bytes_len > SIZE_MAX - AV_WAV_HEADER_BYTES) return 1;
    const size_t length = AV_WAV_HEADER_BYTES + pcm_bytes_len;
    uint8_t *wav = calloc(length, 1u);
    if (wav == NULL) return 1;
    memcpy(wav, "RIFF", 4u); av_store_u32le(wav + 4u, (uint32_t)(length - 8u));
    memcpy(wav + 8u, "WAVEfmt ", 8u); av_store_u32le(wav + 16u, 16u);
    av_store_u16le(wav + 20u, 1u); av_store_u16le(wav + 22u, 2u);
    av_store_u32le(wav + 24u, LYNX_AUDIO_RATE_NUM);
    av_store_u32le(wav + 28u, LYNX_AUDIO_RATE_NUM * 4u);
    av_store_u16le(wav + 32u, 4u); av_store_u16le(wav + 34u, 16u);
    memcpy(wav + 36u, "data", 4u); av_store_u32le(wav + 40u, (uint32_t)pcm_bytes_len);
    memcpy(wav + AV_WAV_HEADER_BYTES, pcm_bytes, pcm_bytes_len);
    *out = wav; *out_len = length;
    return 0;
}

static int av_build_event_text(const lynx_audio_event *events, size_t count,
                               char **out, size_t *out_len)
{
    const size_t capacity = 64u + count * 96u;
    char *text = malloc(capacity);
    size_t at;
    if (text == NULL) return 1;
    int n = snprintf(text, capacity, "LYNX-AUDIO-EVENTS 1\n");
    if (n < 0 || (size_t)n >= capacity) { free(text); return 1; }
    at = (size_t)n;
    for (size_t i = 0u; i < count; i++) {
        const char *cause = events[i].cause == LYNX_AUDIO_EVENT_WRITE ? "write" :
                            events[i].cause == LYNX_AUDIO_EVENT_CLOCK ? "clock" : "unknown";
        n = snprintf(text + at, capacity - at,
            "%" PRIu64 " %u %u %s %d %d\n", events[i].tick, events[i].sequence,
            events[i].channel, cause, (int)events[i].before_output,
            (int)events[i].after_output);
        if (n < 0 || (size_t)n >= capacity - at) { free(text); return 1; }
        at += (size_t)n;
    }
    *out = text; *out_len = at;
    return 0;
}

typedef struct av_output_file {
    const char *name;
    const void *bytes;
    size_t length;
} av_output_file;

static int publish_av_outputs(const char *directory, const av_output_file *files,
                              size_t file_count)
{
    char normalized[4096], staging[4096], probe[4096], path[4096], owner[4096];
    char owner_value[4096];
    char staged_paths[16][4096] = {{0}};
    int stage_length;
    size_t written = 0u;
    int made_staging = 0, stage_identity_valid = 0, nonce_verified = 0;
    struct stat stage_identity = {0};
    if (file_count == 0u || file_count > sizeof staged_paths / sizeof staged_paths[0] ||
        normalize_destination(directory, normalized, sizeof normalized) != 0 ||
        destination_is_absent(normalized) != 0)
        return report_error("A/V output must name a bounded new destination directory");
    for (size_t i = 0u; i < file_count; i++) {
        if (output_path(normalized, files[i].name, probe, sizeof probe) != 0) return 1;
    }
    stage_length = snprintf(staging, sizeof staging, "%s.lynxplosion-av-stage-XXXXXX", normalized);
    if (stage_length < 0 || (size_t)stage_length >= sizeof staging || mkdtemp(staging) == NULL)
        return report_error("cannot create a private sibling A/V staging directory");
    made_staging = 1;
    if (lstat(staging, &stage_identity) != 0 || !S_ISDIR(stage_identity.st_mode)) goto failed;
    stage_identity_valid = 1;
    if (output_path(staging, ".lynx-owner", owner, sizeof owner) != 0) goto failed;
    {
        const char *base = strrchr(staging, '/');
        base = base == NULL ? staging : base + 1;
        stage_length = snprintf(owner_value, sizeof owner_value, "%s\n", base);
    }
    if (stage_length < 0 || (size_t)stage_length >= sizeof owner_value ||
        write_output_owner(owner, owner_value, (size_t)stage_length) != 0) goto failed;
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *replace = getenv("LYNX_TEST_AV_REPLACE_STAGE");
        if (replace != NULL && strcmp(replace, "1") == 0) {
            static const char foreign_owner[] = "foreign test stage\n";
            static const char preserved[] = "preserve replaced stage bytes\n";
            const char *sentinel = getenv("LYNX_TEST_AV_REPLACED_SENTINEL");
            char replaced[4096], replacement_owner[4096];
            stage_length = snprintf(replaced, sizeof replaced, "%s.replaced", staging);
            if (stage_length < 0 || (size_t)stage_length >= sizeof replaced ||
                rename(staging, replaced) != 0 || mkdir(staging, 0700) != 0 ||
                output_path(staging, ".lynx-owner", replacement_owner, sizeof replacement_owner) != 0 ||
                write_output_owner(replacement_owner, foreign_owner, sizeof foreign_owner - 1u) != 0 ||
                sentinel == NULL ||
                write_output_owner(sentinel, preserved, sizeof preserved - 1u) != 0) goto failed;
        }
    }
#endif
    if (!output_stage_is_owned(staging, &stage_identity)) {
        (void)report_error("A/V staging directory inode or invocation marker changed before writing");
        goto failed;
    }
    for (size_t i = 0u; i < file_count; i++) {
        if (output_path(staging, files[i].name, path, sizeof path) != 0) goto failed;
        memcpy(staged_paths[i], path, strlen(path) + 1u);
        written = i + 1u;
        if (write_exclusive(path, files[i].bytes, files[i].length) != 0) goto failed;
    }
    if (!output_stage_is_owned(staging, &stage_identity)) {
        (void)report_error("A/V staging directory inode or invocation marker changed before publication");
        goto failed;
    }
    nonce_verified = 1;
    if (unlink(owner) != 0) goto failed;
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *abort_point = getenv("LYNX_TEST_AV_ABORT");
        if (abort_point != NULL && strcmp(abort_point, "before-publish") == 0) {
            (void)report_error("test-only A/V interruption before publish");
            goto failed;
        }
    }
#endif
    if (publish_directory_exclusive(staging, normalized) != 0) {
        (void)report_error(errno == EEXIST ?
            "A/V output directory already exists; choose a new destination" :
            "cannot publish the complete A/V directory without replacement");
        goto failed;
    }
    made_staging = 0;
    return 0;
failed:
    if (made_staging != 0 && stage_identity_valid != 0 &&
        cleanup_output_stage(staging, &stage_identity, nonce_verified, staged_paths, written) != 0)
        (void)report_error("A/V stage cleanup refused because its invocation marker or directory identity changed");
    return report_error("could not write the complete A/V artifact bundle exclusively");
}

typedef struct av_capture_totals {
    lynx_pcm_frame *pcm;
    lynx_audio_event *events;
    size_t pcm_count;
    size_t event_count;
    uint64_t next_ordinal;
} av_capture_totals;

static int av_step(lynx_machine *machine, uint64_t budget,
                   uint8_t *pens, uint8_t *palettes, uint64_t *line_ticks,
                   size_t pcm_span_capacity, size_t event_span_capacity,
                   av_capture_totals *totals, lynx_av_result *result)
{
    lynx_av_buffers buffers = {0};
    uint64_t before = 0u, after = 0u;
    size_t pcm_remaining, event_remaining;
    int advance_status;
    if (totals->pcm_count >= AV_PCM_CAPACITY || totals->event_count >= AV_EVENT_CAPACITY ||
        lynx_guest_ticks(machine, &before) != LYNX_OK) return 1;
    pcm_remaining = AV_PCM_CAPACITY - totals->pcm_count;
    event_remaining = AV_EVENT_CAPACITY - totals->event_count;
    if (pcm_span_capacity == 0u || event_span_capacity < LYNX_AV_MIN_EVENT_CAPACITY) return 1;
    if (pcm_span_capacity > pcm_remaining) pcm_span_capacity = pcm_remaining;
    if (event_span_capacity > event_remaining) event_span_capacity = event_remaining;
    buffers.struct_size = (uint32_t)sizeof buffers;
    buffers.frame.struct_size = (uint32_t)sizeof buffers.frame;
    buffers.frame.pens = pens; buffers.frame.pens_capacity = LYNX_FRAME_PENS_CAPACITY;
    buffers.frame.palettes = palettes; buffers.frame.palettes_capacity = LYNX_FRAME_PALETTE_CAPACITY;
    buffers.frame.line_ticks = line_ticks; buffers.frame.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
    buffers.pcm = totals->pcm + totals->pcm_count;
    buffers.pcm_capacity = pcm_span_capacity;
    buffers.events = totals->events + totals->event_count;
    buffers.event_capacity = event_span_capacity;
    result->struct_size = (uint32_t)sizeof *result;
    advance_status = (int)lynx_advance_av(machine, budget, &buffers, result);
    if (advance_status != (int)LYNX_OK) {
        fprintf(stderr, "lynx-consume: public A/V advance returned status %d for budget %" PRIu64 "\n",
                advance_status, budget);
        return 1;
    }
    if (result->ticks_consumed > budget || result->pcm_count > buffers.pcm_capacity ||
        result->event_count > buffers.event_capacity ||
        result->first_sample_ordinal != totals->next_ordinal ||
        result->next_sample_ordinal < result->first_sample_ordinal ||
        result->next_sample_ordinal - result->first_sample_ordinal != result->pcm_count) {
        fprintf(stderr, "lynx-consume: A/V result contract mismatch budget=%" PRIu64
                " consumed=%" PRIu64 " stop=%u pcm=%u/%zu events=%u/%zu ordinal=%" PRIu64 "..%" PRIu64 " expected=%" PRIu64 "\n",
                budget, result->ticks_consumed, (unsigned)result->stop, (unsigned)result->pcm_count,
                buffers.pcm_capacity, (unsigned)result->event_count, buffers.event_capacity,
                result->first_sample_ordinal, result->next_sample_ordinal, totals->next_ordinal);
        return 1;
    }
    if (lynx_guest_ticks(machine, &after) != LYNX_OK || after < before ||
        after - before != result->ticks_consumed ||
        (result->stop == LYNX_STOP_OUTPUT && result->ticks_consumed == 0u &&
         result->pcm_count == 0u && result->event_count == 0u)) {
        fprintf(stderr, "lynx-consume: A/V progress contract mismatch before=%" PRIu64
                " after=%" PRIu64 " budget=%" PRIu64 " consumed=%" PRIu64 " stop=%u\n",
                before, after, budget, result->ticks_consumed, (unsigned)result->stop);
        return 1;
    }
    totals->pcm_count += result->pcm_count;
    totals->event_count += result->event_count;
    totals->next_ordinal = result->next_sample_ordinal;
    return 0;
}

#if defined(LYNX_CONSUMER_TESTING)
typedef struct av_interleaved_pair {
    machine_storage first;
    machine_storage second;
    av_capture_totals first_totals;
    av_capture_totals second_totals;
    int enabled;
} av_interleaved_pair;

static int av_result_equal(const lynx_av_result *left, const lynx_av_result *right)
{
    return left->struct_size == right->struct_size && left->stop == right->stop &&
        left->ticks_consumed == right->ticks_consumed &&
        left->frame_start_tick == right->frame_start_tick &&
        left->frame_end_tick == right->frame_end_tick &&
        left->frame_sequence == right->frame_sequence && left->row_count == right->row_count &&
        left->frame_flags == right->frame_flags &&
        left->first_sample_ordinal == right->first_sample_ordinal &&
        left->next_sample_ordinal == right->next_sample_ordinal &&
        left->pcm_count == right->pcm_count && left->event_count == right->event_count;
}

static int av_totals_delta_equal(const av_capture_totals *left, size_t left_pcm_before,
                                 size_t left_event_before, const av_capture_totals *right,
                                 size_t right_pcm_before, size_t right_event_before)
{
    const size_t left_pcm_count = left->pcm_count - left_pcm_before;
    const size_t right_pcm_count = right->pcm_count - right_pcm_before;
    const size_t left_event_count = left->event_count - left_event_before;
    const size_t right_event_count = right->event_count - right_event_before;
    if (left_pcm_count != right_pcm_count || left_event_count != right_event_count ||
        left->next_ordinal != right->next_ordinal) return 1;
    for (size_t i = 0u; i < left_pcm_count; i++) {
        if (left->pcm[left_pcm_before + i].left != right->pcm[right_pcm_before + i].left ||
            left->pcm[left_pcm_before + i].right != right->pcm[right_pcm_before + i].right) return 1;
    }
    for (size_t i = 0u; i < left_event_count; i++) {
        const lynx_audio_event *a = &left->events[left_event_before + i];
        const lynx_audio_event *b = &right->events[right_event_before + i];
        if (a->tick != b->tick || a->sequence != b->sequence || a->channel != b->channel ||
            a->cause != b->cause || a->before_output != b->before_output ||
            a->after_output != b->after_output) return 1;
    }
    return 0;
}

static int av_interleaved_init(av_interleaved_pair *pair, const uint8_t *bll,
                               size_t bll_len, uint32_t profile,
                               const lynx_identity *expected)
{
    lynx_identity first_identity, second_identity;
    memset(pair, 0, sizeof *pair);
    pair->first_totals.pcm = calloc(AV_PCM_CAPACITY, sizeof *pair->first_totals.pcm);
    pair->second_totals.pcm = calloc(AV_PCM_CAPACITY, sizeof *pair->second_totals.pcm);
    pair->first_totals.events = calloc(AV_EVENT_CAPACITY, sizeof *pair->first_totals.events);
    pair->second_totals.events = calloc(AV_EVENT_CAPACITY, sizeof *pair->second_totals.events);
    if (pair->first_totals.pcm == NULL || pair->second_totals.pcm == NULL ||
        pair->first_totals.events == NULL || pair->second_totals.events == NULL ||
        create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &pair->first, &first_identity) != 0 ||
        create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &pair->second, &second_identity) != 0 ||
        first_identity.profile != expected->profile || second_identity.profile != expected->profile ||
        first_identity.timing_policy != expected->timing_policy ||
        second_identity.timing_policy != expected->timing_policy ||
        first_identity.audio_policy != expected->audio_policy ||
        second_identity.audio_policy != expected->audio_policy ||
        memcmp(first_identity.content_sha256, expected->content_sha256, 32u) != 0 ||
        memcmp(second_identity.content_sha256, expected->content_sha256, 32u) != 0) return 1;
    pair->enabled = 1;
    return 0;
}

static void av_interleaved_destroy(av_interleaved_pair *pair)
{
    destroy_machine(&pair->first);
    destroy_machine(&pair->second);
    free(pair->first_totals.pcm);
    free(pair->second_totals.pcm);
    free(pair->first_totals.events);
    free(pair->second_totals.events);
    memset(pair, 0, sizeof *pair);
}

static int av_interleaved_set_input(av_interleaved_pair *pair, uint32_t mask)
{
    if (pair->enabled == 0) return 0;
    if (lynx_set_input(pair->first.machine, mask) != LYNX_OK ||
        lynx_set_input(pair->second.machine, mask) != LYNX_OK) return 1;
    {
        uint8_t first[32], second[32];
        if (lynx_state_digest(pair->first.machine, first) != LYNX_OK ||
            lynx_state_digest(pair->second.machine, second) != LYNX_OK ||
            memcmp(first, second, sizeof first) != 0) return 1;
    }
    return 0;
}

static int av_step_interleaved(lynx_machine *primary, uint64_t budget,
                               uint8_t *pens, uint8_t *palettes, uint64_t *ticks,
                               size_t pcm_capacity, size_t event_capacity,
                               av_capture_totals *primary_totals, lynx_av_result *primary_result,
                               av_interleaved_pair *pair)
{
    if (av_step(primary, budget, pens, palettes, ticks, pcm_capacity,
                event_capacity, primary_totals, primary_result) != 0) return 1;
    if (pair->enabled != 0) {
        uint8_t first_pens[LYNX_FRAME_PENS_CAPACITY], first_palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint8_t second_pens[LYNX_FRAME_PENS_CAPACITY], second_palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint64_t first_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], second_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        lynx_av_result first_result = {0}, second_result = {0};
        const size_t primary_pcm_before = primary_totals->pcm_count - primary_result->pcm_count;
        const size_t primary_event_before = primary_totals->event_count - primary_result->event_count;
        const size_t first_pcm_before = pair->first_totals.pcm_count;
        const size_t first_event_before = pair->first_totals.event_count;
        const size_t second_pcm_before = pair->second_totals.pcm_count;
        const size_t second_event_before = pair->second_totals.event_count;
        uint8_t primary_digest[32], first_digest[32], second_digest[32];
        if (av_step(pair->first.machine, budget, first_pens, first_palettes, first_ticks,
                    pcm_capacity, event_capacity, &pair->first_totals, &first_result) != 0 ||
            av_step(pair->second.machine, budget, second_pens, second_palettes, second_ticks,
                    pcm_capacity, event_capacity, &pair->second_totals, &second_result) != 0 ||
            !av_result_equal(primary_result, &first_result) ||
            !av_result_equal(primary_result, &second_result) ||
            av_totals_delta_equal(primary_totals, primary_pcm_before, primary_event_before,
                                  &pair->first_totals, first_pcm_before, first_event_before) != 0 ||
            av_totals_delta_equal(primary_totals, primary_pcm_before, primary_event_before,
                                  &pair->second_totals, second_pcm_before, second_event_before) != 0 ||
            (primary_result->row_count != 0u &&
             (memcmp(pens, first_pens, (size_t)primary_result->row_count * LYNX_FRAME_ROW_BYTES) != 0 ||
              memcmp(pens, second_pens, (size_t)primary_result->row_count * LYNX_FRAME_ROW_BYTES) != 0 ||
              memcmp(palettes, first_palettes, (size_t)primary_result->row_count * 32u) != 0 ||
              memcmp(palettes, second_palettes, (size_t)primary_result->row_count * 32u) != 0 ||
              memcmp(ticks, first_ticks, (size_t)primary_result->row_count * sizeof *ticks) != 0 ||
              memcmp(ticks, second_ticks, (size_t)primary_result->row_count * sizeof *ticks) != 0)) ||
            lynx_state_digest(primary, primary_digest) != LYNX_OK ||
            lynx_state_digest(pair->first.machine, first_digest) != LYNX_OK ||
            lynx_state_digest(pair->second.machine, second_digest) != LYNX_OK ||
            memcmp(primary_digest, first_digest, sizeof primary_digest) != 0 ||
            memcmp(primary_digest, second_digest, sizeof primary_digest) != 0) return 1;
    }
    return 0;
}

#define AV_REPLAY_STEP(machine, budget, pens, palettes, ticks, pcm_capacity, event_capacity, totals, result) \
    av_step_interleaved((machine), (budget), (pens), (palettes), (ticks), (pcm_capacity), \
                        (event_capacity), (totals), (result), &interleaved)
#else
#define AV_REPLAY_STEP(machine, budget, pens, palettes, ticks, pcm_capacity, event_capacity, totals, result) \
    av_step((machine), (budget), (pens), (palettes), (ticks), (pcm_capacity), \
            (event_capacity), (totals), (result))
#endif

static int av_frame_result(const lynx_av_result *source, lynx_frame_result *out)
{
    if (source->stop != LYNX_STOP_FRAME || source->row_count > LYNX_FRAME_MAX_ROWS) return 1;
    memset(out, 0, sizeof *out);
    out->struct_size = (uint32_t)sizeof *out;
    out->stop = source->stop;
    out->ticks_consumed = source->ticks_consumed;
    out->frame_start_tick = source->frame_start_tick;
    out->frame_end_tick = source->frame_end_tick;
    out->sequence = source->frame_sequence;
    out->row_count = source->row_count;
    out->flags = source->frame_flags;
    return 0;
}

static int av_make_capture(frame_capture *capture, const lynx_av_result *source,
                           const uint8_t *pens, const uint8_t *palettes,
                           const uint64_t *ticks, uint32_t input_mask,
                           const guest_probe *sample, uint32_t display_marker,
                           uint32_t handedness)
{
    uint8_t digest[32];
    if (av_frame_result(source, &capture->result) != 0 ||
        av_build_raw(pens, palettes, ticks, &capture->result,
                     &capture->raw, &capture->raw_len) != 0) return 1;
    capture->input_mask = input_mask;
    capture->sample = *sample;
    capture->display_marker = display_marker;
    capture->handedness = handedness;
    host_sha_digest(capture->raw, capture->raw_len, digest);
    host_sha_hex(digest, capture->raw_sha256);
    if (replay_render_ppm(capture, pens, palettes) != 0) return 1;
    return 0;
}

static int av_read_guest_state(lynx_machine *machine, uint32_t probe_address,
                               guest_probe *sample, uint32_t *marker)
{
    uint8_t bytes[9];
    uint8_t marker_byte = 0u;
    if (lynx_read_memory(machine, probe_address, bytes, sizeof bytes) != LYNX_OK ||
        memcmp(bytes, "LXI5", 4u) != 0 || bytes[4] != 3u || bytes[8] > 1u ||
        lynx_read_memory(machine, 0x4000u, &marker_byte, 1u) != LYNX_OK ||
        (marker_byte != 0x11u && marker_byte != 0x22u)) return 1;
    sample->joystick = bytes[5]; sample->switches = bytes[6];
    sample->progress = (uint32_t)(bytes[7] & 0x3Fu);
    sample->done = (uint32_t)((bytes[7] >> 6) & 1u);
    sample->error = (uint32_t)((bytes[7] >> 7) & 1u);
    sample->audio_started = bytes[8];
    *marker = marker_byte;
    return 0;
}

static void av_hash_u64(host_sha256 *hash, uint64_t value)
{
    uint8_t encoded[8];
    for (size_t i = 0u; i < sizeof encoded; i++) encoded[i] = (uint8_t)(value >> (8u * i));
    host_sha_update(hash, encoded, sizeof encoded);
}

static int av_append_applied_input(char *text, size_t capacity, size_t *length,
                                   size_t cursor, const input_event *event,
                                   uint32_t previous_mask)
{
    if (text == NULL || length == NULL || *length >= capacity) return 1;
    const int written = snprintf(text + *length, capacity - *length,
        "%zu %zu %" PRIu64 " %u %u\n", cursor, event->file_offset,
        event->tick, previous_mask, event->mask);
    if (written < 0 || (size_t)written >= capacity - *length) return 1;
    *length += (size_t)written;
    return 0;
}

static int av_progress_line(const frame_capture *frame, char *line, size_t capacity,
                            size_t *length)
{
    const int written = snprintf(line, capacity,
        "%" PRIu64 " %" PRIu64 " %" PRIu64 " %u %u %u %u %u %u %u %u %u\n",
        frame->result.sequence, frame->result.frame_start_tick, frame->result.frame_end_tick,
        frame->input_mask, frame->sample.joystick, frame->sample.switches,
        frame->sample.progress, frame->sample.done, frame->sample.error,
        frame->sample.audio_started,
        frame->display_marker, frame->handedness);
    if (written < 0 || (size_t)written >= capacity) return 1;
    *length = (size_t)written;
    return 0;
}

static int av_build_progress(const frame_capture frames[FRAME_OUTPUT_LIMIT],
                             size_t frame_count, char **out, size_t *out_len)
{
    static const char header[] = "LYNX-GUEST-PROGRESS 1\n";
    char *text;
    size_t capacity = sizeof header - 1u + frame_count * 192u;
    size_t length = sizeof header - 1u;
    if (frame_count > FRAME_OUTPUT_LIMIT || capacity < length) return 1;
    text = malloc(capacity == 0u ? 1u : capacity);
    if (text == NULL) return 1;
    memcpy(text, header, length);
    for (size_t i = 0u; i < frame_count; i++) {
        size_t line_len = 0u;
        if (av_progress_line(&frames[i], text + length, capacity - length, &line_len) != 0) {
            free(text); return 1;
        }
        length += line_len;
    }
    *out = text; *out_len = length;
    return 0;
}

static int av_prefix_digest(const av_capture_totals *totals,
                            const frame_capture frames[FRAME_OUTPUT_LIMIT],
                            size_t frame_count, const char *applied_text,
                            size_t applied_text_len, uint8_t digest[32])
{
    static const char event_header[] = "LYNX-AUDIO-EVENTS 1\n";
    static const char progress_header[] = "LYNX-GUEST-PROGRESS 1\n";
    host_sha256 hash;
    char line[192];
    host_sha_init(&hash);
    av_hash_u64(&hash, frame_count);
    for (size_t i = 0u; i < frame_count; i++) {
        av_hash_u64(&hash, frames[i].raw_len); host_sha_update(&hash, frames[i].raw, frames[i].raw_len);
        av_hash_u64(&hash, frames[i].ppm_len); host_sha_update(&hash, frames[i].ppm, frames[i].ppm_len);
    }
    host_sha_update(&hash, progress_header, sizeof progress_header - 1u);
    av_hash_u64(&hash, frame_count);
    for (size_t i = 0u; i < frame_count; i++) {
        size_t line_len = 0u;
        if (av_progress_line(&frames[i], line, sizeof line, &line_len) != 0) return 1;
        host_sha_update(&hash, line, line_len);
    }
    av_hash_u64(&hash, totals->pcm_count);
    for (size_t i = 0u; i < totals->pcm_count; i++) {
        uint8_t encoded[4];
        av_store_u16le(encoded, (uint16_t)totals->pcm[i].left);
        av_store_u16le(encoded + 2u, (uint16_t)totals->pcm[i].right);
        host_sha_update(&hash, encoded, sizeof encoded);
    }
    host_sha_update(&hash, event_header, sizeof event_header - 1u);
    av_hash_u64(&hash, totals->event_count);
    for (size_t i = 0u; i < totals->event_count; i++) {
        const lynx_audio_event *event = &totals->events[i];
        const char *cause = event->cause == LYNX_AUDIO_EVENT_WRITE ? "write" :
                            event->cause == LYNX_AUDIO_EVENT_CLOCK ? "clock" : "unknown";
        const int length = snprintf(line, sizeof line, "%" PRIu64 " %u %u %s %d %d\n",
            event->tick, event->sequence, event->channel, cause,
            (int)event->before_output, (int)event->after_output);
        if (length < 0 || (size_t)length >= sizeof line) return 1;
        host_sha_update(&hash, line, (size_t)length);
    }
    if (applied_text_len != 0u && applied_text == NULL) return 1;
    av_hash_u64(&hash, applied_text_len);
    if (applied_text_len != 0u) host_sha_update(&hash, applied_text, applied_text_len);
    host_sha_final(&hash, digest);
    return 0;
}

static size_t av_timeline_file_offset(const input_timeline *timeline,
                                      size_t timeline_length, size_t cursor)
{
    return cursor < timeline->count ? timeline->events[cursor].file_offset : timeline_length;
}

static int av_checkpoint_capture(av_replay_checkpoint *checkpoint,
                                 const lynx_identity *identity, uint32_t profile,
                                 const uint8_t script_digest[32], size_t script_length,
                                 const input_timeline *timeline, uint64_t guest_tick,
                                 size_t cursor, uint32_t current_mask,
                                 checkpoint_phase phase,
                                 const av_capture_totals *totals,
                                 const frame_capture frames[FRAME_OUTPUT_LIMIT],
                                 size_t frame_count, const char *applied_text,
                                 size_t applied_text_len)
{
    memset(checkpoint, 0, sizeof *checkpoint);
    checkpoint->schema = 1u; checkpoint->profile = profile;
    checkpoint->timing_policy = identity->timing_policy;
    checkpoint->audio_policy = identity->audio_policy;
    checkpoint->audio_rate_num = identity->audio_rate_num;
    checkpoint->audio_rate_den = identity->audio_rate_den;
    checkpoint->audio_channels = identity->audio_channels;
    checkpoint->audio_bits = identity->audio_bits;
    memcpy(checkpoint->content_digest, identity->content_sha256, 32u);
    memcpy(checkpoint->script_digest, script_digest, 32u);
    host_sha_digest(lynx_build_id(), strlen(lynx_build_id()), checkpoint->build_digest);
    checkpoint->guest_tick = guest_tick; checkpoint->next_sample_ordinal = totals->next_ordinal;
    checkpoint->frame_sequence = frame_count == 0u ? 0u : frames[frame_count - 1u].result.sequence;
    checkpoint->script_length = script_length;
    checkpoint->file_offset = av_timeline_file_offset(timeline, script_length, cursor);
    checkpoint->cursor = cursor; checkpoint->pcm_count = totals->pcm_count;
    checkpoint->input_applied_length = applied_text_len;
    checkpoint->input_applied_count = cursor;
    checkpoint->event_count = totals->event_count; checkpoint->frame_count = frame_count;
    checkpoint->progress_count = frame_count;
    checkpoint->current_mask = current_mask; checkpoint->phase = phase;
    return av_prefix_digest(totals, frames, frame_count, applied_text,
                            applied_text_len, checkpoint->prefix_digest);
}

static int av_checkpoint_restore(lynx_machine *source, lynx_machine *target,
                                 const av_replay_checkpoint *checkpoint,
                                 const lynx_identity *identity, uint32_t profile,
                                 const uint8_t script_digest[32], size_t script_length,
                                 const input_timeline *timeline,
                                 const av_capture_totals *totals,
                                 const frame_capture frames[FRAME_OUTPUT_LIMIT],
                                 size_t frame_count, const char *applied_text,
                                 size_t applied_text_len, uint64_t expected_tick,
                                 size_t expected_cursor, uint32_t expected_mask,
                                 checkpoint_phase expected_phase)
{
    replay_checkpoint guest_checkpoint = {{0}, 0u, 0u, 0u, CHECKPOINT_PRE_EDGE};
    uint8_t build_digest[32], prefix_digest[32];
    uint64_t guest_tick = 0u, expected_sequence = 0u;
    if (frame_count != 0u) expected_sequence = frames[frame_count - 1u].result.sequence;
    host_sha_digest(lynx_build_id(), strlen(lynx_build_id()), build_digest);
    if (av_prefix_digest(totals, frames, frame_count, applied_text,
            applied_text_len, prefix_digest) != 0 ||
        checkpoint->schema != 1u || checkpoint->profile != profile ||
        checkpoint->timing_policy != identity->timing_policy ||
        checkpoint->audio_policy != identity->audio_policy ||
        checkpoint->audio_rate_num != identity->audio_rate_num ||
        checkpoint->audio_rate_den != identity->audio_rate_den ||
        checkpoint->audio_channels != identity->audio_channels ||
        checkpoint->audio_bits != identity->audio_bits ||
        memcmp(checkpoint->content_digest, identity->content_sha256, 32u) != 0 ||
        memcmp(checkpoint->script_digest, script_digest, 32u) != 0 ||
        memcmp(checkpoint->build_digest, build_digest, 32u) != 0 ||
        checkpoint->guest_tick != expected_tick || checkpoint->script_length != script_length ||
        checkpoint->file_offset != av_timeline_file_offset(timeline, script_length, expected_cursor) ||
        checkpoint->cursor != expected_cursor || checkpoint->cursor > timeline->count ||
        checkpoint->input_applied_count != expected_cursor ||
        checkpoint->input_applied_length != applied_text_len ||
        checkpoint->current_mask != expected_mask || checkpoint->phase != expected_phase ||
        checkpoint->next_sample_ordinal != totals->next_ordinal ||
        checkpoint->pcm_count != totals->pcm_count || checkpoint->event_count != totals->event_count ||
        checkpoint->frame_count != frame_count || checkpoint->progress_count != frame_count ||
        checkpoint->frame_sequence != expected_sequence ||
        memcmp(checkpoint->prefix_digest, prefix_digest, sizeof prefix_digest) != 0 ||
        lynx_guest_ticks(source, &guest_tick) != LYNX_OK || guest_tick != expected_tick) return 1;
    guest_checkpoint.tick = checkpoint->guest_tick;
    guest_checkpoint.cursor = checkpoint->cursor;
    guest_checkpoint.current_mask = checkpoint->current_mask;
    guest_checkpoint.phase = checkpoint->phase;
    memcpy(guest_checkpoint.script_digest, checkpoint->script_digest, 32u);
    if (replay_checkpoint_valid(&guest_checkpoint, timeline, script_digest,
            expected_tick, expected_cursor, expected_mask, expected_phase) != 0) return 1;
    return replay_restore_checkpoint(source, target, &guest_checkpoint, timeline, script_digest);
}

static int av_join_frames(const frame_capture frames[FRAME_OUTPUT_LIMIT], size_t frame_count,
                          int rendered, uint8_t **out, size_t *out_len)
{
    size_t length = 0u, offset = 0u;
    uint8_t *joined;
    for (size_t i = 0u; i < frame_count; i++) {
        const size_t part = rendered != 0 ? frames[i].ppm_len : frames[i].raw_len;
        if (part > SIZE_MAX - length) return 1;
        length += part;
    }
    joined = malloc(length == 0u ? 1u : length);
    if (joined == NULL) return 1;
    for (size_t i = 0u; i < frame_count; i++) {
        const uint8_t *part = rendered != 0 ? frames[i].ppm : frames[i].raw;
        const size_t part_len = rendered != 0 ? frames[i].ppm_len : frames[i].raw_len;
        memcpy(joined + offset, part, part_len); offset += part_len;
    }
    *out = joined; *out_len = length;
    return 0;
}

static int run_av_consumer(const char *content_path, const char *symbols_path,
                           const char *timeline_path, const char *output_directory,
                           uint32_t profile)
{
    uint8_t *bll = NULL, *labels = NULL, *timeline_bytes = NULL, script_digest[32];
    size_t bll_len = 0u, labels_len = 0u, timeline_len = 0u, cursor = 0u, frame_count = 0u;
    input_timeline timeline = {0};
    machine_storage active = {0}, restore_target = {0};
    lynx_identity identity, restore_identity;
    uint32_t probe_address = 0u, current_mask = 0u;
    uint64_t last_tick = 0u, endpoint = 0u, now = 0u, previous_sequence = UINT64_MAX;
    uint64_t previous_frame_end = 0u, first_a_tick = UINT64_MAX, pending_edge_tick = UINT64_MAX;
    uint32_t pending_prior_mask = 0u;
    char normalized_output[4096];
    int target_created = 0, short_spans = 0, watch_expected = 0, watch_resume = 0, bad_checkpoint = 0;
    int uninterrupted_control = 0;
#if defined(LYNX_CONSUMER_TESTING)
    int interleaved_test = 0;
    int final_checkpoint_only = 0;
    av_interleaved_pair interleaved = {0};
#endif
    av_capture_totals totals = {0};
    frame_capture frames[FRAME_OUTPUT_LIMIT] = {0};
    frame_capture *released_capture = NULL, *pressed_capture = NULL;
    uint8_t *pcm_bytes = NULL, *wav = NULL, *raw_stream = NULL, *ppm_stream = NULL;
    char *event_text = NULL, *applied_text = NULL, *progress_text = NULL;
    size_t pcm_bytes_len = 0u, wav_len = 0u, event_text_len = 0u;
    size_t raw_stream_len = 0u, ppm_stream_len = 0u;
    size_t applied_text_len = 0u, progress_text_len = 0u;
    char content_hash[65], input_hash[65], manifest[8192];
    size_t manifest_len = 0u;
    int result = 1;
#if defined(LYNX_CONSUMER_TESTING)
    {
        const char *value = getenv("LYNX_TEST_AV_SHORT_SPANS");
        short_spans = value != NULL && strcmp(value, "1") == 0;
        value = getenv("LYNX_TEST_AV_WATCH");
        watch_expected = value != NULL && strcmp(value, "1") == 0;
        value = getenv("LYNX_TEST_AV_WATCH_RESUME");
        watch_resume = value != NULL && strcmp(value, "1") == 0;
        if (watch_resume != 0) watch_expected = 1;
        value = getenv("LYNX_TEST_AV_BAD_CHECKPOINT");
        bad_checkpoint = value != NULL && value[0] != '\0';
        value = getenv("LYNX_TEST_AV_UNINTERRUPTED");
        uninterrupted_control = value != NULL && strcmp(value, "1") == 0;
        value = getenv("LYNX_TEST_AV_INTERLEAVED");
        interleaved_test = value != NULL && strcmp(value, "1") == 0;
        value = getenv("LYNX_TEST_AV_FINAL_CHECKPOINT_ONLY");
        final_checkpoint_only = value != NULL && strcmp(value, "1") == 0;
    }
#endif
    totals.pcm = calloc(AV_PCM_CAPACITY, sizeof *totals.pcm);
    totals.events = calloc(AV_EVENT_CAPACITY, sizeof *totals.events);
    applied_text = malloc(AV_INPUT_APPLIED_CAPACITY);
    if (totals.pcm == NULL || totals.events == NULL || applied_text == NULL) {
        (void)report_error("cannot allocate bounded A/V capture spans"); goto done;
    }
    if (input_paths_are_distinct(content_path, symbols_path, timeline_path,
            "A/V input paths must refer to distinct files") != 0) goto done;
    if (read_bounded(content_path, BLL_LIMIT, &bll, &bll_len, "BLL content") != 0 || bll_len < 11u ||
        read_bounded(symbols_path, SYMBOL_LIMIT, &labels, &labels_len, "ld65 -Ln symbols") != 0 ||
        parse_named_symbol(labels, labels_len, "probe_result", &probe_address) != 0 ||
        probe_address > LYNX_ADDRESS_SPACE - 9u) {
        (void)report_error("A/V fixture or linked probe symbol is invalid"); goto done;
    }
    if (read_bounded(timeline_path, TIMELINE_LIMIT, &timeline_bytes, &timeline_len,
            "input timeline") != 0) goto done;
    if (parse_input_timeline(timeline_bytes, timeline_len, &timeline) != 0) {
        (void)report_error("A/V timeline is not canonical LYNX-INPUT 1 ASCII; use strictly increasing decimal tick/mask LF rows within the documented bounds"); goto done;
    }
    {
        static const char applied_header[] = "LYNX-APPLIED-INPUT 1\n";
        memcpy(applied_text, applied_header, sizeof applied_header - 1u);
        applied_text_len = sizeof applied_header - 1u;
    }
    uint32_t scan_mask = 0u;
    for (size_t i = 0u; i < timeline.count; i++) {
        if (timeline.events[i].tick > last_tick) last_tick = timeline.events[i].tick;
        if (first_a_tick == UINT64_MAX && (scan_mask & LYNX_INPUT_A) == 0u &&
            (timeline.events[i].mask & LYNX_INPUT_A) != 0u)
            first_a_tick = timeline.events[i].tick;
        scan_mask = timeline.events[i].mask;
    }
    if (first_a_tick == UINT64_MAX || last_tick > UINT64_MAX - UINT64_C(60000) ||
        normalize_destination(output_directory, normalized_output, sizeof normalized_output) != 0 ||
        destination_is_absent(normalized_output) != 0) {
        (void)report_error("A/V replay needs a bounded timeline containing an A press"); goto done;
    }
    endpoint = last_tick + UINT64_C(60000);
    if (endpoint / UINT64_C(400) >= AV_PCM_CAPACITY) {
        (void)report_error("A/V timeline exceeds the bounded output window or destination path limit"); goto done;
    }
    host_sha_digest(timeline_bytes, timeline_len, script_digest);
    host_sha_hex(script_digest, input_hash);
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &active, &identity) != 0) goto done;
    if (create_machine(bll, bll_len, profile, LYNX_TIMING_POLICY_VIDEO_POLICY1,
                       &restore_target, &restore_identity) != 0) {
        destroy_machine(&restore_target); goto done;
    }
    target_created = 1;
    if (identity.audio_policy != LYNX_AUDIO_POLICY_DIGITAL_1 ||
        identity.audio_rate_num != 40000u || identity.audio_rate_den != 1u ||
        identity.audio_channels != 2u || identity.audio_bits != 16u ||
        identity.profile != restore_identity.profile ||
        identity.timing_policy != restore_identity.timing_policy ||
        identity.audio_policy != restore_identity.audio_policy ||
        identity.audio_rate_num != restore_identity.audio_rate_num ||
        identity.audio_rate_den != restore_identity.audio_rate_den ||
        identity.audio_channels != restore_identity.audio_channels ||
        identity.audio_bits != restore_identity.audio_bits ||
        memcmp(identity.content_sha256, restore_identity.content_sha256, 32u) != 0) {
        (void)report_error("public machine identity does not match the fixed digital stereo PCM contract"); goto done;
    }
#if defined(LYNX_CONSUMER_TESTING)
    if (interleaved_test != 0 && av_interleaved_init(&interleaved, bll, bll_len, profile, &identity) != 0) {
        (void)report_error("could not initialize two fresh A/V instances for interleaved conformance"); goto done;
    }
#endif
    host_sha_hex(identity.content_sha256, content_hash);
    if (lynx_advance_av(active.machine, 60000u, NULL, NULL) != LYNX_ERR_ARGUMENT) {
        (void)report_error("invalid A/V descriptors were not rejected before progress"); goto done;
    }
    if (watch_expected != 0 && lynx_watch_configure(active.machine, 0x4000u, 1u) != LYNX_OK) {
        (void)report_error("test watch could not be armed for the released guest marker"); goto done;
    }

    while (cursor < timeline.count) {
        const input_event *event = &timeline.events[cursor];
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || event->tick < now) {
            (void)report_error("input edge precedes current guest time"); goto done;
        }
        while (now < event->tick) {
            uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
            uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], after = 0u;
            lynx_av_result step = {0};
            const uint64_t request = event->tick - now < UINT64_C(60000) ?
                event->tick - now : UINT64_C(60000);
            const size_t pcm_capacity = short_spans != 0 ? LYNX_AV_MIN_PCM_CAPACITY : AV_PCM_CAPACITY;
            const size_t event_capacity = short_spans != 0 ? LYNX_AV_MIN_EVENT_CAPACITY : AV_EVENT_CAPACITY;
            const int step_status = AV_REPLAY_STEP(active.machine, request, pens, palettes, ticks,
                        pcm_capacity, event_capacity, &totals, &step);
            if (step_status != 0) {
#if defined(LYNX_CONSUMER_TESTING)
                (void)fprintf(stderr,
                    "A/V test step failure: cursor=%zu now=%llu edge=%llu request=%llu stop=%d consumed=%llu pcm=%u/%zu events=%u/%zu totals=%zu/%zu\n",
                    cursor, (unsigned long long)now, (unsigned long long)event->tick,
                    (unsigned long long)request, (int)step.stop,
                    (unsigned long long)step.ticks_consumed, (unsigned)step.pcm_count, pcm_capacity,
                    (unsigned)step.event_count, event_capacity, totals.pcm_count, totals.event_count);
#endif
                (void)report_error("A/V advance violated its bounded input-edge interval"); goto done;
            }
            if (lynx_guest_ticks(active.machine, &after) != LYNX_OK || after < now || after > event->tick) {
#if defined(LYNX_CONSUMER_TESTING)
                (void)fprintf(stderr,
                    "A/V test guest-progress failure: cursor=%zu now=%llu edge=%llu after=%llu request=%llu stop=%d consumed=%llu pcm=%u events=%u\n",
                    cursor, (unsigned long long)now, (unsigned long long)event->tick,
                    (unsigned long long)after, (unsigned long long)request, (int)step.stop,
                    (unsigned long long)step.ticks_consumed, (unsigned)step.pcm_count, (unsigned)step.event_count);
#endif
                (void)report_error("A/V advance violated its bounded input-edge interval"); goto done;
            }
            if (step.stop == LYNX_STOP_FRAME) {
                if ((previous_sequence != UINT64_MAX && step.frame_sequence <= previous_sequence) ||
                    step.frame_end_tick != after ||
                    (previous_frame_end != 0u && step.frame_end_tick <= previous_frame_end)) {
                    (void)report_error("A/V frame sequence or guest tick is not monotone"); goto done;
                }
                previous_sequence = step.frame_sequence; previous_frame_end = step.frame_end_tick;
                if (step.row_count == 0u) {
                    if (step.frame_sequence != 0u) { (void)report_error("only sequence zero may synchronize without rows"); goto done; }
                } else {
                    guest_probe sample = {0}; uint32_t marker = 0u, frame_mask = current_mask;
                    if (frame_count >= FRAME_OUTPUT_LIMIT || step.row_count != LYNX_FRAME_MAX_ROWS) {
                        (void)report_error("A/V replay exceeded its frame or row bound"); goto done;
                    }
                    if (step.ticks_consumed == 0u && pending_edge_tick == step.frame_end_tick) {
                        frame_mask = pending_prior_mask; pending_edge_tick = UINT64_MAX;
                    } else if (pending_edge_tick != UINT64_MAX && step.frame_end_tick > pending_edge_tick) {
                        pending_edge_tick = UINT64_MAX;
                    }
                    if (av_read_guest_state(active.machine, probe_address, &sample, &marker) != 0 ||
                        sample.progress == 0u || sample.error != 0u ||
                        (marker == 0x22u && (sample.joystick & 1u) == 0u) ||
                        (marker == 0x11u && (sample.joystick & 1u) != 0u) ||
                        av_make_capture(&frames[frame_count], &step, pens, palettes, ticks,
                                        frame_mask, &sample, marker, 1u) != 0) {
                        (void)report_error("A/V frame guest marker, progress or raw/rendered capture is invalid"); goto done;
                    }
                    if (marker == 0x11u && released_capture == NULL) released_capture = &frames[frame_count];
                    if (marker == 0x22u && step.frame_end_tick > first_a_tick && pressed_capture == NULL)
                        pressed_capture = &frames[frame_count];
                    frame_count++;
                }
            } else if (step.stop == LYNX_STOP_BUDGET) {
                if (step.ticks_consumed == 0u || step.ticks_consumed != request) {
                    (void)report_error("guest-time limit stopped before the requested input edge"); goto done;
                }
            } else if (step.stop == LYNX_STOP_OUTPUT) {
                if (after == now && step.pcm_count == 0u && step.event_count == 0u) {
                    (void)report_error("A/V output frontier made no progress with fresh caller spans"); goto done;
                }
            } else if (step.stop == LYNX_STOP_WATCH) {
                lynx_watch_event hit = {0}; uint32_t marker = 0u; guest_probe sample = {0};
                if (watch_expected == 0 || lynx_watch_get(active.machine, &hit) != LYNX_OK ||
                    hit.hit != 1u || hit.address != 0x4000u || after >= event->tick ||
                    av_read_guest_state(active.machine, probe_address, &sample, &marker) != 0 || marker != 0x11u) {
                    (void)report_error("WATCH stop did not preserve released input before the next edge"); goto done;
                }
                if (watch_resume == 0) {
                    (void)report_error("WATCH stopped before the next input edge; released guest marker is preserved");
                    goto done;
                }
                if (lynx_watch_clear(active.machine) != LYNX_OK) {
                    (void)report_error("public watch clear failed after a validated A/V watch stop"); goto done;
                }
                watch_expected = 0;
                watch_resume = 0;
            } else {
                (void)report_error("undefined, unavailable or unsupported guest stop before input edge"); goto done;
            }
            if (step.stop != LYNX_STOP_FRAME && after == now && step.stop != LYNX_STOP_OUTPUT) {
                (void)report_error("A/V replay made no guest-time progress before an input edge"); goto done;
            }
            now = after;
        }
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || now != event->tick) {
            (void)report_error("A/V replay did not reach the exact declared input tick"); goto done;
        }
        const size_t event_index = cursor;
        for (int phase_index = 0; phase_index < 2; phase_index++) {
            const checkpoint_phase phase = phase_index == 0 ? CHECKPOINT_PRE_EDGE : CHECKPOINT_POST_EDGE;
            const size_t checkpoint_cursor = event_index + (phase_index == 0 ? 0u : 1u);
            const uint32_t checkpoint_mask = phase_index == 0 ? current_mask : event->mask;
            int checkpoint_required = 1;
#if defined(LYNX_CONSUMER_TESTING)
            if (final_checkpoint_only != 0 && event_index + 1u < timeline.count)
                checkpoint_required = 0;
#endif
            if (checkpoint_required != 0) {
                av_replay_checkpoint checkpoint;
                if (av_checkpoint_capture(&checkpoint, &identity, profile, script_digest, timeline_len,
                        &timeline, now, checkpoint_cursor, checkpoint_mask, phase, &totals,
                        frames, frame_count, applied_text, applied_text_len) != 0) {
                    (void)report_error("cannot bind A/V host checkpoint to its complete output prefix"); goto done;
                }
#if defined(LYNX_CONSUMER_TESTING)
                if (bad_checkpoint != 0 && phase_index == 0) {
                    const char *kind = getenv("LYNX_TEST_AV_BAD_CHECKPOINT");
                    if (strcmp(kind, "schema") == 0) checkpoint.schema++;
                    else if (strcmp(kind, "timeline") == 0) checkpoint.script_digest[0] ^= 1u;
                    else if (strcmp(kind, "file-offset") == 0) checkpoint.file_offset++;
                    else if (strcmp(kind, "sample-ordinal") == 0) checkpoint.next_sample_ordinal++;
                    else if (strcmp(kind, "prefix") == 0) checkpoint.prefix_digest[0] ^= 1u;
                    else checkpoint.schema = 0u;
                }
#endif
                if (uninterrupted_control == 0) {
                    const size_t pcm_before = totals.pcm_count, events_before = totals.event_count;
                    const size_t frames_before = frame_count;
                    if (av_checkpoint_restore(active.machine, restore_target.machine, &checkpoint,
                            &identity, profile, script_digest, timeline_len, &timeline, &totals,
                            frames, frame_count, applied_text, applied_text_len, now,
                            checkpoint_cursor, checkpoint_mask, phase) != 0) {
                        uint64_t unchanged_tick = UINT64_MAX;
                        if (lynx_guest_ticks(active.machine, &unchanged_tick) != LYNX_OK || unchanged_tick != now ||
                            totals.pcm_count != pcm_before || totals.event_count != events_before ||
                            frame_count != frames_before) {
                            (void)report_error("invalid A/V host checkpoint changed guest time or a published output prefix"); goto done;
                        }
                        (void)report_error(bad_checkpoint != 0 ?
                            "A/V host checkpoint refused before restore or output append" :
                            "A/V host checkpoint identity, extent, cursor or prefix validation failed"); goto done;
                    }
                    { machine_storage swap = active; active = restore_target; restore_target = swap; }
                }
            }
            if (phase_index == 0) {
                if (lynx_set_input(active.machine, event->mask) != LYNX_OK) {
                    (void)report_error("public input setter refused a validated event at its exact guest tick"); goto done;
                }
#if defined(LYNX_CONSUMER_TESTING)
                if (av_interleaved_set_input(&interleaved, event->mask) != 0) {
                    (void)report_error("interleaved fresh A/V instances refused the same validated input at its guest tick"); goto done;
                }
#endif
                if (av_append_applied_input(applied_text, AV_INPUT_APPLIED_CAPACITY,
                        &applied_text_len, event_index, event, current_mask) != 0) {
                    (void)report_error("bounded applied-input transcript could not record a successful edge"); goto done;
                }
                pending_edge_tick = event->tick; pending_prior_mask = current_mask;
                current_mask = event->mask; cursor++;
            }
        }
    }

    while (frame_count == 0u || frames[frame_count - 1u].result.frame_end_tick <= last_tick) {
        uint8_t pens[LYNX_FRAME_PENS_CAPACITY], palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint64_t ticks[LYNX_FRAME_LINE_TICKS_CAPACITY], after = 0u;
        lynx_av_result step = {0};
        uint64_t remaining;
        const size_t pcm_capacity = short_spans != 0 ? LYNX_AV_MIN_PCM_CAPACITY : AV_PCM_CAPACITY;
        const size_t event_capacity = short_spans != 0 ? LYNX_AV_MIN_EVENT_CAPACITY : AV_EVENT_CAPACITY;
        if (lynx_guest_ticks(active.machine, &now) != LYNX_OK || now >= endpoint) {
            (void)report_error("no complete guest frame followed the final input edge within its bound"); goto done;
        }
        remaining = endpoint - now;
        if (AV_REPLAY_STEP(active.machine, remaining, pens, palettes, ticks, pcm_capacity,
                    event_capacity, &totals, &step) != 0 ||
            lynx_guest_ticks(active.machine, &after) != LYNX_OK || after < now || after > endpoint) {
            (void)report_error("final A/V frame interval violated its guest-time bound"); goto done;
        }
        if (step.stop == LYNX_STOP_FRAME) {
            if ((previous_sequence != UINT64_MAX && step.frame_sequence <= previous_sequence) ||
                step.frame_end_tick != after ||
                (previous_frame_end != 0u && step.frame_end_tick <= previous_frame_end)) {
                (void)report_error("final A/V frame sequence or tick is not monotone"); goto done;
            }
            previous_sequence = step.frame_sequence; previous_frame_end = step.frame_end_tick;
            if (step.row_count == 0u) {
                if (step.frame_sequence != 0u) { (void)report_error("only sequence zero may synchronize without rows"); goto done; }
            } else {
                guest_probe sample = {0}; uint32_t marker = 0u, frame_mask = current_mask;
                if (frame_count >= FRAME_OUTPUT_LIMIT || step.row_count != LYNX_FRAME_MAX_ROWS) {
                    (void)report_error("final A/V frame exceeded its bound"); goto done;
                }
                if (step.ticks_consumed == 0u && pending_edge_tick == step.frame_end_tick) {
                    frame_mask = pending_prior_mask; pending_edge_tick = UINT64_MAX;
                } else if (pending_edge_tick != UINT64_MAX && step.frame_end_tick > pending_edge_tick) {
                    pending_edge_tick = UINT64_MAX;
                }
                if (av_read_guest_state(active.machine, probe_address, &sample, &marker) != 0 ||
                    sample.progress == 0u || sample.error != 0u ||
                    (marker == 0x22u && (sample.joystick & 1u) == 0u) ||
                    (marker == 0x11u && (sample.joystick & 1u) != 0u) ||
                    av_make_capture(&frames[frame_count], &step, pens, palettes, ticks,
                                    frame_mask, &sample, marker, 1u) != 0) {
                    (void)report_error("final A/V guest marker, progress or raw/rendered capture is invalid"); goto done;
                }
                if (marker == 0x11u && released_capture == NULL) released_capture = &frames[frame_count];
                if (marker == 0x22u && step.frame_end_tick > first_a_tick && pressed_capture == NULL)
                    pressed_capture = &frames[frame_count];
                frame_count++;
            }
        } else if (step.stop == LYNX_STOP_BUDGET) {
            if (step.ticks_consumed == 0u || step.ticks_consumed != remaining) {
                (void)report_error("guest-time limit stopped final A/V observation before the endpoint"); goto done;
            }
        } else if (step.stop == LYNX_STOP_OUTPUT) {
            if (after == now && step.pcm_count == 0u && step.event_count == 0u) {
                (void)report_error("final A/V output frontier made no progress with fresh spans"); goto done;
            }
        } else {
            (void)report_error("unsupported guest stop while completing the final A/V frame"); goto done;
        }
    }
    if (released_capture == NULL || pressed_capture == NULL) {
        (void)report_error("A/V timeline did not capture both released and pressed guest frame markers"); goto done;
    }
    if (released_capture->raw_len != pressed_capture->raw_len ||
        memcmp(released_capture->raw, pressed_capture->raw, released_capture->raw_len) == 0) {
        (void)report_error("A/V timeline did not produce distinct released and pressed raw video evidence"); goto done;
    }
    {
        bool nonzero_after_edge = false;
        for (size_t i = 0u; i < totals.pcm_count; i++) {
            const uint64_t ordinal = totals.next_ordinal - totals.pcm_count + i;
            if ((ordinal + 1u) * UINT64_C(400) >= first_a_tick &&
                (totals.pcm[i].left != 0 || totals.pcm[i].right != 0)) nonzero_after_edge = true;
        }
        if (!nonzero_after_edge || totals.pcm_count == 0u || totals.event_count == 0u) {
            (void)report_error("recorded A input did not produce non-silent PCM and channel events"); goto done;
        }
    }
    if (av_build_pcm_bytes(totals.pcm, totals.pcm_count, &pcm_bytes, &pcm_bytes_len) != 0 ||
        av_build_wav(pcm_bytes, pcm_bytes_len, &wav, &wav_len) != 0 ||
        av_build_event_text(totals.events, totals.event_count, &event_text, &event_text_len) != 0 ||
        av_build_progress(frames, frame_count, &progress_text, &progress_text_len) != 0 ||
        av_join_frames(frames, frame_count, 0, &raw_stream, &raw_stream_len) != 0 ||
        av_join_frames(frames, frame_count, 1, &ppm_stream, &ppm_stream_len) != 0) {
        (void)report_error("could not encode bounded complete A/V stream prefixes"); goto done;
    }
    {
        uint8_t digest[32];
        char released_raw_hash[65], released_ppm_hash[65], pressed_raw_hash[65], pressed_ppm_hash[65];
        char pcm_hash[65], wav_hash[65], event_hash[65], raw_stream_hash[65], ppm_stream_hash[65];
        char applied_hash[65], progress_hash[65];
        char timeline_stream_hash[65];
        int n;
        host_sha_digest(released_capture->raw, released_capture->raw_len, digest); host_sha_hex(digest, released_raw_hash);
        host_sha_digest(released_capture->ppm, released_capture->ppm_len, digest); host_sha_hex(digest, released_ppm_hash);
        host_sha_digest(pressed_capture->raw, pressed_capture->raw_len, digest); host_sha_hex(digest, pressed_raw_hash);
        host_sha_digest(pressed_capture->ppm, pressed_capture->ppm_len, digest); host_sha_hex(digest, pressed_ppm_hash);
        host_sha_digest(pcm_bytes, pcm_bytes_len, digest); host_sha_hex(digest, pcm_hash);
        host_sha_digest(wav, wav_len, digest); host_sha_hex(digest, wav_hash);
        host_sha_digest(event_text, event_text_len, digest); host_sha_hex(digest, event_hash);
        host_sha_digest(raw_stream, raw_stream_len, digest); host_sha_hex(digest, raw_stream_hash);
        host_sha_digest(ppm_stream, ppm_stream_len, digest); host_sha_hex(digest, ppm_stream_hash);
        host_sha_digest(applied_text, applied_text_len, digest); host_sha_hex(digest, applied_hash);
        host_sha_digest(progress_text, progress_text_len, digest); host_sha_hex(digest, progress_hash);
        host_sha_digest(timeline_bytes, timeline_len, digest); host_sha_hex(digest, timeline_stream_hash);
        n = snprintf(manifest, sizeof manifest,
            "schema_version = 1\nartifact = AV_TRACE\nprofile = %s\nstartup = A\ntiming_policy = video-policy1-unverified\n"
            "audio_policy = digital-1\naudio_rate_num = 40000\naudio_rate_den = 1\naudio_channels = 2\naudio_bits = 16\n"
            "build_id = %s\ncontent_sha256 = %s\ninput_sha256 = %s\ninput_bytes = %zu\ninput_event_count = %zu\n"
            "input_event_tick = %" PRIu64 "\ninput_event_mask = %u\nlast_input_tick = %" PRIu64 "\n"
            "input_applied_count = %zu\ninput_applied_bytes = %zu\ninput_applied_sha256 = %s\n"
            "frame_count = %zu\nframes_raw_bytes = %zu\nframes_raw_sha256 = %s\nframes_ppm_bytes = %zu\nframes_ppm_sha256 = %s\n"
            "guest_progress_count = %zu\nguest_progress_bytes = %zu\nguest_progress_sha256 = %s\n"
            "released_sequence = %" PRIu64 "\nreleased_start_tick = %" PRIu64 "\nreleased_end_tick = %" PRIu64 "\nreleased_rows = %u\nreleased_raw_bytes = %zu\nreleased_raw_sha256 = %s\nreleased_ppm_bytes = %zu\nreleased_ppm_sha256 = %s\n"
            "pressed_sequence = %" PRIu64 "\npressed_start_tick = %" PRIu64 "\npressed_end_tick = %" PRIu64 "\npressed_rows = %u\npressed_raw_bytes = %zu\npressed_raw_sha256 = %s\npressed_ppm_bytes = %zu\npressed_ppm_sha256 = %s\n"
            "first_sample_ordinal = 0\nnext_sample_ordinal = %" PRIu64 "\npcm_frame_count = %zu\npcm_bytes = %zu\npcm_sha256 = %s\nwav_bytes = %zu\nwav_sha256 = %s\nevent_count = %zu\nevents_bytes = %zu\nevents_sha256 = %s\n"
            "timeline_sha256 = %s\ntimeline_stream_sha256 = %s\n"
            "source_policy = digital channel-0 divider/LFSR; mix and stereo route are software policy, not physical qualification\n",
            lynx_profile_name(profile), lynx_build_id(), content_hash, input_hash, timeline_len, timeline.count,
            timeline.count == 0u ? 0u : timeline.events[0].tick,
            timeline.count == 0u ? 0u : timeline.events[0].mask, last_tick,
            timeline.count, applied_text_len, applied_hash, frame_count,
            raw_stream_len, raw_stream_hash, ppm_stream_len, ppm_stream_hash,
            frame_count, progress_text_len, progress_hash,
            released_capture->result.sequence, released_capture->result.frame_start_tick,
            released_capture->result.frame_end_tick, released_capture->result.row_count,
            released_capture->raw_len, released_raw_hash, released_capture->ppm_len, released_ppm_hash,
            pressed_capture->result.sequence,
            pressed_capture->result.frame_start_tick, pressed_capture->result.frame_end_tick,
            pressed_capture->result.row_count, pressed_capture->raw_len, pressed_raw_hash,
            pressed_capture->ppm_len, pressed_ppm_hash, totals.next_ordinal, totals.pcm_count,
            pcm_bytes_len, pcm_hash, wav_len, wav_hash, totals.event_count, event_text_len,
            event_hash, timeline_stream_hash, timeline_stream_hash);
        if (n < 0 || (size_t)n >= sizeof manifest) { (void)report_error("A/V manifest exceeds its fixed bound"); goto done; }
        manifest_len = (size_t)n;
    }
    {
        const av_output_file files[] = {
            {"released.raw", released_capture->raw, released_capture->raw_len},
            {"released.ppm", released_capture->ppm, released_capture->ppm_len},
            {"pressed.raw", pressed_capture->raw, pressed_capture->raw_len},
            {"pressed.ppm", pressed_capture->ppm, pressed_capture->ppm_len},
            {"frames.raw", raw_stream, raw_stream_len}, {"frames.ppm", ppm_stream, ppm_stream_len},
            {"input.timeline", timeline_bytes, timeline_len},
            {"input.applied", applied_text, applied_text_len},
            {"guest.progress", progress_text, progress_text_len},
            {"audio.pcm", pcm_bytes, pcm_bytes_len}, {"audio.wav", wav, wav_len},
            {"audio.events", event_text, event_text_len}, {"av.manifest", manifest, manifest_len}
        };
        if (publish_av_outputs(output_directory, files, sizeof files / sizeof files[0]) != 0) goto done;
    }
    printf("lynx-consume: replayed %zu input edges and published %zu complete frame/progress records with applied input, PCM and event streams to %s\n",
           timeline.count, frame_count, output_directory);
#if defined(LYNX_CONSUMER_TESTING)
    if (interleaved_test != 0)
        puts("lynx-consume-test: two fresh A/V instances matched every interleaved frame, PCM, event and state boundary");
#endif
    result = 0;
done:
    free(bll); free(labels); free(timeline_bytes); free(timeline.events);
    free(totals.pcm); free(totals.events); free(event_text); free(applied_text);
    free(progress_text); free(pcm_bytes); free(wav);
    free(raw_stream); free(ppm_stream);
    for (size_t i = 0u; i < FRAME_OUTPUT_LIMIT; i++) { free(frames[i].raw); free(frames[i].ppm); }
    destroy_machine(&active);
    if (target_created != 0) destroy_machine(&restore_target);
#if defined(LYNX_CONSUMER_TESTING)
    av_interleaved_destroy(&interleaved);
#endif
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
            else return usage_error("unknown replay-mode option");
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
        if (frame_mode == NULL || (strcmp(frame_mode, "frames") != 0 && strcmp(frame_mode, "av-replay") != 0) || frame_content == NULL || frame_symbols == NULL ||
            frame_timeline == NULL || frame_output == NULL || parse_profile(frame_profile_text, &selected_profile) != 0)
            return usage_error("replay mode requires --mode frames or av-replay, BLL, linked symbols, a canonical timeline, output directory, and a supported profile");
        if (strcmp(frame_mode, "av-replay") == 0)
            return run_av_consumer(frame_content, frame_symbols, frame_timeline, frame_output, selected_profile);
#if defined(LYNX_CONSUMER_TESTING)
        {
            const char *legacy = getenv("LYNX_TEST_REPLAY_LEGACY");
            if (legacy != NULL && strcmp(legacy, "1") == 0)
                return run_frame_consumer_legacy(frame_content, frame_symbols, frame_timeline, frame_output, selected_profile);
        }
#endif
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
