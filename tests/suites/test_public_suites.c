/* Headless public-API harness for exact, source-generated suite evidence.
   This intentionally distinguishes raw guest observations from their
   comparison with the pinned software oracle. */
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "internal.h"
#include "lynxplosion.h"
#include "sha256.h"

enum {
    BLL_HEADER_SIZE = 10,
    RESULT_EXTENT = 18,
    CPU_MEANINGFUL = 18,
    CPU_MIN_TICKS = 256,
    CPU_MAX_TICKS = 50000000,
    TIMER_MEANINGFUL = 16,
    TIMER_RESERVED = 2,
    TIMER_MIN_TICKS = 256,
    TIMER_MAX_TICKS = 50000000,
    JSON_MAX_FIELDS = 48,
    JSON_ARRAY_MAX = 8,
    JSON_TEXT_MAX = 256,
    SYMBOL_LINE_MAX = 255,
    ORACLE_LINE_MAX = 255,
    EXPECT_VALUES_MAX = 8
};

typedef struct suite_symbols {
    uint32_t value[14];
    uint8_t found[14];
} suite_symbols;

typedef struct suite_oracle {
    char archive[65];
    char revision[41];
    char cpu_main[65];
    char cpu_asm[65];
    char timers_main[65];
    char timers_asm[65];
    char license[65];
    uint8_t values[2][RESULT_EXTENT][EXPECT_VALUES_MAX];
    uint8_t counts[2][RESULT_EXTENT];
    uint8_t tuple[2][4];
} suite_oracle;

typedef struct machine_instance {
    void *storage;
    lynx_machine *machine;
} machine_instance;

typedef enum json_kind {
    JSON_STRING = 1,
    JSON_NUMBER,
    JSON_BOOLEAN,
    JSON_STRING_ARRAY
} json_kind;

typedef struct json_field {
    char key[64];
    json_kind kind;
    char string[JSON_TEXT_MAX];
    uint32_t number;
    uint8_t boolean;
    char items[JSON_ARRAY_MAX][32];
    size_t item_count;
} json_field;

typedef struct json_object {
    json_field fields[JSON_MAX_FIELDS];
    size_t count;
} json_object;

typedef struct timer_paths {
    char raw[4096];
    char record[4096];
    char comparison[4096];
    char proof[4096];
    char companion[4096];
    char bll[4096];
    char labels[4096];
} timer_paths;

static const char *const symbol_names[14] = {
    "suite_entry", "suite_test_entry", "_g_results", "suite_result_end",
    "suite_progress", "suite_model", "suite_done", "suite_end",
    "_run_tests", "suite_first_test", "suite_first_mapped", "suite_probe_result",
    "suite_test_return", "suite_after_tests"
};

static int fail(const char *reason)
{
    fprintf(stderr, "suite: %s\n", reason);
    return 1;
}

static int hex_digit(unsigned char ch)
{
    if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') return (int)(ch - (unsigned char)'0');
    if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') return (int)(ch - (unsigned char)'a') + 10;
    return -1;
}

static int valid_hex(const char *text, size_t length)
{
    size_t i;
    if (strlen(text) != length) return 0;
    for (i = 0u; i < length; i++) if (hex_digit((unsigned char)text[i]) < 0) return 0;
    return 1;
}

static int parse_decimal(const char *text, uint32_t *out)
{
    uint32_t value = 0u;
    size_t i;
    if (text[0] == '\0' || (text[0] == '0' && text[1] != '\0') ||
        (text[0] < '0' || text[0] > '9')) return 1;
    for (i = 0u; text[i] != '\0'; i++) {
        uint32_t digit;
        if (text[i] < '0' || text[i] > '9') return 1;
        digit = (uint32_t)(text[i] - '0');
        if (value > (UINT32_MAX - digit) / 10u) return 1;
        value = value * 10u + digit;
    }
    *out = value;
    return 0;
}

static int read_file(const char *path, uint8_t **out, size_t *length, size_t limit)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    size_t got;
    int extra;
    int bad;
    int close_bad;
    if (file == NULL) return fail("cannot open input file");
    if (limit == SIZE_MAX) {
        (void)fclose(file);
        return fail("input bound is too large");
    }
    data = malloc(limit + 1u);
    if (data == NULL) {
        (void)fclose(file);
        return fail("cannot allocate input buffer");
    }
    got = fread(data, 1u, limit, file);
    extra = fgetc(file);
    bad = ferror(file) != 0 || extra != EOF;
    close_bad = fclose(file) != 0;
    if (close_bad != 0) bad = 1;
    if (bad != 0) {
        free(data);
        return fail("input exceeds its bound or contains NUL");
    }
    data[got] = '\0';
    *out = data;
    *length = got;
    return 0;
}

static int read_text(const char *path, char **out, size_t *length, size_t limit)
{
    uint8_t *bytes = NULL;
    if (read_file(path, &bytes, length, limit) != 0) return 1;
    if (memchr(bytes, '\0', *length) != NULL) {
        free(bytes);
        return fail("text input contains NUL");
    }
    bytes[*length] = '\0';
    *out = (char *)bytes;
    return 0;
}

static int next_line(const char *text, size_t length, size_t *offset,
                     char *line, size_t capacity)
{
    size_t start = *offset;
    size_t end = start;
    size_t count;
    while (end < length && text[end] != '\n') end++;
    if (end == length || end == start || end - start >= capacity) return 1;
    count = end - start;
    if (memchr(text + start, '\r', count) != NULL) return 1;
    memcpy(line, text + start, count);
    line[count] = '\0';
    *offset = end + 1u;
    return 0;
}

static size_t split_fields(char *line, char **fields, size_t maximum)
{
    size_t count = 0u;
    char *cursor = line;
    if (line[0] == '\0') return 0u;
    fields[count++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == '\t') {
            *cursor = '\0';
            if (cursor[1] == '\0' || count == maximum) return 0u;
            fields[count++] = cursor + 1;
        }
        cursor++;
    }
    return count;
}

static int copy_field(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (length == 0u || length >= capacity) return 1;
    memcpy(destination, source, length + 1u);
    return 0;
}

static int parse_byte(const char *text, uint8_t *value)
{
    int high;
    int low;
    if (strlen(text) != 2u || (high = hex_digit((unsigned char)text[0])) < 0 ||
        (low = hex_digit((unsigned char)text[1])) < 0) return 1;
    *value = (uint8_t)((unsigned)high * 16u + (unsigned)low);
    return 0;
}

static int oracle_line(const char *text, size_t length, size_t *offset,
                       char *line, char **fields, size_t *field_count)
{
    if (next_line(text, length, offset, line, ORACLE_LINE_MAX + 1u) != 0) return 1;
    *field_count = split_fields(line, fields, 12u);
    return *field_count == 0u ? 1 : 0;
}

static int parse_oracle(const char *path, suite_oracle *out)
{
    static const char *const metadata_names[7] = {
        "archive", "revision", "cpu_main", "cpu_asm",
        "timers_main", "timers_asm", "license"
    };
    static const size_t metadata_lengths[7] = {64u, 40u, 64u, 64u, 64u, 64u, 64u};
    uint8_t metadata_seen[7] = {0u};
    uint8_t tuple_seen[2] = {0u, 0u};
    uint8_t suite_seen[2] = {0u, 0u};
    uint8_t result_seen[2][RESULT_EXTENT] = {{0u}};
    uint8_t result_counts[2] = {0u, 0u};
    uint8_t active = 0u;
    uint8_t section = 0u;
    uint8_t *bytes = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    size_t i;
    char line[ORACLE_LINE_MAX + 1u];
    char *fields[12];
    size_t count;
    char *destinations[7];
    size_t capacities[7];

    memset(out, 0, sizeof *out);
    destinations[0] = out->archive;
    destinations[1] = out->revision;
    destinations[2] = out->cpu_main;
    destinations[3] = out->cpu_asm;
    destinations[4] = out->timers_main;
    destinations[5] = out->timers_asm;
    destinations[6] = out->license;
    capacities[0] = sizeof out->archive;
    capacities[1] = sizeof out->revision;
    capacities[2] = sizeof out->cpu_main;
    capacities[3] = sizeof out->cpu_asm;
    capacities[4] = sizeof out->timers_main;
    capacities[5] = sizeof out->timers_asm;
    capacities[6] = sizeof out->license;
    if (read_file(path, &bytes, &length, 16384u) != 0) return 1;
    if (memchr(bytes, '\0', length) != NULL) {
        free(bytes);
        return fail("oracle: unexpected NUL byte");
    }
    if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
        count != 2u || strcmp(fields[0], "LYNX-SUITE-ORACLE") != 0 ||
        strcmp(fields[1], "1") != 0) {
        free(bytes);
        return fail("oracle: invalid version header");
    }
    for (i = 0u; i < 7u; i++) {
        if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], metadata_names[i]) != 0 ||
            strlen(fields[1]) != metadata_lengths[i] ||
            capacities[i] < metadata_lengths[i] + 1u ||
            copy_field(destinations[i], capacities[i], fields[1]) != 0) {
            free(bytes);
            return fail("oracle: malformed source identity record");
        }
        metadata_seen[i] = 1u;
    }
    if (metadata_seen[0] == 0u || metadata_seen[1] == 0u ||
        !valid_hex(out->archive, 64u) || !valid_hex(out->revision, 40u) ||
        !valid_hex(out->cpu_main, 64u) || !valid_hex(out->cpu_asm, 64u) ||
        !valid_hex(out->timers_main, 64u) || !valid_hex(out->timers_asm, 64u) ||
        !valid_hex(out->license, 64u)) {
        free(bytes);
        return fail("oracle: malformed source identity");
    }
    for (section = 0u; section < 2u; section++) {
        const char *name = section == 0u ? "cpu" : "timers";
        uint32_t expected_meaningful = section == 0u ? CPU_MEANINGFUL : 16u;
        uint32_t value;
        if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "suite") != 0 ||
            strcmp(fields[1], name) != 0) {
            free(bytes);
            return fail("oracle: missing or reordered suite section");
        }
        suite_seen[section] = 1u;
        if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "result_count") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != RESULT_EXTENT) {
            free(bytes);
            return fail("oracle: result count drift");
        }
        if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "extent") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != RESULT_EXTENT) {
            free(bytes);
            return fail("oracle: result extent mismatch");
        }
        if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "meaningful") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != expected_meaningful) {
            free(bytes);
            return fail("oracle: meaningful result count mismatch");
        }
        if (section == 0u) {
            unsigned model;
            for (model = 0u; model < 2u; model++) {
                const char *model_name = model == 0u ? "lynx1" : "lynx2";
                unsigned byte;
                if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
                    count != 3u || strcmp(fields[0], "tuple") != 0 ||
                    strcmp(fields[1], model_name) != 0) {
                    free(bytes);
                    return fail("oracle: missing CPU model tuple");
                }
                {
                    char *cursor = fields[2];
                    for (byte = 0u; byte < 4u; byte++) {
                        char token[3];
                        char *comma = strchr(cursor, ',');
                        size_t token_len = comma == NULL ? strlen(cursor) : (size_t)(comma - cursor);
                        if (token_len != 2u || (byte == 3u && comma != NULL)) {
                            free(bytes);
                            return fail("oracle: malformed CPU model tuple");
                        }
                        token[0] = cursor[0];
                        token[1] = cursor[1];
                        token[2] = '\0';
                        if (parse_byte(token, &out->tuple[model][byte]) != 0) {
                            free(bytes);
                            return fail("oracle: malformed CPU model tuple");
                        }
                        if (byte < 3u) {
                            if (comma == NULL) {
                                free(bytes);
                                return fail("oracle: truncated CPU model tuple");
                            }
                            cursor = comma + 1;
                        }
                    }
                    if (byte == 4u && strchr(cursor, ',') != NULL) {
                        free(bytes);
                        return fail("oracle: malformed CPU model tuple");
                    }
                }
                tuple_seen[model] = 1u;
            }
        }
        for (i = 0u; i < expected_meaningful; i++) {
            uint32_t index;
            uint32_t accepted;
            size_t j;
            if (oracle_line((char *)bytes, length, &offset, line, fields, &count) != 0 ||
                count < 4u || strcmp(fields[0], "result") != 0 ||
                parse_decimal(fields[1], &index) != 0 || index != i ||
                parse_decimal(fields[2], &accepted) != 0 ||
                accepted < 1u || accepted > EXPECT_VALUES_MAX ||
                count != (size_t)(3u + accepted)) {
                free(bytes);
                return fail("oracle: malformed result row");
            }
            if (result_seen[section][index] != 0u) {
                free(bytes);
                return fail("oracle: duplicate result offset");
            }
            result_seen[section][index] = 1u;
            result_counts[section] = (uint8_t)(result_counts[section] + 1u);
            out->counts[section][index] = (uint8_t)accepted;
            for (j = 0u; j < accepted; j++) {
                size_t k;
                if (parse_byte(fields[3u + j], &out->values[section][index][j]) != 0) {
                    free(bytes);
                    return fail("oracle: malformed result byte");
                }
                for (k = 0u; k < j; k++) {
                    if (out->values[section][index][k] == out->values[section][index][j]) {
                        free(bytes);
                        return fail("oracle: duplicate accepted alternative");
                    }
                }
            }
        }
        active = (uint8_t)section;
    }
    (void)active;
    if (offset != length || suite_seen[0] == 0u || suite_seen[1] == 0u ||
        tuple_seen[0] == 0u || tuple_seen[1] == 0u ||
        result_counts[0] != 18u || result_counts[1] != 16u) {
        free(bytes);
        return fail("oracle: trailing data or incomplete result inventory");
    }
    for (i = 0u; i < 4u; i++) {
        size_t j;
        int found_i = 0;
        int found_ii = 0;
        for (j = 0u; j < out->counts[0][12u + i]; j++) {
            if (out->values[0][12u + i][j] == out->tuple[0][i]) found_i = 1;
            if (out->values[0][12u + i][j] == out->tuple[1][i]) found_ii = 1;
        }
        if (out->counts[0][12u + i] != 2u || found_i == 0 || found_ii == 0) {
            free(bytes);
            return fail("oracle: CPU tuple alternatives were lost");
        }
    }
    if (out->counts[1][8] != 2u || out->counts[1][11] != 3u) {
        free(bytes);
        return fail("oracle: timer accepted alternatives were lost");
    }
    free(bytes);
    return 0;
}

static int parse_symbol_address(const char *text, uint32_t *value)
{
    uint32_t result = 0u;
    size_t i;
    size_t length = strlen(text);
    if (length == 0u || length > 6u) return 1;
    for (i = 0u; i < length; i++) {
        unsigned digit;
        unsigned char ch = (unsigned char)text[i];
        if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') digit = (unsigned)(ch - (unsigned char)'0');
        else if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') digit = (unsigned)(ch - (unsigned char)'a') + 10u;
        else if (ch >= (unsigned char)'A' && ch <= (unsigned char)'F') digit = (unsigned)(ch - (unsigned char)'A') + 10u;
        else return 1;
        result = (result << 4) | digit;
    }
    if (result > LYNX_ADDRESS_SPACE) return 1;
    *value = result;
    return 0;
}

static int symbol_target(const char *name)
{
    size_t i;
    if (name[0] == '.') name++;
    for (i = 0u; i < 14u; i++) if (strcmp(name, symbol_names[i]) == 0) return (int)i;
    return -1;
}

static int valid_label_name(const char *name)
{
    size_t i = 0u;
    if (name[0] == '.') i++;
    if (name[i] == '\0' ||
        (name[i] != '_' && (name[i] < 'A' || name[i] > 'Z') &&
         (name[i] < 'a' || name[i] > 'z'))) return 0;
    for (; name[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (ch != '_' && (ch < (unsigned char)'A' || ch > (unsigned char)'Z') &&
            (ch < (unsigned char)'a' || ch > (unsigned char)'z') &&
            (ch < (unsigned char)'0' || ch > (unsigned char)'9')) return 0;
    }
    return 1;
}

static int parse_symbols(const char *text, size_t length, suite_symbols *out)
{
    size_t offset = 0u;
    char line[SYMBOL_LINE_MAX + 1];
    memset(out, 0, sizeof *out);
    while (offset < length) {
        char tokens[4][SYMBOL_LINE_MAX + 1];
        size_t token_count = 0u;
        size_t cursor = 0u;
        int relevant = -1;
        uint32_t address;
        size_t line_len = 0u;
        while (offset + line_len < length && text[offset + line_len] != '\n') line_len++;
        if (line_len > SYMBOL_LINE_MAX || line_len == 0u) return fail("symbol: malformed line length");
        memcpy(line, text + offset, line_len);
        line[line_len] = '\0';
        offset += line_len + (offset + line_len < length ? 1u : 0u);
        while (cursor < line_len) {
            size_t start;
            while (cursor < line_len && isspace((unsigned char)line[cursor]) != 0) cursor++;
            if (cursor == line_len) break;
            start = cursor;
            while (cursor < line_len && isspace((unsigned char)line[cursor]) == 0) cursor++;
            if (token_count < 4u) {
                size_t token_len = cursor - start;
                if (token_len >= sizeof tokens[0]) return fail("symbol: malformed token length");
                memcpy(tokens[token_count], line + start, token_len);
                tokens[token_count][token_len] = '\0';
            }
            token_count++;
        }
        if (token_count != 3u || strcmp(tokens[0], "al") != 0 ||
            parse_symbol_address(tokens[1], &address) != 0 ||
            valid_label_name(tokens[2]) == 0) return fail("symbol: malformed ld65 label row");
        relevant = symbol_target(tokens[2]);
        if (relevant < 0) continue;
        if (out->found[relevant] != 0u) return fail("symbol: duplicate required label");
        out->found[relevant] = 1u;
        out->value[relevant] = address;
    }
    for (offset = 0u; offset < 12u; offset++) {
        if (out->found[offset] == 0u) {
            char message[96];
            (void)snprintf(message, sizeof message, "symbol missing: %s", symbol_names[offset]);
            return fail(message);
        }
    }
    return 0;
}

static int ranges_valid(const suite_symbols *symbols, uint32_t extent)
{
    uint32_t code_start = symbols->value[0];
    uint32_t code_end = symbols->value[7];
    uint32_t result_start = symbols->value[2];
    uint32_t result_end = symbols->value[3];
    uint32_t data[4] = {symbols->value[4], symbols->value[5],
                        symbols->value[6], symbols->value[11]};
    size_t i;
    size_t j;
    if (code_start >= code_end || code_end > LYNX_ADDRESS_SPACE ||
        symbols->value[1] < code_start || symbols->value[1] >= code_end ||
        symbols->value[8] < code_start || symbols->value[8] >= code_end ||
        symbols->value[9] < code_start || symbols->value[9] >= code_end ||
        symbols->value[10] < code_start || symbols->value[10] >= code_end ||
        (symbols->found[12] != 0u &&
         (symbols->value[12] < code_start || symbols->value[12] >= code_end)) ||
        (symbols->found[13] != 0u &&
         (symbols->value[13] < code_start || symbols->value[13] >= code_end))) {
        return fail("symbol: code extent invalid");
    }
    if (result_start > LYNX_ADDRESS_SPACE - extent ||
        result_end != result_start + extent ||
        (result_start < code_end && code_start < result_end)) {
        return fail("symbol: result extent or code overlap");
    }
    if (data[0] >= LYNX_ADDRESS_SPACE || data[1] >= LYNX_ADDRESS_SPACE ||
        data[2] >= LYNX_ADDRESS_SPACE || data[3] >= 0x0100u) {
        return fail("symbol: byte range invalid");
    }
    for (i = 0u; i < 4u; i++) {
        if (data[i] >= result_start && data[i] < result_end) return fail("symbol: result and completion overlap");
        for (j = 0u; j < i; j++) if (data[i] == data[j]) return fail("symbol: completion symbols overlap");
    }
    return 0;
}

static int file_sha256(const char *path, char hex[65])
{
    uint8_t *bytes = NULL;
    size_t length = 0u;
    lynx__sha256 state;
    uint8_t digest[32];
    if (read_file(path, &bytes, &length, 16777216u) != 0) return 1;
    lynx__sha256_init(&state);
    lynx__sha256_update(&state, bytes, length);
    lynx__sha256_final(&state, digest);
    lynx__sha256_hex(digest, hex);
    free(bytes);
    return 0;
}

static int record_value(const char *path, const char *key, char *value, size_t capacity)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    char line[256];
    if (read_text(path, &text, &length, 16384u) != 0) return 1;
    while (offset < length) {
        size_t line_len = 0u;
        char *equals;
        while (offset + line_len < length && text[offset + line_len] != '\n') line_len++;
        if (line_len >= sizeof line || line_len == 0u) {
            free(text);
            return fail("build record malformed");
        }
        memcpy(line, text + offset, line_len);
        line[line_len] = '\0';
        offset += line_len + (offset + line_len < length ? 1u : 0u);
        equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0' || strchr(equals + 1, '=') != NULL) {
            free(text);
            return fail("build record malformed");
        }
        *equals = '\0';
        if (strcmp(line, key) == 0) {
            if (found++ != 0u || copy_field(value, capacity, equals + 1) != 0) {
                free(text);
                return fail("build record duplicate or oversized field");
            }
        }
    }
    free(text);
    if (found != 1u) return fail("build record missing identity field");
    return 0;
}

