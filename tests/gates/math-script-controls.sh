#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd -P)
SCRATCH_ROOT=$ROOT/build/check/scratch
[ -d "$SCRATCH_ROOT" ] || { printf 'math script controls: owned scratch root missing\n' >&2; exit 1; }
WORK=$(mktemp -d "$SCRATCH_ROOT/math-script-controls.XXXXXXXX")
trap 'rm -rf -- "$WORK"' EXIT HUP INT TERM
FIXTURE=$WORK/project
SOURCE=$FIXTURE/build/pins/lynx-tests
CC65=$FIXTURE/build/pins/cc65
OUTPUT=$FIXTURE/build/check/scratch/math-positive
TEST_BINARY=${1:-}
ORACLE=${2:-}

fail() { printf 'math script controls: %s\n' "$1" >&2; exit 1; }
mark_case() { printf '%s\n' "$1" >> "$WORK/cases"; }
expect_failure() {
  label=$1 expected=$2
  shift 2
  if output=$("$@" 2>&1); then fail "$label unexpectedly succeeded: $output"; else result=$?; fi
  [ "$result" -eq 1 ] || fail "$label exited $result instead of 1: $output"
  case "$output" in *"$expected"*) printf 'math script control: refused %s\n' "$label";; *) fail "$label omitted '$expected': $output";; esac
}
expect_success() {
  label=$1 expected=$2
  shift 2
  if output=$("$@" 2>&1); then
    case "$output" in *"$expected"*) printf 'math script control: accepted %s\n' "$label";; *) fail "$label omitted '$expected': $output";; esac
  else result=$?; fail "$label exited $result: $output"; fi
}
shape_failure() {
  label=$1 expected=$2 script=$3
  expect_failure "$label" "$expected" sh "$script" --source "$SOURCE" --include-math-memio --test-source-shape-only
}
clean_shape() {
  expect_success 'restored source shape bracket' '' sh "$FIXTURE/scripts/generate-suite-oracle.sh" \
    --source "$SOURCE" --include-math-memio --test-source-shape-only
}

mkdir -p "$FIXTURE/build/pins" "$FIXTURE/build/check/scratch" "$FIXTURE/fixtures/suites" "$FIXTURE/scripts"
cp -R "$ROOT/build/pins/lynx-tests" "$ROOT/build/pins/cc65" "$FIXTURE/build/pins/"
cp "$ROOT/pins.txt" "$FIXTURE/pins.txt"
cp "$ROOT/scripts/build-suites.sh" "$ROOT/scripts/generate-suite-oracle.sh" "$FIXTURE/scripts/"
cp "$ROOT/fixtures/suites/math-wrapper.s" "$ROOT/fixtures/suites/memio-wrapper.s" \
  "$ROOT/fixtures/suites/cpu-wrapper.s" "$ROOT/fixtures/suites/timers-wrapper.s" \
  "$ROOT/fixtures/suites/audio-wrapper.s" "$ROOT/fixtures/suites/audio2-wrapper.s" \
  "$ROOT/fixtures/suites/bll.cfg" "$ROOT/fixtures/suites/NOTICE" "$FIXTURE/fixtures/suites/"
math_main=$SOURCE/math/main.c
memio_main=$SOURCE/memio/main.c
cp "$math_main" "$WORK/math-main.clean"
cp "$memio_main" "$WORK/memio-main.clean"
clean_shape

expect_success 'clean source build bracket' 'math_labels_sha256=' sh "$FIXTURE/scripts/build-suites.sh" --out "$OUTPUT"
expect_success 'clean object/source identity bracket' 'verified local objects, labels and source/tool identities' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"

sed -e 's/#define TEST_COUNT 8/#define TEST_COUNT 7/' \
  -e 's/{ 0, 2, 4, 6, 8, 10, 12, 14 }/{ 0, 2, 4, 6, 8, 10, 12 }/' \
  -e 's/{ 2, 2, 2, 2, 2, 2, 2, 2 }/{ 2, 2, 2, 2, 2, 2, 2 }/' \
  -e '/    "TIMING"/d' "$WORK/math-main.clean" > "$math_main"
shape_failure 'deleted math timing Test8' 'math must have an 18-byte extent, eight groups and 18 expectations' "$FIXTURE/scripts/generate-suite-oracle.sh"
mark_case math.test8_deleted
cp "$WORK/math-main.clean" "$math_main"
clean_shape

sed 's/{ 0, 2, 4, 6, 8, 10, 12, 14 }/{ 0, 2, 4, 6, 8, 10, 12, 12 }/' \
  "$WORK/math-main.clean" > "$math_main"
shape_failure 'duplicate math result group' 'overlapping test result offsets' "$FIXTURE/scripts/generate-suite-oracle.sh"
mark_case math.duplicate_group
cp "$WORK/math-main.clean" "$math_main"
clean_shape

sed 's/#define RESULT_COUNT 18/#define RESULT_COUNT 17/' "$WORK/math-main.clean" > "$math_main"
shape_failure 'altered math result extent' 'result extent must be 18 bytes' "$FIXTURE/scripts/generate-suite-oracle.sh"
mark_case math.altered_extent
cp "$WORK/math-main.clean" "$math_main"

awk '
  !inserted && /EXPECT\(0xFF\)/ { sub(/,/, ", EXPECT(0x01),"); inserted = 1 }
  { print }
  END { if (!inserted) exit 1 }
