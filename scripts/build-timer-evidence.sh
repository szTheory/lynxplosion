#!/bin/sh
set -eu
umask 077

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
SUITES_DIR="$ROOT/build/pins/lynx-tests"
CC65_DIR="$ROOT/build/pins/cc65"
MODE=
OUT_ARG=
TRACER_ONLY=0

usage() {
  printf 'usage: sh scripts/build-timer-evidence.sh --out|--verify DIR [--tracer]\n' >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --out|--verify)
      [ "$#" -ge 2 ] || usage
      [ -z "$MODE" ] || usage
      MODE=$1
      OUT_ARG=$2
      shift 2
      ;;
    --tracer)
      [ "$TRACER_ONLY" -eq 0 ] || usage
      TRACER_ONLY=1
      shift
      ;;
    *) usage ;;
  esac
done
if [ -z "$MODE" ] || [ -z "$OUT_ARG" ]; then usage; fi
if printf '%s' "$OUT_ARG" | LC_ALL=C grep '[[:cntrl:]]' >/dev/null; then
  printf 'timer evidence: output path contains control characters\n' >&2; exit 1
fi
case "$OUT_ARG" in
  /|..|../*|*/..|*'/../'*) printf 'timer evidence: refusing unsafe output path\n' >&2; exit 1 ;;
  /*) OUT_PATH=$OUT_ARG ;;
  *) OUT_PATH=$ROOT/$OUT_ARG ;;
esac
OUT_PARENT=$(dirname -- "$OUT_PATH")
OUT_BASE=$(basename -- "$OUT_PATH")
cursor=$OUT_PATH
while :; do
  [ ! -L "$cursor" ] || { printf 'timer evidence: refusing symlink output ancestor\n' >&2; exit 1; }
  ancestor=$(dirname -- "$cursor")
  [ "$ancestor" != "$cursor" ] || break
  cursor=$ancestor
done
[ -d "$OUT_PARENT" ] || { printf 'timer evidence: output parent must already exist\n' >&2; exit 1; }
[ ! -L "$OUT_PARENT" ] || { printf 'timer evidence: refusing symlink output parent\n' >&2; exit 1; }
PARENT_REAL=$(CDPATH='' cd -P -- "$OUT_PARENT" && pwd -P)
OUT="$PARENT_REAL/$OUT_BASE"
[ ! -L "$OUT" ] || { printf 'timer evidence: refusing symlink output directory\n' >&2; exit 1; }
SCRATCH_ROOT="$ROOT/build/check/scratch"
TMP_ROOT=${TMPDIR:-/tmp}
[ -d "$SCRATCH_ROOT" ] || { printf 'timer evidence: owned scratch root is missing\n' >&2; exit 1; }
[ -d "$TMP_ROOT" ] || { printf 'timer evidence: invocation temporary root is missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH='' cd -P -- "$SCRATCH_ROOT" && pwd -P)
TMP_REAL=$(CDPATH='' cd -P -- "$TMP_ROOT" && pwd -P)
case "$PARENT_REAL" in
  "$SCRATCH_REAL"|"$SCRATCH_REAL"/*|"$TMP_REAL"|"$TMP_REAL"/*) ;;
  *) printf 'timer evidence: output is outside owned scratch and invocation temporary roots\n' >&2; exit 1 ;;
esac

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}
pin_value() {
  awk -v name="$1" '$1 == name { print $4; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"
}
pin_url() {
  awk -v name="$1" '$1 == name { print $3; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"
}
verify_pinned_file() {
  relative=$1
  [ -f "$SUITES_DIR/$relative" ] || { printf 'timer evidence: pinned source missing: %s\n' "$relative" >&2; return 1; }
  actual=$(sha256 "$SUITES_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$SUITES_DIR/.lynx-pin-files")
  [ "$actual" = "$expected" ] || { printf 'timer evidence: pinned source hash mismatch: %s\n' "$relative" >&2; return 1; }
  printf '%s\n' "$actual"
}
verify_tool() {
  relative=$1
  actual=$(sha256 "$CC65_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$CC65_DIR/.lynx-build-files")
  [ "$actual" = "$expected" ] || { printf 'timer evidence: pinned cc65 tool hash mismatch: %s\n' "$relative" >&2; return 1; }
  printf '%s\n' "$actual"
}
symbol() {
  awk -v target=".$2" '$3 == target { print $2; found++ } END { if (found != 1) exit 1 }' "$1"
}
expect_symbol() {
  actual=$(symbol "$1" "$2") || { printf 'timer evidence: label missing or duplicate: %s\n' "$2" >&2; return 1; }
  [ "$actual" = "$3" ] || { printf 'timer evidence: source-derived label moved: %s=%s expected %s\n' "$2" "$actual" "$3" >&2; return 1; }
}
validate_layout() {
  lbl=$1
  bll=$2
  layout_request=$3
  layout_restore=$4
  expect_symbol "$lbl" __BLL_RUN__ 0001F6
  expect_symbol "$lbl" __BLL_SIZE__ 00000A
  expect_symbol "$lbl" suite_entry 000200
  expect_symbol "$lbl" suite_startup_body 000204
  expect_symbol "$lbl" __STARTUP_RUN__ 000200
  expect_symbol "$lbl" __STARTUP_SIZE__ 000073
  expect_symbol "$lbl" __CODE_RUN__ 000273
  expect_symbol "$lbl" __CODE_SIZE__ 0001E7
  expect_symbol "$lbl" __EXTENSION_RUN__ 000500
  expect_symbol "$lbl" __FRAMEBUFFER_RUN__ 006000
  expect_symbol "$lbl" __FRAMEBUFFER_SIZE__ 001FE0
  expect_symbol "$lbl" __BSS_RUN__ 008000
  stack_helper=$(symbol "$lbl" suite_extension_stack_init)
  stack_helper_dec=$((0x$stack_helper))
  [ "$stack_helper_dec" -ge $((0x000500)) ] || { printf 'timer evidence: stack helper is outside extension segment\n' >&2; return 1; }
  # od emits decimal bytes; split its numeric fields for positional checks.
  # shellcheck disable=SC2046
  set -- $(dd if="$bll" bs=1 skip=10 count=4 2>/dev/null | od -An -tu1)
  if ! { [ "$#" -eq 4 ] && [ "$1" -eq 120 ] && [ "$2" -eq 76 ] &&
    [ "$3" -eq $((stack_helper_dec & 255)) ] &&
    [ "$4" -eq $(((stack_helper_dec >> 8) & 255)) ]; }; then
      printf 'timer evidence: BLL entry must begin SEI+JMP to the stack helper\n' >&2; return 1;
  fi
  ext_size=$(symbol "$lbl" __EXTENSION_SIZE__)
  ext_end=$((0x000500 + 0x$ext_size))
  ext_end_hex=$(printf '%06X' "$ext_end")
  suite_end=$(symbol "$lbl" suite_end)
  [ "$ext_end_hex" = "$suite_end" ] || { printf 'timer evidence: loadable image end differs from extension map\n' >&2; return 1; }
  [ "$ext_end" -le $((0x006000)) ] || { printf 'timer evidence: extension overlaps framebuffer\n' >&2; return 1; }
  display_request=$(symbol "$lbl" suite_dispctl_request)
  display_after=$(symbol "$lbl" suite_dispctl_after)
  display_request_dec=$((0x$display_request))
  if ! { [ "$display_request_dec" -ge $((0x000500)) ] &&
    [ "$((0x$display_after))" -eq "$((display_request_dec + 3))" ] &&
    [ "$((0x$display_after))" -le "$ext_end" ]; }; then
      printf 'timer evidence: display write labels escape the extension\n' >&2; return 1;
  fi
  # od emits decimal bytes; split its numeric fields for positional checks.
  # shellcheck disable=SC2046
  set -- $(dd if="$bll" bs=1 skip="$((display_request_dec - 2 - 512 + 10))" count=5 2>/dev/null | od -An -tu1)
  if ! { [ "$#" -eq 5 ] && [ "$1" -eq 169 ] && [ "$2" -eq "$layout_request" ] &&
    [ "$3" -eq 141 ] && [ "$4" -eq 146 ] && [ "$5" -eq 253 ]; }; then
    printf 'timer evidence: display stop marker is not the requested LDA/STA DISPCTL\n' >&2; return 1;
  fi
  display_restore=$(symbol "$lbl" suite_dispctl_restore)
  display_restore_dec=$((0x$display_restore))
  if ! { [ "$display_restore_dec" -ge "$((0x000500))" ] &&
    [ "$((display_restore_dec + 5))" -le "$ext_end" ]; }; then
      printf 'timer evidence: display restore escapes the extension\n' >&2; return 1;
  fi
  # od emits decimal bytes; split its numeric fields for positional checks.
  # shellcheck disable=SC2046
  set -- $(dd if="$bll" bs=1 skip="$((display_restore_dec - 512 + 10))" count=5 2>/dev/null | od -An -tu1)
  if ! { [ "$#" -eq 5 ] && [ "$1" -eq 169 ] && [ "$2" -eq "$layout_restore" ] &&
    [ "$3" -eq 141 ] && [ "$4" -eq 146 ] && [ "$5" -eq 253 ]; }; then
    printf 'timer evidence: display restore does not match its family\n' >&2; return 1;
  fi
  framebuffer_end=$((0x006000 + 0x001FE0))
  [ "$framebuffer_end" -le $((0x008000)) ] || { printf 'timer evidence: framebuffer overlaps BSS\n' >&2; return 1; }
  bss_size=$(symbol "$lbl" __BSS_SIZE__)
  bss_end=$((0x008000 + 0x$bss_size))
  [ "$bss_end" -le $((0x00FC00)) ] || { printf 'timer evidence: BSS overlaps mapped hardware window\n' >&2; return 1; }
  zp_size=$(symbol "$lbl" __ZEROPAGE_SIZE__)
  [ $((0x$zp_size)) -le $((0x100)) ] || { printf 'timer evidence: zero page range invalid\n' >&2; return 1; }
  # od emits decimal bytes; split its numeric fields for positional checks.
  # shellcheck disable=SC2046
  set -- $(od -An -tu1 -N10 "$bll")
  if ! { [ "$#" -eq 10 ] && [ "$1" -eq 128 ] && [ "$2" -eq 8 ] &&
    [ "$3" -eq 2 ] && [ "$4" -eq 0 ] && [ "$7" -eq 66 ] &&
    [ "$8" -eq 83 ] && [ "$9" -eq 57 ] && [ "${10}" -eq 51 ]; }; then
      printf 'timer evidence: BLL header does not describe the expected BS93 payload\n' >&2; return 1;
  fi
  header_length=$(( $5 * 256 + $6 ))
  file_length=$(wc -c < "$bll" | tr -d ' ')
  [ "$header_length" -eq "$file_length" ] || { printf 'timer evidence: BLL length does not cover its loadable payload\n' >&2; return 1; }
  printf 'timer evidence: map ranges and BLL length valid (header is not loaded into guest RAM; stack stays below payload at 0200)\n'
}
record_value() {
  awk -F= -v key="$2" '$1 == key { print substr($0, length(key) + 2); found++ } END { if (found != 1) exit 1 }' "$1"
}
manifest_value() {
  awk -F ' = ' -v key="$2" '$1 == key { print $2; found++ } END { if (found != 1) exit 1 }' "$1"
}
expect_record() {
  actual=$(record_value "$1" "$2") || { printf 'timer evidence: record missing or duplicates %s\n' "$2" >&2; return 1; }
  [ "$actual" = "$3" ] || { printf 'timer evidence: stale record field %s\n' "$2" >&2; return 1; }
}
expect_manifest() {
  actual=$(manifest_value "$ROOT/fixtures/manifests/timer-evidence.manifest" "$1") || { printf 'timer evidence: manifest missing or duplicates %s\n' "$1" >&2; return 1; }
  [ "$actual" = "$2" ] || { printf 'timer evidence: stale manifest field %s\n' "$1" >&2; return 1; }
}

if [ "$TRACER_ONLY" -eq 1 ]; then
  CASE_NAMES='hardware-dma1-page1 software-dma1-page1'
  PACKAGE_SCOPE=tracer
else
  CASE_NAMES='hardware-dma0-page0 hardware-dma0-page1 hardware-dma1-page0 hardware-dma1-page1 software-dma0-page0 software-dma0-page1 software-dma1-page0 software-dma1-page1'
  PACKAGE_SCOPE=matrix
fi

validate_inventory() {
  expected=$BASE/expected-files.txt
  actual=$BASE/actual-files.txt
  {
    printf '.lynx-timer-owner\nNOTICE\nbuild-record.txt\ncases.txt\nphysical-results.tsv\ntimer-evidence.manifest\n'
    for inventory_case in $CASE_NAMES; do
      printf '%s.bll\n%s.case.txt\n%s.lbl\n%s.map\n' \
        "$inventory_case" "$inventory_case" "$inventory_case" "$inventory_case"
    done
  } | LC_ALL=C sort > "$expected"
  (CDPATH='' cd "$OUT" && find . -type f -print | sed 's#^\./##' | LC_ALL=C sort) > "$actual"
  if [ "$(find "$OUT" -mindepth 1 -type d -print | wc -l | tr -d ' ')" != 0 ] || ! cmp -s "$expected" "$actual"; then
    printf 'timer evidence: exact package file inventory mismatch\n' >&2
    diff -u "$expected" "$actual" >&2 || true
    return 1
  fi
}

LYNX_PIN=$(pin_value lynx-tests)
CC65_PIN=$(pin_value cc65)
LYNX_URL=$(pin_url lynx-tests)
LYNX_REVISION=$(printf '%s\n' "$LYNX_URL" | sed 's#.*/archive/##; s/\.tar\.gz$//')
[ "$(cat "$SUITES_DIR/.lynx-pin")" = "$LYNX_PIN" ] || { printf 'timer evidence: lynx-tests pin marker mismatch\n' >&2; exit 1; }
[ "$(cat "$CC65_DIR/.lynx-pin")" = "$CC65_PIN" ] || { printf 'timer evidence: cc65 pin marker mismatch\n' >&2; exit 1; }
if [ ! -x "$CC65_DIR/bin/ca65" ] || [ ! -x "$CC65_DIR/bin/ld65" ]; then
  printf 'timer evidence: pinned cc65 tools are missing\n' >&2; exit 1
