/* Headless public-API harness for exact, source-generated suite evidence.
   This intentionally distinguishes raw guest observations from their
   comparison with the pinned software oracle. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
    AUDIO_MEANINGFUL = 18,
    AUDIO_MIN_TICKS = 256,
    AUDIO_MAX_TICKS = 50000000,
    AUDIO_SUITE_COUNT = 2,
    AUDIO_CASES_MAX = 7,
    AUDIO_CASE_LABEL_MAX = 32,
    SOURCE_SUITE_COUNT = 2,
    SOURCE_GROUPS_MAX = 8,
    SOURCE_GROUP_LABEL_MAX = 32,
    SOURCE_ORACLE_MAX = 16384,
    SOURCE_TICK_MIN = 256,
    SOURCE_TICK_MAX = 1000000,
    PAGE_MODE_RESULT_COUNT = 6,
    PAGE_MODE_MIN_TICKS = 40000,
    PAGE_MODE_MAX_TICKS = 50000000,
    JSON_MAX_FIELDS = 48,
    JSON_ARRAY_MAX = 8,
    JSON_TEXT_MAX = 256,
    SYMBOL_LINE_MAX = 255,
    ORACLE_LINE_MAX = 255,
    EXPECT_VALUES_MAX = 8,
    SUITE_ZP_END = 0x0100,
    SUITE_STACK_START = 0x0100,
    SUITE_STACK_END = 0x0200,
    SUITE_PAYLOAD_START = 0x0200,
    SUITE_PAYLOAD_END = 0x6200,
    SUITE_BSS_START = 0x8000,
    SUITE_BSS_END = 0xC000
};

typedef struct suite_symbols {
    uint32_t value[15];
    uint8_t found[15];
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

typedef struct suite_audio_oracle {
    char main_sha256[AUDIO_SUITE_COUNT][65];
    char asm_sha256[AUDIO_SUITE_COUNT][65];
    uint8_t values[AUDIO_SUITE_COUNT][AUDIO_MEANINGFUL][EXPECT_VALUES_MAX];
    uint8_t counts[AUDIO_SUITE_COUNT][AUDIO_MEANINGFUL];
    uint8_t case_start[AUDIO_SUITE_COUNT][AUDIO_CASES_MAX];
    uint8_t case_length[AUDIO_SUITE_COUNT][AUDIO_CASES_MAX];
    char case_label[AUDIO_SUITE_COUNT][AUDIO_CASES_MAX][AUDIO_CASE_LABEL_MAX];
    uint8_t case_count[AUDIO_SUITE_COUNT];
} suite_audio_oracle;

typedef struct suite_source_oracle {
    char archive[65];
    char revision[41];
    char license[65];
    char main_sha256[SOURCE_SUITE_COUNT][65];
    char asm_sha256[SOURCE_SUITE_COUNT][65];
    uint8_t values[SOURCE_SUITE_COUNT][RESULT_EXTENT][EXPECT_VALUES_MAX];
    uint8_t counts[SOURCE_SUITE_COUNT][RESULT_EXTENT];
    uint8_t meaningful[SOURCE_SUITE_COUNT];
    uint8_t reserved_start[SOURCE_SUITE_COUNT];
    uint8_t reserved_count[SOURCE_SUITE_COUNT];
    uint8_t group_count[SOURCE_SUITE_COUNT];
    uint8_t group_start[SOURCE_SUITE_COUNT][SOURCE_GROUPS_MAX];
    uint8_t group_length[SOURCE_SUITE_COUNT][SOURCE_GROUPS_MAX];
    char group_kind[SOURCE_SUITE_COUNT][SOURCE_GROUPS_MAX][16];
    char group_label[SOURCE_SUITE_COUNT][SOURCE_GROUPS_MAX][SOURCE_GROUP_LABEL_MAX];
} suite_source_oracle;

typedef struct page_mode_oracle {
    char revision[41];
    char main_sha256[65];
    char tests_sha256[65];
    char notice_sha256[65];
    char recipe_sha256[65];
    uint8_t values[PAGE_MODE_RESULT_COUNT][2];
    uint8_t counts[PAGE_MODE_RESULT_COUNT];
} page_mode_oracle;

typedef struct page_mode_build_identity {
    char object_sha256[65];
    char labels_sha256[65];
    char oracle_sha256[65];
    char build_record_sha256[65];
    char wrapper_sha256[65];
    char layout_sha256[65];
    char manifest_sha256[65];
    char recipe_sha256[65];
    char oracle_recipe_sha256[65];
    char ca65_sha256[65];
    char ld65_sha256[65];
} page_mode_build_identity;

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

typedef struct atomic_output {
    FILE *stream;
    char temporary_path[4096];
    char final_path[4096];
} atomic_output;

static const char *const symbol_names[15] = {
    "suite_entry", "suite_test_entry", "_g_results", "suite_result_end",
    "suite_progress", "suite_model", "suite_done", "suite_end",
    "_run_tests", "suite_first_test", "suite_first_mapped", "suite_probe_result",
    "suite_test_return", "suite_after_tests", "suite_idle"
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

static int validate_owned_output_directory(const char *directory)
{
#ifndef LYNX_SCRATCH_ROOT
#error LYNX_SCRATCH_ROOT must be defined for suite output ownership
#endif
    char canonical_root[4096];
    char canonical_directory[4096];
    char marker_path[4096];
    char expected_prefix[8192];
    char marker[8192];
    struct stat info;
    struct stat marker_info;
    size_t prefix_length;
    size_t length = 0u;
    size_t i;
    ssize_t count;
    int marker_fd;
    int n;
    if (realpath(LYNX_SCRATCH_ROOT, canonical_root) == NULL ||
        strcmp(canonical_root, LYNX_SCRATCH_ROOT) != 0 ||
        lstat(directory, &info) != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) ||
        realpath(directory, canonical_directory) == NULL ||
        strcmp(canonical_directory, directory) != 0) {
        return fail("build directory is not a canonical invocation-owned directory");
    }
    prefix_length = strlen(canonical_root);
    if (strncmp(canonical_directory, canonical_root, prefix_length) != 0 ||
        canonical_directory[prefix_length] != '/' ||
        canonical_directory[prefix_length + 1u] == '\0') {
        return fail("build directory is outside invocation-owned scratch");
    }
    n = snprintf(marker_path, sizeof marker_path, "%s/.lynx-suite-owner", directory);
    if (n < 0 || (size_t)n >= sizeof marker_path) return fail("build directory owner marker path too long");
    if (lstat(marker_path, &marker_info) != 0 || !S_ISREG(marker_info.st_mode) || S_ISLNK(marker_info.st_mode)) {
        return fail("build directory owner marker is missing or invalid");
    }
    marker_fd = open(marker_path, O_RDONLY);
    if (marker_fd < 0 || fstat(marker_fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_dev != marker_info.st_dev || info.st_ino != marker_info.st_ino) {
        if (marker_fd >= 0) (void)close(marker_fd);
        return fail("build directory owner marker is missing or invalid");
    }
    while (length < sizeof marker - 1u) {
        count = read(marker_fd, marker + length, sizeof marker - 1u - length);
        if (count < 0) {
            (void)close(marker_fd);
            return fail("build directory owner marker is unreadable");
        }
        if (count == 0) break;
        length += (size_t)count;
    }
    if (length == sizeof marker - 1u) {
        char extra;
        count = read(marker_fd, &extra, 1u);
        if (count != 0) {
            (void)close(marker_fd);
            return fail("build directory owner marker is malformed");
        }
    }
    if (close(marker_fd) != 0) return fail("build directory owner marker is unreadable");
    n = snprintf(expected_prefix, sizeof expected_prefix,
                 "lynx-suite-output-v1\n%s\n", canonical_directory);
    if (n < 0 || (size_t)n >= sizeof expected_prefix) return fail("build directory owner marker path too long");
    prefix_length = (size_t)n;
    if (length != prefix_length + 49u || memcmp(marker, expected_prefix, prefix_length) != 0 ||
        marker[length - 1u] != '\n') return fail("build directory owner marker is malformed");
    for (i = prefix_length; i < length - 1u; i++) {
        if (hex_digit((unsigned char)marker[i]) < 0) return fail("build directory owner marker is malformed");
    }
    return 0;
}

static int atomic_output_begin(const char *path, atomic_output *output)
{
    char directory[4096];
    char temporary_template[4096];
    const char *slash = strrchr(path, '/');
    int descriptor;
    int n;
    size_t directory_length;
    if (slash == NULL || slash == path || slash[1] == '\0') return fail("output path must be a file in an owned directory");
    directory_length = (size_t)(slash - path);
    if (directory_length >= sizeof directory) return fail("output directory path too long");
    memcpy(directory, path, directory_length);
    directory[directory_length] = '\0';
    if (validate_owned_output_directory(directory) != 0) return 1;
    n = snprintf(temporary_template, sizeof temporary_template,
                 "%s/.lynx-suite-write-XXXXXX", directory);
    if (n < 0 || (size_t)n >= sizeof temporary_template ||
        strlen(path) >= sizeof output->final_path) return fail("output path too long");
    descriptor = mkstemp(temporary_template);
    if (descriptor < 0) return fail("cannot exclusively create suite output temporary");
    if (fchmod(descriptor, S_IRUSR | S_IWUSR) != 0) {
        (void)close(descriptor);
        (void)remove(temporary_template);
        return fail("cannot restrict suite output temporary permissions");
    }
    output->stream = fdopen(descriptor, "wb");
    if (output->stream == NULL) {
        (void)close(descriptor);
        (void)remove(temporary_template);
        return fail("cannot open suite output temporary stream");
    }
    (void)snprintf(output->temporary_path, sizeof output->temporary_path, "%s", temporary_template);
    (void)snprintf(output->final_path, sizeof output->final_path, "%s", path);
    return 0;
}

static void atomic_output_abort(atomic_output *output)
{
    if (output->stream != NULL) {
        (void)fclose(output->stream);
        output->stream = NULL;
    }
    if (output->temporary_path[0] != '\0') (void)remove(output->temporary_path);
}

static int atomic_output_commit(atomic_output *output)
{
    struct stat info;
    int stream_error = ferror(output->stream) != 0 || fflush(output->stream) != 0;
    int close_error = fclose(output->stream) != 0;
    output->stream = NULL;
    if (stream_error != 0 || close_error != 0 ||
        rename(output->temporary_path, output->final_path) != 0 ||
        lstat(output->final_path, &info) != 0 || !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) {
        (void)remove(output->temporary_path);
        return fail("cannot atomically publish suite output");
    }
    output->temporary_path[0] = '\0';
    return 0;
}

static int create_owned_temp_slot(const char *directory, const char *prefix, char path[4096])
{
    char template[4096];
    int descriptor;
    int n;
    if (validate_owned_output_directory(directory) != 0) return 1;
    n = snprintf(template, sizeof template, "%s/.%s-XXXXXX", directory, prefix);
    if (n < 0 || (size_t)n >= sizeof template) return fail("temporary proof path too long");
    descriptor = mkstemp(template);
    if (descriptor < 0) return fail("cannot exclusively create temporary proof slot");
    if (close(descriptor) != 0 || strlen(template) >= 4096u) {
        (void)remove(template);
        return fail("cannot reserve temporary proof slot");
    }
    (void)snprintf(path, 4096u, "%s", template);
    return 0;
}

static int remove_owned_temp_slot(const char *directory, const char *path)
{
    struct stat info;
    const char *slash = strrchr(path, '/');
    size_t directory_length = strlen(directory);
    if (validate_owned_output_directory(directory) != 0 || slash == NULL ||
        (size_t)(slash - path) != directory_length || strncmp(path, directory, directory_length) != 0 ||
        slash[1] == '\0') return fail("temporary proof path is outside its owned directory");
    if (lstat(path, &info) != 0) return errno == ENOENT ? 0 : fail("cannot inspect temporary proof output");
    if (!S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) return fail("temporary proof output is not a regular file");
    if (remove(path) != 0) return fail("cannot remove temporary proof output");
    return 0;
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

static int parse_math_memio_oracle(const char *path, suite_source_oracle *out)
{
    static const char *const metadata_names[7] = {
        "archive", "revision", "license", "math_main", "math_asm", "memio_main", "memio_asm"
    };
    static const size_t metadata_lengths[7] = {64u, 40u, 64u, 64u, 64u, 64u, 64u};
    static const char *const expected_names[SOURCE_SUITE_COUNT][SOURCE_GROUPS_MAX] = {
        {"SIMPLE MUL", "ACCUM MUL", "SIGNED MUL", "MUL $8000 BUG", "SIMPLE DIV", "NO REM DIV", "DIV BY ZERO", "TIMING"},
        {"MIKEY COLORS", "SUZY SPR REGS", "SUZY MIRRORS"}
    };
    uint8_t covered[SOURCE_SUITE_COUNT][RESULT_EXTENT] = {{0u}};
    uint8_t metadata_seen[7] = {0u};
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[ORACLE_LINE_MAX + 1u];
    char *fields[12];
    size_t count;
    unsigned meta;
    unsigned suite;
    memset(out, 0, sizeof *out);
    if (read_text(path, &text, &length, SOURCE_ORACLE_MAX) != 0) return fail("math/memio oracle unreadable");
    if (next_line(text, length, &offset, line, sizeof line) != 0 ||
        (count = split_fields(line, fields, 12u)) != 2u ||
        strcmp(fields[0], "LYNX-SUITE-MATH-MEMIO-ORACLE") != 0 || strcmp(fields[1], "1") != 0) {
        free(text);
        return fail("math/memio oracle: invalid version header");
    }
    for (meta = 0u; meta < 7u; meta++) {
        char *destination = NULL;
        size_t capacity = 0u;
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 2u || strcmp(fields[0], metadata_names[meta]) != 0 ||
            strlen(fields[1]) != metadata_lengths[meta]) {
            free(text);
            return fail("math/memio oracle: malformed source identity record");
        }
        if (meta == 0u) { destination = out->archive; capacity = sizeof out->archive; }
        else if (meta == 1u) { destination = out->revision; capacity = sizeof out->revision; }
        else if (meta == 2u) { destination = out->license; capacity = sizeof out->license; }
        else if (meta == 3u) { destination = out->main_sha256[0]; capacity = sizeof out->main_sha256[0]; }
        else if (meta == 4u) { destination = out->asm_sha256[0]; capacity = sizeof out->asm_sha256[0]; }
        else if (meta == 5u) { destination = out->main_sha256[1]; capacity = sizeof out->main_sha256[1]; }
        else { destination = out->asm_sha256[1]; capacity = sizeof out->asm_sha256[1]; }
        if (copy_field(destination, capacity, fields[1]) != 0 ||
            !valid_hex(destination, metadata_lengths[meta])) {
            free(text);
            return fail("math/memio oracle: malformed source identity");
        }
        metadata_seen[meta] = 1u;
    }
    for (suite = 0u; suite < SOURCE_SUITE_COUNT; suite++) {
        const char *suite_name = suite == 0u ? "math" : "memio";
        uint32_t meaningful = suite == 0u ? 16u : 13u;
        uint32_t group_count = suite == 0u ? 8u : 3u;
        uint32_t result_count;
        uint32_t extent;
        uint32_t declared_meaningful;
        uint32_t reserved_start;
        uint32_t reserved_count;
        uint32_t i;
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 2u || strcmp(fields[0], "suite") != 0 ||
            strcmp(fields[1], suite_name) != 0) {
            free(text);
            return fail("math/memio oracle: missing or reordered suite section");
        }
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 2u || strcmp(fields[0], "result_count") != 0 ||
            parse_decimal(fields[1], &result_count) != 0 || result_count != RESULT_EXTENT ||
            next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 2u || strcmp(fields[0], "extent") != 0 ||
            parse_decimal(fields[1], &extent) != 0 || extent != RESULT_EXTENT ||
            next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 2u || strcmp(fields[0], "meaningful") != 0 ||
            parse_decimal(fields[1], &declared_meaningful) != 0 || declared_meaningful != meaningful ||
            next_line(text, length, &offset, line, sizeof line) != 0 ||
            (count = split_fields(line, fields, 12u)) != 3u || strcmp(fields[0], "reserved") != 0 ||
            parse_decimal(fields[1], &reserved_start) != 0 || reserved_start != meaningful ||
            parse_decimal(fields[2], &reserved_count) != 0 || reserved_count != RESULT_EXTENT - meaningful) {
            free(text);
            return fail("math/memio oracle: result extent or reserved range mismatch");
        }
        out->meaningful[suite] = (uint8_t)meaningful;
        out->reserved_start[suite] = (uint8_t)reserved_start;
        out->reserved_count[suite] = (uint8_t)reserved_count;
        for (i = 0u; i < meaningful; i++) {
            uint32_t index;
            uint32_t accepted;
            size_t j;
            if (next_line(text, length, &offset, line, sizeof line) != 0 ||
                (count = split_fields(line, fields, 12u)) < 4u || strcmp(fields[0], "result") != 0 ||
                parse_decimal(fields[1], &index) != 0 || index != i ||
                parse_decimal(fields[2], &accepted) != 0 || accepted == 0u || accepted > EXPECT_VALUES_MAX ||
                count != 3u + accepted) {
                free(text);
                return fail("math/memio oracle: malformed result row");
            }
            out->counts[suite][i] = (uint8_t)accepted;
            for (j = 0u; j < accepted; j++) {
                size_t k;
                if (parse_byte(fields[3u + j], &out->values[suite][i][j]) != 0) {
                    free(text);
                    return fail("math/memio oracle: malformed result byte");
                }
                for (k = 0u; k < j; k++) {
                    if (out->values[suite][i][k] == out->values[suite][i][j]) {
                        free(text);
                        return fail("math/memio oracle: duplicate expected alternative");
                    }
                }
            }
        }
        for (i = 0u; i < group_count; i++) {
            uint32_t index;
            uint32_t start;
            uint32_t span;
            const char *kind = suite == 0u ? (i == 7u ? "timing" : "arithmetic") : "register";
            if (next_line(text, length, &offset, line, sizeof line) != 0 ||
                (count = split_fields(line, fields, 12u)) != 6u || strcmp(fields[0], "group") != 0 ||
                parse_decimal(fields[1], &index) != 0 || index != i ||
                parse_decimal(fields[2], &start) != 0 || parse_decimal(fields[3], &span) != 0 ||
                span == 0u || start >= meaningful || span > meaningful - start ||
                strcmp(fields[4], kind) != 0 ||
                strcmp(fields[5], expected_names[suite][i]) != 0 ||
                copy_field(out->group_kind[suite][i], sizeof out->group_kind[suite][i], fields[4]) != 0 ||
                copy_field(out->group_label[suite][i], sizeof out->group_label[suite][i], fields[5]) != 0) {
                free(text);
                return fail("math/memio oracle: malformed group row");
            }
            out->group_start[suite][i] = (uint8_t)start;
            out->group_length[suite][i] = (uint8_t)span;
            out->group_count[suite] = (uint8_t)(i + 1u);
            while (span-- > 0u) {
                if (covered[suite][start] != 0u) {
                    free(text);
                    return fail("math/memio oracle: overlapping group range");
                }
                covered[suite][start++] = 1u;
            }
        }
        for (i = 0u; i < meaningful; i++) {
            if (covered[suite][i] != 1u || out->counts[suite][i] == 0u) {
                free(text);
                return fail("math/memio oracle: missing meaningful group result");
            }
        }
    }
    if (offset != length || metadata_seen[0] == 0u || metadata_seen[1] == 0u || metadata_seen[2] == 0u ||
        !valid_hex(out->archive, 64u) || !valid_hex(out->revision, 40u) || !valid_hex(out->license, 64u)) {
        free(text);
        return fail("math/memio oracle: trailing data or incomplete identity");
    }
    free(text);
    return 0;
}

static int parse_audio_oracle(const char *legacy_path, const char *extended_path,
                              suite_audio_oracle *out)
{
    static const char *const metadata_names[AUDIO_SUITE_COUNT * 2] = {
        "audio_main", "audio_asm", "audio2_main", "audio2_asm"
    };
    static const char *const suite_names[AUDIO_SUITE_COUNT] = {"audio", "audio2"};
    static const uint8_t expected_cases[AUDIO_SUITE_COUNT] = {6u, 7u};
    uint8_t *legacy = NULL;
    char *extended = NULL;
    size_t legacy_length = 0u;
    size_t extended_length = 0u;
    size_t offset;
    size_t suite;
    char line[ORACLE_LINE_MAX + 1u];
    char *fields[12];
    size_t count;

    memset(out, 0, sizeof *out);
    if (read_file(legacy_path, &legacy, &legacy_length, 32768u) != 0 ||
        read_text(extended_path, &extended, &extended_length, 32768u) != 0) {
        free(legacy);
        free(extended);
        return 1;
    }
    if (legacy_length > extended_length || memcmp(legacy, extended, legacy_length) != 0) {
        free(legacy);
        free(extended);
        return fail("audio oracle: extended input changed the legacy CPU/timer prefix");
    }
    free(legacy);
    offset = legacy_length;
    for (suite = 0u; suite < AUDIO_SUITE_COUNT * 2u; suite++) {
        char *destination = suite % 2u == 0u ? out->main_sha256[suite / 2u] : out->asm_sha256[suite / 2u];
        if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], metadata_names[suite]) != 0 ||
            !valid_hex(fields[1], 64u) || copy_field(destination, 65u, fields[1]) != 0) {
            free(extended);
            return fail("audio oracle: malformed or reordered source identity record");
        }
    }
    for (suite = 0u; suite < AUDIO_SUITE_COUNT; suite++) {
        uint32_t value;
        uint32_t next_case_offset = 0u;
        size_t result_index;
        size_t case_index;
        if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "suite") != 0 || strcmp(fields[1], suite_names[suite]) != 0) {
            free(extended);
            return fail("audio oracle: missing or reordered suite section");
        }
        if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "result_count") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != AUDIO_MEANINGFUL) {
            free(extended);
            return fail("audio oracle: result count mismatch");
        }
        if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "extent") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != AUDIO_MEANINGFUL) {
            free(extended);
            return fail("audio oracle: result extent mismatch");
        }
        if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
            count != 2u || strcmp(fields[0], "meaningful") != 0 ||
            parse_decimal(fields[1], &value) != 0 || value != AUDIO_MEANINGFUL) {
            free(extended);
            return fail("audio oracle: meaningful result count mismatch");
        }
        for (result_index = 0u; result_index < AUDIO_MEANINGFUL; result_index++) {
            uint32_t index;
            uint32_t accepted;
            size_t i;
            if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
                count < 4u || strcmp(fields[0], "result") != 0 ||
                parse_decimal(fields[1], &index) != 0 || index != result_index ||
                parse_decimal(fields[2], &accepted) != 0 || accepted == 0u ||
                accepted > EXPECT_VALUES_MAX || count != 3u + accepted ||
                (suite == 0u && accepted != 1u) ||
                (suite == 1u && accepted != (result_index == 9u ? 2u : 1u))) {
                free(extended);
                return fail("audio oracle: malformed result row or lost alternative set");
            }
            out->counts[suite][result_index] = (uint8_t)accepted;
            for (i = 0u; i < accepted; i++) {
                size_t prior;
                if (parse_byte(fields[3u + i], &out->values[suite][result_index][i]) != 0) {
                    free(extended);
                    return fail("audio oracle: malformed result byte");
                }
                for (prior = 0u; prior < i; prior++) {
                    if (out->values[suite][result_index][prior] == out->values[suite][result_index][i]) {
                        free(extended);
                        return fail("audio oracle: duplicate result alternative");
                    }
                }
            }
        }
        out->case_count[suite] = expected_cases[suite];
        for (case_index = 0u; case_index < expected_cases[suite]; case_index++) {
            uint32_t index;
            uint32_t start;
            uint32_t length;
            if (oracle_line(extended, extended_length, &offset, line, fields, &count) != 0 ||
                count != 5u || strcmp(fields[0], "case") != 0 ||
                parse_decimal(fields[1], &index) != 0 || index != case_index ||
                parse_decimal(fields[2], &start) != 0 || start != next_case_offset ||
                parse_decimal(fields[3], &length) != 0 || length == 0u ||
                length > AUDIO_MEANINGFUL - start || strlen(fields[4]) == 0u ||
                strlen(fields[4]) >= AUDIO_CASE_LABEL_MAX) {
                free(extended);
                return fail("audio oracle: malformed case coverage");
            }
            out->case_start[suite][case_index] = (uint8_t)start;
            out->case_length[suite][case_index] = (uint8_t)length;
            if (copy_field(out->case_label[suite][case_index], AUDIO_CASE_LABEL_MAX, fields[4]) != 0) {
                free(extended);
                return fail("audio oracle: malformed case label");
            }
            next_case_offset = start + length;
        }
        if (next_case_offset != AUDIO_MEANINGFUL) {
            free(extended);
            return fail("audio oracle: source cases do not cover all 18 results");
        }
    }
    if (offset != extended_length || out->counts[1][9] != 2u ||
        out->values[1][9][0] != 0x30u || out->values[1][9][1] != 0x34u) {
        free(extended);
        return fail("audio oracle: trailing data or audio2 index 9 alternatives changed");
    }
    free(extended);
    return 0;
}

static int parse_page_mode_oracle(const char *path, page_mode_oracle *out)
{
    static const char *const metadata_names[5] = {
        "revision", "main_sha256", "tests_sha256", "notice_sha256", "recipe_sha256"
    };
    static const size_t metadata_lengths[5] = {40u, 64u, 64u, 64u, 64u};
    static const uint8_t expected_counts[PAGE_MODE_RESULT_COUNT] = {1u, 1u, 1u, 1u, 2u, 1u};
    static const uint8_t expected_values[PAGE_MODE_RESULT_COUNT][2] = {
        {0x35u, 0u}, {0x41u, 0u}, {0x69u, 0u}, {0x81u, 0u},
        {0x74u, 0x75u}, {0x81u, 0u}
    };
    char *bytes = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    size_t i;
    char line[ORACLE_LINE_MAX + 1u];
    char *fields[12];
    size_t count;

    memset(out, 0, sizeof *out);
    if (read_text(path, &bytes, &length, 8192u) != 0) return 1;
    if (oracle_line(bytes, length, &offset, line, fields, &count) != 0 || count != 2u ||
        strcmp(fields[0], "LYNX-PAGE-MODE-ORACLE") != 0 || strcmp(fields[1], "1") != 0) {
        free(bytes);
        return fail("page-mode oracle: invalid version header");
    }
    for (i = 0u; i < 5u; i++) {
        char *destination;
        size_t capacity;
        if (oracle_line(bytes, length, &offset, line, fields, &count) != 0 || count != 2u ||
            strcmp(fields[0], metadata_names[i]) != 0 || strlen(fields[1]) != metadata_lengths[i]) {
            free(bytes);
            return fail("page-mode oracle: malformed source identity record");
        }
        if (i == 0u) { destination = out->revision; capacity = sizeof out->revision; }
        else if (i == 1u) { destination = out->main_sha256; capacity = sizeof out->main_sha256; }
        else if (i == 2u) { destination = out->tests_sha256; capacity = sizeof out->tests_sha256; }
        else if (i == 3u) { destination = out->notice_sha256; capacity = sizeof out->notice_sha256; }
        else { destination = out->recipe_sha256; capacity = sizeof out->recipe_sha256; }
        if (copy_field(destination, capacity, fields[1]) != 0 ||
            (i == 0u ? !valid_hex(fields[1], 40u) : !valid_hex(fields[1], 64u))) {
            free(bytes);
            return fail("page-mode oracle: malformed source identity");
        }
    }
    if (oracle_line(bytes, length, &offset, line, fields, &count) != 0 || count != 2u ||
        strcmp(fields[0], "result_count") != 0 || strcmp(fields[1], "6") != 0) {
        free(bytes);
        return fail("page-mode oracle: result count mismatch");
    }
    if (oracle_line(bytes, length, &offset, line, fields, &count) != 0 || count != 2u ||
        strcmp(fields[0], "test_count") != 0 || strcmp(fields[1], "6") != 0) {
        free(bytes);
        return fail("page-mode oracle: test count mismatch");
    }
    if (oracle_line(bytes, length, &offset, line, fields, &count) != 0 || count != 2u ||
        strcmp(fields[0], "extent") != 0 || strcmp(fields[1], "6") != 0) {
        free(bytes);
        return fail("page-mode oracle: result extent mismatch");
    }
    for (i = 0u; i < PAGE_MODE_RESULT_COUNT; i++) {
        uint32_t row = UINT32_MAX;
        uint32_t accepted = UINT32_MAX;
        size_t j;
        if (oracle_line(bytes, length, &offset, line, fields, &count) != 0) {
            char message[128];
            (void)snprintf(message, sizeof message,
                           "page-mode oracle: missing result row %zu at %zu/%zu", i, offset, length);
            free(bytes);
            return fail(message);
        }
        if (count < 4u || strcmp(fields[0], "result") != 0 ||
            parse_decimal(fields[1], &row) != 0 || row != i) {
            free(bytes);
            return fail("page-mode oracle: malformed result row index or shape");
        }
        if (parse_decimal(fields[2], &accepted) != 0 || accepted != expected_counts[i] ||
            count != (size_t)(3u + accepted)) {
            free(bytes);
            return fail("page-mode oracle: malformed result alternative count");
        }
        out->counts[i] = (uint8_t)accepted;
        for (j = 0u; j < accepted; j++) {
            if (parse_byte(fields[3u + j], &out->values[i][j]) != 0 ||
                out->values[i][j] != expected_values[i][j]) {
                free(bytes);
                return fail("page-mode oracle: source expectation mismatch");
            }
        }
    }
    if (offset != length) {
        free(bytes);
        return fail("page-mode oracle: trailing data");
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
    for (i = 0u; i < 15u; i++) if (strcmp(name, symbol_names[i]) == 0) return (int)i;
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
    if (code_start < SUITE_PAYLOAD_START || code_start >= code_end ||
        code_end > SUITE_PAYLOAD_END || code_end > LYNX_ADDRESS_SPACE ||
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
    if (result_start < SUITE_BSS_START || result_start > SUITE_BSS_END - extent ||
        result_start > LYNX_ADDRESS_SPACE - extent ||
        result_end != result_start + extent ||
        (result_start < code_end && code_start < result_end)) {
        return fail("symbol: result extent or code overlap");
    }
    for (i = 0u; i < 4u; i++) {
        if (data[i] >= LYNX_ADDRESS_SPACE) return fail("symbol: byte range invalid");
        if (data[i] >= code_start && data[i] < code_end) return fail("symbol: control and code overlap");
        if (data[i] >= result_start && data[i] < result_end) return fail("symbol: result and completion overlap");
        for (j = 0u; j < i; j++) {
            if (data[i] == data[j]) return fail("symbol: completion symbols overlap");
        }
    }
    if (data[0] < SUITE_BSS_START || data[0] >= SUITE_BSS_END ||
        data[1] < SUITE_BSS_START || data[1] >= SUITE_BSS_END ||
        data[2] < SUITE_BSS_START || data[2] >= SUITE_BSS_END ||
        data[3] >= SUITE_ZP_END ||
        (code_start < SUITE_STACK_END && SUITE_STACK_START < code_end) ||
        (result_start < SUITE_STACK_END && SUITE_STACK_START < result_end)) {
        return fail("symbol: byte range invalid");
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

static int checked_join(char *out, size_t capacity, const char *directory, const char *file);

static int pinned_manifest_hash(const char *relative_path, char expected[65])
{
    char path[4096];
    char *manifest = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    int result = 1;
    if (snprintf(path, sizeof path, "%s/build/pins/lynx-tests/.lynx-pin-files",
                 LYNX_PROJECT_ROOT) >= (int)sizeof path ||
        read_text(path, &manifest, &length, 262144u) != 0) return 1;
    while (offset < length) {
        size_t end = offset;
        char *separator;
        while (end < length && manifest[end] != '\n') end++;
        if (end > offset && manifest[offset] != '#') {
            if (end < length) manifest[end] = '\0';
            separator = strchr(manifest + offset, ' ');
            if (separator != NULL) {
                *separator++ = '\0';
                while (*separator == ' ') separator++;
                if (strcmp(separator, relative_path) == 0 &&
                    valid_hex(manifest + offset, 64u)) {
                    memcpy(expected, manifest + offset, 65u);
                    result = 0;
                    break;
                }
            }
        }
        offset = end + 1u;
    }
    free(manifest);
    return result;
}

static int verify_pinned_source(const char *relative_path, const char *expected_sha)
{
    char path[4096];
    char manifest_sha[65];
    char actual_sha[65];
    if (snprintf(path, sizeof path, "%s/build/pins/lynx-tests/%s",
                 LYNX_PROJECT_ROOT, relative_path) >= (int)sizeof path ||
        pinned_manifest_hash(relative_path, manifest_sha) != 0 ||
        strcmp(manifest_sha, expected_sha) != 0 ||
        file_sha256(path, actual_sha) != 0 || strcmp(actual_sha, expected_sha) != 0) {
        return fail("oracle: source identity mismatch");
    }
    return 0;
}

static int pin_marker_matches(const char *stamp, size_t length, const char *expected)
{
    return (length == 64u || (length == 65u && stamp[64] == '\n')) &&
           memcmp(stamp, expected, 64u) == 0;
}

static int verify_pinned_revision(const suite_oracle *oracle)
{
    char pins_path[4096];
    char stamp_path[4096];
    char *pins = NULL;
    char *stamp = NULL;
    size_t pins_length = 0u;
    size_t stamp_length = 0u;
    size_t offset = 0u;
    int found = 0;
    if (snprintf(pins_path, sizeof pins_path, "%s/pins.txt", LYNX_PROJECT_ROOT) >= (int)sizeof pins_path ||
        snprintf(stamp_path, sizeof stamp_path, "%s/build/pins/lynx-tests/.lynx-pin", LYNX_PROJECT_ROOT) >= (int)sizeof stamp_path ||
        read_text(pins_path, &pins, &pins_length, 65536u) != 0 ||
        read_text(stamp_path, &stamp, &stamp_length, 1024u) != 0) goto bad;
    while (offset < pins_length) {
        size_t end = offset;
        char *line = pins + offset;
        char *save = NULL;
        char *name;
        char *version;
        char *url;
        char *archive_sha;
        while (end < pins_length && pins[end] != '\n') end++;
        if (end < pins_length) pins[end] = '\0';
        name = strtok_r(line, " \t", &save);
        if (name != NULL && strcmp(name, "lynx-tests") == 0) {
            char *revision;
            version = strtok_r(NULL, " \t", &save);
            url = strtok_r(NULL, " \t", &save);
            archive_sha = strtok_r(NULL, " \t", &save);
            if (version == NULL || url == NULL || archive_sha == NULL ||
                strtok_r(NULL, " \t", &save) != NULL ||
                strcmp(archive_sha, oracle->archive) != 0 ||
                (revision = strstr(url, "/archive/")) == NULL) goto bad;
            revision += 9;
            if (strlen(revision) != 47u || strcmp(revision + 40u, ".tar.gz") != 0 ||
                memcmp(revision, oracle->revision, 40u) != 0) goto bad;
            found = 1;
            break;
        }
        offset = end + 1u;
    }
    if (found == 0 || !pin_marker_matches(stamp, stamp_length, oracle->archive)) goto bad;
    free(pins);
    free(stamp);
    return 0;
bad:
    free(pins);
    free(stamp);
    return fail("oracle: source identity mismatch");
}

static int verify_suite_build_artifacts(const char *directory, const char *suite)
{
    char record_path[4096];
    char object_path[4096];
    char labels_path[4096];
    char object_key[64];
    char labels_key[64];
    char expected[128];
    char actual[65];
    if ((strcmp(suite, "cpu") != 0 && strcmp(suite, "timers") != 0 &&
         strcmp(suite, "audio") != 0 && strcmp(suite, "audio2") != 0 &&
         strcmp(suite, "math") != 0 && strcmp(suite, "memio") != 0) ||
        checked_join(record_path, sizeof record_path, directory, "build-record.txt") != 0 ||
        snprintf(object_path, sizeof object_path, "%s/%s.bll", directory, suite) >= (int)sizeof object_path ||
        snprintf(labels_path, sizeof labels_path, "%s/%s.lbl", directory, suite) >= (int)sizeof labels_path ||
        snprintf(object_key, sizeof object_key, "%s_object_sha256", suite) >= (int)sizeof object_key ||
        snprintf(labels_key, sizeof labels_key, "%s_labels_sha256", suite) >= (int)sizeof labels_key) {
        return fail("build record path too long");
    }
    if (file_sha256(object_path, actual) != 0 ||
        record_value(record_path, object_key, expected, sizeof expected) != 0 || strcmp(actual, expected) != 0) {
        return fail("build record object identity mismatch");
    }
    if (file_sha256(labels_path, actual) != 0 ||
        record_value(record_path, labels_key, expected, sizeof expected) != 0 || strcmp(actual, expected) != 0) {
        return fail("build record labels identity mismatch");
    }
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
    if (verify_pinned_revision(oracle) != 0 ||
        verify_pinned_source("cpu/main.c", oracle->cpu_main) != 0 ||
        verify_pinned_source("cpu/tests.s", oracle->cpu_asm) != 0 ||
        verify_pinned_source("timers/main.c", oracle->timers_main) != 0 ||
        verify_pinned_source("timers/tests.s", oracle->timers_asm) != 0 ||
        verify_pinned_source("LICENSE", oracle->license) != 0) {
        return fail("oracle: source identity mismatch");
    }
    return 0;
}

static int verify_audio_build_identity(const char *directory, const suite_audio_oracle *oracle)
{
    static const char *const suites[AUDIO_SUITE_COUNT] = {"audio", "audio2"};
    static const char *const source_fields[AUDIO_SUITE_COUNT][2] = {
        {"audio_main_sha256", "audio_source_sha256"},
        {"audio2_main_sha256", "audio2_source_sha256"}
    };
    char record_path[4096];
    char actual[65];
    char expected[128];
    size_t suite;
    if (checked_join(record_path, sizeof record_path, directory, "build-record.txt") != 0) {
        return fail("audio build record path too long");
    }
    for (suite = 0u; suite < AUDIO_SUITE_COUNT; suite++) {
        const char *main_path = suite == 0u ? "audio/main.c" : "audio2/main.c";
        const char *asm_path = suite == 0u ? "audio/tests.s" : "audio2/tests.s";
        if (record_value(record_path, source_fields[suite][0], expected, sizeof expected) != 0 ||
            strcmp(expected, oracle->main_sha256[suite]) != 0 ||
            record_value(record_path, source_fields[suite][1], expected, sizeof expected) != 0 ||
            strcmp(expected, oracle->asm_sha256[suite]) != 0 ||
            verify_pinned_source(main_path, oracle->main_sha256[suite]) != 0 ||
            verify_pinned_source(asm_path, oracle->asm_sha256[suite]) != 0 ||
            verify_suite_build_artifacts(directory, suites[suite]) != 0) {
            return fail("audio oracle: source/build identity mismatch");
        }
    }
    if (record_value(record_path, "ca65_tool_sha256", expected, sizeof expected) != 0 ||
        !valid_hex(expected, 64u) ||
        record_value(record_path, "ld65_tool_sha256", expected, sizeof expected) != 0 ||
        !valid_hex(expected, 64u) ||
        record_value(record_path, "linker_layout_sha256", expected, sizeof expected) != 0 ||
        !valid_hex(expected, 64u) || file_sha256(record_path, actual) != 0) {
        return fail("audio build record tool/layout identity mismatch");
    }
    return 0;
}

static int regular_nosymlink_file(const char *path)
{
    struct stat info;
    return lstat(path, &info) == 0 && S_ISREG(info.st_mode) && !S_ISLNK(info.st_mode);
}

static int validate_page_mode_owner_marker(const char *marker_path, const char *owned_path)
{
    char *text = NULL;
    char tail[4096];
    size_t length = 0u;
    size_t tail_length;
    size_t i;
    int n;
    if (!regular_nosymlink_file(marker_path) || read_text(marker_path, &text, &length, 8192u) != 0) {
        return fail("page-mode build: owner marker missing or invalid");
    }
    n = snprintf(tail, sizeof tail, "path=%s\n", owned_path);
    if (n < 0 || (size_t)n >= sizeof tail) {
        free(text);
        return fail("page-mode build: owner path too long");
    }
    tail_length = (size_t)n;
    if (length != 55u + tail_length || memcmp(text, "nonce=", 6u) != 0 || text[54] != '\n' ||
        memcmp(text + 55u, tail, tail_length) != 0) {
        free(text);
        return fail("page-mode build: owner marker mismatch");
    }
    for (i = 6u; i < 54u; i++) {
        if (hex_digit((unsigned char)text[i]) < 0) {
            free(text);
            return fail("page-mode build: owner nonce malformed");
        }
    }
    free(text);
    return 0;
}

static int read_named_pin(const char *wanted, char archive_sha[65], char revision[41])
{
    char pins_path[4096];
    char *pins = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    if (snprintf(pins_path, sizeof pins_path, "%s/pins.txt", LYNX_PROJECT_ROOT) >= (int)sizeof pins_path ||
        read_text(pins_path, &pins, &length, 65536u) != 0) return 1;
    while (offset < length) {
        char line[1024];
        char *save = NULL;
        char *name;
        char *version;
        char *url;
        char *archive;
        char *revision_start;
        char revision_value[41];
        size_t end = offset;
        size_t line_length;
        while (end < length && pins[end] != '\n') end++;
        line_length = end - offset;
        if (line_length >= sizeof line) { free(pins); return fail("page-mode build: malformed pins.txt row"); }
        memcpy(line, pins + offset, line_length);
        line[line_length] = '\0';
        offset = end + (end < length ? 1u : 0u);
        name = strtok_r(line, " \t", &save);
        if (name == NULL || name[0] == '#') continue;
        if (strcmp(name, wanted) != 0) continue;
        version = strtok_r(NULL, " \t", &save);
        url = strtok_r(NULL, " \t", &save);
        archive = strtok_r(NULL, " \t", &save);
        if (version == NULL || url == NULL || archive == NULL || strtok_r(NULL, " \t", &save) != NULL ||
            !valid_hex(archive, 64u) || (revision_start = strstr(url, "/archive/")) == NULL) {
            free(pins);
            return fail("page-mode build: malformed pinned identity row");
        }
        revision_start += 9;
        if (strlen(revision_start) != 47u || strcmp(revision_start + 40u, ".tar.gz") != 0 ||
            found++ != 0u) {
            free(pins);
            return fail("page-mode build: malformed pinned revision");
        }
        memcpy(revision_value, revision_start, 40u);
        revision_value[40] = '\0';
        if (!valid_hex(revision_value, 40u)) {
            free(pins);
            return fail("page-mode build: malformed pinned revision");
        }
        memcpy(archive_sha, archive, 65u);
        memcpy(revision, revision_value, sizeof revision_value);
    }
    free(pins);
    return found == 1u ? 0 : fail("page-mode build: pinned identity row missing");
}

static int pinned_cc65_tool_hash(const char *relative_path, char hash[65])
{
    char manifest_path[4096];
    char *manifest = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    if (snprintf(manifest_path, sizeof manifest_path, "%s/build/pins/cc65/.lynx-build-files",
                 LYNX_PROJECT_ROOT) >= (int)sizeof manifest_path ||
        read_text(manifest_path, &manifest, &length, 1048576u) != 0) return 1;
    while (offset < length) {
        char line[512];
        char *save = NULL;
        char *sha;
        char *path;
        size_t end = offset;
        size_t line_length;
        while (end < length && manifest[end] != '\n') end++;
        line_length = end - offset;
        if (line_length >= sizeof line) { free(manifest); return fail("page-mode build: malformed tool manifest row"); }
        memcpy(line, manifest + offset, line_length);
        line[line_length] = '\0';
        offset = end + (end < length ? 1u : 0u);
        sha = strtok_r(line, " \t", &save);
        if (sha == NULL || sha[0] == '#') continue;
        path = strtok_r(NULL, " \t", &save);
        if (path == NULL || strtok_r(NULL, " \t", &save) != NULL) {
            free(manifest);
            return fail("page-mode build: malformed tool manifest row");
        }
        if (strcmp(path, relative_path) == 0) {
            if (!valid_hex(sha, 64u) || found++ != 0u) {
                free(manifest);
                return fail("page-mode build: malformed tool identity");
            }
            memcpy(hash, sha, 65u);
        }
    }
    free(manifest);
    return found == 1u ? 0 : fail("page-mode build: pinned tool identity missing");
}

static int verify_math_memio_build_identity(const char *directory,
                                           const suite_source_oracle *oracle)
{
    static const char *const suites[SOURCE_SUITE_COUNT] = {"math", "memio"};
    static const char *const main_keys[SOURCE_SUITE_COUNT] = {"math_main_sha256", "memio_main_sha256"};
    static const char *const asm_keys[SOURCE_SUITE_COUNT] = {"math_source_sha256", "memio_source_sha256"};
    suite_oracle pinned = {0};
    char record_path[4096];
    char path[4096];
    char expected[128];
    char actual[65];
    char ca65_expected[65];
    char ld65_expected[65];
    char ca65_path[4096];
    char ld65_path[4096];
    char wrapper_path[4096];
    unsigned suite;
    if (checked_join(record_path, sizeof record_path, directory, "build-record.txt") != 0) {
        return fail("math/memio build record path too long");
    }
    (void)snprintf(pinned.archive, sizeof pinned.archive, "%s", oracle->archive);
    (void)snprintf(pinned.revision, sizeof pinned.revision, "%s", oracle->revision);
    (void)snprintf(pinned.license, sizeof pinned.license, "%s", oracle->license);
    if (record_value(record_path, "lynx_tests_pin", expected, sizeof expected) != 0 ||
        strcmp(expected, oracle->archive) != 0 ||
        record_value(record_path, "lynx_tests_revision", expected, sizeof expected) != 0 ||
        strcmp(expected, oracle->revision) != 0 ||
        record_value(record_path, "license_sha256", expected, sizeof expected) != 0 ||
        strcmp(expected, oracle->license) != 0 ||
        verify_pinned_revision(&pinned) != 0 || verify_pinned_source("LICENSE", oracle->license) != 0) {
        return fail("math/memio build record pin/license identity mismatch");
    }
    for (suite = 0u; suite < SOURCE_SUITE_COUNT; suite++) {
        const char *main_path = suite == 0u ? "math/main.c" : "memio/main.c";
        const char *asm_path = suite == 0u ? "math/tests.s" : "memio/tests.s";
        if (record_value(record_path, main_keys[suite], expected, sizeof expected) != 0 ||
            strcmp(expected, oracle->main_sha256[suite]) != 0 ||
            record_value(record_path, asm_keys[suite], expected, sizeof expected) != 0 ||
            strcmp(expected, oracle->asm_sha256[suite]) != 0 ||
            verify_pinned_source(main_path, oracle->main_sha256[suite]) != 0 ||
            verify_pinned_source(asm_path, oracle->asm_sha256[suite]) != 0 ||
            verify_suite_build_artifacts(directory, suites[suite]) != 0) {
            return fail("math/memio oracle: source/build identity mismatch");
        }
        if (snprintf(wrapper_path, sizeof wrapper_path, "%s/fixtures/suites/%s-wrapper.s",
                     LYNX_PROJECT_ROOT, suites[suite]) >= (int)sizeof wrapper_path ||
            file_sha256(wrapper_path, actual) != 0 ||
            record_value(record_path, suite == 0u ? "math_wrapper_sha256" : "memio_wrapper_sha256",
                         expected, sizeof expected) != 0 || strcmp(actual, expected) != 0) {
            return fail("math/memio build record wrapper identity mismatch");
        }
    }
    if (snprintf(path, sizeof path, "%s/scripts/build-suites.sh", LYNX_PROJECT_ROOT) >= (int)sizeof path ||
        file_sha256(path, actual) != 0 ||
        record_value(record_path, "build_script_sha256", expected, sizeof expected) != 0 ||
        strcmp(actual, expected) != 0 ||
        snprintf(path, sizeof path, "%s/fixtures/suites/bll.cfg", LYNX_PROJECT_ROOT) >= (int)sizeof path ||
        file_sha256(path, actual) != 0 ||
        record_value(record_path, "linker_layout_sha256", expected, sizeof expected) != 0 ||
        strcmp(actual, expected) != 0) {
        return fail("math/memio build script or linker layout identity mismatch");
    }
    if (pinned_cc65_tool_hash("bin/ca65", ca65_expected) != 0 ||
        pinned_cc65_tool_hash("bin/ld65", ld65_expected) != 0 ||
        snprintf(ca65_path, sizeof ca65_path, "%s/build/pins/cc65/bin/ca65", LYNX_PROJECT_ROOT) >= (int)sizeof ca65_path ||
        snprintf(ld65_path, sizeof ld65_path, "%s/build/pins/cc65/bin/ld65", LYNX_PROJECT_ROOT) >= (int)sizeof ld65_path ||
        file_sha256(ca65_path, actual) != 0 || strcmp(actual, ca65_expected) != 0 ||
        record_value(record_path, "ca65_tool_sha256", expected, sizeof expected) != 0 || strcmp(actual, expected) != 0 ||
        file_sha256(ld65_path, actual) != 0 || strcmp(actual, ld65_expected) != 0 ||
        record_value(record_path, "ld65_tool_sha256", expected, sizeof expected) != 0 || strcmp(actual, expected) != 0) {
        return fail("math/memio build record tool identity mismatch");
    }
    return 0;
}

static int verify_page_mode_build(const char *run_directory, const char *build_directory,
                                  const char *oracle_path, const page_mode_oracle *oracle,
                                  page_mode_build_identity *identity)
{
    char expected_build[4096];
    char expected_oracle[4096];
    char build_real[4096];
    char record_path[4096];
    char file_path[4096];
    char source_root[4096];
    char archive[65], revision[41], cc65_archive[65], cc65_revision[41];
    char expected[128], actual[65], ca65_pin[65], ld65_pin[65];
    char marker_path[4096];
    char marker[4096];
    struct stat info;
    const char *const files[] = {"page-mode.bll", "page-mode.lbl", "page-mode.o", "NOTICE", "build-record.txt"};
    size_t i;
#define PAGE_RECORD(key, value) do { \
        if (record_value(record_path, (key), expected, sizeof expected) != 0 || strcmp(expected, (value)) != 0) \
            return fail("page-mode build: bound build record identity mismatch"); \
    } while (0)
    if (validate_owned_output_directory(run_directory) != 0 ||
        checked_join(expected_build, sizeof expected_build, run_directory, "page-mode-build") != 0 ||
        checked_join(expected_oracle, sizeof expected_oracle, run_directory, "page-mode-oracle.tsv") != 0 ||
        strcmp(build_directory, expected_build) != 0 || strcmp(oracle_path, expected_oracle) != 0) {
        return fail("page-mode build: artifacts are outside the exact owned invocation");
    }
    if (lstat(build_directory, &info) != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) ||
        realpath(build_directory, build_real) == NULL || strcmp(build_real, build_directory) != 0 ||
        checked_join(marker_path, sizeof marker_path, build_directory, ".lynx-page-owner") != 0 ||
        validate_page_mode_owner_marker(marker_path, build_directory) != 0 ||
        !regular_nosymlink_file(oracle_path)) {
        return fail("page-mode build: output ownership or oracle file invalid");
    }
    if (snprintf(marker, sizeof marker, "%s.owner", oracle_path) >= (int)sizeof marker ||
        validate_page_mode_owner_marker(marker, oracle_path) != 0) return 1;
    for (i = 0u; i < sizeof files / sizeof files[0]; i++) {
        if (checked_join(file_path, sizeof file_path, build_directory, files[i]) != 0 ||
            !regular_nosymlink_file(file_path)) return fail("page-mode build: artifact missing or not a regular file");
    }
    if (read_named_pin("lynx-tests", archive, revision) != 0 ||
        read_named_pin("cc65", cc65_archive, cc65_revision) != 0 ||
        strcmp(archive, "a70edf382afa2e16fae7f46fb115fb8a9edec6a3850a00f51e9f075602fbab3e") != 0 ||
        strcmp(revision, oracle->revision) != 0 ||
        snprintf(source_root, sizeof source_root, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT) >= (int)sizeof source_root) {
        return fail("page-mode build: current pinned source identity differs from oracle");
    }
    (void)cc65_revision;
    {
        char pin_path[4096];
        char cc65_marker_path[4096];
        char pin_text[65];
        FILE *pin_file;
        size_t got;
        if (snprintf(pin_path, sizeof pin_path, "%s/.lynx-pin", source_root) >= (int)sizeof pin_path ||
            snprintf(cc65_marker_path, sizeof cc65_marker_path, "%s/build/pins/cc65/.lynx-pin", LYNX_PROJECT_ROOT) >= (int)sizeof cc65_marker_path ||
            !regular_nosymlink_file(pin_path) || !regular_nosymlink_file(cc65_marker_path) ||
            (pin_file = fopen(pin_path, "rb")) == NULL) return fail("page-mode build: source pin marker missing");
        got = fread(pin_text, 1u, sizeof pin_text, pin_file);
        if (ferror(pin_file) != 0 || fclose(pin_file) != 0 || got != 65u || pin_text[64] != '\n' ||
            memcmp(pin_text, archive, 64u) != 0) return fail("page-mode build: source pin marker mismatch");
        if ((pin_file = fopen(cc65_marker_path, "rb")) == NULL) return fail("page-mode build: cc65 pin marker missing");
        got = fread(pin_text, 1u, sizeof pin_text, pin_file);
        if (ferror(pin_file) != 0 || fclose(pin_file) != 0 || got != 65u || pin_text[64] != '\n' ||
            memcmp(pin_text, cc65_archive, 64u) != 0) return fail("page-mode build: cc65 pin marker mismatch");
    }
    if (verify_pinned_source("page-mode/main.c", oracle->main_sha256) != 0 ||
        verify_pinned_source("page-mode/tests.s", oracle->tests_sha256) != 0 ||
        verify_pinned_source("LICENSE", oracle->notice_sha256) != 0) return 1;
    if (checked_join(file_path, sizeof file_path, build_directory, "NOTICE") != 0 ||
        file_sha256(file_path, actual) != 0 || strcmp(actual, oracle->notice_sha256) != 0) {
        return fail("page-mode build: copied source notice identity mismatch");
    }
    if (snprintf(file_path, sizeof file_path, "%s/build/pins/cc65/bin/ca65", LYNX_PROJECT_ROOT) >= (int)sizeof file_path ||
        !regular_nosymlink_file(file_path) || file_sha256(file_path, actual) != 0 ||
        pinned_cc65_tool_hash("bin/ca65", ca65_pin) != 0 || strcmp(actual, ca65_pin) != 0) {
        return fail("page-mode build: ca65 tool identity mismatch");
    }
    if (snprintf(file_path, sizeof file_path, "%s/build/pins/cc65/bin/ld65", LYNX_PROJECT_ROOT) >= (int)sizeof file_path ||
        !regular_nosymlink_file(file_path) || file_sha256(file_path, actual) != 0 ||
        pinned_cc65_tool_hash("bin/ld65", ld65_pin) != 0 || strcmp(actual, ld65_pin) != 0) {
        return fail("page-mode build: ld65 tool identity mismatch");
    }
    if (checked_join(record_path, sizeof record_path, build_directory, "build-record.txt") != 0 ||
        checked_join(file_path, sizeof file_path, build_directory, "page-mode.bll") != 0 ||
        file_sha256(file_path, identity->object_sha256) != 0 ||
        checked_join(file_path, sizeof file_path, build_directory, "page-mode.lbl") != 0 ||
        file_sha256(file_path, identity->labels_sha256) != 0 ||
        file_sha256(oracle_path, identity->oracle_sha256) != 0 ||
        file_sha256(record_path, identity->build_record_sha256) != 0) return 1;
#define PAGE_FILE_HASH(key, rel, member) do { \
        if (snprintf(file_path, sizeof file_path, "%s/%s", LYNX_PROJECT_ROOT, (rel)) >= (int)sizeof file_path || \
            file_sha256(file_path, actual) != 0 || record_value(record_path, (key), expected, sizeof expected) != 0 || \
            strcmp(actual, expected) != 0) return fail("page-mode build: fixture or recipe hash mismatch"); \
        (void)snprintf((member), 65u, "%s", actual); \
    } while (0)
    PAGE_FILE_HASH("wrapper_sha256", "fixtures/suites/page-mode-wrapper.s", identity->wrapper_sha256);
    PAGE_FILE_HASH("manifest_sha256", "fixtures/manifests/page-mode-tests.manifest", identity->manifest_sha256);
    PAGE_FILE_HASH("layout_sha256", "fixtures/suites/bll.cfg", identity->layout_sha256);
    PAGE_FILE_HASH("recipe_sha256", "scripts/build-page-mode.sh", identity->recipe_sha256);
#undef PAGE_FILE_HASH
    if (snprintf(file_path, sizeof file_path, "%s/scripts/generate-page-mode-oracle.sh", LYNX_PROJECT_ROOT) >= (int)sizeof file_path ||
        file_sha256(file_path, actual) != 0 || strcmp(actual, oracle->recipe_sha256) != 0) {
        return fail("page-mode build: oracle recipe identity mismatch");
    }
    (void)snprintf(identity->oracle_recipe_sha256, sizeof identity->oracle_recipe_sha256, "%s", actual);
    if (file_sha256(oracle_path, actual) != 0 || record_value(record_path, "object_sha256", expected, sizeof expected) != 0 ||
        strcmp(expected, identity->object_sha256) != 0 ||
        record_value(record_path, "labels_sha256", expected, sizeof expected) != 0 || strcmp(expected, identity->labels_sha256) != 0 ||
        record_value(record_path, "lynx_tests_pin", expected, sizeof expected) != 0 || strcmp(expected, archive) != 0 ||
        record_value(record_path, "lynx_tests_revision", expected, sizeof expected) != 0 || strcmp(expected, oracle->revision) != 0 ||
        record_value(record_path, "cc65_pin", expected, sizeof expected) != 0 || strcmp(expected, cc65_archive) != 0 ||
        record_value(record_path, "main_sha256", expected, sizeof expected) != 0 || strcmp(expected, oracle->main_sha256) != 0 ||
        record_value(record_path, "tests_sha256", expected, sizeof expected) != 0 || strcmp(expected, oracle->tests_sha256) != 0 ||
        record_value(record_path, "notice_sha256", expected, sizeof expected) != 0 || strcmp(expected, oracle->notice_sha256) != 0 ||
        record_value(record_path, "ca65_sha256", expected, sizeof expected) != 0 || strcmp(expected, ca65_pin) != 0 ||
        record_value(record_path, "ld65_sha256", expected, sizeof expected) != 0 || strcmp(expected, ld65_pin) != 0 ||
        record_value(record_path, "result_count", expected, sizeof expected) != 0 || strcmp(expected, "6") != 0 ||
        record_value(record_path, "result_extent", expected, sizeof expected) != 0 || strcmp(expected, "6") != 0 ||
        record_value(record_path, "startup_tier", expected, sizeof expected) != 0 || strcmp(expected, "BS93-A") != 0) {
        return fail("page-mode build: bound build record identity mismatch");
    }
    (void)snprintf(identity->ca65_sha256, sizeof identity->ca65_sha256, "%s", ca65_pin);
    (void)snprintf(identity->ld65_sha256, sizeof identity->ld65_sha256, "%s", ld65_pin);
    if (file_sha256(oracle_path, identity->oracle_sha256) != 0 ||
        file_sha256(record_path, identity->build_record_sha256) != 0) return 1;
    PAGE_RECORD("profiles", "lynx1,lynx2");
#undef PAGE_RECORD
    return 0;
}

static int validate_page_mode_build_mode(const char *run_directory,
                                         const char *build_directory,
                                         const char *oracle_path)
{
    page_mode_oracle oracle;
    page_mode_build_identity identity;
    if (parse_page_mode_oracle(oracle_path, &oracle) != 0 ||
        verify_page_mode_build(run_directory, build_directory, oracle_path,
                               &oracle, &identity) != 0) return 1;
    puts("page-mode build: pinned source, tool, notice, wrapper, object, labels and six-row oracle identities valid");
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
        verify_suite_build_artifacts(directory, "timers") != 0 ||
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
    unsigned profile_value;
    if (profile_index(profile, &profile_value) != 0) return fail("timer record: invalid profile");
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
        if (next_line(text, length, &offset, line, sizeof line) != 0) {
            free(text); return fail("timer raw record shape mismatch");
        }
        if (line[0] != '\0' && line[strlen(line) - 1u] == '\t') {
            free(text); return fail("timer raw record count or byte malformed");
        }
        count = split_fields(line, fields, 4u);
        if (count != 3u || strcmp(fields[0], "result") != 0) {
            free(text); return fail("timer raw record shape mismatch");
        }
        if (parse_decimal(fields[1], &index) != 0 || index != i ||
            !valid_hex(fields[2], 2u) || parse_byte(fields[2], &results[i]) != 0) {
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
            !valid_hex(fields[2], 2u) || parse_byte(fields[2], &value) != 0 || value != results[i]) {
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
    atomic_output proof_output = {0};
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
    if (atomic_output_begin(proof_path, &proof_output) != 0) return 1;
    out = proof_output.stream;
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
            atomic_output_abort(&proof_output);
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
    if (atomic_output_commit(&proof_output) != 0) return fail("cascade proof: cannot finish proof output");
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
    atomic_output companion_output = {0};
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
    if (atomic_output_begin(paths->companion, &companion_output) != 0) return 1;
    out = companion_output.stream;
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
    if (atomic_output_commit(&companion_output) != 0) return fail("timer companion: cannot finish pending record");
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
    if (create_owned_temp_slot(directory, "cascade-proof-verify", temp_proof) != 0) return 1;
    if (cascade_proof_write(profile, directory, oracle_path, descriptor_path,
                            source_root, temp_proof) != 0 ||
        file_sha256(temp_proof, actual) != 0 || strcmp(actual, proof_sha) != 0) {
        (void)remove_owned_temp_slot(directory, temp_proof);
        return fail("timer companion: cascade proof is stale or does not match linked source evidence");
    }
    if (remove_owned_temp_slot(directory, temp_proof) != 0) return 1;
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

static int machine_create_with_policy(machine_instance *instance, const uint8_t *bll,
                                     size_t bll_length, uint32_t profile,
                                     uint32_t timing_policy)
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
    config.timing_policy = timing_policy;
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

static int machine_create(machine_instance *instance, const uint8_t *bll, size_t bll_length,
                          uint32_t profile)
{
    return machine_create_with_policy(instance, bll, bll_length, profile,
                                      LYNX_TIMING_POLICY_LEGACY_FLAT5);
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
    char path[4096];
    size_t i;
    if (validate_owned_output_directory(directory) != 0) return 1;
    for (i = 0u; i < sizeof kinds / sizeof kinds[0]; i++) {
        char name[64];
        const char *extension = i == 0u ? "txt" : (i < 3u ? "tsv" : "json");
        int count = snprintf(name, sizeof name, "%s-%s.%s", kinds[i], case_name, extension);
        if (count < 0 || (size_t)count >= sizeof name ||
            checked_join(path, sizeof path, directory, name) != 0) return fail("output path too long");
        /* unlink removes a direct file entry or the symlink itself, never a
           directory and never the target of an output symlink. */
        if (unlink(path) != 0 && errno != ENOENT) return fail("cannot clear stale suite output");
    }
    return 0;
}

