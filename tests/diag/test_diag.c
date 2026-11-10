/* Public-API checks for the original source-built Phase 3 diagnostic.
   This program deliberately knows nothing about the private machine layout.
   The isolated SHA-256 code below is used only to rehash edited snapshot bytes
   in negative controls; all machine access goes through lynxplosion.h. */
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lynxplosion.h"

enum {
    BLL_HEADER_SIZE = 10,
    RESULT_SIZE = 8,
    PIXELS_SIZE = 32,
    SYMBOL_LINE_MAX = 255,
    DIAG_MIN_TICKS = 3000,
    DIAG_MAX_TICKS = 4000,
    FINAL_TICK = 5000,
    SNAP_HEADER_SIZE = 252,
    SNAP_PAYLOAD_SIZE = 65568,
    SNAP_PAYLOAD_HASH_OFFSET = 220,
    SNAP_RAM_OFFSET = 32,
    SNAP_RESERVED_OFFSET = 15
};

typedef struct diag_symbols {
    uint32_t result;
    uint32_t pixels;
    uint32_t pixels_end;
} diag_symbols;

typedef struct machine_instance {
    void *storage;
    lynx_machine *machine;
} machine_instance;

static int fail(const char *reason)
{
    fprintf(stderr, "diag: %s\n", reason);
    return 1;
}

static int read_file(const char *path, uint8_t **out, size_t *length, size_t limit)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    size_t got;
    int extra;
    int read_failed;
    int close_failed;
    if (file == NULL) {
        return fail("cannot open input file");
    }
    data = malloc(limit == 0u ? 1u : limit);
    if (data == NULL) {
        (void)fclose(file);
        return fail("cannot allocate input buffer");
    }
    got = fread(data, 1u, limit, file);
    extra = fgetc(file);
    read_failed = ferror(file) != 0;
    close_failed = fclose(file) != 0;
    if (read_failed != 0 || extra != EOF || close_failed != 0) {
        free(data);
        return fail("input file exceeds its bound or could not be read");
    }
    *out = data;
    *length = got;
    return 0;
}

static int read_text(const char *path, char **out, size_t *length)
{
    uint8_t *bytes = NULL;
    size_t count = 0u;
    if (read_file(path, &bytes, &count, 16384u) != 0) {
        return 1;
    }
    if (memchr(bytes, '\0', count) != NULL) {
        free(bytes);
        return fail("symbol file contains NUL");
    }
    *out = (char *)bytes;
    *length = count;
    return 0;
}

static int parse_hex_address(const char *text, size_t length, uint32_t *value)
{
    size_t index;
    uint32_t result = 0u;
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
        result = (result << 4) | (uint32_t)digit;
    }
    if (result > LYNX_ADDRESS_SPACE) {
        return 1;
    }
    *value = result;
    return 0;
}

static int is_target_name(const char *begin, size_t length, size_t *target)
{
    static const char *const names[] = {"diag_result", "diag_pixels", "diag_pixels_end"};
    size_t index;
    if (length > 0u && begin[0] == '.') {
        begin++;
        length--;
    }
    for (index = 0u; index < sizeof names / sizeof names[0]; index++) {
        if (strlen(names[index]) == length && memcmp(begin, names[index], length) == 0) {
            *target = index;
            return 1;
        }
    }
    return 0;
}

static int parse_symbols(const char *text, size_t length, diag_symbols *out,
                         char *reason, size_t reason_cap)
{
    size_t positions[3] = {0u, 0u, 0u};
    unsigned found[3] = {0u, 0u, 0u};
    size_t offset = 0u;
    while (offset < length) {
        char line[SYMBOL_LINE_MAX + 1];
        size_t line_len = 0u;
        size_t token_begin[4] = {0u, 0u, 0u, 0u};
        size_t token_len[4] = {0u, 0u, 0u, 0u};
        size_t token_count = 0u;
        size_t cursor;
        size_t target = 0u;
        int relevant = 0;
        uint32_t address = 0u;
        while (offset + line_len < length && text[offset + line_len] != '\n') {
            line_len++;
        }
        if (line_len > SYMBOL_LINE_MAX) {
            (void)snprintf(reason, reason_cap, "symbol line too long");
            return 1;
        }
        memcpy(line, text + offset, line_len);
        line[line_len] = '\0';
        if (offset + line_len < length && text[offset + line_len] == '\n') {
            offset += line_len + 1u;
        } else {
            offset += line_len;
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
                token_begin[token_count] = start;
                token_len[token_count] = cursor - start;
            }
            token_count++;
        }
        for (cursor = 0u; cursor < token_count && cursor < 4u; cursor++) {
            if (is_target_name(line + token_begin[cursor], token_len[cursor], &target) != 0) {
                relevant = 1;
            }
        }
        if (relevant == 0) {
            continue;
        }
        if (token_count != 3u || token_len[0] != 2u || memcmp(line + token_begin[0], "al", 2u) != 0 ||
            is_target_name(line + token_begin[2], token_len[2], &target) == 0 ||
            parse_hex_address(line + token_begin[1], token_len[1], &address) != 0) {
            (void)snprintf(reason, reason_cap, "symbol malformed");
            return 1;
        }
        if (found[target] != 0u) {
            (void)snprintf(reason, reason_cap, "symbol duplicate");
            return 1;
        }
        found[target] = 1u;
        positions[target] = (size_t)address;
    }
    if (found[0] == 0u) {
        (void)snprintf(reason, reason_cap, "symbol missing diag_result");
        return 1;
    }
    if (found[1] == 0u) {
        (void)snprintf(reason, reason_cap, "symbol missing diag_pixels");
        return 1;
    }
    if (found[2] == 0u) {
        (void)snprintf(reason, reason_cap, "symbol missing diag_pixels_end");
        return 1;
    }
    if (positions[0] > LYNX_ADDRESS_SPACE - RESULT_SIZE) {
        (void)snprintf(reason, reason_cap, "result symbol range");
        return 1;
    }
    if (positions[1] > LYNX_ADDRESS_SPACE - PIXELS_SIZE ||
        positions[2] > LYNX_ADDRESS_SPACE) {
        (void)snprintf(reason, reason_cap, "pixel symbol range");
        return 1;
    }
    if (positions[2] - positions[1] != PIXELS_SIZE) {
        (void)snprintf(reason, reason_cap, "pixel end mismatch");
        return 1;
    }
    if (!(positions[0] + RESULT_SIZE <= positions[1] || positions[2] <= positions[0])) {
        (void)snprintf(reason, reason_cap, "symbol ranges overlap");
        return 1;
    }
    out->result = (uint32_t)positions[0];
    out->pixels = (uint32_t)positions[1];
    out->pixels_end = (uint32_t)positions[2];
    reason[0] = '\0';
    return 0;
}

static int parse_hex_text(const char *path, const uint8_t *binary, size_t binary_len)
{
    FILE *file = fopen(path, "rb");
    char line[512];
    size_t used = 0u;
    int saw_data = 0;
    int read_failed;
    int close_failed;
    if (file == NULL) {
        return fail("cannot open text hex identity");
    }
    while (fgets(line, sizeof line, file) != NULL) {
        size_t index = 0u;
        int high = -1;
        if (line[0] == '#') {
            continue;
        }
        for (index = 0u; line[index] != '\0'; index++) {
            unsigned char ch = (unsigned char)line[index];
            int digit;
            if (isspace(ch) != 0) {
                continue;
            }
            if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
                digit = (int)(ch - (unsigned char)'0');
            } else if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') {
                digit = (int)(ch - (unsigned char)'a') + 10;
            } else {
                (void)fclose(file);
                return fail("text hex is not lowercase whole-byte data");
            }
            saw_data = 1;
            if (high < 0) {
                high = digit;
            } else {
                if (used >= binary_len || binary[used] != (uint8_t)((unsigned)high * 16u + (unsigned)digit)) {
                    (void)fclose(file);
                    return fail("text hex identity differs from assembled BLL");
                }
                used++;
                high = -1;
            }
        }
        if (high >= 0) {
            (void)fclose(file);
            return fail("text hex has an odd nibble");
        }
    }
    read_failed = ferror(file) != 0;
    close_failed = fclose(file) != 0;
    if (read_failed != 0 || close_failed != 0 || saw_data == 0 || used != binary_len) {
        return fail("text hex identity has wrong length");
    }
    return 0;
}

