#!/bin/sh
# Seeded controls for scripts/scan-binaries.sh, run as
#   tests/hygiene/scan_control.sh REPO_ROOT
#
# Fault prevented: a rights scan that misses a firmware-like file in some zone,
# trusts a file it should not (a modified or unlisted pin-zone file, toolchain
# magic outside the build output paths, a stale build list) or rejects
# legitimate content. Every tree is built under a fresh temporary directory
# outside the repository and the build tree, every fake byte is generated
# deterministically here at run time, and nothing is left behind. Each case
# seeds a file, asserts the rejection (exit 1 and the finding's wording), removes
# or restores it and asserts a pass.
set -eu

if [ $# -ne 1 ]; then
  echo "usage: scan_control.sh REPO_ROOT" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)
scan=$repo/scripts/scan-binaries.sh

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-scan.XXXXXX")
work=$(cd "$work" && pwd -P)
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$repo"/*)
    echo "scan_control: the work directory $work is inside the repository" >&2
    exit 1
    ;;
esac

fail() {
  echo "scan_control: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "scan_control: $1: ok"
}

# The isolated repository, with a noreply identity and an ignored build dir.
iso=$work/repo
mkdir -p "$iso/scripts" "$iso/fixtures/manifests"
cp "$scan" "$iso/scripts/scan-binaries.sh"
(
  cd "$iso"
  git init -q .
  git config user.email control@users.noreply.github.com
  git config user.name control
  git config commit.gpgsign false
  printf 'build/\n' >.gitignore
  printf 'text\n' >readme.txt
  git add .gitignore readme.txt
  git commit -q -m initial
)
B=$iso/build
CB=$B/check
mkdir -p "$CB/scratch" "$B/pins/x" "$B/other"

# --- deterministic fake bytes -------------------------------------------------

# gen_hi FILE N: N bytes with values 128 + (i % 127): never NUL, never ASCII.
gen_hi() {
  gh_i=0
  : >"$1"
  while [ "$gh_i" -lt "$2" ]; do
    gh_v=$((128 + gh_i % 127))
    # shellcheck disable=SC2059 # the octal escape is built on purpose
    printf "\\$(printf '%03o' "$gh_v")" >>"$1"
    gh_i=$((gh_i + 1))
  done
}

# gen_nul FILE N: one NUL byte followed by N-1 'A' bytes.
gen_nul() {
  printf '\000' >"$1"
  head -c "$(($2 - 1))" /dev/zero | tr '\000' 'A' >>"$1"
}

# gen_magic FILE MAGIC N: the magic (a printf format), then control bytes up to N.
gen_magic() {
  # shellcheck disable=SC2059 # the magic is a printf format on purpose
  printf "$2" >"$1"
  gm_have=$(wc -c <"$1" | tr -d ' ')
  head -c "$(($3 - gm_have))" /dev/zero | tr '\000' '\001' >>"$1"
}

ELF='\177ELF'
MACHO='\317\372\355\376'
AR='!<arch>\n\000'
NINJA='# ninjadeps\n\004\000\000\000'

sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    so=$(sha256sum "$1")
  else
    so=$(shasum -a 256 "$1")
  fi
  printf '%s\n' "${so%% *}"
}

size_of() { wc -c <"$1" | tr -d ' '; }

# --- assertions ---------------------------------------------------------------

scan_src() { sh "$iso/scripts/scan-binaries.sh" --source --root "$iso"; }
scan_art() { sh "$iso/scripts/scan-binaries.sh" --artifacts "$B" --cmake-binary-dir "$CB" --root "$iso"; }

# accept NAME FUNCTION: exit 0.
accept() {
  if ! "$2" >"$work/out" 2>"$work/err"; then
    cat "$work/err" >&2
    fail "$1" "expected a pass, got exit 1"
  fi
}

