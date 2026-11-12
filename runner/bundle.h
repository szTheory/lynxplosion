/* Artifact bundle writer for the headless runner (host side only).

   A bundle is a directory of text files: run.manifest, obs.ram.hex,
   obs.regs.hex, obs.state.hex, obs.time.hex and digests.txt. Nothing in it
   carries a timestamp, host name, path, wall time, flag or pointer value, so
   two runs of the same build with the same inputs write the same bytes.

   Every digest here measures repeatability (R), not accuracy. */
#ifndef LYNX_RUNNER_BUNDLE_H
#define LYNX_RUNNER_BUNDLE_H

#include <stddef.h>
#include <stdint.h>

#include "lynxplosion.h"

/* What the runner did, as the bundle records it. content_name is for the
   runner's own messages and is never written to a bundle file. */
typedef struct bundle_run {
    const char *content_name;
    uint64_t tick_budget;
    uint64_t ticks_consumed;
    lynx_stop_reason stop;
} bundle_run;

/* Computes the observation digest of the machine state and run result without
   writing any file: SHA-256 of the sorted "obs.*" lines of digests.txt, each
   ending in a newline. Writes 64 lowercase hex digits and a NUL to
   obs_digest_hex. Returns 0 on success, non-zero on a refusal. */
int bundle_observe(const lynx_machine *m, const bundle_run *run, char obs_digest_hex[65]);

/* Writes the bundle files into dir, which must already exist, and returns the
   observation digest and the bundle digest (SHA-256 of the whole digests.txt).
   Refuses a machine with no loaded content. Every file is created afresh: an
   earlier bundle's file, or a symlink at its name, is removed first and the new
   file is created exclusively, and digests.txt and run.manifest are removed
   before anything is written, so a write that fails part way leaves no
   digests.txt beside new files. Returns 0 on success, non-zero on a refusal or
   a file error. */
int bundle_write(const char *dir, const lynx_machine *m, const bundle_run *run,
                 char obs_digest_hex[65], char bundle_digest_hex[65]);

/* Reads one binary BS93/BLL object, bounded to 65535 bytes. A 65536th byte,
   read error or empty file is refused; output arguments change only on success.
   On success *bytes holds *len bytes that the caller frees. */
int bundle_read_bll(const char *path, uint8_t **bytes, size_t *len);

/* Reads content given as hex text: pairs of hex digits, whitespace, and
   # comments to the end of the line. Anything else is refused, as are an odd
   digit count, a pair split by whitespace and an empty result. On success
   *bytes holds *len bytes that the caller frees. Returns 0 on success. */
int bundle_read_hex(const char *path, uint8_t **bytes, size_t *len);

#endif /* LYNX_RUNNER_BUNDLE_H */
