#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
CC65_DIR="$ROOT/build/pins/cc65"
SUITES_DIR="$ROOT/build/pins/lynx-tests"
OUT_ARG=
MODE=

usage() {
  printf 'usage: sh scripts/build-suites.sh --out DIR | --verify DIR\n' >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --out)
      [ "$#" -ge 2 ] || usage
      [ -z "$MODE" ] || usage
      OUT_ARG=$2
      MODE=build
      shift 2
      ;;
    --verify)
      [ "$#" -ge 2 ] || usage
      [ -z "$MODE" ] || usage
      OUT_ARG=$2
      MODE=verify
      shift 2
      ;;
    *) usage ;;
  esac
done
if [ -z "$MODE" ] || [ -z "$OUT_ARG" ]; then usage; fi
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
PARENT_REAL=$(CDPATH='' cd -P -- "$OUT_PARENT" && pwd -P)
SCRATCH_ROOT="$ROOT/build/check/scratch"
TMP_ROOT=${TMPDIR:-/tmp}
[ -d "$SCRATCH_ROOT" ] || { printf 'build-suites: owned scratch root is missing\n' >&2; exit 1; }
[ -d "$TMP_ROOT" ] || { printf 'build-suites: invocation temporary root is missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH='' cd -P -- "$SCRATCH_ROOT" && pwd -P)
TMP_REAL=$(CDPATH='' cd -P -- "$TMP_ROOT" && pwd -P)
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
if [ "$MODE" = build ]; then mkdir -p -- "$OUT"; fi
[ -d "$OUT" ] || { printf 'build-suites: verification output directory is missing\n' >&2; exit 1; }
OUT_REAL=$(CDPATH='' cd -P -- "$OUT" && pwd -P)
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
  printf '%s\n' "$actual"
}

LYNX_PIN=$(pin_value lynx-tests)
CC65_PIN=$(pin_value cc65)
LYNX_SOURCE_URL=$(pin_url lynx-tests)
LYNX_REVISION=$(printf '%s\n' "$LYNX_SOURCE_URL" | sed 's#.*/archive/##; s/\.tar\.gz$//')
[ "$(cat "$SUITES_DIR/.lynx-pin")" = "$LYNX_PIN" ] || { printf 'build-suites: lynx-tests pin marker mismatch\n' >&2; exit 1; }
[ "$(cat "$CC65_DIR/.lynx-pin")" = "$CC65_PIN" ] || { printf 'build-suites: cc65 pin marker mismatch\n' >&2; exit 1; }
if [ ! -x "$CC65_DIR/bin/ca65" ] || [ ! -x "$CC65_DIR/bin/ld65" ]; then
  printf 'build-suites: pinned ca65/ld65 are missing; scripts/check.sh owns preparation\n' >&2
  exit 1
fi
if [ ! -f "$CC65_DIR/.lynx-build" ] || [ ! -f "$CC65_DIR/.lynx-build-files" ]; then
  printf 'build-suites: verified cc65 build records are missing\n' >&2
  exit 1
fi
if ! awk -v pin="$CC65_PIN" '
  $1 == "pin_sha256" && $2 == "=" { pin_seen++; pin_value = $3 }
  $1 == "recipe" && $2 == "=" { recipe_seen++; recipe_value = $3 }
  $1 == "build_seconds" && $2 == "=" { seconds_seen++; seconds_value = $3 }
  END {
    if (pin_seen != 1 || pin_value != pin || recipe_seen != 1 || recipe_value != "1" ||
        seconds_seen != 1 || seconds_value !~ /^(0|[1-9][0-9]*)$/) exit 1
  }
' "$CC65_DIR/.lynx-build"; then
  printf 'build-suites: local cc65 build record is not a successful pinned recipe-1 build\n' >&2
  exit 1
fi
CA65_SHA=$(verify_tool bin/ca65)
LD65_SHA=$(verify_tool bin/ld65)

CPU_WRAPPER_SHA=$(sha256 "$ROOT/fixtures/suites/cpu-wrapper.s")
TIMERS_WRAPPER_SHA=$(sha256 "$ROOT/fixtures/suites/timers-wrapper.s")
AUDIO_WRAPPER_SHA=$(sha256 "$ROOT/fixtures/suites/audio-wrapper.s")
AUDIO2_WRAPPER_SHA=$(sha256 "$ROOT/fixtures/suites/audio2-wrapper.s")
LAYOUT_SHA=$(sha256 "$ROOT/fixtures/suites/bll.cfg")
BUILDER_SHA=$(sha256 "$ROOT/scripts/build-suites.sh")
CC65_BUILD_SHA=$(sha256 "$CC65_DIR/.lynx-build")
CC65_BUILD_FILES_SHA=$(sha256 "$CC65_DIR/.lynx-build-files")

