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

#include "lynxplosion.h"
#include "sha256.h"

enum {
    BLL_HEADER_SIZE = 10,
    RESULT_EXTENT = 18,
    CPU_MEANINGFUL = 18,
    CPU_MIN_TICKS = 256,
    CPU_MAX_TICKS = 50000000,
    SYMBOL_LINE_MAX = 255,
    ORACLE_LINE_MAX = 255,
    EXPECT_VALUES_MAX = 8
};

typedef struct suite_symbols {
    uint32_t value[12];
    uint8_t found[12];
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

static const char *const symbol_names[12] = {
    "suite_entry", "suite_test_entry", "_g_results", "suite_result_end",
    "suite_progress", "suite_model", "suite_done", "suite_end",
    "_run_tests", "suite_first_test", "suite_first_mapped", "suite_probe_result"
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
    data = malloc(limit == 0u ? 1u : limit);
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
    const char *destinations[7];

    memset(out, 0, sizeof *out);
    destinations[0] = out->archive;
    destinations[1] = out->revision;
    destinations[2] = out->cpu_main;
    destinations[3] = out->cpu_asm;
    destinations[4] = out->timers_main;
    destinations[5] = out->timers_asm;
    destinations[6] = out->license;
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
            copy_field((char *)destinations[i], 65u, fields[1]) != 0) {
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
    for (i = 0u; i < 12u; i++) if (strcmp(name, symbol_names[i]) == 0) return (int)i;
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
        symbols->value[10] < code_start || symbols->value[10] >= code_end) {
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

static int clear_old_outputs(const char *directory)
{
    static const char *const names[] = {
        "run-record.txt", "raw-results.tsv", "comparison.tsv"
    };
    struct stat info;
    char path[4096];
    size_t i;
    if (lstat(directory, &info) != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
        return fail("build directory must be a real invocation-owned directory");
    }
    for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
        if (checked_join(path, sizeof path, directory, names[i]) != 0) return fail("output path too long");
        if (remove(path) != 0) {
            struct stat existing;
            if (lstat(path, &existing) == 0) return fail("cannot clear stale suite output");
        }
    }
    return 0;
}

static int write_raw(const char *path, const char *suite, const char *profile,
                     const uint8_t *results, uint32_t count)
{
    FILE *file = fopen(path, "wb");
    uint32_t i;
    if (file == NULL) return fail("cannot create raw result record");
    (void)fprintf(file, "suite\t%s\nprofile_input\t%s\n", suite, profile);
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

static int write_run_record(const char *path, const char *profile,
                            const char *bll_sha, const char *labels_sha,
                            const char *oracle_sha, uint64_t ticks,
                            uint8_t probe, uint8_t model, uint8_t progress)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) return fail("cannot create suite run record");
    (void)fprintf(file,
        "evidence=raw-observation-plus-source-comparison\n"
        "suite=cpu\nprofile_input=%s\nbll_sha256=%s\nlabels_sha256=%s\noracle_sha256=%s\n"
        "guest_ticks=%" PRIu64 "\nminimum_guest_ticks=%u\nmaximum_guest_ticks=%u\n"
        "meaningful_results=18\nreserved_results=0\nguest_probe_byte=%02x\n"
        "guest_model_marker=%02x\nguest_progress=%u\ncompletion_sentinel=a5\n"
        "scheduler_policy=timer-cpu-poll-order-1\nphase_policy=timer-phase-fixed-zero-1\n"
        "hardware_accuracy=unverified\n",
        profile, bll_sha, labels_sha, oracle_sha, ticks, CPU_MIN_TICKS, CPU_MAX_TICKS,
        probe, model, progress);
    if (fclose(file) != 0) return fail("cannot close suite run record");
    return 0;
}

