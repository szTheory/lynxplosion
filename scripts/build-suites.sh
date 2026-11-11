#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
CC65_DIR="$ROOT/build/pins/cc65"
SUITES_DIR="$ROOT/build/pins/lynx-tests"
OUT_ARG=

usage() {
  printf 'usage: sh scripts/build-suites.sh --out DIR\n' >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --out)
      [ "$#" -ge 2 ] || usage
      [ -z "$OUT_ARG" ] || usage
      OUT_ARG=$2
      shift 2
      ;;
    *) usage ;;
  esac
done
[ -n "$OUT_ARG" ] || usage
case "$OUT_ARG" in
  /) printf 'build-suites: refusing root output path\n' >&2; exit 1 ;;
  *'/../'*|../*|*/..|..) printf 'build-suites: refusing parent traversal in output path\n' >&2; exit 1 ;;
esac

case "$OUT_ARG" in
  /*) OUT_PATH=$OUT_ARG ;;
  *) OUT_PATH=$ROOT/$OUT_ARG ;;
esac
OUT_PARENT=$(dirname -- "$OUT_PATH")
OUT_BASE=$(basename -- "$OUT_PATH")
[ -d "$OUT_PARENT" ] || { printf 'build-suites: output parent must already exist\n' >&2; exit 1; }
[ ! -L "$OUT_PARENT" ] || { printf 'build-suites: refusing symlink output parent\n' >&2; exit 1; }
PARENT_REAL=$(CDPATH= cd -P -- "$OUT_PARENT" && pwd -P)
SCRATCH_ROOT="$ROOT/build/check/scratch"
TMP_ROOT=${TMPDIR:-/tmp}
[ -d "$SCRATCH_ROOT" ] || { printf 'build-suites: owned scratch root is missing\n' >&2; exit 1; }
[ -d "$TMP_ROOT" ] || { printf 'build-suites: invocation temporary root is missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH= cd -P -- "$SCRATCH_ROOT" && pwd -P)
TMP_REAL=$(CDPATH= cd -P -- "$TMP_ROOT" && pwd -P)
case "$PARENT_REAL" in
  "$SCRATCH_REAL"|"$SCRATCH_REAL"/*|"$TMP_REAL"|"$TMP_REAL"/*) ;;
  *) printf 'build-suites: output parent is outside owned scratch and invocation temporary roots\n' >&2; exit 1 ;;
esac
OUT="$PARENT_REAL/$OUT_BASE"
[ ! -L "$OUT" ] || { printf 'build-suites: refusing symlink output directory\n' >&2; exit 1; }
if [ -e "$OUT" ] && [ ! -d "$OUT" ]; then
  printf 'build-suites: output path exists and is not a directory\n' >&2
  exit 1
fi
mkdir -p -- "$OUT"
OUT_REAL=$(CDPATH= cd -P -- "$OUT" && pwd -P)
[ "$OUT_REAL" = "$OUT" ] || { printf 'build-suites: output directory resolved through a symlink\n' >&2; exit 1; }

sha256() {
  shasum -a 256 "$1" | awk '{print $1}'
}

pin_value() {
  awk -v name="$1" '$1 == name { print $4; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"
}

pin_url() {
  awk -v name="$1" '$1 == name { print $3; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"
}

verify_pinned_file() {
  relative=$1
  [ -f "$SUITES_DIR/$relative" ] || {
    printf 'build-suites: pinned source missing: %s\n' "$relative" >&2
    return 1
  }
  actual=$(sha256 "$SUITES_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$SUITES_DIR/.lynx-pin-files")
  [ "$actual" = "$expected" ] || {
    printf 'build-suites: pinned source hash mismatch: %s\n' "$relative" >&2
    return 1
  }
  printf '%s\n' "$actual"
}

verify_tool() {
  relative=$1
  actual=$(sha256 "$CC65_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$CC65_DIR/.lynx-build-files")
  [ "$actual" = "$expected" ] || {
    printf 'build-suites: pinned cc65 tool hash mismatch: %s\n' "$relative" >&2
    return 1
  }
}

LYNX_PIN=$(pin_value lynx-tests)
CC65_PIN=$(pin_value cc65)
LYNX_SOURCE_URL=$(pin_url lynx-tests)
LYNX_REVISION=$(printf '%s\n' "$LYNX_SOURCE_URL" | sed 's#.*/archive/##; s/\.tar\.gz$//')
[ "$(cat "$SUITES_DIR/.lynx-pin")" = "$LYNX_PIN" ] || { printf 'build-suites: lynx-tests pin marker mismatch\n' >&2; exit 1; }
[ "$(cat "$CC65_DIR/.lynx-pin")" = "$CC65_PIN" ] || { printf 'build-suites: cc65 pin marker mismatch\n' >&2; exit 1; }
[ -x "$CC65_DIR/bin/ca65" ] && [ -x "$CC65_DIR/bin/ld65" ] || {
  printf 'build-suites: pinned ca65/ld65 are missing; scripts/check.sh owns preparation\n' >&2
  exit 1
}
[ -f "$CC65_DIR/.lynx-build" ] && [ -f "$CC65_DIR/.lynx-build-files" ] || {
  printf 'build-suites: verified cc65 build records are missing\n' >&2
  exit 1
}
verify_tool bin/ca65
verify_tool bin/ld65

