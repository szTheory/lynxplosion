/* Strict reader for SingleStepTests 65x02 vector data; see sst_reader.h. No
   allocation, no stdio, no recursion: fixed arrays and an iterator. Messages are
   built by hand into the reader's own buffer. */

#include "cpu/sst_reader.h"

#include <string.h>

/* ---- messages ------------------------------------------------------------ */

static void msg_s(sst_reader *r, const char *s)
{
    while (*s != '\0' && r->msglen + 1u < (size_t)SST_MSG) {
        r->msg[r->msglen] = *s;
        r->msglen++;
        s++;
    }
    r->msg[r->msglen] = '\0';
}

static void msg_u(sst_reader *r, unsigned v)
{
    char tmp[12];
    unsigned n = 0;
    char out[12];
    unsigned i;
    do {
        tmp[n] = (char)('0' + (v % 10u));
        n++;
        v /= 10u;
    } while (v != 0u);
    for (i = 0; i < n; i++) {
        out[i] = tmp[n - 1u - i];
    }
    out[n] = '\0';
    msg_s(r, out);
}

static void msg_hex2(sst_reader *r, unsigned v)
{
    static const char digits[] = "0123456789abcdef";
    char out[3];
    out[0] = digits[(v >> 4) & 15u];
    out[1] = digits[v & 15u];
    out[2] = '\0';
    msg_s(r, out);
}

/* Builds "<label>: opcode xx vector N: <what><extra>" (the location parts only
   when known) and returns -1. */
static int fail2(sst_reader *r, const char *what, const char *extra)
{
    r->msglen = 0;
    r->msg[0] = '\0';
    msg_s(r, r->label);
    msg_s(r, ": ");
    if (r->opcode >= 0) {
        msg_s(r, "opcode ");
        msg_hex2(r, (unsigned)r->opcode);
        msg_s(r, " ");
        if (r->in_object != 0) {
            msg_s(r, "vector ");
            msg_u(r, r->objects);
            msg_s(r, ": ");
        } else {
            msg_s(r, "section: ");
        }
    }
    msg_s(r, what);
    msg_s(r, extra);
    return -1;
}

static int fail(sst_reader *r, const char *what)
{
    return fail2(r, what, "");
}

/* ---- scanning primitives ------------------------------------------------- */

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void skip_ws(sst_reader *r)
{
    while (r->pos < r->end && is_ws(r->buf[r->pos])) {
        r->pos++;
    }
}

static int expect_char(sst_reader *r, char c, const char *what)
{
    if (r->pos >= r->end) {
        return fail2(r, "truncated, expected ", what);
    }
    if (r->buf[r->pos] != c) {
        return fail2(r, "expected ", what);
    }
    r->pos++;
    return 0;
}

/* A decimal number: "0" or [1-9][0-9]*, no sign, at most max. */
static int parse_num(sst_reader *r, unsigned max, const char *field, unsigned *out)
{
    unsigned value = 0;
    unsigned digits = 0;
    char c;
    if (r->pos >= r->end) {
        return fail2(r, "truncated, expected number for ", field);
    }
    c = r->buf[r->pos];
    if (c < '0' || c > '9') {
        return fail2(r, "expected unsigned number for ", field);
    }
    if (c == '0' && r->pos + 1u < r->end && r->buf[r->pos + 1u] >= '0' && r->buf[r->pos + 1u] <= '9') {
        return fail2(r, "leading zero in ", field);
    }
    while (r->pos < r->end && r->buf[r->pos] >= '0' && r->buf[r->pos] <= '9') {
        digits++;
        if (digits > 6u) {
            return fail2(r, "number out of range for ", field);
        }
        value = value * 10u + (unsigned)(r->buf[r->pos] - '0');
        r->pos++;
    }
    if (value > max) {
        return fail2(r, "number out of range for ", field);
    }
    *out = value;
    return 0;
}

/* `"key"` ws `:` ws */
static int expect_key(sst_reader *r, const char *key)
{
    size_t n = strlen(key);
    skip_ws(r);
    if (r->pos >= r->end) {
        return fail2(r, "truncated, expected key ", key);
    }
    if (r->buf[r->pos] != '"' || r->pos + n + 2u > r->end || memcmp(r->buf + r->pos + 1u, key, n) != 0 ||
        r->buf[r->pos + 1u + n] != '"') {
        return fail2(r, "unknown, missing or duplicate key, expected ", key);
    }
    r->pos += n + 2u;
    skip_ws(r);
    if (expect_char(r, ':', "':'") != 0) {
        return -1;
    }
    skip_ws(r);
    return 0;
}