static int write_raw(const char *path, const char *suite, const char *machine_profile,
                     const char *selected_profile, const char *case_name,
                     const uint8_t *results, uint32_t count)
{
    atomic_output output = {0};
    FILE *file;
    uint32_t i;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file, "suite\t%s\nmachine_profile\t%s\nprofile_input\t%s\nram_seed\t%s\nresult_count\t%u\n",
                  suite, machine_profile, selected_profile, case_name, count);
    for (i = 0u; i < count; i++) (void)fprintf(file, "result\t%" PRIu32 "\t%02x\n", i, results[i]);
    if (atomic_output_commit(&output) != 0) return fail("cannot close raw result record");
    return 0;
}

static int write_comparison(const char *path, const suite_oracle *oracle,
                            const uint8_t *results, uint32_t count)
{
    atomic_output output = {0};
    FILE *file;
    uint32_t i;
    int match = 1;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
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
    if (atomic_output_commit(&output) != 0) return fail("cannot close comparison record");
    return match != 0 ? 0 : fail("source oracle comparison mismatch");
}

static int write_timer_comparison(const char *path, const suite_oracle *oracle,
                                  const uint8_t *results, int *matches)
{
    atomic_output output = {0};
    FILE *file;
    uint32_t i;
    int match = 1;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
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
    if (atomic_output_commit(&output) != 0) return fail("cannot close timer comparison record");
    *matches = match;
    return 0;
}

