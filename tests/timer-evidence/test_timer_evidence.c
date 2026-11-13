#include "lynxplosion.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct image_labels {
    uint32_t done;
    uint32_t completion;
    uint32_t after_dispctl;
    uint32_t result_start;
    uint32_t initial_counter;
    uint32_t requested_dispctl;
    uint32_t requested_mapctl;
    uint32_t progress;
    uint32_t framebuffer_start;
    uint32_t framebuffer_end;
} image_labels;

static int fail(const char *message)
{
    fprintf(stderr, "timer evidence: %s\n", message);
    return 1;
}

static int read_file(const char *path, uint8_t **out, size_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *bytes;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
        fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) (void)fclose(file);
        return -1;
    }
    bytes = malloc((size_t)size);
    if (bytes == NULL || fread(bytes, 1u, (size_t)size, file) != (size_t)size || fclose(file) != 0) {
        free(bytes);
        return -1;
    }
    *out = bytes;
    *length = (size_t)size;
    return 0;
}

static int label_value(const char *labels_path, const char *wanted, uint32_t *value)
{
    FILE *file = fopen(labels_path, "r");
    char line[256];
    unsigned found = 0u;
    if (file == NULL) return -1;
    while (fgets(line, sizeof line, file) != NULL) {
        char name[160];
        unsigned parsed;
        if (sscanf(line, "al %x .%159s", &parsed, name) == 2 && strcmp(name, wanted) == 0) {
            *value = (uint32_t)parsed;
            found++;
        }
    }
    if (fclose(file) != 0 || found != 1u) return -1;
    return 0;
}

static int record_value(const char *path, const char *wanted, char *out, size_t capacity)
{
    FILE *file = fopen(path, "r");
    char line[1024];
    unsigned found = 0u;
    if (file == NULL || capacity == 0u) {
        if (file != NULL) (void)fclose(file);
        return -1;
    }
    while (fgets(line, sizeof line, file) != NULL) {
        const size_t key_length = strlen(wanted);
        if (strncmp(line, wanted, key_length) == 0 && line[key_length] == '=') {
            size_t value_length = strlen(line + key_length + 1u);
            while (value_length != 0u && (line[key_length + value_length] == '\n' ||
                   line[key_length + value_length] == '\r')) value_length--;
            if (value_length >= capacity) { (void)fclose(file); return -1; }
            memcpy(out, line + key_length + 1u, value_length);
            out[value_length] = '\0';
            found++;
        }
    }
    if (fclose(file) != 0 || found != 1u) return -1;
    return 0;
}

static int validate_case_record(const char *path, const char *case_name, int hardware,
                                const lynx_identity *identity)
{
    char value[160];
    char image_sha[65];
    static const char hex[] = "0123456789abcdef";
    size_t i;
    if (identity == NULL || record_value(path, "case", value, sizeof value) != 0 || strcmp(value, case_name) != 0 ||
        record_value(path, "family", value, sizeof value) != 0 ||
        strcmp(value, hardware ? "hardware0D" : "software01") != 0 ||
        record_value(path, "dma", value, sizeof value) != 0 || strcmp(value, "1") != 0 ||
        record_value(path, "page", value, sizeof value) != 0 || strcmp(value, "1") != 0 ||
        record_value(path, "requested_dispctl", value, sizeof value) != 0 ||
        strcmp(value, hardware ? "0D" : "01") != 0 ||
        record_value(path, "requested_mapctl", value, sizeof value) != 0 || strcmp(value, "0C") != 0 ||
        record_value(path, "measurement_display", value, sizeof value) != 0 || strcmp(value, "requested") != 0 ||
        record_value(path, "restored_display", value, sizeof value) != 0 ||
        strcmp(value, hardware ? "unsupported" : "01") != 0 ||
        record_value(path, "object_sha256", image_sha, sizeof image_sha) != 0) {
        return fail("case record does not match the requested image identity");
    }
    for (i = 0u; i < sizeof identity->content_sha256; i++) {
        if (image_sha[i * 2u] != hex[identity->content_sha256[i] >> 4] ||
            image_sha[i * 2u + 1u] != hex[identity->content_sha256[i] & 0x0Fu])
            return fail("case record object hash differs from the core-loaded image identity");
    }
    return 0;
}