/* `"read"` or `"write"`: 0 or 1, or -1 with the message set. */
static int parse_direction(sst_reader *r, unsigned *out)
{
    static const char rd[] = "\"read\"";
    static const char wr[] = "\"write\"";
    if (r->pos + 6u <= r->end && memcmp(r->buf + r->pos, rd, 6u) == 0) {
        r->pos += 6u;
        *out = 0u;
        return 0;
    }
    if (r->pos + 7u <= r->end && memcmp(r->buf + r->pos, wr, 7u) == 0) {
        r->pos += 7u;
        *out = 1u;
        return 0;
    }
    return fail(r, "cycle direction is neither \"read\" nor \"write\"");
}

/* ---- objects ------------------------------------------------------------- */

static int parse_name(sst_reader *r, sst_vector *v)
{
    unsigned n = 0;
    if (expect_char(r, '"', "'\"' opening the name") != 0) {
        return -1;
    }
    for (;;) {
        char c;
        if (r->pos >= r->end) {
            return fail(r, "truncated inside the name");
        }
        c = r->buf[r->pos];
        if (c == '"') {
            r->pos++;
            break;
        }
        if (c < ' ' || c > '~' || c == '\\') {
            return fail(r, "illegal character in the name");
        }
        if (n + 1u >= (unsigned)SST_NAME) {
            return fail(r, "name too long");
        }
        v->name[n] = c;
        n++;
        r->pos++;
    }
    v->name[n] = '\0';
    /* the first token is the opcode as two lowercase hex digits */
    {
        static const char digits[] = "0123456789abcdef";
        unsigned op = (unsigned)r->opcode;
        if (n < 2u || v->name[0] != digits[(op >> 4) & 15u] || v->name[1] != digits[op & 15u] ||
            (n > 2u && v->name[2] != ' ')) {
            return fail(r, "the name's first token is not the file opcode");
        }
    }
    return 0;
}

static int parse_ram(sst_reader *r, sst_state *st)
{
    st->nram = 0;
    if (expect_char(r, '[', "'[' opening ram") != 0) {
        return -1;
    }
    skip_ws(r);
    if (r->pos < r->end && r->buf[r->pos] == ']') {
        r->pos++;
        return 0;
    }
    for (;;) {
        unsigned addr = 0;
        unsigned val = 0;
        if (st->nram >= (unsigned)SST_MAX_RAM) {
            return fail(r, "more than 64 ram entries");
        }
        if (expect_char(r, '[', "'[' opening a ram pair") != 0) {
            return -1;
        }
        skip_ws(r);
        if (parse_num(r, 65535u, "ram address", &addr) != 0) {
            return -1;
        }
        skip_ws(r);
        if (expect_char(r, ',', "',' in a ram pair") != 0) {
            return -1;
        }
        skip_ws(r);
        if (parse_num(r, 255u, "ram value", &val) != 0) {
            return -1;
        }
        skip_ws(r);
        if (expect_char(r, ']', "']' closing a ram pair") != 0) {
            return -1;
        }
        st->ram_addr[st->nram] = (uint16_t)addr;
        st->ram_val[st->nram] = (uint8_t)val;
        st->nram++;
        skip_ws(r);
        if (r->pos >= r->end) {
            return fail(r, "truncated inside ram");
        }
        if (r->buf[r->pos] == ',') {
            r->pos++;
            skip_ws(r);
            continue;
        }
        if (r->buf[r->pos] == ']') {
            r->pos++;
            return 0;
        }
        return fail(r, "expected ',' or ']' in ram");
    }
}

static int parse_state(sst_reader *r, sst_state *st)
{
    unsigned n = 0;
    skip_ws(r);
    if (expect_char(r, '{', "'{' opening a state") != 0) {
        return -1;
    }
    if (expect_key(r, "pc") != 0 || parse_num(r, 65535u, "pc", &n) != 0) {
        return -1;
    }
    st->pc = (uint16_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after pc") != 0 || expect_key(r, "s") != 0 || parse_num(r, 255u, "s", &n) != 0) {
        return -1;
    }
    st->s = (uint8_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after s") != 0 || expect_key(r, "a") != 0 || parse_num(r, 255u, "a", &n) != 0) {
        return -1;
    }
    st->a = (uint8_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after a") != 0 || expect_key(r, "x") != 0 || parse_num(r, 255u, "x", &n) != 0) {
        return -1;
    }
    st->x = (uint8_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after x") != 0 || expect_key(r, "y") != 0 || parse_num(r, 255u, "y", &n) != 0) {
        return -1;
    }
    st->y = (uint8_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after y") != 0 || expect_key(r, "p") != 0 || parse_num(r, 255u, "p", &n) != 0) {
        return -1;
    }
    st->p = (uint8_t)n;
    skip_ws(r);
    if (expect_char(r, ',', "',' after p") != 0 || expect_key(r, "ram") != 0 || parse_ram(r, st) != 0) {
        return -1;
    }
    skip_ws(r);
    return expect_char(r, '}', "'}' closing a state");
}