static int write_audio_comparison(const char *path, const suite_audio_oracle *oracle,
                                  unsigned suite_index, const uint8_t *results,
                                  uint32_t *agreements)
{
    atomic_output output = {0};
    FILE *file;
    uint32_t i;
    uint32_t accepted_rows = 0u;
    if (suite_index >= AUDIO_SUITE_COUNT || atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    for (i = 0u; i < AUDIO_MEANINGFUL; i++) {
        size_t j;
        int accepted = 0;
        (void)fprintf(file, "result\t%" PRIu32 "\t%02x\tEXPECT(", i, results[i]);
        for (j = 0u; j < oracle->counts[suite_index][i]; j++) {
            if (j != 0u) (void)fputc('|', file);
            (void)fprintf(file, "%02x", oracle->values[suite_index][i][j]);
            if (oracle->values[suite_index][i][j] == results[i]) accepted = 1;
        }
        (void)fprintf(file, ")\t%s\n", accepted != 0 ? "source-accepted" : "source-mismatch");
        if (accepted != 0) accepted_rows++;
    }
    (void)fprintf(file, "comparison\t%s\n", accepted_rows == AUDIO_MEANINGFUL ? "match" : "mismatch");
    if (atomic_output_commit(&output) != 0) return fail("cannot close audio comparison record");
    *agreements = accepted_rows;
    return 0;
}

static int write_audio_run_record(const char *path, const char *build_directory,
                                  const char *suite, const char *machine_profile,
                                  const char *selected_profile, const suite_audio_oracle *oracle,
                                  unsigned suite_index, const lynx_identity *identity,
                                  const char *bll_sha, const char *labels_sha,
                                  const char *oracle_sha, uint64_t ticks,
                                  uint8_t probe, uint8_t marker, uint8_t progress,
                                  int detected_model, uint32_t agreements)
{
    char build_record[4096];
    char build_record_sha[65];
    char wrapper_sha[65];
    char layout_sha[65];
    char builder_sha[65];
    char ca65_sha[65];
    char ld65_sha[65];
    char source_main[65];
    char source_asm[65];
    char raw_sha[65];
    char comparison_sha[65];
    char raw_path[4096];
    char comparison_path[4096];
    char object_sha[65];
    char labels_identity[65];
    char object_key[64];
    char labels_key[64];
    char wrapper_key[64];
    atomic_output output = {0};
    FILE *file;
    if (suite_index >= AUDIO_SUITE_COUNT || checked_join(build_record, sizeof build_record,
        build_directory, "build-record.txt") != 0 ||
        snprintf(object_key, sizeof object_key, "%s_object_sha256", suite) >= (int)sizeof object_key ||
        snprintf(labels_key, sizeof labels_key, "%s_labels_sha256", suite) >= (int)sizeof labels_key ||
        snprintf(wrapper_key, sizeof wrapper_key, "%s_wrapper_sha256", suite) >= (int)sizeof wrapper_key ||
        record_value(build_record, wrapper_key, wrapper_sha, sizeof wrapper_sha) != 0 ||
        record_value(build_record, "linker_layout_sha256", layout_sha, sizeof layout_sha) != 0 ||
        record_value(build_record, "build_script_sha256", builder_sha, sizeof builder_sha) != 0 ||
        record_value(build_record, "ca65_tool_sha256", ca65_sha, sizeof ca65_sha) != 0 ||
        record_value(build_record, "ld65_tool_sha256", ld65_sha, sizeof ld65_sha) != 0 ||
        record_value(build_record, object_key, object_sha, sizeof object_sha) != 0 ||
        record_value(build_record, labels_key, labels_identity, sizeof labels_identity) != 0 ||
        file_sha256(build_record, build_record_sha) != 0) {
        return fail("audio run record build identity is incomplete");
    }
    {
        char name[128];
        if (snprintf(name, sizeof name, "raw-results-%s-%s.tsv", suite, machine_profile) >= (int)sizeof name ||
            checked_join(raw_path, sizeof raw_path, build_directory, name) != 0 ||
            snprintf(name, sizeof name, "comparison-%s-%s.tsv", suite, machine_profile) >= (int)sizeof name ||
            checked_join(comparison_path, sizeof comparison_path, build_directory, name) != 0 ||
            file_sha256(raw_path, raw_sha) != 0 || file_sha256(comparison_path, comparison_sha) != 0) {
            return fail("audio run record raw/comparison identity is incomplete");
        }
    }
    (void)snprintf(source_main, sizeof source_main, "%s", oracle->main_sha256[suite_index]);
    (void)snprintf(source_asm, sizeof source_asm, "%s", oracle->asm_sha256[suite_index]);
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file,
        "schema=lynx-audio-suite-run-v1\nsuite=%s\nmachine_profile=%s\nprofile_input=%s\n"
        "detected_model=%s\nsource_tuple_model=not-applicable\nmodel_probe_bytes=%02x,%02x\n"
        "source_main_sha256=%s\nsource_assembly_sha256=%s\nwrapper_sha256=%s\n"
        "builder_sha256=%s\nlayout_sha256=%s\nca65_tool_sha256=%s\nld65_tool_sha256=%s\n"
        "object_sha256=%s\nlabels_sha256=%s\nbll_sha256=%s\nlabels_file_sha256=%s\n"
        "build_record_sha256=%s\noracle_sha256=%s\nraw_results_sha256=%s\ncomparison_sha256=%s\n"
        "result_extent=18\nexpected_count=18\nmeaningful_count=18\ntick_min=%u\ntick_max=%u\n"
        "ticks_observed=%" PRIu64 "\ncompletion_margin_ticks=%" PRIu64 "\nmaximum_headroom_ticks=%" PRIu64 "\n"
        "guest_ticks=%" PRIu64 "\nminimum_guest_ticks=%u\nmaximum_guest_ticks=%u\n"
        "guest_progress=%u\ncompletion_sentinel=a5\nguest_return=observed\n"
        "zero_page_initialization=full-256-byte-wrapper-clear\n"
        "source_agreements=%u\nsource_comparison=separate-record\n"
        "timing_policy=%s\naudio_policy=%s\naudio_rate_num=%u\naudio_rate_den=%u\n"
        "audio_channels=%u\naudio_bits=%u\nhardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        suite, machine_profile, selected_profile,
        detected_model < 0 ? "unknown" : profile_label((unsigned)detected_model), probe, marker,
        source_main, source_asm, wrapper_sha, builder_sha, layout_sha, ca65_sha, ld65_sha,
        object_sha, labels_identity, bll_sha, labels_sha, build_record_sha, oracle_sha,
        raw_sha, comparison_sha,
        AUDIO_MIN_TICKS, AUDIO_MAX_TICKS, ticks, ticks - AUDIO_MIN_TICKS,
        AUDIO_MAX_TICKS - ticks, ticks, AUDIO_MIN_TICKS, AUDIO_MAX_TICKS, (unsigned)progress,
        agreements,
        identity->timing_policy == LYNX_TIMING_POLICY_LEGACY_FLAT5 ? "legacy-flat5" : "unknown",
        identity->audio_policy == LYNX_AUDIO_POLICY_DIGITAL_1 ? "digital-1" : "unknown",
        identity->audio_rate_num, identity->audio_rate_den, identity->audio_channels, identity->audio_bits);
    if (atomic_output_commit(&output) != 0) return fail("cannot close audio run record");
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

static int audio_symbol_layout(const char *labels, size_t labels_length,
                               const uint8_t *bll, size_t bll_length,
                               suite_symbols *symbols)
{
    if (parse_symbols(labels, labels_length, symbols) != 0 ||
        ranges_valid(symbols, AUDIO_MEANINGFUL) != 0) return 1;
    if (symbols->found[12] == 0u || symbols->found[13] == 0u ||
        symbols->value[1] != symbols->value[8] ||
        symbols->value[12] < 0x01F7u || symbols->value[12] - 0x01F6u >= bll_length ||
        bll[symbols->value[12] - 0x01F6u] != 0x60u ||
        symbols->value[13] < 0x01F9u || symbols->value[13] - 0x01F6u >= bll_length ||
        bll[symbols->value[13] - 0x01F6u - 3u] != 0x20u ||
        bll[symbols->value[13] - 0x01F6u - 2u] != (uint8_t)symbols->value[8] ||
        bll[symbols->value[13] - 0x01F6u - 1u] != (uint8_t)(symbols->value[8] >> 8) ||
        symbols->value[7] < BLL_HEADER_SIZE || symbols->value[7] - 0x01F6u != bll_length) {
        return fail("audio symbol extent, aliases or wrapper return gate invalid");
    }
    return 0;
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
    atomic_output output = {0};
    FILE *file;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
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
    if (atomic_output_commit(&output) != 0) return fail("cannot close suite run record");
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
    if (verify_suite_build_artifacts(directory, "cpu") != 0 ||
        clear_old_outputs(directory, case_name) != 0) goto done;
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

static int run_audio(const char *suite_name, const char *machine_profile_name,
                     const char *selected_profile_name, const char *directory,
                     const char *legacy_oracle_path, const char *extended_oracle_path,
                     int no_run, uint32_t maximum_ticks)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char bll_path[4096];
    char labels_path[4096];
    char record_path[4096];
    char raw_path[4096];
    char comparison_path[4096];
    char oracle_sha[65];
    char bll_sha[65];
    char labels_sha[65];
    char output_key[96];
    char filename[128];
    uint8_t results[AUDIO_MEANINGFUL];
    uint8_t progress = 0u;
    uint8_t model = 0u;
    uint8_t done = 0u;
    uint8_t probe = 0u;
    uint32_t machine_profile;
    uint32_t selected_profile;
    unsigned suite_index;
    uint64_t guest_ticks = 0u;
    uint64_t total_consumed = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    lynx_identity identity;
    lynx__cpu initial_cpu;
    machine_instance instance = {0};
    suite_symbols symbols;
    suite_oracle legacy_oracle;
    suite_audio_oracle audio_oracle;
    int complete = 0;
    int initial_done_clear_seen = 0;
    int detected_model = -1;
    uint32_t source_agreements = 0u;
    int result = 1;

    if (strcmp(suite_name, "audio") == 0) suite_index = 0u;
    else if (strcmp(suite_name, "audio2") == 0) suite_index = 1u;
    else return fail("audio runner requires suite audio or audio2");
    if (profile_index(machine_profile_name, &machine_profile) != 0 ||
        profile_index(selected_profile_name, &selected_profile) != 0) {
        return fail("audio runner requires explicit lynx1 or lynx2 profiles");
    }
    if (maximum_ticks < AUDIO_MIN_TICKS || maximum_ticks > AUDIO_MAX_TICKS) {
        return fail("audio time: guest ticks outside bounded completion window");
    }
    machine_profile = machine_profile == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
    if (validate_owned_output_directory(directory) != 0 ||
        parse_oracle(legacy_oracle_path, &legacy_oracle) != 0 ||
        read_build_identity(directory, &legacy_oracle) != 0 ||
        parse_audio_oracle(legacy_oracle_path, extended_oracle_path, &audio_oracle) != 0 ||
        verify_audio_build_identity(directory, &audio_oracle) != 0) goto done;
    if (snprintf(filename, sizeof filename, "%s.bll", suite_name) >= (int)sizeof filename ||
        checked_join(bll_path, sizeof bll_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "%s.lbl", suite_name) >= (int)sizeof filename ||
        checked_join(labels_path, sizeof labels_path, directory, filename) != 0 ||
        snprintf(output_key, sizeof output_key, "%s-%s", suite_name, machine_profile_name) >= (int)sizeof output_key ||
        snprintf(filename, sizeof filename, "run-record-%s.txt", output_key) >= (int)sizeof filename ||
        checked_join(record_path, sizeof record_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "raw-results-%s.tsv", output_key) >= (int)sizeof filename ||
        checked_join(raw_path, sizeof raw_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "comparison-%s.tsv", output_key) >= (int)sizeof filename ||
        checked_join(comparison_path, sizeof comparison_path, directory, filename) != 0) {
        (void)fail("audio suite input path too long");
        goto done;
    }
    if (read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0 ||
        audio_symbol_layout(labels, labels_length, bll, bll_length, &symbols) != 0) {
        (void)fail("audio symbol extent, aliases or wrapper return gate invalid");
        goto done;
    }
    if (file_sha256(bll_path, bll_sha) != 0 || file_sha256(labels_path, labels_sha) != 0 ||
        file_sha256(legacy_oracle_path, oracle_sha) != 0 ||
        clear_old_outputs(directory, output_key) != 0) goto done;
    if (machine_create(&instance, bll, bll_length, machine_profile) != 0 ||
        lynx_get_identity(instance.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A || identity.entry_addr != symbols.value[0] ||
        identity.object_len != bll_length || identity.timing_policy != LYNX_TIMING_POLICY_LEGACY_FLAT5 ||
        identity.audio_policy != LYNX_AUDIO_POLICY_DIGITAL_1 || identity.audio_rate_num != 40000u ||
        identity.audio_rate_den != 1u || identity.audio_channels != 2u || identity.audio_bits != 16u ||
        symbols.value[7] < BLL_HEADER_SIZE || symbols.value[7] - 0x01F6u != bll_length) {
        (void)fail("audio public BS93 identity, startup tier or immutable audio policy mismatch");
        goto done;
    }
    {
        lynx__sha256 state;
        uint8_t digest[32];
        char content_sha[65];
        lynx__sha256_init(&state);
        lynx__sha256_update(&state, bll, bll_length);
        lynx__sha256_final(&state, digest);
        lynx__sha256_hex(digest, content_sha);
        if (memcmp(identity.content_sha256, digest, sizeof digest) != 0 ||
            verify_suite_build_artifacts(directory, suite_name) != 0) {
            (void)fail("audio public BLL content or build object identity mismatch");
            goto done;
        }
        (void)content_sha;
    }
    for (uint32_t i = 0u; i < 0x100u; i++) {
        if (instance.machine->ram[i] != 0u) {
            (void)fail("audio fresh machine zero-page was not clear before guest entry");
            goto done;
        }
    }
    for (uint32_t i = 0u; i < AUDIO_MEANINGFUL; i++) {
        if (instance.machine->ram[symbols.value[2] + i] != 0u) {
            (void)fail("audio fresh machine result block was not clear before guest entry");
            goto done;
        }
    }
    if (lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
        (void)fail("audio runner could not arm post-test sentinel watch");
        goto done;
    }
    initial_cpu = instance.machine->cpu;
    if (no_run != 0) {
        int completion_result;
        if (prove_held_no_run(&instance, &symbols, AUDIO_MIN_TICKS, &guest_ticks) != 0) goto done;
        total_consumed = guest_ticks;
        completion_result = validate_guest_completion(&instance, &symbols, suite_name, 0, 0, 0,
                                                      guest_ticks, total_consumed, AUDIO_MIN_TICKS,
                                                      AUDIO_MAX_TICKS, &done, &progress, &model, &probe);
        if (completion_result != 1 || lynx__machine_set_bus_hold(instance.machine, 0u) != LYNX_OK ||
            instance.machine->bus_hold != 0u) {
            if (completion_result == 0) (void)fail("audio no-run: completion validator accepted a held suite");
            goto done;
        }
        puts("audio no-run control: held guest did not execute; same instance released for resumed completion");
    }
    while (total_consumed < maximum_ticks) {
        uint64_t budget = maximum_ticks - total_consumed;
        uint64_t used = 0u;
        uint64_t before = 0u;
        uint64_t after = 0u;
        if (budget > 10000u) budget = 10000u;
        if (lynx_guest_ticks(instance.machine, &before) != LYNX_OK ||
            lynx_advance(instance.machine, budget, &used, &stop) != LYNX_OK ||
            used > budget || lynx_guest_ticks(instance.machine, &after) != LYNX_OK || after < before ||
            after - before != used || after > maximum_ticks) {
            (void)fail("audio bounded guest advance or tick accounting failed");
            goto done;
        }
        total_consumed += used;
        guest_ticks = after;
        if (stop == LYNX_STOP_WATCH) {
            if (lynx_watch_get(instance.machine, &event) != LYNX_OK || event.hit != 1u ||
                event.address != symbols.value[6] || event.return_tick != after ||
                event.return_tick != event.hit_tick + 1u) {
                (void)fail("audio completion watch did not match a guest sentinel write");
                goto done;
            }
            if (event.value == 0u && initial_done_clear_seen == 0) {
                uint8_t initial_progress = 0u;
                uint8_t initial_model = 0u;
                if (lynx_read_memory(instance.machine, symbols.value[4], &initial_progress, 1u) != LYNX_OK ||
                    lynx_read_memory(instance.machine, symbols.value[5], &initial_model, 1u) != LYNX_OK ||
                    initial_progress != 0u || initial_model != 0u ||
                    lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                    (void)fail("audio wrapper initial sentinel clear was not an empty-state prefix");
                    goto done;
                }
                initial_done_clear_seen = 1;
                continue;
            }
            if (event.value == 0xA5u && initial_done_clear_seen != 0) {
                complete = 1;
                break;
            }
            (void)fail("audio sentinel value or write order mismatch");
            goto done;
        }
        if (stop != LYNX_STOP_BUDGET) {
            (void)fail("audio guest stopped before completion");
            goto done;
        }
        if (used == 0u || after <= before) {
            (void)fail("audio guest made no bounded progress toward completion");
            goto done;
        }
    }
    {
        const int execution_started = memcmp(&initial_cpu, &instance.machine->cpu, sizeof initial_cpu) != 0;
        if (validate_guest_completion(&instance, &symbols, suite_name, execution_started, complete,
                                      initial_done_clear_seen, guest_ticks, total_consumed,
                                      AUDIO_MIN_TICKS, maximum_ticks,
                                      &done, &progress, &model, &probe) != 0) goto done;
    }
    if (lynx_read_memory(instance.machine, symbols.value[2], results, AUDIO_MEANINGFUL) != LYNX_OK) {
        (void)fail("audio result block could not be read after completion");
        goto done;
    }
    detected_model = probe_model(probe, model);
    if (write_raw(raw_path, suite_name, machine_profile_name, selected_profile_name,
                  output_key, results, AUDIO_MEANINGFUL) != 0 ||
        write_audio_comparison(comparison_path, &audio_oracle, suite_index, results,
                               &source_agreements) != 0 ||
        write_audio_run_record(record_path, directory, suite_name, machine_profile_name,
                               selected_profile_name, &audio_oracle, suite_index, &identity,
                               bll_sha, labels_sha, oracle_sha, guest_ticks,
                               probe, model, progress, detected_model, source_agreements) != 0) goto done;
    if (detected_model < 0) {
        result = fail("audio guest model probe/marker evidence is invalid");
        goto done;
    }
    if (detected_model != (int)selected_profile) {
        result = fail("audio guest model does not match selected profile input");
        goto done;
    }
    printf("audio observation complete: suite=%s profile=%s ticks=%" PRIu64
           " progress=%u source_rows=18 source_agreements=%u model=%s\n",
           suite_name, machine_profile_name, guest_ticks, (unsigned)progress,
           source_agreements, profile_label((unsigned)detected_model));
    puts("timing_policy=legacy-flat5 audio_policy=digital-1 audio_rate=40000/1 stereo_s16 hardware_accuracy=unverified profile_qualification=unresolved");
    if (source_agreements != AUDIO_MEANINGFUL) result = fail("audio source oracle comparison mismatch");
    else result = 0;
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
}

static int source_suite_index(const char *suite_name, unsigned *index)
{
    if (strcmp(suite_name, "math") == 0) { *index = 0u; return 0; }
    if (strcmp(suite_name, "memio") == 0) { *index = 1u; return 0; }
    return 1;
}

static int source_symbol_layout(const char *labels, size_t labels_length,
                                const uint8_t *bll, size_t bll_length,
                                suite_symbols *symbols)
{
    uint32_t return_offset;
    uint32_t after_offset;
    uint32_t idle_offset;
    if (parse_symbols(labels, labels_length, symbols) != 0 ||
        ranges_valid(symbols, RESULT_EXTENT) != 0 || symbols->found[12] == 0u ||
        symbols->found[13] == 0u || symbols->found[14] == 0u || symbols->value[1] != symbols->value[8] ||
        symbols->value[12] < 0x01F7u || symbols->value[13] < 0x01F9u ||
        symbols->value[12] - 0x01F6u >= bll_length ||
        symbols->value[13] - 0x01F6u < 3u || symbols->value[13] - 0x01F6u >= bll_length ||
        symbols->value[13] > LYNX_ADDRESS_SPACE - 10u ||
        symbols->value[14] != symbols->value[13] + 10u ||
        symbols->value[14] - 0x01F6u >= bll_length ||
        symbols->value[7] < BLL_HEADER_SIZE || symbols->value[7] - 0x01F6u != bll_length) {
        return fail("math/memio symbol extent or wrapper boundary invalid");
    }
    return_offset = symbols->value[12] - 0x01F6u;
    after_offset = symbols->value[13] - 0x01F6u;
    idle_offset = symbols->value[14] - 0x01F6u;
    if (idle_offset + 2u >= bll_length || bll[return_offset] != 0x60u ||
        bll[after_offset - 3u] != 0x20u ||
        bll[after_offset - 2u] != (uint8_t)symbols->value[8] ||
        bll[after_offset - 1u] != (uint8_t)(symbols->value[8] >> 8) ||
        bll[after_offset] != 0xA9u || bll[after_offset + 1u] != 0x08u ||
        bll[after_offset + 2u] != 0x8Du ||
        bll[after_offset + 3u] != (uint8_t)symbols->value[4] ||
        bll[after_offset + 4u] != (uint8_t)(symbols->value[4] >> 8) ||
        bll[after_offset + 5u] != 0xA9u || bll[after_offset + 6u] != 0xA5u ||
        bll[after_offset + 7u] != 0x8Du ||
        bll[after_offset + 8u] != (uint8_t)symbols->value[6] ||
        bll[after_offset + 9u] != (uint8_t)(symbols->value[6] >> 8) ||
        bll[idle_offset] != 0x4Cu || bll[idle_offset + 1u] != (uint8_t)symbols->value[14] ||
        bll[idle_offset + 2u] != (uint8_t)(symbols->value[14] >> 8)) {
        return fail("math/memio source return gate does not match the linked wrapper");
    }
    return 0;
}

static int write_source_raw(const char *path, const char *suite, const char *profile,
                            const uint8_t results[RESULT_EXTENT])
{
    atomic_output output = {0};
    FILE *file;
    unsigned i;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file,
        "schema\tlynx-source-suite-raw-v1\nsuite\t%s\nmachine_profile\t%s\nprofile_input\t%s\n"
        "startup_tier\tA\nresult_count\t18\n", suite, profile, profile);
    for (i = 0u; i < RESULT_EXTENT; i++) {
        (void)fprintf(file, "result\t%u\t%02x\n", i, results[i]);
    }
    if (atomic_output_commit(&output) != 0) return fail("cannot close math/memio raw record");
    return 0;
}

static int write_source_comparison(const char *path, const suite_source_oracle *oracle,
                                   unsigned suite_index, const uint8_t results[RESULT_EXTENT],
                                   uint32_t *scoped_agreements, uint32_t *strict_mismatches)
{
    atomic_output output = {0};
    FILE *file;
    uint32_t scoped_count = 0u;
    uint32_t mismatch_count = 0u;
    unsigned i;
    unsigned scoped_limit = suite_index == 0u ? 14u : 13u;
    if (suite_index >= SOURCE_SUITE_COUNT || atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file, "schema\tlynx-source-suite-comparison-v1\nsuite\t%s\n", suite_index == 0u ? "math" : "memio");
    for (i = 0u; i < oracle->meaningful[suite_index]; i++) {
        unsigned group;
        int accepted = 0;
        for (group = 0u; group < oracle->group_count[suite_index]; group++) {
            const unsigned start = oracle->group_start[suite_index][group];
            const unsigned end = start + oracle->group_length[suite_index][group];
            if (i >= start && i < end) break;
        }
        for (unsigned j = 0u; j < oracle->counts[suite_index][i]; j++) {
            if (oracle->values[suite_index][i][j] == results[i]) accepted = 1;
        }
        (void)fprintf(file, "result\t%u\t%02x\t%u\t%s\tEXPECT(", i, results[i], group,
                      oracle->group_kind[suite_index][group]);
        for (unsigned j = 0u; j < oracle->counts[suite_index][i]; j++) {
            if (j != 0u) (void)fputc(',', file);
            (void)fprintf(file, "%02x", oracle->values[suite_index][i][j]);
        }
        (void)fprintf(file, ")\t%s\n", accepted != 0 ? "source-accepted" : "source-mismatch");
        if (accepted == 0) {
            mismatch_count++;
            if (i < scoped_limit) {
                (void)fprintf(file, "strict-mismatch\t%u\t%02x\tEXPECT(", i, results[i]);
                for (unsigned j = 0u; j < oracle->counts[suite_index][i]; j++) {
                    if (j != 0u) (void)fputc(',', file);
                    (void)fprintf(file, "%02x", oracle->values[suite_index][i][j]);
                }
                (void)fprintf(file, ")\n");
            } else {
                (void)fprintf(file, "strict-mismatch\t%u\t%02x\tEXPECT(", i, results[i]);
                for (unsigned j = 0u; j < oracle->counts[suite_index][i]; j++) {
                    if (j != 0u) (void)fputc(',', file);
                    (void)fprintf(file, "%02x", oracle->values[suite_index][i][j]);
                }
                (void)fprintf(file, ")\n");
            }
        } else if (i < scoped_limit) scoped_count++;
    }
    for (i = oracle->reserved_start[suite_index]; i < RESULT_EXTENT; i++) {
        (void)fprintf(file, "reserved\t%u\t%02x\n", i, results[i]);
    }
    (void)fprintf(file, "scope\t%s\t%s\n", suite_index == 0u ? "arithmetic-register" : "register",
                  scoped_count == scoped_limit ? "match" : "mismatch");
    (void)fprintf(file, "strict\t%s\n", mismatch_count == 0u ? "match" : "mismatch");
    (void)fprintf(file, "strict_mismatch_count\t%u\ncomparison\t%s\n", mismatch_count,
                  mismatch_count == 0u ? "strict-match" : "strict-mismatch");
    if (atomic_output_commit(&output) != 0) return fail("cannot close math/memio comparison record");
    *scoped_agreements = scoped_count;
    *strict_mismatches = mismatch_count;
    return 0;
}

static int write_source_run_record(const char *path, const char *build_directory,
                                   const char *suite, const char *profile,
                                   const suite_source_oracle *oracle, unsigned suite_index,
                                   const lynx_identity *identity, const char *bll_sha,
                                   const char *labels_sha, const char *oracle_sha,
                                   const char *raw_path, const char *comparison_path,
                                   uint64_t ticks, uint8_t probe, uint8_t marker,
                                   uint8_t progress, int detected_model,
                                   uint32_t scoped_agreements, uint32_t strict_mismatches,
                                   const suite_symbols *symbols, uint32_t runtime_pc,
                                   uint32_t runtime_stack)
{
    atomic_output output = {0};
    FILE *file;
    char record_path[4096];
    char build_sha[65], raw_sha[65], comparison_sha[65], wrapper_sha[65], builder_sha[65];
    char layout_sha[65], ca65_sha[65], ld65_sha[65], object_sha[65], labels_identity[65];
    const char *wrapper_name = suite_index == 0u ? "fixtures/suites/math-wrapper.s" : "fixtures/suites/memio-wrapper.s";
    const char *wrapper_key = suite_index == 0u ? "math_wrapper_sha256" : "memio_wrapper_sha256";
    const char *object_key = suite_index == 0u ? "math_object_sha256" : "memio_object_sha256";
    const char *labels_key = suite_index == 0u ? "math_labels_sha256" : "memio_labels_sha256";
    const char *main_key = suite_index == 0u ? "math_main_sha256" : "memio_main_sha256";
    const char *asm_key = suite_index == 0u ? "math_source_sha256" : "memio_source_sha256";
    char path_buffer[4096];
    char value[128];
    unsigned meaningful = oracle->meaningful[suite_index];
    unsigned scoped_count = suite_index == 0u ? 14u : 13u;
    if (checked_join(record_path, sizeof record_path, build_directory, "build-record.txt") != 0 ||
        file_sha256(record_path, build_sha) != 0 || file_sha256(raw_path, raw_sha) != 0 ||
        file_sha256(comparison_path, comparison_sha) != 0 ||
        snprintf(path_buffer, sizeof path_buffer, "%s/%s", LYNX_PROJECT_ROOT, wrapper_name) >= (int)sizeof path_buffer ||
        file_sha256(path_buffer, wrapper_sha) != 0 ||
        snprintf(path_buffer, sizeof path_buffer, "%s/scripts/build-suites.sh", LYNX_PROJECT_ROOT) >= (int)sizeof path_buffer ||
        file_sha256(path_buffer, builder_sha) != 0 ||
        snprintf(path_buffer, sizeof path_buffer, "%s/fixtures/suites/bll.cfg", LYNX_PROJECT_ROOT) >= (int)sizeof path_buffer ||
        file_sha256(path_buffer, layout_sha) != 0 ||
        snprintf(path_buffer, sizeof path_buffer, "%s/build/pins/cc65/bin/ca65", LYNX_PROJECT_ROOT) >= (int)sizeof path_buffer ||
        file_sha256(path_buffer, ca65_sha) != 0 ||
        snprintf(path_buffer, sizeof path_buffer, "%s/build/pins/cc65/bin/ld65", LYNX_PROJECT_ROOT) >= (int)sizeof path_buffer ||
        file_sha256(path_buffer, ld65_sha) != 0) {
        return fail("math/memio run record identity input is incomplete");
    }
    if (record_value(record_path, object_key, object_sha, sizeof object_sha) != 0 ||
        record_value(record_path, labels_key, labels_identity, sizeof labels_identity) != 0 ||
        record_value(record_path, wrapper_key, value, sizeof value) != 0 || strcmp(value, wrapper_sha) != 0 ||
        record_value(record_path, "build_script_sha256", value, sizeof value) != 0 || strcmp(value, builder_sha) != 0 ||
        record_value(record_path, "linker_layout_sha256", value, sizeof value) != 0 || strcmp(value, layout_sha) != 0 ||
        record_value(record_path, "ca65_tool_sha256", value, sizeof value) != 0 || strcmp(value, ca65_sha) != 0 ||
        record_value(record_path, "ld65_tool_sha256", value, sizeof value) != 0 || strcmp(value, ld65_sha) != 0 ||
        record_value(record_path, main_key, value, sizeof value) != 0 || strcmp(value, oracle->main_sha256[suite_index]) != 0 ||
        record_value(record_path, asm_key, value, sizeof value) != 0 || strcmp(value, oracle->asm_sha256[suite_index]) != 0) {
        return fail("math/memio build record does not bind the observed source");
    }
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file,
        "schema=lynx-source-suite-run-v1\nsuite=%s\nmachine_profile=%s\nprofile_input=%s\n"
        "detected_model=%s\nmodel_probe_bytes=%02x,%02x\n"
        "source_main_sha256=%s\nsource_assembly_sha256=%s\nwrapper_sha256=%s\n"
        "builder_sha256=%s\nlayout_sha256=%s\nca65_tool_sha256=%s\nld65_tool_sha256=%s\n"
        "object_sha256=%s\nlabels_sha256=%s\nbll_sha256=%s\nlabels_file_sha256=%s\n"
        "build_record_sha256=%s\noracle_sha256=%s\nraw_results_sha256=%s\ncomparison_sha256=%s\n"
        "result_extent=18\nmeaningful_count=%u\nreserved_start=%u\nreserved_count=%u\n"
        "scoped_count=%u\nscoped_agreements=%u\nstrict_mismatch_count=%u\n"
        "tick_min=%u\ntick_max=%u\nticks_observed=%" PRIu64 "\n"
        "guest_window_ticks=%u\ncompletion_sentinel=a5\nguest_progress=%u\n"
        "source_call_return_label=suite_after_tests@%04x\nsource_runner_rts_label=suite_test_return@%04x\n"
        "post_return_idle_label=suite_idle@%04x\nguest_pc_observed=%04x\nguest_s_observed=%02x\n"
        "source_group_count=%u\n"
        "timing_policy=%s\nhardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        suite, profile, profile, detected_model < 0 ? "unknown" : profile_label((unsigned)detected_model),
        probe, marker, oracle->main_sha256[suite_index], oracle->asm_sha256[suite_index], wrapper_sha,
        builder_sha, layout_sha, ca65_sha, ld65_sha, object_sha, labels_identity, bll_sha, labels_sha,
        build_sha, oracle_sha, raw_sha, comparison_sha, meaningful,
        oracle->reserved_start[suite_index], oracle->reserved_count[suite_index], scoped_count,
        scoped_agreements, strict_mismatches, SOURCE_TICK_MIN, SOURCE_TICK_MAX, ticks,
        SOURCE_TICK_MAX, (unsigned)progress, (unsigned)symbols->value[13],
        (unsigned)symbols->value[12], (unsigned)symbols->value[14], (unsigned)runtime_pc,
        (unsigned)runtime_stack, oracle->group_count[suite_index],
        identity->timing_policy == LYNX_TIMING_POLICY_LEGACY_FLAT5 ? "legacy-flat5" : "unknown");
    if (atomic_output_commit(&output) != 0) return fail("cannot close math/memio run record");
    return 0;
}