typedef struct json_cursor {
    const char *text;
    size_t length;
    size_t offset;
} json_cursor;

typedef struct json_spec {
    const char *key;
    json_kind kind;
} json_spec;

static void json_space(json_cursor *cursor)
{
    while (cursor->offset < cursor->length &&
           (cursor->text[cursor->offset] == ' ' || cursor->text[cursor->offset] == '\n' ||
            cursor->text[cursor->offset] == '\r' || cursor->text[cursor->offset] == '\t')) {
        cursor->offset++;
    }
}

static int json_string(json_cursor *cursor, char *out, size_t capacity)
{
    size_t start;
    size_t count;
    if (cursor->offset >= cursor->length || cursor->text[cursor->offset] != '"') return 1;
    start = ++cursor->offset;
    while (cursor->offset < cursor->length && cursor->text[cursor->offset] != '"') {
        const unsigned char ch = (unsigned char)cursor->text[cursor->offset];
        if (ch < 0x20u || ch == (unsigned char)'\\') return 1;
        cursor->offset++;
    }
    if (cursor->offset >= cursor->length) return 1;
    count = cursor->offset - start;
    if (count == 0u || count >= capacity) return 1;
    memcpy(out, cursor->text + start, count);
    out[count] = '\0';
    cursor->offset++;
    return 0;
}

static int json_spec_index(const json_spec *specs, size_t spec_count, const char *key)
{
    size_t i;
    for (i = 0u; i < spec_count; i++) if (strcmp(specs[i].key, key) == 0) return (int)i;
    return -1;
}

static int json_parse_value(json_cursor *cursor, json_field *field)
{
    json_space(cursor);
    if (cursor->offset >= cursor->length) return 1;
    if (cursor->text[cursor->offset] == '"') {
        field->kind = JSON_STRING;
        return json_string(cursor, field->string, sizeof field->string);
    }
    if (cursor->text[cursor->offset] == '[') {
        field->kind = JSON_STRING_ARRAY;
        cursor->offset++;
        json_space(cursor);
        if (cursor->offset < cursor->length && cursor->text[cursor->offset] == ']') {
            cursor->offset++;
            return 0;
        }
        for (;;) {
            if (field->item_count >= JSON_ARRAY_MAX ||
                json_string(cursor, field->items[field->item_count], sizeof field->items[0]) != 0) return 1;
            field->item_count++;
            json_space(cursor);
            if (cursor->offset >= cursor->length) return 1;
            if (cursor->text[cursor->offset] == ']') {
                cursor->offset++;
                return 0;
            }
            if (cursor->text[cursor->offset] != ',') return 1;
            cursor->offset++;
            json_space(cursor);
        }
    }
    if (cursor->text[cursor->offset] == 't' && cursor->length - cursor->offset >= 4u &&
        memcmp(cursor->text + cursor->offset, "true", 4u) == 0) {
        field->kind = JSON_BOOLEAN;
        field->boolean = 1u;
        cursor->offset += 4u;
        return 0;
    }
    if (cursor->text[cursor->offset] == 'f' && cursor->length - cursor->offset >= 5u &&
        memcmp(cursor->text + cursor->offset, "false", 5u) == 0) {
        field->kind = JSON_BOOLEAN;
        field->boolean = 0u;
        cursor->offset += 5u;
        return 0;
    }
    if (cursor->text[cursor->offset] >= '0' && cursor->text[cursor->offset] <= '9') {
        char number[32];
        size_t start = cursor->offset;
        size_t count;
        while (cursor->offset < cursor->length && cursor->text[cursor->offset] >= '0' &&
               cursor->text[cursor->offset] <= '9') cursor->offset++;
        count = cursor->offset - start;
        if (count == 0u || count >= sizeof number) return 1;
        memcpy(number, cursor->text + start, count);
        number[count] = '\0';
        field->kind = JSON_NUMBER;
        return parse_decimal(number, &field->number);
    }
    return 1;
}

static int json_parse_file(const char *path, const json_spec *specs, size_t spec_count,
                           json_object *out, const char *kind)
{
    char *text = NULL;
    size_t length = 0u;
    json_cursor cursor;
    if (read_text(path, &text, &length, 32768u) != 0) {
        if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor malformed JSON");
        return fail("suite: pending record malformed JSON");
    }
    memset(out, 0, sizeof *out);
    cursor.text = text;
    cursor.length = length;
    cursor.offset = 0u;
    json_space(&cursor);
    if (cursor.offset >= cursor.length || cursor.text[cursor.offset++] != '{') goto malformed;
    json_space(&cursor);
    if (cursor.offset < cursor.length && cursor.text[cursor.offset] == '}') cursor.offset++;
    else for (;;) {
        json_field *field;
        int spec_index;
        size_t i;
        if (out->count >= JSON_MAX_FIELDS || out->count >= spec_count) goto unknown;
        field = &out->fields[out->count];
        if (json_string(&cursor, field->key, sizeof field->key) != 0) goto malformed;
        for (i = 0u; i < out->count; i++) {
            if (strcmp(out->fields[i].key, field->key) == 0) {
                free(text);
                if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor duplicate key");
                return fail("suite: pending record duplicate key");
            }
        }
        spec_index = json_spec_index(specs, spec_count, field->key);
        if (spec_index < 0) {
            free(text);
            if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor unknown key");
            return fail("suite: pending record unknown key");
        }
        json_space(&cursor);
        if (cursor.offset >= cursor.length || cursor.text[cursor.offset++] != ':') goto malformed;
        if (json_parse_value(&cursor, field) != 0) goto malformed;
        if (field->kind != specs[(size_t)spec_index].kind) {
            free(text);
            if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor invalid field type");
            return fail("suite: pending record invalid field type");
        }
        out->count++;
        json_space(&cursor);
        if (cursor.offset >= cursor.length) goto malformed;
        if (cursor.text[cursor.offset] == '}') {
            cursor.offset++;
            break;
        }
        if (cursor.text[cursor.offset++] != ',') goto malformed;
        json_space(&cursor);
    }
    json_space(&cursor);
    if (cursor.offset != cursor.length) goto malformed;
    free(text);
    return 0;
unknown:
    free(text);
    if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor unknown key");
    return fail("suite: pending record unknown key");
malformed:
    free(text);
    if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor malformed JSON");
    return fail("suite: pending record malformed JSON");
}

static const json_field *json_find(const json_object *object, const char *key)
{
    size_t i;
    for (i = 0u; i < object->count; i++) if (strcmp(object->fields[i].key, key) == 0) return &object->fields[i];
    return NULL;
}

static int json_missing_required(const json_object *object, const json_spec *specs, size_t spec_count,
                                const char *kind)
{
    size_t i;
    for (i = 0u; i < spec_count; i++) {
        if (json_find(object, specs[i].key) == NULL) {
            if (strcmp(kind, "descriptor") == 0 &&
                (strcmp(specs[i].key, "schema") == 0 || strcmp(specs[i].key, "version") == 0)) {
                return fail("pending descriptor missing schema/version");
            }
            if (strcmp(kind, "descriptor") == 0) return fail("pending descriptor missing key");
            return fail("suite: pending record missing key");
        }
    }
    return 0;
}

static int json_string_is(const json_object *object, const char *key, const char *expected)
{
    const json_field *field = json_find(object, key);
    return field != NULL && field->kind == JSON_STRING && strcmp(field->string, expected) == 0;
}

static int json_number_is(const json_object *object, const char *key, uint32_t expected)
{
    const json_field *field = json_find(object, key);
    return field != NULL && field->kind == JSON_NUMBER && field->number == expected;
}

static int json_boolean_is(const json_object *object, const char *key, uint8_t expected)
{
    const json_field *field = json_find(object, key);
    return field != NULL && field->kind == JSON_BOOLEAN && field->boolean == expected;
}

static int json_array_is(const json_object *object, const char *key,
                         const char *const *items, size_t item_count)
{
    const json_field *field = json_find(object, key);
    size_t i;
    if (field == NULL || field->kind != JSON_STRING_ARRAY || field->item_count != item_count) return 0;
    for (i = 0u; i < item_count; i++) if (strcmp(field->items[i], items[i]) != 0) return 0;
    return 1;
}

static int read_build_identity(const char *directory, const suite_oracle *oracle)
{
    char path[4096];
    char value[128];
    if (snprintf(path, sizeof path, "%s/build-record.txt", directory) >= (int)sizeof path) {
        return fail("build record path too long");
    }
#define CHECK_RECORD(key, expected) \
    do { \
        if (record_value(path, (key), value, sizeof value) != 0 || strcmp(value, (expected)) != 0) \
            return fail("oracle: source identity mismatch"); \
    } while (0)
    CHECK_RECORD("lynx_tests_pin", oracle->archive);
    CHECK_RECORD("lynx_tests_revision", oracle->revision);
    CHECK_RECORD("cpu_main_sha256", oracle->cpu_main);
    CHECK_RECORD("cpu_source_sha256", oracle->cpu_asm);
    CHECK_RECORD("timers_main_sha256", oracle->timers_main);
    CHECK_RECORD("timers_source_sha256", oracle->timers_asm);
    CHECK_RECORD("license_sha256", oracle->license);
#undef CHECK_RECORD
    return 0;
}

static const json_spec pending_descriptor_specs[] = {
    {"schema", JSON_STRING}, {"version", JSON_NUMBER}, {"status", JSON_STRING},
    {"decision", JSON_STRING}, {"requirement", JSON_STRING}, {"suite", JSON_STRING},
    {"source_index", JSON_NUMBER}, {"expected_source_set", JSON_STRING_ARRAY}, {"actual", JSON_STRING},
    {"profiles", JSON_STRING_ARRAY}, {"revisit_phase", JSON_NUMBER},
    {"lynx_tests_revision", JSON_STRING}, {"lynx_tests_archive_sha256", JSON_STRING},
    {"cc65_pin_sha256", JSON_STRING}, {"timers_tests_sha256", JSON_STRING},
    {"timers_main_sha256", JSON_STRING}, {"oracle_sha256", JSON_STRING},
    {"wrapper_sha256", JSON_STRING}, {"build_recipe_sha256", JSON_STRING},
    {"oracle_generator_sha256", JSON_STRING}, {"phase_policy", JSON_STRING},
    {"scheduler_policy", JSON_STRING}, {"cost_model", JSON_STRING},
    {"cost_ticks", JSON_NUMBER}, {"sample_offset_ticks", JSON_NUMBER},
    {"page_mode_video_enabled", JSON_BOOLEAN}, {"refresh_dma_enabled", JSON_BOOLEAN},
    {"counter_initialization", JSON_STRING}, {"proof_report_path", JSON_STRING},
    {"proof_report_sha256", JSON_STRING}
};

static int pending_descriptor_validate(const char *descriptor_path, const char *directory,
                                       const char *oracle_path)
{
    static const char *const expected_values[] = {"0d"};
    static const char *const expected_profiles[] = {"lynx1", "lynx2"};
    json_object descriptor;
    suite_oracle oracle;
    char root_path[4096];
    char path[4096];
    char source_tests[65], source_main[65], oracle_sha[65], wrapper_sha[65];
    char recipe_sha[65], generator_sha[65], report_sha[65], cc65_pin[65], value[128];
    int n;
    if (json_parse_file(descriptor_path, pending_descriptor_specs,
                        sizeof pending_descriptor_specs / sizeof pending_descriptor_specs[0],
                        &descriptor, "descriptor") != 0) return 1;
    if (json_missing_required(&descriptor, pending_descriptor_specs,
                              sizeof pending_descriptor_specs / sizeof pending_descriptor_specs[0],
                              "descriptor") != 0) return 1;
    if (!json_string_is(&descriptor, "schema", "lynxplosion.timer-source-pending") ||
        !json_number_is(&descriptor, "version", 1u)) return fail("pending descriptor unsupported schema/version");
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0) return 1;
#ifndef LYNX_PROJECT_ROOT
#error LYNX_PROJECT_ROOT must be defined for suite evidence
#endif
    n = snprintf(root_path, sizeof root_path, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT);
    if (n < 0 || (size_t)n >= sizeof root_path) return fail("pending descriptor value mismatch");
    n = snprintf(path, sizeof path, "%s/timers/tests.s", root_path);
    if (n < 0 || (size_t)n >= sizeof path || file_sha256(path, source_tests) != 0) return 1;
    n = snprintf(path, sizeof path, "%s/timers/main.c", root_path);
    if (n < 0 || (size_t)n >= sizeof path || file_sha256(path, source_main) != 0) return 1;
    n = snprintf(path, sizeof path, "%s/build-record.txt", directory);
    if (n < 0 || (size_t)n >= sizeof path ||
        record_value(path, "timers_wrapper_sha256", wrapper_sha, sizeof wrapper_sha) != 0 ||
        record_value(path, "cc65_pin", cc65_pin, sizeof cc65_pin) != 0) return 1;
    n = snprintf(path, sizeof path, "%s/scripts/build-suites.sh", LYNX_PROJECT_ROOT);
    if (n < 0 || (size_t)n >= sizeof path || file_sha256(path, recipe_sha) != 0) return 1;
    n = snprintf(path, sizeof path, "%s/scripts/generate-suite-oracle.sh", LYNX_PROJECT_ROOT);
    if (n < 0 || (size_t)n >= sizeof path || file_sha256(path, generator_sha) != 0) return 1;
    n = snprintf(path, sizeof path, "%s/%s", LYNX_PROJECT_ROOT,
                 ".planning/debug/evidence/timer-suite-cascade-count.md");
    if (n < 0 || (size_t)n >= sizeof path || file_sha256(path, report_sha) != 0) return 1;
#define PENDING_STRING(key, wanted) \
    do { if (!json_string_is(&descriptor, (key), (wanted))) return fail("pending descriptor value mismatch"); } while (0)
#define PENDING_NUMBER(key, wanted) \
    do { if (!json_number_is(&descriptor, (key), (wanted))) return fail("pending descriptor value mismatch"); } while (0)
#define PENDING_HASH(key, wanted) \
    do { if (!valid_hex((wanted), 64u) || !json_string_is(&descriptor, (key), (wanted))) return fail("pending descriptor value mismatch"); } while (0)
    PENDING_STRING("status", "pending");
    PENDING_STRING("decision", "D-10");
    PENDING_STRING("requirement", "EVD-02");
    PENDING_STRING("suite", "timers");
    PENDING_NUMBER("source_index", 14u);
    if (!json_array_is(&descriptor, "expected_source_set", expected_values, 1u) ||
        !json_array_is(&descriptor, "profiles", expected_profiles, 2u)) return fail("pending descriptor value mismatch");
    PENDING_STRING("actual", "0c");
    PENDING_NUMBER("revisit_phase", 5u);
    PENDING_STRING("lynx_tests_revision", oracle.revision);
    PENDING_HASH("lynx_tests_archive_sha256", oracle.archive);
    PENDING_HASH("cc65_pin_sha256", cc65_pin);
    PENDING_HASH("timers_tests_sha256", source_tests);
    PENDING_HASH("timers_main_sha256", source_main);
    PENDING_HASH("oracle_sha256", oracle_sha);
    PENDING_HASH("wrapper_sha256", wrapper_sha);
    PENDING_HASH("build_recipe_sha256", recipe_sha);
    PENDING_HASH("oracle_generator_sha256", generator_sha);
    PENDING_STRING("phase_policy", "timer-phase-fixed-zero-1");
    PENDING_STRING("scheduler_policy", "timer-cpu-poll-order-1");
    PENDING_STRING("cost_model", "flat5");
    PENDING_NUMBER("cost_ticks", 5u);
    PENDING_NUMBER("sample_offset_ticks", 0u);
    if (!json_boolean_is(&descriptor, "page_mode_video_enabled", 0u) ||
        !json_boolean_is(&descriptor, "refresh_dma_enabled", 0u)) return fail("pending descriptor value mismatch");
    PENDING_STRING("counter_initialization", "zero");
    PENDING_STRING("proof_report_path", ".planning/debug/evidence/timer-suite-cascade-count.md");
    PENDING_HASH("proof_report_sha256", report_sha);
#undef PENDING_STRING
#undef PENDING_NUMBER
#undef PENDING_HASH
    (void)value;
    return 0;
}

static int checked_join(char *out, size_t capacity, const char *directory, const char *file);
static const char *profile_label(unsigned profile);
static int profile_index(const char *name, unsigned *index);
static int probe_model(uint8_t probe, uint8_t marker);