static int read_labels(const char *path, image_labels *labels)
{
    return label_value(path, "suite_done", &labels->done) == 0 &&
           label_value(path, "suite_completion_signature", &labels->completion) == 0 &&
           label_value(path, "suite_dispctl_after", &labels->after_dispctl) == 0 &&
           label_value(path, "suite_result_start", &labels->result_start) == 0 &&
           label_value(path, "suite_initial_counter", &labels->initial_counter) == 0 &&
           label_value(path, "suite_requested_dispctl", &labels->requested_dispctl) == 0 &&
           label_value(path, "suite_requested_mapctl", &labels->requested_mapctl) == 0 &&
           label_value(path, "suite_progress", &labels->progress) == 0 &&
           label_value(path, "suite_framebuffer_start", &labels->framebuffer_start) == 0 &&
           label_value(path, "suite_framebuffer_end", &labels->framebuffer_end) == 0 ? 0 : -1;
}

static int guest_byte(const lynx_machine *machine, uint32_t address, uint8_t *value)
{
    return lynx_read_memory(machine, address, value, 1u) == LYNX_OK ? 0 : -1;
}

static int glyph_rows(unsigned char character, const uint8_t **rows)
{
    static const uint8_t glyphs[38][5] = {
        {6,9,9,9,6}, {2,6,2,2,7}, {6,9,2,4,15}, {14,1,6,1,14},
        {9,9,15,1,1}, {15,8,14,1,14}, {6,8,14,9,6}, {15,1,2,4,4},
        {6,9,6,9,6}, {6,9,7,1,6},
        {6,9,15,9,9}, {14,9,14,9,14}, {7,8,8,8,7}, {14,9,9,9,14},
        {15,8,14,8,15}, {15,8,14,8,8}, {7,8,11,9,7}, {9,9,15,9,9},
        {7,2,2,2,7}, {1,1,1,9,6}, {9,10,12,10,9}, {8,8,8,8,15},
        {9,15,15,9,9}, {9,13,11,9,9}, {6,9,9,9,6}, {14,9,14,8,8},
        {6,9,9,11,7}, {14,9,14,10,9}, {7,8,6,1,14}, {15,2,2,2,2},
        {9,9,9,9,6}, {9,9,9,6,6}, {9,9,15,15,9}, {9,6,6,6,9},
        {9,6,2,2,2}, {15,1,2,4,15}, {0,15,0,15,0}, {0,0,0,0,0}
    };
    unsigned index;
    if (character >= '0' && character <= '9') index = (unsigned)(character - '0');
    else if (character >= 'A' && character <= 'Z') index = 10u + (unsigned)(character - 'A');
    else if (character == '=') index = 36u;
    else if (character == ' ') index = 37u;
    else return -1;
    *rows = glyphs[index];
    return 0;
}

static unsigned frame_pixel(const uint8_t *pens, unsigned x, unsigned y)
{
    const uint8_t packed = pens[(size_t)y * LYNX_FRAME_ROW_BYTES + x / 2u];
    return (x & 1u) == 0u ? packed >> 4 : packed & 0x0Fu;
}

static int decode_text(const uint8_t *pens, unsigned row_count, const char *text,
                       unsigned x_start, unsigned y_start)
{
    unsigned column = 0u;
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != 0u) {
        const uint8_t *expected_rows;
        unsigned y;
        if (glyph_rows(*cursor, &expected_rows) != 0) return -1;
        for (y = 0u; y < 5u; y++) {
            unsigned x;
            if (y_start + y >= row_count) return -1;
            for (x = 0u; x < 6u; x++) {
                const unsigned set = x < 4u &&
                    (expected_rows[y] & (uint8_t)(1u << (3u - x))) != 0u;
                if (frame_pixel(pens, x_start + column * 6u + x, y_start + y) != (set ? 15u : 0u)) {
                    fprintf(stderr, "raster mismatch char=%c x=%u y=%u got=%u expected=%u\n",
                            *cursor, x_start + column * 6u + x, y_start + y,
                            frame_pixel(pens, x_start + column * 6u + x, y_start + y), set ? 15u : 0u);
                    return -1;
                }
            }
        }
        column++;
        cursor++;
    }
    return 0;
}