static int run_math_memio(const char *suite_name, const char *profile,
                          const char *directory, const char *oracle_path)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char bll_path[4096], labels_path[4096], raw_path[4096], comparison_path[4096], record_path[4096];
    char filename[128], oracle_sha[65], bll_sha[65], labels_sha[65];
    char output_key[96];
    suite_source_oracle oracle;
    suite_symbols symbols;
    machine_instance instance = {0};
    lynx_identity identity;
    lynx__cpu initial_cpu;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    uint8_t results[RESULT_EXTENT];
    uint8_t progress = 0u, done = 0u, model = 0u, probe = 0u;
    uint32_t runtime_pc = 0u, runtime_stack = 0u;
    unsigned suite_index, profile_number;
    uint32_t machine_profile;
    uint64_t total_consumed = 0u, ticks = 0u;
    uint32_t scoped_agreements = 0u, strict_mismatches = 0u;
    int clear_seen = 0, started_seen = 0, complete = 0, return_observed = 0;
    int detected_model = -1, result = 1;
    if (source_suite_index(suite_name, &suite_index) != 0 || profile_index(profile, &profile_number) != 0 ||
        validate_owned_output_directory(directory) != 0 || parse_math_memio_oracle(oracle_path, &oracle) != 0 ||
        verify_math_memio_build_identity(directory, &oracle) != 0) {
        return fail("math/memio source/build identity or profile is invalid");
    }
    machine_profile = profile_number == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
    if (snprintf(filename, sizeof filename, "%s.bll", suite_name) >= (int)sizeof filename ||
        checked_join(bll_path, sizeof bll_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "%s.lbl", suite_name) >= (int)sizeof filename ||
        checked_join(labels_path, sizeof labels_path, directory, filename) != 0 ||
        snprintf(output_key, sizeof output_key, "%s-%s", suite_name, profile) >= (int)sizeof output_key ||
        snprintf(filename, sizeof filename, "raw-results-%s.tsv", output_key) >= (int)sizeof filename ||
        checked_join(raw_path, sizeof raw_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "comparison-%s.tsv", output_key) >= (int)sizeof filename ||
        checked_join(comparison_path, sizeof comparison_path, directory, filename) != 0 ||
        snprintf(filename, sizeof filename, "run-record-%s.txt", output_key) >= (int)sizeof filename ||
        checked_join(record_path, sizeof record_path, directory, filename) != 0 ||
        read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0 ||
        source_symbol_layout(labels, labels_length, bll, bll_length, &symbols) != 0 ||
        file_sha256(bll_path, bll_sha) != 0 || file_sha256(labels_path, labels_sha) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0 || clear_old_outputs(directory, output_key) != 0) {
        (void)fail("math/memio linked object or output identity is invalid");
        goto done;
    }
    if (machine_create(&instance, bll, bll_length, machine_profile) != 0 ||
        lynx_get_identity(instance.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A || identity.entry_addr != symbols.value[0] ||
        identity.object_len != bll_length || identity.timing_policy != LYNX_TIMING_POLICY_LEGACY_FLAT5 ||
        instance.machine->cfg.profile != machine_profile || instance.machine->guest_ticks != 0u ||
        verify_suite_build_artifacts(directory, suite_name) != 0) {
        (void)fail("math/memio public BS93 identity, fresh state or timing configuration mismatch");
        goto done;
    }
    {
        lynx__sha256 sha;
        uint8_t digest[32];
        lynx__sha256_init(&sha);
        lynx__sha256_update(&sha, bll, bll_length);
        lynx__sha256_final(&sha, digest);
        if (memcmp(identity.content_sha256, digest, sizeof digest) != 0) {
            (void)fail("math/memio public BLL content identity mismatch");
            goto done;
        }
    }
    for (uint32_t i = 0u; i < 0x100u; i++) {
        if (instance.machine->ram[i] != 0u) {
            (void)fail("math/memio fresh machine zero-page was not clear before guest entry");
            goto done;
        }
    }
    for (uint32_t i = 0u; i < RESULT_EXTENT; i++) {
        if (instance.machine->ram[symbols.value[2] + i] != 0u) {
            (void)fail("math/memio fresh machine result extent was not clear before guest entry");
            goto done;
        }
    }
    if (lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
        (void)fail("math/memio runner could not arm post-return sentinel watch");
        goto done;
    }
    initial_cpu = instance.machine->cpu;
    while (total_consumed < SOURCE_TICK_MAX) {
        uint64_t budget = SOURCE_TICK_MAX - total_consumed;
        uint64_t used = 0u, before = 0u, after = 0u;
        if (budget > 10000u) budget = 10000u;
        if (lynx_guest_ticks(instance.machine, &before) != LYNX_OK ||
            lynx_advance(instance.machine, budget, &used, &stop) != LYNX_OK || used > budget ||
            lynx_guest_ticks(instance.machine, &after) != LYNX_OK || after < before || after - before != used ||
            after > SOURCE_TICK_MAX) {
            (void)fail("math/memio bounded guest advance or tick accounting failed");
            goto done;
        }
        total_consumed += used;
        ticks = after;
        if (lynx_read_memory(instance.machine, symbols.value[4], &progress, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[5], &model, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[6], &done, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[11], &probe, 1u) != LYNX_OK) {
            (void)fail("math/memio guest progress/model observation read failed");
            goto done;
        }
        if (progress == 1u && done == 0u) started_seen = 1;
        else if (progress == 8u && done == 0xA5u) {
            if (started_seen == 0) {
                (void)fail("math/memio guest completion omitted observed source progress");
                goto done;
            }
        } else if (progress != 0u && progress != 1u && progress != 8u) {
            (void)fail("math/memio wrapper progress left its declared sequence");
            goto done;
        }
        if (stop == LYNX_STOP_WATCH) {
            if (lynx_watch_get(instance.machine, &event) != LYNX_OK || event.hit != 1u ||
                event.address != symbols.value[6] || event.return_tick != after ||
                event.return_tick != event.hit_tick + 1u) {
                (void)fail("math/memio completion watch did not match a guest sentinel write");
                goto done;
            }
            if (event.value == 0u && clear_seen == 0) {
                if (progress != 0u || model != 0u || lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                    (void)fail("math/memio initial completion clear was not an empty-state prefix");
                    goto done;
                }
                clear_seen = 1;
                continue;
            }
            if (event.value == 0xA5u && clear_seen != 0 && progress == 8u && started_seen != 0) {
                if (lynx_read_register(instance.machine, LYNX_REG_PC, &runtime_pc) != LYNX_OK ||
                    lynx_read_register(instance.machine, LYNX_REG_S, &runtime_stack) != LYNX_OK ||
                    runtime_pc != symbols.value[14] || runtime_stack != 0xFFu) {
                    (void)fail("math/memio runtime source-return PC or restored stack does not match linked boundary");
                    goto done;
                }
                return_observed = 1;
                complete = 1;
                break;
            }
            (void)fail("math/memio completion sentinel value or write order mismatch");
            goto done;
        }
        if (stop != LYNX_STOP_BUDGET || used == 0u || after <= before) {
            (void)fail("math/memio guest stopped or made no progress before completion");
            goto done;
        }
    }
    if (return_observed == 0 || validate_guest_completion(&instance, &symbols, suite_name,
                                  memcmp(&initial_cpu, &instance.machine->cpu, sizeof initial_cpu) != 0,
                                  complete, clear_seen, ticks, total_consumed, SOURCE_TICK_MIN,
                                  SOURCE_TICK_MAX, &done, &progress, &model, &probe) != 0 ||
        lynx_read_memory(instance.machine, symbols.value[2], results, RESULT_EXTENT) != LYNX_OK) {
        (void)fail("math/memio fresh bounded completion or full result capture failed");
        goto done;
    }
    detected_model = probe_model(probe, model);
    if (detected_model < 0 || detected_model != (int)profile_number) {
        (void)fail("math/memio guest model probe does not match selected profile");
        goto done;
    }
    if (write_source_raw(raw_path, suite_name, profile, results) != 0 ||
        write_source_comparison(comparison_path, &oracle, suite_index, results,
                                &scoped_agreements, &strict_mismatches) != 0 ||
        write_source_run_record(record_path, directory, suite_name, profile, &oracle, suite_index,
                                &identity, bll_sha, labels_sha, oracle_sha, raw_path,
                                comparison_path, ticks, probe, model, progress, detected_model,
                                scoped_agreements, strict_mismatches, &symbols,
                                runtime_pc, runtime_stack) != 0) goto done;
    printf("math/memio observation complete: suite=%s profile=%s ticks=%" PRIu64
           " progress=%u raw=18 scoped=%u/%u strict_mismatches=%u\n",
           suite_name, profile, ticks, (unsigned)progress, scoped_agreements,
           suite_index == 0u ? 14u : 13u, strict_mismatches);
    puts("hardware_accuracy=unverified profile_qualification=unresolved");
    if (scoped_agreements != (suite_index == 0u ? 14u : 13u)) {
        result = fail("math/memio arithmetic/register source oracle comparison mismatch");
        goto done;
    }
    result = 0;
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
}