static int timer_paths_make(timer_paths *paths, const char *directory, const char *profile)
{
    char name[64];
    if (snprintf(name, sizeof name, "raw-results-%s.tsv", profile) >= (int)sizeof name ||
        checked_join(paths->raw, sizeof paths->raw, directory, name) != 0) return fail("timer record path too long");
    if (snprintf(name, sizeof name, "run-record-%s.txt", profile) >= (int)sizeof name ||
        checked_join(paths->record, sizeof paths->record, directory, name) != 0) return fail("timer record path too long");
    if (snprintf(name, sizeof name, "comparison-%s.tsv", profile) >= (int)sizeof name ||
        checked_join(paths->comparison, sizeof paths->comparison, directory, name) != 0) return fail("timer record path too long");
    if (snprintf(name, sizeof name, "cascade-proof-%s.json", profile) >= (int)sizeof name ||
        checked_join(paths->proof, sizeof paths->proof, directory, name) != 0) return fail("timer record path too long");
    if (snprintf(name, sizeof name, "pending-record-%s.json", profile) >= (int)sizeof name ||
        checked_join(paths->companion, sizeof paths->companion, directory, name) != 0) return fail("timer record path too long");
    if (checked_join(paths->bll, sizeof paths->bll, directory, "timers.bll") != 0 ||
        checked_join(paths->labels, sizeof paths->labels, directory, "timers.lbl") != 0) return fail("timer record path too long");
    return 0;
}

static int read_timer_raw(const char *path, const char *profile, uint8_t results[TIMER_MEANINGFUL])
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[256];
    char *fields[4];
    size_t count;
    uint32_t i;
    if (read_text(path, &text, &length, 8192u) != 0) return fail("timer raw record unreadable");
#define RAW_ROW(expected_fields, expected_count) \
    do { \
        if (next_line(text, length, &offset, line, sizeof line) != 0 || \
            (count = split_fields(line, fields, 4u)) != (expected_count) || \
            strcmp(fields[0], (expected_fields)) != 0) { \
            free(text); return fail("timer raw record shape mismatch"); \
        } \
    } while (0)
    RAW_ROW("suite", 2u);
    if (strcmp(fields[1], "timers") != 0) { free(text); return fail("timer raw record identity mismatch"); }
    RAW_ROW("machine_profile", 2u);
    if (strcmp(fields[1], profile) != 0) { free(text); return fail("timer raw record identity mismatch"); }
    RAW_ROW("profile_input", 2u);
    if (strcmp(fields[1], profile) != 0) { free(text); return fail("timer raw record identity mismatch"); }
    RAW_ROW("ram_seed", 2u);
    if (strcmp(fields[1], "zero") != 0) { free(text); return fail("timer raw record identity mismatch"); }
    RAW_ROW("result_count", 2u);
    {
        uint32_t raw_count;
        if (parse_decimal(fields[1], &raw_count) != 0 || raw_count != TIMER_MEANINGFUL) {
            free(text); return fail("timer raw record count drift");
        }
    }
    for (i = 0u; i < TIMER_MEANINGFUL; i++) {
        uint32_t index;
        RAW_ROW("result", 3u);
        if (parse_decimal(fields[1], &index) != 0 || index != i || parse_byte(fields[2], &results[i]) != 0) {
            free(text); return fail("timer raw record count or byte malformed");
        }
    }
    if (offset != length) { free(text); return fail("timer raw record has extra rows"); }
#undef RAW_ROW
    free(text);
    return 0;
}

static int validate_timer_comparison_file(const char *path, const suite_oracle *oracle,
                                          const uint8_t results[TIMER_MEANINGFUL], uint32_t *agreements)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[256];
    char *fields[6];
    size_t count;
    uint32_t i;
    uint32_t matches = 0u;
    if (read_text(path, &text, &length, 8192u) != 0) return fail("timer comparison record unreadable");
    for (i = 0u; i < TIMER_MEANINGFUL; i++) {
        uint32_t index;
        uint8_t value;
        char expected[96];
        size_t j;
        int accepted = 0;
        size_t pos = 0u;
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 6u)) != 5u || strcmp(fields[0], "result") != 0 ||
            parse_decimal(fields[1], &index) != 0 || index != i ||
            parse_byte(fields[2], &value) != 0 || value != results[i]) {
            free(text); return fail("timer comparison record row mismatch");
        }
        if (snprintf(expected, sizeof expected, "EXPECT(") >= (int)sizeof expected) {
            free(text); return fail("timer comparison record malformed");
        }
        pos = strlen(expected);
        for (j = 0u; j < oracle->counts[1][i]; j++) {
            const int n = snprintf(expected + pos, sizeof expected - pos, "%s%02x", j == 0u ? "" : ",",
                                   oracle->values[1][i][j]);
            if (n < 0 || (size_t)n >= sizeof expected - pos) {
                free(text); return fail("timer comparison record malformed");
            }
            pos += (size_t)n;
            if (oracle->values[1][i][j] == results[i]) accepted = 1;
        }
        if (pos + 2u >= sizeof expected) { free(text); return fail("timer comparison record malformed"); }
        expected[pos++] = ')';
        expected[pos] = '\0';
        if (strcmp(fields[3], expected) != 0 ||
            strcmp(fields[4], accepted != 0 ? "source-accepted" : "source-mismatch") != 0) {
            free(text); return fail("timer comparison record row mismatch");
        }
        if (accepted != 0) matches++;
    }
    if (next_line(text, length, &offset, line, sizeof line) != 0 ||
        (count = split_fields(line, fields, 6u)) != 2u || strcmp(fields[0], "comparison") != 0 ||
        strcmp(fields[1], matches == TIMER_MEANINGFUL ? "match" : "mismatch") != 0 || offset != length) {
        free(text); return fail("timer comparison record summary mismatch");
    }
    free(text);
    *agreements = matches;
    return 0;
}

static int timer_record_field(const char *path, const char *key,
                              char *value, size_t capacity)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    if (read_text(path, &text, &length, 16384u) != 0) {
        char message[128];
        (void)snprintf(message, sizeof message, "timer record unreadable: %s", key);
        return fail(message);
    }
    while (offset < length) {
        size_t line_length = 0u;
        char line[256];
        char *equals;
        while (offset + line_length < length && text[offset + line_length] != '\n') line_length++;
        if (line_length == 0u || line_length >= sizeof line) {
            free(text);
            return fail("timer record malformed field line");
        }
        memcpy(line, text + offset, line_length);
        line[line_length] = '\0';
        offset += line_length + (offset + line_length < length ? 1u : 0u);
        equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0' || strchr(equals + 1, '=') != NULL) {
            free(text);
            return fail("timer record malformed field line");
        }
        *equals = '\0';
        if (strcmp(line, key) == 0) {
            if (found++ != 0u || copy_field(value, capacity, equals + 1) != 0) {
                free(text);
                return fail("timer record duplicate or oversized field");
            }
        }
    }
    free(text);
    if (found != 1u) {
        char message[128];
        (void)snprintf(message, sizeof message, "timer record missing field: %s", key);
        return fail(message);
    }
    return 0;
}

static int timer_record_is(const char *path, const char *key, const char *expected,
                           const char *diagnostic)
{
    char value[256];
    if (timer_record_field(path, key, value, sizeof value) != 0) return 1;
    return strcmp(value, expected) == 0 ? 0 : fail(diagnostic);
}

static int timer_record_number(const char *path, const char *key, uint32_t *out,
                               const char *diagnostic)
{
    char value[64];
    if (timer_record_field(path, key, value, sizeof value) != 0 || parse_decimal(value, out) != 0) {
        return fail(diagnostic);
    }
    return 0;
}

static int validate_timer_observation(const char *profile, const char *directory,
                                      const char *oracle_path, const char *descriptor_path,
                                      timer_paths *paths, suite_oracle *oracle_out,
                                      uint8_t results[TIMER_MEANINGFUL],
                                      char bll_sha[65], char labels_sha[65], char oracle_sha[65],
                                      uint32_t *ticks_out, uint32_t *agreements_out)
{
    char value[256];
    char expected_detected[8];
    char expected_address[64];
    char expected_model_address[64];
    char expected_rts_address[64];
    char expected_after_address[64];
    char build_record_path[4096];
    char manifest_path[4096];
    char *manifest = NULL;
    size_t manifest_length = 0u;
    uint32_t count, reserved, extent, tick_min, tick_max, ticks, guest_ticks;
    uint32_t margin, headroom, progress, agreements;
    unsigned profile_idx;
    uint8_t model_probe[2];
    char *comma;
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    suite_symbols symbols;
    char wrapper_sha[65];
    char actual_sha[65];
    int n;
    int status = 1;
    if (profile_index(profile, &profile_idx) != 0) return fail("timer record: invalid profile");
    if (pending_descriptor_validate(descriptor_path, directory, oracle_path) != 0 ||
        parse_oracle(oracle_path, oracle_out) != 0 || read_build_identity(directory, oracle_out) != 0 ||
        timer_paths_make(paths, directory, profile) != 0) return 1;
    if (file_sha256(oracle_path, oracle_sha) != 0 ||
        file_sha256(paths->bll, bll_sha) != 0 || file_sha256(paths->labels, labels_sha) != 0 ||
        read_file(paths->bll, &bll, &bll_length, 65536u) != 0 ||
        read_text(paths->labels, &labels, &labels_length, 16384u) != 0) goto done;
    if (parse_symbols(labels, labels_length, &symbols) != 0 || ranges_valid(&symbols, RESULT_EXTENT) != 0 ||
        symbols.found[12] == 0u || symbols.found[13] == 0u ||
        symbols.value[1] != symbols.value[8] || symbols.value[12] != symbols.value[8] + 26u ||
        symbols.value[12] + 1u != symbols.value[7] || symbols.value[12] < 0x01F7u ||
        symbols.value[12] - 0x01F6u >= bll_length ||
        bll[symbols.value[12] - 0x01F6u] != 0x60u ||
        bll[symbols.value[12] - 0x01F6u - 1u] != 0x58u ||
        symbols.value[13] < 0x01F9u || symbols.value[13] - 0x01F6u >= bll_length ||
        bll[symbols.value[13] - 0x01F6u - 3u] != 0x20u ||
        bll[symbols.value[13] - 0x01F6u - 2u] != (uint8_t)symbols.value[8] ||
        bll[symbols.value[13] - 0x01F6u - 1u] != (uint8_t)(symbols.value[8] >> 8)) {
        (void)fail("timer record: symbol or source-runner identity mismatch");
        goto done;
    }
    if (read_timer_raw(paths->raw, profile, results) != 0) goto done;
    {
        uint32_t index;
        int all_zero = 1;
        for (index = 0u; index < TIMER_MEANINGFUL; index++) {
            if (results[index] != 0u) all_zero = 0;
        }
        if (all_zero != 0) {
            (void)fail("timer record: zero/unexecuted result block");
            goto done;
        }
    }
    if (timer_record_is(paths->record, "suite", "timers", "timer record: run identity mismatch") != 0 ||
        timer_record_is(paths->record, "machine_profile", profile, "timer record: run identity mismatch") != 0 ||
        timer_record_is(paths->record, "profile_input", profile, "timer record: run identity mismatch") != 0 ||
        timer_record_is(paths->record, "ram_seed", "zero", "timer record: run identity mismatch") != 0 ||
        timer_record_is(paths->record, "model_probe_source", "fixtures/suites/timers-wrapper.s", "timer record: model probe identity mismatch") != 0) {
        goto done;
    }
    if (timer_record_is(paths->record, "bll_sha256", bll_sha, "timer record: BLL object identity mismatch") != 0 ||
        timer_record_is(paths->record, "labels_sha256", labels_sha, "timer record: labels identity mismatch") != 0 ||
        timer_record_is(paths->record, "oracle_sha256", oracle_sha, "timer record: oracle identity mismatch") != 0) goto done;
    if (timer_record_field(paths->record, "model_probe_source_sha256", value, sizeof value) != 0 ||
        timer_record_field(paths->record, "wrapper_sha256", wrapper_sha, sizeof wrapper_sha) != 0 ||
        !valid_hex(wrapper_sha, 64u) || strcmp(value, wrapper_sha) != 0) {
        (void)fail("timer record: wrapper identity mismatch");
        goto done;
    }
    if (
        timer_record_field(paths->record, "detected_model", value, sizeof value) != 0) {
        (void)fail("timer record: detected model field missing");
        goto done;
    }
    (void)snprintf(expected_detected, sizeof expected_detected, "%s", profile_label(profile_idx));
    if (strcmp(value, expected_detected) != 0 ||
        timer_record_field(paths->record, "model_probe_bytes", value, sizeof value) != 0 ||
        (comma = strchr(value, ',')) == NULL || (size_t)(comma - value) != 2u || strlen(comma + 1) != 2u) {
        (void)fail("timer record: model probe or selected profile mismatch");
        goto done;
    }
    *comma = '\0';
    if (parse_byte(value, &model_probe[0]) != 0 || parse_byte(comma + 1, &model_probe[1]) != 0 ||
        probe_model(model_probe[0], model_probe[1]) != (int)profile_idx) {
        (void)fail("timer record: model probe or selected profile mismatch");
        goto done;
    }
    if (timer_record_is(paths->record, "source_tuple_model", "not-applicable", "timer record: unexpected source model tuple") != 0 ||
        timer_record_is(paths->record, "meaningful_count", "16", "timer record: result count drift") != 0 ||
        timer_record_is(paths->record, "reserved_count", "2", "timer record: result count drift") != 0 ||
        timer_record_is(paths->record, "result_extent", "18", "timer record: result count drift") != 0 ||
        timer_record_is(paths->record, "minimum_guest_ticks", "256", "timer record: completion bound metadata mismatch") != 0 ||
        timer_record_is(paths->record, "maximum_guest_ticks", "50000000", "timer record: completion bound metadata mismatch") != 0 ||
        timer_record_is(paths->record, "meaningful_results", "16", "timer record: result count drift") != 0 ||
        timer_record_is(paths->record, "reserved_results", "2", "timer record: result count drift") != 0 ||
        timer_record_is(paths->record, "guest_probe_byte", profile_idx == 0u ? "01" : "00", "timer record: probe evidence mismatch") != 0 ||
        timer_record_is(paths->record, "guest_model_marker", profile_idx == 0u ? "a1" : "a2", "timer record: marker evidence mismatch") != 0 ||
        timer_record_is(paths->record, "guest_progress", "8", "timer record: guest progress evidence mismatch") != 0 ||
        timer_record_is(paths->record, "completion_sentinel", "a5", "timer record: completion sentinel mismatch") != 0 ||
        timer_record_is(paths->record, "scheduler_policy", "timer-cpu-poll-order-1", "timer record: policy metadata mismatch") != 0 ||
        timer_record_is(paths->record, "phase_policy", "timer-phase-fixed-zero-1", "timer record: policy metadata mismatch") != 0 ||
        timer_record_is(paths->record, "cost_model", "flat5", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "cost_ticks", "5", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "sample_offset_ticks", "0", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "page_mode_video_enabled", "false", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "refresh_dma_enabled", "false", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "counter_initialization", "zero", "timer record: environment metadata mismatch") != 0 ||
        timer_record_is(paths->record, "source_comparison", "separate-record", "timer record: comparison metadata mismatch") != 0 ||
        timer_record_is(paths->record, "hardware_accuracy", "unverified", "timer record: claim metadata mismatch") != 0 ||
        timer_record_is(paths->record, "profile_qualification", "unresolved", "timer record: claim metadata mismatch") != 0 ||
        timer_record_is(paths->record, "postamble_irq_policy", "mask-only-after-suite-return", "timer record: wrapper postamble metadata mismatch") != 0 ||
        timer_record_is(paths->record, "postamble_pending_irq", "08", "timer record: wrapper pending descriptor mismatch") != 0 ||
        timer_record_is(paths->record, "source_runner_rts_symbol", "suite_test_return@0459", "timer record: source-runner boundary mismatch") != 0 ||
        timer_record_is(paths->record, "postamble_return_symbol", "suite_after_tests@0248", "timer record: source-runner boundary mismatch") != 0) goto done;
    n = snprintf(expected_address, sizeof expected_address, "suite_probe_result@%04x", symbols.value[11]);
    if (n < 0 || (size_t)n >= sizeof expected_address) goto done;
    n = snprintf(expected_model_address, sizeof expected_model_address, "suite_model@%04x", symbols.value[5]);
    if (n < 0 || (size_t)n >= sizeof expected_model_address) goto done;
    n = snprintf(expected_rts_address, sizeof expected_rts_address, "suite_test_return@%04x", symbols.value[12]);
    if (n < 0 || (size_t)n >= sizeof expected_rts_address) goto done;
    n = snprintf(expected_after_address, sizeof expected_after_address, "suite_after_tests@%04x", symbols.value[13]);
    if (n < 0 || (size_t)n >= sizeof expected_after_address) goto done;
    if (timer_record_field(paths->record, "model_probe_symbol", value, sizeof value) != 0 ||
        strcmp(value, expected_address) != 0 ||
        timer_record_field(paths->record, "model_marker_symbol", value, sizeof value) != 0 ||
        strcmp(value, expected_model_address) != 0 ||
        timer_record_is(paths->record, "source_runner_rts_symbol", expected_rts_address, "timer record: source-runner boundary mismatch") != 0 ||
        timer_record_is(paths->record, "postamble_return_symbol", expected_after_address, "timer record: source-runner boundary mismatch") != 0) goto done;
    if (timer_record_number(paths->record, "meaningful_count", &count, "timer record: result count drift") != 0 ||
        timer_record_number(paths->record, "reserved_count", &reserved, "timer record: result count drift") != 0 ||
        timer_record_number(paths->record, "result_extent", &extent, "timer record: result count drift") != 0 ||
        timer_record_number(paths->record, "tick_min", &tick_min, "timer record: completion bound metadata mismatch") != 0 ||
        timer_record_number(paths->record, "tick_max", &tick_max, "timer record: completion bound metadata mismatch") != 0 ||
        timer_record_number(paths->record, "ticks_observed", &ticks, "timer record: invalid ticks_observed") != 0 ||
        timer_record_number(paths->record, "guest_ticks", &guest_ticks, "timer record: guest tick records disagree") != 0 ||
        timer_record_number(paths->record, "completion_margin_ticks", &margin, "timer record: completion margin mismatch") != 0 ||
        timer_record_number(paths->record, "maximum_headroom_ticks", &headroom, "timer record: maximum headroom mismatch") != 0 ||
        timer_record_number(paths->record, "guest_progress", &progress, "timer record: guest progress evidence mismatch") != 0) goto done;
    if (count != TIMER_MEANINGFUL || reserved != TIMER_RESERVED || extent != RESULT_EXTENT) {
        (void)fail("timer record: result count drift");
        goto done;
    }
    if (tick_min != TIMER_MIN_TICKS || tick_max != TIMER_MAX_TICKS) {
        (void)fail("timer record: completion bound metadata mismatch");
        goto done;
    }
    if (ticks < tick_min || ticks > tick_max) {
        (void)fail("timer record: ticks outside completion bounds");
        goto done;
    }
    if (guest_ticks != ticks) {
        (void)fail("timer record: guest tick records disagree");
        goto done;
    }
    if (margin != ticks - tick_min || headroom != tick_max - ticks) {
        (void)fail("timer record: completion margin mismatch");
        goto done;
    }
    if (progress != 8u) {
        (void)fail("timer record: guest progress evidence mismatch");
        goto done;
    }
    if (timer_record_field(paths->record, "oracle_sha256", value, sizeof value) != 0 || strcmp(value, oracle_sha) != 0) {
        (void)fail("timer record: oracle identity mismatch");
        goto done;
    }
    if (timer_record_field(paths->record, "bll_sha256", value, sizeof value) != 0 || strcmp(value, bll_sha) != 0) {
        (void)fail("timer record: BLL object identity mismatch");
        goto done;
    }
    if (timer_record_field(paths->record, "labels_sha256", value, sizeof value) != 0 || strcmp(value, labels_sha) != 0) {
        (void)fail("timer record: labels identity mismatch");
        goto done;
    }
    if (timer_record_field(paths->record, "wrapper_sha256", value, sizeof value) != 0 || strcmp(value, wrapper_sha) != 0 ||
        timer_record_field(paths->record, "model_probe_source_sha256", value, sizeof value) != 0 || strcmp(value, wrapper_sha) != 0) {
        (void)fail("timer record: wrapper identity mismatch");
        goto done;
    }
    if (checked_join(build_record_path, sizeof build_record_path, directory, "build-record.txt") != 0 ||
        file_sha256(paths->bll, actual_sha) != 0 || strcmp(actual_sha, bll_sha) != 0 ||
        file_sha256(paths->labels, actual_sha) != 0 || strcmp(actual_sha, labels_sha) != 0 ||
        file_sha256(oracle_path, actual_sha) != 0 || strcmp(actual_sha, oracle_sha) != 0 ||
        record_value(build_record_path, "timers_wrapper_sha256", value, sizeof value) != 0 ||
        strcmp(value, wrapper_sha) != 0) {
        (void)fail("timer record: source identity mismatch");
        goto done;
    }
    n = snprintf(manifest_path, sizeof manifest_path, "%s/fixtures/manifests/timer-tests.manifest", LYNX_PROJECT_ROOT);
    if (n < 0 || (size_t)n >= sizeof manifest_path || read_text(manifest_path, &manifest, &manifest_length, 8192u) != 0 ||
        strstr(manifest, wrapper_sha) == NULL) {
        (void)fail("timer record: wrapper manifest identity mismatch");
        goto done;
    }
    status = 0;
    if (read_timer_raw(paths->raw, profile, results) != 0 ||
        validate_timer_comparison_file(paths->comparison, oracle_out, results, &agreements) != 0) status = 1;
    else if (results[14] != 0x0Cu || oracle_out->counts[1][14] != 1u ||
             oracle_out->values[1][14][0] != 0x0Du || agreements != 15u) {
        status = fail("timer observation: source comparison is outside the exact pending row");
    }
    if (status == 0) {
        *ticks_out = ticks;
        *agreements_out = agreements;
    }
done:
    free(manifest);
    free(bll);
    free(labels);
    return status;
}