static int decode_timer_screen(const uint8_t *pens, unsigned row_count,
                               const uint8_t results[18], int hardware)
{
    static const char hex[] = "0123456789ABCDEF";
    const char *header = hardware ? "TIMER D1P1 H0D" : "TIMER D1P1 S01";
    unsigned i;
    if (decode_text(pens, row_count, header, 0u, 0u) != 0 ||
        decode_text(pens, row_count, "INIT00 DONEA5", 0u, 6u) != 0) return -1;
    for (i = 0u; i < 18u; i++) {
        char line[6];
        const unsigned xchar = i < 9u ? 0u : 10u;
        const unsigned row = i % 9u;
        line[0] = hex[(i >> 4) & 0x0Fu];
        line[1] = hex[i & 0x0Fu];
        line[2] = '=';
        line[3] = hex[results[i] >> 4];
        line[4] = hex[results[i] & 0x0Fu];
        line[5] = '\0';
        if (decode_text(pens, row_count, line, xchar * 6u, 14u + row * 6u) != 0) return -1;
    }
    return 0;
}

static int run_image(const char *kind, const char *bll_path, const char *labels_path,
                     const char *profile_name, const char *case_record,
                     const char *case_name)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    image_labels labels;
    lynx_config config;
    lynx_machine *machine = NULL;
    void *machine_memory = NULL;
    lynx_identity identity;
    uint32_t profile;
    uint64_t remaining = UINT64_C(12000000);
    uint64_t total_used = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_status status;
    lynx_watch_event watch;
    int expected_hardware = strcmp(kind, "hardware") == 0;
    int completed = 0;
    uint8_t result_bytes[18];
    uint8_t signature = 0u, done = 0u, initial = 0u, requested = 0u, mapctl = 0u, progress = 0u;
    size_t i;

    if (strcmp(profile_name, "lynx1") == 0) profile = LYNX_PROFILE_LYNX1;
    else if (strcmp(profile_name, "lynx2") == 0) profile = LYNX_PROFILE_LYNX2;
    else return fail("profile must be lynx1 or lynx2");
    if ((!expected_hardware && strcmp(kind, "software") != 0) || read_labels(labels_path, &labels) != 0 ||
        read_file(bll_path, &bll, &bll_length) != 0) return fail("image or required labels could not be read");

    lynx_config_default(&config);
    config.profile = profile;
    config.timing_policy = LYNX_TIMING_POLICY_VIDEO_POLICY1;
    if (posix_memalign(&machine_memory, lynx_machine_align(), lynx_machine_size()) != 0 ||
        lynx_machine_init(machine_memory, lynx_machine_size(), &config, &machine) != LYNX_OK ||
        lynx_load_bll(machine, bll, bll_length) != LYNX_OK || lynx_get_identity(machine, &identity) != LYNX_OK) {
        free(machine_memory);
        free(bll);
        return fail("fresh machine initialization or BS93 load failed");
    }
    if (identity.profile != profile || identity.has_content != 1u || identity.object_len != bll_length ||
        identity.content_addr != 0x0200u || identity.startup_tier != LYNX_STARTUP_A) {
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return fail("loaded image identity differs from the requested profile or BS93 object");
    }
    if (case_record != NULL && validate_case_record(case_record, case_name, expected_hardware, &identity) != 0) {
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return 1;
    }
    if (lynx_watch_configure(machine, labels.done, 1u) != LYNX_OK) {
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return fail("completion watch could not be armed");
    }

    while (remaining != 0u) {
        uint64_t used = 0u;
        const uint64_t slice = remaining < UINT64_C(5000) ? remaining : UINT64_C(5000);
        status = lynx_advance(machine, slice, &used, &stop);
        if (status != LYNX_OK || used > slice) break;
        total_used += used;
        remaining -= used;
        if (stop == LYNX_STOP_WATCH) {
            if (lynx_watch_get(machine, &watch) != LYNX_OK || watch.hit != 1u ||
                watch.address != labels.done) break;
            if (watch.value == 0x00u) {
                if (lynx_watch_configure(machine, labels.done, 1u) != LYNX_OK) break;
                continue;
            }
            if (watch.value == 0xA5u) {
                completed = 1;
                break;
            }
            break;
        }
        if (stop == LYNX_STOP_UNAVAILABLE || used == 0u) break;
    }
    if (status != LYNX_OK || (expected_hardware && stop != LYNX_STOP_UNAVAILABLE) ||
        (!expected_hardware && (completed == 0 || stop != LYNX_STOP_WATCH))) {
        uint32_t pc = 0u;
        (void)lynx_read_register(machine, LYNX_REG_PC, &pc);
        fprintf(stderr, "timer evidence: %s run did not reach its expected stop (status=%u stop=%u pc=%04" PRIx32 " ticks=%" PRIu64 ")\n",
                kind, status, stop, pc, total_used);
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return 1;
    }

    if (expected_hardware) {
        uint32_t pc = 0u;
        (void)lynx_read_register(machine, LYNX_REG_PC, &pc);
        if (pc != labels.after_dispctl || guest_byte(machine, labels.done, &done) != 0 || done != 0u ||
            guest_byte(machine, labels.completion, &signature) != 0 || signature != 0u ||
            guest_byte(machine, labels.requested_dispctl, &requested) != 0 || requested != 0x0Du ||
            lynx_watch_get(machine, &watch) != LYNX_OK || watch.hit != 0u) {
            lynx_machine_deinit(machine);
            free(machine_memory);
            free(bll);
            return fail("hardware0D did not visibly refuse its requested display mode before completion");
        }
        printf("hardware0D-refusal profile=%s stop=UNAVAILABLE pc=%04" PRIX32 " requested_dispctl=%02X completion=missing\n",
               profile_name, pc, requested);
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return 0;
    }

    {
        const lynx_status watch_status = lynx_watch_get(machine, &watch);
        const int read_ok = guest_byte(machine, labels.done, &done) == 0 &&
            guest_byte(machine, labels.completion, &signature) == 0 &&
            guest_byte(machine, labels.initial_counter, &initial) == 0 &&
            guest_byte(machine, labels.requested_dispctl, &requested) == 0 &&
            guest_byte(machine, labels.requested_mapctl, &mapctl) == 0 &&
            guest_byte(machine, labels.progress, &progress) == 0 &&
            lynx_read_memory(machine, labels.result_start, result_bytes, sizeof result_bytes) == LYNX_OK;
        if (watch_status != LYNX_OK || watch.hit != 1u || watch.address != labels.done ||
            watch.value != 0xA5u || read_ok == 0 || done != 0xA5u || signature != 0xA5u ||
            initial != 0u || requested != 0x01u || mapctl != 0x0Cu || progress != 0x08u) {
            fprintf(stderr, "tracer fields: watch=%u/%" PRIu32 "/%02" PRIX32 " read=%d done=%02X sig=%02X init=%02X disp=%02X map=%02X progress=%02X result=%04" PRIX32 "\n",
                    watch.hit, watch.address, watch.value, read_ok, done, signature, initial, requested, mapctl, progress, labels.result_start);
        } else {
            goto software_fields_valid;
        }
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return fail("software result capture, initial state or completion record is invalid");
    }