static int run_cpu(const char *profile_name, const char *directory, const char *oracle_path)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char path[4096];
    char bll_path[4096];
    char labels_path[4096];
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
    uint32_t profile;
    uint8_t expected_probe;
    uint8_t expected_model;
    uint64_t guest_ticks = 0u;
    uint64_t total_consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    lynx_identity identity;
    machine_instance instance = {0};
    suite_symbols symbols;
    suite_oracle oracle;
    int complete = 0;
    int initial_done_clear_seen = 0;
    int result = 1;
    uint32_t i;

    if (strcmp(profile_name, "lynx1") == 0) {
        profile = LYNX_PROFILE_LYNX1;
        expected_probe = 1u;
        expected_model = 0xA1u;
    } else if (strcmp(profile_name, "lynx2") == 0) {
        profile = LYNX_PROFILE_LYNX2;
        expected_probe = 0u;
        expected_model = 0xA2u;
    } else return fail("explicit lynx1 or lynx2 profile input required");
    if (clear_old_outputs(directory) != 0) return 1;
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0) goto done;
    if (checked_join(bll_path, sizeof bll_path, directory, "cpu.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, directory, "cpu.lbl") != 0 ||
        checked_join(record_path, sizeof record_path, directory, "run-record.txt") != 0 ||
        checked_join(raw_path, sizeof raw_path, directory, "raw-results.tsv") != 0 ||
        checked_join(comparison_path, sizeof comparison_path, directory, "comparison.tsv") != 0) {
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
    if (machine_create(&instance, bll, bll_length, profile) != 0 ||
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
    while (total_consumed < CPU_MAX_TICKS) {
        uint64_t budget = CPU_MAX_TICKS - total_consumed;
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
        if (after < before || after - before != used || after > CPU_MAX_TICKS) {
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
    if (complete == 0 || initial_done_clear_seen == 0) {
        (void)fail("guest did not reach completion before maximum ticks");
        goto done;
    }
    if (lynx_read_memory(instance.machine, symbols.value[6], &done, 1u) != LYNX_OK ||
        lynx_read_memory(instance.machine, symbols.value[4], &progress, 1u) != LYNX_OK ||
        lynx_read_memory(instance.machine, symbols.value[5], &model, 1u) != LYNX_OK ||
        lynx_read_memory(instance.machine, symbols.value[11], &probe, 1u) != LYNX_OK) {
        (void)fail("completion observation could not read validated guest bytes");
        goto done;
    }
    if (guest_ticks < CPU_MIN_TICKS || guest_ticks > CPU_MAX_TICKS ||
        total_consumed == 0u || done != 0xA5u || progress != 8u ||
        probe != expected_probe || model != expected_model) {
        (void)fail("completion, progress, source-work bound, or guest model evidence mismatch");
        goto done;
    }
    if (lynx_read_memory(instance.machine, symbols.value[2], results, CPU_MEANINGFUL) != LYNX_OK) {
        (void)fail("validated CPU result buffer could not be read");
        goto done;
    }
    for (i = 0u; i < 4u; i++) {
        if (results[12u + i] != oracle.tuple[profile == LYNX_PROFILE_LYNX1 ? 0u : 1u][i]) {
            (void)fail("upstream CPU model tuple differs from selected profile input");
            if (write_raw(raw_path, "cpu", profile_name, results, CPU_MEANINGFUL) != 0) goto done;
            goto done;
        }
    }
    if (write_raw(raw_path, "cpu", profile_name, results, CPU_MEANINGFUL) != 0 ||
        write_run_record(record_path, profile_name, identity_sha, labels_sha,
                         oracle_sha, guest_ticks, probe, model, progress) != 0) goto done;
    result = write_comparison(comparison_path, &oracle, results, CPU_MEANINGFUL);
    if (result == 0) {
        printf("suite raw source comparison: match; suite=cpu profile_input=%s ticks=%" PRIu64
               " results=%u guest_model_marker=%02x guest_probe_byte=%02x\n",
               profile_name, guest_ticks, CPU_MEANINGFUL, model, probe);
        printf("hardware_accuracy=unverified profile_qualification=unresolved\n");
    }
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
}

static int validate_oracle_mode(const char *path)
{
    suite_oracle oracle;
    if (parse_oracle(path, &oracle) != 0) return 1;
    printf("oracle: cpu=18/18 timers=16/18 source identity and alternatives valid\n");
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

int main(int argc, char **argv)
{
    if (argc == 6 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "cpu") == 0) {
        return run_cpu(argv[3], argv[4], argv[5]);
    }
    if (argc == 3 && strcmp(argv[1], "validate-oracle") == 0) return validate_oracle_mode(argv[2]);
    if (argc == 3 && strcmp(argv[1], "validate-symbols") == 0) return validate_symbols_mode(argv[2]);
    fprintf(stderr, "usage: lynx-test-suites run cpu PROFILE BUILDDIR ORACLE\n");
    return 2;
}