static int validate_math_memio_return_evidence(const char *record_path,
                                               const char *directory,
                                               const char *suite_name)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char bll_path[4096], labels_path[4096], value[128], expected[96];
    uint32_t runtime_pc;
    uint8_t runtime_stack;
    suite_symbols symbols;
    int result = 1;
    if (snprintf(bll_path, sizeof bll_path, "%s/%s.bll", directory, suite_name) >= (int)sizeof bll_path ||
        snprintf(labels_path, sizeof labels_path, "%s/%s.lbl", directory, suite_name) >= (int)sizeof labels_path ||
        read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0 ||
        source_symbol_layout(labels, labels_length, bll, bll_length, &symbols) != 0) {
        (void)fail("math/memio linked return boundary could not be verified");
        goto done;
    }
    if (snprintf(expected, sizeof expected, "suite_after_tests@%04x", (unsigned)symbols.value[13]) >= (int)sizeof expected ||
        timer_record_field(record_path, "source_call_return_label", value, sizeof value) != 0 ||
        strcmp(value, expected) != 0 ||
        snprintf(expected, sizeof expected, "suite_test_return@%04x", (unsigned)symbols.value[12]) >= (int)sizeof expected ||
        timer_record_field(record_path, "source_runner_rts_label", value, sizeof value) != 0 ||
        strcmp(value, expected) != 0 ||
        snprintf(expected, sizeof expected, "suite_idle@%04x", (unsigned)symbols.value[14]) >= (int)sizeof expected ||
        timer_record_field(record_path, "post_return_idle_label", value, sizeof value) != 0 ||
        strcmp(value, expected) != 0) {
        (void)fail("math/memio source return boundary evidence missing or mismatched");
        goto done;
    }
    if (timer_record_field(record_path, "guest_pc_observed", value, sizeof value) != 0 ||
        parse_symbol_address(value, &runtime_pc) != 0 || runtime_pc != symbols.value[14] ||
        timer_record_field(record_path, "guest_s_observed", value, sizeof value) != 0 ||
        parse_byte(value, &runtime_stack) != 0 || runtime_stack != 0xFFu) {
        (void)fail("math/memio runtime PC or restored stack does not match linked return boundary");
        goto done;
    }
    result = 0;
done:
    free(labels);
    free(bll);
    return result;
}

static int validate_math_memio_record_mode(const char *suite_name, const char *profile,
                                           const char *directory, const char *oracle_path)
{
    suite_source_oracle oracle;
    uint8_t results[RESULT_EXTENT];
    char raw_path[4096], comparison_path[4096], record_path[4096], file_path[4096];
    char name[128], *text = NULL, line[512], *fields[12];
    size_t length = 0u, offset = 0u, count;
    char value[256], expected_text[160], actual[65];
    uint8_t row_value;
    uint32_t declared_count, row_index, mismatches = 0u, scoped = 0u, tick_value;
    unsigned suite_index, profile_index_value, meaningful, scoped_limit, i;
    if (source_suite_index(suite_name, &suite_index) != 0 || profile_index(profile, &profile_index_value) != 0 ||
        validate_owned_output_directory(directory) != 0 || parse_math_memio_oracle(oracle_path, &oracle) != 0 ||
        verify_math_memio_build_identity(directory, &oracle) != 0) {
        return fail("math/memio record: source/build identity invalid");
    }
    meaningful = oracle.meaningful[suite_index];
    scoped_limit = suite_index == 0u ? 14u : 13u;
    if (snprintf(name, sizeof name, "raw-results-%s-%s.tsv", suite_name, profile) >= (int)sizeof name ||
        checked_join(raw_path, sizeof raw_path, directory, name) != 0 ||
        snprintf(name, sizeof name, "comparison-%s-%s.tsv", suite_name, profile) >= (int)sizeof name ||
        checked_join(comparison_path, sizeof comparison_path, directory, name) != 0 ||
        snprintf(name, sizeof name, "run-record-%s-%s.txt", suite_name, profile) >= (int)sizeof name ||
        checked_join(record_path, sizeof record_path, directory, name) != 0) {
        return fail("math/memio record: path too long");
    }
    if (read_text(raw_path, &text, &length, 8192u) != 0) return fail("math/memio raw record unreadable");
#define SOURCE_RAW_ROW(expected_key, expected_fields) \
    do { \
        if (next_line(text, length, &offset, line, sizeof line) != 0 || \
            (count = split_fields(line, fields, 12u)) != (expected_fields) || \
            strcmp(fields[0], (expected_key)) != 0) { \
            free(text); return fail("math/memio raw record shape mismatch"); \
        } \
    } while (0)
    SOURCE_RAW_ROW("schema", 2u);
    if (strcmp(fields[1], "lynx-source-suite-raw-v1") != 0) { free(text); return fail("math/memio raw record schema mismatch"); }
    SOURCE_RAW_ROW("suite", 2u);
    if (strcmp(fields[1], suite_name) != 0) { free(text); return fail("math/memio raw suite identity mismatch"); }
    SOURCE_RAW_ROW("machine_profile", 2u);
    if (strcmp(fields[1], profile) != 0) { free(text); return fail("math/memio raw profile identity mismatch"); }
    SOURCE_RAW_ROW("profile_input", 2u);
    if (strcmp(fields[1], profile) != 0) { free(text); return fail("math/memio raw profile input mismatch"); }
    SOURCE_RAW_ROW("startup_tier", 2u);
    if (strcmp(fields[1], "A") != 0) { free(text); return fail("math/memio raw startup tier mismatch"); }
    SOURCE_RAW_ROW("result_count", 2u);
    if (parse_decimal(fields[1], &declared_count) != 0 || declared_count != RESULT_EXTENT) {
        free(text); return fail("math/memio raw result count mismatch");
    }
    for (i = 0u; i < RESULT_EXTENT; i++) {
        SOURCE_RAW_ROW("result", 3u);
        if (parse_decimal(fields[1], &row_index) != 0 || row_index != i ||
            parse_byte(fields[2], &results[i]) != 0) {
            free(text); return fail("math/memio raw result row mismatch");
        }
    }
    if (offset != length) { free(text); return fail("math/memio raw record has extra rows"); }
    free(text);
    text = NULL;
    offset = 0u;
#undef SOURCE_RAW_ROW
    for (i = 0u; i < scoped_limit; i++) {
        unsigned alternative;
        int accepted = 0;
        for (alternative = 0u; alternative < oracle.counts[suite_index][i]; alternative++) {
            if (results[i] == oracle.values[suite_index][i][alternative]) accepted = 1;
        }
        if (accepted == 0) return fail("math/memio arithmetic/register scoped comparison failed");
    }
    if (read_text(comparison_path, &text, &length, 16384u) != 0) return fail("math/memio comparison record unreadable");
#define SOURCE_COMPARE_ROW(expected_key, expected_fields) \
    do { \
        if (next_line(text, length, &offset, line, sizeof line) != 0 || \
            (count = split_fields(line, fields, 12u)) != (expected_fields) || \
            strcmp(fields[0], (expected_key)) != 0) { \
            free(text); return fail("math/memio comparison record shape mismatch"); \
        } \
    } while (0)
    SOURCE_COMPARE_ROW("schema", 2u);
    if (strcmp(fields[1], "lynx-source-suite-comparison-v1") != 0) { free(text); return fail("math/memio comparison schema mismatch"); }
    SOURCE_COMPARE_ROW("suite", 2u);
    if (strcmp(fields[1], suite_name) != 0) { free(text); return fail("math/memio comparison suite identity mismatch"); }
    for (i = 0u; i < meaningful; i++) {
        uint32_t group;
        int accepted = 0;
        unsigned j;
        for (group = 0u; group < oracle.group_count[suite_index]; group++) {
            if (i >= oracle.group_start[suite_index][group] &&
                i < (uint32_t)oracle.group_start[suite_index][group] + oracle.group_length[suite_index][group]) break;
        }
        for (j = 0u; j < oracle.counts[suite_index][i]; j++) {
            if (results[i] == oracle.values[suite_index][i][j]) accepted = 1;
        }
        SOURCE_COMPARE_ROW("result", 7u);
        if (parse_decimal(fields[1], &row_index) != 0 || row_index != i ||
            parse_byte(fields[2], &row_value) != 0 || row_value != results[i] ||
            parse_decimal(fields[3], &row_index) != 0 || row_index != group ||
            strcmp(fields[4], oracle.group_kind[suite_index][group]) != 0) {
            free(text); return fail("math/memio comparison row does not match raw/oracle group");
        }
        (void)snprintf(expected_text, sizeof expected_text, "EXPECT(");
        for (j = 0u; j < oracle.counts[suite_index][i]; j++) {
            size_t pos = strlen(expected_text);
            (void)snprintf(expected_text + pos, sizeof expected_text - pos, "%s%02x",
                           j == 0u ? "" : ",", oracle.values[suite_index][i][j]);
        }
        {
            const size_t expected_length = strlen(expected_text);
            if (expected_length + 2u > sizeof expected_text) {
                free(text);
                return fail("math/memio comparison expectation too long");
            }
            expected_text[expected_length] = ')';
            expected_text[expected_length + 1u] = '\0';
        }
        if (strcmp(fields[5], expected_text) != 0 ||
            strcmp(fields[6], accepted != 0 ? "source-accepted" : "source-mismatch") != 0) {
            free(text); return fail("math/memio comparison expectation or verdict mismatch");
        }
        if (accepted != 0 && i < scoped_limit) scoped++;
        if (accepted == 0) {
            SOURCE_COMPARE_ROW("strict-mismatch", 4u);
            if (parse_decimal(fields[1], &row_index) != 0 || row_index != i ||
                parse_byte(fields[2], &row_value) != 0 || row_value != results[i] ||
                strcmp(fields[3], expected_text) != 0) {
                free(text); return fail("math/memio strict mismatch row was omitted or altered");
            }
            mismatches++;
        }
    }
    for (i = oracle.reserved_start[suite_index]; i < RESULT_EXTENT; i++) {
        SOURCE_COMPARE_ROW("reserved", 3u);
        if (parse_decimal(fields[1], &row_index) != 0 || row_index != i ||
            parse_byte(fields[2], &row_value) != 0 || row_value != results[i]) {
            free(text); return fail("math/memio reserved result row mismatch");
        }
    }
    SOURCE_COMPARE_ROW("scope", 3u);
    if (strcmp(fields[1], suite_index == 0u ? "arithmetic-register" : "register") != 0 ||
        strcmp(fields[2], scoped == scoped_limit ? "match" : "mismatch") != 0) {
        free(text); return fail("math/memio scoped comparison verdict mismatch");
    }
    SOURCE_COMPARE_ROW("strict", 2u);
    if (strcmp(fields[1], mismatches == 0u ? "match" : "mismatch") != 0) {
        free(text); return fail("math/memio strict comparison verdict mismatch");
    }
    SOURCE_COMPARE_ROW("strict_mismatch_count", 2u);
    if (parse_decimal(fields[1], &declared_count) != 0 || declared_count != mismatches) {
        free(text); return fail("math/memio strict mismatch count is stale");
    }
    SOURCE_COMPARE_ROW("comparison", 2u);
    if (strcmp(fields[1], mismatches == 0u ? "strict-match" : "strict-mismatch") != 0 || offset != length) {
        free(text); return fail("math/memio strict comparison summary mismatch");
    }
    free(text);
    text = NULL;
#undef SOURCE_COMPARE_ROW
    if (checked_join(file_path, sizeof file_path, directory, "build-record.txt") != 0 ||
        file_sha256(file_path, actual) != 0 || timer_record_is(record_path, "schema", "lynx-source-suite-run-v1", "math/memio run record schema mismatch") != 0 ||
        timer_record_is(record_path, "suite", suite_name, "math/memio run record suite mismatch") != 0 ||
        timer_record_is(record_path, "machine_profile", profile, "math/memio run record profile mismatch") != 0 ||
        timer_record_is(record_path, "profile_input", profile, "math/memio run record profile input mismatch") != 0 ||
        timer_record_is(record_path, "completion_sentinel", "a5", "math/memio completion sentinel mismatch") != 0 ||
        timer_record_is(record_path, "hardware_accuracy", "unverified", "math/memio hardware claim mismatch") != 0 ||
        timer_record_is(record_path, "profile_qualification", "unresolved", "math/memio qualification claim mismatch") != 0) {
        return fail("math/memio execution-envelope record identity invalid");
    }
    if (validate_math_memio_return_evidence(record_path, directory, suite_name) != 0) return 1;
#define SOURCE_RECORD_IS(key, expected_value, diagnostic) \
    do { if (timer_record_is(record_path, (key), (expected_value), (diagnostic)) != 0) return 1; } while (0)
    SOURCE_RECORD_IS("result_extent", "18", "math/memio run result extent mismatch");
    SOURCE_RECORD_IS("guest_progress", "8", "math/memio progress evidence mismatch");
    SOURCE_RECORD_IS("timing_policy", "legacy-flat5", "math/memio run timing policy mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", oracle.group_count[suite_index]) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("source_group_count", expected_text, "math/memio source group count mismatch");
    SOURCE_RECORD_IS("source_main_sha256", oracle.main_sha256[suite_index], "math/memio run source main identity mismatch");
    SOURCE_RECORD_IS("source_assembly_sha256", oracle.asm_sha256[suite_index], "math/memio run source assembly identity mismatch");
    SOURCE_RECORD_IS("build_record_sha256", actual, "math/memio build record hash mismatch");
    if (file_sha256(oracle_path, actual) != 0) return 1;
    SOURCE_RECORD_IS("oracle_sha256", actual, "math/memio oracle hash mismatch");
    if (file_sha256(raw_path, actual) != 0) return 1;
    SOURCE_RECORD_IS("raw_results_sha256", actual, "math/memio raw hash mismatch");
    if (file_sha256(comparison_path, actual) != 0) return 1;
    SOURCE_RECORD_IS("comparison_sha256", actual, "math/memio comparison hash mismatch");
    if (snprintf(file_path, sizeof file_path, "%s/%s.bll", directory, suite_name) >= (int)sizeof file_path ||
        file_sha256(file_path, actual) != 0) return fail("math/memio BLL hash unavailable");
    SOURCE_RECORD_IS("bll_sha256", actual, "math/memio BLL hash mismatch");
    if (snprintf(file_path, sizeof file_path, "%s/%s.lbl", directory, suite_name) >= (int)sizeof file_path ||
        file_sha256(file_path, actual) != 0) return fail("math/memio labels hash unavailable");
    SOURCE_RECORD_IS("labels_file_sha256", actual, "math/memio labels hash mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", meaningful) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("meaningful_count", expected_text, "math/memio meaningful count mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", oracle.reserved_start[suite_index]) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("reserved_start", expected_text, "math/memio reserved start mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", oracle.reserved_count[suite_index]) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("reserved_count", expected_text, "math/memio reserved count mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", scoped_limit) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("scoped_count", expected_text, "math/memio scoped count mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", scoped) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("scoped_agreements", expected_text, "math/memio scoped agreement count mismatch");
    if (snprintf(expected_text, sizeof expected_text, "%u", mismatches) >= (int)sizeof expected_text) return 1;
    SOURCE_RECORD_IS("strict_mismatch_count", expected_text, "math/memio run strict count mismatch");
    SOURCE_RECORD_IS("detected_model", profile, "math/memio guest model mismatch");
    SOURCE_RECORD_IS("model_probe_bytes", profile_index_value == 0u ? "01,a1" : "00,a2", "math/memio model probe malformed");
    if (timer_record_field(record_path, "ticks_observed", value, sizeof value) != 0 ||
        parse_decimal(value, &tick_value) != 0 || tick_value < SOURCE_TICK_MIN || tick_value > SOURCE_TICK_MAX) {
        return fail("math/memio observed guest ticks outside the named bound");
    }
    SOURCE_RECORD_IS("tick_min", "256", "math/memio tick minimum mismatch");
    SOURCE_RECORD_IS("tick_max", "1000000", "math/memio tick maximum mismatch");
    SOURCE_RECORD_IS("guest_window_ticks", "1000000", "math/memio guest window mismatch");
#undef SOURCE_RECORD_IS
    if (scoped != scoped_limit) return fail("math/memio arithmetic/register scoped comparison failed");
    printf("math/memio record: suite=%s profile=%s raw=18 scoped=%u/%u strict_mismatches=%u guest_ticks=%u\n",
           suite_name, profile, scoped, scoped_limit, mismatches, tick_value);
    return 0;
}

static int validate_math_memio_strict_record_mode(const char *suite_name, const char *profile,
                                                  const char *directory, const char *oracle_path)
{
    char comparison_path[4096], filename[128];
    char *text = NULL;
    size_t length = 0u, offset = 0u;
    unsigned mismatch_count = 0u;
    if (validate_math_memio_record_mode(suite_name, profile, directory, oracle_path) != 0) return 1;
    if (snprintf(filename, sizeof filename, "comparison-%s-%s.tsv", suite_name, profile) >= (int)sizeof filename ||
        checked_join(comparison_path, sizeof comparison_path, directory, filename) != 0 ||
        read_text(comparison_path, &text, &length, 16384u) != 0) {
        return fail("math/memio strict comparison record is unreadable");
    }
    while (offset < length) {
        size_t line_length = 0u;
        while (offset + line_length < length && text[offset + line_length] != '\n') line_length++;
        if (line_length >= 512u) {
            free(text);
            return fail("math/memio strict comparison row is oversized");
        }
        if (line_length >= 16u && memcmp(text + offset, "strict-mismatch\t", 16u) == 0) {
            (void)printf("strict source mismatch row: %.*s\n", (int)line_length, text + offset);
            mismatch_count++;
        }
        offset += line_length + (offset + line_length < length ? 1u : 0u);
    }
    free(text);
    if (mismatch_count != 0u) return fail("math/memio strict source comparison failed");
    puts("math/memio strict source comparison: every meaningful source result matches");
    return 0;
}

static int audio_record_field(const char *path, const char *key,
                              char *value, size_t capacity)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    unsigned found = 0u;
    char line[512];
    if (read_text(path, &text, &length, 32768u) != 0) {
        return fail("audio record unreadable");
    }
    while (offset < length) {
        size_t line_length = 0u;
        char *equals;
        while (offset + line_length < length && text[offset + line_length] != '\n') line_length++;
        if (line_length == 0u || line_length >= sizeof line) {
            free(text);
            return fail("audio record malformed field line");
        }
        memcpy(line, text + offset, line_length);
        line[line_length] = '\0';
        offset += line_length + (offset + line_length < length ? 1u : 0u);
        equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0' || strchr(equals + 1, '=') != NULL) {
            free(text);
            return fail("audio record malformed field line");
        }
        *equals = '\0';
        if (strcmp(line, key) == 0) {
            if (found++ != 0u || copy_field(value, capacity, equals + 1) != 0) {
                free(text);
                return fail("audio record duplicate or oversized field");
            }
        }
    }
    free(text);
    if (found != 1u) {
        char message[128];
        (void)snprintf(message, sizeof message, "audio record missing field: %s", key);
        return fail(message);
    }
    return 0;
}

static int audio_record_is(const char *path, const char *key, const char *expected,
                           const char *diagnostic)
{
    char value[512];
    if (audio_record_field(path, key, value, sizeof value) != 0) return 1;
    return strcmp(value, expected) == 0 ? 0 : fail(diagnostic);
}

static int audio_record_number(const char *path, const char *key, uint32_t *out,
                               const char *diagnostic)
{
    char value[64];
    if (audio_record_field(path, key, value, sizeof value) != 0 || parse_decimal(value, out) != 0) {
        return fail(diagnostic);
    }
    return 0;
}

static int audio_record_build_field(const char *record_path, const char *build_path,
                                    const char *record_key, const char *build_key,
                                    const char *diagnostic)
{
    char recorded[128];
    char built[128];
    if (audio_record_field(record_path, record_key, recorded, sizeof recorded) != 0 ||
        record_value(build_path, build_key, built, sizeof built) != 0 ||
        strcmp(recorded, built) != 0) return fail(diagnostic);
    return 0;
}

static int audio_record_file_hash(const char *record_path, const char *key,
                                  const char *file_path, const char *diagnostic)
{
    char recorded[128];
    char actual[65];
    if (audio_record_field(record_path, key, recorded, sizeof recorded) != 0 ||
        file_sha256(file_path, actual) != 0 || !valid_hex(recorded, 64u) ||
        strcmp(recorded, actual) != 0) return fail(diagnostic);
    return 0;
}

static int audio_raw_rows(const char *path, const char *suite, const char *profile,
                          uint8_t results[AUDIO_MEANINGFUL])
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[256];
    char *fields[4];
    uint32_t i;
    const char *keys[] = {"suite", "machine_profile", "profile_input", "ram_seed", "result_count"};
    const char *values[5] = {suite, profile, profile, NULL, "18"};
    char seed[64];
    if (snprintf(seed, sizeof seed, "%s-%s", suite, profile) >= (int)sizeof seed) {
        return fail("audio raw record identity is too long");
    }
    values[3] = seed;
    if (read_text(path, &text, &length, 8192u) != 0) return fail("audio raw record unreadable");
    for (i = 0u; i < 5u; i++) {
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            split_fields(line, fields, 4u) != 2u || strcmp(fields[0], keys[i]) != 0 ||
            strcmp(fields[1], values[i]) != 0) {
            free(text);
            return fail("audio raw record identity or result count mismatch");
        }
    }
    for (i = 0u; i < AUDIO_MEANINGFUL; i++) {
        uint32_t index;
        if (next_line(text, length, &offset, line, sizeof line) != 0 ||
            split_fields(line, fields, 4u) != 3u || strcmp(fields[0], "result") != 0 ||
            parse_decimal(fields[1], &index) != 0 || index != i || parse_byte(fields[2], &results[i]) != 0) {
            free(text);
            return fail("audio raw record result row count/order mismatch");
        }
    }
    if (offset != length) {
        free(text);
        return fail("audio raw record contains extra or duplicate result rows");
    }
    free(text);
    return 0;
}

static int audio_comparison_rows(const char *path, const suite_audio_oracle *oracle,
                                 unsigned suite_index, const uint8_t *results)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    char line[256];
    char *fields[6];
    uint32_t i;
    if (read_text(path, &text, &length, 8192u) != 0) return fail("audio comparison record unreadable");
    for (i = 0u; i < AUDIO_MEANINGFUL; i++) {
        char expected[256];
        size_t pos;
        size_t j;
        int accepted = 0;
        int n;
        n = snprintf(expected, sizeof expected, "result\t%u\t%02x\tEXPECT(", i, results[i]);
        if (n < 0 || (size_t)n >= sizeof expected) { free(text); return fail("audio comparison record malformed"); }
        pos = (size_t)n;
        for (j = 0u; j < oracle->counts[suite_index][i]; j++) {
            n = snprintf(expected + pos, sizeof expected - pos, "%s%02x", j == 0u ? "" : "|",
                         oracle->values[suite_index][i][j]);
            if (n < 0 || (size_t)n >= sizeof expected - pos) { free(text); return fail("audio comparison record malformed"); }
            pos += (size_t)n;
            if (oracle->values[suite_index][i][j] == results[i]) accepted = 1;
        }
        n = snprintf(expected + pos, sizeof expected - pos, ")\t%s", accepted != 0 ? "source-accepted" : "source-mismatch");
        if (n < 0 || (size_t)n >= sizeof expected - pos) { free(text); return fail("audio comparison record malformed"); }
        if (accepted == 0) { free(text); return fail("audio comparison: source oracle mismatch"); }
        if (next_line(text, length, &offset, line, sizeof line) != 0 || strcmp(line, expected) != 0) {
            free(text);
            return fail("audio comparison record row mismatch");
        }
    }
    if (next_line(text, length, &offset, line, sizeof line) != 0 ||
        split_fields(line, fields, 4u) != 2u || strcmp(fields[0], "comparison") != 0 ||
        strcmp(fields[1], "match") != 0 || offset != length) {
        free(text);
        return fail("audio comparison record summary mismatch");
    }
    free(text);
    return 0;
}