static int check_header(const uint8_t *bytes, size_t length, const char *hex_path)
{
    uint32_t declared_length;
    if (length < BLL_HEADER_SIZE || bytes[0] != 0x80u || bytes[1] != 0x08u ||
        bytes[2] != 0x02u || bytes[3] != 0x00u || memcmp(bytes + 6u, "BS93", 4u) != 0) {
        return fail("bll header: signature or canonical load address");
    }
    declared_length = ((uint32_t)bytes[4] << 8) | (uint32_t)bytes[5];
    if (declared_length != length) {
        return fail("bll header: declared length differs from object length");
    }
    return hex_path == NULL ? 0 : parse_hex_text(hex_path, bytes, length);
}

static char *replace_line(const char *text, const char *needle, const char *replacement)
{
    const char *found = strstr(text, needle);
    size_t before;
    size_t total;
    char *copy;
    if (found == NULL) {
        return NULL;
    }
    before = (size_t)(found - text);
    total = before + strlen(replacement) + strlen(found + strlen(needle)) + 1u;
    copy = malloc(total);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, before);
    memcpy(copy + before, replacement, strlen(replacement));
    strcpy(copy + before + strlen(replacement), found + strlen(needle));
    return copy;
}

static int expect_symbol_failure(const char *case_name, const char *text, size_t length,
                                 const char *expected)
{
    diag_symbols symbols;
    char reason[96];
    if (parse_symbols(text, length, &symbols, reason, sizeof reason) == 0 ||
        strcmp(reason, expected) != 0) {
        fprintf(stderr, "diag symbols: %s expected '%s', got '%s'\n", case_name,
                expected, reason);
        return 1;
    }
    return 0;
}

static int symbol_controls(const char *valid, size_t length)
{
    diag_symbols parsed;
    char reason[96];
    char *text = NULL;
    size_t bad_len;
    int failed = 0;
    if (parse_symbols(valid, length, &parsed, reason, sizeof reason) != 0) {
        return fail("valid -Ln symbols rejected");
    }
    text = replace_line(valid, "al 000400 .diag_result\n", "");
    if (text == NULL || expect_symbol_failure("missing", text,
                                               length - strlen("al 000400 .diag_result\n"),
                                               "symbol missing diag_result") != 0) {
        failed = 1;
    }
    free(text);
    text = NULL;
    if (failed != 0) {
        return 1;
    }
    {
        static const char duplicate[] = "al 000400 .diag_result\n";
        size_t cap = length + sizeof duplicate;
        text = malloc(cap);
        if (text == NULL) {
            return fail("cannot allocate duplicate symbol control");
        }
        memcpy(text, valid, length);
        memcpy(text + length, duplicate, sizeof duplicate - 1u);
        if (expect_symbol_failure("duplicate", text, length + sizeof duplicate - 1u,
                                  "symbol duplicate") != 0) {
            failed = 1;
        }
    }
    free(text);
    text = replace_line(valid, "al 000400 .diag_result", "al G00400 .diag_result");
    if (text == NULL || expect_symbol_failure("malformed hex", text, length,
                                               "symbol malformed") != 0) {
        failed = 1;
    }
    free(text);
    text = NULL;
    if (failed != 0) {
        return 1;
    }
    bad_len = length + 300u;
    text = malloc(bad_len + 1u);
    if (text == NULL) {
        return fail("cannot allocate long symbol control");
    }
    memcpy(text, valid, length);
    {
        static const char long_line[] = "al 000400 .diag_result ";
        memcpy(text + length, long_line, sizeof long_line - 1u);
        memset(text + length + sizeof long_line - 1u, 'x',
               300u - (sizeof long_line - 1u));
    }
    text[bad_len] = '\0';
    if (expect_symbol_failure("overlong", text, bad_len, "symbol line too long") != 0) {
        failed = 1;
    }
    free(text);
    text = replace_line(valid, "al 000400 .diag_result", "al 00fff9 .diag_result");
    if (text == NULL || expect_symbol_failure("result range", text, length,
                                               "result symbol range") != 0) {
        failed = 1;
    }
    free(text);
    text = replace_line(valid, "al 000500 .diag_pixels\nal 000520 .diag_pixels_end",
                        "al 000404 .diag_pixels\nal 000424 .diag_pixels_end");
    if (text == NULL || expect_symbol_failure("overlap", text, length,
                                               "symbol ranges overlap") != 0) {
        failed = 1;
    }
    free(text);
    text = replace_line(valid, "al 000520 .diag_pixels_end", "al 000521 .diag_pixels_end");
    if (text == NULL || expect_symbol_failure("pixel end", text, length,
                                               "pixel end mismatch") != 0) {
        failed = 1;
    }
    free(text);
    text = replace_line(valid, "al 000500 .diag_pixels", "al 00ffe1 .diag_pixels");
    if (text == NULL || expect_symbol_failure("pixel range", text, length,
                                               "pixel symbol range") != 0) {
        failed = 1;
    }
    free(text);
    text = replace_line(valid, "al 000400 .diag_result", "al 000400 .diag_result trailing");
    if (text == NULL || expect_symbol_failure("trailing token", text, length,
                                               "symbol malformed") != 0) {
        failed = 1;
    }
    free(text);
    if (failed != 0) {
        return 1;
    }
    printf("diag symbols: strict names, bounded ranges, duplicates and malformed lines pass\n");
    return 0;
}

static int machine_create(machine_instance *instance, const uint8_t *bll, size_t bll_len)
{
    lynx_config config;
    lynx_identity identity;
    memset(instance, 0, sizeof *instance);
    instance->storage = malloc(lynx_machine_size());
    if (instance->storage == NULL ||
        (size_t)((uintptr_t)instance->storage % lynx_machine_align()) != 0u) {
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("machine storage allocation or alignment failed");
    }
    lynx_config_default(&config);
    if (lynx_machine_init(instance->storage, lynx_machine_size(), &config,
                          &instance->machine) != LYNX_OK) {
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("public machine initialization failed");
    }
    if (lynx_load_bll(instance->machine, bll, bll_len) != LYNX_OK) {
        lynx_machine_deinit(instance->machine);
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("public BS93 load failed");
    }
    if (lynx_get_identity(instance->machine, &identity) != LYNX_OK ||
        identity.content_format != LYNX_CONTENT_BS93 || identity.startup_tier != LYNX_STARTUP_A ||
        identity.entry_addr != 0x0200u) {
        lynx_machine_deinit(instance->machine);
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("public identity is not the declared BS93 tier-A entry");
    }
    return 0;
}

static void machine_destroy(machine_instance *instance)
{
    if (instance->machine != NULL) {
        lynx_machine_deinit(instance->machine);
    }
    free(instance->storage);
    memset(instance, 0, sizeof *instance);
}

static int read_ticks(const lynx_machine *machine, uint64_t *ticks)
{
    if (lynx_guest_ticks(machine, ticks) != LYNX_OK) {
        return fail("public guest-tick read failed");
    }
    return 0;
}

static int advance_once(lynx_machine *machine, uint64_t budget, uint64_t *used,
                        lynx_stop_reason *stop)
{
    *used = 0u;
    *stop = LYNX_STOP_HALT;
    if (lynx_advance(machine, budget, used, stop) != LYNX_OK || *used > budget) {
        return fail("public advance refused or exceeded its tick budget");
    }
    return 0;
}