software_fields_valid:

    {
        uint8_t pens[LYNX_FRAME_PENS_CAPACITY];
        uint8_t palettes[LYNX_FRAME_PALETTE_CAPACITY];
        uint64_t line_ticks[LYNX_FRAME_LINE_TICKS_CAPACITY];
        lynx_frame_buffers buffers = {0};
        lynx_frame_result frame = {0};
        unsigned attempts;
        buffers.struct_size = (uint32_t)sizeof buffers;
        buffers.pens = pens;
        buffers.pens_capacity = sizeof pens;
        buffers.palettes = palettes;
        buffers.palettes_capacity = sizeof palettes;
        buffers.line_ticks = line_ticks;
        buffers.line_ticks_capacity = LYNX_FRAME_LINE_TICKS_CAPACITY;
        frame.struct_size = (uint32_t)sizeof frame;
        for (attempts = 0u; attempts < 4u; attempts++) {
            status = lynx_advance_frame(machine, UINT64_C(500000), &buffers, &frame);
            if (status != LYNX_OK) break;
            if (frame.stop == LYNX_STOP_FRAME && frame.sequence > 0u && frame.row_count > 0u) break;
            if (frame.ticks_consumed == 0u) break;
        }
        if (status != LYNX_OK || frame.stop != LYNX_STOP_FRAME || frame.sequence == 0u || frame.row_count == 0u) {
            lynx_machine_deinit(machine);
            free(machine_memory);
            free(bll);
            return fail("software completion succeeded but no raw video frame was captured");
        }
        if (decode_timer_screen(pens, frame.row_count, result_bytes, 0) != 0) {
            uint8_t framebuffer_head[42] = {0};
            (void)lynx_read_memory(machine, labels.framebuffer_start, framebuffer_head, sizeof framebuffer_head);
            fprintf(stderr, "framebuffer row0=");
            for (i = 0u; i < sizeof framebuffer_head; i++) fprintf(stderr, "%02X", framebuffer_head[i]);
            fprintf(stderr, " row1=");
            (void)lynx_read_memory(machine, labels.framebuffer_start + LYNX_FRAME_ROW_BYTES,
                                   framebuffer_head, sizeof framebuffer_head);
            for (i = 0u; i < sizeof framebuffer_head; i++) fprintf(stderr, "%02X", framebuffer_head[i]);
            fprintf(stderr, " raw row1=");
            for (i = 0u; i < sizeof framebuffer_head; i++) fprintf(stderr, "%02X", pens[LYNX_FRAME_ROW_BYTES + i]);
            fputc('\n', stderr);
            lynx_machine_deinit(machine);
            free(machine_memory);
            free(bll);
            return fail("independent raw-frame glyph decoding disagrees with the captured result bytes");
        }
        printf("raster_decode=passed captured_rows=18 result14=%02X\n", result_bytes[14]);
        printf("frame sequence=%" PRIu64 " rows=%" PRIu32 " pens_sha_preview=", frame.sequence, frame.row_count);
        for (i = 0u; i < 8u; i++) printf("%02X", pens[i]);
        putchar('\n');
    }
    printf("software01-complete profile=%s ticks=%" PRIu64 " init_counter=%02X mapctl=%02X dispctl=%02X results=",
           profile_name, total_used, initial, mapctl, requested);
    for (i = 0u; i < sizeof result_bytes; i++) printf("%02X", result_bytes[i]);
    putchar('\n');
    lynx_machine_deinit(machine);
    free(machine_memory);
    free(bll);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--tracer") == 0) {
        const char *profiles[] = {"lynx1", "lynx2"};
        const char *families[] = {"hardware", "software"};
        size_t i, j;
        char bll[4096], labels[4096], record[4096];
        for (i = 0u; i < sizeof profiles / sizeof profiles[0]; i++) {
            for (j = 0u; j < sizeof families / sizeof families[0]; j++) {
                const char *case_name = j == 0u ? "hardware-dma1-page1" : "software-dma1-page1";
                if (snprintf(bll, sizeof bll, "%s/%s.bll", argv[2], case_name) >= (int)sizeof bll ||
                    snprintf(labels, sizeof labels, "%s/%s.lbl", argv[2], case_name) >= (int)sizeof labels ||
                    snprintf(record, sizeof record, "%s/%s.case.txt", argv[2], case_name) >= (int)sizeof record ||
                    run_image(families[j], bll, labels, profiles[i], record, case_name) != 0) return 1;
            }
        }
        return 0;
    }
    if (argc != 5 || (strcmp(argv[1], "--software") != 0 && strcmp(argv[1], "--hardware") != 0)) {
        fprintf(stderr, "usage: lynx-test-timer-evidence --tracer PACKAGE | --software|--hardware IMAGE.bll IMAGE.lbl lynx1|lynx2\n");
        return 2;
    }
    return run_image(strcmp(argv[1], "--hardware") == 0 ? "hardware" : "software",
                     argv[2], argv[3], argv[4], NULL, NULL);
}