# reject NAME FUNCTION TEXT: exit 1 and TEXT in the findings.
reject() {
  rc=0
  "$2" >"$work/out" 2>"$work/err" || rc=$?
  if [ "$rc" -ne 1 ]; then
    cat "$work/err" >&2
    fail "$1" "expected exit 1, got $rc"
  fi
  if ! grep -F -q "$3" "$work/err"; then
    cat "$work/err" >&2
    fail "$1" "expected finding text: $3"
  fi
}

# --- source zone --------------------------------------------------------------

accept source_clean scan_src
ok source_clean

gen_nul "$iso/nul512.bin" 512
reject source_nul512 scan_src 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$iso/nul512.bin"
accept source_nul512_removed scan_src
ok source_nul512

gen_hi "$iso/hi512.dat" 512
reject source_hi512 scan_src 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$iso/hi512.dat"
accept source_hi512_removed scan_src
ok source_hi512_no_nul

gen_hi "$iso/blob.dat" 100
reject source_unmanifested scan_src 'scan-binaries: unmanifested binary: '
blob_sha=$(sha_of "$iso/blob.dat")
printf 'sha256 = %s\n' "$blob_sha" >"$iso/fixtures/manifests/blob.manifest"
accept source_manifested scan_src
rm -f "$iso/blob.dat" "$iso/fixtures/manifests/blob.manifest"
ok source_unmanifested_then_manifested

gen_hi "$iso/big.dat" 512
printf 'sha256 = %s\n' "$(sha_of "$iso/big.dat")" >"$iso/fixtures/manifests/big.manifest"
reject source_manifested_512 scan_src 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$iso/big.dat" "$iso/fixtures/manifests/big.manifest"
accept source_manifested_512_removed scan_src
ok source_manifested_512_still_rejected

gen_magic "$iso/tool.dat" "$ELF" 100
printf 'sha256 = %s\n' "$(sha_of "$iso/tool.dat")" >"$iso/fixtures/manifests/tool.manifest"
reject source_elf_manifested scan_src 'scan-binaries: toolchain output outside build output paths: '
rm -f "$iso/tool.dat" "$iso/fixtures/manifests/tool.manifest"
accept source_elf_removed scan_src
ok source_toolchain_magic_even_if_manifested

gen_magic "$iso/tool.dat" "$ELF" 100
reject source_elf scan_src 'scan-binaries: toolchain output outside build output paths: '
rm -f "$iso/tool.dat"
gen_magic "$iso/tool.dat" "$MACHO" 100
reject source_macho scan_src 'scan-binaries: toolchain output outside build output paths: '
rm -f "$iso/tool.dat"
accept source_magic_removed scan_src
ok source_elf_and_macho

# Valid UTF-8 with non-ASCII characters is text, not a binary.
printf 'caf\303\251 \342\200\224 \342\234\223 \316\273\n' >"$iso/utf8.txt"
accept source_utf8_text scan_src
rm -f "$iso/utf8.txt"
ok source_utf8_text_accepted

# A control byte other than tab, LF, FF and CR makes a file non-text; the
# allowed ones do not.
printf 'a\tb\fc\r\nd\n' >"$iso/allowed-controls.txt"
accept source_allowed_controls scan_src
rm -f "$iso/allowed-controls.txt"
printf 'esc \033 here\n' >"$iso/esc.dat"
reject source_esc_control scan_src 'scan-binaries: unmanifested binary: '
rm -f "$iso/esc.dat"
accept source_esc_removed scan_src
ok source_control_bytes

# --- text-side rules (WR-06) ----------------------------------------------------

# hexfile FILE BYTES PER_LINE: BYTES bytes of hex text, PER_LINE bytes a line, from
# a deterministic byte sequence; written as text only.
hexfile() {
  hf_i=0
  : >"$1"
  while [ "$hf_i" -lt "$2" ]; do
    printf '%02x' $(((hf_i * 7 + 3) % 256)) >>"$1"
    hf_i=$((hf_i + 1))
    if [ $((hf_i % $3)) -eq 0 ] || [ "$hf_i" -eq "$2" ]; then
      printf '\n' >>"$1"
    fi
  done
}