' "$WORK/math-main.clean" > "$math_main"
shape_failure 'extra math result expectation' 'math must have an 18-byte extent, eight groups and 18 expectations' "$FIXTURE/scripts/generate-suite-oracle.sh"
mark_case math.extra_result
cp "$WORK/math-main.clean" "$math_main"

sed 's/EXPECT(0xFF)/EXPECT(0xGG)/' "$WORK/math-main.clean" > "$math_main"
shape_failure 'malformed math expectation' 'unexpected EXPECT token or nonbyte value' "$FIXTURE/scripts/generate-suite-oracle.sh"
mark_case math.malformed_expect
cp "$WORK/math-main.clean" "$math_main"
clean_shape

# Memio's final three initializer entries are deliberately outside its 13
# meaningful group bytes. Changing one must not expand or redefine coverage.
sed 's/EXPECT(0x00), EXPECT(0x00), EXPECT(0x00),/EXPECT(0x00), EXPECT(0x00), EXPECT(0x01),/' \
  "$WORK/memio-main.clean" > "$memio_main"
expect_success 'unused memio initializer excluded from result groups' '' \
  sh "$FIXTURE/scripts/generate-suite-oracle.sh" --source "$SOURCE" --include-math-memio --test-source-shape-only
mark_case memio.unused_initializer
cp "$WORK/memio-main.clean" "$memio_main"

printf '\n/* private pinned source identity mutation */\n' >> "$math_main"
expect_failure 'changed pinned math source identity' 'pinned source hash mismatch: math/main.c' \
  sh "$FIXTURE/scripts/build-suites.sh" --out "$FIXTURE/build/check/scratch/identity-build"
expect_failure 'oracle rejects changed pinned math source identity' 'pinned source hash mismatch: math/main.c' \
  sh "$FIXTURE/scripts/generate-suite-oracle.sh" --source "$SOURCE" --include-math-memio --out "$FIXTURE/build/check/scratch/oracles/changed.tsv"
mark_case math.source_identity
cp "$WORK/math-main.clean" "$math_main"

cp "$OUTPUT/math.bll" "$WORK/math.bll.clean"
printf '\001' >> "$OUTPUT/math.bll"
expect_failure 'stale math object identity' 'stale object: math.bll' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case math.stale_object
cp "$WORK/math.bll.clean" "$OUTPUT/math.bll"
cp "$OUTPUT/memio.lbl" "$WORK/memio.lbl.clean"
printf '\n; mutated label control\n' >> "$OUTPUT/memio.lbl"
expect_failure 'stale memio label identity' 'stale labels: memio.lbl' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case memio.stale_labels
cp "$WORK/memio.lbl.clean" "$OUTPUT/memio.lbl"

wrapper=$FIXTURE/fixtures/suites/math-wrapper.s
cp "$wrapper" "$WORK/math-wrapper.clean"
printf '\n; stale wrapper control\n' >> "$wrapper"
expect_failure 'stale math wrapper identity' 'stale build identity: math_wrapper_sha256' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case math.stale_wrapper
cp "$WORK/math-wrapper.clean" "$wrapper"

build_record=$OUTPUT/build-record.txt
cp "$build_record" "$WORK/build-record.clean"
sed '/^math_wrapper_sha256=/d' "$WORK/build-record.clean" > "$build_record"
expect_failure 'incomplete math build record' 'build record missing or duplicates math_wrapper_sha256' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case math.incomplete_build_record
cp "$WORK/build-record.clean" "$build_record"

cp "$CC65/.lynx-build" "$WORK/cc65-build.clean"
sed 's/^recipe = 1$/recipe = 9/' "$WORK/cc65-build.clean" > "$CC65/.lynx-build"
expect_failure 'changed local tool build identity' 'not a successful pinned recipe-1 build' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case tool.changed_recipe
cp "$WORK/cc65-build.clean" "$CC65/.lynx-build"

cp "$OUTPUT/NOTICE" "$WORK/notice.clean"
printf '\nmutated notice\n' >> "$OUTPUT/NOTICE"
expect_failure 'changed copied license notice' 'copied source notice mismatch' sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
mark_case license.changed_notice
cp "$WORK/notice.clean" "$OUTPUT/NOTICE"
expect_success 'restored build identity bracket' 'verified local objects, labels and source/tool identities' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"

if [ -n "$TEST_BINARY" ]; then
  [ -n "$ORACLE" ] || fail 'math/memio oracle path not supplied'
  oracle=$ORACLE
  expect_success 'canonical math/memio oracle' 'exact source groups, meaningful rows and reserved extents valid' \
    "$TEST_BINARY" validate-math-memio-oracle "$oracle"
  cp "$oracle" "$WORK/oracle.clean"
  sed '/^reserved[[:space:]]13[[:space:]]5$/d' "$WORK/oracle.clean" > "$oracle"
  expect_failure 'omitted memio reserved extent' 'result extent or reserved range mismatch' \
    "$TEST_BINARY" validate-math-memio-oracle "$oracle"
  mark_case memio.missing_reserved_range
fi

expected_cases='math.test8_deleted
math.duplicate_group
math.altered_extent
math.extra_result
math.malformed_expect
memio.unused_initializer
math.source_identity
math.stale_object
memio.stale_labels
math.stale_wrapper
math.incomplete_build_record
tool.changed_recipe
license.changed_notice
memio.missing_reserved_range'
actual_cases=$(cat "$WORK/cases")
[ "$actual_cases" = "$expected_cases" ] || fail "source-control manifest missing, duplicated or reordered: $actual_cases"

printf 'math script controls: 14 unique source/build identity subcases passed\n'