static int validate_timer_comparison_corpus(const char *profile, const char *directory,
                                            const char *raw_path, const char *comparison_path,
                                            const char *oracle_path, const char *descriptor_path)
{
    suite_oracle oracle;
    uint8_t results[TIMER_MEANINGFUL];
    uint32_t agreements;
    if (pending_descriptor_validate(descriptor_path, directory, oracle_path) != 0 ||
        parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0 ||
        read_timer_raw(raw_path, profile, results) != 0 ||
        validate_timer_comparison_file(comparison_path, &oracle, results, &agreements) != 0) return 1;
    if (results[14] != 0x0Cu || oracle.counts[1][14] != 1u ||
        oracle.values[1][14][0] != 0x0Du || agreements != 15u) {
        return fail("timer comparator corpus: alternatives or exact pending row mismatch");
    }
    printf("captured comparator corpus: 16 rows, 15 accepted, exact pending index14; not guest execution\n");
    return 0;
}

static int bll_byte_at(const uint8_t *bll, size_t length, uint32_t address, uint8_t *out)
{
    if (address < 0x01F6u || (size_t)(address - 0x01F6u) >= length) return 1;
    *out = bll[address - 0x01F6u];
    return 0;
}

static int bll_word_at(const uint8_t *bll, size_t length, uint32_t address, uint16_t *out)
{
    uint8_t lo, hi;
    if (bll_byte_at(bll, length, address, &lo) != 0 ||
        bll_byte_at(bll, length, address + 1u, &hi) != 0) return 1;
    *out = (uint16_t)((uint16_t)lo | (uint16_t)((uint16_t)hi << 8));
    return 0;
}

static int verify_source_control_bytes(const char *source_root, const char *directory,
                                       const suite_symbols *symbols, const uint8_t *bll,
                                       size_t bll_length, const char *oracle_path,
                                       const char *descriptor_path, const char *descriptor_sha,
                                       const timer_paths *paths)
{
    char tests_path[4096], main_path[4096], wrapper_path[4096];
    char *tests_text = NULL, *main_text = NULL, *wrapper_text = NULL;
    size_t tests_length = 0u, main_length = 0u, wrapper_length = 0u;
    char tests_sha[65], main_sha[65], wrapper_sha[65], actual_sha[65];
    char build_record_path[4096], value[128];
    uint32_t runner = symbols->value[8];
    uint32_t test6, poll;
    uint32_t code_start = symbols->value[0], code_end = symbols->value[7];
    uint32_t call_addr, occurrences = 0u, address;
    uint16_t word, reset_target;
    uint8_t byte, prefix[36], loop[17];
    static const uint8_t expected_loop[17] = {
        0xADu, 0x81u, 0xFDu, 0x29u, 0x20u, 0xF0u, 0x07u,
        0xA9u, 0x20u, 0x8Du, 0x80u, 0xFDu, 0xE6u, 0x01u,
        0xCAu, 0xD0u, 0xEFu
    };
    int n;
    if ((n = snprintf(tests_path, sizeof tests_path, "%s/timers/tests.s", source_root)) < 0 || (size_t)n >= sizeof tests_path ||
        (n = snprintf(main_path, sizeof main_path, "%s/timers/main.c", source_root)) < 0 || (size_t)n >= sizeof main_path ||
        (n = snprintf(wrapper_path, sizeof wrapper_path, "%s/fixtures/suites/timers-wrapper.s", LYNX_PROJECT_ROOT)) < 0 || (size_t)n >= sizeof wrapper_path) {
        return fail("cascade model: source path too long");
    }
    if (read_text(tests_path, &tests_text, &tests_length, 65536u) != 0 ||
        read_text(main_path, &main_text, &main_length, 65536u) != 0 ||
        read_text(wrapper_path, &wrapper_text, &wrapper_length, 16384u) != 0 ||
        file_sha256(tests_path, tests_sha) != 0 || file_sha256(main_path, main_sha) != 0 ||
        file_sha256(wrapper_path, wrapper_sha) != 0) goto bad;
    (void)tests_length;
    (void)main_length;
    (void)wrapper_length;
    if (strstr(tests_text, "ldx #$C0") == NULL || strstr(tests_text, "sta TIM5CTLA") == NULL ||
        strstr(tests_text, "sta TIM3BKUP") == NULL || strstr(tests_text, "sta TIM3CNT") == NULL ||
        strstr(tests_text, "sta TIM3CTLA") == NULL || strstr(tests_text, "and #TIMER5_INTERRUPT") == NULL ||
        strstr(tests_text, "sta INTRST") == NULL || strstr(tests_text, "inc t5_count") == NULL ||
        strstr(tests_text, "dex") == NULL || strstr(tests_text, "bne @poll_t5") == NULL ||
        strstr(main_text, "EXPECT(0x0D)") == NULL || strstr(main_text, "{ 0, 3, 5, 8, 12, 14, 15 }") == NULL ||
        strstr(wrapper_text, "sta $00,x") == NULL) {
        (void)fail("cascade model: pinned source instruction or initialization shape mismatch");
        goto bad;
    }
    if (snprintf(build_record_path, sizeof build_record_path, "%s/build-record.txt", directory) >= (int)sizeof build_record_path ||
        record_value(build_record_path, "timers_source_sha256", value, sizeof value) != 0 || strcmp(value, tests_sha) != 0 ||
        record_value(build_record_path, "timers_main_sha256", value, sizeof value) != 0 || strcmp(value, main_sha) != 0 ||
        record_value(build_record_path, "timers_wrapper_sha256", value, sizeof value) != 0 || strcmp(value, wrapper_sha) != 0 ||
        file_sha256(paths->bll, actual_sha) != 0 || record_value(build_record_path, "timers_object_sha256", value, sizeof value) != 0 || strcmp(value, actual_sha) != 0 ||
        file_sha256(paths->labels, actual_sha) != 0 || file_sha256(oracle_path, actual_sha) != 0 ||
        file_sha256(descriptor_path, actual_sha) != 0 || strcmp(actual_sha, descriptor_sha) != 0 ||
        file_sha256(paths->raw, actual_sha) != 0 || file_sha256(paths->record, actual_sha) != 0 ||
        file_sha256(paths->comparison, actual_sha) != 0) {
        (void)fail("cascade model: source/build/artifact identity mismatch");
        goto bad;
    }
    call_addr = runner + 16u;
    if (bll_byte_at(bll, bll_length, runner, &byte) != 0 || byte != 0x78u ||
        bll_byte_at(bll, bll_length, call_addr, &byte) != 0 || byte != 0x20u ||
        bll_word_at(bll, bll_length, call_addr + 1u, &word) != 0) {
        (void)fail("cascade model: pinned runner Test6 call is malformed");
        goto bad;
    }
    test6 = word;
    if (bll_byte_at(bll, bll_length, runner + 22u, &byte) != 0 || byte != 0x20u ||
        bll_word_at(bll, bll_length, runner + 23u, &word) != 0 ||
        bll_byte_at(bll, bll_length, runner + 25u, &byte) != 0 || byte != 0x58u ||
        bll_byte_at(bll, bll_length, runner + 26u, &byte) != 0 || byte != 0x60u) {
        (void)fail("cascade model: Test6 setup call or pinned runner geometry mismatch");
        goto bad;
    }
    if (bll_word_at(bll, bll_length, test6 + 1u, &reset_target) != 0 || reset_target != word) {
        fprintf(stderr, "cascade geometry: runner=%04x test6=%04x cleanup=%04x test6_reset=%04x\n",
                runner, test6, word, reset_target);
        (void)fail("cascade model: Test6 and runner cleanup do not share ResetTimers");
        goto bad;
    }
    /* The runner cleanup JSR is the same linked ResetTimers routine Test6 begins with. */
    {
        static const uint8_t setup_tail[33] = {
            0xA9u, 0xFFu, 0x8Du, 0x80u, 0xFDu, 0xA9u, 0x00u,
            0x8Du, 0x14u, 0xFDu, 0x8Du, 0x16u, 0xFDu,
            0xA9u, 0xCFu, 0x8Du, 0x15u, 0xFDu, 0xA2u, 0xC0u,
            0xA9u, 0x10u, 0x8Du, 0x0Cu, 0xFDu, 0x8Du, 0x0Eu,
            0xFDu, 0xA9u, 0x9Au, 0x8Du, 0x0Du, 0xFDu
        };
        uint32_t i;
        prefix[0] = 0x20u;
        prefix[1] = (uint8_t)word;
        prefix[2] = (uint8_t)(word >> 8);
        memcpy(prefix + 3u, setup_tail, sizeof setup_tail);
        for (i = 0u; i < sizeof prefix; i++) {
            if (bll_byte_at(bll, bll_length, test6 + i, &byte) != 0 || byte != prefix[i]) {
                (void)fail("cascade model: Test6 timer setup bytes or operands changed");
                goto bad;
            }
        }
    }
    poll = test6 + (uint32_t)sizeof prefix;
    for (address = 0u; address < sizeof loop; address++) {
        if (bll_byte_at(bll, bll_length, poll + address, &loop[address]) != 0 || loop[address] != expected_loop[address]) {
            (void)fail("cascade model: Test6 poll opcode, operand or addressing mode changed");
            goto bad;
        }
    }
    {
        const int32_t beq_offset = loop[6] < 0x80u ? (int32_t)loop[6] : (int32_t)loop[6] - 256;
        const int32_t bne_offset = loop[16] < 0x80u ? (int32_t)loop[16] : (int32_t)loop[16] - 256;
        const int32_t beq_next = (int32_t)(poll + 7u);
        const int32_t bne_next = (int32_t)(poll + 17u);
        const int32_t beq_target = beq_next + beq_offset;
        const int32_t bne_target = bne_next + bne_offset;
        if (beq_target != (int32_t)(poll + 14u) || bne_target != (int32_t)poll ||
            (((uint32_t)beq_next & 0xFF00u) != ((uint32_t)beq_target & 0xFF00u)) ||
            (((uint32_t)bne_next & 0xFF00u) != ((uint32_t)bne_target & 0xFF00u))) {
            (void)fail("cascade model: Test6 branch destination or page-cross geometry mismatch");
            goto bad;
        }
    }
    if (code_start >= code_end || test6 < code_start || poll + (uint32_t)sizeof loop > code_end) {
        (void)fail("cascade model: Test6 is outside linked code bounds");
        goto bad;
    }
    for (address = code_start; address + (uint32_t)sizeof loop <= code_end; address++) {
        uint32_t i;
        int equal = 1;
        for (i = 0u; i < sizeof loop; i++) {
            if (bll_byte_at(bll, bll_length, address + i, &byte) != 0 || byte != expected_loop[i]) {
                equal = 0;
                break;
            }
        }
        if (equal != 0) {
            occurrences++;
            if (address != poll) {
                (void)fail("cascade model: Test6 poll sequence is ambiguous in linked code");
                goto bad;
            }
        }
    }
    if (occurrences != 1u) {
        (void)fail("cascade model: Test6 poll sequence is missing or ambiguous");
        goto bad;
    }
    free(tests_text);
    free(main_text);
    free(wrapper_text);
    return 0;
bad:
    free(tests_text);
    free(main_text);
    free(wrapper_text);
    return 1;
}

static int cascade_proof_write(const char *profile, const char *directory,
                              const char *oracle_path, const char *descriptor_path,
                              const char *source_root, const char *proof_path)
{
    timer_paths paths;
    suite_oracle oracle;
    suite_symbols symbols;
    uint8_t results[TIMER_MEANINGFUL];
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char bll_sha[65], labels_sha[65], oracle_sha[65], descriptor_sha[65];
    char raw_sha[65], record_sha[65], comparison_sha[65], wrapper_sha[65];
    char tests_path[4096], main_path[4096], build_record[4096];
    char tests_sha[65], main_sha[65];
    uint32_t ticks, agreements, phase;
    uint16_t test6_address;
    uint32_t enable_to_first_poll_cycles = 4u;
    uint32_t first_poll, no_irq_cycles, irq_cycles;
    uint32_t no_irq_ticks, irq_ticks, poll_period = 17u * 64u;
    FILE *out = NULL;
    if (pending_descriptor_validate(descriptor_path, directory, oracle_path) != 0 ||
        parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0 ||
        timer_paths_make(&paths, directory, profile) != 0 ||
        validate_timer_observation(profile, directory, oracle_path, descriptor_path, &paths,
                                   &oracle, results, bll_sha, labels_sha, oracle_sha,
                                   &ticks, &agreements) != 0 || agreements != 15u || results[14] != 0x0Cu) {
        return fail("cascade proof: complete D-10 observation is missing or invalid");
    }
    if (file_sha256(descriptor_path, descriptor_sha) != 0 ||
        file_sha256(paths.raw, raw_sha) != 0 || file_sha256(paths.record, record_sha) != 0 ||
        file_sha256(paths.comparison, comparison_sha) != 0 ||
        read_file(paths.bll, &bll, &bll_length, 65536u) != 0) {
        free(bll);
        return fail("cascade proof: could not bind observed artifacts");
    }
    /* Read and parse labels separately so all linked addresses come from this BLL. */
    {
        char *labels = NULL;
        size_t labels_length = 0u;
        if (read_text(paths.labels, &labels, &labels_length, 16384u) != 0 ||
            parse_symbols(labels, labels_length, &symbols) != 0 ||
            ranges_valid(&symbols, RESULT_EXTENT) != 0) {
            free(labels);
            free(bll);
            return fail("cascade proof: linked symbols are invalid");
        }
        free(labels);
    }
    if (bll_word_at(bll, bll_length, symbols.value[8] + 17u, &test6_address) != 0) {
        free(bll);
        return fail("cascade proof: runner Test6 call target is unreadable");
    }
    if (snprintf(build_record, sizeof build_record, "%s/build-record.txt", directory) >= (int)sizeof build_record ||
        record_value(build_record, "timers_wrapper_sha256", wrapper_sha, sizeof wrapper_sha) != 0 ||
        snprintf(tests_path, sizeof tests_path, "%s/timers/tests.s", source_root) >= (int)sizeof tests_path ||
        snprintf(main_path, sizeof main_path, "%s/timers/main.c", source_root) >= (int)sizeof main_path ||
        file_sha256(tests_path, tests_sha) != 0 || file_sha256(main_path, main_sha) != 0) {
        free(bll);
        return fail("cascade proof: source identity path or hash invalid");
    }
    if (verify_source_control_bytes(source_root, directory, &symbols, bll, bll_length,
                                    oracle_path, descriptor_path, descriptor_sha, &paths) != 0 ||
        file_sha256(paths.bll, bll_sha) != 0 || file_sha256(paths.labels, labels_sha) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0) {
        free(bll);
        return 1;
    }
    free(bll);
    first_poll = enable_to_first_poll_cycles * 5u;
    no_irq_cycles = 4u + 2u + 3u + 2u + 3u;
    irq_cycles = 4u + 2u + 2u + 2u + 4u + 5u + 2u + 3u;
    no_irq_ticks = no_irq_cycles * 5u;
    irq_ticks = irq_cycles * 5u;
    if (first_poll != 20u || no_irq_cycles != 14u || irq_cycles != 24u ||
        no_irq_ticks != 70u || irq_ticks != 120u || poll_period != 1088u) {
        return fail("cascade proof: instruction cycle derivation mismatch");
    }
    out = fopen(proof_path, "wb");
    if (out == NULL) return fail("cascade proof: cannot create proof output");
    (void)fprintf(out,
        "{\n  \"schema\":\"lynxplosion.timer-cascade-proof\",\n  \"version\":1,\n"
        "  \"profile\":\"%s\",\n  \"policy\":\"timer-phase-fixed-zero-1\",\n"
        "  \"scheduler\":\"timer-cpu-poll-order-1\",\n  \"cost_ticks\":5,\n"
        "  \"sample_offset_ticks\":0,\n  \"source_index14_actual\":\"0c\",\n"
        "  \"source_agreements\":15,\n  \"observation_ticks\":%" PRIu32 ",\n"
        "  \"first_poll_offset_ticks\":%" PRIu32 ",\n  \"no_irq_cycles\":14,\n"
        "  \"irq_cycles\":24,\n  \"no_irq_interval_ticks\":%" PRIu32 ",\n"
        "  \"irq_interval_ticks\":%" PRIu32 ",\n  \"poll_count\":192,\n"
        "  \"timer3_reload_period_ticks\":%" PRIu32 ",\n  \"all_phases\":[\n",
        profile, ticks, first_poll, no_irq_ticks, irq_ticks, poll_period);
    for (phase = 1u; phase <= 64u; phase++) {
        uint32_t poll = first_poll;
        uint32_t expiry = phase + 16u * 64u;
        uint32_t acknowledgements = 0u;
        uint32_t i;
        for (i = 0u; i < 192u; i++) {
            int irq = poll >= expiry;
            if (irq != 0) {
                acknowledgements++;
                expiry += poll_period;
            }
            if (i != 191u) poll += irq != 0 ? irq_ticks : no_irq_ticks;
        }
        if (acknowledgements != 12u || poll != 13990u || expiry != phase + 16u * 64u + 12u * poll_period) {
            fclose(out);
            (void)remove(proof_path);
            return fail("cascade proof: phase enumeration did not derive the pinned 12-count path");
        }
        (void)fprintf(out,
            "    {\"first_edge_delay_ticks\":%" PRIu32 ",\"acknowledgements\":%" PRIu32
            ",\"final_poll_ticks\":%" PRIu32 ",\"expiry13_ticks\":%" PRIu32 "}%s\n",
            phase, acknowledgements, poll, expiry, phase == 64u ? "" : ",");
    }
    (void)fprintf(out,
        "  ],\n  \"source_tests_sha256\":\"%s\",\n  \"source_main_sha256\":\"%s\",\n"
        "  \"bll_sha256\":\"%s\",\n  \"labels_sha256\":\"%s\",\n"
        "  \"oracle_sha256\":\"%s\",\n  \"descriptor_sha256\":\"%s\",\n"
        "  \"raw_sha256\":\"%s\",\n  \"record_sha256\":\"%s\",\n"
        "  \"comparison_sha256\":\"%s\",\n  \"wrapper_sha256\":\"%s\",\n"
        "  \"test6_address\":\"%04x\",\n  \"poll_address\":\"%04x\",\n"
        "  \"zero_page_counter_operand\":\"01\",\n  \"hardware_accuracy\":\"unverified\",\n"
        "  \"profile_qualification\":\"unresolved\"\n}\n",
        tests_sha, main_sha, bll_sha, labels_sha, oracle_sha, descriptor_sha,
        raw_sha, record_sha, comparison_sha, wrapper_sha, (unsigned)test6_address,
        (unsigned)test6_address + 36u);
    if (fclose(out) != 0) return fail("cascade proof: cannot finish proof output");
    return 0;
}