CPU_BEFORE=$(verify_pinned_file cpu/tests.s)
TIMERS_BEFORE=$(verify_pinned_file timers/tests.s)
_AUDIO_BEFORE=$(verify_pinned_file audio/tests.s)
_AUDIO2_BEFORE=$(verify_pinned_file audio2/tests.s)
CPU_MAIN_BEFORE=$(verify_pinned_file cpu/main.c)
TIMERS_MAIN_BEFORE=$(verify_pinned_file timers/main.c)
AUDIO_MAIN_BEFORE=$(verify_pinned_file audio/main.c)
AUDIO2_MAIN_BEFORE=$(verify_pinned_file audio2/main.c)
LICENSE_SHA=$(verify_pinned_file LICENSE)

record_value() {
  key=$1
  awk -F= -v key="$key" '$1 == key { print substr($0, length(key) + 2); found++ } END { if (found != 1) exit 1 }' "$OUT/build-record.txt"
}

expect_record() {
  key=$1
  expected=$2
  actual=$(record_value "$key") || {
    printf 'build-suites: build record missing or duplicates %s\n' "$key" >&2
    return 1
  }
  [ "$actual" = "$expected" ] || {
    printf 'build-suites: stale build identity: %s\n' "$key" >&2
    return 1
  }
}

if [ "$MODE" = verify ]; then
  [ -f "$OUT/build-record.txt" ] || { printf 'build-suites: build record missing\n' >&2; exit 1; }
  if [ ! -f "$OUT/NOTICE" ]; then
    printf 'build-suites: copied source notice missing\n' >&2
    exit 1
  fi
  if ! cmp "$SUITES_DIR/LICENSE" "$OUT/NOTICE"; then
    printf 'build-suites: copied source notice mismatch\n' >&2
    exit 1
  fi
  expect_record lynx_tests_revision "$LYNX_REVISION"
  expect_record lynx_tests_pin "$LYNX_PIN"
  expect_record cc65_pin "$CC65_PIN"
  expect_record cc65_build_record_sha256 "$CC65_BUILD_SHA"
  expect_record cc65_build_files_sha256 "$CC65_BUILD_FILES_SHA"
  expect_record ca65_tool_sha256 "$CA65_SHA"
  expect_record ld65_tool_sha256 "$LD65_SHA"
  expect_record build_script_sha256 "$BUILDER_SHA"
  expect_record linker_layout_sha256 "$LAYOUT_SHA"
  expect_record cpu_wrapper_sha256 "$CPU_WRAPPER_SHA"
  expect_record timers_wrapper_sha256 "$TIMERS_WRAPPER_SHA"
  expect_record audio_wrapper_sha256 "$AUDIO_WRAPPER_SHA"
  expect_record audio2_wrapper_sha256 "$AUDIO2_WRAPPER_SHA"
  expect_record cpu_source_sha256 "$CPU_BEFORE"
  expect_record timers_source_sha256 "$TIMERS_BEFORE"
  expect_record audio_source_sha256 "$_AUDIO_BEFORE"
  expect_record audio2_source_sha256 "$_AUDIO2_BEFORE"
  expect_record cpu_main_sha256 "$CPU_MAIN_BEFORE"
  expect_record timers_main_sha256 "$TIMERS_MAIN_BEFORE"
  expect_record audio_main_sha256 "$AUDIO_MAIN_BEFORE"
  expect_record audio2_main_sha256 "$AUDIO2_MAIN_BEFORE"
  expect_record license_sha256 "$LICENSE_SHA"
  for suite in cpu timers audio audio2; do
    [ -f "$OUT/$suite.bll" ] || { printf 'build-suites: stale object missing: %s.bll\n' "$suite" >&2; exit 1; }
    [ -f "$OUT/$suite.lbl" ] || { printf 'build-suites: stale labels missing: %s.lbl\n' "$suite" >&2; exit 1; }
    expect_record "${suite}_object_sha256" "$(sha256 "$OUT/$suite.bll")" || {
      printf 'build-suites: stale object: %s.bll\n' "$suite" >&2
      exit 1
    }
    expect_record "${suite}_labels_sha256" "$(sha256 "$OUT/$suite.lbl")" || {
      printf 'build-suites: stale labels: %s.lbl\n' "$suite" >&2
      exit 1
    }
  done
  printf 'build-suites: verified local objects, labels and source/tool identities\n'
  exit 0
fi

rm -f "$OUT/cpu.bll" "$OUT/cpu.lbl" "$OUT/cpu.o" \
  "$OUT/timers.bll" "$OUT/timers.lbl" "$OUT/timers.o" \
  "$OUT/audio.bll" "$OUT/audio.lbl" "$OUT/audio.o" \
  "$OUT/audio2.bll" "$OUT/audio2.lbl" "$OUT/audio2.o" \
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
  if [ ! -s "$OUT/$suite.bll" ] || [ ! -s "$OUT/$suite.lbl" ]; then
    printf 'build-suites: missing linked output for %s\n' "$suite" >&2
    return 1
  fi
}

