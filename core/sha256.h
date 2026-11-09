/* SHA-256 (FIPS 180-4), owned code shared by the core and the runner.
   Adapted from muhtari (MIT, owner project) at commit 7fd0386. */
#ifndef LYNX_SHA256_H
#define LYNX_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct lynx__sha256 {
    uint32_t h[8];     /* intermediate hash value */
    uint8_t block[64]; /* bytes of the current, unfinished block */
    uint32_t used;     /* bytes in block */
    uint64_t total;    /* message length in bytes */
} lynx__sha256;

void lynx__sha256_init(lynx__sha256 *s);
void lynx__sha256_update(lynx__sha256 *s, const void *data, size_t len);
void lynx__sha256_final(lynx__sha256 *s, uint8_t digest[32]);

/* Writes the digest as 64 lowercase hex digits and a terminating NUL. */
void lynx__sha256_hex(const uint8_t digest[32], char hex[65]);

#endif /* LYNX_SHA256_H */