static int advance_to_tick(lynx_machine *machine, uint64_t target, uint64_t slice,
                           int accept_watch, uint32_t watch_address,
                           uint32_t watch_value, int *watch_seen)
{
    while (1) {
        uint64_t before = 0u;
        uint64_t after = 0u;
        uint64_t used = 0u;
        uint64_t budget;
        lynx_stop_reason stop;
        if (read_ticks(machine, &before) != 0) {
            return 1;
        }
        if (before == target) {
            return 0;
        }
        if (before > target) {
            return fail("guest time passed common final tick");
        }
        budget = target - before;
        if (budget > slice) {
            budget = slice;
        }
        if (advance_once(machine, budget, &used, &stop) != 0 || read_ticks(machine, &after) != 0) {
            return 1;
        }
        if (stop == LYNX_STOP_WATCH && accept_watch != 0) {
            lynx_watch_event event;
            if (*watch_seen != 0 || lynx_watch_get(machine, &event) != LYNX_OK ||
                event.hit != 1u || event.address != watch_address || event.value != watch_value) {
                return fail("diagnostic write watch did not report the first result byte");
            }
            *watch_seen = 1;
        } else if (stop != LYNX_STOP_BUDGET) {
            if (stop == LYNX_STOP_UNAVAILABLE) {
                return fail("guest stopped at unavailable route");
            }
            if (stop == LYNX_STOP_UNDEFINED) {
                return fail("guest stopped at undefined opcode");
            }
            return fail("guest returned an unexpected stop reason");
        }
        if (after <= before || used == 0u) {
            return fail("guest made no progress toward common final tick");
        }
    }
}

static int read_result(const lynx_machine *machine, const diag_symbols *symbols,
                       uint8_t result[RESULT_SIZE], char *reason, size_t reason_cap)
{
    if (lynx_read_memory(machine, symbols->result, result, RESULT_SIZE) != LYNX_OK) {
        (void)snprintf(reason, reason_cap, "result read");
        return 1;
    }
    if (memcmp(result, "LXD3", 4u) != 0) {
        (void)snprintf(reason, reason_cap, "result magic");
        return 1;
    }
    /* Schema is inside the fixed, symbol-bounded result block. It must be
       checked before any dependent pixel read or result acceptance. */
    if (result[4] != 1u) {
        (void)snprintf(reason, reason_cap, "result schema");
        return 1;
    }
    if (result[5] != 3u) {
        (void)snprintf(reason, reason_cap, "result progress");
        return 1;
    }
    if (result[6] != 0u) {
        (void)snprintf(reason, reason_cap, "result error");
        return 1;
    }
    if (result[7] != 0xA5u) {
        (void)snprintf(reason, reason_cap, "result done");
        return 1;
    }
    reason[0] = '\0';
    return 0;
}

static const uint8_t expected_palette[16][3] = {
    {0x00u, 0xFFu, 0x00u}, {0x11u, 0xEEu, 0x33u}, {0x22u, 0xDDu, 0x66u},
    {0x33u, 0xCCu, 0x99u}, {0x44u, 0xBBu, 0xCCu}, {0x55u, 0xAAu, 0xFFu},
    {0x66u, 0x99u, 0x22u}, {0x77u, 0x88u, 0x55u}, {0x88u, 0x77u, 0x88u},
    {0x99u, 0x66u, 0xBBu}, {0xAAu, 0x55u, 0xEEu}, {0xBBu, 0x44u, 0x11u},
    {0xCCu, 0x33u, 0x44u}, {0xDDu, 0x22u, 0x77u}, {0xEEu, 0x11u, 0xAAu},
    {0xFFu, 0x00u, 0xDDu}
};

static int check_pixels(const lynx_machine *machine, const diag_symbols *symbols,
                        char *reason, size_t reason_cap)
{
    uint8_t pixels[PIXELS_SIZE];
    unsigned y;
    if (symbols->pixels_end - symbols->pixels != PIXELS_SIZE ||
        lynx_read_memory(machine, symbols->pixels, pixels, sizeof pixels) != LYNX_OK) {
        (void)snprintf(reason, reason_cap, "result pixels read");
        return 1;
    }
    for (y = 0u; y < 8u; y++) {
        unsigned x;
        for (x = 0u; x < 8u; x++) {
            unsigned pixel_index = y * 8u + x;
            unsigned byte_index = pixel_index / 2u;
            unsigned pen = (pixel_index % 2u == 0u) ? (unsigned)(pixels[byte_index] >> 4)
                                                    : (unsigned)(pixels[byte_index] & 0x0Fu);
            unsigned expected = (x + 2u * y) & 15u;
            if (pen != expected) {
                (void)snprintf(reason, reason_cap, "result pixels");
                return 1;
            }
            /* The diagnostic reports palette indices only. This separate,
               test-owned palette is the independent host interpretation. */
            if (expected_palette[pen][0] != (uint8_t)(pen * 17u) ||
                expected_palette[pen][1] != (uint8_t)((15u - pen) * 17u) ||
                expected_palette[pen][2] != (uint8_t)(((pen * 3u) & 15u) * 17u)) {
                (void)snprintf(reason, reason_cap, "result palette oracle");
                return 1;
            }
        }
    }
    reason[0] = '\0';
    return 0;
}

static int validate_complete(const lynx_machine *machine, const diag_symbols *symbols,
                             char *reason, size_t reason_cap)
{
    uint8_t result[RESULT_SIZE];
    uint32_t mapctl = 0u;
    if (read_result(machine, symbols, result, reason, reason_cap) != 0) {
        return 1;
    }
    if (check_pixels(machine, symbols, reason, reason_cap) != 0) {
        return 1;
    }
    if (lynx_read_register(machine, LYNX_REG_MAPCTL, &mapctl) != LYNX_OK || mapctl != 0x0Cu) {
        (void)snprintf(reason, reason_cap, "guest MAPCTL initialization");
        return 1;
    }
    reason[0] = '\0';
    return 0;
}

static int run_until_complete(lynx_machine *machine, const diag_symbols *symbols,
                              uint64_t slice, uint64_t *done_tick, char *reason,
                              size_t reason_cap)
{
    uint64_t current = 0u;
    while (current < DIAG_MAX_TICKS) {
        uint64_t used = 0u;
        uint64_t budget = DIAG_MAX_TICKS - current;
        lynx_stop_reason stop;
        uint8_t result[RESULT_SIZE];
        if (budget > slice) {
            budget = slice;
        }
        if (advance_once(machine, budget, &used, &stop) != 0 || read_ticks(machine, &current) != 0) {
            return 1;
        }
        if (stop != LYNX_STOP_BUDGET) {
            if (stop == LYNX_STOP_UNAVAILABLE) {
                (void)snprintf(reason, reason_cap, "guest stopped at unavailable route");
            } else if (stop == LYNX_STOP_UNDEFINED) {
                (void)snprintf(reason, reason_cap, "guest stopped at undefined opcode");
            } else {
                (void)snprintf(reason, reason_cap, "guest returned unexpected stop reason");
            }
            return 1;
        }
        if (used == 0u) {
            (void)snprintf(reason, reason_cap, "guest made no completion progress");
            return 1;
        }
        if (lynx_read_memory(machine, symbols->result, result, RESULT_SIZE) != LYNX_OK) {
            (void)snprintf(reason, reason_cap, "result read");
            return 1;
        }
        if (memcmp(result, "LXD3", 4u) == 0 && result[4] == 1u && result[7] == 0xA5u) {
            *done_tick = current;
            if (current < DIAG_MIN_TICKS || current > DIAG_MAX_TICKS) {
                (void)snprintf(reason, reason_cap, "completion bound");
                return 1;
            }
            if (validate_complete(machine, symbols, reason, reason_cap) != 0) {
                return 1;
            }
            return 0;
        }
    }
    (void)snprintf(reason, reason_cap, "completion bound");
    return 1;
}

/* A small, test-local SHA-256 implementation. It is deliberately isolated
   from core/ and is used only for valid-hash forged-snapshot controls. */
typedef struct test_sha256 {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
} test_sha256;