hexfile "$iso/image.hex" 512 32
reject text_hex512 scan_src 'scan-binaries: hex text decoding to exactly 512 bytes: '
printf 'sha256 = %s\n' "$(sha_of "$iso/image.hex")" >"$iso/fixtures/manifests/image.manifest"
accept text_hex512_manifested scan_src
rm -f "$iso/fixtures/manifests/image.manifest"
reject text_hex512_unmanifested_again scan_src 'scan-binaries: hex text decoding to exactly 512 bytes: '
rm -f "$iso/image.hex"
accept text_hex512_removed scan_src
ok text_hex_512_rejected_unless_manifested

# The layout does not hide it: other line widths, comments, blank lines, CRLF.
hexfile "$iso/image.hex" 512 16
reject text_hex512_width16 scan_src 'scan-binaries: hex text decoding to exactly 512 bytes: '
{
  printf '# a boot image, as hex text\n\n'
  hexfile "$work/body.hex" 512 8
  sed 's/$/ # row/' "$work/body.hex"
  printf '\n# end\n'
} >"$iso/image.hex"
reject text_hex512_comments scan_src 'scan-binaries: hex text decoding to exactly 512 bytes: '
hexfile "$work/body.hex" 512 32
sed 's/$/\r/' "$work/body.hex" >"$iso/image.hex"
reject text_hex512_crlf scan_src 'scan-binaries: hex text decoding to exactly 512 bytes: '
rm -f "$iso/image.hex"
accept text_hex512_layouts_removed scan_src
ok text_hex_layout_variants_rejected

# Other sizes are not firmware-sized hex text: 511 and 513 bytes, and a digest.
hexfile "$iso/image.hex" 511 32
accept text_hex511 scan_src
hexfile "$iso/image.hex" 513 32
accept text_hex513 scan_src
printf '%s\n' "$(sha_of "$iso/readme.txt")" >"$iso/image.hex"
accept text_digest_line scan_src
rm -f "$iso/image.hex"
ok text_hex_other_sizes_accepted

# A long base64-like run on one line is rejected unless manifested; a shorter run
# and prose are not.
head -c 684 /dev/zero | tr '\000' 'Q' >"$iso/blob.b64"
printf '\n' >>"$iso/blob.b64"
reject text_base64_run scan_src 'scan-binaries: long base64 or hex run in a text file: '
printf 'sha256 = %s\n' "$(sha_of "$iso/blob.b64")" >"$iso/fixtures/manifests/b64.manifest"
accept text_base64_manifested scan_src
rm -f "$iso/fixtures/manifests/b64.manifest"
reject text_base64_unmanifested_again scan_src 'scan-binaries: long base64 or hex run in a text file: '
head -c 199 /dev/zero | tr '\000' 'Q' >"$iso/blob.b64"
printf '\n' >>"$iso/blob.b64"
accept text_base64_199 scan_src
head -c 200 /dev/zero | tr '\000' 'Q' >"$iso/blob.b64"
printf '\n' >>"$iso/blob.b64"
reject text_base64_200 scan_src 'scan-binaries: long base64 or hex run in a text file: '
{
  i=0
  while [ "$i" -lt 60 ]; do
    printf 'word%s ' "$i"
    i=$((i + 1))
  done
  printf '\n'
} >"$iso/blob.b64"
accept text_long_prose_line scan_src
rm -f "$iso/blob.b64"
ok text_long_run_threshold

# The same rules in the build tree: another build path and the pin zone (where a
# file on the archive list is still a finding).
hexfile "$B/other/image.hex" 512 32
reject text_hex512_other scan_art 'scan-binaries: hex text decoding to exactly 512 bytes: '
rm -f "$B/other/image.hex"
accept text_hex512_other_removed scan_art
ok text_hex_512_in_build_tree

# --- pin zone -----------------------------------------------------------------

PD=$B/pins/x
pin_digest=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
other_digest=fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210
printf '%s\n' "$pin_digest" >"$PD/.lynx-pin"