static int validate_audio_record_mode(const char *suite, const char *profile,
                                      const char *directory, const char *raw_path,
                                      const char *record_path, const char *legacy_oracle_path,
                                      const char *extended_oracle_path)
{
    suite_oracle legacy;
    suite_audio_oracle audio;
    uint8_t results[AUDIO_MEANINGFUL];
    char path[4096];
    char build_path[4096];
    char bll_path[4096];
    char labels_path[4096];
    char oracle_sha[65];
    char expected[128];
    char probe[16];
    unsigned suite_index;
    unsigned profile_idx;
    uint32_t tick_min, tick_max, ticks, guest_ticks, margin, headroom, value;
    if (strcmp(suite, "audio") == 0) suite_index = 0u;
    else if (strcmp(suite, "audio2") == 0) suite_index = 1u;
    else return fail("audio record: invalid suite");
    if (profile_index(profile, &profile_idx) != 0) return fail("audio record: invalid profile");
    if (checked_join(build_path, sizeof build_path, directory, "build-record.txt") != 0 ||
        snprintf(expected, sizeof expected, "%s.bll", suite) >= (int)sizeof expected ||
        checked_join(bll_path, sizeof bll_path, directory, expected) != 0 ||
        snprintf(expected, sizeof expected, "%s.lbl", suite) >= (int)sizeof expected ||
        checked_join(labels_path, sizeof labels_path, directory, expected) != 0 ||
        parse_oracle(legacy_oracle_path, &legacy) != 0 || read_build_identity(directory, &legacy) != 0 ||
        parse_audio_oracle(legacy_oracle_path, extended_oracle_path, &audio) != 0 ||
        verify_audio_build_identity(directory, &audio) != 0) return 1;
    if (audio_raw_rows(raw_path, suite, profile, results) != 0) return 1;
    {
        char name[128];
        if (snprintf(name, sizeof name, "comparison-%s-%s.tsv", suite, profile) >= (int)sizeof name ||
            checked_join(path, sizeof path, directory, name) != 0 ||
            audio_comparison_rows(path, &audio, suite_index, results) != 0) return 1;
    }
    if (audio_record_is(record_path, "schema", "lynx-audio-suite-run-v1", "audio record: schema mismatch") != 0 ||
        audio_record_is(record_path, "suite", suite, "audio record: run identity mismatch") != 0 ||
        audio_record_is(record_path, "machine_profile", profile, "audio record: run identity mismatch") != 0 ||
        audio_record_is(record_path, "profile_input", profile, "audio record: run identity mismatch") != 0 ||
        audio_record_is(record_path, "detected_model", profile, "audio record: detected model does not match guest evidence") != 0 ||
        audio_record_is(record_path, "source_tuple_model", "not-applicable", "audio record: source tuple model mismatch") != 0) return 1;
    (void)snprintf(probe, sizeof probe, "%s", profile_idx == 0u ? "01,a1" : "00,a2");
    if (audio_record_is(record_path, "model_probe_bytes", probe, "audio record: guest model probe mismatch") != 0 ||
        audio_record_is(record_path, "source_main_sha256", audio.main_sha256[suite_index], "audio record: source identity mismatch") != 0 ||
        audio_record_is(record_path, "source_assembly_sha256", audio.asm_sha256[suite_index], "audio record: source identity mismatch") != 0) return 1;
#define AUDIO_BUILD_ID(record_key, build_key, diagnostic) \
    do { if (audio_record_build_field(record_path, build_path, (record_key), (build_key), (diagnostic)) != 0) return 1; } while (0)
    (void)snprintf(expected, sizeof expected, "%s_wrapper_sha256", suite);
    AUDIO_BUILD_ID("wrapper_sha256", expected, "audio record: wrapper identity mismatch");
    (void)snprintf(expected, sizeof expected, "%s_object_sha256", suite);
    AUDIO_BUILD_ID("object_sha256", expected, "audio record: object identity mismatch");
    (void)snprintf(expected, sizeof expected, "%s_labels_sha256", suite);
    AUDIO_BUILD_ID("labels_sha256", expected, "audio record: labels identity mismatch");
    AUDIO_BUILD_ID("builder_sha256", "build_script_sha256", "audio record: build identity mismatch");
    AUDIO_BUILD_ID("layout_sha256", "linker_layout_sha256", "audio record: layout identity mismatch");
    AUDIO_BUILD_ID("ca65_tool_sha256", "ca65_tool_sha256", "audio record: tool identity mismatch");
    AUDIO_BUILD_ID("ld65_tool_sha256", "ld65_tool_sha256", "audio record: tool identity mismatch");
#undef AUDIO_BUILD_ID
    if (audio_record_file_hash(record_path, "bll_sha256", bll_path, "audio record: content identity mismatch") != 0 ||
        audio_record_file_hash(record_path, "labels_file_sha256", labels_path, "audio record: labels file identity mismatch") != 0 ||
        audio_record_file_hash(record_path, "build_record_sha256", build_path, "audio record: build record identity mismatch") != 0 ||
        file_sha256(legacy_oracle_path, oracle_sha) != 0 ||
        audio_record_is(record_path, "oracle_sha256", oracle_sha, "audio record: oracle identity mismatch") != 0 ||
        audio_record_file_hash(record_path, "raw_results_sha256", raw_path, "audio record: changed observation identity") != 0) return 1;
    {
        char comparison_path[4096];
        char name[128];
        if (snprintf(name, sizeof name, "comparison-%s-%s.tsv", suite, profile) >= (int)sizeof name ||
            checked_join(comparison_path, sizeof comparison_path, directory, name) != 0 ||
            audio_record_file_hash(record_path, "comparison_sha256", comparison_path,
                                   "audio record: comparison identity mismatch") != 0) return 1;
    }
    if (audio_record_number(record_path, "result_extent", &value, "audio record: result extent mismatch") != 0 || value != AUDIO_MEANINGFUL ||
        audio_record_number(record_path, "expected_count", &value, "audio record: result count mismatch") != 0 || value != AUDIO_MEANINGFUL ||
        audio_record_number(record_path, "meaningful_count", &value, "audio record: result count mismatch") != 0 || value != AUDIO_MEANINGFUL ||
        audio_record_number(record_path, "tick_min", &tick_min, "audio record: completion bound metadata mismatch") != 0 ||
        audio_record_number(record_path, "tick_max", &tick_max, "audio record: completion bound metadata mismatch") != 0 ||
        audio_record_number(record_path, "ticks_observed", &ticks, "audio record: invalid ticks_observed") != 0) return 1;
    if (tick_min != AUDIO_MIN_TICKS || tick_max != AUDIO_MAX_TICKS) return fail("audio record: completion bound metadata mismatch");
    if (ticks < tick_min || ticks > tick_max) return fail("audio record: ticks outside completion bounds");
    if (audio_record_number(record_path, "guest_ticks", &guest_ticks, "audio record: guest tick mismatch") != 0 ||
        audio_record_number(record_path, "completion_margin_ticks", &margin, "audio record: completion margin mismatch") != 0 ||
        audio_record_number(record_path, "maximum_headroom_ticks", &headroom, "audio record: maximum headroom mismatch") != 0) return 1;
    if (guest_ticks != ticks || margin != ticks - tick_min || headroom != tick_max - ticks) {
        return fail("audio record: tick accounting mismatch");
    }
    if (audio_record_is(record_path, "minimum_guest_ticks", "256", "audio record: completion bound metadata mismatch") != 0 ||
        audio_record_is(record_path, "maximum_guest_ticks", "50000000", "audio record: completion bound metadata mismatch") != 0 ||
        audio_record_is(record_path, "guest_progress", "8", "audio record: guest progress evidence mismatch") != 0 ||
        audio_record_is(record_path, "completion_sentinel", "a5", "audio record: completion sentinel mismatch") != 0 ||
        audio_record_is(record_path, "guest_return", "observed", "audio record: suite return evidence missing") != 0 ||
        audio_record_is(record_path, "source_agreements", "18", "audio record: source agreement count mismatch") != 0 ||
        audio_record_is(record_path, "source_comparison", "separate-record", "audio record: source comparison policy mismatch") != 0 ||
        audio_record_is(record_path, "timing_policy", "legacy-flat5", "audio record: timing policy mismatch") != 0 ||
        audio_record_is(record_path, "audio_policy", "digital-1", "audio record: audio policy mismatch") != 0 ||
        audio_record_is(record_path, "audio_rate_num", "40000", "audio record: audio format mismatch") != 0 ||
        audio_record_is(record_path, "audio_rate_den", "1", "audio record: audio format mismatch") != 0 ||
        audio_record_is(record_path, "audio_channels", "2", "audio record: audio format mismatch") != 0 ||
        audio_record_is(record_path, "audio_bits", "16", "audio record: audio format mismatch") != 0 ||
        audio_record_is(record_path, "hardware_accuracy", "unverified", "audio record: qualification limit mismatch") != 0 ||
        audio_record_is(record_path, "profile_qualification", "unresolved", "audio record: qualification limit mismatch") != 0) return 1;
    printf("audio record: complete 18-row capture, source and build identities valid for %s/%s\n", suite, profile);
    return 0;
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
                                  int detected_model, int video_revisit)
{
    atomic_output output = {0};
    FILE *file;
    if (atomic_output_begin(path, &output) != 0) return 1;
    file = output.stream;
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
        "page_mode_video_enabled=%s\nrefresh_dma_enabled=%s\ncounter_initialization=zero\n"
        "source_comparison=separate-record\nhardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        machine_profile, selected_profile, bll_sha, labels_sha, oracle_sha, wrapper_sha,
        wrapper_sha, probe_address, model_address, test_return_address,
        after_tests_address, pending,
        detected_model < 0 ? "unknown" : profile_label((unsigned)detected_model),
        probe, model, TIMER_MIN_TICKS, TIMER_MAX_TICKS, ticks,
        ticks - TIMER_MIN_TICKS, TIMER_MAX_TICKS - ticks,
        ticks, TIMER_MIN_TICKS, TIMER_MAX_TICKS, probe, model, progress,
        video_revisit != 0 ? "true" : "false",
        video_revisit != 0 ? "true" : "false");
    if (video_revisit != 0) {
        /* This record keeps the legacy CPU cost model distinct from the
           separately configured video timing policy. */
        (void)fprintf(file,
            "timing_policy=video-policy1\nrefresh_policy=video-policy1\n"
            "video_byte_grant_ticks=5\nrefresh_grant_ticks=5\nrefresh_period_ticks=256\n");
    }
    if (atomic_output_commit(&output) != 0) return fail("cannot close timer run record");
    return 0;
}

static int prepare_timer_video_environment(machine_instance *instance)
{
    uint64_t ticks = UINT64_MAX;
    lynx_machine *machine = instance->machine;
    if (machine == NULL || lynx_guest_ticks(machine, &ticks) != LYNX_OK || ticks != 0u ||
        machine->cfg.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        machine->video.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        machine->cost_model != LYNX__COST_FLAT5 || machine->mapctl != 0u ||
        machine->cpu_page_latch != 0u || machine->ram[0x0001u] != 0u ||
        machine->video.refresh_next_tick != LYNX__VIDEO_REFRESH_PERIOD) {
        return fail("timer video precondition: fresh VIDEO_POLICY1 state mismatch");
    }
    if (!lynx__timer_write(&machine->timer_bank, 0xFD00u, 0x9Eu) ||
        !lynx__timer_write(&machine->timer_bank, 0xFD01u, 0x18u) ||
        !lynx__timer_write(&machine->timer_bank, 0xFD08u, 0x68u) ||
        !lynx__timer_write(&machine->timer_bank, 0xFD09u, 0x1Fu) ||
        !lynx__video_write(&machine->video, 0xFD93u, 0x29u) ||
        !lynx__video_write(&machine->video, 0xFD94u, 0x00u) ||
        !lynx__video_write(&machine->video, 0xFD95u, 0x20u) ||
        !lynx__video_write(&machine->video, 0xFD92u, 0x01u)) {
        return fail("timer video precondition: unsupported frozen register setup");
    }
    machine->mapctl = 0x0Cu;
    if (machine->timer_bank.timers[0].backup != 0x9Eu ||
        machine->timer_bank.timers[0].count != 0u ||
        machine->timer_bank.timers[0].control_a != 0x18u ||
        machine->timer_bank.timers[2].backup != 0x68u ||
        machine->timer_bank.timers[2].count != 0u ||
        machine->timer_bank.timers[2].control_a != 0x1Fu ||
        machine->video.pbkup != 0x29u || machine->video.backup_addr != 0x2000u ||
        machine->video.dispctl != 0x01u || machine->mapctl != 0x0Cu ||
        machine->cpu_page_latch != 0u) {
        return fail("timer video precondition: frozen register readback mismatch");
    }
    return 0;
}

static void timer_video_core_build_id_sha(char output[65])
{
    lynx__sha256 state;
    uint8_t digest[32];
    const char *build_id = lynx_build_id();
    lynx__sha256_init(&state);
    lynx__sha256_update(&state, build_id, strlen(build_id));
    lynx__sha256_final(&state, digest);
    lynx__sha256_hex(digest, output);
}

static int write_timer_video_revisit_record(const char *directory, const char *profile,
                                            const machine_instance *instance,
                                            const char *bll_sha, const char *labels_sha,
                                            const char *oracle_sha, const char *wrapper_sha,
                                            uint32_t source_write_count,
                                            uint32_t acknowledged_increments,
                                            uint8_t initial_clear_seen,
                                            uint8_t counter_at_complete,
                                            uint8_t result14)
{
    char build_record_path[4096], source_root[4096], source_tests_path[4096];
    char source_main_path[4096], layout_path[4096], builder_path[4096], oracle_builder_path[4096];
    char record_path[4096], build_record_sha[65], source_tests_sha[65], source_main_sha[65];
    char layout_sha[65], builder_sha[65], oracle_builder_sha[65], core_build_id_sha[65];
    char source_revision[128], source_archive[128], cc65_archive[128];
    char actual_object[65], actual_labels[65];
    atomic_output output = {0};
    FILE *file;
    const lynx_machine *machine = instance->machine;
    const lynx__timer *timer0 = &machine->timer_bank.timers[0];
    const lynx__timer *timer2 = &machine->timer_bank.timers[2];
    timer_video_core_build_id_sha(core_build_id_sha);
    const int n0 = snprintf(build_record_path, sizeof build_record_path, "%s/build-record.txt", directory);
    const int n1 = snprintf(source_root, sizeof source_root, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT);
    if (n0 < 0 || (size_t)n0 >= sizeof build_record_path || n1 < 0 || (size_t)n1 >= sizeof source_root ||
        snprintf(source_tests_path, sizeof source_tests_path, "%s/timers/tests.s", source_root) >= (int)sizeof source_tests_path ||
        snprintf(source_main_path, sizeof source_main_path, "%s/timers/main.c", source_root) >= (int)sizeof source_main_path ||
        snprintf(layout_path, sizeof layout_path, "%s/fixtures/manifests/timer-tests.manifest", LYNX_PROJECT_ROOT) >= (int)sizeof layout_path ||
        snprintf(builder_path, sizeof builder_path, "%s/scripts/build-suites.sh", LYNX_PROJECT_ROOT) >= (int)sizeof builder_path ||
        snprintf(oracle_builder_path, sizeof oracle_builder_path, "%s/scripts/generate-suite-oracle.sh", LYNX_PROJECT_ROOT) >= (int)sizeof oracle_builder_path ||
        snprintf(record_path, sizeof record_path, "%s/timer-video-revisit-%s.txt", directory, profile) >= (int)sizeof record_path ||
        record_value(build_record_path, "lynx_tests_revision", source_revision, sizeof source_revision) != 0 ||
        record_value(build_record_path, "lynx_tests_pin", source_archive, sizeof source_archive) != 0 ||
        record_value(build_record_path, "cc65_pin", cc65_archive, sizeof cc65_archive) != 0 ||
        record_value(build_record_path, "timers_object_sha256", actual_object, sizeof actual_object) != 0 ||
        record_value(build_record_path, "timers_labels_sha256", actual_labels, sizeof actual_labels) != 0 ||
        strcmp(actual_object, bll_sha) != 0 || strcmp(actual_labels, labels_sha) != 0 ||
        file_sha256(build_record_path, build_record_sha) != 0 ||
        file_sha256(source_tests_path, source_tests_sha) != 0 ||
        file_sha256(source_main_path, source_main_sha) != 0 ||
        file_sha256(layout_path, layout_sha) != 0 ||
        file_sha256(builder_path, builder_sha) != 0 ||
        file_sha256(oracle_builder_path, oracle_builder_sha) != 0) {
        return fail("timer video record: source/tool/object/layout identity unavailable");
    }
    if (atomic_output_begin(record_path, &output) != 0) return 1;
    file = output.stream;
    (void)fprintf(file,
        "schema=lynx-timer-video-revisit-v1\nsuite=timers\nmachine_profile=%s\n"
        "source_revision=%s\nsource_archive_sha256=%s\ncc65_archive_sha256=%s\n"
        "timer_tests_sha256=%s\ntimer_main_sha256=%s\nwrapper_sha256=%s\n"
        "oracle_sha256=%s\nbuild_record_sha256=%s\nbll_sha256=%s\nlabels_sha256=%s\n"
        "layout_manifest_sha256=%s\nbuild_recipe_sha256=%s\noracle_recipe_sha256=%s\n"
        "core_build_id_sha256=%s\ncore_timing_policy=video-policy1\ncpu_cost_model=flat5\ncpu_bus_cycle_ticks=5\n"
        "video_byte_grant_ticks=5\nrefresh_grant_ticks=5\nrefresh_period_ticks=256\n"
        "refresh_next_tick_initial=256\nrefresh_next_tick_final=%" PRIu64 "\n"
        "start_tick=0\ninitial_counter_address=0001\ninitial_counter_value=00\n"
        "counter_initialization=wrapper-zero-page-clear\nwrapper_counter_clear_write_seen=%s\n"
        "counter_watched_guest_writes=%u\nsource_acknowledged_increments=%u\n"
        "counter_value_at_complete=%02x\nresult14=%02x\nstrict_comparison=all-16-source-rows\n"
        "pending_descriptor=none\nsource_timer_index14_admission=none\n"
        "timer0_backup_initial=9e\ntimer0_count_initial=00\ntimer0_control_a_initial=18\n"
        "timer0_backup_final=%02x\ntimer0_count_final=%02x\ntimer0_control_a_final=%02x\ntimer0_control_b_final=%02x\n"
        "timer2_backup_initial=68\ntimer2_count_initial=00\ntimer2_control_a_initial=1f\n"
        "timer2_backup_final=%02x\ntimer2_count_final=%02x\ntimer2_control_a_final=%02x\ntimer2_control_b_final=%02x\n"
        "pbkup_initial=29\npbkup_final=%02x\ndispctl_initial=01\ndispctl_final=%02x\ndispaddr_initial=2000\n"
        "dispaddr_final=%04x\nmapctl_initial=0c\nmapctl_final=%02x\n"
        "page_latch_initial=00\npage_latch_final=%02x\nvideo_buffer=zero-filled-scratch-2000\n"
        "display_dma_owner=machine-event-scheduler\nrefresh_dma_owner=machine-event-scheduler\n"
        "wrapper_initialization=clears-zero-page;sets-mapctl-0c;does-not-set-display-registers\n"
        "crt_difference=DISPCTL-0d-unsupported;project-run-uses-supported-bit0-only\n"
        "hardware_accuracy=unverified\nprofile_qualification=unresolved\n",
        profile, source_revision, source_archive, cc65_archive, source_tests_sha, source_main_sha,
        wrapper_sha, oracle_sha, build_record_sha, bll_sha, labels_sha, layout_sha,
        builder_sha, oracle_builder_sha, core_build_id_sha, machine->video.refresh_next_tick,
        initial_clear_seen != 0u ? "true" : "false", source_write_count,
        acknowledged_increments, counter_at_complete, result14,
        timer0->backup, timer0->count, timer0->control_a, timer0->control_b,
        timer2->backup, timer2->count, timer2->control_a, timer2->control_b,
        machine->video.pbkup, machine->video.dispctl, machine->video.dispaddr,
        machine->mapctl, machine->cpu_page_latch);
    if (atomic_output_commit(&output) != 0) return fail("timer video record close failed");
    return 0;
}

static int timer_video_record_validate(const char *profile, const char *raw_path,
                                      const char *run_record_path, const char *video_record_path,
                                      const char *comparison_path, const char *oracle_path,
                                      const char *directory)
{
    char build_record_path[4096], source_root[4096], source_tests_path[4096];
    char source_main_path[4096], layout_path[4096], builder_path[4096], oracle_builder_path[4096];
    char bll_path[4096], labels_path[4096], value[256], expected[256];
    char source_revision[128], source_archive[128], cc65_archive[128];
    char source_tests_sha[65], source_main_sha[65], layout_sha[65], core_build_id_sha[65];
    char builder_sha[65], oracle_builder_sha[65], build_record_sha[65];
    char bll_sha[65], labels_sha[65], oracle_sha[65], wrapper_sha[65];
    uint8_t results[TIMER_MEANINGFUL];
    uint32_t ticks, guest_ticks, tick_min, tick_max, margin, headroom;
    uint32_t acknowledgements, watched_writes, agreements = 0u;
    uint8_t counter_value, result14;
    unsigned profile_index_value;
    suite_oracle oracle;
    int status = 1;
    if (profile == NULL || raw_path == NULL || run_record_path == NULL ||
        video_record_path == NULL || comparison_path == NULL || oracle_path == NULL ||
        directory == NULL) return fail("timer video record: null input");
    if (profile_index(profile, &profile_index_value) != 0) return fail("timer video record: invalid profile");
    (void)profile_index_value;
    if (!regular_nosymlink_file(raw_path) || !regular_nosymlink_file(run_record_path) ||
        !regular_nosymlink_file(video_record_path) || !regular_nosymlink_file(comparison_path)) {
        return fail("timer video record: missing or unsafe record input");
    }
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0 ||
        verify_suite_build_artifacts(directory, "timers") != 0 ||
        file_sha256(raw_path, expected) != 0 || file_sha256(run_record_path, expected) != 0 ||
        file_sha256(comparison_path, expected) != 0 ||
        snprintf(build_record_path, sizeof build_record_path, "%s/build-record.txt", directory) >= (int)sizeof build_record_path ||
        snprintf(source_root, sizeof source_root, "%s/build/pins/lynx-tests", LYNX_PROJECT_ROOT) >= (int)sizeof source_root ||
        snprintf(source_tests_path, sizeof source_tests_path, "%s/timers/tests.s", source_root) >= (int)sizeof source_tests_path ||
        snprintf(source_main_path, sizeof source_main_path, "%s/timers/main.c", source_root) >= (int)sizeof source_main_path ||
        snprintf(layout_path, sizeof layout_path, "%s/fixtures/manifests/timer-tests.manifest", LYNX_PROJECT_ROOT) >= (int)sizeof layout_path ||
        snprintf(builder_path, sizeof builder_path, "%s/scripts/build-suites.sh", LYNX_PROJECT_ROOT) >= (int)sizeof builder_path ||
        snprintf(oracle_builder_path, sizeof oracle_builder_path, "%s/scripts/generate-suite-oracle.sh", LYNX_PROJECT_ROOT) >= (int)sizeof oracle_builder_path ||
        checked_join(bll_path, sizeof bll_path, directory, "timers.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, directory, "timers.lbl") != 0 ||
        record_value(build_record_path, "lynx_tests_revision", source_revision, sizeof source_revision) != 0 ||
        record_value(build_record_path, "lynx_tests_pin", source_archive, sizeof source_archive) != 0 ||
        record_value(build_record_path, "cc65_pin", cc65_archive, sizeof cc65_archive) != 0 ||
        file_sha256(build_record_path, build_record_sha) != 0 ||
        file_sha256(source_tests_path, source_tests_sha) != 0 ||
        file_sha256(source_main_path, source_main_sha) != 0 ||
        file_sha256(layout_path, layout_sha) != 0 ||
        file_sha256(builder_path, builder_sha) != 0 ||
        file_sha256(oracle_builder_path, oracle_builder_sha) != 0 ||
        file_sha256(bll_path, bll_sha) != 0 || file_sha256(labels_path, labels_sha) != 0 ||
        file_sha256(oracle_path, oracle_sha) != 0 ||
        record_value(build_record_path, "timers_wrapper_sha256", wrapper_sha, sizeof wrapper_sha) != 0) {
        return fail("timer video record: source/tool/build identity unavailable");
    }
    timer_video_core_build_id_sha(core_build_id_sha);
    if (read_timer_raw(raw_path, profile, results) != 0) return 1;
#define VIDEO_RUN_CHECK(key, wanted, diagnostic) \
    do { if (timer_record_is(run_record_path, (key), (wanted), (diagnostic)) != 0) goto done; } while (0)
    VIDEO_RUN_CHECK("suite", "timers", "timer video record: run identity mismatch");
    VIDEO_RUN_CHECK("machine_profile", profile, "timer video record: run identity mismatch");
    VIDEO_RUN_CHECK("profile_input", profile, "timer video record: run identity mismatch");
    VIDEO_RUN_CHECK("ram_seed", "zero", "timer video record: run identity mismatch");
    VIDEO_RUN_CHECK("bll_sha256", bll_sha, "timer video record: object identity mismatch");
    VIDEO_RUN_CHECK("labels_sha256", labels_sha, "timer video record: layout identity mismatch");
    VIDEO_RUN_CHECK("oracle_sha256", oracle_sha, "timer video record: oracle identity mismatch");
    VIDEO_RUN_CHECK("wrapper_sha256", wrapper_sha, "timer video record: wrapper identity mismatch");
    VIDEO_RUN_CHECK("cost_model", "flat5", "timer video record: CPU cost identity mismatch");
    VIDEO_RUN_CHECK("cost_ticks", "5", "timer video record: CPU cost identity mismatch");
    VIDEO_RUN_CHECK("sample_offset_ticks", "0", "timer video record: sampling phase mismatch");
    VIDEO_RUN_CHECK("page_mode_video_enabled", "true", "timer video record: video policy mismatch");
    VIDEO_RUN_CHECK("refresh_dma_enabled", "true", "timer video record: refresh policy mismatch");
    VIDEO_RUN_CHECK("timing_policy", "video-policy1", "timer video record: timing policy mismatch");
    VIDEO_RUN_CHECK("refresh_policy", "video-policy1", "timer video record: refresh policy mismatch");
    VIDEO_RUN_CHECK("video_byte_grant_ticks", "5", "timer video record: video cost identity mismatch");
    VIDEO_RUN_CHECK("refresh_grant_ticks", "5", "timer video record: refresh cost identity mismatch");
    VIDEO_RUN_CHECK("refresh_period_ticks", "256", "timer video record: refresh period mismatch");
    VIDEO_RUN_CHECK("counter_initialization", "zero", "timer video record: counter initialization mismatch");
    VIDEO_RUN_CHECK("hardware_accuracy", "unverified", "timer video record: qualification claim mismatch");
    VIDEO_RUN_CHECK("profile_qualification", "unresolved", "timer video record: qualification claim mismatch");
#undef VIDEO_RUN_CHECK
#define VIDEO_ENV_CHECK(key, wanted, diagnostic) \
    do { if (timer_record_is(video_record_path, (key), (wanted), (diagnostic)) != 0) goto done; } while (0)
    VIDEO_ENV_CHECK("schema", "lynx-timer-video-revisit-v1", "timer video record: unsupported schema");
    VIDEO_ENV_CHECK("suite", "timers", "timer video record: run identity mismatch");
    VIDEO_ENV_CHECK("machine_profile", profile, "timer video record: run identity mismatch");
    VIDEO_ENV_CHECK("source_revision", source_revision, "timer video record: source revision mismatch");
    VIDEO_ENV_CHECK("source_archive_sha256", source_archive, "timer video record: source archive mismatch");
    VIDEO_ENV_CHECK("cc65_archive_sha256", cc65_archive, "timer video record: tool identity mismatch");
    VIDEO_ENV_CHECK("timer_tests_sha256", source_tests_sha, "timer video record: source identity mismatch");
    VIDEO_ENV_CHECK("timer_main_sha256", source_main_sha, "timer video record: source identity mismatch");
    VIDEO_ENV_CHECK("wrapper_sha256", wrapper_sha, "timer video record: wrapper identity mismatch");
    VIDEO_ENV_CHECK("oracle_sha256", oracle_sha, "timer video record: oracle identity mismatch");
    VIDEO_ENV_CHECK("build_record_sha256", build_record_sha, "timer video record: build identity mismatch");
    VIDEO_ENV_CHECK("bll_sha256", bll_sha, "timer video record: object identity mismatch");
    VIDEO_ENV_CHECK("labels_sha256", labels_sha, "timer video record: labels identity mismatch");
    VIDEO_ENV_CHECK("layout_manifest_sha256", layout_sha, "timer video record: layout identity mismatch");
    VIDEO_ENV_CHECK("build_recipe_sha256", builder_sha, "timer video record: build recipe identity mismatch");
    VIDEO_ENV_CHECK("oracle_recipe_sha256", oracle_builder_sha, "timer video record: oracle recipe identity mismatch");
    VIDEO_ENV_CHECK("core_build_id_sha256", core_build_id_sha, "timer video record: core build identity mismatch");
    VIDEO_ENV_CHECK("core_timing_policy", "video-policy1", "timer video record: timing policy mismatch");
    VIDEO_ENV_CHECK("cpu_cost_model", "flat5", "timer video record: CPU cost identity mismatch");
    VIDEO_ENV_CHECK("cpu_bus_cycle_ticks", "5", "timer video record: CPU cost identity mismatch");
    VIDEO_ENV_CHECK("video_byte_grant_ticks", "5", "timer video record: video cost identity mismatch");
    VIDEO_ENV_CHECK("refresh_grant_ticks", "5", "timer video record: refresh cost identity mismatch");
    VIDEO_ENV_CHECK("refresh_period_ticks", "256", "timer video record: refresh period mismatch");
    VIDEO_ENV_CHECK("refresh_next_tick_initial", "256", "timer video record: refresh phase mismatch");
    VIDEO_ENV_CHECK("start_tick", "0", "timer video record: start phase mismatch");
    VIDEO_ENV_CHECK("initial_counter_address", "0001", "timer video record: counter identity mismatch");
    VIDEO_ENV_CHECK("initial_counter_value", "00", "timer video record: counter initialization mismatch");
    VIDEO_ENV_CHECK("counter_initialization", "wrapper-zero-page-clear", "timer video record: counter initialization mismatch");
    VIDEO_ENV_CHECK("wrapper_counter_clear_write_seen", "true", "timer video record: counter initialization mismatch");
    VIDEO_ENV_CHECK("strict_comparison", "all-16-source-rows", "timer video record: comparison policy mismatch");
    VIDEO_ENV_CHECK("pending_descriptor", "none", "timer video record: legacy descriptor inheritance refused");
    VIDEO_ENV_CHECK("source_timer_index14_admission", "none", "timer video record: legacy descriptor inheritance refused");
    VIDEO_ENV_CHECK("timer0_backup_initial", "9e", "timer video record: Timer0 precondition mismatch");
    VIDEO_ENV_CHECK("timer0_count_initial", "00", "timer video record: Timer0 precondition mismatch");
    VIDEO_ENV_CHECK("timer0_control_a_initial", "18", "timer video record: Timer0 precondition mismatch");
    VIDEO_ENV_CHECK("timer2_backup_initial", "68", "timer video record: Timer2 precondition mismatch");
    VIDEO_ENV_CHECK("timer2_count_initial", "00", "timer video record: Timer2 precondition mismatch");
    VIDEO_ENV_CHECK("timer2_control_a_initial", "1f", "timer video record: Timer2 precondition mismatch");
    VIDEO_ENV_CHECK("pbkup_initial", "29", "timer video record: display register mismatch");
    VIDEO_ENV_CHECK("dispctl_initial", "01", "timer video record: display register mismatch");
    VIDEO_ENV_CHECK("dispaddr_initial", "2000", "timer video record: display register mismatch");
    VIDEO_ENV_CHECK("mapctl_initial", "0c", "timer video record: bus map mismatch");
    VIDEO_ENV_CHECK("page_latch_initial", "00", "timer video record: page state mismatch");
    VIDEO_ENV_CHECK("video_buffer", "zero-filled-scratch-2000", "timer video record: display buffer identity mismatch");
    VIDEO_ENV_CHECK("display_dma_owner", "machine-event-scheduler", "timer video record: bus ownership mismatch");
    VIDEO_ENV_CHECK("refresh_dma_owner", "machine-event-scheduler", "timer video record: bus ownership mismatch");
    VIDEO_ENV_CHECK("hardware_accuracy", "unverified", "timer video record: qualification claim mismatch");
    VIDEO_ENV_CHECK("profile_qualification", "unresolved", "timer video record: qualification claim mismatch");
#undef VIDEO_ENV_CHECK
    if (timer_record_number(run_record_path, "ticks_observed", &ticks, "timer video record: invalid ticks_observed") != 0 ||
        timer_record_number(run_record_path, "guest_ticks", &guest_ticks, "timer video record: guest tick records disagree") != 0 ||
        timer_record_number(run_record_path, "tick_min", &tick_min, "timer video record: completion bound metadata mismatch") != 0 ||
        timer_record_number(run_record_path, "tick_max", &tick_max, "timer video record: completion bound metadata mismatch") != 0 ||
        timer_record_number(run_record_path, "completion_margin_ticks", &margin, "timer video record: completion margin mismatch") != 0 ||
        timer_record_number(run_record_path, "maximum_headroom_ticks", &headroom, "timer video record: completion headroom mismatch") != 0) goto done;
    if (tick_min != TIMER_MIN_TICKS || tick_max != TIMER_MAX_TICKS || ticks < tick_min || ticks > tick_max) {
        (void)fail("timer video record: ticks outside completion bounds");
        goto done;
    }
    if (guest_ticks != ticks || margin != ticks - tick_min || headroom != tick_max - ticks) {
        (void)fail("timer video record: checked guest tick arithmetic mismatch");
        goto done;
    }
    if (timer_record_number(video_record_path, "counter_watched_guest_writes", &watched_writes,
                            "timer video record: malformed counter watch count") != 0 ||
        timer_record_number(video_record_path, "source_acknowledged_increments", &acknowledgements,
                            "timer video record: malformed counter increment count") != 0 ||
        timer_record_field(video_record_path, "counter_value_at_complete", value, sizeof value) != 0 ||
        parse_byte(value, &counter_value) != 0 ||
        timer_record_field(video_record_path, "result14", value, sizeof value) != 0 ||
        parse_byte(value, &result14) != 0) {
        (void)fail("timer video record: malformed counter/result evidence");
        goto done;
    }
    if (acknowledgements > UINT8_MAX || watched_writes < acknowledgements ||
        (uint32_t)counter_value != acknowledgements || counter_value != results[14] || result14 != results[14]) {
        (void)fail("timer video record: counter/result evidence mismatch");
        goto done;
    }
    if (validate_timer_comparison_file(comparison_path, &oracle, results, &agreements) != 0) goto done;
    for (uint32_t i = 0u; i < TIMER_MEANINGFUL; i++) {
        int accepted = 0;
        for (uint32_t j = 0u; j < oracle.counts[1][i]; j++) {
            if (oracle.values[1][i][j] == results[i]) accepted = 1;
        }
        if ((i == 14u && accepted != 0) || (i != 14u && accepted == 0)) {
            (void)fail("timer video record: strict mismatch set changed");
            goto done;
        }
    }
    if (agreements != TIMER_MEANINGFUL - 1u) {
        (void)fail("timer video record: strict mismatch set changed");
        goto done;
    }
    printf("timer video record: strict %u/16 source agreements; profile=%s, identity and frozen preconditions valid\n",
           agreements, profile);
    status = 0;
done:
    return status;
}