static const json_spec timer_companion_specs[] = {
    {"schema", JSON_STRING}, {"version", JSON_NUMBER}, {"profile", JSON_STRING},
    {"decision", JSON_STRING}, {"requirement", JSON_STRING}, {"source_index", JSON_NUMBER},
    {"actual", JSON_STRING}, {"expected_source_set", JSON_STRING_ARRAY},
    {"source_agreements", JSON_NUMBER}, {"meaningful_count", JSON_NUMBER},
    {"reserved_count", JSON_NUMBER}, {"detected_model", JSON_STRING},
    {"model_probe_bytes", JSON_STRING}, {"guest_ticks", JSON_NUMBER},
    {"raw_sha256", JSON_STRING}, {"record_sha256", JSON_STRING},
    {"comparison_sha256", JSON_STRING}, {"bll_sha256", JSON_STRING},
    {"labels_sha256", JSON_STRING}, {"oracle_sha256", JSON_STRING},
    {"descriptor_sha256", JSON_STRING}, {"proof_sha256", JSON_STRING},
    {"hardware_accuracy", JSON_STRING}, {"profile_qualification", JSON_STRING}
};

static int timer_companion_write(const char *profile, const timer_paths *paths,
                                 const char *descriptor_path, uint32_t ticks,
                                 uint32_t agreements)
{
    char raw_sha[65], record_sha[65], comparison_sha[65], bll_sha[65];
    char labels_sha[65], oracle_sha[65], descriptor_sha[65], proof_sha[65];
    char detected[32], probe_bytes[32];
    FILE *out;
    if (file_sha256(paths->raw, raw_sha) != 0 || file_sha256(paths->record, record_sha) != 0 ||
        file_sha256(paths->comparison, comparison_sha) != 0 || file_sha256(paths->bll, bll_sha) != 0 ||
        file_sha256(paths->labels, labels_sha) != 0 || file_sha256(paths->proof, proof_sha) != 0 ||
        file_sha256(descriptor_path, descriptor_sha) != 0 ||
        timer_record_field(paths->record, "oracle_sha256", oracle_sha, sizeof oracle_sha) != 0 ||
        timer_record_field(paths->record, "detected_model", detected, sizeof detected) != 0 ||
        timer_record_field(paths->record, "model_probe_bytes", probe_bytes, sizeof probe_bytes) != 0) {
        return fail("timer companion: source artifact hash or run metadata missing");
    }
    out = fopen(paths->companion, "wb");
    if (out == NULL) return fail("timer companion: cannot create pending record");
    (void)fprintf(out,
        "{\n  \"schema\":\"lynxplosion.timer-source-pending-record\",\n  \"version\":1,\n"
        "  \"profile\":\"%s\",\n  \"decision\":\"D-10\",\n  \"requirement\":\"EVD-02\",\n"
        "  \"source_index\":14,\n  \"actual\":\"0c\",\n  \"expected_source_set\":[\"0d\"],\n"
        "  \"source_agreements\":%" PRIu32 ",\n  \"meaningful_count\":16,\n  \"reserved_count\":2,\n"
        "  \"detected_model\":\"%s\",\n  \"model_probe_bytes\":\"%s\",\n"
        "  \"guest_ticks\":%" PRIu32 ",\n  \"raw_sha256\":\"%s\",\n"
        "  \"record_sha256\":\"%s\",\n  \"comparison_sha256\":\"%s\",\n"
        "  \"bll_sha256\":\"%s\",\n  \"labels_sha256\":\"%s\",\n"
        "  \"oracle_sha256\":\"%s\",\n  \"descriptor_sha256\":\"%s\",\n"
        "  \"proof_sha256\":\"%s\",\n  \"hardware_accuracy\":\"unverified\",\n"
        "  \"profile_qualification\":\"unresolved\"\n}\n",
        profile, agreements, detected, probe_bytes, ticks, raw_sha, record_sha,
        comparison_sha, bll_sha, labels_sha, oracle_sha, descriptor_sha, proof_sha);
    if (fclose(out) != 0) return fail("timer companion: cannot finish pending record");
    return 0;
}

static int timer_companion_validate(const char *profile, const char *directory,
                                   const char *oracle_path, const char *descriptor_path,
                                   const char *source_root, const char *raw_path,
                                   const char *record_path)
{
    static const char *const expected_values[] = {"0d"};
    timer_paths paths;
    suite_oracle oracle;
    uint8_t results[TIMER_MEANINGFUL];
    uint32_t ticks, agreements;
    char bll_sha[65], labels_sha[65], oracle_sha[65];
    char raw_sha[65], record_sha[65], comparison_sha[65], descriptor_sha[65], proof_sha[65];
    char actual[65], detected[32], probe_bytes[32], temp_proof[4096];
    json_object companion;
    int n;
    if (timer_paths_make(&paths, directory, profile) != 0 ||
        strcmp(paths.raw, raw_path) != 0 || strcmp(paths.record, record_path) != 0) return 1;
    if (validate_timer_observation(profile, directory, oracle_path, descriptor_path, &paths,
                                   &oracle, results, bll_sha, labels_sha, oracle_sha,
                                   &ticks, &agreements) != 0) return 1;
    if (file_sha256(paths.raw, raw_sha) != 0 || file_sha256(paths.record, record_sha) != 0 ||
        file_sha256(paths.comparison, comparison_sha) != 0 ||
        file_sha256(descriptor_path, descriptor_sha) != 0 ||
        file_sha256(paths.proof, proof_sha) != 0 ||
        timer_record_field(paths.record, "detected_model", detected, sizeof detected) != 0 ||
        timer_record_field(paths.record, "model_probe_bytes", probe_bytes, sizeof probe_bytes) != 0) {
        return fail("timer companion: complete raw, run, comparison or proof inputs missing");
    }
    n = snprintf(temp_proof, sizeof temp_proof, "%s/.cascade-proof-%s.verify.json", directory, profile);
    if (n < 0 || (size_t)n >= sizeof temp_proof) return fail("timer companion: temporary proof path too long");
    (void)remove(temp_proof);
    if (cascade_proof_write(profile, directory, oracle_path, descriptor_path,
                            source_root, temp_proof) != 0 ||
        file_sha256(temp_proof, actual) != 0 || strcmp(actual, proof_sha) != 0) {
        (void)remove(temp_proof);
        return fail("timer companion: cascade proof is stale or does not match linked source evidence");
    }
    (void)remove(temp_proof);
    if (json_parse_file(paths.companion, timer_companion_specs,
                        sizeof timer_companion_specs / sizeof timer_companion_specs[0],
                        &companion, "timer companion") != 0 ||
        json_missing_required(&companion, timer_companion_specs,
                              sizeof timer_companion_specs / sizeof timer_companion_specs[0],
                              "timer companion") != 0) return 1;
#define COMPANION_STRING(key, value) do { if (!json_string_is(&companion, (key), (value))) return fail("suite: pending record value mismatch"); } while (0)
#define COMPANION_NUMBER(key, value) do { if (!json_number_is(&companion, (key), (value))) return fail("suite: pending record value mismatch"); } while (0)
    if (!json_string_is(&companion, "schema", "lynxplosion.timer-source-pending-record") ||
        !json_number_is(&companion, "version", 1u)) return fail("suite: pending record unsupported schema/version");
    COMPANION_STRING("profile", profile);
    COMPANION_STRING("decision", "D-10");
    COMPANION_STRING("requirement", "EVD-02");
    COMPANION_NUMBER("source_index", 14u);
    COMPANION_STRING("actual", "0c");
    if (!json_array_is(&companion, "expected_source_set", expected_values, 1u)) return fail("suite: pending record value mismatch");
    COMPANION_NUMBER("source_agreements", 15u);
    COMPANION_NUMBER("meaningful_count", 16u);
    COMPANION_NUMBER("reserved_count", 2u);
    COMPANION_STRING("detected_model", detected);
    COMPANION_STRING("model_probe_bytes", probe_bytes);
    COMPANION_NUMBER("guest_ticks", ticks);
    COMPANION_STRING("hardware_accuracy", "unverified");
    COMPANION_STRING("profile_qualification", "unresolved");
#undef COMPANION_STRING
#undef COMPANION_NUMBER
#define COMPANION_HASH(key, value) do { if (!valid_hex((value), 64u) || !json_string_is(&companion, (key), (value))) return fail("suite: pending record artifact identity mismatch"); } while (0)
    COMPANION_HASH("raw_sha256", raw_sha);
    COMPANION_HASH("record_sha256", record_sha);
    COMPANION_HASH("comparison_sha256", comparison_sha);
    COMPANION_HASH("bll_sha256", bll_sha);
    COMPANION_HASH("labels_sha256", labels_sha);
    COMPANION_HASH("oracle_sha256", oracle_sha);
    COMPANION_HASH("descriptor_sha256", descriptor_sha);
    COMPANION_HASH("proof_sha256", proof_sha);
#undef COMPANION_HASH
    printf("timer record: complete observation, executable phase proof and pending row valid for %s\n", profile);
    return 0;
}

static int machine_create(machine_instance *instance, const uint8_t *bll, size_t bll_length,
                          uint32_t profile)
{
    lynx_config config;
    memset(instance, 0, sizeof *instance);
    instance->storage = malloc(lynx_machine_size());
    if (instance->storage == NULL ||
        (size_t)((uintptr_t)instance->storage % lynx_machine_align()) != 0u) {
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("machine storage allocation or alignment failed");
    }
    lynx_config_default(&config);
    config.profile = profile;
    if (lynx_machine_init(instance->storage, lynx_machine_size(), &config,
                          &instance->machine) != LYNX_OK ||
        lynx_load_bll(instance->machine, bll, bll_length) != LYNX_OK) {
        if (instance->machine != NULL) lynx_machine_deinit(instance->machine);
        free(instance->storage);
        memset(instance, 0, sizeof *instance);
        return fail("public BS93 load failed");
    }
    return 0;
}

static void machine_destroy(machine_instance *instance)
{
    if (instance->machine != NULL) lynx_machine_deinit(instance->machine);
    free(instance->storage);
    memset(instance, 0, sizeof *instance);
}

static const char *stop_name(lynx_stop_reason stop)
{
    switch (stop) {
        case LYNX_STOP_BUDGET: return "budget";
        case LYNX_STOP_WATCH: return "watch";
        case LYNX_STOP_UNAVAILABLE: return "unavailable";
        case LYNX_STOP_UNDEFINED: return "undefined";
        case LYNX_STOP_HALT: return "halt";
        default: return "unknown";
    }
}

static int checked_join(char *out, size_t capacity, const char *directory, const char *file)
{
    int count = snprintf(out, capacity, "%s/%s", directory, file);
    return count < 0 || (size_t)count >= capacity;
}

static int clear_old_outputs(const char *directory, const char *case_name)
{
    static const char *const kinds[] = {"run-record", "raw-results", "comparison", "cascade-proof", "pending-record"};
    struct stat info;
    char path[4096];
    size_t i;
    if (lstat(directory, &info) != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
        return fail("build directory must be a real invocation-owned directory");
    }
    for (i = 0u; i < sizeof kinds / sizeof kinds[0]; i++) {
        char name[64];
        const char *extension = i == 0u ? "txt" : (i < 3u ? "tsv" : "json");
        int count = snprintf(name, sizeof name, "%s-%s.%s", kinds[i], case_name, extension);
        if (count < 0 || (size_t)count >= sizeof name ||
            checked_join(path, sizeof path, directory, name) != 0) return fail("output path too long");
        if (remove(path) != 0) {
            struct stat existing;
            if (lstat(path, &existing) == 0) return fail("cannot clear stale suite output");
        }
    }
    return 0;
}

static int write_raw(const char *path, const char *suite, const char *machine_profile,
                     const char *selected_profile, const char *case_name,
                     const uint8_t *results, uint32_t count)
{
    FILE *file = fopen(path, "wb");
    uint32_t i;
    if (file == NULL) return fail("cannot create raw result record");
    (void)fprintf(file, "suite\t%s\nmachine_profile\t%s\nprofile_input\t%s\nram_seed\t%s\nresult_count\t%u\n",
                  suite, machine_profile, selected_profile, case_name, count);
    for (i = 0u; i < count; i++) (void)fprintf(file, "result\t%" PRIu32 "\t%02x\n", i, results[i]);
    if (fclose(file) != 0) return fail("cannot close raw result record");
    return 0;
}

static int write_comparison(const char *path, const suite_oracle *oracle,
                            const uint8_t *results, uint32_t count)
{
    FILE *file = fopen(path, "wb");
    uint32_t i;
    int match = 1;
    if (file == NULL) return fail("cannot create comparison record");
    for (i = 0u; i < count; i++) {
        size_t j;
        int accepted = 0;
        for (j = 0u; j < oracle->counts[0][i]; j++) {
            if (oracle->values[0][i][j] == results[i]) accepted = 1;
        }
        (void)fprintf(file, "result\t%" PRIu32 "\t%02x\t%s\n",
                      i, results[i], accepted != 0 ? "source-accepted" : "source-mismatch");
        if (accepted == 0) match = 0;
    }
    (void)fprintf(file, "comparison\t%s\n", match != 0 ? "match" : "mismatch");
    if (fclose(file) != 0) return fail("cannot close comparison record");
    return match != 0 ? 0 : fail("source oracle comparison mismatch");
}

static int write_timer_comparison(const char *path, const suite_oracle *oracle,
                                  const uint8_t *results, int *matches)
{
    FILE *file = fopen(path, "wb");
    uint32_t i;
    int match = 1;
    if (file == NULL) return fail("cannot create timer comparison record");
    for (i = 0u; i < TIMER_MEANINGFUL; i++) {
        size_t j;
        int accepted = 0;
        (void)fprintf(file, "result\t%" PRIu32 "\t%02x\tEXPECT(", i, results[i]);
        for (j = 0u; j < oracle->counts[1][i]; j++) {
            if (j != 0u) (void)fputc(',', file);
            (void)fprintf(file, "%02x", oracle->values[1][i][j]);
            if (oracle->values[1][i][j] == results[i]) accepted = 1;
        }
        (void)fprintf(file, ")\t%s\n", accepted != 0 ? "source-accepted" : "source-mismatch");
        if (accepted == 0) match = 0;
    }
    (void)fprintf(file, "comparison\t%s\n", match != 0 ? "match" : "mismatch");
    if (fclose(file) != 0) return fail("cannot close timer comparison record");
    *matches = match;
    return 0;
}

static const char *profile_label(unsigned profile)
{
    return profile == 0u ? "lynx1" : "lynx2";
}

static int profile_index(const char *name, unsigned *index)
{
    if (strcmp(name, "lynx1") == 0) {
        *index = 0u;
        return 0;
    }
    if (strcmp(name, "lynx2") == 0) {
        *index = 1u;
        return 0;
    }
    return 1;
}

static int probe_model(const uint8_t probe, const uint8_t marker)
{
    if (probe == 0x01u && marker == 0xA1u) return 0;
    if (probe == 0x00u && marker == 0xA2u) return 1;
    return -1;
}