CPU_BEFORE=$(verify_pinned_file cpu/tests.s)
TIMERS_BEFORE=$(verify_pinned_file timers/tests.s)
CPU_MAIN_BEFORE=$(verify_pinned_file cpu/main.c)
TIMERS_MAIN_BEFORE=$(verify_pinned_file timers/main.c)
LICENSE_SHA=$(verify_pinned_file LICENSE)
rm -f "$OUT/cpu.bll" "$OUT/cpu.lbl" "$OUT/cpu.o" \
  "$OUT/timers.bll" "$OUT/timers.lbl" "$OUT/timers.o" \
  "$OUT/NOTICE" "$OUT/build-record.txt"
cp "$SUITES_DIR/LICENSE" "$OUT/NOTICE"
cmp "$SUITES_DIR/LICENSE" "$OUT/NOTICE" || {
  printf 'build-suites: failed to preserve the pinned source notice\n' >&2
  exit 1
}

build_suite() {
  suite=$1
  wrapper="$ROOT/fixtures/suites/$suite-wrapper.s"
  (
    cd "$SUITES_DIR/$suite"
    env -i PATH=/usr/bin:/bin LC_ALL=C CC65_HOME="$CC65_DIR" \
      "$CC65_DIR/bin/ca65" --cpu 65C02 -I "$SUITES_DIR/$suite" -I "$CC65_DIR/asminc" \
      -o "$OUT/$suite.o" "$wrapper"
  )
  env -i PATH=/usr/bin:/bin LC_ALL=C CC65_HOME="$CC65_DIR" \
    "$CC65_DIR/bin/ld65" -C "$ROOT/fixtures/suites/bll.cfg" \
    -Ln "$OUT/$suite.lbl" -o "$OUT/$suite.bll" "$OUT/$suite.o"
  [ -s "$OUT/$suite.bll" ] && [ -s "$OUT/$suite.lbl" ] || {
    printf 'build-suites: missing linked output for %s\n' "$suite" >&2
    return 1
  }
}

build_suite cpu
build_suite timers
CPU_AFTER=$(verify_pinned_file cpu/tests.s)
TIMERS_AFTER=$(verify_pinned_file timers/tests.s)
CPU_MAIN_AFTER=$(verify_pinned_file cpu/main.c)
TIMERS_MAIN_AFTER=$(verify_pinned_file timers/main.c)
[ "$CPU_BEFORE" = "$CPU_AFTER" ] && [ "$TIMERS_BEFORE" = "$TIMERS_AFTER" ] &&
  [ "$CPU_MAIN_BEFORE" = "$CPU_MAIN_AFTER" ] &&
  [ "$TIMERS_MAIN_BEFORE" = "$TIMERS_MAIN_AFTER" ] || {
  printf 'build-suites: pinned upstream assembly changed during build\n' >&2
  exit 1
}

{
  printf 'lynx_tests_revision=%s\n' "$LYNX_REVISION"
  printf 'lynx_tests_pin=%s\n' "$LYNX_PIN"
  printf 'cc65_pin=%s\n' "$CC65_PIN"
  printf 'cpu_main_sha256=%s\n' "$CPU_MAIN_AFTER"
  printf 'timers_main_sha256=%s\n' "$TIMERS_MAIN_AFTER"
  printf 'cpu_source_sha256=%s\n' "$CPU_AFTER"
  printf 'timers_source_sha256=%s\n' "$TIMERS_AFTER"
  printf 'license_sha256=%s\n' "$LICENSE_SHA"
  printf 'cpu_object_sha256=%s\n' "$(sha256 "$OUT/cpu.bll")"
  printf 'timers_object_sha256=%s\n' "$(sha256 "$OUT/timers.bll")"
} > "$OUT/build-record.txt"
cat "$OUT/build-record.txt"