# write_pin_list FILE...: the archive list, in the fetch-pin.sh format.
write_pin_list() {
  {
    printf '# sha256 of the verified archive: %s\n' "$pin_digest"
    printf '# one line per regular file: sha256, a space, the path relative to the archive top\n'
    for wp in "$@"; do
      printf '%s %s\n' "$(sha_of "$PD/$wp")" "$wp"
    done | LC_ALL=C sort -t ' ' -k2
  } >"$PD/.lynx-pin-files"
}

gen_hi "$PD/listed.bin" 100
printf 'archive text\n' >"$PD/notes.txt"
write_pin_list listed.bin notes.txt
accept pins_listed scan_art
ok pins_listed_passes

gen_hi "$PD/unlisted.bin" 100
reject pins_unlisted scan_art 'scan-binaries: unlisted pin-zone binary: '
rm -f "$PD/unlisted.bin"
accept pins_unlisted_removed scan_art
ok pins_unlisted_rejected

cp "$PD/listed.bin" "$work/listed.bak"
printf '\377' | dd of="$PD/listed.bin" bs=1 seek=99 conv=notrunc 2>/dev/null
reject pins_modified scan_art 'scan-binaries: unlisted pin-zone binary: '
cp "$work/listed.bak" "$PD/listed.bin"
accept pins_modified_restored scan_art
ok pins_modified_listed_file_rejected

gen_magic "$PD/tool" "$MACHO" 100
reject pins_macho_unlisted scan_art 'scan-binaries: unlisted pin-zone binary: '
rm -f "$PD/tool"
accept pins_macho_removed scan_art
ok pins_unlisted_toolchain_magic_rejected

gen_hi "$PD/big.bin" 512
write_pin_list listed.bin notes.txt big.bin
reject pins_listed_512 scan_art 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$PD/big.bin"
write_pin_list listed.bin notes.txt
accept pins_listed_512_removed scan_art
ok pins_listed_512_rejected

# The build list: files the pinned recipe added, listed after the build.
mkdir -p "$PD/bin" "$PD/lib"
gen_magic "$PD/bin/tool" "$MACHO" 100
gen_hi "$PD/lib/x.lib" 100
# write_build_list DIGEST FILE...
write_build_list() {
  wb_digest=$1
  shift
  {
    printf '# pin_sha256 = %s\n' "$wb_digest"
    printf '# recipe = 1\n'
    for wb in "$@"; do
      printf '%s %s\n' "$(sha_of "$PD/$wb")" "$wb"
    done | LC_ALL=C sort -t ' ' -k2
  } >"$PD/.lynx-build-files"
}
write_build_list "$pin_digest" bin/tool lib/x.lib
accept pins_build_list scan_art
ok pins_build_list_passes

gen_hi "$PD/bin/extra" 100
reject pins_added_after_build scan_art 'scan-binaries: unlisted pin-zone binary: '
rm -f "$PD/bin/extra"
accept pins_added_removed scan_art
ok pins_added_after_build_list_rejected

cp "$PD/bin/tool" "$work/tool.bak"
printf '\000' | dd of="$PD/bin/tool" bs=1 seek=50 conv=notrunc 2>/dev/null
reject pins_built_modified scan_art 'scan-binaries: unlisted pin-zone binary: '
cp "$work/tool.bak" "$PD/bin/tool"
accept pins_built_restored scan_art
ok pins_modified_built_file_rejected

write_build_list "$other_digest" bin/tool lib/x.lib
reject pins_stale_build_list scan_art 'scan-binaries: unlisted pin-zone binary: '
if ! grep -F -q "$PD/bin/tool" "$work/err" || ! grep -F -q "$PD/lib/x.lib" "$work/err"; then
  fail pins_stale_build_list "both built files must be rejected"
fi
write_build_list "$pin_digest" bin/tool lib/x.lib
accept pins_stale_build_list_restored scan_art
ok pins_build_list_digest_mismatch_not_honoured

gen_hi "$PD/lib/big.lib" 512
write_build_list "$pin_digest" bin/tool lib/x.lib lib/big.lib
reject pins_build_512 scan_art 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$PD/lib/big.lib"
write_build_list "$pin_digest" bin/tool lib/x.lib
accept pins_build_512_removed scan_art
ok pins_build_listed_512_rejected