static int validate_guest_completion(machine_instance *instance, const suite_symbols *symbols,
                                     const char *suite,
                                     int execution_started, int complete,
                                     int initial_done_clear_seen, uint64_t guest_ticks,
                                     uint64_t total_consumed, uint32_t minimum_ticks,
                                     uint32_t maximum_ticks, uint8_t *done,
                                     uint8_t *progress, uint8_t *model, uint8_t *probe)
{
    lynx_machine *machine = instance->machine;
    if (lynx_read_memory(machine, symbols->value[6], done, 1u) != LYNX_OK ||
        lynx_read_memory(machine, symbols->value[4], progress, 1u) != LYNX_OK ||
        lynx_read_memory(machine, symbols->value[5], model, 1u) != LYNX_OK ||
        lynx_read_memory(machine, symbols->value[11], probe, 1u) != LYNX_OK) {
        (void)fail("completion: guest completion bytes could not be read");
        return 2;
    }
    fprintf(stderr,
            "completion state: suite=%s execution_started=%s progress=%u sentinel=%02x ticks=%" PRIu64
            " maximum=%u post_return_watch=%s\n",
            suite, execution_started != 0 ? "true" : "false", (unsigned)*progress, (unsigned)*done,
            guest_ticks, maximum_ticks, complete != 0 ? "hit" : "absent");
    if (execution_started == 0) {
        return fail("completion: guest execution absent; post-return sentinel was not written");
    }
    if (complete == 0 || initial_done_clear_seen == 0 || *done != 0xA5u) {
        if (guest_ticks >= maximum_ticks && *progress != 8u) {
            return fail("completion: guest-time limit reached before the suite returned");
        }
        if (*progress == 8u) {
            return fail("completion: post-return sentinel absent after the suite returned");
        }
        return fail("completion: post-return sentinel absent or invalid");
    }
    if (guest_ticks < minimum_ticks || guest_ticks > maximum_ticks ||
        total_consumed == 0u || *progress != 8u) {
        return fail("completion: semantic progress or guest-time bound mismatch");
    }
    return 0;
}

/* Prove a negative run was held before the first guest instruction.  The
   private hold is installed only after the fresh BLL has been loaded and
   before any advance; unchanged CPU/request and RAM state prove no grant or
   guest write occurred while independent guest time reached the suite floor. */
static int prove_held_no_run(machine_instance *instance, const suite_symbols *symbols,
                             uint32_t minimum_ticks, uint64_t *ticks_out)
{
    lynx_machine *machine = instance->machine;
    lynx__cpu cpu_before;
    uint8_t *ram_before;
    uint64_t before = 0u;
    uint64_t after = 0u;
    uint64_t used = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;

    if (machine == NULL || minimum_ticks == 0u) return fail("no-run: invalid held instance");
    if (machine->guest_ticks != 0u || machine->cpu.pc != symbols->value[0]) {
        return fail("no-run: guest advanced before hold was armed");
    }
    cpu_before = machine->cpu;
    ram_before = malloc(sizeof machine->ram);
    if (ram_before == NULL) return fail("no-run: cannot snapshot guest RAM");
    memcpy(ram_before, machine->ram, sizeof machine->ram);
    if (lynx__machine_set_bus_hold(machine, 1u) != LYNX_OK || machine->bus_hold != 1u) {
        free(ram_before);
        return fail("no-run: private bus hold was not armed");
    }
    if (lynx_guest_ticks(machine, &before) != LYNX_OK ||
        lynx_advance(machine, minimum_ticks, &used, &stop) != LYNX_OK ||
        lynx_guest_ticks(machine, &after) != LYNX_OK) {
        free(ram_before);
        return fail("no-run: bounded held guest advance failed");
    }
    if (stop != LYNX_STOP_BUDGET || used != minimum_ticks || after < minimum_ticks ||
        after < before || after - before != used || memcmp(&cpu_before, &machine->cpu, sizeof cpu_before) != 0 ||
        memcmp(ram_before, machine->ram, sizeof machine->ram) != 0 || machine->bus_hold != 1u) {
        free(ram_before);
        return fail("no-run: held CPU/request or guest RAM changed before completion");
    }
    free(ram_before);
    *ticks_out = after;
    return 0;
}

static int tuple_model(const suite_oracle *oracle, const uint8_t *results)
{
    unsigned model;
    int found = -1;
    for (model = 0u; model < 2u; model++) {
        if (memcmp(results + 12u, oracle->tuple[model], 4u) == 0) {
            if (found >= 0) return -1;
            found = (int)model;
        }
    }
    return found;
}

static uint8_t seed_byte(uint32_t address)
{
    return (uint8_t)(0x5Au ^ (address * 37u));
}

static int prepare_cpu_ram(machine_instance *instance, const suite_symbols *symbols,
                           const char *case_name)
{
    uint32_t i;
    if (strcmp(case_name, "zero") == 0) {
        for (i = 0u; i < 0x100u; i++) {
            if (instance->machine->ram[i] != 0u) return fail("fresh zero-page RAM was not zero");
        }
        for (i = 0u; i < CPU_MEANINGFUL; i++) {
            if (instance->machine->ram[symbols->value[2] + i] != 0u) {
                return fail("fresh CPU result RAM was not zero");
            }
        }
        for (i = 4u; i <= 6u; i++) {
            if (instance->machine->ram[symbols->value[i]] != 0u) {
                return fail("fresh CPU completion RAM was not zero");
            }
        }
        return 0;
    }
    if (strcmp(case_name, "seeded") != 0) return fail("CPU RAM case must be zero or seeded");
    for (i = 0u; i < 0x100u; i++) instance->machine->ram[i] = seed_byte(i);
    memset(instance->machine->ram + symbols->value[2], 0x5Au, CPU_MEANINGFUL);
    instance->machine->ram[symbols->value[4]] = 0xC3u;
    instance->machine->ram[symbols->value[5]] = 0x3Cu;
    instance->machine->ram[symbols->value[6]] = 0x69u;
    for (i = 0u; i < 0x100u; i++) {
        if (instance->machine->ram[i] != seed_byte(i)) return fail("seeded zero-page RAM was not established");
    }
    for (i = 0u; i < CPU_MEANINGFUL; i++) {
        if (instance->machine->ram[symbols->value[2] + i] != 0x5Au) {
            return fail("seeded CPU result RAM was not established");
        }
    }
    if (instance->machine->ram[symbols->value[4]] != 0xC3u ||
        instance->machine->ram[symbols->value[5]] != 0x3Cu ||
        instance->machine->ram[symbols->value[6]] != 0x69u) {
        return fail("seeded CPU completion RAM was not established");
    }
    return 0;
}

static int write_run_record(const char *path, const char *machine_profile,
                            const char *selected_profile, const char *case_name,
                            const char *bll_sha, const char *labels_sha,
                            const char *oracle_sha, uint64_t ticks,
                            uint8_t probe, uint8_t model, uint8_t progress,
                            int observed_model, int source_tuple_model)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) return fail("cannot create suite run record");
    (void)fprintf(file,
        "evidence=raw-observation-plus-source-comparison\n"
        "suite=cpu\nmachine_profile=%s\nprofile_input=%s\nram_seed=%s\n"
        "bll_sha256=%s\nlabels_sha256=%s\noracle_sha256=%s\n"
        "detected_model=%s\nmodel_probe_bytes=%02x,%02x\nsource_tuple_model=%s\n"
        "meaningful_count=18\nreserved_count=0\ntick_min=%u\ntick_max=%u\n"
        "ticks_observed=%" PRIu64 "\ncompletion_margin_ticks=%" PRIu64 "\n"
        "maximum_headroom_ticks=%" PRIu64 "\n"
        "guest_ticks=%" PRIu64 "\nminimum_guest_ticks=%u\nmaximum_guest_ticks=%u\n"
        "meaningful_results=18\nreserved_results=0\nguest_probe_byte=%02x\n"
        "guest_model_marker=%02x\nguest_progress=%u\ncompletion_sentinel=a5\n"
        "scheduler_policy=timer-cpu-poll-order-1\nphase_policy=timer-phase-fixed-zero-1\n"
        "source_comparison=separate-record\nhardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        machine_profile, selected_profile, case_name,
        bll_sha, labels_sha, oracle_sha,
        observed_model < 0 ? "unknown" : profile_label((unsigned)observed_model),
        probe, model,
        source_tuple_model < 0 ? "unknown" : profile_label((unsigned)source_tuple_model),
        CPU_MIN_TICKS, CPU_MAX_TICKS, ticks,
        ticks - CPU_MIN_TICKS, CPU_MAX_TICKS - ticks,
        ticks, CPU_MIN_TICKS, CPU_MAX_TICKS,
        probe, model, progress);
    if (fclose(file) != 0) return fail("cannot close suite run record");
    return 0;
}

static int run_cpu(const char *machine_profile_name, const char *selected_profile_name,
                   const char *directory, const char *oracle_path, const char *case_name,
                   int no_run, uint32_t maximum_ticks)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char path[4096];
    char bll_path[4096];
    char labels_path[4096];
    char record_name[64];
    char raw_name[64];
    char comparison_name[64];
    char record_path[4096];
    char raw_path[4096];
    char comparison_path[4096];
    char bll_sha[65];
    char labels_sha[65];
    char oracle_sha[65];
    char build_sha[128];
    char identity_sha[65];
    uint8_t results[RESULT_EXTENT];
    uint8_t progress = 0u;
    uint8_t model = 0u;
    uint8_t done = 0u;
    uint8_t probe = 0u;
    uint32_t machine_profile;
    unsigned machine_profile_index;
    uint32_t selected_profile;
    uint64_t guest_ticks = 0u;
    uint64_t total_consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    lynx_identity identity;
    lynx__cpu initial_cpu;
    machine_instance instance = {0};
    suite_symbols symbols;
    suite_oracle oracle;
    int complete = 0;
    int initial_done_clear_seen = 0;
    int probe_observed_model = -1;
    int guest_model = -1;
    int result_tuple_model = -1;
    int result = 1;

    if (profile_index(machine_profile_name, &machine_profile_index) != 0 ||
        profile_index(selected_profile_name, &selected_profile) != 0) {
        return fail("explicit lynx1 or lynx2 profile input required");
    }
    if (maximum_ticks < CPU_MIN_TICKS || maximum_ticks > CPU_MAX_TICKS) {
        return fail("time: guest ticks outside bounded completion window");
    }
    machine_profile = machine_profile_index == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
    if (strcmp(case_name, "zero") != 0 && strcmp(case_name, "seeded") != 0) {
        return fail("CPU RAM case must be zero or seeded");
    }
    if (clear_old_outputs(directory, case_name) != 0) return 1;
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0) goto done;
    if (snprintf(record_name, sizeof record_name, "run-record-%s.txt", case_name) >= (int)sizeof record_name ||
        snprintf(raw_name, sizeof raw_name, "raw-results-%s.tsv", case_name) >= (int)sizeof raw_name ||
        snprintf(comparison_name, sizeof comparison_name, "comparison-%s.tsv", case_name) >= (int)sizeof comparison_name ||
        checked_join(bll_path, sizeof bll_path, directory, "cpu.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, directory, "cpu.lbl") != 0 ||
        checked_join(record_path, sizeof record_path, directory, record_name) != 0 ||
        checked_join(raw_path, sizeof raw_path, directory, raw_name) != 0 ||
        checked_join(comparison_path, sizeof comparison_path, directory, comparison_name) != 0) {
        (void)fail("suite input path too long");
        goto done;
    }
    if (read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0) goto done;
    if (parse_symbols(labels, labels_length, &symbols) != 0 ||
        ranges_valid(&symbols, RESULT_EXTENT) != 0) goto done;
    if (file_sha256(bll_path, bll_sha) != 0 ||
        file_sha256(labels_path, labels_sha) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0) goto done;
    if (machine_create(&instance, bll, bll_length, machine_profile) != 0 ||
        lynx_get_identity(instance.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A ||
        identity.entry_addr != symbols.value[0] ||
        identity.object_len != bll_length || symbols.value[7] < BLL_HEADER_SIZE ||
        symbols.value[7] - 0x01F6u != bll_length) {
        (void)fail("strict public BS93 identity or linked extent mismatch");
        goto done;
    }
    lynx__sha256 state;
    uint8_t digest[32];
    lynx__sha256_init(&state);
    lynx__sha256_update(&state, bll, bll_length);
    lynx__sha256_final(&state, digest);
    lynx__sha256_hex(digest, build_sha);
    lynx__sha256_hex(identity.content_sha256, identity_sha);
    if (strcmp(build_sha, identity_sha) != 0) {
        (void)fail("public BLL content identity mismatch");
        goto done;
    }
    if (prepare_cpu_ram(&instance, &symbols, case_name) != 0) goto done;
    if (snprintf(path, sizeof path, "%s/build-record.txt", directory) >= (int)sizeof path ||
        record_value(path, "cpu_object_sha256", build_sha, sizeof build_sha) != 0 ||
        strcmp(build_sha, identity_sha) != 0) {
        (void)fail("build record object identity mismatch");
        goto done;
    }
    if (lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
        (void)fail("could not arm post-test sentinel watch");
        goto done;
    }
    initial_cpu = instance.machine->cpu;
    if (no_run != 0) {
        int completion_result;
        if (prove_held_no_run(&instance, &symbols, CPU_MIN_TICKS, &guest_ticks) != 0) goto done;
        total_consumed = guest_ticks;
        completion_result = validate_guest_completion(&instance, &symbols, "cpu", 0, 0, 0,
                                                      guest_ticks, total_consumed,
                                                      CPU_MIN_TICKS, CPU_MAX_TICKS,
                                                      &done, &progress, &model, &probe);
        if (completion_result != 1) {
            if (completion_result == 0) (void)fail("no-run: completion validator accepted a held suite");
            goto done;
        }
        if (lynx__machine_set_bus_hold(instance.machine, 0u) != LYNX_OK ||
            instance.machine->bus_hold != 0u) {
            (void)fail("no-run: private bus hold could not be released");
            goto done;
        }
        puts("no-run control: exact absent-execution refusal observed after minimum held guest time; hold released");
    }
    while (total_consumed < maximum_ticks) {
        uint64_t budget = maximum_ticks - total_consumed;
        uint64_t used = 0u;
        uint64_t before = 0u;
        uint64_t after = 0u;
        if (budget > 10000u) budget = 10000u;
        if (lynx_guest_ticks(instance.machine, &before) != LYNX_OK ||
            lynx_advance(instance.machine, budget, &used, &stop) != LYNX_OK ||
            used > budget || lynx_guest_ticks(instance.machine, &after) != LYNX_OK) {
            (void)fail("bounded public guest advance failed");
            goto done;
        }
        total_consumed += used;
        if (after < before || after - before != used || after > maximum_ticks) {
            (void)fail("guest tick accounting exceeded the bounded budget");
            goto done;
        }
        guest_ticks = after;
        if (stop == LYNX_STOP_WATCH) {
            if (lynx_watch_get(instance.machine, &event) != LYNX_OK ||
                event.hit != 1u || event.address != symbols.value[6] ||
                event.return_tick != after || event.return_tick != event.hit_tick + 1u) {
                (void)fail("completion-address watch did not match a guest write");
                goto done;
            }
            if (event.value == 0u && initial_done_clear_seen == 0) {
                uint8_t initial_progress = 0u;
                uint8_t initial_model = 0u;
                if (lynx_read_memory(instance.machine, symbols.value[4], &initial_progress, 1u) != LYNX_OK ||
                    lynx_read_memory(instance.machine, symbols.value[5], &initial_model, 1u) != LYNX_OK ||
                    initial_progress != 0u || initial_model != 0u ||
                    lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                    (void)fail("source wrapper's initial sentinel clear was not an empty-state prefix");
                    goto done;
                }
                initial_done_clear_seen = 1;
                continue;
            }
            if (event.value == 0xA5u && initial_done_clear_seen != 0) {
                complete = 1;
                break;
            }
            (void)fail("guest sentinel value or write order mismatch");
            goto done;
        }
        if (stop != LYNX_STOP_BUDGET) {
            char message[128];
            (void)snprintf(message, sizeof message, "guest stopped before completion: %s", stop_name(stop));
            (void)fail(message);
            goto done;
        }
        if (used == 0u || after <= before) {
            (void)fail("guest made no bounded progress toward completion");
            goto done;
        }
    }
    {
        const int execution_started = memcmp(&initial_cpu, &instance.machine->cpu,
                                             sizeof initial_cpu) != 0;
        if (validate_guest_completion(&instance, &symbols, "cpu", execution_started, complete,
                                      initial_done_clear_seen, guest_ticks, total_consumed,
                                      CPU_MIN_TICKS, maximum_ticks,
                                      &done, &progress, &model, &probe) != 0) {
            goto done;
        }
    }
    if (done != 0xA5u || progress != 8u) {
        (void)fail("completion: validated CPU completion bytes changed unexpectedly");
        goto done;
    }
    if (lynx_read_memory(instance.machine, symbols.value[2], results, CPU_MEANINGFUL) != LYNX_OK) {
        (void)fail("validated CPU result buffer could not be read");
        goto done;
    }
    probe_observed_model = probe_model(probe, model);
    result_tuple_model = tuple_model(&oracle, results);
    if (probe_observed_model == result_tuple_model) guest_model = probe_observed_model;
    if (write_raw(raw_path, "cpu", machine_profile_name, selected_profile_name,
                  case_name, results, CPU_MEANINGFUL) != 0 ||
        write_run_record(record_path, machine_profile_name, selected_profile_name,
                         case_name, identity_sha, labels_sha, oracle_sha, guest_ticks,
                         probe, model, progress, guest_model, result_tuple_model) != 0) goto done;
    result = write_comparison(comparison_path, &oracle, results, CPU_MEANINGFUL);
    if (result != 0) goto done;
    printf("suite raw source comparison: match; suite=cpu machine_profile=%s profile_input=%s ram_seed=%s"
           " ticks=%" PRIu64 " results=%u detected_model=%s source_tuple_model=%s"
           " guest_model_marker=%02x guest_probe_byte=%02x\n",
           machine_profile_name, selected_profile_name, case_name, guest_ticks, CPU_MEANINGFUL,
           guest_model < 0 ? "unknown" : profile_label((unsigned)guest_model),
           result_tuple_model < 0 ? "unknown" : profile_label((unsigned)result_tuple_model), model, probe);
    printf("hardware_accuracy=unverified profile_qualification=unresolved\n");
    if (probe_observed_model < 0) {
        result = fail("guest model probe/marker evidence is invalid");
    } else if (result_tuple_model < 0) {
        result = fail("CPU result tuple does not match exactly one source model tuple");
    } else if (probe_observed_model != result_tuple_model) {
        result = fail("guest model probe and source-defined tuple disagree");
    } else if (guest_model != (int)selected_profile) {
        result = fail("detected model does not match selected profile input");
    }
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
}

static int prepare_timer_ram(machine_instance *instance, const suite_symbols *symbols)
{
    uint32_t i;
    for (i = 0u; i < TIMER_RESERVED + TIMER_MEANINGFUL; i++) {
        if (instance->machine->ram[symbols->value[2] + i] != 0u) {
            return fail("fresh timer result extent was not zero");
        }
    }
    for (i = 4u; i <= 6u; i++) {
        if (instance->machine->ram[symbols->value[i]] != 0u) {
            return fail("fresh timer completion RAM was not zero");
        }
    }
    if (instance->machine->ram[symbols->value[11]] != 0u) {
        return fail("fresh timer probe RAM was not zero");
    }
    return 0;
}

static int write_timer_run_record(const char *path, const char *machine_profile,
                                  const char *selected_profile, const char *bll_sha,
                                  const char *labels_sha, const char *oracle_sha,
                                  const char *wrapper_sha, uint64_t ticks,
                                  uint8_t probe, uint8_t model, uint8_t progress,
                                  uint32_t probe_address, uint32_t model_address,
                                  uint32_t test_return_address,
                                  uint32_t after_tests_address, uint8_t pending,
                                  int detected_model)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) return fail("cannot create timer run record");
    (void)fprintf(file,
        "evidence=raw-observation-plus-source-comparison\n"
        "suite=timers\nmachine_profile=%s\nprofile_input=%s\nram_seed=zero\n"
        "bll_sha256=%s\nlabels_sha256=%s\noracle_sha256=%s\nwrapper_sha256=%s\n"
        "model_probe_source=fixtures/suites/timers-wrapper.s\n"
        "model_probe_source_sha256=%s\nmodel_probe_symbol=suite_probe_result@%04x\n"
        "model_marker_symbol=suite_model@%04x\nsource_runner_rts_symbol=suite_test_return@%04x\n"
        "postamble_return_symbol=suite_after_tests@%04x\n"
        "postamble_irq_policy=mask-only-after-suite-return\npostamble_pending_irq=%02x\n"
        "detected_model=%s\nmodel_probe_bytes=%02x,%02x\n"
        "source_tuple_model=not-applicable\nmeaningful_count=16\nreserved_count=2\nresult_extent=18\n"
        "tick_min=%u\ntick_max=%u\nticks_observed=%" PRIu64 "\n"
        "completion_margin_ticks=%" PRIu64 "\nmaximum_headroom_ticks=%" PRIu64 "\n"
        "guest_ticks=%" PRIu64 "\nminimum_guest_ticks=%u\nmaximum_guest_ticks=%u\n"
        "meaningful_results=16\nreserved_results=2\n"
        "guest_probe_byte=%02x\nguest_model_marker=%02x\nguest_progress=%u\ncompletion_sentinel=a5\n"
        "scheduler_policy=timer-cpu-poll-order-1\nphase_policy=timer-phase-fixed-zero-1\n"
        "cost_model=flat5\ncost_ticks=5\nsample_offset_ticks=0\n"
        "page_mode_video_enabled=false\nrefresh_dma_enabled=false\ncounter_initialization=zero\n"
        "source_comparison=separate-record\nhardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        machine_profile, selected_profile, bll_sha, labels_sha, oracle_sha, wrapper_sha,
        wrapper_sha, probe_address, model_address, test_return_address,
        after_tests_address, pending,
        detected_model < 0 ? "unknown" : profile_label((unsigned)detected_model),
        probe, model, TIMER_MIN_TICKS, TIMER_MAX_TICKS, ticks,
        ticks - TIMER_MIN_TICKS, TIMER_MAX_TICKS - ticks,
        ticks, TIMER_MIN_TICKS, TIMER_MAX_TICKS, probe, model, progress);
    if (fclose(file) != 0) return fail("cannot close timer run record");
    return 0;
}