fi
if ! awk -v pin="$CC65_PIN" '$1 == "pin_sha256" && $2 == "=" { p=$3; pc++ } $1 == "recipe" && $2 == "=" { r=$3; rc++ } $1 == "build_seconds" && $2 == "=" { s=$3; sc++ } END { if (pc != 1 || p != pin || rc != 1 || r != "1" || sc != 1 || s !~ /^(0|[1-9][0-9]*)$/) exit 1 }' "$CC65_DIR/.lynx-build"; then
  printf 'timer evidence: cc65 successful pinned recipe record is invalid\n' >&2; exit 1
fi
CA65_SHA=$(verify_tool bin/ca65)
LD65_SHA=$(verify_tool bin/ld65)
CC65_BUILD_SHA=$(sha256 "$CC65_DIR/.lynx-build")
CC65_FILES_SHA=$(sha256 "$CC65_DIR/.lynx-build-files")
TIMERS_SOURCE_SHA=$(verify_pinned_file timers/tests.s)
TIMERS_MAIN_SHA=$(verify_pinned_file timers/main.c)
LICENSE_SHA=$(verify_pinned_file LICENSE)
WRAPPER_SHA=$(sha256 "$ROOT/fixtures/timer-evidence/timer-evidence.s")
LAYOUT_SHA=$(sha256 "$ROOT/fixtures/timer-evidence/bll.cfg")
BUILDER_SHA=$(sha256 "$ROOT/scripts/build-timer-evidence.sh")
NOTICE_SHA=$(sha256 "$ROOT/fixtures/timer-evidence/NOTICE")
MANIFEST_SHA=$(sha256 "$ROOT/fixtures/manifests/timer-evidence.manifest")
CASE_SET_SHA=$(sha256 "$ROOT/fixtures/timer-evidence/images.sha256")
expect_manifest sha256 "$WRAPPER_SHA"
expect_manifest wrapper_sha256 "$WRAPPER_SHA"