build_suite cpu
build_suite timers
build_suite audio
build_suite audio2
CPU_AFTER=$(verify_pinned_file cpu/tests.s)
TIMERS_AFTER=$(verify_pinned_file timers/tests.s)
_AUDIO_AFTER=$(verify_pinned_file audio/tests.s)
_AUDIO2_AFTER=$(verify_pinned_file audio2/tests.s)
CPU_MAIN_AFTER=$(verify_pinned_file cpu/main.c)
TIMERS_MAIN_AFTER=$(verify_pinned_file timers/main.c)
AUDIO_MAIN_AFTER=$(verify_pinned_file audio/main.c)
AUDIO2_MAIN_AFTER=$(verify_pinned_file audio2/main.c)
if [ "$CPU_BEFORE" != "$CPU_AFTER" ] || [ "$TIMERS_BEFORE" != "$TIMERS_AFTER" ] ||
  [ "$_AUDIO_BEFORE" != "$_AUDIO_AFTER" ] || [ "$_AUDIO2_BEFORE" != "$_AUDIO2_AFTER" ] ||
  [ "$CPU_MAIN_BEFORE" != "$CPU_MAIN_AFTER" ] ||
  [ "$TIMERS_MAIN_BEFORE" != "$TIMERS_MAIN_AFTER" ] ||
  [ "$AUDIO_MAIN_BEFORE" != "$AUDIO_MAIN_AFTER" ] ||
  [ "$AUDIO2_MAIN_BEFORE" != "$AUDIO2_MAIN_AFTER" ]; then
  printf 'build-suites: pinned upstream assembly changed during build\n' >&2
  exit 1
fi

{
  printf 'lynx_tests_revision=%s\n' "$LYNX_REVISION"
  printf 'lynx_tests_pin=%s\n' "$LYNX_PIN"
  printf 'cc65_pin=%s\n' "$CC65_PIN"
  printf 'cc65_build_record_sha256=%s\n' "$CC65_BUILD_SHA"
  printf 'cc65_build_files_sha256=%s\n' "$CC65_BUILD_FILES_SHA"
  printf 'ca65_tool_sha256=%s\n' "$CA65_SHA"
  printf 'ld65_tool_sha256=%s\n' "$LD65_SHA"
  printf 'build_script_sha256=%s\n' "$BUILDER_SHA"
  printf 'linker_layout_sha256=%s\n' "$LAYOUT_SHA"
  printf 'cpu_main_sha256=%s\n' "$CPU_MAIN_AFTER"
  printf 'timers_main_sha256=%s\n' "$TIMERS_MAIN_AFTER"
  printf 'audio_main_sha256=%s\n' "$AUDIO_MAIN_AFTER"
  printf 'audio2_main_sha256=%s\n' "$AUDIO2_MAIN_AFTER"
  printf 'cpu_wrapper_sha256=%s\n' "$CPU_WRAPPER_SHA"
  printf 'timers_wrapper_sha256=%s\n' "$TIMERS_WRAPPER_SHA"
  printf 'audio_wrapper_sha256=%s\n' "$AUDIO_WRAPPER_SHA"
  printf 'audio2_wrapper_sha256=%s\n' "$AUDIO2_WRAPPER_SHA"
  printf 'cpu_source_sha256=%s\n' "$CPU_AFTER"
  printf 'timers_source_sha256=%s\n' "$TIMERS_AFTER"
  printf 'audio_source_sha256=%s\n' "$_AUDIO_AFTER"
  printf 'audio2_source_sha256=%s\n' "$_AUDIO2_AFTER"
  printf 'license_sha256=%s\n' "$LICENSE_SHA"
  printf 'cpu_object_sha256=%s\n' "$(sha256 "$OUT/cpu.bll")"
  printf 'cpu_labels_sha256=%s\n' "$(sha256 "$OUT/cpu.lbl")"
  printf 'timers_object_sha256=%s\n' "$(sha256 "$OUT/timers.bll")"
  printf 'timers_labels_sha256=%s\n' "$(sha256 "$OUT/timers.lbl")"
  printf 'audio_object_sha256=%s\n' "$(sha256 "$OUT/audio.bll")"
  printf 'audio_labels_sha256=%s\n' "$(sha256 "$OUT/audio.lbl")"
  printf 'audio2_object_sha256=%s\n' "$(sha256 "$OUT/audio2.bll")"
  printf 'audio2_labels_sha256=%s\n' "$(sha256 "$OUT/audio2.lbl")"
} > "$OUT/build-record.txt"
cat "$OUT/build-record.txt"
