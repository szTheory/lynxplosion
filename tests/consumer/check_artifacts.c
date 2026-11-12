/* Independent parser for the consumer's bounded P3 snapshot and PCM WAV.
   This file intentionally includes no emulator or runner headers. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    PPM_LIMIT = 4096,
    FRAME_PPM_LIMIT = 50000,
    MANIFEST_LIMIT = 2048,
    WAV_DATA_BYTES = 1600,
    WAV_SIZE = 44 + WAV_DATA_BYTES,
    FIELD_COUNT = 12,
    VALUE_CAP = 160
};

typedef enum field_id {
    FIELD_ARTIFACT = 0,
    FIELD_IMAGE_FORMAT,
    FIELD_WIDTH,
    FIELD_HEIGHT,
    FIELD_GUEST_TICK,
    FIELD_STARTUP,
    FIELD_PROFILE,
    FIELD_CONTENT_SHA256,
    FIELD_PALETTE,
    FIELD_INTERPRETATION,
    FIELD_AUDIO,
    FIELD_WAV
} field_id;

typedef struct fields {
    char value[FIELD_COUNT][VALUE_CAP];
    unsigned present;
} fields;

static const char *const field_names[FIELD_COUNT] = {
    "artifact", "image_format", "width", "height", "guest_tick", "startup",
    "profile", "content_sha256", "palette", "interpretation", "audio", "wav"
};

static void set_error(char *error, size_t cap, const char *message)
{
    if (cap > 0u) {
        (void)snprintf(error, cap, "%s", message);
    }
}

static int path_join(const char *directory, const char *name, char *out, size_t cap)
{
    int length = snprintf(out, cap, "%s/%s", directory, name);
    return length < 0 || (size_t)length >= cap ? 1 : 0;
}

static int read_bounded(const char *path, size_t limit, uint8_t **out,
                        size_t *length, char *error, size_t error_cap)
{
    FILE *file = fopen(path, "rb");
    uint8_t *buffer;
    size_t got;
    int extra;
    int failed;
    if (file == NULL) {
        set_error(error, error_cap, "cannot open artifact; provide a complete consumer output directory");
        return 1;
    }
    buffer = malloc(limit == 0u ? 1u : limit);
    if (buffer == NULL) {
        (void)fclose(file);
        set_error(error, error_cap, "cannot allocate bounded artifact buffer; reduce the input size");
        return 1;
    }
    got = fread(buffer, 1u, limit, file);
    extra = fgetc(file);
    failed = ferror(file) != 0;
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed != 0 || extra != EOF) {
        free(buffer);
        set_error(error, error_cap, "artifact exceeds its size bound or could not be read; regenerate the diagnostic output");
        return 1;
    }
    *out = buffer;
    *length = got;
    return 0;
}

static int lookup_field(const char *key, size_t key_len, field_id *id)
{
    unsigned index;
    for (index = 0u; index < FIELD_COUNT; index++) {
        if (strlen(field_names[index]) == key_len &&
            memcmp(field_names[index], key, key_len) == 0) {
            *id = (field_id)index;
            return 0;
        }
    }
    return 1;
}

static int parse_field_lines(const uint8_t *text, size_t length, int comments,
                             fields *out, size_t *end_offset, char *error,
                             size_t error_cap)
{
    size_t offset = 0u;
    memset(out, 0, sizeof *out);
    while (offset < length) {
        size_t line_len = 0u;
        size_t start;
        size_t delimiter = SIZE_MAX;
        size_t cursor;
        field_id id;
        unsigned bit;
        while (offset + line_len < length && text[offset + line_len] != (uint8_t)'\n') {
            line_len++;
        }
        if (line_len == 0u || line_len > 255u || offset + line_len == length ||
            memchr(text + offset, '\r', line_len) != NULL ||
            memchr(text + offset, '\0', line_len) != NULL) {
            set_error(error, error_cap, "metadata line is empty, overlong, or missing LF; regenerate the artifact metadata");
            return 1;
        }
        if (comments != 0) {
            if (text[offset] != (uint8_t)'#') {
                *end_offset = offset;
                return 0;
            }
            if (line_len < 4u || text[offset + 1u] != (uint8_t)' ') {
                set_error(error, error_cap, "PPM metadata comment is malformed; regenerate the diagnostic P3 header");
                return 1;
            }
            start = offset + 2u;
        } else {
            start = offset;
        }
        for (cursor = start; cursor + 2u < offset + line_len; cursor++) {
            if (text[cursor] == (uint8_t)' ' && text[cursor + 1u] == (uint8_t)'=' &&
                text[cursor + 2u] == (uint8_t)' ') {
                delimiter = cursor;
                break;
            }
        }
        if (delimiter == SIZE_MAX || delimiter == start ||
            lookup_field((const char *)(text + start), delimiter - start, &id) != 0) {
            set_error(error, error_cap, "metadata contains an unknown field or lacks ' = '; regenerate from the diagnostic consumer");
            return 1;
        }
        bit = 1u << (unsigned)id;
        if ((out->present & bit) != 0u) {
            set_error(error, error_cap, "metadata repeats a field; regenerate a single complete manifest");
            return 1;
        }
        {
            size_t value_start = delimiter + 3u;
            size_t value_len = offset + line_len - value_start;
            if (value_len == 0u || value_len >= VALUE_CAP ||
                memchr(text + value_start, '\t', value_len) != NULL) {
                set_error(error, error_cap, "metadata value is empty or exceeds its bound; regenerate the artifact");
                return 1;
            }
            memcpy(out->value[id], text + value_start, value_len);
            out->value[id][value_len] = '\0';
        }
        out->present |= bit;
        offset += line_len + 1u;
    }
    *end_offset = offset;
    return 0;
}

static int expect_field(const fields *parsed, field_id id, const char *expected,
                        char *error, size_t error_cap)
{
    unsigned bit = 1u << (unsigned)id;
    if ((parsed->present & bit) == 0u || strcmp(parsed->value[id], expected) != 0) {
        (void)snprintf(error, error_cap,
                       "metadata field %s is missing or incorrect; regenerate a Phase 3 diagnostic snapshot",
                       field_names[id]);
        return 1;
    }
    return 0;
}

static int valid_sha256(const char *value)
{
    size_t index;
    if (strlen(value) != 64u) {
        return 0;
    }
    for (index = 0u; index < 64u; index++) {
        unsigned char ch = (unsigned char)value[index];
        if (!((ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
              (ch >= (unsigned char)'a' && ch <= (unsigned char)'f'))) {
            return 0;
        }
    }
    return 1;
}

static int valid_profile(const char *value)
{
    return strcmp(value, "none") == 0 || strcmp(value, "lynx1") == 0 ||
           strcmp(value, "lynx2") == 0;
}

static int validate_ppm(const uint8_t *ppm, size_t length, fields *image_fields,
                        char *error, size_t error_cap)
{
    size_t offset = 0u;
    size_t data_offset = 0u;
    size_t samples = 0u;
    unsigned y;
    if (length == 0u || memchr(ppm, '\0', length) != NULL) {
        set_error(error, error_cap, "snapshot.ppm is empty or contains NUL; regenerate the P3 snapshot");
        return 1;
    }
    if (length < 3u || memcmp(ppm, "P3\n", 3u) != 0) {
        set_error(error, error_cap, "snapshot.ppm is not plain P3; regenerate the unscaled diagnostic snapshot");
        return 1;
    }
    offset = 3u;
    if (parse_field_lines(ppm + offset, length - offset, 1, image_fields,
                          &data_offset, error, error_cap) != 0) {
        return 1;
    }
    offset += data_offset;
    if (offset > length || length - offset < 1u) {
        set_error(error, error_cap, "snapshot.ppm header is truncated; regenerate the 8x8 P3 snapshot");
        return 1;
    }
    {
        static const char dimension_line[] = "8 8\n255\n";
        size_t n = sizeof dimension_line - 1u;
        if (length - offset < n || memcmp(ppm + offset, dimension_line, n) != 0) {
            set_error(error, error_cap, "snapshot.ppm dimensions or max value are not 8x8/255; regenerate the original diagnostic snapshot");
            return 1;
        }
        offset += n;
    }
    for (y = 0u; y < 8u; y++) {
        unsigned x;
        for (x = 0u; x < 8u; x++) {
            unsigned component;
            unsigned pen = (x + 2u * y) & 15u;
            unsigned expected[3] = {
                pen * 17u,
                (15u - pen) * 17u,
                ((pen * 3u) & 15u) * 17u
            };
            for (component = 0u; component < 3u; component++) {
                size_t token_start;
                unsigned value = 0u;
                while (offset < length && isspace((unsigned char)ppm[offset]) != 0) {
                    offset++;
                }
                token_start = offset;
                while (offset < length && ppm[offset] >= (uint8_t)'0' &&
                       ppm[offset] <= (uint8_t)'9') {
                    unsigned digit = (unsigned)(ppm[offset] - (uint8_t)'0');
                    if (value > 25u || (value == 25u && digit > 5u)) {
                        set_error(error, error_cap, "snapshot.ppm sample exceeds 255; regenerate the diagnostic palette output");
                        return 1;
                    }
                    value = value * 10u + digit;
                    offset++;
                }
                if (offset == token_start || value != expected[component]) {
                    set_error(error, error_cap, "snapshot.ppm pixel differs from diagnostic-palette-v1; regenerate from the validated guest pixels");
                    return 1;
                }
                samples++;
                if (samples > 192u) {
                    set_error(error, error_cap, "snapshot.ppm has more than 192 color samples; regenerate the exact 8x8 output");
                    return 1;
                }
            }
        }
    }
    while (offset < length && isspace((unsigned char)ppm[offset]) != 0) {
        offset++;
    }
    if (samples != 192u || offset != length) {
        set_error(error, error_cap, "snapshot.ppm sample count or trailing data is invalid; regenerate the exact 8x8 output");
        return 1;
    }
    (void)data_offset;
    return 0;
}

static uint16_t load_u16le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t load_u32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int validate_wav(const uint8_t *wav, size_t length, char *error,
                        size_t error_cap)
{
    if (length != WAV_SIZE || memcmp(wav, "RIFF", 4u) != 0 ||
        memcmp(wav + 8u, "WAVEfmt ", 8u) != 0 ||
        memcmp(wav + 36u, "data", 4u) != 0) {
        set_error(error, error_cap, "silence.wav is not the exact bounded RIFF/WAVE layout; regenerate the placeholder output");
        return 1;
    }
    if (load_u32le(wav + 4u) != (uint32_t)(WAV_SIZE - 8) ||
        load_u32le(wav + 16u) != 16u || load_u16le(wav + 20u) != 1u ||
        load_u16le(wav + 22u) != 1u || load_u32le(wav + 24u) != 8000u ||
        load_u32le(wav + 28u) != 16000u || load_u16le(wav + 32u) != 2u ||
        load_u16le(wav + 34u) != 16u || load_u32le(wav + 40u) != WAV_DATA_BYTES) {
        set_error(error, error_cap, "silence.wav chunk sizes or PCM format are invalid; regenerate mono 8000Hz signed16le placeholder silence");
        return 1;
    }
    if (memchr(wav + 44u, 1, WAV_DATA_BYTES) != NULL) {
        set_error(error, error_cap, "silence.wav contains non-zero placeholder samples; regenerate the 800-sample silence block");
        return 1;
    }
    {
        size_t index;
        for (index = 44u; index < length; index++) {
            if (wav[index] != 0u) {
                set_error(error, error_cap, "silence.wav contains non-zero placeholder samples; regenerate the 800-sample silence block");
                return 1;
            }
        }
    }
    return 0;
}

static int validate_directory(const char *directory, char *error, size_t error_cap)
{
    uint8_t *ppm = NULL;
    uint8_t *manifest = NULL;
    uint8_t *wav = NULL;
    size_t ppm_len = 0u;
    size_t manifest_len = 0u;
    size_t wav_len = 0u;
    size_t ppm_data_offset = 0u;
    fields ppm_fields;
    fields manifest_fields;
    char path[4096];
    int result = 1;
    const unsigned required_ppm = (1u << FIELD_ARTIFACT) | (1u << FIELD_GUEST_TICK) |
        (1u << FIELD_STARTUP) | (1u << FIELD_PROFILE) | (1u << FIELD_CONTENT_SHA256) |
        (1u << FIELD_PALETTE) | (1u << FIELD_INTERPRETATION);
    const unsigned required_manifest = (1u << FIELD_COUNT) - 1u;

    if (directory == NULL || strlen(directory) >= sizeof path - sizeof("/snapshot.manifest")) {
        set_error(error, error_cap, "output directory path exceeds its bound; choose a shorter destination path");
        goto done;
    }
    if (path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
        read_bounded(path, PPM_LIMIT, &ppm, &ppm_len, error, error_cap) != 0) {
        goto done;
    }
    if (path_join(directory, "snapshot.manifest", path, sizeof path) != 0 ||
        read_bounded(path, MANIFEST_LIMIT, &manifest, &manifest_len, error, error_cap) != 0) {
        goto done;
    }
    if (path_join(directory, "silence.wav", path, sizeof path) != 0 ||
        read_bounded(path, WAV_SIZE, &wav, &wav_len, error, error_cap) != 0) {
        goto done;
    }
    if (validate_ppm(ppm, ppm_len, &ppm_fields, error, error_cap) != 0 ||
        parse_field_lines(manifest, manifest_len, 0, &manifest_fields,
                          &ppm_data_offset, error, error_cap) != 0) {
        goto done;
    }
    if (ppm_fields.present != required_ppm) {
        set_error(error, error_cap, "snapshot PPM comments have missing or unexpected fields; regenerate the complete diagnostic metadata");
        goto done;
    }
    if (manifest_fields.present != required_manifest) {
        unsigned index;
        for (index = 0u; index < FIELD_COUNT; index++) {
            if ((manifest_fields.present & (1u << index)) == 0u) {
                (void)snprintf(error, error_cap,
                    "metadata field %s is missing; regenerate the complete diagnostic manifest",
                    field_names[index]);
                break;
            }
        }
        if (manifest_fields.present == required_manifest) {
            set_error(error, error_cap, "snapshot manifest contains unexpected fields; regenerate the complete diagnostic manifest");
        }
        goto done;
    }
    if (expect_field(&manifest_fields, FIELD_ARTIFACT, "SNAPSHOT", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_IMAGE_FORMAT, "P3", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_WIDTH, "8", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_HEIGHT, "8", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_GUEST_TICK, "5000", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_STARTUP, "A", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_PALETTE, "diagnostic-palette-v1", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_INTERPRETATION,
                     "diagnostic-owned pixels; no canonical Lynx video claim", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_AUDIO, "placeholder silence pending Phase6", error, error_cap) != 0 ||
        expect_field(&manifest_fields, FIELD_WAV, "PCM mono 8000Hz signed16le; 800 zero samples", error, error_cap) != 0) {
        goto done;
    }
    if (!valid_profile(manifest_fields.value[FIELD_PROFILE]) ||
        !valid_sha256(manifest_fields.value[FIELD_CONTENT_SHA256]) ||
        strcmp(ppm_fields.value[FIELD_ARTIFACT], manifest_fields.value[FIELD_ARTIFACT]) != 0 ||
        strcmp(ppm_fields.value[FIELD_GUEST_TICK], manifest_fields.value[FIELD_GUEST_TICK]) != 0 ||
        strcmp(ppm_fields.value[FIELD_STARTUP], manifest_fields.value[FIELD_STARTUP]) != 0 ||
        strcmp(ppm_fields.value[FIELD_PROFILE], manifest_fields.value[FIELD_PROFILE]) != 0 ||
        strcmp(ppm_fields.value[FIELD_CONTENT_SHA256], manifest_fields.value[FIELD_CONTENT_SHA256]) != 0 ||
        strcmp(ppm_fields.value[FIELD_PALETTE], manifest_fields.value[FIELD_PALETTE]) != 0 ||
        strcmp(ppm_fields.value[FIELD_INTERPRETATION], manifest_fields.value[FIELD_INTERPRETATION]) != 0) {
        set_error(error, error_cap, "snapshot comment and manifest identity disagree; regenerate both from one accepted BLL run");
        goto done;
    }
    if (expect_field(&ppm_fields, FIELD_ARTIFACT, "SNAPSHOT", error, error_cap) != 0 ||
        expect_field(&ppm_fields, FIELD_GUEST_TICK, "5000", error, error_cap) != 0 ||
        expect_field(&ppm_fields, FIELD_STARTUP, "A", error, error_cap) != 0 ||
        expect_field(&ppm_fields, FIELD_PALETTE, "diagnostic-palette-v1", error, error_cap) != 0 ||
        expect_field(&ppm_fields, FIELD_INTERPRETATION,
                     "diagnostic-owned pixels; no canonical Lynx video claim", error, error_cap) != 0) {
        goto done;
    }
    if (validate_wav(wav, wav_len, error, error_cap) != 0) {
        goto done;
    }
    result = 0;
done:
    free(ppm);
    free(manifest);
    free(wav);
    (void)ppm_data_offset;
    return result;
}

static int has_exact_line(const uint8_t *bytes, size_t length, const char *line)
{
    const size_t line_len = strlen(line);
    size_t offset = 0u;
    while (offset < length) {
        size_t end = offset;
        while (end < length && bytes[end] != (uint8_t)'\n') end++;
        if (end - offset == line_len && memcmp(bytes + offset, line, line_len) == 0) return 1;
        offset = end < length ? end + 1u : end;
    }
    return 0;
}

static int decimal_line_value(const uint8_t *bytes, size_t length,
                              const char *prefix, uint32_t *value)
{
    const size_t prefix_len = strlen(prefix);
    size_t offset = 0u;
    while (offset < length) {
        size_t end = offset, at;
        uint32_t parsed = 0u;
        while (end < length && bytes[end] != (uint8_t)'\n') end++;
        if (end - offset >= prefix_len && memcmp(bytes + offset, prefix, prefix_len) == 0) {
            at = offset + prefix_len;
            if (at == end) return 1;
            for (; at < end; at++) {
                const uint8_t byte = bytes[at];
                if (byte < (uint8_t)'0' || byte > (uint8_t)'9' ||
                    parsed > (UINT32_MAX - (uint32_t)(byte - (uint8_t)'0')) / 10u) return 1;
                parsed = parsed * 10u + (uint32_t)(byte - (uint8_t)'0');
            }
            *value = parsed;
            return 0;
        }
        offset = end < length ? end + 1u : end;
    }
    return 1;
}

static int line_has_fragments(const uint8_t *bytes, size_t length,
                              const char *prefix, const char *fragment)
{
    size_t offset = 0u;
    const size_t prefix_len = strlen(prefix), fragment_len = strlen(fragment);
    while (offset < length) {
        size_t end = offset;
        while (end < length && bytes[end] != (uint8_t)'\n') end++;
        if (end - offset >= prefix_len && memcmp(bytes + offset, prefix, prefix_len) == 0) {
            for (size_t at = offset; at + fragment_len <= end; at++)
                if (memcmp(bytes + at, fragment, fragment_len) == 0) return 1;
        }
        offset = end < length ? end + 1u : end;
    }
    return 0;
}

static uint32_t expected_joystick(uint32_t mask, unsigned lefthand)
{
    uint32_t value = (mask & 16u ? 1u : 0u) | (mask & 32u ? 2u : 0u) |
                     (mask & 128u ? 4u : 0u) | (mask & 64u ? 8u : 0u);
    if (lefthand != 0u) {
        value |= (mask & 1u ? 0x80u : 0u) | (mask & 2u ? 0x40u : 0u) |
                 (mask & 4u ? 0x20u : 0u) | (mask & 8u ? 0x10u : 0u);
    } else {
        value |= (mask & 1u ? 0x40u : 0u) | (mask & 2u ? 0x80u : 0u) |
                 (mask & 4u ? 0x10u : 0u) | (mask & 8u ? 0x20u : 0u);
    }
    return value;
}

static int has_sha_field(const uint8_t *bytes, size_t length, const char *name)
{
    const size_t prefix_len = strlen(name);
    size_t offset = 0u;
    while (offset < length) {
        size_t end = offset;
        while (end < length && bytes[end] != (uint8_t)'\n') end++;
        if (end - offset == prefix_len + 64u && memcmp(bytes + offset, name, prefix_len) == 0) {
            char value[65];
            memcpy(value, bytes + offset + prefix_len, 64u);
            value[64] = '\0';
            return valid_sha256(value);
        }
        offset = end < length ? end + 1u : end;
    }
    return 0;
}

static int validate_frame_ppm(const char *path, unsigned frame_index,
                              uint64_t start_tick, uint64_t end_tick,
                              uint32_t input_mask, uint32_t sample_a,
                              char *error, size_t error_cap)
{
    uint8_t *bytes = NULL;
    size_t length = 0u, marker_at = SIZE_MAX;
    const char marker[] = "\n160 102\n255\n";
    const uint8_t released[3] = {0u,255u,0u};
    const uint8_t pressed[3] = {255u,0u,0u};
    const uint8_t black[3] = {0u,0u,0u};
    const uint8_t *expected = frame_index == 0u ? released : pressed;
    char line[128];
    if (read_bounded(path, FRAME_PPM_LIMIT, &bytes, &length, error, error_cap) != 0) return 1;
    if (length < 3u || memcmp(bytes, "P6\n", 3u) != 0) goto invalid;
    for (size_t i = 0u; i + sizeof marker - 1u <= length; i++) {
        if (memcmp(bytes + i, marker, sizeof marker - 1u) == 0) { marker_at = i; break; }
    }
    if (marker_at == SIZE_MAX || length - (marker_at + sizeof marker - 1u) != 160u * 102u * 3u) goto invalid;
    if (!has_exact_line(bytes, marker_at, "# artifact = FRAME")) goto invalid;
    (void)snprintf(line, sizeof line, "# sequence = %u", frame_index == 0u ? 1u : 2u);
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    (void)snprintf(line, sizeof line, "# frame_start_tick = %" PRIu64, start_tick);
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    (void)snprintf(line, sizeof line, "# frame_end_tick = %" PRIu64, end_tick);
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    (void)snprintf(line, sizeof line, "# rows = 102");
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    (void)snprintf(line, sizeof line, "# input_mask = %u", input_mask);
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    (void)snprintf(line, sizeof line, "# sample_a = %u", sample_a);
    if (!has_exact_line(bytes, marker_at, line)) goto invalid;
    {
        const size_t raster = marker_at + sizeof marker - 1u;
        for (size_t pixel = 0u; pixel < 160u * 102u; pixel++) {
            const uint8_t *wanted = pixel < 2u ? expected : black;
            if (memcmp(bytes + raster + pixel * 3u, wanted, 3u) != 0) goto invalid;
        }
    }
    free(bytes);
    return 0;
invalid:
    free(bytes);
    set_error(error, error_cap, "raw PPM geometry, identity or independently expected released/A pen pixels are invalid");
    return 1;
}

static int validate_frame_directory(const char *directory, char *error, size_t error_cap)
{
    uint8_t *manifest = NULL;
    size_t manifest_len = 0u;
    char paths[3][4096];
    const char *names[3] = {"released.ppm", "pressed.ppm", "frame-trace.manifest"};
    if (directory == NULL || strlen(directory) >= sizeof paths[0] - sizeof("/frame-trace.manifest")) {
        set_error(error, error_cap, "frame output path exceeds its bound");
        return 1;
    }
    for (size_t i = 0u; i < 3u; i++) {
        if (path_join(directory, names[i], paths[i], sizeof paths[i]) != 0) return 1;
    }
    if (validate_frame_ppm(paths[0], 0u, UINT64_C(54272), UINT64_C(108544), 0u, 0u,
                           error, error_cap) != 0 ||
        validate_frame_ppm(paths[1], 1u, UINT64_C(108544), UINT64_C(162816), 16u, 1u,
                           error, error_cap) != 0 ||
        read_bounded(paths[2], 4096u, &manifest, &manifest_len, error, error_cap) != 0) goto failed;
#define EXPECT_LINE(text) do { if (!has_exact_line(manifest, manifest_len, (text))) goto invalid; } while (0)
    EXPECT_LINE("artifact = FRAME_TRACE");
    EXPECT_LINE("profile = none");
    EXPECT_LINE("startup = A");
    EXPECT_LINE("timing_policy = video-policy1-unverified");
    EXPECT_LINE("event_count = 1");
    EXPECT_LINE("event_tick = 109321");
    EXPECT_LINE("event_mask = 16");
    EXPECT_LINE("sync_rows = 0");
    EXPECT_LINE("sync_end_tick = 54272");
    EXPECT_LINE("released_sequence = 1");
    EXPECT_LINE("released_start_tick = 54272");
    EXPECT_LINE("released_end_tick = 108544");
    EXPECT_LINE("released_rows = 102");
    EXPECT_LINE("released_sample_a = 0");
    EXPECT_LINE("pressed_sequence = 2");
    EXPECT_LINE("pressed_start_tick = 108544");
    EXPECT_LINE("pressed_end_tick = 162816");
    EXPECT_LINE("pressed_rows = 102");
    EXPECT_LINE("pressed_sample_a = 1");
    EXPECT_LINE("guest_progress = 2");
    EXPECT_LINE("palette_policy = per-line GREEN then BLUERED raw nibbles; intra-line color accuracy unverified");
    EXPECT_LINE("raw_pen_order = high nibble first; no host rotation");
    if (!has_sha_field(manifest, manifest_len, "content_sha256 = ") ||
        !has_sha_field(manifest, manifest_len, "input_sha256 = ")) goto invalid;
#undef EXPECT_LINE
    free(manifest);
    return 0;
invalid:
    set_error(error, error_cap, "frame trace manifest does not match its independent guest/input expectations");
failed:
    free(manifest);
    return 1;
}

static int validate_replay_directory(const char *directory, const char *scenario,
                                     const char *expected_input_sha,
                                     const char *expected_handedness,
                                     const char *expected_profile,
                                     char *error, size_t error_cap,
                                     int corrupt_transition_color)
{
    uint8_t *manifest = NULL;
    size_t manifest_len = 0u;
    size_t expected_frames;
    uint64_t starts[32], ends[32];
    uint32_t masks[32] = {0u};
    uint32_t samples[32] = {0u};
    static const uint32_t pattern_bits[9] = {16u,32u,1u,2u,4u,8u,64u,128u,256u};
    const int replay_all_inputs = strcmp(scenario, "frame_replay") == 0 ||
        strcmp(scenario, "frame_replay_sensitivity") == 0;
    unsigned handedness = 1u;
    char path[4096], line[512];
    for (size_t i = 0u; i < 32u; i++) {
        starts[i] = UINT64_C(54272) + (uint64_t)i * UINT64_C(54272);
        ends[i] = UINT64_C(108544) + (uint64_t)i * UINT64_C(54272);
    }
    if (strcmp(scenario, "replay_edges") == 0) {
        expected_frames = 4u;
        masks[2] = 16u; masks[3] = 16u; samples[2] = 1u; samples[3] = 1u;
    } else if (strcmp(scenario, "replay_restore") == 0) {
        expected_frames = 3u; masks[1] = 16u; masks[2] = 16u; samples[1] = 1u; samples[2] = 1u;
    } else if (strcmp(scenario, "frame_tracer") == 0) {
        expected_frames = 2u; masks[1] = 16u; samples[1] = 1u;
    } else if (replay_all_inputs != 0) {
        static const uint32_t scenario_masks[23] = {
            0u,16u,16u,32u,32u,1u,1u,2u,2u,4u,4u,8u,8u,64u,64u,128u,
            128u,256u,256u,511u,511u,0u,0u
        };
        expected_frames = 23u;
        memcpy(masks, scenario_masks, sizeof scenario_masks);
        if (strcmp(scenario, "frame_replay_sensitivity") == 0) {
            masks[21] = 16u;
            masks[22] = 16u;
        }
        for (size_t i = 0u; i < expected_frames; i++) samples[i] = (masks[i] & 16u) != 0u ? 1u : 0u;
    } else if (strcmp(scenario, "empty") == 0) {
        expected_frames = 1u;
    } else {
        set_error(error, error_cap, "unknown generic replay scenario");
        return 1;
    }
    if (strcmp(expected_handedness, "0") == 0) handedness = 0u;
    else if (strcmp(expected_handedness, "1") != 0) goto invalid;
    if (strcmp(expected_profile, "none") != 0 && strcmp(expected_profile, "lynx1") != 0 &&
        strcmp(expected_profile, "lynx2") != 0) goto invalid;
    if (path_join(directory, "frame-trace.manifest", path, sizeof path) != 0 ||
        read_bounded(path, 8192u, &manifest, &manifest_len, error, error_cap) != 0) return 1;
#define EXPECT_REPLAY(text) do { if (!has_exact_line(manifest, manifest_len, (text))) goto invalid; } while (0)
    EXPECT_REPLAY("artifact = FRAME_TRACE");
    (void)snprintf(line, sizeof line, "profile = %s", expected_profile);
    EXPECT_REPLAY(line);
    EXPECT_REPLAY("startup = A");
    EXPECT_REPLAY("timing_policy = video-policy1-unverified");
    EXPECT_REPLAY("raw_pen_order = high nibble first; no host rotation");
    if (expected_input_sha == NULL || !valid_sha256(expected_input_sha) ||
        !has_sha_field(manifest, manifest_len, "content_sha256 = ")) goto invalid;
    (void)snprintf(line, sizeof line, "input_sha256 = %s", expected_input_sha);
    EXPECT_REPLAY(line);
    (void)snprintf(line, sizeof line, "frame_count = %zu", expected_frames);
    EXPECT_REPLAY(line);
    if (strcmp(scenario, "replay_edges") == 0) {
        EXPECT_REPLAY("event_count = 5");
        EXPECT_REPLAY("finish_tick = 271361");
    } else if (strcmp(scenario, "replay_restore") == 0) {
        EXPECT_REPLAY("event_count = 1");
        EXPECT_REPLAY("finish_tick = 217089");
    } else if (strcmp(scenario, "frame_tracer") == 0) {
        EXPECT_REPLAY("event_count = 1");
        EXPECT_REPLAY("finish_tick = 163594");
    } else if (replay_all_inputs != 0) {
        EXPECT_REPLAY("event_count = 12");
        EXPECT_REPLAY("finish_tick = 1302530");
        EXPECT_REPLAY("fixture_schema = LXI5-v3");
        (void)snprintf(line, sizeof line, "handedness = %u", handedness);
        EXPECT_REPLAY(line);
        EXPECT_REPLAY("repeatability = test harness compares raw and rendered digests across fresh and interleaved runs");
    } else {
        EXPECT_REPLAY("event_count = 0");
        EXPECT_REPLAY("finish_tick = 108545");
    }
    for (size_t i = 0u; i < expected_frames; i++) {
        uint8_t *raw = NULL, *ppm = NULL;
        size_t raw_len = 0u, ppm_len = 0u, header_end = SIZE_MAX;
        uint32_t progress = 0u;
        char raw_name[40], ppm_name[40];
        const char ppm_marker[] = "\n160 102\n255\n";
        const uint8_t green[3] = {0u,255u,0u}, red[3] = {255u,0u,0u}, black[3] = {0u,0u,0u};
        uint64_t previous_tick = 0u;
        (void)snprintf(raw_name, sizeof raw_name, "frame-%02zu.raw", i);
        (void)snprintf(ppm_name, sizeof ppm_name, "frame-%02zu.ppm", i);
        if (path_join(directory, raw_name, path, sizeof path) != 0 ||
            read_bounded(path, 102u * 120u, &raw, &raw_len, error, error_cap) != 0 ||
            raw_len != 102u * 120u) { free(raw); goto invalid; }
        if (path_join(directory, ppm_name, path, sizeof path) != 0 ||
            read_bounded(path, FRAME_PPM_LIMIT, &ppm, &ppm_len, error, error_cap) != 0 ||
            ppm_len < 3u || memcmp(ppm, "P6\n", 3u) != 0) { free(raw); free(ppm); goto invalid; }
        for (size_t pos = 0u; pos + sizeof ppm_marker - 1u <= ppm_len; pos++)
            if (memcmp(ppm + pos, ppm_marker, sizeof ppm_marker - 1u) == 0) { header_end = pos; break; }
        if (header_end == SIZE_MAX || ppm_len - (header_end + sizeof ppm_marker - 1u) != 160u * 102u * 3u) {
            free(raw); free(ppm); goto invalid;
        }
        if (corrupt_transition_color != 0 && replay_all_inputs != 0 && i == 1u) {
            const size_t raster_start = header_end + sizeof ppm_marker - 1u;
            ppm[raster_start] ^= 0xFFu;
        }
        (void)snprintf(line, sizeof line, "# sequence = %zu", i + 1u);
        if (!has_exact_line(ppm, header_end, "# artifact = FRAME") || !has_exact_line(ppm, header_end, line)) {
            free(raw); free(ppm); goto invalid;
        }
        (void)snprintf(line, sizeof line, "# frame_start_tick = %" PRIu64, starts[i]);
        if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto invalid; }
        (void)snprintf(line, sizeof line, "# frame_end_tick = %" PRIu64, ends[i]);
        if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto invalid; }
        (void)snprintf(line, sizeof line, "# input_mask_at_end = %u", masks[i]);
        if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto invalid; }
        (void)snprintf(line, sizeof line, "# sample_a = %u", samples[i]);
        if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto invalid; }
        if (replay_all_inputs != 0) {
            const uint32_t joy = expected_joystick(masks[i], handedness);
            (void)snprintf(line, sizeof line, "# sample_joystick = %u", joy);
            if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto missing_guest_samples; }
            (void)snprintf(line, sizeof line, "# sample_switches = %u", masks[i] & 256u ? 1u : 0u);
            if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto missing_guest_samples; }
            if (decimal_line_value(ppm, header_end, "# poll_progress = ", &progress) != 0 ||
                progress < 2u || progress > 63u ||
                !has_exact_line(ppm, header_end, "# error = 0") ||
                !has_exact_line(ppm, header_end, "# done = 1")) { free(raw); free(ppm); goto invalid; }
            (void)snprintf(line, sizeof line, "# handedness = %u", handedness);
            if (!has_exact_line(ppm, header_end, line)) { free(raw); free(ppm); goto invalid; }
        }
        for (size_t row = 0u; row < 102u; row++) {
            const uint8_t *record = raw + row * 120u;
            const uint8_t *pixels = ppm + header_end + sizeof ppm_marker - 1u + row * 160u * 3u;
            uint64_t tick = 0u;
            for (size_t byte = 0u; byte < 8u; byte++) tick |= (uint64_t)record[112u + byte] << (8u * byte);
            if ((replay_all_inputs == 0 && record[0] != (row == 0u ? (samples[i] != 0u ? 0x22u : 0x11u) : 0u)) ||
                record[80u + 1u] != 0x0Fu || record[80u + 17u] != 0u ||
                record[80u + 2u] != 0u || record[80u + 18u] != 0x0Fu ||
                tick < starts[i] || tick >= ends[i] || (row != 0u && tick <= previous_tick)) {
                free(raw); free(ppm); goto invalid;
            }
            if (replay_all_inputs != 0) {
                for (size_t button = 0u; button < 9u; button++) {
                    const uint8_t expected_byte = (masks[i] & pattern_bits[button]) != 0u ? 0x22u : 0x11u;
                    if (row == 0u && (i % 2u) != 0u) {
                        if (record[button] != 0x11u && record[button] != 0x22u) { free(raw); free(ppm); goto invalid; }
                    } else if (record[button] != (row == 0u ? expected_byte : 0u)) {
                        free(raw); free(ppm); goto invalid;
                    }
                }
            } else {
                for (size_t button = 1u; button < 9u; button++) {
                    if (record[button] != (row == 0u ? 0x11u : 0u)) { free(raw); free(ppm); goto invalid; }
                }
            }
            previous_tick = tick;
            for (size_t pixel = 0u; pixel < 160u; pixel++) {
                const uint8_t *wanted = black;
                if (replay_all_inputs != 0 && row == 0u && pixel < 18u) {
                    const size_t button = pixel / 2u;
                    wanted = record[button] == 0x22u ? red : green;
                } else if (replay_all_inputs == 0 && row == 0u && pixel < 18u) {
                    wanted = pixel < 2u && samples[i] != 0u ? red : green;
                }
                if (memcmp(pixels + pixel * 3u, wanted, 3u) != 0) { free(raw); free(ppm); goto invalid; }
            }
        }
        if (replay_all_inputs != 0) {
            const uint32_t joy = expected_joystick(masks[i], handedness), switches = masks[i] & 256u ? 1u : 0u;
            (void)snprintf(line, sizeof line,
                "frame[%zu] = sequence:%zu,start:%" PRIu64 ",end:%" PRIu64 ",rows:102,flags:0,mask:%u,",
                i, i + 1u, starts[i], ends[i], masks[i]);
            char fragment[192];
            (void)snprintf(fragment, sizeof fragment,
                "joy:%u,switches:%u,progress:%u,error:0,done:1,handedness:%u,",
                joy, switches, progress, handedness);
            if (!line_has_fragments(manifest, manifest_len, line, fragment)) { free(raw); free(ppm); goto invalid; }
        }
        free(raw); free(ppm);
    }
#undef EXPECT_REPLAY
    free(manifest);
    return 0;
invalid:
    free(manifest);
    set_error(error, error_cap, "replay transcript, exact input digest, raw pen/palette/ticks or representative rendered pixels are invalid");
    return 1;
missing_guest_samples:
    free(manifest);
    set_error(error, error_cap, "frame artifact omits the guest's joystick or switches sample for the recorded input mask");
    return 1;
}

static int write_owned_file(const char *path, const void *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    int failed;
    if (file == NULL) {
        return 1;
    }
    failed = fwrite(bytes, 1u, length, file) != length;
    if (fclose(file) != 0) {
        failed = 1;
    }
    return failed;
}

static int replace_file_text(const char *path, size_t limit, const char *before,
                             const char *after)
{
    uint8_t bytes[PPM_LIMIT];
    uint8_t updated[PPM_LIMIT];
    uint8_t *read_bytes = NULL;
    size_t length = 0u;
    size_t before_len = strlen(before);
    size_t after_len = strlen(after);
    size_t index;
    size_t found = SIZE_MAX;
    char error[256] = {0};
    if (limit > sizeof bytes || read_bounded(path, limit, &read_bytes, &length,
                                             error, sizeof error) != 0) {
        free(read_bytes);
        return 1;
    }
    for (index = 0u; index + before_len <= length; index++) {
        if (memcmp(read_bytes + index, before, before_len) == 0) {
            found = index;
            break;
        }
    }
    if (found == SIZE_MAX || after_len > sizeof updated - (length - before_len)) {
        free(read_bytes);
        return 1;
    }
    memcpy(updated, read_bytes, found);
    memcpy(updated + found, after, after_len);
    memcpy(updated + found + after_len, read_bytes + found + before_len,
           length - found - before_len);
    length = length - before_len + after_len;
    free(read_bytes);
    return write_owned_file(path, updated, length);
}

static int set_file_byte(const char *path, long offset, int value)
{
    FILE *file = fopen(path, "r+b");
    int failed = 0;
    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fputc(value, file) == EOF) {
        failed = 1;
    }
    if (fclose(file) != 0) {
        failed = 1;
    }
    return failed;
}

static int build_sample_ppm(char *out, size_t cap, int corrupt_pixel)
{
    size_t used = 0u;
    unsigned y;
    int n = snprintf(out, cap,
        "P3\n"
        "# artifact = SNAPSHOT\n"
        "# guest_tick = 5000\n"
        "# startup = A\n"
        "# profile = none\n"
        "# content_sha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "# palette = diagnostic-palette-v1\n"
        "# interpretation = diagnostic-owned pixels; no canonical Lynx video claim\n"
        "8 8\n255\n");
    if (n < 0 || (size_t)n >= cap) {
        return 1;
    }
    used = (size_t)n;
    for (y = 0u; y < 8u; y++) {
        unsigned x;
        for (x = 0u; x < 8u; x++) {
            unsigned pen = (x + 2u * y) & 15u;
            unsigned red = pen * 17u;
            unsigned green = (15u - pen) * 17u;
            unsigned blue = ((pen * 3u) & 15u) * 17u;
            if (corrupt_pixel != 0 && x == 0u && y == 0u) {
                red = 1u;
            }
            n = snprintf(out + used, cap - used, "%u %u %u%s", red, green, blue,
                         x == 7u ? "\n" : " ");
            if (n < 0 || (size_t)n >= cap - used) {
                return 1;
            }
            used += (size_t)n;
        }
    }
    return (int)used;
}

static int build_sample_manifest(char *out, size_t cap, int omit_hash)
{
    int n;
    if (omit_hash != 0) {
        n = snprintf(out, cap,
            "artifact = SNAPSHOT\nimage_format = P3\nwidth = 8\nheight = 8\n"
            "guest_tick = 5000\nstartup = A\nprofile = none\n"
            "palette = diagnostic-palette-v1\n"
            "interpretation = diagnostic-owned pixels; no canonical Lynx video claim\n"
            "audio = placeholder silence pending Phase6\n"
            "wav = PCM mono 8000Hz signed16le; 800 zero samples\n");
    } else {
        n = snprintf(out, cap,
            "artifact = SNAPSHOT\nimage_format = P3\nwidth = 8\nheight = 8\n"
            "guest_tick = 5000\nstartup = A\nprofile = none\n"
            "content_sha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
            "palette = diagnostic-palette-v1\n"
            "interpretation = diagnostic-owned pixels; no canonical Lynx video claim\n"
            "audio = placeholder silence pending Phase6\n"
            "wav = PCM mono 8000Hz signed16le; 800 zero samples\n");
    }
    return n < 0 || (size_t)n >= cap ? -1 : n;
}

static void store_u16le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)(value >> 8);
}

static void store_u32le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)(value >> 24);
}

static int build_sample_wav(uint8_t out[WAV_SIZE], int corrupt_length)
{
    memset(out, 0, WAV_SIZE);
    memcpy(out, "RIFF", 4u);
    store_u32le(out + 4u, (uint32_t)(WAV_SIZE - 8) - (corrupt_length != 0 ? 1u : 0u));
    memcpy(out + 8u, "WAVEfmt ", 8u);
    store_u32le(out + 16u, 16u);
    store_u16le(out + 20u, 1u);
    store_u16le(out + 22u, 1u);
    store_u32le(out + 24u, 8000u);
    store_u32le(out + 28u, 16000u);
    store_u16le(out + 32u, 2u);
    store_u16le(out + 34u, 16u);
    memcpy(out + 36u, "data", 4u);
    store_u32le(out + 40u, WAV_DATA_BYTES);
    return 0;
}

static int write_sample_set(const char *directory, int corrupt_pixel,
                            int corrupt_wav, int omit_hash)
{
    char path[4096];
    char ppm[PPM_LIMIT];
    char manifest[MANIFEST_LIMIT];
    uint8_t wav[WAV_SIZE];
    int ppm_len = build_sample_ppm(ppm, sizeof ppm, corrupt_pixel);
    int manifest_len = build_sample_manifest(manifest, sizeof manifest, omit_hash);
    if (ppm_len < 0 || manifest_len < 0 || build_sample_wav(wav, corrupt_wav) != 0) {
        return 1;
    }
    if (path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
        write_owned_file(path, ppm, (size_t)ppm_len) != 0 ||
        path_join(directory, "snapshot.manifest", path, sizeof path) != 0 ||
        write_owned_file(path, manifest, (size_t)manifest_len) != 0 ||
        path_join(directory, "silence.wav", path, sizeof path) != 0 ||
        write_owned_file(path, wav, sizeof wav) != 0) {
        return 1;
    }
    return 0;
}

static int expect_rejection(const char *directory, const char *phrase)
{
    char error[256] = {0};
    if (validate_directory(directory, error, sizeof error) == 0 ||
        strstr(error, phrase) == NULL) {
        fprintf(stderr, "artifact-check self-test: expected rejection '%s', got '%s'\n",
                phrase, error);
        return 1;
    }
    return 0;
}

static int self_test(void)
{
    char directory[] = "/tmp/lynxplosion-artifacts-XXXXXX";
    char path[4096];
    char error[256] = {0};
    int temp_file;
    int failed = 0;
    temp_file = mkstemp(directory);
    if (temp_file < 0) {
        fprintf(stderr, "artifact-check self-test: cannot create isolated temporary directory\n");
        return 1;
    }
    if (close(temp_file) != 0 || remove(directory) != 0 || mkdir(directory, 0700) != 0) {
        (void)remove(directory);
        fprintf(stderr, "artifact-check self-test: cannot prepare isolated temporary directory\n");
        return 1;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        validate_directory(directory, error, sizeof error) != 0) {
        fprintf(stderr, "artifact-check self-test: valid control failed: %s\n", error);
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 1, 0, 0) != 0 ||
        expect_rejection(directory, "pixel differs") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0) {
        failed = 1;
        goto done;
    }
    if (path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
        replace_file_text(path, PPM_LIMIT, "P3\n", "P6\n") != 0 ||
        expect_rejection(directory, "not plain P3") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
        replace_file_text(path, PPM_LIMIT, "0 255 0", "999 255 0") != 0 ||
        expect_rejection(directory, "sample exceeds 255") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
        replace_file_text(path, PPM_LIMIT, "85 170 255\n", "85 170 255\n0\n") != 0 ||
        expect_rejection(directory, "sample count or trailing data is invalid") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 1, 0) != 0 ||
        expect_rejection(directory, "chunk sizes or PCM") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "silence.wav", path, sizeof path) != 0 ||
        set_file_byte(path, 0L, 'N') != 0 ||
        expect_rejection(directory, "exact bounded RIFF/WAVE layout") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "silence.wav", path, sizeof path) != 0 ||
        set_file_byte(path, 4L, 0xFF) != 0 || set_file_byte(path, 5L, 0xFF) != 0 ||
        set_file_byte(path, 6L, 0xFF) != 0 || set_file_byte(path, 7L, 0xFF) != 0 ||
        expect_rejection(directory, "chunk sizes or PCM") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "silence.wav", path, sizeof path) != 0 ||
        set_file_byte(path, 44L, 1) != 0 ||
        expect_rejection(directory, "non-zero placeholder samples") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 1) != 0 ||
        expect_rejection(directory, "content_sha256") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        path_join(directory, "snapshot.manifest", path, sizeof path) != 0 ||
        replace_file_text(path, MANIFEST_LIMIT,
            "diagnostic-owned pixels; no canonical Lynx video claim",
            "frame pixels; no canonical Lynx video claim") != 0 ||
        expect_rejection(directory, "metadata field interpretation is missing or incorrect") != 0) {
        failed = 1;
        goto done;
    }
    if (write_sample_set(directory, 0, 0, 0) != 0 ||
        validate_directory(directory, error, sizeof error) != 0) {
        fprintf(stderr, "artifact-check self-test: accepted control did not recover: %s\n", error);
        failed = 1;
    }
done:
    if (path_join(directory, "snapshot.ppm", path, sizeof path) == 0) {
        (void)remove(path);
    }
    if (path_join(directory, "snapshot.manifest", path, sizeof path) == 0) {
        (void)remove(path);
    }
    if (path_join(directory, "silence.wav", path, sizeof path) == 0) {
        (void)remove(path);
    }
    if (rmdir(directory) != 0) {
        fprintf(stderr, "artifact-check self-test: could not remove its isolated temporary directory\n");
        failed = 1;
    }
    if (failed != 0) {
        return 1;
    }
    puts("artifact-check self-test: P3/WAV/metadata accepted; pixel, WAV-size and metadata controls rejected");
    return 0;
}

static int wait_for_child(pid_t child, int *exit_code)
{
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != child || !WIFEXITED(status)) {
        return 1;
    }
    *exit_code = WEXITSTATUS(status);
    return 0;
}

static int directory_has_exact_artifacts(const char *directory)
{
    DIR *stream = opendir(directory);
    struct dirent *entry;
    unsigned seen = 0u;
    int valid = stream != NULL;
    if (stream == NULL) {
        return 0;
    }
    while ((entry = readdir(stream)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, "snapshot.ppm") == 0) {
            seen |= 1u;
        } else if (strcmp(entry->d_name, "silence.wav") == 0) {
            seen |= 2u;
        } else if (strcmp(entry->d_name, "snapshot.manifest") == 0) {
            seen |= 4u;
        } else {
            valid = 0;
        }
    }
    if (closedir(stream) != 0 || seen != 7u) {
        valid = 0;
    }
    return valid;
}

static int remove_publication_output(const char *directory)
{
    static const char *const names[] = {
        "snapshot.ppm", "silence.wav", "snapshot.manifest"
    };
    char path[4096];
    size_t index;
    for (index = 0u; index < sizeof names / sizeof names[0]; index++) {
        if (path_join(directory, names[index], path, sizeof path) != 0 ||
            unlink(path) != 0) {
            return 1;
        }
    }
    return rmdir(directory) == 0 ? 0 : 1;
}

static int publication_control(const char *consumer, const char *content,
                               const char *symbols, const char *output)
{
    int gate[2] = {-1, -1};
    pid_t children[2] = {-1, -1};
    int exit_codes[2] = {-1, -1};
    unsigned index;
    char release[2] = {'x', 'x'};
    char error[256] = {0};
    int result = 1;

    if (pipe(gate) != 0) {
        fputs("artifact-check: cannot create the publication start gate\n", stderr);
        return 1;
    }
    for (index = 0u; index < 2u; index++) {
        children[index] = fork();
        if (children[index] == 0) {
            char ready;
            (void)close(gate[1]);
            if (read(gate[0], &ready, 1u) != 1) {
                _exit(126);
            }
            (void)close(gate[0]);
            (void)unsetenv("LYNX_TEST_IO_FAILURE");
            (void)unsetenv("LYNX_TEST_PUBLICATION_STOP");
            (void)unsetenv("LYNX_TEST_PUBLICATION_READY_FD");
            (void)unsetenv("LYNX_TEST_PUBLICATION_RESUME_FD");
            execl(consumer, consumer, "--content", content, "--symbols", symbols,
                  "--out", output, "--profile", "none", (char *)NULL);
            _exit(127);
        }
        if (children[index] < 0) {
            break;
        }
    }
    (void)close(gate[0]);
    if (index != 2u || write(gate[1], release, sizeof release) != (ssize_t)sizeof release) {
        (void)close(gate[1]);
        for (index = 0u; index < 2u; index++) {
            if (children[index] > 0) {
                (void)wait_for_child(children[index], &exit_codes[index]);
            }
        }
        fputs("artifact-check: could not start both competing publishers\n", stderr);
        return 1;
    }
    (void)close(gate[1]);
    for (index = 0u; index < 2u; index++) {
        if (wait_for_child(children[index], &exit_codes[index]) != 0) {
            fputs("artifact-check: a competing publisher did not exit normally\n", stderr);
            goto done;
        }
    }
    if (!((exit_codes[0] == 0 && exit_codes[1] == 1) ||
          (exit_codes[0] == 1 && exit_codes[1] == 0))) {
        fprintf(stderr,
                "artifact-check: expected exactly one complete publication and one refusal; got exits %d and %d\n",
                exit_codes[0], exit_codes[1]);
        goto done;
    }
    if (!directory_has_exact_artifacts(output) ||
        validate_directory(output, error, sizeof error) != 0) {
        fprintf(stderr, "artifact-check: winning publication is incomplete: %s\n",
                error[0] == '\0' ? "unexpected output entry or missing artifact" : error);
        goto done;
    }
    if (remove_publication_output(output) != 0) {
        fputs("artifact-check: could not clean its own successful publication fixture\n", stderr);
        goto done;
    }
    puts("artifact-check: concurrent publishers yielded one intact directory and one refusal");
    result = 0;
done:
    return result;
}

enum {
    INTERRUPTION_BEFORE_MANIFEST = 1u,
    INTERRUPTION_BEFORE_PUBLISH = 2u,
    INTERRUPTION_EXPECTED_PARTIAL = (1u << 0) | (1u << 1),
    INTERRUPTION_EXPECTED_COMPLETE = (1u << 0) | (1u << 1) | (1u << 2)
};

static int read_control_value(int fd, uint32_t *value)
{
    uint8_t *bytes = (uint8_t *)value;
    size_t used = 0u;
    while (used < sizeof *value) {
        ssize_t got = read(fd, bytes + used, sizeof *value - used);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got <= 0) {
            return 1;
        }
        used += (size_t)got;
    }
    return 0;
}

static int await_publication_checkpoint(int fd, uint32_t *event)
{
    struct pollfd ready_event;
    int poll_result;
    ready_event.fd = fd;
    ready_event.events = POLLIN;
    ready_event.revents = 0;
    do {
        poll_result = poll(&ready_event, 1u, 10000);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result != 1 || (ready_event.revents & POLLIN) == 0) {
        return 1;
    }
    return read_control_value(fd, event);
}

static int reap_after_sigkill(pid_t child, int *did_reap)
{
    int status;
    int kill_succeeded = kill(child, SIGKILL) == 0;
    pid_t waited;
    *did_reap = 0;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != child) {
        return 1;
    }
    *did_reap = 1;
    if (!kill_succeeded || !WIFSIGNALED(status) ||
        WTERMSIG(status) != SIGKILL) {
        return 1;
    }
    return 0;
}

static int destination_absent(const char *path)
{
    struct stat info;
    if (lstat(path, &info) == 0) {
        return 1;
    }
    return errno == ENOENT ? 0 : 1;
}

static int create_preservation_marker(const char *path)
{
    static const char marker[] = "preserve interruption sibling\n";
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    size_t used = 0u;
    int close_result;
    if (fd < 0) {
        return 1;
    }
    while (used < sizeof marker - 1u) {
        ssize_t written = write(fd, marker + used, sizeof marker - 1u - used);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            (void)close(fd);
            (void)unlink(path);
            return 1;
        }
        used += (size_t)written;
    }
    close_result = close(fd);
    return close_result == 0 ? 0 : 1;
}

static int preservation_marker_intact(const char *path)
{
    static const char marker[] = "preserve interruption sibling\n";
    uint8_t *bytes = NULL;
    size_t length = 0u;
    char error[160] = {0};
    int result = read_bounded(path, 128u, &bytes, &length, error, sizeof error);
    int intact = result == 0 && length == sizeof marker - 1u &&
                 memcmp(bytes, marker, sizeof marker - 1u) == 0;
    free(bytes);
    return intact ? 0 : 1;
}

static int locate_owned_stage(const char *output, char *stage, size_t stage_cap)
{
    const char *slash = strrchr(output, '/');
    const char *basename = slash == NULL ? output : slash + 1;
    char parent[4096];
    char prefix[4096];
    size_t prefix_len;
    DIR *stream;
    struct dirent *entry;
    unsigned matches = 0u;
    int valid = 1;
    if (slash == NULL) {
        (void)snprintf(parent, sizeof parent, ".");
    } else if (slash == output) {
        (void)snprintf(parent, sizeof parent, "/");
    } else {
        size_t parent_len = (size_t)(slash - output);
        if (parent_len >= sizeof parent) {
            return 1;
        }
        memcpy(parent, output, parent_len);
        parent[parent_len] = '\0';
    }
    {
        int prefix_len_value = snprintf(prefix, sizeof prefix,
                                        "%s.lynxplosion-stage-", basename);
        if (prefix_len_value < 0 || (size_t)prefix_len_value >= sizeof prefix) {
            return 1;
        }
    }
    prefix_len = strlen(prefix);
    stream = opendir(parent);
    if (stream == NULL) {
        return 1;
    }
    while ((entry = readdir(stream)) != NULL) {
        char candidate[4096];
        struct stat info;
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) {
            continue;
        }
        if (path_join(parent, entry->d_name, candidate, sizeof candidate) != 0 ||
            lstat(candidate, &info) != 0 || !S_ISDIR(info.st_mode) ||
            strlen(candidate) >= stage_cap) {
            valid = 0;
            continue;
        }
        matches++;
        (void)snprintf(stage, stage_cap, "%s", candidate);
    }
    if (closedir(stream) != 0) {
        valid = 0;
    }
    if (valid != 0 && matches == 1u) {
        return 0;
    }
    stage[0] = '\0';
    return 1;
}

static int stage_file_mask(const char *directory, unsigned *mask_out)
{
    struct stat directory_info;
    DIR *stream;
    struct dirent *entry;
    unsigned seen = 0u;
    int valid = 1;
    if (lstat(directory, &directory_info) != 0 || !S_ISDIR(directory_info.st_mode)) {
        return 1;
    }
    stream = opendir(directory);
    if (stream == NULL) {
        return 1;
    }
    while ((entry = readdir(stream)) != NULL) {
        unsigned bit;
        char path[4096];
        struct stat info;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, "snapshot.ppm") == 0) {
            bit = 1u << 0;
        } else if (strcmp(entry->d_name, "silence.wav") == 0) {
            bit = 1u << 1;
        } else if (strcmp(entry->d_name, "snapshot.manifest") == 0) {
            bit = 1u << 2;
        } else {
            valid = 0;
            continue;
        }
        if ((seen & bit) != 0u || path_join(directory, entry->d_name, path, sizeof path) != 0 ||
            lstat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) {
            valid = 0;
            continue;
        }
        seen |= bit;
    }
    if (closedir(stream) != 0) {
        valid = 0;
    }
    *mask_out = seen;
    return valid != 0 ? 0 : 1;
}

static int validate_interruption_stage(const char *directory, uint32_t event)
{
    unsigned mask = 0u;
    char error[256] = {0};
    if (stage_file_mask(directory, &mask) != 0) {
        return 1;
    }
    if (event == INTERRUPTION_BEFORE_PUBLISH) {
        return mask == INTERRUPTION_EXPECTED_COMPLETE &&
               validate_directory(directory, error, sizeof error) == 0 ? 0 : 1;
    }
    if (event == INTERRUPTION_BEFORE_MANIFEST) {
        uint8_t *ppm = NULL;
        uint8_t *wav = NULL;
        size_t ppm_len = 0u;
        size_t wav_len = 0u;
        fields image_fields;
        char path[4096];
        int valid = mask == INTERRUPTION_EXPECTED_PARTIAL;
        if (valid != 0 && (path_join(directory, "snapshot.ppm", path, sizeof path) != 0 ||
                           read_bounded(path, PPM_LIMIT, &ppm, &ppm_len,
                                        error, sizeof error) != 0 ||
                           validate_ppm(ppm, ppm_len, &image_fields, error, sizeof error) != 0)) {
            valid = 0;
        }
        if (valid != 0 && (path_join(directory, "silence.wav", path, sizeof path) != 0 ||
                           read_bounded(path, WAV_SIZE, &wav, &wav_len,
                                        error, sizeof error) != 0 ||
                           validate_wav(wav, wav_len, error, sizeof error) != 0)) {
            valid = 0;
        }
        free(ppm);
        free(wav);
        return valid != 0 ? 0 : 1;
    }
    return 1;
}

static int remove_owned_output_directory(const char *directory)
{
    static const char *const names[] = {
        "snapshot.ppm", "silence.wav", "snapshot.manifest"
    };
    unsigned mask = 0u;
    size_t index;
    if (stage_file_mask(directory, &mask) != 0 ||
        (mask != INTERRUPTION_EXPECTED_PARTIAL && mask != INTERRUPTION_EXPECTED_COMPLETE)) {
        return 1;
    }
    for (index = 0u; index < sizeof names / sizeof names[0]; index++) {
        unsigned bit = 1u << index;
        char path[4096];
        if ((mask & bit) == 0u) {
            continue;
        }
        if (path_join(directory, names[index], path, sizeof path) != 0 || unlink(path) != 0) {
            return 1;
        }
    }
    return rmdir(directory) == 0 ? 0 : 1;
}

static int interruption_control(const char *consumer, const char *content,
                                const char *symbols, const char *base,
                                const char *checkpoint, uint32_t expected_event)
{
    int ready_pipe[2] = {-1, -1};
    int resume_pipe[2] = {-1, -1};
    pid_t child = -1;
    int reaped = 0;
    int result = 1;
    int failure = 0;
    uint32_t event = 0u;
    char output[4096] = {0};
    char marker[4096] = {0};
    char stage[4096] = {0};
    char ready_text[32];
    char resume_text[32];

    if (mkdir(base, 0700) != 0 || path_join(base, "output", output, sizeof output) != 0 ||
        path_join(base, "preserved.marker", marker, sizeof marker) != 0 ||
        create_preservation_marker(marker) != 0 || destination_absent(output) != 0) {
        fputs("artifact-check: could not prepare the isolated interruption fixture\n", stderr);
        goto done;
    }
    if (pipe(ready_pipe) != 0 || pipe(resume_pipe) != 0) {
        fputs("artifact-check: cannot create the interruption synchronization pipes\n", stderr);
        goto done;
    }
    if (snprintf(ready_text, sizeof ready_text, "%d", ready_pipe[1]) < 0 ||
        snprintf(resume_text, sizeof resume_text, "%d", resume_pipe[0]) < 0) {
        fputs("artifact-check: cannot encode interruption synchronization descriptors\n", stderr);
        goto done;
    }
    child = fork();
    if (child == 0) {
        (void)close(ready_pipe[0]);
        (void)close(resume_pipe[1]);
        if (unsetenv("LYNX_TEST_IO_FAILURE") != 0 ||
            setenv("LYNX_TEST_PUBLICATION_STOP", checkpoint, 1) != 0 ||
            setenv("LYNX_TEST_PUBLICATION_READY_FD", ready_text, 1) != 0 ||
            setenv("LYNX_TEST_PUBLICATION_RESUME_FD", resume_text, 1) != 0) {
            _exit(125);
        }
        execl(consumer, consumer, "--content", content, "--symbols", symbols,
              "--out", output, "--profile", "none", (char *)NULL);
        _exit(127);
    }
    if (child < 0) {
        fputs("artifact-check: could not launch the interruption consumer\n", stderr);
        goto done;
    }
    (void)close(ready_pipe[1]);
    ready_pipe[1] = -1;
    (void)close(resume_pipe[0]);
    resume_pipe[0] = -1;
    if (await_publication_checkpoint(ready_pipe[0], &event) != 0) {
        fputs("artifact-check: consumer did not reach its bounded publication checkpoint\n", stderr);
        failure = 1;
        goto terminate;
    }
    if (event != expected_event) {
        fputs("artifact-check: consumer stopped at the wrong publication checkpoint\n", stderr);
        failure = 1;
        goto terminate;
    }
    if (destination_absent(output) != 0) {
        fputs("artifact-check: output was published before the requested interruption checkpoint\n", stderr);
        failure = 1;
        goto terminate;
    }
    if (locate_owned_stage(output, stage, sizeof stage) != 0 ||
        validate_interruption_stage(stage, event) != 0) {
        fputs("artifact-check: private staging contents do not match the requested checkpoint\n", stderr);
        failure = 1;
        goto terminate;
    }

terminate:
    {
        int child_reaped = 0;
        if (reap_after_sigkill(child, &child_reaped) != 0) {
            fputs("artifact-check: could not SIGKILL and reap the exact interrupted consumer child\n", stderr);
            failure = 1;
        }
        if (child_reaped != 0) {
            reaped = 1;
            child = -1;
        }
    }
    if (child > 0) {
        goto done;
    }
    if (reaped == 0) {
        fputs("artifact-check: could not SIGKILL and reap the exact interrupted consumer child\n", stderr);
        failure = 1;
    }
    if (failure == 0 && (destination_absent(output) != 0 ||
                         preservation_marker_intact(marker) != 0 ||
                         locate_owned_stage(output, stage, sizeof stage) != 0 ||
                         validate_interruption_stage(stage, event) != 0)) {
        fputs("artifact-check: interrupted publication changed its destination, sibling marker, or private stage\n", stderr);
        failure = 1;
    }
    if (reaped != 0 && stage[0] != '\0' && access(stage, F_OK) == 0 &&
        remove_owned_output_directory(stage) != 0) {
        fputs("artifact-check: could not clean the invocation-owned interrupted staging directory\n", stderr);
        failure = 1;
    }
    if (reaped != 0 && access(output, F_OK) == 0 &&
        remove_owned_output_directory(output) != 0) {
        fputs("artifact-check: could not clean the invocation-owned interrupted destination\n", stderr);
        failure = 1;
    }
    if (failure == 0) {
        printf("artifact-check: SIGKILL at %s left no published destination; private stage matched its expected contents\n",
               checkpoint);
        result = 0;
    }
done:
    if (child > 0) {
        int child_reaped = 0;
        (void)reap_after_sigkill(child, &child_reaped);
        if (child_reaped != 0) {
            reaped = 1;
            child = -1;
        }
    }
    if (reaped != 0) {
        if (stage[0] == '\0' && output[0] != '\0') {
            (void)locate_owned_stage(output, stage, sizeof stage);
        }
        if (stage[0] != '\0' && access(stage, F_OK) == 0) {
            (void)remove_owned_output_directory(stage);
        }
        if (output[0] != '\0' && access(output, F_OK) == 0) {
            (void)remove_owned_output_directory(output);
        }
    }
    if (ready_pipe[0] >= 0) {
        (void)close(ready_pipe[0]);
    }
    if (ready_pipe[1] >= 0) {
        (void)close(ready_pipe[1]);
    }
    if (resume_pipe[0] >= 0) {
        (void)close(resume_pipe[0]);
    }
    if (resume_pipe[1] >= 0) {
        (void)close(resume_pipe[1]);
    }
    return result;
}

int main(int argc, char **argv)
{
    char error[256] = {0};
    char before_manifest[4096];
    char before_publish[4096];
    if (argc == 2 && strcmp(argv[1], "self-test") == 0) {
        return self_test();
    }
    if (argc == 3 && strcmp(argv[1], "frames") == 0) {
        if (validate_frame_directory(argv[2], error, sizeof error) == 0) {
            puts("artifact-check: independent released/A raw-frame semantics are valid");
            return 0;
        }
        fprintf(stderr, "artifact-check: %s\n", error);
        return 1;
    }
    if (argc == 5 && strcmp(argv[1], "frames") == 0) {
        if (validate_replay_directory(argv[2], argv[3], argv[4], "1", "none", error, sizeof error, 0) == 0) {
            puts("artifact-check: canonical replay raw/rendered frame transcript is valid");
            return 0;
        }
        fprintf(stderr, "artifact-check: %s\n", error);
        return 1;
    }
    if (argc == 7 && strcmp(argv[1], "frames") == 0) {
        if (validate_replay_directory(argv[2], argv[3], argv[4], argv[5], argv[6], error, sizeof error, 0) == 0) {
            puts("artifact-check: canonical replay raw/rendered frame transcript is valid");
            return 0;
        }
        fprintf(stderr, "artifact-check: %s\n", error);
        return 1;
    }
    if (argc == 7 && strcmp(argv[1], "frames-color-negative") == 0) {
        if (validate_replay_directory(argv[2], argv[3], argv[4], argv[5], argv[6], error, sizeof error, 1) == 0) {
            fputs("artifact-check: accepted a deliberately corrupted transition pixel color\n", stderr);
            return 1;
        }
        puts("artifact-check: rejected the deliberately corrupted transition pixel color");
        return 0;
    }
    if (argc == 6 && strcmp(argv[1], "publication") == 0) {
        if (publication_control(argv[2], argv[3], argv[4], argv[5]) != 0) {
            return 1;
        }
        {
            int before_manifest_len = snprintf(before_manifest, sizeof before_manifest,
                                               "%s.interruption-before-manifest", argv[5]);
            int before_publish_len = snprintf(before_publish, sizeof before_publish,
                                              "%s.interruption-before-publish", argv[5]);
            if (before_manifest_len < 0 ||
                (size_t)before_manifest_len >= sizeof before_manifest ||
                before_publish_len < 0 ||
                (size_t)before_publish_len >= sizeof before_publish) {
                fputs("artifact-check: interruption fixture path exceeds its bound\n", stderr);
                return 1;
            }
        }
        if (interruption_control(argv[2], argv[3], argv[4], before_manifest,
                                 "before-manifest", INTERRUPTION_BEFORE_MANIFEST) != 0 ||
            interruption_control(argv[2], argv[3], argv[4], before_publish,
                                 "before-publish", INTERRUPTION_BEFORE_PUBLISH) != 0) {
            return 1;
        }
        puts("artifact-check: interruption controls passed at both publication boundaries");
        return 0;
    }
    if (argc == 2 && validate_directory(argv[1], error, sizeof error) == 0) {
        puts("artifact-check: SNAPSHOT P3 and placeholder PCM WAV are valid");
        return 0;
    }
    if (argc == 2) {
        fprintf(stderr, "artifact-check: %s\n", error);
        return 1;
    }
    fprintf(stderr, "Usage: lynx-check-artifacts <output-directory> | frames DIRECTORY [SCENARIO INPUT_SHA256] | self-test | publication CONSUMER BLL LABELS OUTPUT\n");
    return 2;
}
