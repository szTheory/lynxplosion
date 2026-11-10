/* A strict, allocation-free reader for SingleStepTests 65x02 vector data. It is
   test-side code: the core never sees it.

   Two entry points over a caller-owned buffer:
   - sample mode: the committed container (three header lines, then 256 sections
     "#file <op>.json lines <L>" followed by exactly L upstream lines each, the
     last object still ending in its upstream comma, no closing bracket);
   - complete mode: one whole upstream file, either physical layout, exactly
     10000 objects and a closing bracket.

   Objects are parsed with a fixed key order (name, initial, final, cycles; pc,
   s, a, x, y, p, ram), whitespace-insensitive, with range-checked decimal
   numbers and fixed bounds (64 ram entries, 16 cycles). Anything else is an
   error with a located message: sst_next returns a negative value and the
   message names the label, the opcode and the vector ordinal (0-based index of
   the object inside its file or section). The reader never calls stdio. */
#ifndef LYNX_TESTS_CPU_SST_READER_H
#define LYNX_TESTS_CPU_SST_READER_H

#include <stddef.h>
#include <stdint.h>

enum {
    SST_MAX_RAM = 64,
    SST_MAX_CYCLES = 16,
    SST_SAMPLE_SECTIONS = 256,
    SST_SAMPLE_OBJECTS = 32,
    SST_FILE_OBJECTS = 10000,
    SST_MSG = 240,
    SST_NAME = 32
};

typedef struct sst_state {
    uint16_t pc;
    uint8_t s, a, x, y, p;
    unsigned nram;
    uint16_t ram_addr[SST_MAX_RAM];
    uint8_t ram_val[SST_MAX_RAM];
} sst_state;

typedef struct sst_cycle {
    uint16_t addr;
    uint8_t value;
    uint8_t write; /* 1 for "write", 0 for "read" */
} sst_cycle;

typedef struct sst_vector {
    unsigned opcode;  /* the file or section opcode */
    unsigned ordinal; /* 0-based index inside the file or section */
    char name[SST_NAME];
    sst_state initial, final;
    unsigned ncycles;
    sst_cycle cycles[SST_MAX_CYCLES];
} sst_vector;

typedef struct sst_reader {
    const char *buf;
    size_t len;
    size_t pos;
    size_t end;           /* end of the current extent (section body or file) */
    const char *label;    /* names the input in messages, caller-owned */
    int sample;           /* 1 sample mode, 0 complete mode */
    int partial;          /* sample mode: fewer than 256 sections are accepted */
    int state;            /* 0 between units, 1 inside a unit, 2 finished */
    int opcode;           /* -1 until a section header or the file opcode is known */
    int prev_opcode;      /* sample mode: opcode of the previous section, -1 before the first */
    int in_object;        /* a message names the vector ordinal while set */
    unsigned sections;    /* sections opened so far */
    unsigned objects;     /* objects completed in the current unit */
    unsigned total;       /* vectors returned so far */
    unsigned lines_decl;  /* the current section's declared line count */
    char commit[41];      /* sample mode: the 40-hex commit of the source line */
    char variant[40];     /* sample mode: "<variant>/v1" of the source line */
    size_t msglen;
    char msg[SST_MSG];
} sst_reader;

/* Sample mode: checks the three header lines. Returns 0, or -1 with r->msg set.
   partial != 0 accepts any ascending subset of sections (at least one). */
int sst_open_sample(sst_reader *r, const char *buf, size_t len, const char *label, int partial);

/* Complete mode for the file of the given opcode (0 to 255). */
void sst_open_file(sst_reader *r, const char *buf, size_t len, unsigned opcode, const char *label);

/* Next vector: 1 (filled in v), 0 at a clean end, -1 on an error (r->msg). After
   a 0 every section or the whole file was consumed and counted. */
int sst_next(sst_reader *r, sst_vector *v);

#endif /* LYNX_TESTS_CPU_SST_READER_H */