static uint32_t rotate_right(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32u - shift));
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void test_sha_transform(test_sha256 *hash, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t words[64];
    uint32_t a = hash->state[0];
    uint32_t b = hash->state[1];
    uint32_t c = hash->state[2];
    uint32_t d = hash->state[3];
    uint32_t e = hash->state[4];
    uint32_t f = hash->state[5];
    uint32_t g = hash->state[6];
    uint32_t h = hash->state[7];
    size_t index;
    for (index = 0u; index < 16u; index++) {
        words[index] = read_be32(block + index * 4u);
    }
    for (index = 16u; index < 64u; index++) {
        uint32_t x = words[index - 15u];
        uint32_t y = words[index - 2u];
        uint32_t small0 = rotate_right(x, 7u) ^ rotate_right(x, 18u) ^ (x >> 3);
        uint32_t small1 = rotate_right(y, 17u) ^ rotate_right(y, 19u) ^ (y >> 10);
        words[index] = words[index - 16u] + small0 + words[index - 7u] + small1;
    }
    for (index = 0u; index < 64u; index++) {
        uint32_t big1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t temp1 = h + big1 + choose + constants[index] + words[index];
        uint32_t big0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = big0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    hash->state[0] += a;
    hash->state[1] += b;
    hash->state[2] += c;
    hash->state[3] += d;
    hash->state[4] += e;
    hash->state[5] += f;
    hash->state[6] += g;
    hash->state[7] += h;
}

static void test_sha_init(test_sha256 *hash)
{
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memcpy(hash->state, initial, sizeof initial);
    hash->bit_count = 0u;
    hash->used = 0u;
}

static void test_sha_update(test_sha256 *hash, const uint8_t *bytes, size_t length)
{
    size_t offset = 0u;
    hash->bit_count += (uint64_t)length * 8u;
    while (offset < length) {
        size_t room = sizeof hash->block - hash->used;
        size_t take = length - offset;
        if (take > room) {
            take = room;
        }
        memcpy(hash->block + hash->used, bytes + offset, take);
        hash->used += take;
        offset += take;
        if (hash->used == sizeof hash->block) {
            test_sha_transform(hash, hash->block);
            hash->used = 0u;
        }
    }
}

static void test_sha_final(test_sha256 *hash, uint8_t digest[32])
{
    uint64_t original_bits = hash->bit_count;
    size_t index;
    hash->block[hash->used++] = 0x80u;
    if (hash->used > 56u) {
        memset(hash->block + hash->used, 0, sizeof hash->block - hash->used);
        test_sha_transform(hash, hash->block);
        hash->used = 0u;
    }
    memset(hash->block + hash->used, 0, 56u - hash->used);
    for (index = 0u; index < 8u; index++) {
        hash->block[63u - index] = (uint8_t)(original_bits >> (index * 8u));
    }
    test_sha_transform(hash, hash->block);
    for (index = 0u; index < 8u; index++) {
        write_be32(digest + index * 4u, hash->state[index]);
    }
}

static void test_sha(const uint8_t *bytes, size_t length, uint8_t digest[32])
{
    test_sha256 hash;
    test_sha_init(&hash);
    test_sha_update(&hash, bytes, length);
    test_sha_final(&hash, digest);
}

static int verify_test_sha(void)
{
    static const uint8_t empty_sha[32] = {
        0xE3u,0xB0u,0xC4u,0x42u,0x98u,0xFCu,0x1Cu,0x14u,0x9Au,0xFBu,0xF4u,0xC8u,0x99u,0x6Fu,0xB9u,0x24u,
        0x27u,0xAEu,0x41u,0xE4u,0x64u,0x9Bu,0x93u,0x4Cu,0xA4u,0x95u,0x99u,0x1Bu,0x78u,0x52u,0xB8u,0x55u
    };
    uint8_t digest[32];
    test_sha(NULL, 0u, digest);
    return memcmp(digest, empty_sha, sizeof digest) == 0 ? 0 : fail("test-local SHA-256 self-check failed");
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int snapshot_save(const lynx_machine *machine, uint8_t **out, size_t *length)
{
    size_t size = 0u;
    size_t written = 0u;
    uint8_t *snapshot;
    if (lynx_snapshot_size(machine, &size) != LYNX_OK ||
        size != SNAP_HEADER_SIZE + SNAP_PAYLOAD_SIZE) {
        return fail("public snapshot size does not match its documented format");
    }
    snapshot = malloc(size);
    if (snapshot == NULL) {
        return fail("cannot allocate public snapshot buffer");
    }
    if (lynx_snapshot_save(machine, snapshot, size, &written) != LYNX_OK || written != size) {
        free(snapshot);
        return fail("public snapshot save failed");
    }
    *out = snapshot;
    *length = size;
    return 0;
}

static int rehash_snapshot(uint8_t *snapshot, size_t length)
{
    uint8_t digest[32];
    if (length != SNAP_HEADER_SIZE + SNAP_PAYLOAD_SIZE) {
        return fail("snapshot forge input has unexpected length");
    }
    test_sha(snapshot + SNAP_HEADER_SIZE, SNAP_PAYLOAD_SIZE, digest);
    memcpy(snapshot + SNAP_PAYLOAD_HASH_OFFSET, digest, sizeof digest);
    return 0;
}

static size_t snapshot_ram_offset(uint32_t address)
{
    return SNAP_HEADER_SIZE + SNAP_RAM_OFFSET + (size_t)address;
}

typedef struct result_mutation {
    const char *name;
    size_t offset;
    uint8_t value;
    const char *reason;
} result_mutation;

static int expect_forged_result_failure(lynx_machine *candidate, const uint8_t *base,
                                        size_t size, const diag_symbols *symbols,
                                        const result_mutation *mutation)
{
    uint8_t *forged = malloc(size);
    char reason[96];
    if (forged == NULL) {
        return fail("cannot allocate forged result snapshot");
    }
    memcpy(forged, base, size);
    forged[snapshot_ram_offset(symbols->result) + mutation->offset] = mutation->value;
    if (rehash_snapshot(forged, size) != 0 ||
        lynx_snapshot_restore(candidate, forged, size) != LYNX_OK) {
        free(forged);
        fprintf(stderr, "diag controls: rehashed %s result snapshot did not restore\n", mutation->name);
        return 1;
    }
    if (validate_complete(candidate, symbols, reason, sizeof reason) == 0 ||
        strcmp(reason, mutation->reason) != 0) {
        free(forged);
        fprintf(stderr, "diag controls: forged %s expected '%s', got '%s'\n",
                mutation->name, mutation->reason, reason);
        return 1;
    }
    free(forged);
    return 0;
}

static int expect_invalid_forged_snapshot(lynx_machine *candidate, const uint8_t *base,
                                          size_t size, int invert)
{
    uint8_t *forged = malloc(size);
    uint8_t *before = NULL;
    size_t before_size = 0u;
    uint8_t *after = NULL;
    size_t after_size = 0u;
    lynx_status status;
    if (forged == NULL || snapshot_save(candidate, &before, &before_size) != 0) {
        free(forged);
        free(before);
        return fail("cannot prepare invalid snapshot control");
    }
    memcpy(forged, base, size);
    forged[SNAP_HEADER_SIZE + SNAP_RESERVED_OFFSET] = 1u;
    if (rehash_snapshot(forged, size) != 0) {
        free(forged);
        free(before);
        return 1;
    }
    status = lynx_snapshot_restore(candidate, forged, size);
    if (status != LYNX_ERR_SNAPSHOT_INVALID) {
        free(forged);
        free(before);
        return fail("rehashed reserved-byte snapshot was not rejected as invalid");
    }
    if (invert != 0) {
        free(forged);
        free(before);
        return fail("invalid snapshot was rejected");
    }
    if (snapshot_save(candidate, &after, &after_size) != 0 || before_size != after_size ||
        memcmp(before, after, before_size) != 0) {
        free(forged);
        free(before);
        free(after);
        return fail("invalid snapshot refusal changed machine state");
    }
    free(forged);
    free(before);
    free(after);
    return 0;
}

static int expect_guest_failure(machine_instance *instance, const diag_symbols *symbols,
                                const char *expected)
{
    uint64_t done_tick = 0u;
    char reason[96];
    if (run_until_complete(instance->machine, symbols, 127u, &done_tick,
                           reason, sizeof reason) == 0 || strcmp(reason, expected) != 0) {
        fprintf(stderr, "diag controls: expected guest failure '%s', got '%s'\n", expected, reason);
        return 1;
    }
    if (done_tick < DIAG_MIN_TICKS || done_tick > DIAG_MAX_TICKS) {
        return fail("broken guest completion fell outside the hand-derived diagnostic bound");
    }
    return 0;
}

static int expect_no_run_failure(machine_instance *instance, const diag_symbols *symbols)
{
    uint8_t result[RESULT_SIZE];
    char reason[96];
    if (read_result(instance->machine, symbols, result, reason, sizeof reason) == 0 ||
        strcmp(reason, "result magic") != 0) {
        return fail("zero-budget no-run state did not fail at result magic");
    }
    printf("diag controls: no-run rejected at %s\n", reason);
    return 0;
}

static int run_timeout_bound(machine_instance *instance)
{
    uint64_t current = 0u;
    while (current < DIAG_MAX_TICKS) {
        uint64_t budget = DIAG_MAX_TICKS - current;
        uint64_t used = 0u;
        lynx_stop_reason stop;
        if (budget > 127u) {
            budget = 127u;
        }
        if (advance_once(instance->machine, budget, &used, &stop) != 0 ||
            read_ticks(instance->machine, &current) != 0) {
            return 1;
        }
        if (stop != LYNX_STOP_BUDGET || used == 0u) {
            return fail("timeout guest did not continue within the bound");
        }
    }
    if (current != DIAG_MAX_TICKS) {
        return fail("timeout guest failed to reach the fixed upper bound");
    }
    /* Deliberately do not inspect guest result bytes: timeout is the criterion. */
    printf("diag controls: no-done guest rejected at fixed upper bound=%" PRIu64 " ticks\n",
           current);
    return 0;
}

static int save_snapshot_for_machine(const lynx_machine *machine, uint8_t **snapshot,
                                     size_t *snapshot_size)
{
    if (snapshot_save(machine, snapshot, snapshot_size) != 0) {
        return 1;
    }
    if (read_le32(*snapshot + 8u) != 3u ||
        read_le32(*snapshot + 12u) != SNAP_HEADER_SIZE ||
        read_le32(*snapshot + SNAP_HEADER_SIZE + 16u) != FINAL_TICK) {
        free(*snapshot);
        *snapshot = NULL;
        return fail("public snapshot does not describe format 3 at final tick");
    }
    return 0;
}

static int forged_result_controls(machine_instance *baseline, machine_instance *candidate,
                                  const diag_symbols *symbols)
{
    static const result_mutation mutations[] = {
        {"magic", 0u, (uint8_t)'X', "result magic"},
        {"schema", 4u, 2u, "result schema"},
        {"progress", 5u, 2u, "result progress"},
        {"error", 6u, 1u, "result error"},
        {"done", 7u, 0u, "result done"}
    };
    uint8_t *base = NULL;
    size_t size = 0u;
    size_t index;
    char reason[96];
    uint8_t *forged = NULL;
    if (save_snapshot_for_machine(baseline->machine, &base, &size) != 0) {
        return 1;
    }
    for (index = 0u; index < sizeof mutations / sizeof mutations[0]; index++) {
        if (expect_forged_result_failure(candidate->machine, base, size, symbols,
                                         &mutations[index]) != 0) {
            free(base);
            return 1;
        }
    }
    forged = malloc(size);
    if (forged == NULL) {
        free(base);
        return fail("cannot allocate forged pixel snapshot");
    }
    memcpy(forged, base, size);
    forged[snapshot_ram_offset(symbols->pixels)] ^= 0x80u;
    if (rehash_snapshot(forged, size) != 0 ||
        lynx_snapshot_restore(candidate->machine, forged, size) != LYNX_OK ||
        validate_complete(candidate->machine, symbols, reason, sizeof reason) == 0 ||
        strcmp(reason, "result pixels") != 0) {
        free(base);
        free(forged);
        return fail("rehashed incorrect pixel snapshot did not fail the pixel oracle");
    }
    if (expect_invalid_forged_snapshot(candidate->machine, base, size, 0) != 0) {
        free(base);
        free(forged);
        return 1;
    }
    free(base);
    free(forged);
    printf("diag controls: rehashed magic/schema/progress/error/done/pixel and invalid-state controls pass\n");
    return 0;
}

static int compare_snapshots(const char *label, const lynx_machine *expected,
                             const lynx_machine *actual)
{
    uint8_t *left = NULL;
    uint8_t *right = NULL;
    size_t left_len = 0u;
    size_t right_len = 0u;
    int same;
    if (snapshot_save(expected, &left, &left_len) != 0 ||
        snapshot_save(actual, &right, &right_len) != 0) {
        free(left);
        free(right);
        return 1;
    }
    same = left_len == right_len && memcmp(left, right, left_len) == 0;
    free(left);
    free(right);
    if (same == 0) {
        fprintf(stderr, "diag restore: %s public snapshot differs at common final tick\n", label);
        return 1;
    }
    return 0;
}

static int read_symbols_file(const char *path, char **text, size_t *length,
                             diag_symbols *symbols)
{
    char reason[96];
    if (read_text(path, text, length) != 0) {
        return 1;
    }
    if (parse_symbols(*text, *length, symbols, reason, sizeof reason) != 0) {
        fprintf(stderr, "diag symbols: %s\n", reason);
        free(*text);
        *text = NULL;
        *length = 0u;
        return 1;
    }
    return 0;
}

static int read_bll_file(const char *path, uint8_t **bll, size_t *length)
{
    return read_file(path, bll, length, 4096u);
}

static int header_mode(const char *bll_path, const char *labels_path, const char *hex_path)
{
    uint8_t *bll = NULL;
    size_t bll_len = 0u;
    char *labels = NULL;
    size_t labels_len = 0u;
    diag_symbols symbols;
    char reason[96];
    int result = 1;
    if (read_bll_file(bll_path, &bll, &bll_len) != 0 ||
        read_symbols_file(labels_path, &labels, &labels_len, &symbols) != 0) {
        goto done;
    }
    if (check_header(bll, bll_len, hex_path) != 0) {
        goto done;
    }
    if (symbols.result != 0x0400u || symbols.pixels != 0x0500u || symbols.pixels_end != 0x0520u) {
        (void)snprintf(reason, sizeof reason, "linked symbol contract differs from declared fixture ranges");
        fprintf(stderr, "diag header: %s\n", reason);
        goto done;
    }
    printf("diag header: canonical BS93 header, linked symbols and text identity pass (%zu bytes)\n",
           bll_len);
    result = 0;
done:
    free(bll);
    free(labels);
    return result;
}

static int symbol_mode(const char *labels_path)
{
    char *labels = NULL;
    size_t length = 0u;
    diag_symbols symbols;
    int result;
    if (read_symbols_file(labels_path, &labels, &length, &symbols) != 0) {
        return 1;
    }
    result = symbol_controls(labels, length);
    free(labels);
    return result;
}

static int controls_mode(const char *normal_path, const char *normal_labels,
                         const char *broken_path, const char *broken_labels,
                         const char *timeout_path, const char *timeout_labels)
{
    uint8_t *normal_bll = NULL;
    uint8_t *broken_bll = NULL;
    uint8_t *timeout_bll = NULL;
    size_t normal_len = 0u;
    size_t broken_len = 0u;
    size_t timeout_len = 0u;
    char *normal_label_text = NULL;
    char *broken_label_text = NULL;
    char *timeout_label_text = NULL;
    size_t normal_label_len = 0u;
    size_t broken_label_len = 0u;
    size_t timeout_label_len = 0u;
    diag_symbols normal_symbols;
    diag_symbols broken_symbols;
    diag_symbols timeout_symbols;
    machine_instance normal = {0};
    machine_instance broken = {0};
    machine_instance timeout = {0};
    machine_instance candidate = {0};
    machine_instance no_run = {0};
    uint64_t done_tick = 0u;
    char reason[96];
    int result = 1;
    if (verify_test_sha() != 0 || read_bll_file(normal_path, &normal_bll, &normal_len) != 0 ||
        read_bll_file(broken_path, &broken_bll, &broken_len) != 0 ||
        read_bll_file(timeout_path, &timeout_bll, &timeout_len) != 0 ||
        read_symbols_file(normal_labels, &normal_label_text, &normal_label_len, &normal_symbols) != 0 ||
        read_symbols_file(broken_labels, &broken_label_text, &broken_label_len, &broken_symbols) != 0 ||
        read_symbols_file(timeout_labels, &timeout_label_text, &timeout_label_len, &timeout_symbols) != 0) {
        goto done;
    }
    if (check_header(normal_bll, normal_len, NULL) != 0 ||
        check_header(broken_bll, broken_len, NULL) != 0 ||
        check_header(timeout_bll, timeout_len, NULL) != 0) {
        goto done;
    }
    if (machine_create(&normal, normal_bll, normal_len) != 0 ||
        machine_create(&broken, broken_bll, broken_len) != 0 ||
        machine_create(&timeout, timeout_bll, timeout_len) != 0 ||
        machine_create(&candidate, normal_bll, normal_len) != 0 ||
        machine_create(&no_run, normal_bll, normal_len) != 0) {
        goto done;
    }
    if (run_until_complete(normal.machine, &normal_symbols, 127u, &done_tick,
                           reason, sizeof reason) != 0) {
        fprintf(stderr, "diag controls: normal guest failed: %s\n", reason);
        goto done;
    }
    if (advance_to_tick(normal.machine, FINAL_TICK, 173u, 0, 0u, 0u, &(int){0}) != 0 ||
        expect_no_run_failure(&no_run, &normal_symbols) != 0 ||
        expect_guest_failure(&broken, &broken_symbols, "result error") != 0 ||
        run_timeout_bound(&timeout) != 0) {
        goto done;
    }
    if (forged_result_controls(&normal, &candidate, &normal_symbols) != 0) {
        goto done;
    }
    if (validate_complete(normal.machine, &normal_symbols, reason, sizeof reason) != 0) {
        fprintf(stderr, "diag controls: normal result at final tick failed: %s\n", reason);
        goto done;
    }
    printf("diag controls: positive result first observed complete by tick=%" PRIu64
           " and final tick=%u (flat5 PLACEHOLDER)\n", done_tick, FINAL_TICK);
    result = 0;
done:
    machine_destroy(&normal);
    machine_destroy(&broken);
    machine_destroy(&timeout);
    machine_destroy(&candidate);
    machine_destroy(&no_run);
    free(normal_bll);
    free(broken_bll);
    free(timeout_bll);
    free(normal_label_text);
    free(broken_label_text);
    free(timeout_label_text);
    return result;
}

static int restore_mode(const char *normal_path, const char *labels_path, int invert_forgery)
{
    static const uint64_t slices[] = {1u, 17u, 63u, 127u, 511u};
    uint8_t *bll = NULL;
    size_t bll_len = 0u;
    char *labels = NULL;
    size_t labels_len = 0u;
    diag_symbols symbols;
    machine_instance baseline = {0};
    machine_instance sliced = {0};
    machine_instance watched = {0};
    machine_instance mid_source = {0};
    machine_instance restored = {0};
    uint8_t *mid_snapshot = NULL;
    size_t mid_size = 0u;
    uint64_t done_tick = 0u;
    uint8_t result_bytes[RESULT_SIZE];
    char reason[96];
    size_t index = 0u;
    int watch_seen = 0;
    int result = 1;
    if (read_bll_file(normal_path, &bll, &bll_len) != 0 ||
        read_symbols_file(labels_path, &labels, &labels_len, &symbols) != 0 ||
        machine_create(&baseline, bll, bll_len) != 0 ||
        machine_create(&sliced, bll, bll_len) != 0 ||
        machine_create(&watched, bll, bll_len) != 0 ||
        machine_create(&mid_source, bll, bll_len) != 0 ||
        machine_create(&restored, bll, bll_len) != 0) {
        goto done;
    }
    {
        uint64_t used = 0u;
        lynx_stop_reason stop = LYNX_STOP_HALT;
        if (advance_once(baseline.machine, FINAL_TICK, &used, &stop) != 0 ||
            used != FINAL_TICK || stop != LYNX_STOP_BUDGET) {
            goto done;
        }
    }
    if (read_ticks(baseline.machine, &done_tick) != 0 || done_tick != FINAL_TICK ||
        validate_complete(baseline.machine, &symbols, reason, sizeof reason) != 0) {
        goto done;
    }
    while (1) {
        uint64_t tick = 0u;
        uint64_t slice = slices[index % (sizeof slices / sizeof slices[0])];
        if (read_ticks(sliced.machine, &tick) != 0 || tick == FINAL_TICK) {
            break;
        }
        {
            uint64_t target = tick + slice;
            if (target > FINAL_TICK) {
                target = FINAL_TICK;
            }
            if (advance_to_tick(sliced.machine, target, slice, 0, 0u, 0u,
                                &watch_seen) != 0) {
                goto done;
            }
        }
        index++;
    }
    if (lynx_watch_configure(watched.machine, symbols.result + 7u, 1u) != LYNX_OK ||
        advance_to_tick(watched.machine, FINAL_TICK, FINAL_TICK, 1,
                        symbols.result + 7u, 0xA5u, &watch_seen) != 0 ||
        watch_seen == 0) {
        goto done;
    }
    if (advance_to_tick(mid_source.machine, 1777u, 137u, 0, 0u, 0u, &watch_seen) != 0 ||
        lynx_read_memory(mid_source.machine, symbols.result, result_bytes, sizeof result_bytes) != LYNX_OK ||
        result_bytes[7] == 0xA5u || snapshot_save(mid_source.machine, &mid_snapshot, &mid_size) != 0 ||
        lynx_snapshot_restore(restored.machine, mid_snapshot, mid_size) != LYNX_OK ||
        advance_to_tick(restored.machine, FINAL_TICK, 131u, 0, 0u, 0u, &watch_seen) != 0) {
        goto done;
    }
    if (validate_complete(sliced.machine, &symbols, reason, sizeof reason) != 0 ||
        validate_complete(watched.machine, &symbols, reason, sizeof reason) != 0 ||
        validate_complete(restored.machine, &symbols, reason, sizeof reason) != 0 ||
        compare_snapshots("sliced", baseline.machine, sliced.machine) != 0 ||
        compare_snapshots("watched", baseline.machine, watched.machine) != 0 ||
        compare_snapshots("mid-diagnostic restore", baseline.machine, restored.machine) != 0) {
        goto done;
    }
    if (invert_forgery != 0) {
        uint8_t *base = NULL;
        size_t base_size = 0u;
        if (snapshot_save(baseline.machine, &base, &base_size) != 0) {
            free(base);
            goto done;
        }
        result = expect_invalid_forged_snapshot(restored.machine, base, base_size, 1);
        free(base);
        goto done;
    }
    printf("diag restore: sliced, watched and tick-1777 restored paths agree at tick=%u\n",
           FINAL_TICK);
    result = 0;
done:
    free(mid_snapshot);
    free(bll);
    free(labels);
    machine_destroy(&baseline);
    machine_destroy(&sliced);
    machine_destroy(&watched);
    machine_destroy(&mid_source);
    machine_destroy(&restored);
    return result;
}

static int write_bytes_exclusive(const char *path, const uint8_t *bytes, size_t length)
{
    FILE *file = fopen(path, "wbx");
    int failed;
    if (file == NULL) {
        return fail("cannot create owned fault fixture");
    }
    failed = fwrite(bytes, 1u, length, file) != length;
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed != 0) {
        (void)remove(path);
        return fail("cannot finish owned fault fixture");
    }
    return 0;
}

static int corrupt_map_mode(const char *input_path, const char *hex_path,
                            const char *output_path)
{
    uint8_t *bll = NULL;
    size_t length = 0u;
    int result = 1;
    if (read_bll_file(input_path, &bll, &length) != 0 ||
        check_header(bll, length, hex_path) != 0) {
        goto done;
    }
    if (length < BLL_HEADER_SIZE + 5u || bll[BLL_HEADER_SIZE] != 0xA9u ||
        bll[BLL_HEADER_SIZE + 1u] != 0x0Cu || bll[BLL_HEADER_SIZE + 2u] != 0x8Du ||
        bll[BLL_HEADER_SIZE + 3u] != 0xF9u || bll[BLL_HEADER_SIZE + 4u] != 0xFFu) {
        (void)fail("positive source fixture lacks the expected MAPCTL selection instruction");
        goto done;
    }
    bll[BLL_HEADER_SIZE + 1u] = 0u;
    if (write_bytes_exclusive(output_path, bll, length) != 0) {
        goto done;
    }
    printf("diag fault: derived wrong-map control by changing the MAPCTL operand byte\n");
    result = 0;
done:
    free(bll);
    return result;
}

static int corrupt_pixels_mode(const char *input_path, const char *hex_path,
                               const char *output_path)
{
    static const uint8_t packed_pixels[PIXELS_SIZE] = {
        0x01u, 0x23u, 0x45u, 0x67u, 0x23u, 0x45u, 0x67u, 0x89u,
        0x45u, 0x67u, 0x89u, 0xABu, 0x67u, 0x89u, 0xABu, 0xCDu,
        0x89u, 0xABu, 0xCDu, 0xEFu, 0xABu, 0xCDu, 0xEFu, 0x01u,
        0xCDu, 0xEFu, 0x01u, 0x23u, 0xEFu, 0x01u, 0x23u, 0x45u
    };
    uint8_t *bll = NULL;
    size_t length = 0u;
    size_t offset;
    size_t match = SIZE_MAX;
    int result = 1;
    if (read_bll_file(input_path, &bll, &length) != 0 ||
        check_header(bll, length, hex_path) != 0) {
        goto done;
    }
    if (length < sizeof packed_pixels) {
        (void)fail("positive source fixture is shorter than its pixel fault pattern");
        goto done;
    }
    for (offset = 0u; offset <= length - sizeof packed_pixels; offset++) {
        if (memcmp(bll + offset, packed_pixels, sizeof packed_pixels) == 0) {
            if (match != SIZE_MAX) {
                (void)fail("positive source fixture contains ambiguous packed pixel pattern");
                goto done;
            }
            match = offset;
        }
    }
    if (match == SIZE_MAX) {
        (void)fail("positive source fixture lacks the exact packed pixel pattern");
        goto done;
    }
    bll[match] ^= 0x10u;
    if (write_bytes_exclusive(output_path, bll, length) != 0) {
        goto done;
    }
    printf("diag fault: derived bad-pixels control from one packed guest pixel byte\n");
    result = 0;
done:
    free(bll);
    return result;
}

static int hex_digit_value(uint8_t value)
{
    if (value >= (uint8_t)'0' && value <= (uint8_t)'9') {
        return (int)(value - (uint8_t)'0');
    }
    if (value >= (uint8_t)'a' && value <= (uint8_t)'f') {
        return (int)(value - (uint8_t)'a') + 10;
    }
    return -1;
}

static int read_hex_rows(const char *path, size_t expected_bytes, uint8_t **out)
{
    const size_t row_bytes = 32u;
    size_t rows = (expected_bytes + row_bytes - 1u) / row_bytes;
    size_t text_limit;
    size_t text_length = 0u;
    uint8_t *text = NULL;
    uint8_t *decoded = NULL;
    size_t byte_offset = 0u;
    size_t row;
    if (expected_bytes > (SIZE_MAX - rows) / 2u) {
        return fail("runner hex observation size overflow");
    }
    text_limit = expected_bytes * 2u + rows;
    if (read_file(path, &text, &text_length, text_limit) != 0) {
        return fail("cannot read bounded runner hex observation");
    }
    if (text_length != text_limit) {
        free(text);
        return fail("runner hex observation has the wrong length");
    }
    decoded = malloc(expected_bytes == 0u ? 1u : expected_bytes);
    if (decoded == NULL) {
        free(text);
        return fail("cannot allocate runner observation bytes");
    }
    for (row = 0u; row < rows; row++) {
        size_t remaining = expected_bytes - byte_offset;
        size_t count = remaining < row_bytes ? remaining : row_bytes;
        size_t index;
        size_t row_offset = row * (row_bytes * 2u + 1u);
        for (index = 0u; index < count; index++) {
            int high = hex_digit_value(text[row_offset + index * 2u]);
            int low = hex_digit_value(text[row_offset + index * 2u + 1u]);
            if (high < 0 || low < 0) {
                free(decoded);
                free(text);
                return fail("runner hex observation contains non-canonical data");
            }
            decoded[byte_offset + index] = (uint8_t)((unsigned)high * 16u + (unsigned)low);
        }
        if (text[row_offset + count * 2u] != (uint8_t)'\n') {
            free(decoded);
            free(text);
            return fail("runner hex observation line width is invalid");
        }
        byte_offset += count;
    }
    free(text);
    *out = decoded;
    return 0;
}

static int read_bundle_file(const char *directory, const char *name,
                            size_t limit, uint8_t **bytes, size_t *length)
{
    char path[4096];
    int written = snprintf(path, sizeof path, "%s/%s", directory, name);
    if (written < 0 || (size_t)written >= sizeof path) {
        return fail("runner bundle path exceeds its bound");
    }
    return read_file(path, bytes, length, limit);
}

static int manifest_field(const char *manifest, size_t length, const char *key,
                          char *value, size_t value_cap)
{
    size_t key_len = strlen(key);
    size_t offset = 0u;
    unsigned found = 0u;
    value[0] = '\0';
    while (offset < length) {
        size_t line_len = 0u;
        size_t value_start;
        size_t value_len;
        while (offset + line_len < length && manifest[offset + line_len] != '\n') {
            line_len++;
        }
        if (line_len > key_len + 3u &&
            memcmp(manifest + offset, key, key_len) == 0 &&
            manifest[offset + key_len] == ' ' && manifest[offset + key_len + 1u] == '=' &&
            manifest[offset + key_len + 2u] == ' ') {
            if (found != 0u) {
                return 1;
            }
            value_start = offset + key_len + 3u;
            value_len = line_len - key_len - 3u;
            if (value_len == 0u || value_len >= value_cap ||
                memchr(manifest + value_start, '\r', value_len) != NULL) {
                return 1;
            }
            memcpy(value, manifest + value_start, value_len);
            value[value_len] = '\0';
            found = 1u;
        }
        offset += line_len;
        if (offset < length && manifest[offset] == '\n') {
            offset++;
        }
    }
    return found == 1u ? 0 : 1;
}

static int require_manifest_field(const char *manifest, size_t length, const char *key,
                                  const char *expected)
{
    char actual[256];
    if (manifest_field(manifest, length, key, actual, sizeof actual) != 0 ||
        strcmp(actual, expected) != 0) {
        fprintf(stderr, "diag bundle: manifest %s disagrees with expected identity/run data\n", key);
        return 1;
    }
    return 0;
}

static uint64_t load_u64le(const uint8_t *bytes)
{
    uint64_t value = 0u;
    unsigned index;
    for (index = 0u; index < 8u; index++) {
        value |= (uint64_t)bytes[index] << (index * 8u);
    }
    return value;
}

static uint32_t load_u32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void identity_hex(const uint8_t digest[32], char out[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;
    for (index = 0u; index < 32u; index++) {
        out[index * 2u] = digits[digest[index] >> 4];
        out[index * 2u + 1u] = digits[digest[index] & 0x0Fu];
    }
    out[64] = '\0';
}

static int check_expected_pixels(const uint8_t *ram, const diag_symbols *symbols)
{
    unsigned y;
    for (y = 0u; y < 8u; y++) {
        unsigned x;
        for (x = 0u; x < 8u; x++) {
            size_t index = (size_t)y * 8u + x;
            uint8_t pair = ram[symbols->pixels + index / 2u];
            unsigned pen = index % 2u == 0u ? (unsigned)(pair >> 4)
                                             : (unsigned)(pair & 0x0Fu);
            if (pen != ((x + 2u * y) & 15u)) {
                return fail("runner packed pixels differ from the independent diagnostic contract");
            }
        }
    }
    return 0;
}

static int bundle_mode(const char *bll_path, const char *labels_path,
                       const char *bundle_dir, const char *expected_case)
{
    uint8_t *bll = NULL;
    uint8_t *ram = NULL;
    uint8_t *regs = NULL;
    uint8_t *time_bytes = NULL;
    uint8_t *manifest_bytes = NULL;
    size_t bll_len = 0u;
    size_t manifest_len = 0u;
    char *label_text = NULL;
    size_t label_len = 0u;
    diag_symbols symbols;
    machine_instance public_machine = {0};
    lynx_identity identity;
    lynx_stop_reason public_stop = LYNX_STOP_HALT;
    uint64_t public_used = 0u;
    uint64_t public_ticks = 0u;
    char hash[65];
    char expected_value[64];
    uint8_t result_bytes[RESULT_SIZE];
    uint32_t mapctl;
    int expected_map_failure = strcmp(expected_case, "map-failure") == 0;
    int expected_guest_failure = strcmp(expected_case, "guest-failure") == 0;
    int result = 1;
    if (strcmp(expected_case, "pass") != 0 && expected_map_failure == 0 &&
        expected_guest_failure == 0) {
        return fail("unknown bundle expectation");
    }
    if (read_bll_file(bll_path, &bll, &bll_len) != 0 ||
        read_symbols_file(labels_path, &label_text, &label_len, &symbols) != 0 ||
        machine_create(&public_machine, bll, bll_len) != 0) {
        goto done;
    }
    if (lynx_advance(public_machine.machine, 5000u, &public_used, &public_stop) != LYNX_OK ||
        public_used > 5000u || read_ticks(public_machine.machine, &public_ticks) != 0 ||
        lynx_get_identity(public_machine.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A || identity.entry_addr != 0x0200u ||
        identity.object_len != bll_len) {
        (void)fail("public runner comparison machine did not reach a valid BLL identity");
        goto done;
    }
    if ((expected_map_failure != 0 && public_stop != LYNX_STOP_UNAVAILABLE) ||
        (expected_map_failure == 0 && public_stop != LYNX_STOP_BUDGET) ||
        public_ticks != public_used) {
        (void)fail("runner stop/time differs from the independently advanced public machine");
        goto done;
    }
    {
        char path[4096];
        int n = snprintf(path, sizeof path, "%s/obs.ram.hex", bundle_dir);
        if (n < 0 || (size_t)n >= sizeof path ||
            read_hex_rows(path, LYNX_ADDRESS_SPACE, &ram) != 0) {
            goto done;
        }
    }
    {
        char path[4096];
        int n = snprintf(path, sizeof path, "%s/obs.regs.hex", bundle_dir);
        if (n < 0 || (size_t)n >= sizeof path || read_hex_rows(path, 36u, &regs) != 0) {
            goto done;
        }
    }
    {
        char path[4096];
        int n = snprintf(path, sizeof path, "%s/obs.time.hex", bundle_dir);
        if (n < 0 || (size_t)n >= sizeof path || read_hex_rows(path, 20u, &time_bytes) != 0) {
            goto done;
        }
    }
    if (read_bundle_file(bundle_dir, "run.manifest", 4096u,
                         &manifest_bytes, &manifest_len) != 0 || manifest_len == 0u) {
        (void)fail("runner manifest is missing or exceeds its bound");
        goto done;
    }
    {
        uint8_t *terminated = realloc(manifest_bytes, manifest_len + 1u);
        if (terminated == NULL) {
            (void)fail("cannot terminate bounded runner manifest");
            goto done;
        }
        if (memchr(terminated, '\0', manifest_len) != NULL) {
            free(terminated);
            manifest_bytes = NULL;
            (void)fail("runner manifest is not bounded plain text");
            goto done;
        }
        manifest_bytes = terminated;
        manifest_bytes[manifest_len] = '\0';
    }
    mapctl = load_u32le(regs + LYNX_REG_MAPCTL * 4u);
    if (mapctl != (expected_map_failure != 0 ? 0u : 0x0Cu) ||
        load_u64le(time_bytes) != public_ticks || load_u64le(time_bytes + 8u) != public_used ||
        load_u32le(time_bytes + 16u) != public_stop) {
        (void)fail("runner raw register/time bytes differ from public core observations");
        goto done;
    }
    memcpy(result_bytes, ram + symbols.result, sizeof result_bytes);
    if (memcmp(result_bytes, "LXD3", 4u) != 0 || result_bytes[4] != 1u) {
        (void)fail("runner raw result header is invalid");
        goto done;
    }
    if (expected_map_failure != 0) {
        if (result_bytes[5] != 1u || result_bytes[6] != 0u || result_bytes[7] == 0xA5u) {
            (void)fail("wrong-map runner control did not stop with an incomplete guest result");
            goto done;
        }
    } else {
        uint8_t expected_error = expected_guest_failure != 0 ? 1u : 0u;
        if (result_bytes[5] != 3u || result_bytes[6] != expected_error || result_bytes[7] != 0xA5u ||
            check_expected_pixels(ram, &symbols) != 0) {
            if (result_bytes[5] != 3u || result_bytes[6] != expected_error || result_bytes[7] != 0xA5u) {
                (void)fail("runner raw result bytes differ from the independent guest expectation");
            }
            goto done;
        }
    }
    identity_hex(identity.content_sha256, hash);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "profile", "none") != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "content_format", "bs93") != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "startup", "A") != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "startup_tier", "A") != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "content_sha256", hash) != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "core_build", lynx_build_id()) != 0) {
        goto done;
    }
    (void)snprintf(expected_value, sizeof expected_value, "0x%04x", (unsigned)identity.content_addr);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "content_load_addr", expected_value) != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "payload_addr", expected_value) != 0) {
        goto done;
    }
    (void)snprintf(expected_value, sizeof expected_value, "0x%04x", (unsigned)identity.entry_addr);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "entry_addr", expected_value) != 0) {
        goto done;
    }
    (void)snprintf(expected_value, sizeof expected_value, "%u", (unsigned)identity.object_len);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "object_len", expected_value) != 0) {
        goto done;
    }
    (void)snprintf(expected_value, sizeof expected_value, "%u", (unsigned)identity.content_len);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "payload_len", expected_value) != 0) {
        goto done;
    }
    (void)snprintf(expected_value, sizeof expected_value, "%llu",
                   (unsigned long long)public_ticks);
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "tick_budget", "5000") != 0 ||
        require_manifest_field((const char *)manifest_bytes, manifest_len, "ticks_consumed", expected_value) != 0) {
        goto done;
    }
    if (require_manifest_field((const char *)manifest_bytes, manifest_len, "stop_reason",
                               expected_map_failure != 0 ? "unavailable" : "budget") != 0) {
        goto done;
    }
    printf("diag bundle: %s runner identity, raw result, packed pixels and guest time agree\n",
           expected_case);
    result = 0;