static int run_timers(const char *machine_profile_name, const char *selected_profile_name,
                      const char *directory, const char *oracle_path,
                      const char *pending_descriptor_path, int no_run,
                      uint32_t maximum_ticks)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char path[4096];
    char bll_path[4096];
    char labels_path[4096];
    char record_name[64];
    char raw_name[64];
    char comparison_name[64];
    char record_path[4096];
    char raw_path[4096];
    char comparison_path[4096];
    char bll_sha[65];
    char labels_sha[65];
    char oracle_sha[65];
    char wrapper_sha[65];
    uint8_t results[TIMER_MEANINGFUL];
    uint8_t progress = 0u;
    uint8_t model = 0u;
    uint8_t done = 0u;
    uint8_t probe = 0u;
    uint32_t machine_profile;
    uint32_t selected_profile;
    uint64_t guest_ticks = 0u;
    uint64_t total_consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    lynx_identity identity;
    lynx__cpu initial_cpu;
    machine_instance instance = {0};
    suite_symbols symbols;
    suite_oracle oracle;
    int complete = 0;
    int initial_done_clear_seen = 0;
    int detected_model = -1;
    int comparison_matches = 0;
    int result = 1;

    if (profile_index(machine_profile_name, &machine_profile) != 0 ||
        profile_index(selected_profile_name, &selected_profile) != 0) {
        return fail("explicit lynx1 or lynx2 timer profile input required");
    }
    if (maximum_ticks < TIMER_MIN_TICKS || maximum_ticks > TIMER_MAX_TICKS) {
        return fail("time: guest ticks outside bounded completion window");
    }
    machine_profile = machine_profile == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
    if (clear_old_outputs(directory, machine_profile_name) != 0) return 1;
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0) goto done;
    if (pending_descriptor_path != NULL &&
        pending_descriptor_validate(pending_descriptor_path, directory, oracle_path) != 0) goto done;
    if (snprintf(record_name, sizeof record_name, "run-record-%s.txt", machine_profile_name) >= (int)sizeof record_name ||
        snprintf(raw_name, sizeof raw_name, "raw-results-%s.tsv", machine_profile_name) >= (int)sizeof raw_name ||
        snprintf(comparison_name, sizeof comparison_name, "comparison-%s.tsv", machine_profile_name) >= (int)sizeof comparison_name ||
        checked_join(bll_path, sizeof bll_path, directory, "timers.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, directory, "timers.lbl") != 0 ||
        checked_join(record_path, sizeof record_path, directory, record_name) != 0 ||
        checked_join(raw_path, sizeof raw_path, directory, raw_name) != 0 ||
        checked_join(comparison_path, sizeof comparison_path, directory, comparison_name) != 0) {
        (void)fail("timer suite input path too long");
        goto done;
    }
    if (read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0) goto done;
    if (parse_symbols(labels, labels_length, &symbols) != 0 ||
        ranges_valid(&symbols, RESULT_EXTENT) != 0) goto done;
    if (symbols.found[12] == 0u || symbols.found[13] == 0u ||
        symbols.value[1] != symbols.value[8] ||
        symbols.value[12] != symbols.value[8] + 26u ||
        symbols.value[12] + 1u != symbols.value[7] ||
        symbols.value[12] < 0x01F7u ||
        symbols.value[12] - 0x01F6u >= bll_length ||
        bll[symbols.value[12] - 0x01F6u] != 0x60u ||
        bll[symbols.value[12] - 0x01F6u - 1u] != 0x58u ||
        symbols.value[13] < 0x01F9u ||
        symbols.value[13] - 0x01F6u >= bll_length ||
        bll[symbols.value[13] - 0x01F6u - 3u] != 0x20u ||
        bll[symbols.value[13] - 0x01F6u - 2u] != (uint8_t)symbols.value[8] ||
        bll[symbols.value[13] - 0x01F6u - 1u] != (uint8_t)(symbols.value[8] >> 8)) {
        (void)fail("timer wrapper postamble gate is not the pinned CLI-to-RTS return");
        goto done;
    }
    if (file_sha256(bll_path, bll_sha) != 0 || file_sha256(labels_path, labels_sha) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0) goto done;
    if (snprintf(path, sizeof path, "%s/build-record.txt", directory) >= (int)sizeof path ||
        record_value(path, "timers_wrapper_sha256", wrapper_sha, sizeof wrapper_sha) != 0 ||
        !valid_hex(wrapper_sha, 64u)) {
        (void)fail("timer wrapper source identity missing from build record");
        goto done;
    }
    if (machine_create(&instance, bll, bll_length, machine_profile) != 0 ||
        lynx_get_identity(instance.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A || identity.entry_addr != symbols.value[0] ||
        identity.object_len != bll_length || symbols.value[7] < BLL_HEADER_SIZE ||
        symbols.value[7] - 0x01F6u != bll_length) {
        (void)fail("strict timer BS93 identity or linked extent mismatch");
        goto done;
    }
    {
        lynx__sha256 state;
        uint8_t digest[32];
        char identity_sha[65];
        lynx__sha256_init(&state);
        lynx__sha256_update(&state, bll, bll_length);
        lynx__sha256_final(&state, digest);
        lynx__sha256_hex(digest, identity_sha);
        lynx__sha256_hex(identity.content_sha256, path);
        if (strcmp(identity_sha, path) != 0) {
            (void)fail("timer public BLL content identity mismatch");
            goto done;
        }
        (void)snprintf(bll_sha, sizeof bll_sha, "%s", identity_sha);
    }
    if (prepare_timer_ram(&instance, &symbols) != 0) goto done;
    if (lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
        (void)fail("could not arm timer post-test sentinel watch");
        goto done;
    }
    initial_cpu = instance.machine->cpu;
    if (no_run != 0) {
        int completion_result;
        if (prove_held_no_run(&instance, &symbols, TIMER_MIN_TICKS, &guest_ticks) != 0) goto done;
        total_consumed = guest_ticks;
        completion_result = validate_guest_completion(&instance, &symbols, "timers", 0, 0, 0,
                                                      guest_ticks, total_consumed,
                                                      TIMER_MIN_TICKS, TIMER_MAX_TICKS,
                                                      &done, &progress, &model, &probe);
        if (completion_result != 1) {
            if (completion_result == 0) (void)fail("no-run: completion validator accepted a held timer suite");
            goto done;
        }
        if (lynx__machine_set_bus_hold(instance.machine, 0u) != LYNX_OK ||
            instance.machine->bus_hold != 0u) {
            (void)fail("no-run: private bus hold could not be released");
            goto done;
        }
        puts("no-run control: exact absent-execution refusal observed after minimum held guest time; hold released");
    }
    while (total_consumed < maximum_ticks) {
        uint64_t budget = maximum_ticks - total_consumed;
        uint64_t used = 0u;
        uint64_t before = 0u;
        uint64_t after = 0u;
        if (budget > 10000u) budget = 10000u;
        if (lynx_guest_ticks(instance.machine, &before) != LYNX_OK ||
            lynx_advance(instance.machine, budget, &used, &stop) != LYNX_OK ||
            used > budget || lynx_guest_ticks(instance.machine, &after) != LYNX_OK) {
            (void)fail("bounded timer guest advance failed");
            goto done;
        }
        total_consumed += used;
        if (after < before || after - before != used || after > maximum_ticks) {
            (void)fail("timer guest tick accounting exceeded the bounded budget");
            goto done;
        }
        guest_ticks = after;
        if (stop == LYNX_STOP_WATCH) {
            if (lynx_watch_get(instance.machine, &event) != LYNX_OK ||
                event.hit != 1u || event.address != symbols.value[6] ||
                event.return_tick != after || event.return_tick != event.hit_tick + 1u) {
                (void)fail("timer completion-address watch did not match a guest write");
                goto done;
            }
            if (event.value == 0u && initial_done_clear_seen == 0) {
                uint8_t initial_progress = 0u;
                uint8_t initial_model = 0u;
                if (lynx_read_memory(instance.machine, symbols.value[4], &initial_progress, 1u) != LYNX_OK ||
                    lynx_read_memory(instance.machine, symbols.value[5], &initial_model, 1u) != LYNX_OK ||
                    initial_progress != 0u || initial_model != 0u ||
                    lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                    (void)fail("timer wrapper initial sentinel clear was not an empty-state prefix");
                    goto done;
                }
                initial_done_clear_seen = 1;
                continue;
            }
            if (event.value == 0xA5u && initial_done_clear_seen != 0) {
                complete = 1;
                break;
            }
            (void)fail("timer sentinel value or write order mismatch");
            goto done;
        }
        if (stop != LYNX_STOP_BUDGET) {
            (void)fail("timer guest stopped before completion");
            goto done;
        }
        if (used == 0u || after <= before) {
            (void)fail("timer guest made no bounded progress toward completion");
            goto done;
        }
    }
    if (complete == 0 || initial_done_clear_seen == 0) {
        const lynx__timer *timer3 = &instance.machine->timer_bank.timers[3];
        const lynx__timer *timer5 = &instance.machine->timer_bank.timers[5];
        const uint8_t stack_pointer = instance.machine->cpu.s;
        const uint8_t stacked_status = instance.machine->ram[0x0100u + (uint8_t)(stack_pointer + 1u)];
        const uint8_t stacked_return_low = instance.machine->ram[0x0100u + (uint8_t)(stack_pointer + 2u)];
        const uint8_t stacked_return_high = instance.machine->ram[0x0100u + (uint8_t)(stack_pointer + 3u)];
        fprintf(stderr,
                "timer incomplete: ticks=%" PRIu64 " pc=%04x op=%02x step=%u req=%04x sp=%02x frame=%02x/%02x%02x "
                "a=%02x x=%02x y=%02x p=%02x flags=%02x progress=%02x model=%02x "
                "t3=%02x/%02x/%02x/%02x t5=%02x/%02x/%02x/%02x pending=%02x\n",
                guest_ticks, instance.machine->cpu.pc, instance.machine->cpu.opcode,
                instance.machine->cpu.step, instance.machine->cpu.req_addr, stack_pointer,
                stacked_status, stacked_return_high, stacked_return_low,
                instance.machine->cpu.a, instance.machine->cpu.x, instance.machine->cpu.y,
                instance.machine->cpu.p, instance.machine->cpu.flags,
                instance.machine->ram[symbols.value[4]], instance.machine->ram[symbols.value[5]], timer3->backup, timer3->count,
                timer3->control_a, timer3->control_b, timer5->backup, timer5->count,
                timer5->control_a, timer5->control_b, instance.machine->timer_bank.pending);
    }
    {
        const int execution_started = memcmp(&initial_cpu, &instance.machine->cpu,
                                             sizeof initial_cpu) != 0;
        if (validate_guest_completion(&instance, &symbols, "timers", execution_started, complete,
                                      initial_done_clear_seen, guest_ticks, total_consumed,
                                      TIMER_MIN_TICKS, maximum_ticks,
                                      &done, &progress, &model, &probe) != 0) {
            goto done;
        }
    }
    if (done != 0xA5u || progress != 8u) {
        (void)fail("completion: validated timer completion bytes changed unexpectedly");
        goto done;
    }
    if (lynx_read_memory(instance.machine, symbols.value[2], results, TIMER_MEANINGFUL) != LYNX_OK) {
        (void)fail("validated 16-byte timer result buffer could not be read");
        goto done;
    }
    detected_model = probe_model(probe, model);
    if (write_raw(raw_path, "timers", machine_profile_name, selected_profile_name,
                  "zero", results, TIMER_MEANINGFUL) != 0 ||
        write_timer_run_record(record_path, machine_profile_name, selected_profile_name,
                               bll_sha, labels_sha, oracle_sha, wrapper_sha, guest_ticks,
                               probe, model, progress, symbols.value[11], symbols.value[5],
                               symbols.value[12], symbols.value[13], instance.machine->timer_bank.pending,
                               detected_model) != 0) goto done;
    if (detected_model < 0) {
        result = fail("timer guest model probe/marker evidence is invalid");
        goto done;
    }
    if (detected_model != (int)selected_profile) {
        result = fail("timer guest model does not match selected profile input");
        goto done;
    }
    if (write_timer_comparison(comparison_path, &oracle, results, &comparison_matches) != 0) goto done;
    printf("timer raw source comparison: %s; suite=timers machine_profile=%s profile_input=%s"
           " ticks=%" PRIu64 " results=16/18 detected_model=%s source_tuple_model=not-applicable"
           " guest_model_marker=%02x guest_probe_byte=%02x\n",
           comparison_matches != 0 ? "match" : "mismatch", machine_profile_name, selected_profile_name,
           guest_ticks, detected_model < 0 ? "unknown" : profile_label((unsigned)detected_model), model, probe);
    printf("hardware_accuracy=unverified profile_qualification=unresolved\n");
    if (pending_descriptor_path != NULL) {
        timer_paths observation_paths;
        suite_oracle observation_oracle;
        uint8_t observation_results[TIMER_MEANINGFUL];
        uint32_t observation_ticks = 0u;
        uint32_t agreements = 0u;
        char source_root[4096];
        char observation_bll[65], observation_labels[65], observation_oracle_sha[65];
        if (validate_timer_observation(machine_profile_name, directory, oracle_path,
                                       pending_descriptor_path, &observation_paths,
                                       &observation_oracle, observation_results,
                                       observation_bll, observation_labels,
                                       observation_oracle_sha, &observation_ticks,
                                       &agreements) != 0 ||
            snprintf(source_root, sizeof source_root, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT) >= (int)sizeof source_root ||
            cascade_proof_write(machine_profile_name, directory, oracle_path,
                                pending_descriptor_path, source_root, observation_paths.proof) != 0 ||
            timer_companion_write(machine_profile_name, &observation_paths,
                                  pending_descriptor_path, observation_ticks, agreements) != 0 ||
            timer_companion_validate(machine_profile_name, directory, oracle_path,
                                     pending_descriptor_path, source_root,
                                     observation_paths.raw, observation_paths.record) != 0) goto done;
        printf("timer observation complete: profile=%s meaningful=16 reserved=2 source_agreements=%u pending_index=14 actual=0c expected=0d ticks=%" PRIu32 "\n",
               machine_profile_name, agreements, observation_ticks);
        result = 0;
        goto done;
    }
    if (comparison_matches == 0) result = fail("timer source oracle comparison mismatch");
    else result = 0;
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
}

static int timer_cascade_model_mode(const char *profile, const char *directory,
                                    const char *oracle_path, const char *descriptor_path,
                                    const char *source_root)
{
    timer_paths paths;
    if (timer_paths_make(&paths, directory, profile) != 0 ||
        cascade_proof_write(profile, directory, oracle_path, descriptor_path,
                            source_root, paths.proof) != 0 ||
        timer_companion_validate(profile, directory, oracle_path, descriptor_path,
                                 source_root, paths.raw, paths.record) != 0) return 1;
    printf("timer cascade model: 64 phase offsets derive 12 acknowledgments for %s\n", profile);
    return 0;
}

static int validate_timer_record_mode(const char *profile, const char *raw_path,
                                      const char *record_path, const char *oracle_path,
                                      const char *descriptor_path)
{
    char directory[4096];
    char source_root[4096];
    const char *slash = strrchr(raw_path, '/');
    size_t length;
    int n;
    if (slash == NULL || slash == raw_path) return fail("timer record: raw path must name a file in a build directory");
    length = (size_t)(slash - raw_path);
    if (length >= sizeof directory) return fail("timer record: build directory path too long");
    memcpy(directory, raw_path, length);
    directory[length] = '\0';
    n = snprintf(source_root, sizeof source_root, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT);
    if (n < 0 || (size_t)n >= sizeof source_root) return fail("timer record: pinned source path too long");
    return timer_companion_validate(profile, directory, oracle_path, descriptor_path,
                                    source_root, raw_path, record_path);
}

static int validate_oracle_mode(const char *path)
{
    suite_oracle oracle;
    if (parse_oracle(path, &oracle) != 0) return 1;
    printf("oracle: cpu=18/18 timers=16/18 source identity and alternatives valid\n");
    return 0;
}

static int validate_text_token_mode(const char *path, const char *limit_text,
                                    const char *token)
{
    uint32_t parsed_limit;
    char *text = NULL;
    size_t length = 0u;
    if (parse_decimal(limit_text, &parsed_limit) != 0 || parsed_limit > 16384u) {
        return fail("text control: invalid bound");
    }
    if (read_text(path, &text, &length, (size_t)parsed_limit) != 0) return 1;
    if (strstr(text, token) == NULL) {
        free(text);
        return fail("text control: token missing");
    }
    free(text);
    printf("text control: token present within %zu bytes\n", length);
    return 0;
}

static int validate_symbols_mode(const char *path)
{
    char *text = NULL;
    size_t length = 0u;
    suite_symbols symbols;
    if (read_text(path, &text, &length, 16384u) != 0) return 1;
    if (parse_symbols(text, length, &symbols) != 0 ||
        ranges_valid(&symbols, RESULT_EXTENT) != 0) {
        free(text);
        return 1;
    }
    free(text);
    printf("symbols: required suite labels and nonoverlapping extents valid\n");
    return 0;
}

static int validate_time_mode(const char *text)
{
    uint32_t ticks;
    if (parse_decimal(text, &ticks) != 0) return fail("time: invalid guest tick integer");
    if (ticks < CPU_MIN_TICKS || ticks > CPU_MAX_TICKS) {
        return fail("time: guest ticks outside bounded completion window");
    }
    printf("suite time: accepted guest ticks=%" PRIu32 "\n", ticks);
    return 0;
}

static int validate_selection_list(const char *text, const char *kind,
                                   const char *const *allowed, size_t allowed_count)
{
    unsigned char seen[8] = {0u};
    char buffer[1024];
    char *cursor;
    size_t length;
    if (text == NULL || text[0] == '\0') {
        char message[128];
        (void)snprintf(message, sizeof message, "selection: %s selection is empty", kind);
        return fail(message);
    }
    length = strlen(text);
    if (length >= sizeof buffer || allowed_count > sizeof seen) {
        return fail("selection: case selection is too large");
    }
    memcpy(buffer, text, length + 1u);
    cursor = buffer;
    for (;;) {
        char *comma = strchr(cursor, ',');
        size_t i;
        int found = -1;
        if (comma != NULL) *comma = '\0';
        if (cursor[0] == '\0') return fail("selection: empty case ID");
        for (i = 0u; i < allowed_count; i++) {
            if (strcmp(cursor, allowed[i]) == 0) {
                found = (int)i;
                break;
            }
        }
        if (found < 0) {
            char message[160];
            (void)snprintf(message, sizeof message, "selection: unknown %s case ID", kind);
            return fail(message);
        }
        if (seen[(size_t)found] != 0u) {
            char message[160];
            (void)snprintf(message, sizeof message, "selection: duplicate %s case ID", kind);
            return fail(message);
        }
        seen[(size_t)found] = 1u;
        if (comma == NULL) break;
        cursor = comma + 1;
        if (cursor[0] == '\0') return fail("selection: empty case ID");
    }
    {
        size_t i;
        for (i = 0u; i < allowed_count; i++) {
            if (seen[i] == 0u) {
                char message[160];
                (void)snprintf(message, sizeof message, "selection: incomplete %s inventory", kind);
                return fail(message);
            }
        }
    }
    return 0;
}

static int validate_selection_mode(const char *required, const char *controls)
{
    static const char *const required_ids[] = {
        "suite.cpu.lynx1", "suite.cpu.lynx2",
        "suite.timers.observation.lynx1", "suite.timers.observation.lynx2"
    };
    static const char *const control_ids[] = {
        "suite.cpu.no_run", "suite.timers.no_run",
        "suite.completion_controls", "suite.selection_controls"
    };
    if (validate_selection_list(required, "required-suite", required_ids,
                                sizeof required_ids / sizeof required_ids[0]) != 0 ||
        validate_selection_list(controls, "control", control_ids,
                                sizeof control_ids / sizeof control_ids[0]) != 0) return 1;
    puts("suite selection: required=4 controls=4 inventory complete");
    return 0;
}

static int cpu_record_field(const char *path, const char *key,
                           char *value, size_t capacity)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    if (read_text(path, &text, &length, 16384u) != 0) return 1;
    while (offset < length) {
        size_t line_length = 0u;
        char line[256];
        char *equals;
        while (offset + line_length < length && text[offset + line_length] != '\n') line_length++;
        if (line_length == 0u || line_length >= sizeof line) {
            free(text);
            return fail("cpu record: malformed field line");
        }
        memcpy(line, text + offset, line_length);
        line[line_length] = '\0';
        offset += line_length + (offset + line_length < length ? 1u : 0u);
        equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0' || strchr(equals + 1, '=') != NULL) {
            free(text);
            return fail("cpu record: malformed field line");
        }
        *equals = '\0';
        if (strcmp(line, key) == 0) {
            if (found++ != 0u || copy_field(value, capacity, equals + 1) != 0) {
                free(text);
                return fail("cpu record: duplicate or oversized field");
            }
        }
    }
    free(text);
    if (found != 1u) {
        char message[128];
        (void)snprintf(message, sizeof message, "cpu record missing field: %s", key);
        return fail(message);
    }
    return 0;
}