hexfile "$PD/image.hex" 512 32
write_build_list "$pin_digest" bin/tool lib/x.lib
reject pins_text_hex512 scan_art 'scan-binaries: hex text decoding to exactly 512 bytes: '
write_pin_list listed.bin notes.txt image.hex
reject pins_text_hex512_listed scan_art 'scan-binaries: hex text decoding to exactly 512 bytes: '
rm -f "$PD/image.hex"
write_pin_list listed.bin notes.txt
accept pins_text_hex512_removed scan_art
ok pins_text_hex_512_rejected_even_when_listed

# --- toolchain-output zone (the CMake binary dir) -----------------------------

gen_magic "$CB/a.o" "$ELF" 100
gen_magic "$CB/m.o" "$MACHO" 100
gen_magic "$CB/lib.a" "$AR" 100
gen_magic "$CB/.ninja_deps" "$NINJA" 100
accept check_toolchain_magic scan_art
ok check_toolchain_outputs_pass

gen_hi "$CB/unknown.bin" 100
reject check_unknown scan_art 'scan-binaries: unmanifested binary: '
rm -f "$CB/unknown.bin"
accept check_unknown_removed scan_art
ok check_unmanifested_rejected

gen_magic "$CB/elf512.o" "$ELF" 512
reject check_elf512 scan_art 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$CB/elf512.o"
accept check_elf512_removed scan_art
ok check_512_byte_elf_rejected

gen_magic "$CB/scratch/x.o" "$ELF" 100
reject scratch_elf scan_art 'scan-binaries: toolchain output outside build output paths: '
rm -f "$CB/scratch/x.o"
accept scratch_elf_removed scan_art
ok scratch_toolchain_magic_rejected

gen_magic "$B/other/x.o" "$MACHO" 100
reject other_macho scan_art 'scan-binaries: toolchain output outside build output paths: '
rm -f "$B/other/x.o"
accept other_macho_removed scan_art
ok other_build_dir_toolchain_magic_rejected

gen_hi "$CB/scratch/f.bin" 512
reject scratch_512 scan_art 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$CB/scratch/f.bin"
gen_hi "$B/other/f.bin" 512
reject other_512 scan_art 'scan-binaries: firmware-like 512-byte binary: '
rm -f "$B/other/f.bin"
accept f512_removed scan_art
ok scratch_and_other_512_rejected

gen_hi "$B/other/probe.bin" 100
reject other_unmanifested scan_art 'scan-binaries: unmanifested binary: '
printf 'sha256 = %s\n' "$(sha_of "$B/other/probe.bin")" >"$iso/fixtures/manifests/probe.manifest"
accept other_manifested scan_art
rm -f "$B/other/probe.bin" "$iso/fixtures/manifests/probe.manifest"
accept other_manifest_removed scan_art
ok other_build_file_passes_only_when_manifested

# A 512-byte text file is not a binary.
# Words with spaces, so the line holds no long base64-like run.
head -c 511 /dev/zero | tr '\000' 'a' | sed 's/aaaa/aaa /g' | head -c 511 >"$B/other/text512.txt"
printf '\n' >>"$B/other/text512.txt"
if [ "$(size_of "$B/other/text512.txt")" -ne 512 ]; then
  fail text512 "the fixture is not 512 bytes"
fi
accept text512 scan_art
rm -f "$B/other/text512.txt"
ok text_512_byte_file_accepted

# The summary line reports the counts.
scan_art >"$work/out" 2>&1
if ! grep -q '^scan-binaries: artifacts: [0-9][0-9]* files, 0 of exactly 512 bytes$' "$work/out"; then
  cat "$work/out" >&2
  fail summary "the summary line is missing or wrong"
fi
ok summary_line

# The controls left nothing in the real repository or its build tree.
if [ -n "$(find "$repo" -name 'unlisted.bin' -print 2>/dev/null)" ]; then
  fail clean "a seeded file reached the repository"
fi
ok isolated_tree