if [ "$MODE" = --out ]; then
  [ ! -e "$OUT" ] || { printf 'timer evidence: refusing existing or colliding output\n' >&2; exit 1; }
  mkdir -m 700 -- "$OUT"
  NONCE=$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')
  printf 'lynx-timer-output-v1\n%s\n%s\n' "$OUT" "$NONCE" > "$OUT/.lynx-timer-owner"
else
  [ -d "$OUT" ] || { printf 'timer evidence: verification output directory is missing\n' >&2; exit 1; }
  [ -n "$(find "$OUT" -prune -user "$(id -u)" -perm 0700 -print)" ] || {
    printf 'timer evidence: package must be owner-only mode 0700 and owned by this user\n' >&2; exit 1;
  }
  for entry in "$OUT"/* "$OUT"/.[!.]* "$OUT"/..?*; do
    if [ -e "$entry" ] || [ -L "$entry" ]; then
      if [ ! -f "$entry" ] || [ -L "$entry" ]; then
        printf 'timer evidence: refusing non-regular package entry before reading records\n' >&2; exit 1;
      fi
    fi
  done
  if [ -n "$(find "$OUT" -type l -print)" ]; then
    printf 'timer evidence: refusing symlink package entry before reading records\n' >&2; exit 1
  fi
  if [ ! -f "$OUT/.lynx-timer-owner" ] || [ -L "$OUT/.lynx-timer-owner" ]; then
    printf 'timer evidence: output owner marker missing or invalid\n' >&2; exit 1
  fi
  awk -v dir="$OUT" 'NR == 1 && $0 == "lynx-timer-output-v1" { a=1 } NR == 2 && $0 == dir { b=1 } NR == 3 && length($0) == 48 && $0 ~ /^[0-9a-f]+$/ { c=1 } END { if (NR != 3 || !a || !b || !c) exit 1 }' "$OUT/.lynx-timer-owner" || { printf 'timer evidence: output owner marker mismatch\n' >&2; exit 1; }
  [ -f "$OUT/build-record.txt" ] || { printf 'timer evidence: build record missing\n' >&2; exit 1; }
  cmp -s "$ROOT/fixtures/timer-evidence/NOTICE" "$OUT/NOTICE" || { printf 'timer evidence: NOTICE mismatch\n' >&2; exit 1; }
  cmp -s "$ROOT/fixtures/manifests/timer-evidence.manifest" "$OUT/timer-evidence.manifest" || { printf 'timer evidence: manifest mismatch\n' >&2; exit 1; }
  find "$OUT" -type l -print | awk 'END { if (NR != 0) exit 1 }' || { printf 'timer evidence: symlink in package output\n' >&2; exit 1; }
  for key_value in \
    "lynx_tests_revision|$LYNX_REVISION" "lynx_tests_pin|$LYNX_PIN" "cc65_pin|$CC65_PIN" \
    "cc65_build_record_sha256|$CC65_BUILD_SHA" "cc65_build_files_sha256|$CC65_FILES_SHA" \
    "ca65_tool_sha256|$CA65_SHA" "ld65_tool_sha256|$LD65_SHA" "timer_source_sha256|$TIMERS_SOURCE_SHA" \
    "timer_main_sha256|$TIMERS_MAIN_SHA" "license_sha256|$LICENSE_SHA" "wrapper_sha256|$WRAPPER_SHA" \
    "linker_layout_sha256|$LAYOUT_SHA" "build_script_sha256|$BUILDER_SHA" \
    "notice_sha256|$NOTICE_SHA" "manifest_sha256|$MANIFEST_SHA"; do
    key=${key_value%%|*}; value=${key_value#*|}; expect_record "$OUT/build-record.txt" "$key" "$value"
  done
fi

TMP_REAL=$(CDPATH='' cd -P -- "$TMP_ROOT" && pwd -P)
BASE=$(mktemp -d "$TMP_REAL/lynx-timer-baseline.XXXXXX")
BASE_NONCE=$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')
printf 'lynx-timer-baseline-v1\n%s\n%s\n' "$BASE" "$BASE_NONCE" > "$BASE/.lynx-timer-owner"
cleanup_baseline() {
  status=$?
  trap - EXIT HUP INT TERM
  if [ "$status" -eq 0 ] && [ ! -L "$BASE" ] &&
     [ -f "$BASE/.lynx-timer-owner" ] && [ ! -L "$BASE/.lynx-timer-owner" ] &&
     awk -v dir="$BASE" -v nonce="$BASE_NONCE" 'NR==1&&$0=="lynx-timer-baseline-v1"{a=1} NR==2&&$0==dir{b=1} NR==3&&$0==nonce{c=1} END{if(NR!=3||!a||!b||!c)exit 1}' "$BASE/.lynx-timer-owner"; then
    rm -rf -- "$BASE"
  fi
  exit "$status"
}
trap cleanup_baseline 0
trap 'exit 1' HUP INT TERM
mkdir -p "$BASE/tmp"
TMPDIR="$TMP_REAL" sh "$ROOT/scripts/build-suites.sh" --out "$BASE" >/dev/null
expect_symbol "$BASE/timers.lbl" __CODE_RUN__ 000273
expect_symbol "$BASE/timers.lbl" __CODE_SIZE__ 0001E7
expect_symbol "$BASE/timers.lbl" suite_after_tests 000248
expect_symbol "$BASE/timers.lbl" suite_test_return 000459

code_hash() {
  bll=$1
  temp=$2
  dd if="$bll" of="$temp" bs=1 skip=125 count=487 2>/dev/null
  [ "$(wc -c < "$temp" | tr -d ' ')" = 487 ] || return 1
  sha256 "$temp"
}
BASE_CODE_SHA=$(code_hash "$BASE/timers.bll" "$BASE/base-code.bin")

build_case() {
  case_name=$1
  family=$2
  dma=$3
  page=$4
  requested_dispctl=$5
  requested_mapctl=$6
  restored_dispctl=$([ "$family" = 1 ] && printf 13 || printf 1)
  if [ "$MODE" = --out ]; then
    env -i PATH=/usr/bin:/bin LC_ALL=C "CC65_HOME=$CC65_DIR" \
      "$CC65_DIR/bin/ca65" --cpu 65C02 \
      -D "DMA=$dma" -D "PAGE=$page" -D "FAMILY_HARDWARE=$family" \
      -D "DISPCTL_REQUESTED=$requested_dispctl" -D "MAPCTL_REQUESTED=$requested_mapctl" \
      -I "$SUITES_DIR/timers" -I "$CC65_DIR/asminc" \
      -o "$OUT/$case_name.o" "$ROOT/fixtures/timer-evidence/timer-evidence.s"
    env -i PATH=/usr/bin:/bin LC_ALL=C "CC65_HOME=$CC65_DIR" \
      "$CC65_DIR/bin/ld65" -C "$ROOT/fixtures/timer-evidence/bll.cfg" \
      -Ln "$OUT/$case_name.lbl" -m "$OUT/$case_name.map" \
      -o "$OUT/$case_name.bll" "$OUT/$case_name.o"
    rm -- "$OUT/$case_name.o"
    case_code_sha=$(code_hash "$OUT/$case_name.bll" "$OUT/$case_name.code.bin")
    rm -- "$OUT/$case_name.code.bin"
    [ "$case_code_sha" = "$BASE_CODE_SHA" ] || { printf 'timer evidence: pinned CODE differs from ordinary timer baseline\n' >&2; return 1; }
    expect_symbol "$OUT/$case_name.lbl" __CODE_RUN__ 000273
    expect_symbol "$OUT/$case_name.lbl" __CODE_SIZE__ 0001E7
    expect_symbol "$OUT/$case_name.lbl" suite_test6 0003DC
    expect_symbol "$OUT/$case_name.lbl" suite_test6_poll 000400
    expect_symbol "$OUT/$case_name.lbl" suite_counter_zp 000001
    expect_symbol "$OUT/$case_name.lbl" suite_after_tests 000248
    expect_symbol "$OUT/$case_name.lbl" suite_call_tests 000245
    expect_symbol "$OUT/$case_name.lbl" suite_test_return 000459
    expect_symbol "$OUT/$case_name.lbl" suite_pinned_code_start 000273
    done_store=$(symbol "$OUT/$case_name.lbl" suite_done_store)
    [ "$((0x$done_store))" -ge $((0x000500)) ] || { printf 'timer evidence: post-render completion store is outside the extension segment\n' >&2; return 1; }
    validate_layout "$OUT/$case_name.lbl" "$OUT/$case_name.bll" "$requested_dispctl" "$restored_dispctl"
    object_sha=$(sha256 "$OUT/$case_name.bll")
    labels_sha=$(sha256 "$OUT/$case_name.lbl")
    map_sha=$(sha256 "$OUT/$case_name.map")
    expected_object=$(awk -v name="$case_name.bll" '$2==name {print $1; found++} END{if(found!=1)exit 1}' "$ROOT/fixtures/timer-evidence/images.sha256")
    [ "$object_sha" = "$expected_object" ] || { printf 'timer evidence: complete image differs from approved source build\n' >&2; return 1; }
    {
      printf 'case=%s\n' "$case_name"
      printf 'family=%s\n' "$([ "$family" = 1 ] && printf hardware0D || printf software01)"
      printf 'dma=%s\npage=%s\nrequested_dispctl=%02X\nrequested_mapctl=%02X\n' "$dma" "$page" "$requested_dispctl" "$requested_mapctl"
      printf 'measurement_display=requested\n'
      printf 'restored_display=%s\n' "$([ "$family" = 1 ] && printf not-reached-core-refusal || printf 01-after-render)"
      printf 'physical_readback=unknown\n'
      printf 'case_set_sha256=%s\nrequested_restore_dispctl=%02X\n' "$CASE_SET_SHA" "$restored_dispctl"
      printf 'object_sha256=%s\nlabels_sha256=%s\nmap_sha256=%s\n' "$object_sha" "$labels_sha" "$map_sha"
      printf 'pinned_code_sha256=%s\n' "$case_code_sha"
    } > "$OUT/$case_name.case.txt"
    {
      printf '%s_bll_sha256=%s\n' "$case_name" "$object_sha"
      printf '%s_labels_sha256=%s\n' "$case_name" "$labels_sha"
      printf '%s_map_sha256=%s\n' "$case_name" "$map_sha"
      printf '%s_code_sha256=%s\n' "$case_name" "$case_code_sha"
    } >> "$OUT/build-record.txt"
  else
    if [ ! -f "$OUT/$case_name.bll" ] || [ ! -f "$OUT/$case_name.lbl" ] ||
       [ ! -f "$OUT/$case_name.map" ] || [ ! -f "$OUT/$case_name.case.txt" ]; then
      printf 'timer evidence: package case missing: %s\n' "$case_name" >&2; return 1
    fi
    case_code_sha=$(code_hash "$OUT/$case_name.bll" "$BASE/base-case-code.bin")
    [ "$case_code_sha" = "$BASE_CODE_SHA" ] || { printf 'timer evidence: package CODE no longer matches pinned baseline\n' >&2; return 1; }
    object_sha=$(sha256 "$OUT/$case_name.bll")
    labels_sha=$(sha256 "$OUT/$case_name.lbl")
    map_sha=$(sha256 "$OUT/$case_name.map")
    expect_record "$OUT/build-record.txt" "${case_name}_bll_sha256" "$object_sha"
    expect_record "$OUT/build-record.txt" "${case_name}_labels_sha256" "$labels_sha"
    expect_record "$OUT/build-record.txt" "${case_name}_map_sha256" "$map_sha"
    expect_record "$OUT/build-record.txt" "${case_name}_code_sha256" "$case_code_sha"
    expect_symbol "$OUT/$case_name.lbl" __CODE_RUN__ 000273
    expect_symbol "$OUT/$case_name.lbl" __CODE_SIZE__ 0001E7
    expect_symbol "$OUT/$case_name.lbl" suite_test6 0003DC
    expect_symbol "$OUT/$case_name.lbl" suite_test6_poll 000400
    expect_symbol "$OUT/$case_name.lbl" suite_counter_zp 000001
    expect_symbol "$OUT/$case_name.lbl" suite_after_tests 000248
    expect_symbol "$OUT/$case_name.lbl" suite_call_tests 000245
    expect_symbol "$OUT/$case_name.lbl" suite_test_return 000459
    done_store=$(symbol "$OUT/$case_name.lbl" suite_done_store)
    [ "$((0x$done_store))" -ge $((0x000500)) ] || { printf 'timer evidence: post-render completion store is outside the extension segment\n' >&2; return 1; }
    validate_layout "$OUT/$case_name.lbl" "$OUT/$case_name.bll" "$requested_dispctl" "$restored_dispctl"
    expected_object=$(awk -v name="$case_name.bll" '$2==name {print $1; found++} END{if(found!=1)exit 1}' "$ROOT/fixtures/timer-evidence/images.sha256")
    [ "$object_sha" = "$expected_object" ] || { printf 'timer evidence: complete image differs from approved source build\n' >&2; return 1; }
    expected_family=$([ "$family" = 1 ] && printf hardware0D || printf software01)
    expected_restore=$([ "$family" = 1 ] && printf not-reached-core-refusal || printf 01-after-render)
    expect_record "$OUT/$case_name.case.txt" case "$case_name"
    expect_record "$OUT/$case_name.case.txt" family "$expected_family"
    expect_record "$OUT/$case_name.case.txt" dma "$dma"
    expect_record "$OUT/$case_name.case.txt" page "$page"
    expect_record "$OUT/$case_name.case.txt" requested_dispctl "$(printf '%02X' "$requested_dispctl")"
    expect_record "$OUT/$case_name.case.txt" requested_mapctl "$(printf '%02X' "$requested_mapctl")"
    expect_record "$OUT/$case_name.case.txt" measurement_display requested
    expect_record "$OUT/$case_name.case.txt" restored_display "$expected_restore"
    expect_record "$OUT/$case_name.case.txt" physical_readback unknown
    expect_record "$OUT/$case_name.case.txt" case_set_sha256 "$CASE_SET_SHA"
    expect_record "$OUT/$case_name.case.txt" requested_restore_dispctl "$(printf '%02X' "$restored_dispctl")"
    expect_record "$OUT/$case_name.case.txt" object_sha256 "$object_sha"
    expect_record "$OUT/$case_name.case.txt" labels_sha256 "$labels_sha"
    expect_record "$OUT/$case_name.case.txt" map_sha256 "$map_sha"
    expect_record "$OUT/$case_name.case.txt" pinned_code_sha256 "$case_code_sha"
  fi
}

if [ "$MODE" = --out ]; then
  cp "$ROOT/fixtures/timer-evidence/NOTICE" "$OUT/NOTICE"
  cp "$ROOT/fixtures/manifests/timer-evidence.manifest" "$OUT/timer-evidence.manifest"
  : > "$OUT/build-record.txt"
  {
    printf 'package_scope=%s\n' "$PACKAGE_SCOPE"
    printf 'lynx_tests_revision=%s\nlynx_tests_pin=%s\n' "$LYNX_REVISION" "$LYNX_PIN"
    printf 'cc65_pin=%s\ncc65_build_record_sha256=%s\ncc65_build_files_sha256=%s\n' "$CC65_PIN" "$CC65_BUILD_SHA" "$CC65_FILES_SHA"
    printf 'ca65_tool_sha256=%s\nld65_tool_sha256=%s\n' "$CA65_SHA" "$LD65_SHA"
    printf 'timer_source_sha256=%s\ntimer_main_sha256=%s\nlicense_sha256=%s\n' "$TIMERS_SOURCE_SHA" "$TIMERS_MAIN_SHA" "$LICENSE_SHA"
    printf 'wrapper_sha256=%s\nlinker_layout_sha256=%s\nbuild_script_sha256=%s\n' "$WRAPPER_SHA" "$LAYOUT_SHA" "$BUILDER_SHA"
    printf 'notice_sha256=%s\nmanifest_sha256=%s\n' "$NOTICE_SHA" "$MANIFEST_SHA"
    printf 'baseline_code_sha256=%s\ncode_address=0273\ncode_size=01E7\ntest6_address=03DC\ntest6_poll=0400\ncounter_zp=01\n' "$BASE_CODE_SHA"
    printf 'requested_display_registers=ADDR:6000,PBKUP:29,TIM0:9E/00/18/00,TIM2:68/00/1F/00,GREEN0:00,BLUERED0:00,GREEN15:0F,BLUERED15:FF\n'
    printf 'case_set_sha256=%s\ninitial_counter_record=ram-read-after-zero\n' "$CASE_SET_SHA"
    printf 'physical_evidence=missing\nsource13_role=reference-only\n'
  } >> "$OUT/build-record.txt"
  if [ "$TRACER_ONLY" -eq 1 ]; then
    build_case hardware-dma1-page1 1 1 1 13 12
    build_case software-dma1-page1 0 1 1 1 12
  else
    build_case hardware-dma0-page0 1 0 0 12 140
    build_case hardware-dma0-page1 1 0 1 12 12
    build_case hardware-dma1-page0 1 1 0 13 140
    build_case hardware-dma1-page1 1 1 1 13 12
    build_case software-dma0-page0 0 0 0 0 140
    build_case software-dma0-page1 0 0 1 0 12
    build_case software-dma1-page0 0 1 0 1 140
    build_case software-dma1-page1 0 1 1 1 12
  fi
  printf 'cases=%s\n' "$CASE_NAMES" > "$OUT/cases.txt"
  {
    printf 'case\trun\tcase_set_sha256\tobject_sha256\tsetup\trequested_dispctl\trequested_mapctl\tobserved_dispctl\tobserved_mapctl\tmodel\tloader\tinitial_counter\traw_results\tcompletion\tphoto_reference\tevidence_kind\n'
    if [ "$TRACER_ONLY" -eq 0 ]; then
      for sheet_case in hardware-dma0-page0 hardware-dma0-page1 hardware-dma1-page0 hardware-dma1-page1; do
        sheet_object=$(record_value "$OUT/$sheet_case.case.txt" object_sha256)
        sheet_disp=$(record_value "$OUT/$sheet_case.case.txt" requested_dispctl)
        sheet_map=$(record_value "$OUT/$sheet_case.case.txt" requested_mapctl)
        for sheet_run in 1 2 3; do
          printf '%s\t%s\t%s\t%s\trecorded-startup-v1\t%s\t%s\tunknown\tunknown\t\t\t\t\t\t\t\n' \
            "$sheet_case" "$sheet_run" "$CASE_SET_SHA" "$sheet_object" "$sheet_disp" "$sheet_map"
        done
      done
    fi
  } > "$OUT/physical-results.tsv"
  validate_inventory
else
  expect_record "$OUT/build-record.txt" lynx_tests_revision "$LYNX_REVISION"
  expect_record "$OUT/build-record.txt" lynx_tests_pin "$LYNX_PIN"
  expect_record "$OUT/build-record.txt" cc65_pin "$CC65_PIN"
  expect_record "$OUT/build-record.txt" package_scope "$PACKAGE_SCOPE"
  expect_record "$OUT/build-record.txt" timer_source_sha256 "$TIMERS_SOURCE_SHA"
  expect_record "$OUT/build-record.txt" timer_main_sha256 "$TIMERS_MAIN_SHA"
  expect_record "$OUT/build-record.txt" wrapper_sha256 "$WRAPPER_SHA"
  expect_record "$OUT/build-record.txt" linker_layout_sha256 "$LAYOUT_SHA"
  expect_record "$OUT/build-record.txt" build_script_sha256 "$BUILDER_SHA"
  expect_record "$OUT/build-record.txt" case_set_sha256 "$CASE_SET_SHA"
  expect_record "$OUT/build-record.txt" baseline_code_sha256 "$(code_hash "$BASE/timers.bll" "$BASE/base-code.bin")"
  [ "$(cat "$OUT/cases.txt")" = "cases=$CASE_NAMES" ] || { printf 'timer evidence: case inventory mismatch\n' >&2; exit 1; }
  if [ "$TRACER_ONLY" -eq 1 ]; then
    build_case hardware-dma1-page1 1 1 1 13 12
    build_case software-dma1-page1 0 1 1 1 12
  else
    build_case hardware-dma0-page0 1 0 0 12 140
    build_case hardware-dma0-page1 1 0 1 12 12
    build_case hardware-dma1-page0 1 1 0 13 140
    build_case hardware-dma1-page1 1 1 1 13 12
    build_case software-dma0-page0 0 0 0 0 140
    build_case software-dma0-page1 0 0 1 0 12
    build_case software-dma1-page0 0 1 0 1 140
    build_case software-dma1-page1 0 1 1 1 12
  fi
  validate_inventory
  printf 'timer evidence: verified timer package, source/tool identity and pinned CODE bytes\n'
fi