static int parse_cycles(sst_reader *r, sst_vector *v)
{
    v->ncycles = 0;
    if (expect_char(r, '[', "'[' opening cycles") != 0) {
        return -1;
    }
    skip_ws(r);
    if (r->pos < r->end && r->buf[r->pos] == ']') {
        r->pos++;
        return 0;
    }
    for (;;) {
        unsigned addr = 0;
        unsigned val = 0;
        unsigned dir = 0;
        if (v->ncycles >= (unsigned)SST_MAX_CYCLES) {
            return fail(r, "more than 16 cycles");
        }
        if (expect_char(r, '[', "'[' opening a cycle") != 0) {
            return -1;
        }
        skip_ws(r);
        if (parse_num(r, 65535u, "cycle address", &addr) != 0) {
            return -1;
        }
        skip_ws(r);
        if (expect_char(r, ',', "',' in a cycle") != 0) {
            return -1;
        }
        skip_ws(r);
        if (parse_num(r, 255u, "cycle value", &val) != 0) {
            return -1;
        }
        skip_ws(r);
        if (expect_char(r, ',', "',' in a cycle") != 0) {
            return -1;
        }
        skip_ws(r);
        if (parse_direction(r, &dir) != 0) {
            return -1;
        }
        skip_ws(r);
        if (expect_char(r, ']', "']' closing a cycle") != 0) {
            return -1;
        }
        v->cycles[v->ncycles].addr = (uint16_t)addr;
        v->cycles[v->ncycles].value = (uint8_t)val;
        v->cycles[v->ncycles].write = (uint8_t)dir;
        v->ncycles++;
        skip_ws(r);
        if (r->pos >= r->end) {
            return fail(r, "truncated inside cycles");
        }
        if (r->buf[r->pos] == ',') {
            r->pos++;
            skip_ws(r);
            continue;
        }
        if (r->buf[r->pos] == ']') {
            r->pos++;
            return 0;
        }
        return fail(r, "expected ',' or ']' in cycles");
    }
}

/* Whole-object checks that need both lists. */
static int check_object(sst_reader *r, const sst_vector *v)
{
    unsigned i;
    unsigned j;
    int found = 0;
    for (i = 0; i < v->initial.nram; i++) {
        for (j = i + 1u; j < v->initial.nram; j++) {
            if (v->initial.ram_addr[i] == v->initial.ram_addr[j]) {
                return fail(r, "duplicate initial.ram address");
            }
        }
        if (v->initial.ram_addr[i] == v->initial.pc) {
            found = 1;
            if ((unsigned)v->initial.ram_val[i] != (unsigned)r->opcode) {
                return fail(r, "initial.ram byte at pc is not the file opcode");
            }
        }
    }
    if (found == 0) {
        return fail(r, "initial.ram has no byte at pc");
    }
    return 0;
}

static int parse_object(sst_reader *r, sst_vector *v)
{
    r->in_object = 1;
    v->opcode = (unsigned)r->opcode;
    v->ordinal = r->objects;
    skip_ws(r);
    if (expect_char(r, '{', "'{' opening an object") != 0) {
        return -1;
    }
    if (expect_key(r, "name") != 0 || parse_name(r, v) != 0) {
        return -1;
    }
    skip_ws(r);
    if (expect_char(r, ',', "',' after name") != 0 || expect_key(r, "initial") != 0 || parse_state(r, &v->initial) != 0) {
        return -1;
    }
    skip_ws(r);
    if (expect_char(r, ',', "',' after initial") != 0 || expect_key(r, "final") != 0 || parse_state(r, &v->final) != 0) {
        return -1;
    }
    skip_ws(r);
    if (expect_char(r, ',', "',' after final") != 0 || expect_key(r, "cycles") != 0 || parse_cycles(r, v) != 0) {
        return -1;
    }
    skip_ws(r);
    if (expect_char(r, '}', "'}' closing an object") != 0) {
        return -1;
    }
    if (check_object(r, v) != 0) {
        return -1;
    }
    r->in_object = 0;
    return 0;
}

