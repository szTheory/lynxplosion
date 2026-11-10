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
    WAV_BYTES = 44 + WAV_DATA_BYTES
};

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
                    "Usage: lynx-consume --content BLL --symbols LABELS --out NEW-DIRECTORY "
                    "[--profile none|lynx1|lynx2]\n"
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

static int create_machine(const uint8_t *bll, size_t bll_len, uint32_t profile,
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
        identity->ram_policy != LYNX_RAM_POLICY_ZERO || identity->object_len != bll_len ||
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
    write_result = write_exclusive(manifest_path, manifest, manifest_len);
    if (write_result != 0) {
        (void)report_error("cannot write snapshot.manifest exclusively; check output permissions and use a new directory");
        goto failed;
    }
    made_manifest = 1;
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
    if (create_machine(bll, bll_len, profile, &storage, &identity) != 0) {
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

int main(int argc, char **argv)
{
    const char *content = NULL;
    const char *label_file = NULL;
    const char *output = NULL;
    const char *profile_text = "none";
    uint32_t profile = LYNX_PROFILE_NONE;
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