static int read_cpu_raw(const char *path, const char *machine_profile,
                        const char *selected_profile, const char *case_name,
                        uint8_t results[CPU_MEANINGFUL])
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[256];
    char *fields[4];
    size_t field_count;
    uint32_t value;
    uint32_t i;
    if (read_text(path, &text, &length, 4096u) != 0) return 1;
#define EXPECT_RAW_ROW(expected_key, expected_value, diagnostic) \
    do { \
        if (next_line(text, length, &offset, line, sizeof line) != 0 || \
            (field_count = split_fields(line, fields, 4u)) != 2u || \
            strcmp(fields[0], (expected_key)) != 0 || \
            strcmp(fields[1], (expected_value)) != 0) { \
            free(text); \
            return fail((diagnostic)); \
        } \
    } while (0)
    EXPECT_RAW_ROW("suite", "cpu", "cpu record: raw suite identity mismatch");
    EXPECT_RAW_ROW("machine_profile", machine_profile, "cpu record: raw machine profile mismatch");
    EXPECT_RAW_ROW("profile_input", selected_profile, "cpu record: raw selected profile mismatch");
    EXPECT_RAW_ROW("ram_seed", case_name, "cpu record: raw RAM case mismatch");
    if (next_line(text, length, &offset, line, sizeof line) != 0 ||
        (field_count = split_fields(line, fields, 4u)) != 2u ||
        strcmp(fields[0], "result_count") != 0 ||
        parse_decimal(fields[1], &value) != 0 || value != CPU_MEANINGFUL) {
        free(text);
        return fail("cpu record: result count drift");
    }
    for (i = 0u; i < CPU_MEANINGFUL; i++) {
        uint32_t index;
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            (field_count = split_fields(line, fields, 4u)) != 3u ||
            strcmp(fields[0], "result") != 0 ||
            parse_decimal(fields[1], &index) != 0 || index != i ||
            parse_byte(fields[2], &results[i]) != 0) {
            free(text);
            return fail("cpu record: result count drift or malformed raw byte");
        }
    }
    if (offset != length) {
        free(text);
        return fail("cpu record: result count drift or trailing raw data");
    }
#undef EXPECT_RAW_ROW
    free(text);
    return 0;
}

static int validate_cpu_record_mode(const char *selected_profile,
                                    const char *raw_path, const char *record_path,
                                    const char *oracle_path, const char *artifact_directory)
{
    char suite[32];
    char machine_profile[32];
    char record_selected_profile[32];
    char case_name[32];
    char detected_model[32];
    char source_tuple[32];
    char model_bytes[32];
    char value[64];
    char oracle_sha[65];
    char actual_oracle_sha[65];
    char bll_sha[65];
    char labels_sha[65];
    char actual_bll_sha[65];
    char actual_labels_sha[65];
    char bll_path[4096];
    char labels_path[4096];
    char *comma;
    unsigned selected_index;
    unsigned machine_index;
    uint32_t meaningful_count;
    uint32_t reserved_count;
    uint32_t tick_min;
    uint32_t tick_max;
    uint32_t ticks;
    uint32_t guest_ticks;
    uint32_t completion_margin;
    uint32_t maximum_headroom;
    uint32_t progress;
    uint8_t probe;
    uint8_t marker;
    uint8_t results[CPU_MEANINGFUL];
    suite_oracle oracle;
    int probe_index;
    int tuple_index;
    int i;

    if (profile_index(selected_profile, &selected_index) != 0) {
        return fail("cpu record: explicit lynx1 or lynx2 profile input required");
    }
    if (parse_oracle(oracle_path, &oracle) != 0 || file_sha256(oracle_path, actual_oracle_sha) != 0 ||
        checked_join(bll_path, sizeof bll_path, artifact_directory, "cpu.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, artifact_directory, "cpu.lbl") != 0 ||
        file_sha256(bll_path, actual_bll_sha) != 0 || file_sha256(labels_path, actual_labels_sha) != 0) {
        return fail("cpu record: identity artifacts missing");
    }
#define READ_CPU_FIELD(key, target, target_size) \
    do { if (cpu_record_field(record_path, (key), (target), (target_size)) != 0) return 1; } while (0)
    READ_CPU_FIELD("suite", suite, sizeof suite);
    READ_CPU_FIELD("machine_profile", machine_profile, sizeof machine_profile);
    READ_CPU_FIELD("profile_input", record_selected_profile, sizeof record_selected_profile);
    READ_CPU_FIELD("ram_seed", case_name, sizeof case_name);
    READ_CPU_FIELD("detected_model", detected_model, sizeof detected_model);
    READ_CPU_FIELD("source_tuple_model", source_tuple, sizeof source_tuple);
    READ_CPU_FIELD("model_probe_bytes", model_bytes, sizeof model_bytes);
    READ_CPU_FIELD("meaningful_count", value, sizeof value);
    if (parse_decimal(value, &meaningful_count) != 0) return fail("cpu record: result count drift");
    READ_CPU_FIELD("reserved_count", value, sizeof value);
    if (parse_decimal(value, &reserved_count) != 0 || meaningful_count != CPU_MEANINGFUL || reserved_count != 0u) {
        return fail("cpu record: result count drift");
    }
    READ_CPU_FIELD("tick_min", value, sizeof value);
    if (parse_decimal(value, &tick_min) != 0) return fail("cpu record: invalid tick_min");
    READ_CPU_FIELD("tick_max", value, sizeof value);
    if (parse_decimal(value, &tick_max) != 0) return fail("cpu record: invalid tick_max");
    READ_CPU_FIELD("ticks_observed", value, sizeof value);
    if (parse_decimal(value, &ticks) != 0) return fail("cpu record: invalid ticks_observed");
    READ_CPU_FIELD("guest_ticks", value, sizeof value);
    if (parse_decimal(value, &guest_ticks) != 0 || guest_ticks != ticks) return fail("cpu record: guest tick records disagree");
    READ_CPU_FIELD("completion_margin_ticks", value, sizeof value);
    if (parse_decimal(value, &completion_margin) != 0) return fail("cpu record: invalid completion margin");
    READ_CPU_FIELD("maximum_headroom_ticks", value, sizeof value);
    if (parse_decimal(value, &maximum_headroom) != 0) return fail("cpu record: invalid maximum headroom");
    READ_CPU_FIELD("guest_progress", value, sizeof value);
    if (parse_decimal(value, &progress) != 0 || progress != 8u) return fail("cpu record: guest progress evidence mismatch");
    READ_CPU_FIELD("completion_sentinel", value, sizeof value);
    if (strcmp(value, "a5") != 0) return fail("cpu record: completion sentinel mismatch");
    READ_CPU_FIELD("bll_sha256", bll_sha, sizeof bll_sha);
    if (!valid_hex(bll_sha, 64u) || strcmp(bll_sha, actual_bll_sha) != 0) {
        return fail("cpu record: BLL object identity mismatch");
    }
    READ_CPU_FIELD("labels_sha256", labels_sha, sizeof labels_sha);
    if (!valid_hex(labels_sha, 64u) || strcmp(labels_sha, actual_labels_sha) != 0) {
        return fail("cpu record: labels identity mismatch");
    }
    READ_CPU_FIELD("oracle_sha256", oracle_sha, sizeof oracle_sha);
    if (strcmp(oracle_sha, actual_oracle_sha) != 0) return fail("cpu record: source identity mismatch");
#undef READ_CPU_FIELD

    if (strcmp(suite, "cpu") != 0 || profile_index(machine_profile, &machine_index) != 0 ||
        strcmp(machine_profile, selected_profile) != 0 ||
        strcmp(record_selected_profile, selected_profile) != 0 ||
        (strcmp(case_name, "zero") != 0 && strcmp(case_name, "seeded") != 0)) {
        return fail("cpu record: run identity mismatch");
    }
    if (tick_min != CPU_MIN_TICKS || tick_max != CPU_MAX_TICKS) return fail("cpu record: completion bound metadata mismatch");
    if (ticks < tick_min || ticks > tick_max) return fail("cpu record: ticks outside completion bounds");
    if (completion_margin != ticks - tick_min || maximum_headroom != tick_max - ticks) {
        return fail("cpu record: completion margin mismatch");
    }
    comma = strchr(model_bytes, ',');
    if (comma == NULL || (size_t)(comma - model_bytes) != 2u || strlen(comma + 1) != 2u) {
        return fail("cpu record: invalid model probe bytes");
    }
    *comma = '\0';
    if (parse_byte(model_bytes, &probe) != 0 || parse_byte(comma + 1, &marker) != 0) {
        return fail("cpu record: invalid model probe bytes");
    }
    if (read_cpu_raw(raw_path, machine_profile, selected_profile, case_name, results) != 0) return 1;
    {
        int all_zero = 1;
        for (i = 0; i < (int)CPU_MEANINGFUL; i++) if (results[i] != 0u) all_zero = 0;
        if (all_zero != 0) return fail("cpu record: zero/unexecuted result block");
    }
    probe_index = probe_model(probe, marker);
    tuple_index = tuple_model(&oracle, results);
    if (probe_index < 0) return fail("cpu record: invalid model probe bytes");
    if (tuple_index < 0) return fail("cpu record: raw results lack exactly one source model tuple");
    if (probe_index != tuple_index) return fail("cpu record: guest model probe and source-defined tuple disagree");
    if (probe_index != (int)machine_index) return fail("cpu record: observed model disagrees with executed machine profile");
    if (strcmp(detected_model, profile_label((unsigned)probe_index)) != 0 ||
        strcmp(source_tuple, profile_label((unsigned)tuple_index)) != 0) {
        return fail("cpu record: detected model fields disagree with guest evidence");
    }
    for (i = 0; i < (int)CPU_MEANINGFUL; i++) {
        size_t j;
        int accepted = 0;
        for (j = 0u; j < oracle.counts[0][i]; j++) {
            if (oracle.values[0][i][j] == results[i]) accepted = 1;
        }
        if (accepted == 0) return fail("cpu record: source oracle mismatch");
    }
    if (probe_index != (int)selected_index) return fail("cpu record: detected model does not match selected profile");
    printf("cpu record: 18 raw results, completion and model evidence valid for %s\n", selected_profile);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 6 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "cpu") == 0) {
        return run_cpu(argv[3], argv[3], argv[4], argv[5], "zero", 0, CPU_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "cpu") == 0) {
        return run_cpu(argv[3], argv[3], argv[4], argv[5], argv[6], 0, CPU_MAX_TICKS);
    }
    if (argc == 8 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "cpu-selected") == 0) {
        return run_cpu(argv[3], argv[4], argv[5], argv[6], argv[7], 0, CPU_MAX_TICKS);
    }
    if (argc == 6 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "timers") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], NULL, 0, TIMER_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "timers-selected") == 0) {
        return run_timers(argv[3], argv[4], argv[5], argv[6], NULL, 0, TIMER_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "observe") == 0 && strcmp(argv[2], "timers") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], argv[6], 0, TIMER_MAX_TICKS);
    }
    if (argc == 6 && strcmp(argv[1], "no-run") == 0 && strcmp(argv[2], "cpu") == 0) {
        return run_cpu(argv[3], argv[3], argv[4], argv[5], "zero", 1, CPU_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "no-run") == 0 && strcmp(argv[2], "timers") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], argv[6], 1, TIMER_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "run-bounded") == 0 && strcmp(argv[2], "cpu") == 0) {
        uint32_t maximum_ticks;
        if (parse_decimal(argv[6], &maximum_ticks) != 0) return fail("time: invalid guest tick integer");
        return run_cpu(argv[3], argv[3], argv[4], argv[5], "zero", 0, maximum_ticks);
    }
    if (argc == 7 && strcmp(argv[1], "run-bounded") == 0 && strcmp(argv[2], "timers") == 0) {
        uint32_t maximum_ticks;
        if (parse_decimal(argv[6], &maximum_ticks) != 0) return fail("time: invalid guest tick integer");
        return run_timers(argv[3], argv[3], argv[4], argv[5], NULL, 0, maximum_ticks);
    }
    if (argc == 7 && strcmp(argv[1], "timer-cascade-model") == 0) {
        return timer_cascade_model_mode(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
    if (argc == 8 && strcmp(argv[1], "validate-timer-comparison") == 0) {
        return validate_timer_comparison_corpus(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
    }
    if (argc == 7 && strcmp(argv[1], "validate-timer-record") == 0) {
        return validate_timer_record_mode(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
    if (argc == 3 && strcmp(argv[1], "validate-oracle") == 0) return validate_oracle_mode(argv[2]);
    if (argc == 5 && strcmp(argv[1], "validate-text-token") == 0) {
        return validate_text_token_mode(argv[2], argv[3], argv[4]);
    }
    if (argc == 3 && strcmp(argv[1], "validate-symbols") == 0) return validate_symbols_mode(argv[2]);
    if (argc == 3 && strcmp(argv[1], "validate-time") == 0) return validate_time_mode(argv[2]);
    if (argc == 4 && strcmp(argv[1], "validate-selection") == 0) {
        return validate_selection_mode(argv[2], argv[3]);
    }
    if (argc == 7 && strcmp(argv[1], "validate-cpu-record") == 0) {
        return validate_cpu_record_mode(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
    if (argc == 5 && strcmp(argv[1], "validate-pending-descriptor") == 0) {
        if (pending_descriptor_validate(argv[2], argv[3], argv[4]) != 0) return 1;
        puts("pending descriptor: exact D-10 source, policy and environment identity valid");
        return 0;
    }
    fprintf(stderr, "usage: lynx-test-suites run cpu PROFILE BUILDDIR ORACLE [zero|seeded]\n");
    return 2;
}