static int timer_video_null_inputs_self_test(void)
{
    const char *valid = "not-opened";
    if (timer_video_record_validate(NULL, valid, valid, valid, valid, valid, valid) == 0 ||
        timer_video_record_validate("lynx1", NULL, valid, valid, valid, valid, valid) == 0 ||
        timer_video_record_validate("lynx1", valid, NULL, valid, valid, valid, valid) == 0 ||
        timer_video_record_validate("lynx1", valid, valid, NULL, valid, valid, valid) == 0 ||
        timer_video_record_validate("lynx1", valid, valid, valid, NULL, valid, valid) == 0 ||
        timer_video_record_validate("lynx1", valid, valid, valid, valid, NULL, valid) == 0 ||
        timer_video_record_validate("lynx1", valid, valid, valid, valid, valid, NULL) == 0) {
        return fail("timer video null control: validator accepted a NULL input");
    }
    puts("timer video null control: all NULL source/record inputs refused");
    return 0;
}

static int timer_video_time_candidate_check(const char *path)
{
    uint32_t ticks, minimum, maximum;
    if (path == NULL) return fail("timer video time candidate: null input");
    if (timer_record_number(path, "tick_min", &minimum,
                            "timer video time candidate: malformed tick_min") != 0 ||
        timer_record_number(path, "tick_max", &maximum,
                            "timer video time candidate: malformed tick_max") != 0 ||
        timer_record_number(path, "ticks_observed", &ticks,
                            "timer video time candidate: invalid ticks_observed") != 0) return 1;
    if (minimum != TIMER_MIN_TICKS || maximum != TIMER_MAX_TICKS) {
        return fail("timer video time candidate: bound identity mismatch");
    }
    if (ticks < minimum || ticks > maximum) {
        return fail("timer video time candidate: ticks outside completion bounds");
    }
    puts("timer video time candidate: integer within declared bounds; not guest evidence");
    return 0;
}

static int timer_video_comparator_only(const char *profile, const char *raw_path,
                                       const char *comparison_path, const char *oracle_path)
{
    uint8_t results[TIMER_MEANINGFUL];
    uint32_t agreements = 0u;
    suite_oracle oracle;
    if (profile == NULL || raw_path == NULL || comparison_path == NULL || oracle_path == NULL) {
        return fail("timer video comparator-only: null input");
    }
    if (profile_index(profile, &agreements) != 0 || parse_oracle(oracle_path, &oracle) != 0 ||
        read_timer_raw(raw_path, profile, results) != 0 ||
        validate_timer_comparison_file(comparison_path, &oracle, results, &agreements) != 0) return 1;
    if (agreements != TIMER_MEANINGFUL) {
        return fail("timer video comparator-only: all-source agreement is required");
    }
    puts("timer video comparator-only: 16/16 source rows accepted; not guest evidence");
    return 0;
}

static int timer_video_advance_to(machine_instance *instance, uint64_t target,
                                  unsigned mode, uint32_t *watch_events)
{
    uint64_t total = 0u;
    if (mode == 2u && lynx_watch_configure(instance->machine, 0x0001u, 1u) != LYNX_OK) {
        return fail("timer video slicing control: counter watch setup failed");
    }
    while (total < target) {
        uint64_t budget = target - total;
        uint64_t before = 0u, after = 0u, used = 0u;
        lynx_stop_reason stop = LYNX_STOP_BUDGET;
        lynx_watch_event event;
        if (mode == 1u && budget > 37u) budget = 37u;
        if (lynx_guest_ticks(instance->machine, &before) != LYNX_OK ||
            lynx_advance(instance->machine, budget, &used, &stop) != LYNX_OK ||
            used > budget || lynx_guest_ticks(instance->machine, &after) != LYNX_OK ||
            after < before || after - before != used || after > target) {
            return fail("timer video slicing control: bounded advance accounting failed");
        }
        total += used;
        if (stop == LYNX_STOP_WATCH) {
            if (mode != 2u || lynx_watch_get(instance->machine, &event) != LYNX_OK ||
                event.hit != 1u || event.address != 0x0001u ||
                event.return_tick != after || event.return_tick != event.hit_tick + 1u ||
                lynx_watch_configure(instance->machine, 0x0001u, 1u) != LYNX_OK) {
                return fail("timer video slicing control: unexpected watched bus event");
            }
            (*watch_events)++;
        } else if (stop != LYNX_STOP_BUDGET) {
            return fail("timer video slicing control: guest stopped before common tick");
        }
        if (used == 0u || after == before) {
            return fail("timer video slicing control: no progress to common tick");
        }
    }
    return total == target ? 0 : fail("timer video slicing control: common tick mismatch");
}

static int timer_video_slicing_control(const char *profile, const char *directory,
                                       const char *oracle_path, uint32_t target_ticks)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char bll_path[4096], labels_path[4096], bll_sha[65], labels_sha[65];
    char *labels = NULL;
    size_t labels_length = 0u;
    uint8_t results[3][TIMER_MEANINGFUL];
    uint8_t digests[3][32];
    char digest_hex[65];
    uint32_t watch_events = 0u;
    uint32_t profile_value;
    suite_symbols symbols;
    suite_oracle oracle;
    unsigned mode;
    int result = 1;
    if (profile_index(profile, &profile_value) != 0 ||
        (target_ticks < TIMER_MIN_TICKS || target_ticks > TIMER_MAX_TICKS)) {
        return fail("timer video slicing control: invalid profile or common tick");
    }
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(directory, &oracle) != 0 ||
        verify_suite_build_artifacts(directory, "timers") != 0 ||
        checked_join(bll_path, sizeof bll_path, directory, "timers.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, directory, "timers.lbl") != 0 ||
        read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0 ||
        file_sha256(bll_path, bll_sha) != 0 || file_sha256(labels_path, labels_sha) != 0 ||
        parse_symbols(labels, labels_length, &symbols) != 0 || ranges_valid(&symbols, RESULT_EXTENT) != 0) goto done;
    for (mode = 0u; mode < 3u; mode++) {
        machine_instance instance = {0};
        uint32_t selected_profile = profile_value == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
        uint8_t done = 0u, progress = 0u, marker = 0u, probe = 0u;
        uint32_t events_for_mode = 0u;
        if (machine_create_with_policy(&instance, bll, bll_length, selected_profile,
                                       LYNX_TIMING_POLICY_VIDEO_POLICY1) != 0 ||
            prepare_timer_video_environment(&instance) != 0 ||
            prepare_timer_ram(&instance, &symbols) != 0 ||
            timer_video_advance_to(&instance, target_ticks, mode, &events_for_mode) != 0 ||
            lynx_read_memory(instance.machine, symbols.value[2], results[mode], TIMER_MEANINGFUL) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[6], &done, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[4], &progress, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[5], &marker, 1u) != LYNX_OK ||
            lynx_read_memory(instance.machine, symbols.value[11], &probe, 1u) != LYNX_OK ||
            lynx_state_digest(instance.machine, digests[mode]) != LYNX_OK) {
            machine_destroy(&instance);
            goto done;
        }
        if (instance.machine->guest_ticks != target_ticks || done != 0xA5u || progress != 8u ||
            probe != (profile_value == 0u ? 0x01u : 0x00u) ||
            marker != (profile_value == 0u ? 0xA1u : 0xA2u) ||
            instance.machine->ram[0x0001u] != results[mode][14]) {
            machine_destroy(&instance);
            (void)fail("timer video slicing control: common-tick guest completion mismatch");
            goto done;
        }
        if (mode == 2u) watch_events = events_for_mode;
        machine_destroy(&instance);
    }
    if (memcmp(results[0], results[1], sizeof results[0]) != 0 ||
        memcmp(results[0], results[2], sizeof results[0]) != 0 ||
        memcmp(digests[0], digests[1], sizeof digests[0]) != 0 ||
        memcmp(digests[0], digests[2], sizeof digests[0]) != 0 || watch_events == 0u) {
        (void)fail("timer video slicing control: uninterrupted, sliced and watched states differ");
        goto done;
    }
    lynx__sha256_hex(digests[0], digest_hex);
    printf("timer video slicing control: profile=%s common_tick=%u uninterrupted/sliced/watched state equal digest=%s watched_events=%u\n",
           profile, target_ticks, digest_hex, watch_events);
    result = 0;
done:
    free(labels);
    free(bll);
    return result;
}

static int run_timers(const char *machine_profile_name, const char *selected_profile_name,
                      const char *directory, const char *oracle_path,
                      const char *pending_descriptor_path, int no_run,
                      uint32_t maximum_ticks, int video_revisit)
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
    uint32_t source_counter_write_count = 0u;
    uint32_t acknowledged_increments = 0u;
    uint32_t counter_watch_phase = 0u;
    uint8_t counter_value = 0u;
    uint8_t counter_at_complete = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_watch_event event;
    lynx_identity identity;
    lynx__cpu initial_cpu;
    machine_instance instance = {0};
    suite_symbols symbols;
    suite_oracle oracle;
    int complete = 0;
    int initial_done_clear_seen = 0;
    int wrapper_counter_clear_seen = 0;
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
    if (verify_suite_build_artifacts(directory, "timers") != 0 ||
        clear_old_outputs(directory, machine_profile_name) != 0) goto done;
    if ((video_revisit != 0 ?
         machine_create_with_policy(&instance, bll, bll_length, machine_profile,
                                    LYNX_TIMING_POLICY_VIDEO_POLICY1) :
         machine_create(&instance, bll, bll_length, machine_profile)) != 0 ||
        lynx_get_identity(instance.machine, &identity) != LYNX_OK ||
        identity.has_content != 1u || identity.content_format != LYNX_CONTENT_BS93 ||
        identity.startup_tier != LYNX_STARTUP_A || identity.entry_addr != symbols.value[0] ||
        identity.object_len != bll_length || symbols.value[7] < BLL_HEADER_SIZE ||
        symbols.value[7] - 0x01F6u != bll_length) {
        (void)fail("strict timer BS93 identity or linked extent mismatch");
        goto done;
    }
    if (video_revisit != 0 && prepare_timer_video_environment(&instance) != 0) goto done;
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
    if (lynx_watch_configure(instance.machine,
                             video_revisit != 0 ? 0x0001u : symbols.value[6], 1u) != LYNX_OK) {
        (void)fail("could not arm timer completion/counter watch");
        goto done;
    }
    if (video_revisit != 0) counter_watch_phase = 0u;
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
        if (video_revisit != 0) {
            /* One flat5 bus cycle per observation lets the private watch move
               from the wrapper's zero-page clear to Test6 and back without
               skipping the source counter interval. */
            if (budget > 5u) budget = 5u;
            if (counter_watch_phase == 1u && initial_done_clear_seen != 0 &&
                instance.machine->cpu.pc == (uint16_t)(symbols.value[8] + 16u)) {
                if (instance.machine->ram[0x0001u] != 0u ||
                    lynx_watch_configure(instance.machine, 0x0001u, 1u) != LYNX_OK) {
                    (void)fail("timer video counter: source interval did not start from watched zero");
                    goto done;
                }
                counter_watch_phase = 2u;
                counter_value = 0u;
            } else if (counter_watch_phase == 2u &&
                       instance.machine->cpu.pc == (uint16_t)(symbols.value[8] + 19u)) {
                counter_at_complete = instance.machine->ram[0x0001u];
                if (counter_at_complete != counter_value ||
                    lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                    (void)fail("timer video counter: watched source interval did not close at Test6 return");
                    goto done;
                }
                counter_watch_phase = 3u;
            }
        } else if (budget > 10000u) {
            budget = 10000u;
        }
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
                event.hit != 1u || event.return_tick != after || event.return_tick != event.hit_tick + 1u) {
                (void)fail("timer completion/counter watch did not match a guest write");
                goto done;
            }
            if (video_revisit != 0 && event.address == 0x0001u) {
                if (counter_watch_phase == 0u) {
                    if (event.value != 0u || instance.machine->ram[0x0001u] != 0u ||
                        lynx_watch_configure(instance.machine, symbols.value[6], 1u) != LYNX_OK) {
                        (void)fail("timer video counter: wrapper zero initialization was not observed");
                        goto done;
                    }
                    wrapper_counter_clear_seen = 1;
                    counter_watch_phase = 1u;
                } else if (counter_watch_phase == 2u) {
                    source_counter_write_count++;
                    if (event.value == counter_value) {
                        /* The first 65C02 RMW bus write preserves the old byte. */
                    } else if (event.value <= UINT8_MAX &&
                               event.value == (uint32_t)(uint8_t)(counter_value + 1u)) {
                        counter_value = (uint8_t)event.value;
                        acknowledged_increments++;
                    } else {
                        (void)fail("timer video counter: source write was not an increment or RMW old-byte write");
                        goto done;
                    }
                    if (lynx_watch_configure(instance.machine, 0x0001u, 1u) != LYNX_OK) {
                        (void)fail("timer video counter: source write watch could not be rearmed");
                        goto done;
                    }
                } else {
                    (void)fail("timer video counter: write occurred outside the frozen source interval");
                    goto done;
                }
                continue;
            }
            if (event.address != symbols.value[6]) {
                (void)fail("timer completion/counter watch hit an unexpected address");
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
        if ((used == 0u || after <= before) && video_revisit == 0) {
            (void)fail("timer guest made no bounded progress toward completion");
            goto done;
        }
        if ((used == 0u || after <= before) && video_revisit != 0) {
            (void)fail("timer video guest made no progress in one-cycle watch mode");
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
    if (video_revisit != 0 &&
        (counter_watch_phase != 3u || wrapper_counter_clear_seen == 0 ||
         counter_at_complete != instance.machine->ram[0x0001u] ||
         counter_at_complete != results[14])) {
        (void)fail("timer video counter: source writes, final counter, and result14 disagree");
        goto done;
    }
    detected_model = probe_model(probe, model);
    if (write_raw(raw_path, "timers", machine_profile_name, selected_profile_name,
                  "zero", results, TIMER_MEANINGFUL) != 0 ||
        write_timer_run_record(record_path, machine_profile_name, selected_profile_name,
                               bll_sha, labels_sha, oracle_sha, wrapper_sha, guest_ticks,
                               probe, model, progress, symbols.value[11], symbols.value[5],
                               symbols.value[12], symbols.value[13], instance.machine->timer_bank.pending,
                               detected_model, video_revisit) != 0 ||
        (video_revisit != 0 &&
         write_timer_video_revisit_record(directory, machine_profile_name, &instance,
                                          bll_sha, labels_sha, oracle_sha, wrapper_sha,
                                          source_counter_write_count, acknowledged_increments,
                                          (uint8_t)wrapper_counter_clear_seen, counter_at_complete,
                                          results[14]) != 0)) goto done;
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

static const char *const page_mode_record_keys[] = {
    "schema", "suite", "machine_profile", "detected_model", "probe_byte", "model_marker",
    "probe_source", "probe_source_sha256", "bll_sha256", "labels_sha256", "oracle_sha256",
    "build_record_sha256", "wrapper_sha256", "layout_sha256", "manifest_sha256",
    "build_recipe_sha256", "oracle_recipe_sha256", "ca65_sha256", "ld65_sha256",
    "timing_policy", "refresh_policy", "display_dma", "mapctl", "result_extent",
    "expected_count", "meaningful_count", "raw_observation", "source_comparison",
    "ticks_observed", "tick_min", "tick_max", "guest_progress", "guest_done", "stop",
    "request_address", "request_kind", "request_value", "no_run_hold_verified",
    "timer0_backup", "timer0_control", "timer2_backup", "timer2_control",
    "hardware_accuracy", "profile_qualification"
};

static int page_mode_record_shape(const char *path)
{
    char *text = NULL;
    size_t length = 0u;
    size_t offset = 0u;
    uint8_t seen[sizeof page_mode_record_keys / sizeof page_mode_record_keys[0]] = {0u};
    size_t rows = 0u;
    if (!regular_nosymlink_file(path) || read_text(path, &text, &length, 16384u) != 0) {
        return fail("page-mode record is missing or not a regular file");
    }
    while (offset < length) {
        char line[256];
        char *equals;
        size_t line_length = 0u;
        size_t key_index;
        while (offset + line_length < length && text[offset + line_length] != '\n') line_length++;
        if (line_length == 0u || line_length >= sizeof line || offset + line_length >= length) {
            free(text);
            return fail("page-mode record malformed line");
        }
        memcpy(line, text + offset, line_length);
        line[line_length] = '\0';
        offset += line_length + 1u;
        equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0' || strchr(equals + 1, '=') != NULL) {
            free(text);
            return fail("page-mode record malformed field");
        }
        *equals = '\0';
        for (key_index = 0u; key_index < sizeof page_mode_record_keys / sizeof page_mode_record_keys[0]; key_index++) {
            if (strcmp(line, page_mode_record_keys[key_index]) == 0) break;
        }
        if (key_index == sizeof page_mode_record_keys / sizeof page_mode_record_keys[0]) {
            free(text);
            return fail("page-mode record unknown key");
        }
        if (seen[key_index] != 0u) {
            free(text);
            return fail("page-mode record duplicate key");
        }
        seen[key_index] = 1u;
        rows++;
    }
    free(text);
    if (rows != sizeof page_mode_record_keys / sizeof page_mode_record_keys[0]) {
        return fail("page-mode record missing key");
    }
    return 0;
}

static int page_mode_record_check(const char *path, const char *profile,
                                  const page_mode_build_identity *identity,
                                  uint32_t expected_ticks)
{
    char value[256];
    uint32_t parsed;
    const char *probe_byte = strcmp(profile, "lynx1") == 0 ? "01" : "00";
    const char *model_marker = strcmp(profile, "lynx1") == 0 ? "a1" : "a2";
    if (page_mode_record_shape(path) != 0) return 1;
#define PAGE_CHECK(key, want, message) do { \
        if (record_value(path, (key), value, sizeof value) != 0 || strcmp(value, (want)) != 0) \
            return fail(message); \
    } while (0)
    PAGE_CHECK("schema", "lynx-page-mode-run-v1", "page-mode record unsupported schema");
    PAGE_CHECK("suite", "page-mode", "page-mode record suite mismatch");
    PAGE_CHECK("machine_profile", profile, "page-mode record profile mismatch");
    PAGE_CHECK("detected_model", profile, "page-mode record detected model mismatch");
    PAGE_CHECK("probe_byte", probe_byte, "page-mode record model probe mismatch");
    PAGE_CHECK("model_marker", model_marker, "page-mode record model marker mismatch");
    PAGE_CHECK("probe_source", "fixtures/suites/page-mode-wrapper.s", "page-mode record probe source mismatch");
    PAGE_CHECK("probe_source_sha256", identity->wrapper_sha256, "page-mode record probe source identity mismatch");
    PAGE_CHECK("bll_sha256", identity->object_sha256, "page-mode record BLL identity mismatch");
    PAGE_CHECK("labels_sha256", identity->labels_sha256, "page-mode record labels identity mismatch");
    PAGE_CHECK("oracle_sha256", identity->oracle_sha256, "page-mode record oracle identity mismatch");
    PAGE_CHECK("build_record_sha256", identity->build_record_sha256, "page-mode record build record identity mismatch");
    PAGE_CHECK("wrapper_sha256", identity->wrapper_sha256, "page-mode record wrapper identity mismatch");
    PAGE_CHECK("layout_sha256", identity->layout_sha256, "page-mode record layout identity mismatch");
    PAGE_CHECK("manifest_sha256", identity->manifest_sha256, "page-mode record manifest identity mismatch");
    PAGE_CHECK("build_recipe_sha256", identity->recipe_sha256, "page-mode record builder identity mismatch");
    PAGE_CHECK("oracle_recipe_sha256", identity->oracle_recipe_sha256, "page-mode record oracle recipe identity mismatch");
    PAGE_CHECK("ca65_sha256", identity->ca65_sha256, "page-mode record ca65 identity mismatch");
    PAGE_CHECK("ld65_sha256", identity->ld65_sha256, "page-mode record ld65 identity mismatch");
    PAGE_CHECK("timing_policy", "video-policy1", "page-mode record timing policy mismatch");
    PAGE_CHECK("refresh_policy", "video-policy1", "page-mode record refresh policy mismatch");
    PAGE_CHECK("display_dma", "disabled-dispctl-00", "page-mode record display DMA precondition mismatch");
    PAGE_CHECK("mapctl", "0c", "page-mode record MAPCTL precondition mismatch");
    PAGE_CHECK("result_extent", "6", "page-mode record result extent mismatch");
    PAGE_CHECK("expected_count", "6", "page-mode record expected count mismatch");
    PAGE_CHECK("meaningful_count", "0", "page-mode record must preserve missing results");
    PAGE_CHECK("raw_observation", "missing", "page-mode record raw observation must remain missing");
    PAGE_CHECK("source_comparison", "not-run", "page-mode record comparison must remain unrun");
    PAGE_CHECK("tick_min", "40000", "page-mode record tick minimum mismatch");
    PAGE_CHECK("tick_max", "50000000", "page-mode record tick maximum mismatch");
    PAGE_CHECK("guest_progress", "1", "page-mode record source progress mismatch");
    PAGE_CHECK("guest_done", "00", "page-mode record completion state mismatch");
    PAGE_CHECK("stop", "unavailable", "page-mode record stop reason mismatch");
    PAGE_CHECK("request_address", "fd8c", "page-mode record unsupported request address mismatch");
    PAGE_CHECK("request_kind", "write", "page-mode record unsupported request kind mismatch");
    PAGE_CHECK("request_value", "06", "page-mode record unsupported request value mismatch");
    PAGE_CHECK("timer0_backup", "9e", "page-mode record Timer0 source setup mismatch");
    PAGE_CHECK("timer0_control", "18", "page-mode record Timer0 control setup mismatch");
    PAGE_CHECK("timer2_backup", "68", "page-mode record Timer2 source setup mismatch");
    PAGE_CHECK("timer2_control", "1f", "page-mode record Timer2 control setup mismatch");
    PAGE_CHECK("hardware_accuracy", "unverified", "page-mode record hardware status mismatch");
    PAGE_CHECK("profile_qualification", "unresolved", "page-mode record profile qualification mismatch");
    if (record_value(path, "ticks_observed", value, sizeof value) != 0 ||
        parse_decimal(value, &parsed) != 0 || parsed < 40000u || parsed > PAGE_MODE_MAX_TICKS ||
        (expected_ticks != 0u && parsed != expected_ticks)) {
        return fail("page-mode record ticks outside bound or do not match guest time");
    }
    if (record_value(path, "no_run_hold_verified", value, sizeof value) != 0 ||
        (strcmp(value, "true") != 0 && strcmp(value, "false") != 0)) {
        return fail("page-mode record no-run evidence malformed");
    }
#undef PAGE_CHECK
    return 0;
}

