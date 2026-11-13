#include "lynxplosion.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "timer_evidence_ids.h"

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
    uint32_t done_store;
    uint32_t model;
    uint32_t glyphs;
} image_labels;

typedef struct timer_case {
    const char *name;
    unsigned dma;
    unsigned page;
} timer_case;

static const timer_case timer_cases[4] = {
    {"hardware-dma0-page0", 0u, 0u},
    {"hardware-dma0-page1", 0u, 1u},
    {"hardware-dma1-page0", 1u, 0u},
    {"hardware-dma1-page1", 1u, 1u}
};

enum run_check { RUN_CAPTURE = 0, RUN_DECODER_CONTROLS = 1, RUN_WITHOUT_EXECUTION = 2 };

static int fail(const char *message)
{
    fprintf(stderr, "timer evidence: %s\n", message);
    return 1;
}

static const char *approved_image_hash(const char *name)
{
    if (name == NULL) return NULL;
    for (size_t i = 0u; i < sizeof timer_approved_images / sizeof timer_approved_images[0]; i++)
        if (strcmp(name, timer_approved_images[i].name) == 0) return timer_approved_images[i].sha256;
    return NULL;
}

static int read_file(const char *path, uint8_t **out, size_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *bytes;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (size = ftell(file)) <= 0 || size > 65535L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) (void)fclose(file);
        return -1;
    }
    bytes = malloc((size_t)size);
    if (bytes == NULL) { (void)fclose(file); return -1; }
    const size_t got = fread(bytes, 1u, (size_t)size, file);
    const int closed = fclose(file);
    if (got != (size_t)size || closed != 0) {
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
                                const timer_case *settings, const lynx_identity *identity)
{
    char value[160];
    char image_sha[65];
    const char *approved = approved_image_hash(case_name);
    if (settings == NULL || identity == NULL || approved == NULL)
        return fail("case validator requires approved settings and loaded identity");
    static const char hex[] = "0123456789abcdef";
    const unsigned requested_dispctl = hardware ? (settings->dma ? 0x0Du : 0x0Cu) :
        (settings->dma ? 0x01u : 0x00u);
    const unsigned requested_mapctl = settings->page ? 0x0Cu : 0x8Cu;
    const char *expected_restore = hardware ? "not-reached-core-refusal" : "01-after-render";
    char expected_dma[2], expected_page[2], expected_dispctl_text[3], expected_mapctl_text[3];
    size_t i;
    (void)snprintf(expected_dma, sizeof expected_dma, "%u", settings->dma);
    (void)snprintf(expected_page, sizeof expected_page, "%u", settings->page);
    (void)snprintf(expected_dispctl_text, sizeof expected_dispctl_text, "%02X", requested_dispctl);
    (void)snprintf(expected_mapctl_text, sizeof expected_mapctl_text, "%02X", requested_mapctl);
    if (record_value(path, "case", value, sizeof value) != 0 || strcmp(value, case_name) != 0 ||
        record_value(path, "family", value, sizeof value) != 0 ||
        strcmp(value, hardware ? "hardware0D" : "software01") != 0 ||
        record_value(path, "dma", value, sizeof value) != 0 || strcmp(value, expected_dma) != 0 ||
        record_value(path, "page", value, sizeof value) != 0 || strcmp(value, expected_page) != 0 ||
        record_value(path, "requested_dispctl", value, sizeof value) != 0 ||
        strcmp(value, expected_dispctl_text) != 0 ||
        record_value(path, "requested_mapctl", value, sizeof value) != 0 ||
        strcmp(value, expected_mapctl_text) != 0 ||
        record_value(path, "measurement_display", value, sizeof value) != 0 || strcmp(value, "requested") != 0 ||
        record_value(path, "restored_display", value, sizeof value) != 0 ||
        strcmp(value, expected_restore) != 0 ||
        record_value(path, "physical_readback", value, sizeof value) != 0 || strcmp(value, "unknown") != 0 ||
        record_value(path, "case_set_sha256", value, sizeof value) != 0 ||
        strcmp(value, TIMER_CASE_SET_SHA256) != 0 ||
        record_value(path, "object_sha256", image_sha, sizeof image_sha) != 0) {
        return fail("case record does not match the requested image identity");
    }
    if (strcmp(image_sha, approved) != 0) return fail("case image is outside the approved source-built set");
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
           label_value(path, "suite_framebuffer_end", &labels->framebuffer_end) == 0 &&
           label_value(path, "suite_done_store", &labels->done_store) == 0 &&
           label_value(path, "suite_model", &labels->model) == 0 &&
           label_value(path, "suite_glyphs", &labels->glyphs) == 0 ? 0 : -1;
}

static const timer_case *find_case(const char *name, int *hardware)
{
    size_t i;
    if (name == NULL || hardware == NULL) return NULL;
    for (i = 0u; i < sizeof timer_cases / sizeof timer_cases[0]; i++) {
        char software_name[64];
        if (strcmp(name, timer_cases[i].name) == 0) {
            *hardware = 1;
            return &timer_cases[i];
        }
        (void)snprintf(software_name, sizeof software_name, "software%s", timer_cases[i].name + 8u);
        if (strcmp(name, software_name) == 0) {
            *hardware = 0;
            return &timer_cases[i];
        }
    }
    return NULL;
}