/* ---- sample container ---------------------------------------------------- */

static const char k_line1[] = "#sst65x02-sample v1";
static const char k_source_prefix[] = "#source https://github.com/SingleStepTests/65x02 ";
static const char k_rule[] = "#rule first 32 objects of each file, upstream bytes verbatim, files in opcode order";

/* End (index of the newline) of the line starting at pos, or len when none. */
static size_t line_end(const sst_reader *r, size_t pos)
{
    while (pos < r->len && r->buf[pos] != '\n') {
        pos++;
    }
    return pos;
}

static void reader_clear(sst_reader *r, const char *buf, size_t len, const char *label)
{
    memset(r, 0, sizeof *r);
    r->buf = buf;
    r->len = len;
    r->end = len;
    r->label = label;
    r->opcode = -1;
    r->prev_opcode = -1;
}

int sst_open_sample(sst_reader *r, const char *buf, size_t len, const char *label, int partial)
{
    size_t e;
    size_t n;
    size_t i;
    reader_clear(r, buf, len, label);
    r->sample = 1;
    r->partial = partial;

    e = line_end(r, r->pos);
    n = sizeof k_line1 - 1u;
    if (e >= len || e - r->pos != n || memcmp(buf + r->pos, k_line1, n) != 0) {
        return fail(r, "bad container line 1, expected '#sst65x02-sample v1'");
    }
    r->pos = e + 1u;

    e = line_end(r, r->pos);
    n = sizeof k_source_prefix - 1u;
    if (e >= len || e - r->pos < n + 40u + 1u + 4u || memcmp(buf + r->pos, k_source_prefix, n) != 0) {
        return fail(r, "bad container line 2, expected '#source <url> <commit> <variant>/v1'");
    }
    for (i = 0; i < 40u; i++) {
        char c = buf[r->pos + n + i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return fail(r, "the source line's commit is not 40 lowercase hex digits");
        }
        r->commit[i] = c;
    }
    r->commit[40] = '\0';
    n += 40u;
    if (buf[r->pos + n] != ' ') {
        return fail(r, "the source line has no variant after the commit");
    }
    n++;
    {
        size_t vl = e - (r->pos + n);
        if (vl < 4u || vl >= sizeof r->variant || memcmp(buf + r->pos + n + vl - 3u, "/v1", 3u) != 0) {
            return fail(r, "the source line's variant is not '<name>/v1'");
        }
        for (i = 0; i < vl; i++) {
            char c = buf[r->pos + n + i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c == '/' && i + 3u == vl))) {
                return fail(r, "the source line's variant has an illegal character");
            }
            r->variant[i] = c;
        }
        r->variant[vl] = '\0';
    }
    r->pos = e + 1u;

    e = line_end(r, r->pos);
    n = sizeof k_rule - 1u;
    if (e >= len || e - r->pos != n || memcmp(buf + r->pos, k_rule, n) != 0) {
        return fail(r, "bad container line 3, expected the sample rule line");
    }
    r->pos = e + 1u;
    return 0;
}

void sst_open_file(sst_reader *r, const char *buf, size_t len, unsigned opcode, const char *label)
{
    reader_clear(r, buf, len, label);
    r->sample = 0;
    r->opcode = (int)(opcode & 255u);
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}