static int validate_page_mode_record_mode(const char *profile, const char *run_directory,
                                          const char *build_directory, const char *oracle_path,
                                          const char *record_path)
{
    unsigned profile_number;
    char expected_record[4096];
    char record_name[96];
    page_mode_oracle oracle;
    page_mode_build_identity identity;
    if (profile_index(profile, &profile_number) != 0 ||
        snprintf(record_name, sizeof record_name, "page-mode-run-%s.txt", profile) >= (int)sizeof record_name ||
        checked_join(expected_record, sizeof expected_record, run_directory, record_name) != 0 ||
        strcmp(expected_record, record_path) != 0 || validate_owned_output_directory(run_directory) != 0 ||
        parse_page_mode_oracle(oracle_path, &oracle) != 0 ||
        verify_page_mode_build(run_directory, build_directory, oracle_path, &oracle, &identity) != 0 ||
        page_mode_record_check(record_path, profile, &identity, 0u) != 0) return 1;
    (void)profile_number;
    printf("page-mode record: exact SERCTL blocker and missing six-byte observation valid for %s\n", profile);
    return 0;
}

static int run_page_mode(const char *profile, const char *run_directory,
                         const char *build_directory, const char *oracle_path,
                         const char *record_path, int no_run, int control_mode)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    char *labels = NULL;
    size_t labels_length = 0u;
    char expected_record[4096];
    char record_name[96];
    char bll_path[4096];
    char labels_path[4096];
    char record_path_expected[4096];
    char bll_sha[65], labels_sha[65], oracle_sha[65];
    uint32_t profile_number;
    uint32_t machine_profile;
    uint64_t total_consumed = 0u;
    uint64_t guest_ticks = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    machine_instance instance = {0};
    suite_symbols symbols;
    page_mode_oracle oracle;
    page_mode_build_identity identity;
    lynx_identity public_identity;
    uint8_t model_byte;
    uint8_t model_marker;
    uint8_t progress;
    uint8_t done;
    uint8_t *results;
    int hold_verified = 0;
    int result = 1;
    if (profile_index(profile, &profile_number) != 0 ||
        snprintf(record_name, sizeof record_name, "page-mode-run-%s.txt", profile) >= (int)sizeof record_name ||
        checked_join(record_path_expected, sizeof record_path_expected, run_directory, record_name) != 0 ||
        strcmp(record_path_expected, record_path) != 0 ||
        validate_owned_output_directory(run_directory) != 0 ||
        parse_page_mode_oracle(oracle_path, &oracle) != 0 ||
        verify_page_mode_build(run_directory, build_directory, oracle_path, &oracle, &identity) != 0) {
        return fail("page-mode runner source/build identity or invocation mismatch");
    }
    machine_profile = profile_number == 0u ? LYNX_PROFILE_LYNX1 : LYNX_PROFILE_LYNX2;
    if (checked_join(bll_path, sizeof bll_path, build_directory, "page-mode.bll") != 0 ||
        checked_join(labels_path, sizeof labels_path, build_directory, "page-mode.lbl") != 0 ||
        read_file(bll_path, &bll, &bll_length, 65536u) != 0 ||
        read_text(labels_path, &labels, &labels_length, 16384u) != 0 ||
        parse_symbols(labels, labels_length, &symbols) != 0 ||
        ranges_valid(&symbols, PAGE_MODE_RESULT_COUNT) != 0 ||
        symbols.value[3] != symbols.value[2] + PAGE_MODE_RESULT_COUNT ||
        symbols.value[11] >= SUITE_ZP_END || file_sha256(bll_path, bll_sha) != 0 ||
        file_sha256(labels_path, labels_sha) != 0 || file_sha256(oracle_path, oracle_sha) != 0 ||
        strcmp(bll_sha, identity.object_sha256) != 0 || strcmp(labels_sha, identity.labels_sha256) != 0 ||
        strcmp(oracle_sha, identity.oracle_sha256) != 0) {
        (void)fail("page-mode symbols, result extent or build identity mismatch");
        goto done;
    }
    if (machine_create_with_policy(&instance, bll, bll_length, machine_profile,
                                   LYNX_TIMING_POLICY_VIDEO_POLICY1) != 0 ||
        lynx_get_identity(instance.machine, &public_identity) != LYNX_OK ||
        public_identity.has_content != 1u || public_identity.content_format != LYNX_CONTENT_BS93 ||
        public_identity.startup_tier != LYNX_STARTUP_A ||
        public_identity.entry_addr != symbols.value[0] || public_identity.object_len != bll_length ||
        symbols.value[7] < BLL_HEADER_SIZE || symbols.value[7] - 0x01F6u != bll_length ||
        instance.machine->cfg.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        instance.machine->mapctl != 0u ||
        instance.machine->video.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1) {
        (void)fail("page-mode public BS93 identity or VIDEO_POLICY1 environment mismatch");
        goto done;
    }
    if (lynx__machine_set_bus_hold(instance.machine, 0u) != LYNX_OK) {
        (void)fail("page-mode runner could not establish released bus state");
        goto done;
    }
    if (no_run != 0) {
        if (prove_held_no_run(&instance, &symbols, PAGE_MODE_MIN_TICKS, &guest_ticks) != 0 ||
            lynx__machine_set_bus_hold(instance.machine, 0u) != LYNX_OK ||
            instance.machine->bus_hold != 0u) {
            (void)fail("page-mode no-run hold/refusal proof failed");
            goto done;
        }
        total_consumed = guest_ticks;
        hold_verified = 1;
    }
    while (total_consumed < PAGE_MODE_MAX_TICKS) {
        uint64_t budget = PAGE_MODE_MAX_TICKS - total_consumed;
        uint64_t used = 0u;
        uint64_t before = 0u;
        uint64_t after = 0u;
        if (budget > 10000u) budget = 10000u;
        if (lynx_guest_ticks(instance.machine, &before) != LYNX_OK ||
            lynx_advance(instance.machine, budget, &used, &stop) != LYNX_OK || used > budget ||
            lynx_guest_ticks(instance.machine, &after) != LYNX_OK || after < before ||
            after - before != used || after > PAGE_MODE_MAX_TICKS) {
            (void)fail("page-mode bounded guest advance or tick accounting failed");
            goto done;
        }
        total_consumed += used;
        guest_ticks = after;
        if (stop == LYNX_STOP_UNAVAILABLE) break;
        if (stop != LYNX_STOP_BUDGET) {
            (void)fail("page-mode guest stopped before the exact unsupported request");
            goto done;
        }
        if (used == 0u) {
            (void)fail("page-mode guest made no progress within a nonempty budget");
            goto done;
        }
    }
    if (stop != LYNX_STOP_UNAVAILABLE || guest_ticks < PAGE_MODE_MIN_TICKS ||
        guest_ticks > PAGE_MODE_MAX_TICKS || instance.machine->cpu.req_addr != 0xFD8Cu ||
        (instance.machine->cpu.req_flags & LYNX__CF_WRITE) == 0u ||
        instance.machine->cpu.req_data != 0x06u || instance.machine->mapctl != 0x0Cu ||
        instance.machine->cfg.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        instance.machine->video.timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        instance.machine->video.dispctl != 0u ||
        instance.machine->timer_bank.timers[0].backup != 0x9Eu ||
        instance.machine->timer_bank.timers[0].control_a != 0x18u ||
        instance.machine->timer_bank.timers[2].backup != 0x68u ||
        instance.machine->timer_bank.timers[2].control_a != 0x1Fu) {
        (void)fail("page-mode did not stop at the exact first SERCTL write under its declared preconditions");
        goto done;
    }
    model_byte = instance.machine->ram[symbols.value[11]];
    model_marker = instance.machine->ram[symbols.value[5]];
    progress = instance.machine->ram[symbols.value[4]];
    done = instance.machine->ram[symbols.value[6]];
    results = instance.machine->ram + symbols.value[2];
    if (probe_model(model_byte, model_marker) != (int)profile_number || progress != 1u || done != 0u ||
        memcmp(results, "\0\0\0\0\0\0", PAGE_MODE_RESULT_COUNT) != 0) {
        (void)fail("page-mode model probe, setup progress or unrun result state mismatch");
        goto done;
    }
    if (snprintf(expected_record, sizeof expected_record, "%s", record_path) >= (int)sizeof expected_record) {
        (void)fail("page-mode record path too long");
        goto done;
    }
    {
        atomic_output output = {0};
        FILE *file;
        if (atomic_output_begin(record_path, &output) != 0) goto done;
        file = output.stream;
        (void)fprintf(file,
            "schema=lynx-page-mode-run-v1\nsuite=page-mode\nmachine_profile=%s\ndetected_model=%s\n"
            "probe_byte=%02x\nmodel_marker=%02x\nprobe_source=fixtures/suites/page-mode-wrapper.s\n"
            "probe_source_sha256=%s\nbll_sha256=%s\nlabels_sha256=%s\noracle_sha256=%s\n"
            "build_record_sha256=%s\nwrapper_sha256=%s\nlayout_sha256=%s\nmanifest_sha256=%s\n"
            "build_recipe_sha256=%s\noracle_recipe_sha256=%s\nca65_sha256=%s\nld65_sha256=%s\n"
            "timing_policy=video-policy1\nrefresh_policy=video-policy1\ndisplay_dma=disabled-dispctl-00\n"
            "mapctl=0c\nresult_extent=6\nexpected_count=6\nmeaningful_count=0\n"
            "raw_observation=missing\nsource_comparison=not-run\nticks_observed=%" PRIu64 "\n"
            "tick_min=%u\ntick_max=%u\nguest_progress=%u\nguest_done=%02x\nstop=unavailable\n"
            "request_address=fd8c\nrequest_kind=write\nrequest_value=06\nno_run_hold_verified=%s\n"
            "timer0_backup=9e\ntimer0_control=18\ntimer2_backup=68\ntimer2_control=1f\n"
            "hardware_accuracy=unverified\nprofile_qualification=unresolved\n",
            profile, profile, model_byte, model_marker, identity.wrapper_sha256, identity.object_sha256,
            identity.labels_sha256, identity.oracle_sha256, identity.build_record_sha256,
            identity.wrapper_sha256, identity.layout_sha256, identity.manifest_sha256,
            identity.recipe_sha256, identity.oracle_recipe_sha256, identity.ca65_sha256,
            identity.ld65_sha256, guest_ticks, PAGE_MODE_MIN_TICKS, PAGE_MODE_MAX_TICKS,
            (unsigned)progress, (unsigned)done, hold_verified != 0 ? "true" : "false");
        if (atomic_output_commit(&output) != 0) goto done;
    }
    if (page_mode_record_check(record_path, profile, &identity, (uint32_t)guest_ticks) != 0) goto done;
    if (control_mode != 0) {
        printf("page-mode control: exact SERCTL write blocker at $FD8C after %" PRIu64
               " guest ticks; profile=%s progress=1 missing-results=6 no-run=%s\n",
               guest_ticks, profile, hold_verified != 0 ? "verified" : "not-requested");
    } else {
        puts("LYNX-SKIP: page-mode source requires unavailable SERCTL write at $FD8C");
    }
    result = 0;
done:
    machine_destroy(&instance);
    free(labels);
    free(bll);
    return result;
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

static int validate_pin_marker_mode(const char *path, const char *expected)
{
    char *stamp = NULL;
    size_t length = 0u;
    int matches;
    if (!valid_hex(expected, 64u)) return fail("pin marker: expected identity invalid");
    if (read_text(path, &stamp, &length, 1024u) != 0) return 1;
    matches = pin_marker_matches(stamp, length, expected);
    free(stamp);
    if (!matches) return fail("pin marker: malformed or mismatched identity");
    puts("pin marker: exact identity valid");
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
    unsigned char seen[16] = {0u};
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
        "suite.timers.observation.lynx1", "suite.timers.observation.lynx2",
        "suite.page_mode.observation.lynx1", "suite.page_mode.observation.lynx2",
        "suite.audio.observation.lynx1", "suite.audio.observation.lynx2",
        "suite.audio2.observation.lynx1", "suite.audio2.observation.lynx2",
        "suite.math.observation.lynx1", "suite.math.observation.lynx2",
        "suite.memio.observation.lynx1", "suite.memio.observation.lynx2"
    };
    static const char *const control_ids[] = {
        "suite.cpu.no_run", "suite.timers.no_run",
        "suite.completion_controls", "suite.selection_controls",
        "suite.page_mode.controls", "suite.audio.controls",
        "suite.audio.source_controls", "suite.audio2.controls",
        "suite.math.controls", "suite.memio.controls",
        "suite.math.source_controls", "suite.math.strict_comparison_controls"
    };
    if (validate_selection_list(required, "required-suite", required_ids,
                                sizeof required_ids / sizeof required_ids[0]) != 0 ||
        validate_selection_list(controls, "control", control_ids,
                                sizeof control_ids / sizeof control_ids[0]) != 0) return 1;
    puts("suite selection: required=14 controls=12 inventory complete");
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
    if (parse_oracle(oracle_path, &oracle) != 0 || read_build_identity(artifact_directory, &oracle) != 0 ||
        verify_suite_build_artifacts(artifact_directory, "cpu") != 0 ||
        file_sha256(oracle_path, actual_oracle_sha) != 0 ||
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
    if (argc == 3 && strcmp(argv[1], "validate-math-memio-oracle") == 0) {
        suite_source_oracle oracle;
        if (parse_math_memio_oracle(argv[2], &oracle) != 0) return 1;
        puts("math/memio oracle: exact source groups, meaningful rows and reserved extents valid");
        return 0;
    }
    if (argc == 7 && strcmp(argv[1], "observe") == 0 && strcmp(argv[2], "math-memio") == 0) {
        return run_math_memio(argv[3], argv[4], argv[5], argv[6]);
    }
    if (argc == 6 && strcmp(argv[1], "validate-math-memio-record") == 0) {
        return validate_math_memio_record_mode(argv[2], argv[3], argv[4], argv[5]);
    }
    if (argc == 6 && strcmp(argv[1], "validate-math-memio-strict-record") == 0) {
        return validate_math_memio_strict_record_mode(argv[2], argv[3], argv[4], argv[5]);
    }
    if (argc == 5 && strcmp(argv[1], "validate-audio-oracle") == 0) {
        suite_oracle legacy;
        suite_audio_oracle audio;
        if (parse_oracle(argv[2], &legacy) != 0 || read_build_identity(argv[4], &legacy) != 0 ||
            parse_audio_oracle(argv[2], argv[3], &audio) != 0 ||
            verify_audio_build_identity(argv[4], &audio) != 0) return 1;
        puts("audio oracle: exact audio/audio2 source rows, case coverage, and build identities valid");
        return 0;
    }
    if (argc == 8 && strcmp(argv[1], "observe") == 0 && strcmp(argv[2], "audio") == 0) {
        return run_audio(argv[3], argv[4], argv[4], argv[5], argv[6], argv[7], 0, AUDIO_MAX_TICKS);
    }
    if (argc == 8 && strcmp(argv[1], "no-run") == 0 && strcmp(argv[2], "audio") == 0) {
        return run_audio(argv[3], argv[4], argv[4], argv[5], argv[6], argv[7], 1, AUDIO_MAX_TICKS);
    }
    if (argc == 8 && strcmp(argv[1], "run-audio-selected") == 0) {
        return run_audio(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], 0, AUDIO_MAX_TICKS);
    }
    if (argc == 9 && strcmp(argv[1], "run-audio-bounded") == 0) {
        uint32_t maximum_ticks;
        if (parse_decimal(argv[8], &maximum_ticks) != 0) return fail("audio time: invalid guest tick integer");
        return run_audio(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], 0, maximum_ticks);
    }
    if (argc == 9 && strcmp(argv[1], "validate-audio-record") == 0) {
        return validate_audio_record_mode(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
    }
    if (argc == 4 && strcmp(argv[1], "validate-audio-symbols") == 0) {
        uint8_t *bll = NULL;
        size_t bll_length = 0u;
        char *labels = NULL;
        size_t labels_length = 0u;
        suite_symbols symbols;
        int status;
        if (read_file(argv[3], &bll, &bll_length, 65536u) != 0 ||
            read_text(argv[2], &labels, &labels_length, 16384u) != 0) {
            free(bll);
            free(labels);
            return 1;
        }
        status = audio_symbol_layout(labels, labels_length, bll, bll_length, &symbols);
        free(bll);
        free(labels);
        if (status == 0) puts("audio symbols: linked result extent, aliases and return gate valid");
        return status;
    }
    if (argc == 3 && strcmp(argv[1], "validate-page-mode-oracle") == 0) {
        page_mode_oracle oracle;
        if (parse_page_mode_oracle(argv[2], &oracle) != 0 ||
            verify_pinned_source("page-mode/main.c", oracle.main_sha256) != 0 ||
            verify_pinned_source("page-mode/tests.s", oracle.tests_sha256) != 0 ||
            verify_pinned_source("LICENSE", oracle.notice_sha256) != 0) return 1;
        puts("page-mode oracle: six source-derived rows and source identities valid");
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "validate-page-mode-build") == 0) {
        return validate_page_mode_build_mode(argv[2], argv[3], argv[4]);
    }
    if ((argc == 8 || argc == 9) && strcmp(argv[1], "observe") == 0 &&
        strcmp(argv[2], "page-mode") == 0) {
        return run_page_mode(argv[3], argv[4], argv[5], argv[6], argv[7], 0,
                             argc == 9 && strcmp(argv[8], "control") == 0);
    }
    if (argc == 8 && strcmp(argv[1], "no-run") == 0 &&
        strcmp(argv[2], "page-mode") == 0) {
        return run_page_mode(argv[3], argv[4], argv[5], argv[6], argv[7], 1, 1);
    }
    if (argc == 7 && strcmp(argv[1], "validate-page-mode-record") == 0) {
        return validate_page_mode_record_mode(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
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
        return run_timers(argv[3], argv[3], argv[4], argv[5], NULL, 0, TIMER_MAX_TICKS, 0);
    }
    if (argc == 7 && strcmp(argv[1], "run") == 0 && strcmp(argv[2], "timers-selected") == 0) {
        return run_timers(argv[3], argv[4], argv[5], argv[6], NULL, 0, TIMER_MAX_TICKS, 0);
    }
    if (argc == 7 && strcmp(argv[1], "observe") == 0 && strcmp(argv[2], "timers") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], argv[6], 0, TIMER_MAX_TICKS, 0);
    }
    if (argc == 6 && strcmp(argv[1], "observe") == 0 && strcmp(argv[2], "timers-video") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], NULL, 0, TIMER_MAX_TICKS, 1);
    }
    if (argc == 6 && strcmp(argv[1], "no-run") == 0 && strcmp(argv[2], "cpu") == 0) {
        return run_cpu(argv[3], argv[3], argv[4], argv[5], "zero", 1, CPU_MAX_TICKS);
    }
    if (argc == 7 && strcmp(argv[1], "no-run") == 0 && strcmp(argv[2], "timers") == 0) {
        return run_timers(argv[3], argv[3], argv[4], argv[5], argv[6], 1, TIMER_MAX_TICKS, 0);
    }
    if (argc == 7 && strcmp(argv[1], "run-bounded") == 0 && strcmp(argv[2], "cpu") == 0) {
        uint32_t maximum_ticks;
        if (parse_decimal(argv[6], &maximum_ticks) != 0) return fail("time: invalid guest tick integer");
        return run_cpu(argv[3], argv[3], argv[4], argv[5], "zero", 0, maximum_ticks);
    }
    if (argc == 7 && strcmp(argv[1], "run-bounded") == 0 && strcmp(argv[2], "timers") == 0) {
        uint32_t maximum_ticks;
        if (parse_decimal(argv[6], &maximum_ticks) != 0) return fail("time: invalid guest tick integer");
        return run_timers(argv[3], argv[3], argv[4], argv[5], NULL, 0, maximum_ticks, 0);
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
    if (argc == 9 && strcmp(argv[1], "validate-timer-video-record") == 0) {
        return timer_video_record_validate(argv[2], argv[3], argv[4], argv[5],
                                           argv[6], argv[7], argv[8]);
    }
    if (argc == 2 && strcmp(argv[1], "timer-video-null-controls") == 0) {
        return timer_video_null_inputs_self_test();
    }
    if (argc == 3 && strcmp(argv[1], "timer-video-time-candidate") == 0) {
        return timer_video_time_candidate_check(argv[2]);
    }
    if (argc == 6 && strcmp(argv[1], "validate-timer-video-comparator-only") == 0) {
        return timer_video_comparator_only(argv[2], argv[3], argv[4], argv[5]);
    }
    if (argc == 6 && strcmp(argv[1], "timer-video-slicing-control") == 0) {
        uint32_t ticks;
        if (parse_decimal(argv[5], &ticks) != 0) return fail("timer video slicing control: invalid common tick");
        return timer_video_slicing_control(argv[2], argv[3], argv[4], ticks);
    }
    if (argc == 3 && strcmp(argv[1], "validate-oracle") == 0) return validate_oracle_mode(argv[2]);
    if (argc == 4 && strcmp(argv[1], "validate-pin-marker") == 0) {
        return validate_pin_marker_mode(argv[2], argv[3]);
    }
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
    fprintf(stderr,
        "usage: lynx-test-suites COMMAND ...\n"
        "  validate-page-mode-oracle ORACLE\n"
        "  validate-page-mode-build RUN_DIRECTORY BUILDDIR ORACLE\n"
        "  observe page-mode PROFILE RUN_DIRECTORY BUILDDIR ORACLE RECORD [control]\n"
        "  no-run page-mode PROFILE RUN_DIRECTORY BUILDDIR ORACLE RECORD\n"
        "  validate-page-mode-record PROFILE RUN_DIRECTORY BUILDDIR ORACLE RECORD\n"
        "  run cpu PROFILE BUILDDIR ORACLE [zero|seeded]\n"
        "  run cpu-selected MACHINE_PROFILE SELECTED_PROFILE BUILDDIR ORACLE CASE\n"
        "  run timers PROFILE BUILDDIR ORACLE\n"
        "  run timers-selected MACHINE_PROFILE SELECTED_PROFILE BUILDDIR ORACLE\n"
        "  observe timers PROFILE BUILDDIR ORACLE PENDING_DESCRIPTOR\n"
        "  no-run cpu PROFILE BUILDDIR ORACLE\n"
        "  no-run timers PROFILE BUILDDIR ORACLE PENDING_DESCRIPTOR\n"
        "  run-bounded cpu PROFILE BUILDDIR ORACLE MAX_GUEST_TICKS\n"
        "  run-bounded timers PROFILE BUILDDIR ORACLE MAX_GUEST_TICKS\n"
        "PROFILE and MACHINE_PROFILE must be lynx1 or lynx2. SELECTED_PROFILE\n"
        "must be lynx1 or lynx2; it is compared with the executed machine.\n"
        "Use separate arguments; PROFILE syntax does not include a shell pipe.\n");
    return 2;
}