static int package_case_path(char *out, size_t capacity, const char *package,
                             const char *case_name, const char *extension)
{
    const int written = snprintf(out, capacity, "%s/%s.%s", package, case_name, extension);
    return written >= 0 && (size_t)written < capacity ? 0 : -1;
}

static int create_no_completion_image(const char *source_path, const image_labels *labels,
                                      char *out_path, size_t capacity)
{
    const char *tmp = getenv("TMPDIR");
    uint8_t *bytes = NULL;
    size_t length = 0u;
    size_t offset;
    char pattern[4096];
    int fd;
    FILE *file;
    int close_status;
    if (tmp == NULL || labels == NULL ||
        snprintf(pattern, sizeof pattern, "%s/lynx-timer-no-completion-XXXXXX", tmp) >= (int)sizeof pattern ||
        read_file(source_path, &bytes, &length) != 0 || labels->done_store < 0x0200u) {
        free(bytes);
        return -1;
    }
    offset = 10u + (size_t)(labels->done_store - 0x0200u);
    if (offset > length || length - offset < 3u || strlen(pattern) + 1u > capacity) {
        free(bytes);
        return -1;
    }
    fd = mkstemp(pattern);
    if (fd < 0) { free(bytes); return -1; }
    bytes[offset] = 0xEAu;
    bytes[offset + 1u] = 0xEAu;
    bytes[offset + 2u] = 0xEAu;
    file = fdopen(fd, "wb");
    if (file == NULL) {
        (void)close(fd);
        (void)unlink(pattern);
        free(bytes);
        return -1;
    }
    if (fwrite(bytes, 1u, length, file) != length) {
        (void)fclose(file);
        (void)unlink(pattern);
        free(bytes);
        return -1;
    }
    close_status = fclose(file);
    if (close_status != 0) {
        (void)unlink(pattern);
        free(bytes);
        return -1;
    }
    (void)strcpy(out_path, pattern);
    free(bytes);
    return 0;
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

static int validate_font_rom(const lynx_machine *machine, uint32_t address)
{
    static const char characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ= ";
    uint8_t bytes[38u * 10u];
    if (lynx_read_memory(machine, address, bytes, sizeof bytes) != LYNX_OK) return -1;
    for (size_t glyph = 0u; glyph < sizeof characters - 1u; glyph++) {
        const uint8_t *expected;
        if (glyph_rows((unsigned char)characters[glyph], &expected) != 0) return -1;
        for (size_t y = 0u; y < 5u; y++) {
            for (size_t x = 0u; x < 4u; x++) {
                const uint8_t packed = bytes[glyph * 10u + y * 2u + x / 2u];
                const unsigned pen = (x & 1u) != 0u ? packed & 0x0Fu : packed >> 4;
                const unsigned bit = (expected[y] >> (3u - x)) & 1u;
                if (pen != (bit != 0u ? 15u : 0u)) return -1;
            }
        }
    }
    return 0;
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
                               const uint8_t results[18], int hardware,
                               const timer_case *settings, uint8_t model)
{
    static const char hex[] = "0123456789ABCDEF";
    char header[32];
    const unsigned dispctl = hardware ? (settings->dma ? 0x0Du : 0x0Cu) :
        (settings->dma ? 0x01u : 0x00u);
    unsigned i;
    (void)snprintf(header, sizeof header, "TIMER D%uP%u %c%02X", settings->dma, settings->page,
                   hardware ? 'H' : 'S', dispctl);
    if (decode_text(pens, row_count, header, 0u, 0u) != 0) return -1;
    (void)snprintf(header, sizeof header, "INIT00 DONEA5 CPU%02X", model);
    if (decode_text(pens, row_count, header, 0u, 6u) != 0) return -1;
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

static FILE *exclusive_capture_file(const char *directory, const char *name,
                                    const char *profile, const char *suffix)
{
    char path[4096];
    int fd;
    FILE *file;
    if (snprintf(path, sizeof path, "%s/%s-%s.%s", directory, name, profile, suffix) >= (int)sizeof path)
        return NULL;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return NULL;
    file = fdopen(fd, "wb");
    if (file == NULL) (void)close(fd);
    return file;
}

static int create_capture_directory(const char *directory)
{
    char parent[4096];
    char *slash;
    char *canonical;
    const char *temporary = getenv("TMPDIR");
    uint8_t nonce[24];
    FILE *random;
    FILE *owner;
    if (temporary == NULL || directory[0] != '/' || strlen(directory) >= sizeof parent ||
        strpbrk(directory, "\r\n\t") != NULL) return -1;
    for (const unsigned char *byte = (const unsigned char *)directory; *byte != 0u; byte++)
        if (*byte < 32u || *byte == 127u) return -1;
    (void)strcpy(parent, directory);
    slash = strrchr(parent, '/');
    if (slash == NULL || slash[1] == '\0' || strcmp(slash + 1, ".") == 0 ||
        strcmp(slash + 1, "..") == 0) return -1;
    *slash = '\0';
    canonical = realpath(temporary, NULL);
    if (canonical == NULL) return -1;
    const int matches = strcmp(parent, canonical) == 0;
    free(canonical);
    if (!matches) return -1;
    random = fopen("/dev/urandom", "rb");
    if (random == NULL) return -1;
    const size_t got = fread(nonce, 1u, sizeof nonce, random);
    const int closed = fclose(random);
    if (got != sizeof nonce || closed != 0 || mkdir(directory, 0700) != 0) return -1;
    owner = exclusive_capture_file(directory, ".owner", "software", "txt");
    if (owner == NULL) return -1;
    int ok = fprintf(owner, "lynx-timer-capture-v1\n%s\n", directory) >= 0;
    for (size_t i = 0u; i < sizeof nonce; i++) if (fprintf(owner, "%02x", nonce[i]) < 0) ok = 0;
    if (fputc('\n', owner) == EOF) ok = 0;
    if (fclose(owner) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int export_software_capture(const char *directory, const char *case_name,
                                   const char *profile, const lynx_identity *identity,
                                   const uint8_t *pens, unsigned rows, const uint8_t *results,
                                   uint8_t model, uint8_t requested, uint8_t mapctl)
{
    FILE *image_file = exclusive_capture_file(directory, case_name, profile, "pgm");
    if (image_file == NULL) return -1;
    int ok = fprintf(image_file, "P5\n160 %u\n255\n", rows) >= 0;
    for (unsigned y = 0u; y < rows; y++) {
        for (unsigned x = 0u; x < 160u; x++) {
            const unsigned pen = frame_pixel(pens, x, y);
            if ((pen != 0u && pen != 15u) || fputc(pen == 15u ? 255 : 0, image_file) == EOF) ok = 0;
        }
    }
    if (fclose(image_file) != 0) ok = 0;
    if (!ok) return -1;
    FILE *record = exclusive_capture_file(directory, case_name, profile, "txt");
    if (record == NULL) return -1;
    ok = fprintf(record, "family=software01\nphysical_execution=false\naccuracy=unverified\ncase=%s\nprofile_input=%s\nguest_probe=%02X\ninitial_counter=00\ncompletion=A5\nrequested_dispctl=%02X\nrequested_mapctl=%02X\ncase_set_sha256=%s\nobject_sha256=",
                 case_name, profile, model, requested, mapctl, TIMER_CASE_SET_SHA256) >= 0;
    for (size_t i = 0u; i < sizeof identity->content_sha256; i++)
        if (fprintf(record, "%02x", identity->content_sha256[i]) < 0) ok = 0;
    if (fputs("\nraw_results=", record) == EOF) ok = 0;
    for (size_t i = 0u; i < 18u; i++) if (fprintf(record, "%02X", results[i]) < 0) ok = 0;
    if (fputc('\n', record) == EOF) ok = 0;
    if (fclose(record) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int run_image(const char *kind, const char *bll_path, const char *labels_path,
                     const char *profile_name, const char *case_record,
                     const char *case_name, const timer_case *settings,
                     int check_mode, const char *captures)
{
    uint8_t *bll = NULL;
    size_t bll_length = 0u;
    image_labels labels;
    lynx_config config;
    lynx_machine *machine = NULL;
    void *machine_memory = NULL;
    lynx_identity identity;
    uint32_t profile;
    uint64_t remaining = check_mode == RUN_WITHOUT_EXECUTION ? 0u : UINT64_C(12000000);
    uint64_t total_used = 0u;
    lynx_stop_reason stop = LYNX_STOP_BUDGET;
    lynx_status status = LYNX_OK;
    lynx_watch_event watch;
    int expected_hardware = strcmp(kind, "hardware") == 0;
    const uint8_t expected_dispctl = (uint8_t)(expected_hardware ?
        (settings != NULL && settings->dma ? 0x0Du : 0x0Cu) :
        (settings != NULL && settings->dma ? 0x01u : 0x00u));
    const uint8_t expected_mapctl = (uint8_t)(settings != NULL && settings->page ? 0x0Cu : 0x8Cu);
    int completed = 0;
    uint8_t result_bytes[18];
    uint8_t signature = 0u, done = 0u, initial = 0u, requested = 0u, mapctl = 0u, progress = 0u;
    uint8_t model = 0u;
    size_t i;

    if (case_name == NULL) case_name = "standalone-unqualified";
    if (strcmp(profile_name, "lynx1") == 0) profile = LYNX_PROFILE_LYNX1;
    else if (strcmp(profile_name, "lynx2") == 0) profile = LYNX_PROFILE_LYNX2;
    else return fail("profile must be lynx1 or lynx2");
    if (settings == NULL || (!expected_hardware && strcmp(kind, "software") != 0) ||
        read_labels(labels_path, &labels) != 0 ||
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
        identity.content_addr != 0x0200u || identity.startup_tier != LYNX_STARTUP_A ||
        validate_font_rom(machine, labels.glyphs) != 0) {
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return fail("loaded image identity differs from the requested profile or BS93 object");
    }
    if (case_record != NULL && validate_case_record(case_record, case_name, expected_hardware, settings, &identity) != 0) {
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        return 5; /* The loaded object cannot satisfy its declared case record. */
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
        return !expected_hardware && status == LYNX_OK && completed == 0 &&
            stop == LYNX_STOP_BUDGET ? 7 : 1;
    }

    if (expected_hardware) {
        uint32_t pc = 0u;
        (void)lynx_read_register(machine, LYNX_REG_PC, &pc);
        if (pc != labels.after_dispctl || guest_byte(machine, labels.done, &done) != 0 || done != 0u ||
            guest_byte(machine, labels.completion, &signature) != 0 || signature != 0u ||
            guest_byte(machine, labels.requested_dispctl, &requested) != 0 || requested != expected_dispctl ||
            guest_byte(machine, labels.requested_mapctl, &mapctl) != 0 || mapctl != expected_mapctl ||
            lynx_watch_get(machine, &watch) != LYNX_OK || watch.hit != 0u) {
            lynx_machine_deinit(machine);
            free(machine_memory);
            free(bll);
            return fail("hardware0D did not visibly refuse its requested display mode before completion");
        }
        printf("hardware-family-refusal profile=%s case=%s stop=UNAVAILABLE pc=%04" PRIX32 " requested_dispctl=%02X requested_mapctl=%02X completion=missing\n",
               profile_name, case_name, pc, requested, mapctl);
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
            guest_byte(machine, labels.model, &model) == 0 &&
            lynx_read_memory(machine, labels.result_start, result_bytes, sizeof result_bytes) == LYNX_OK;
        if (watch_status != LYNX_OK || watch.hit != 1u || watch.address != labels.done ||
            watch.value != 0xA5u || read_ok == 0 || done != 0xA5u || signature != 0xA5u ||
            initial != 0u || requested != expected_dispctl || mapctl != expected_mapctl || progress != 0x08u ||
            model != (profile == LYNX_PROFILE_LYNX1 ? 0xA1u : 0xA2u)) {
            fprintf(stderr, "tracer fields: watch=%u/%" PRIu32 "/%02" PRIX32 " read=%d done=%02X sig=%02X init=%02X disp=%02X map=%02X progress=%02X result=%04" PRIX32 "\n",
                    watch.hit, watch.address, watch.value, read_ok, done, signature, initial, requested, mapctl, progress, labels.result_start);
        } else {
            goto software_fields_valid;
        }
        lynx_machine_deinit(machine);
        free(machine_memory);
        free(bll);
        (void)fail("software result capture, initial state or completion record is invalid");
        return 6;
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
            if (frame.stop == LYNX_STOP_FRAME && frame.sequence > 0u &&
                frame.row_count == LYNX_FRAME_MAX_ROWS && line_ticks[0] >= watch.return_tick) break;
            if (frame.ticks_consumed == 0u) break;
        }
        if (status != LYNX_OK || frame.stop != LYNX_STOP_FRAME || frame.sequence == 0u ||
            frame.row_count != LYNX_FRAME_MAX_ROWS || line_ticks[0] < watch.return_tick) {
            lynx_machine_deinit(machine);
            free(machine_memory);
            free(bll);
            return fail("software completion succeeded but no raw video frame was captured");
        }
        for (uint32_t row = 0u; row < frame.row_count; row++) {
            const uint8_t *palette = palettes + (size_t)row * 32u;
            if (palette[0] != 0u || palette[16] != 0u ||
                palette[15] != 0x0Fu || palette[31] != 0xFFu) {
                lynx_machine_deinit(machine); free(machine_memory); free(bll);
                return fail("captured display palette does not make glyph pens white on black");
            }
        }
        if (decode_timer_screen(pens, frame.row_count, result_bytes, 0, settings, model) != 0) {
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
        if (check_mode == RUN_DECODER_CONTROLS) {
            uint8_t altered_results[18];
            uint8_t altered_pens[LYNX_FRAME_PENS_CAPACITY];
            memcpy(altered_results, result_bytes, sizeof altered_results);
            altered_results[0] ^= 1u;
            if (decode_timer_screen(pens, frame.row_count, altered_results, 0, settings, model) == 0) {
                lynx_machine_deinit(machine); free(machine_memory); free(bll);
                return fail("result-byte mutation escaped independent raster comparison");
            }
            memcpy(altered_pens, pens, sizeof pens);
            altered_pens[0] ^= 0xF0u;
            if (decode_timer_screen(altered_pens, frame.row_count, result_bytes, 0, settings, model) == 0) {
                lynx_machine_deinit(machine); free(machine_memory); free(bll);
                return fail("framebuffer mutation escaped independent glyph comparison");
            }
            puts("control=result-byte-corruption-rejected raster-corruption-rejected");
            if (decode_timer_screen(pens, frame.row_count, result_bytes, 0, settings,
                                    (uint8_t)(model ^ 3u)) == 0) {
                lynx_machine_deinit(machine); free(machine_memory); free(bll);
                return fail("wrong guest CPU probe escaped raster comparison");
            }
            puts("control=guest-probe-record-corruption-rejected");
        }
        printf("raster_decode=passed captured_rows=18 result14=%02X\n", result_bytes[14]);
        if (captures != NULL && export_software_capture(captures, case_name, profile_name,
                &identity, pens, frame.row_count, result_bytes, model, requested, mapctl) != 0) {
            lynx_machine_deinit(machine); free(machine_memory); free(bll);
            return fail("exclusive software capture export failed");
        }
        printf("frame sequence=%" PRIu64 " rows=%" PRIu32 " pens_sha_preview=", frame.sequence, frame.row_count);
        for (i = 0u; i < 8u; i++) printf("%02X", pens[i]);
        putchar('\n');
    }
    printf("software01-complete profile=%s guest_probe=%02X case=%s ticks=%" PRIu64 " init_counter=%02X requested_mapctl=%02X requested_dispctl=%02X results=",
           profile_name, model, case_name, total_used, initial, mapctl, requested);
    for (i = 0u; i < sizeof result_bytes; i++) printf("%02X", result_bytes[i]);
    putchar('\n');
    lynx_machine_deinit(machine);
    free(machine_memory);
    free(bll);
    return 0;
}

enum physical_sheet_result { PHYSICAL_SHEET_VALID = 0, PHYSICAL_SHEET_REJECTED = 1, PHYSICAL_SHEET_MISSING = 3 };

static const char physical_header[] =
    "case\trun\tcase_set_sha256\tobject_sha256\tsetup\trequested_dispctl\trequested_mapctl"
    "\tobserved_dispctl\tobserved_mapctl\tmodel\tloader\tinitial_counter\traw_results"
    "\tcompletion\tphoto_reference\tevidence_kind";

static int split_tsv(char *line, char *fields[16])
{
    size_t count = 1u;
    char *cursor;
    fields[0] = line;
    for (cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor == '\t') {
            *cursor = '\0';
            if (count >= 16u) return -1;
            fields[count++] = cursor + 1;
        }
    }
    return (int)count;
}

static int is_hex_text(const char *value, size_t length, int lowercase_only)
{
    size_t i;
    if (strlen(value) != length) return 0;
    for (i = 0u; i < length; i++) {
        const char c = value[i];
        const int digit = c >= '0' && c <= '9';
        const int lower = c >= 'a' && c <= 'f';
        const int upper = c >= 'A' && c <= 'F';
        if (!(digit || lower || (!lowercase_only && upper))) return 0;
    }
    return 1;
}

static int read_sheet_line(FILE *file, char *line, size_t capacity)
{
    size_t used = 0u;
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') { line[used] = '\0'; return 1; }
        if (c == '\r') {
            if (fgetc(file) != '\n') return -1;
            line[used] = '\0'; return 1;
        }
        if ((c < 32 && c != '\t') || c == 127 || used + 1u >= capacity) return -1;
        line[used++] = (char)c;
    }
    line[used] = '\0';
    return ferror(file) != 0 ? -1 : (used != 0u ? 1 : 0);
}

static int validate_physical_sheet(FILE *file, const char *expected_kind,
                                  char *reason, size_t reason_capacity)
{
    char line[2048];
    unsigned char seen[4][3] = {{0u}};
    unsigned rows = 0u, complete_rows = 0u;
    int line_status;
    if (file == NULL || reason == NULL || reason_capacity == 0u) return PHYSICAL_SHEET_REJECTED;
    if (read_sheet_line(file, line, sizeof line) != 1) {
        (void)snprintf(reason, reason_capacity, "missing header");
        return PHYSICAL_SHEET_REJECTED;
    }
    if (strcmp(line, physical_header) != 0) {
        (void)snprintf(reason, reason_capacity, "header mismatch");
        return PHYSICAL_SHEET_REJECTED;
    }
    while ((line_status = read_sheet_line(file, line, sizeof line)) == 1) {
        char *fields[16];
        int field_count;
        size_t i;
        const timer_case *settings;
        int hardware = 0;
        unsigned run;
        rows++;
        if (rows > 12u) {
            (void)snprintf(reason, reason_capacity, "more than twelve result rows");
            return PHYSICAL_SHEET_REJECTED;
        }
        field_count = split_tsv(line, fields);
        if (field_count != 16) {
            (void)snprintf(reason, reason_capacity, "row column count mismatch");
            return PHYSICAL_SHEET_REJECTED;
        }
        if (fields[0][0] == '\0' && fields[1][0] == '\0') {
            for (i = 2u; i < 16u; i++) {
                if (fields[i][0] != '\0') {
                    (void)snprintf(reason, reason_capacity, "result fields have no case/run identity");
                    return PHYSICAL_SHEET_REJECTED;
                }
            }
            continue;
        }
        settings = find_case(fields[0], &hardware);
        if (settings == NULL || !hardware) {
            (void)snprintf(reason, reason_capacity, "unknown or non-hardware case");
            return PHYSICAL_SHEET_REJECTED;
        }
        if (strcmp(fields[1], "1") == 0) run = 0u;
        else if (strcmp(fields[1], "2") == 0) run = 1u;
        else if (strcmp(fields[1], "3") == 0) run = 2u;
        else {
            (void)snprintf(reason, reason_capacity, "unknown run number");
            return PHYSICAL_SHEET_REJECTED;
        }
        {
            const size_t case_index = (size_t)(settings - timer_cases);
            if (seen[case_index][run] != 0u) {
                (void)snprintf(reason, reason_capacity, "duplicate case/run pair");
                return PHYSICAL_SHEET_REJECTED;
            }
            seen[case_index][run] = 1u;
        }
        for (i = 0u; i < 16u; i++) {
            if (fields[i][0] == '\0') {
                (void)snprintf(reason, reason_capacity, "incomplete physical result row");
                return PHYSICAL_SHEET_MISSING;
            }
        }
        {
            const unsigned requested_dispctl = settings->dma ? 0x0Du : 0x0Cu;
            const unsigned requested_mapctl = settings->page ? 0x0Cu : 0x8Cu;
            char expected_dispctl[3], expected_mapctl[3];
            (void)snprintf(expected_dispctl, sizeof expected_dispctl, "%02X", requested_dispctl);
            (void)snprintf(expected_mapctl, sizeof expected_mapctl, "%02X", requested_mapctl);
            if (!is_hex_text(fields[2], 64u, 1) || !is_hex_text(fields[3], 64u, 1) ||
                strcmp(fields[5], expected_dispctl) != 0 || strcmp(fields[6], expected_mapctl) != 0 ||
                (strcmp(fields[7], "unknown") != 0 && !is_hex_text(fields[7], 2u, 0)) ||
                (strcmp(fields[8], "unknown") != 0 && !is_hex_text(fields[8], 2u, 0)) ||
                !is_hex_text(fields[11], 2u, 0) || !is_hex_text(fields[12], 36u, 0) ||
                !is_hex_text(fields[13], 2u, 0)) {
                (void)snprintf(reason, reason_capacity, "physical result identity or hexadecimal field invalid");
                return PHYSICAL_SHEET_REJECTED;
            }
        }
        if (strcmp(fields[15], expected_kind) != 0) {
            (void)snprintf(reason, reason_capacity, "software-only control row cannot qualify physical evidence");
            return PHYSICAL_SHEET_REJECTED;
        }
        if (strcmp(expected_kind, "physical") == 0 &&
            (strcmp(fields[9], "software-control") == 0 ||
             strcmp(fields[10], "software-control") == 0 || strcmp(fields[14], "not-a-photo") == 0)) {
            (void)snprintf(reason, reason_capacity, "synthetic placeholders cannot qualify physical evidence");
            return PHYSICAL_SHEET_REJECTED;
        }
        {
            const char *approved = approved_image_hash(fields[0]);
            if (approved == NULL || strcmp(fields[2], TIMER_CASE_SET_SHA256) != 0 ||
                strcmp(fields[3], approved) != 0) {
                (void)snprintf(reason, reason_capacity, "physical row does not bind the approved case-set/object");
                return PHYSICAL_SHEET_REJECTED;
            }
            if (strcmp(fields[9], "unknown") == 0 || strcmp(fields[10], "unknown") == 0) {
                (void)snprintf(reason, reason_capacity, "hardware model or loader evidence missing");
                return PHYSICAL_SHEET_MISSING;
            }
            if (strcmp(fields[11], "00") != 0 || strcmp(fields[13], "A5") != 0) {
                (void)snprintf(reason, reason_capacity, "initial counter or completion precondition failed");
                return PHYSICAL_SHEET_REJECTED;
            }
        }
        complete_rows++;
    }
    if (line_status < 0) {
        (void)snprintf(reason, reason_capacity, "invalid or unreadable result line");
        return PHYSICAL_SHEET_REJECTED;
    }
    if (rows != 12u || complete_rows != 12u) {
        (void)snprintf(reason, reason_capacity, "physical evidence missing: %u/12 complete rows", complete_rows);
        return PHYSICAL_SHEET_MISSING;
    }
    (void)snprintf(reason, reason_capacity, "twelve unique case/run rows are structurally complete; origin unverified");
    return PHYSICAL_SHEET_VALID;
}

static int write_empty_physical_rows(FILE *file, unsigned count)
{
    unsigned row;
    if (fputs(physical_header, file) == EOF || fputc('\n', file) == EOF) return -1;
    for (row = 0u; row < count; row++) {
        unsigned column;
        for (column = 0u; column < 16u; column++) {
            if (column != 0u && fputc('\t', file) == EOF) return -1;
        }
        if (fputc('\n', file) == EOF) return -1;
    }
    return 0;
}

static int write_software_control_rows(FILE *file, int duplicate, int fault)
{
    unsigned row;
    if (fputs(physical_header, file) == EOF || fputc('\n', file) == EOF) return -1;
    for (row = 0u; row < 12u; row++) {
        unsigned case_index = row / 3u;
        unsigned run_number = row % 3u + 1u;
        const timer_case *settings;
        const unsigned dispctl = case_index >= 2u ? 0x0Du : 0x0Cu;
        const unsigned mapctl = (case_index & 1u) != 0u ? 0x0Cu : 0x8Cu;
        if (duplicate && row == 1u) { case_index = 0u; run_number = 1u; }
        settings = &timer_cases[case_index];
        if (fprintf(file, "%s\t%u\t%s\t%s\tsoftware-control setup\t%02X\t%02X\tunknown\tunknown\t%s\tsoftware-control\t%s\t%s\t%s\tnot-a-photo\tsoftware-control\n",
                    settings->name, run_number, fault == 1 ? approved_image_hash(settings->name) : TIMER_CASE_SET_SHA256,
                    fault == 2 ? TIMER_CASE_SET_SHA256 : approved_image_hash(settings->name), dispctl, mapctl,
                    fault == 5 ? "unknown" : "software-control", fault == 3 ? "01" : "00",
                    "000000000000000000000000000000000000", fault == 4 ? "00" : "A5") < 0) return -1;
    }
    return 0;
}

static int physical_expect(FILE *file, int expected, const char *expected_reason,
                           const char *label, const char *expected_kind)
{
    char reason[160];
    int actual;
    rewind(file);
    actual = validate_physical_sheet(file, expected_kind, reason, sizeof reason);
    if (actual != expected || strstr(reason, expected_reason) == NULL) {
        fprintf(stderr, "physical control %s: result=%d reason=%s\n", label, actual, reason);
        (void)fclose(file);
        return -1;
    }
    (void)fclose(file);
    printf("control=sheet-%s status=%s reason=%s\n", label,
           actual == PHYSICAL_SHEET_VALID ? "software-schema-positive-only" : "rejected", reason);
    return 0;
}

static int physical_controls_self_test(void)
{
    FILE *file;
    int fault;
    for (fault = 1; fault <= 5; fault++) {
        const int expected = fault == 5 ? PHYSICAL_SHEET_MISSING : PHYSICAL_SHEET_REJECTED;
        const char *reason = fault <= 2 ? "approved case-set/object" :
                             (fault == 5 ? "model or loader evidence missing" : "precondition failed");
        file = tmpfile();
        if (file == NULL || write_software_control_rows(file, 0, fault) != 0 ||
            physical_expect(file, expected, reason, "identity-or-precondition", "software-control") != 0) return -1;
    }
    file = tmpfile();
    if (file == NULL || fputs(physical_header, file) == EOF || fputc('\0', file) == EOF ||
        fputs("hidden suffix\n", file) == EOF ||
        physical_expect(file, PHYSICAL_SHEET_REJECTED, "missing header", "embedded-nul", "physical") != 0) return -1;
    file = tmpfile();
    if (file == NULL || write_empty_physical_rows(file, 12u) != 0 ||
        physical_expect(file, PHYSICAL_SHEET_MISSING, "physical evidence missing: 0/12", "empty", "physical") != 0) return -1;
    file = tmpfile();
    if (file == NULL || fputs("case\trun\twrong header\n", file) == EOF ||
        physical_expect(file, PHYSICAL_SHEET_REJECTED, "header mismatch", "header", "physical") != 0) return -1;
    file = tmpfile();
    if (file == NULL || write_empty_physical_rows(file, 11u) != 0 ||
        physical_expect(file, PHYSICAL_SHEET_MISSING, "physical evidence missing: 0/12", "missing-row", "physical") != 0) return -1;
    /* This internal parser positive accepts only explicitly software-only
       synthetic rows. The physical-file CLI always requires physical kind. */
    file = tmpfile();
    if (file == NULL || write_software_control_rows(file, 0, 0) != 0 ||
        physical_expect(file, PHYSICAL_SHEET_VALID, "structurally complete", "synthetic-complete", "software-control") != 0) return -1;
    file = tmpfile();
    if (file == NULL || write_software_control_rows(file, 1, 0) != 0 ||
        physical_expect(file, PHYSICAL_SHEET_REJECTED, "duplicate case/run pair", "duplicate", "software-control") != 0) return -1;
    file = tmpfile();
    if (file == NULL || write_software_control_rows(file, 0, 0) != 0 ||
        physical_expect(file, PHYSICAL_SHEET_REJECTED, "software-only control row", "software-only", "physical") != 0) return -1;
    file = tmpfile();
    if (file == NULL || fputs(physical_header, file) == EOF || fputc('\n', file) == EOF ||
        fputs("unknown-case\t1\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n", file) == EOF ||
        physical_expect(file, PHYSICAL_SHEET_REJECTED, "unknown or non-hardware case", "unknown-case", "physical") != 0) return -1;
    file = tmpfile();
    if (file == NULL || fputs(physical_header, file) == EOF || fputc('\n', file) == EOF ||
        fputs("hardware-dma0-page0\t1\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n", file) == EOF ||
        physical_expect(file, PHYSICAL_SHEET_MISSING, "incomplete physical result row", "incomplete", "physical") != 0) return -1;
    puts("control=physical-strict-rows-passed");
    return 0;
}

static int run_package_case(const char *package, const char *case_name,
                           const char *profile, int decoder_controls, const char *captures)
{
    char bll[4096], labels[4096], record[4096];
    int hardware = 0;
    const timer_case *settings = find_case(case_name, &hardware);
    if (settings == NULL || package_case_path(bll, sizeof bll, package, case_name, "bll") != 0 ||
        package_case_path(labels, sizeof labels, package, case_name, "lbl") != 0 ||
        package_case_path(record, sizeof record, package, case_name, "case.txt") != 0) {
        return fail("unknown package case or path too long");
    }
    return run_image(hardware ? "hardware" : "software", bll, labels, profile,
                     record, case_name, settings, decoder_controls, captures);
}

static int run_tracer(const char *package)
{
    const char *profiles[] = {"lynx1", "lynx2"};
    const char *cases[] = {"hardware-dma1-page1", "software-dma1-page1"};
    size_t i, j;
    for (i = 0u; i < sizeof profiles / sizeof profiles[0]; i++)
        for (j = 0u; j < sizeof cases / sizeof cases[0]; j++)
            if (run_package_case(package, cases[j], profiles[i], 0, NULL) != 0) return 1;
    return 0;
}

static int run_matrix(const char *package, const char *captures)
{
    static const char *const profiles[] = {"lynx1", "lynx2"};
    size_t case_index, profile_index;
    char case_name[64];
    for (case_index = 0u; case_index < sizeof timer_cases / sizeof timer_cases[0]; case_index++) {
        const timer_case *settings = &timer_cases[case_index];
        for (profile_index = 0u; profile_index < sizeof profiles / sizeof profiles[0]; profile_index++) {
            (void)snprintf(case_name, sizeof case_name, "%s", settings->name);
            if (run_package_case(package, case_name, profiles[profile_index], 0, captures) != 0) return 1;
            (void)snprintf(case_name, sizeof case_name, "software%s", settings->name + 8u);
            if (run_package_case(package, case_name, profiles[profile_index], 0, captures) != 0) return 1;
        }
    }
    puts("matrix=4 hardware-mode refusals + 4 software companions x 2 CPU profiles passed");
    return 0;
}

static int run_controls(const char *package)
{
    const char *software_case = "software-dma1-page1";
    const char *hardware_case = "hardware-dma1-page1";
    char software_bll[4096], hardware_bll[4096], labels[4096], record[4096];
    char mutated[4096];
    image_labels parsed_labels;
    const timer_case *settings = &timer_cases[3];
    int rc;
    if (run_package_case(package, software_case, "lynx1", 1, NULL) != 0) return 1;
    if (package_case_path(software_bll, sizeof software_bll, package, software_case, "bll") != 0 ||
        package_case_path(hardware_bll, sizeof hardware_bll, package, hardware_case, "bll") != 0 ||
        package_case_path(labels, sizeof labels, package, software_case, "lbl") != 0 ||
        package_case_path(record, sizeof record, package, software_case, "case.txt") != 0)
        return fail("control package path is too long");
    if (run_image("software", software_bll, labels, "lynx1", record,
                  hardware_case, settings, RUN_CAPTURE, NULL) != 5)
        return fail("case-swap control accepted a hardware case name for a software image");
    puts("control=case-swap-rejected");
    if (run_image("hardware", hardware_bll, labels, "lynx1", record,
                  software_case, settings, RUN_CAPTURE, NULL) != 5)
        return fail("cross-family case identity control accepted a swapped record");
    puts("control=cross-family-object-record-swap-rejected");

    if (read_labels(labels, &parsed_labels) != 0 ||
        create_no_completion_image(software_bll, &parsed_labels, mutated, sizeof mutated) != 0)
        return fail("could not create an isolated no-completion image control");
    rc = run_image("software", mutated, labels, "lynx1", NULL,
                   software_case, settings, 0, NULL);
    (void)unlink(mutated);
    if (rc != 7) return fail("no-completion control did not stop at the fixed guest budget");
    puts("control=missing-completion-rejected");

    if (run_image("software", software_bll, labels, "lynx1", record,
                  software_case, settings, RUN_WITHOUT_EXECUTION, NULL) != 7)
        return fail("fresh loaded no-run control escaped without execution");
    puts("control=no-execution-fresh-loaded-machine-rejected ticks=0");

    if (snprintf(mutated, sizeof mutated, "%s/no-run-control.bll", package) >= (int)sizeof mutated ||
        run_image("software", mutated, "/timer-evidence/no-run.lbl",
                  "lynx1", NULL, software_case, settings, 0, NULL) == 0)
        return fail("missing-run control accepted absent files");
    puts("control=missing-inputs-rejected");

    if (physical_controls_self_test() != 0) return fail("physical result sheet negative controls failed");
    return 0;
}

static int run_physical_file(const char *path)
{
    FILE *file = fopen(path, "r");
    char reason[160];
    int result;
    if (file == NULL) {
        fprintf(stderr, "timer evidence: physical result sheet cannot be opened\n");
        return 2;
    }
    result = validate_physical_sheet(file, "physical", reason, sizeof reason);
    (void)fclose(file);
    if (result == PHYSICAL_SHEET_VALID) {
        printf("physical evidence status: %s\n", reason);
        return 0;
    }
    fprintf(stderr, "timer evidence: %s\n", reason);
    return result;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--tracer") == 0) {
        return run_tracer(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "--matrix") == 0) return run_matrix(argv[2], NULL);
    if (argc == 4 && strcmp(argv[1], "--export-software") == 0) {
        if (create_capture_directory(argv[3]) != 0) return fail("capture directory must be a fresh direct child of canonical TMPDIR");
        return run_matrix(argv[2], argv[3]);
    }
    if (argc == 3 && strcmp(argv[1], "--controls") == 0) return run_controls(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--validate-physical") == 0) return run_physical_file(argv[2]);
    if (argc != 5 || (strcmp(argv[1], "--software") != 0 && strcmp(argv[1], "--hardware") != 0)) {
        fprintf(stderr, "usage: lynx-test-timer-evidence --tracer|--matrix|--controls PACKAGE | --export-software PACKAGE FRESH_DIR | --validate-physical SHEET | --software|--hardware IMAGE.bll IMAGE.lbl lynx1|lynx2\n");
        return 2;
    }
    return run_image(strcmp(argv[1], "--hardware") == 0 ? "hardware" : "software",
                     argv[2], argv[3], argv[4], NULL, NULL, &timer_cases[3], 0, NULL);
}