/* Starts the next section: 1 opened, 0 no more sections, -1 error. */
static int begin_section(sst_reader *r)
{
    static const char hdr[] = "#file ";
    static const char mid[] = ".json lines ";
    size_t e;
    size_t p;
    size_t q;
    unsigned lines;
    unsigned declared = 0;
    unsigned digits = 0;
    int h1;
    int h2;
    unsigned op;

    r->in_object = 0;
    if (r->pos >= r->len) {
        if (r->partial == 0 && r->sections != (unsigned)SST_SAMPLE_SECTIONS) {
            return fail(r, "the sample ends before all 256 sections");
        }
        if (r->partial != 0 && r->sections == 0u) {
            return fail(r, "the sample holds no section");
        }
        return 0;
    }
    if (r->partial == 0 && r->sections == (unsigned)SST_SAMPLE_SECTIONS) {
        return fail(r, "trailing bytes after the last section");
    }
    e = line_end(r, r->pos);
    p = r->pos;
    if (e >= r->len || e - p < (sizeof hdr - 1u) + 2u + (sizeof mid - 1u) + 1u ||
        memcmp(r->buf + p, hdr, sizeof hdr - 1u) != 0) {
        return fail(r, "expected a '#file <op>.json lines <L>' header");
    }
    p += sizeof hdr - 1u;
    h1 = hex_val(r->buf[p]);
    h2 = hex_val(r->buf[p + 1u]);
    if (h1 < 0 || h2 < 0) {
        return fail(r, "the section header opcode is not two lowercase hex digits");
    }
    op = (unsigned)(h1 * 16 + h2);
    p += 2u;
    if (memcmp(r->buf + p, mid, sizeof mid - 1u) != 0) {
        return fail(r, "malformed section header");
    }
    p += sizeof mid - 1u;
    if (r->buf[p] == '0') {
        return fail(r, "the section header line count is zero or has a leading zero");
    }
    while (p < e && r->buf[p] >= '0' && r->buf[p] <= '9') {
        digits++;
        if (digits > 6u) {
            return fail(r, "the section header line count is out of range");
        }
        declared = declared * 10u + (unsigned)(r->buf[p] - '0');
        p++;
    }
    if (p != e || digits == 0u) {
        return fail(r, "malformed section header line count");
    }
    r->opcode = (int)op;
    if (r->partial == 0 ? (op != r->sections) : ((int)op <= r->prev_opcode)) {
        return fail(r, "sections are not in opcode order");
    }
    q = e + 1u;
    for (lines = 0; lines < declared; lines++) {
        size_t nl = line_end(r, q);
        if (nl >= r->len) {
            return fail(r, "the section is shorter than its 'lines' header");
        }
        q = nl + 1u;
    }
    r->pos = e + 1u;
    r->end = q;
    r->lines_decl = declared;
    r->sections++;
    r->prev_opcode = (int)op;
    r->objects = 0;
    return 1;
}

/* 1 object parsed into v, 0 the unit ended (state updated), -1 error. */
static int unit_object(sst_reader *r, sst_vector *v)
{
    r->in_object = 0;
    skip_ws(r);
    if (r->objects == 0u) {
        if (expect_char(r, '[', "'[' opening the vector list") != 0) {
            return -1;
        }
        skip_ws(r);
        if (r->pos >= r->end) {
            return fail(r, "no vectors after '['");
        }
    } else if (r->sample != 0) {
        if (r->pos < r->end && r->buf[r->pos] == ',') {
            r->pos++;
            skip_ws(r);
        } else if (r->pos < r->end) {
            return fail(r, "expected ',' or the end of the section after an object");
        }
        if (r->pos >= r->end) {
            if (r->objects != (unsigned)SST_SAMPLE_OBJECTS) {
                (void)fail(r, "the section holds a different object count than 32: ");
                msg_u(r, r->objects);
                return -1;
            }
            r->state = 0;
            return 0;
        }
    } else {
        if (r->pos >= r->end) {
            return fail(r, "the file ends without its closing ']'");
        }
        if (r->buf[r->pos] == ',') {
            r->pos++;
            skip_ws(r);
        } else if (r->buf[r->pos] == ']') {
            r->pos++;
            skip_ws(r);
            if (r->pos != r->end) {
                return fail(r, "trailing bytes after the closing ']'");
            }
            if (r->objects != (unsigned)SST_FILE_OBJECTS) {
                (void)fail(r, "the file holds a different object count than 10000: ");
                msg_u(r, r->objects);
                return -1;
            }
            r->state = 2;
            return 0;
        } else {
            return fail(r, "expected ',' or ']' after an object");
        }
    }
    if (r->objects >= (r->sample != 0 ? (unsigned)SST_SAMPLE_OBJECTS : (unsigned)SST_FILE_OBJECTS)) {
        return fail(r, "more objects than the unit may hold");
    }
    if (parse_object(r, v) != 0) {
        return -1;
    }
    r->objects++;
    r->total++;
    return 1;
}

int sst_next(sst_reader *r, sst_vector *v)
{
    for (;;) {
        int rc;
        if (r->state == 2) {
            return 0;
        }
        if (r->state == 0) {
            if (r->sample != 0) {
                rc = begin_section(r);
                if (rc < 0) {
                    return -1;
                }
                if (rc == 0) {
                    r->state = 2;
                    return 0;
                }
            } else {
                r->sections = 1;
                r->objects = 0;
            }
            r->state = 1;
        }
        rc = unit_object(r, v);
        if (rc != 0) {
            return rc;
        }
        /* the unit ended: a section moves on to the next header, a file is done */
    }
}