done:
    free(bll);
    free(ram);
    free(regs);
    free(time_bytes);
    free(manifest_bytes);
    free(label_text);
    machine_destroy(&public_machine);
    return result;
}

int main(int argc, char **argv)
{
    if (argc == 5 && strcmp(argv[1], "header") == 0) {
        return header_mode(argv[2], argv[3], argv[4]);
    }
    if (argc == 3 && strcmp(argv[1], "symbols") == 0) {
        return symbol_mode(argv[2]);
    }
    if (argc == 8 && strcmp(argv[1], "controls") == 0) {
        return controls_mode(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
    }
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "restore") == 0) {
        int invert = argc == 5 && strcmp(argv[4], "fault-accept-forgery") == 0;
        if (argc == 5 && invert == 0) {
            return fail("unknown restore mode");
        }
        return restore_mode(argv[2], argv[3], invert);
    }
    if (argc == 4 && strcmp(argv[1], "corrupt-header") == 0) {
        uint8_t *bytes = NULL;
        size_t length = 0u;
        FILE *out;
        int result = 1;
        if (read_bll_file(argv[2], &bytes, &length) != 0 ||
            check_header(bytes, length, NULL) != 0 || length < BLL_HEADER_SIZE) {
            free(bytes);
            return fail("header fault control lacks a valid positive fixture");
        }
        bytes[6] = (uint8_t)'X';
        out = fopen(argv[3], "wb");
        if (out == NULL) {
            free(bytes);
            return fail("cannot write corrupted header control");
        }
        if (fwrite(bytes, 1u, length, out) == length) {
            if (fclose(out) == 0) {
                result = 0;
            } else {
                result = fail("cannot complete corrupted header control");
            }
        } else {
            (void)fclose(out);
            result = fail("cannot complete corrupted header control");
        }
        free(bytes);
        return result;
    }
    if (argc == 5 && strcmp(argv[1], "corrupt-map") == 0) {
        return corrupt_map_mode(argv[2], argv[3], argv[4]);
    }
    if (argc == 5 && strcmp(argv[1], "corrupt-pixels") == 0) {
        return corrupt_pixels_mode(argv[2], argv[3], argv[4]);
    }
    if (argc == 6 && strcmp(argv[1], "bundle") == 0) {
        return bundle_mode(argv[2], argv[3], argv[4], argv[5]);
    }
    fprintf(stderr, "diag: invalid command line\n");
    return 2;
}



