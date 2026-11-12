#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd -P)
SCRATCH_ROOT=$ROOT/build/check/scratch
[ -d "$SCRATCH_ROOT" ] || { printf 'audio script controls: owned scratch root missing\n' >&2; exit 1; }
WORK=$(mktemp -d "$SCRATCH_ROOT/audio-script-controls.XXXXXXXX")
trap 'rm -rf -- "$WORK"' EXIT HUP INT TERM
FIXTURE=$WORK/project
SOURCE=$FIXTURE/build/pins/lynx-tests
CC65=$FIXTURE/build/pins/cc65
OUTPUT=$FIXTURE/build/check/scratch/audio-positive

fail() {
  printf 'audio script controls: %s\n' "$1" >&2
  exit 1
}

expect_failure() {
  label=$1
  expected=$2
  shift 2
  if output=$("$@" 2>&1); then
    fail "$label unexpectedly succeeded: $output"
  else
    result=$?
  fi
  [ "$result" -eq 1 ] || fail "$label exited $result instead of 1: $output"
  case "$output" in
    *"$expected"*) printf 'audio script control: refused %s\n' "$label" ;;
    *) fail "$label omitted '$expected': $output" ;;
  esac
}

expect_success() {
  label=$1
  expected=$2
  shift 2
  if output=$("$@" 2>&1); then
    case "$output" in
      *"$expected"*) printf 'audio script control: accepted %s\n' "$label" ;;
      *) fail "$label omitted '$expected': $output" ;;
    esac
  else
    result=$?
    fail "$label exited $result: $output"
  fi
}

expect_shape_failure() {
  label=$1
  expected=$2
  script=$3
  expect_failure "$label" "$expected" sh "$script" --source "$SOURCE" --include-audio --test-source-shape-only
}

check_clean_shape() {
  expect_success 'clean source bracket' '' sh "$FIXTURE/scripts/generate-suite-oracle.sh" \
    --source "$SOURCE" --include-audio --test-source-shape-only
}

mkdir -p "$FIXTURE/build/pins" "$FIXTURE/build/check/scratch" \
  "$FIXTURE/fixtures/suites" "$FIXTURE/scripts" "$FIXTURE/build/check/scratch/oracles"
cp -R "$ROOT/build/pins/lynx-tests" "$FIXTURE/build/pins/"
cp -R "$ROOT/build/pins/cc65" "$FIXTURE/build/pins/"
cp "$ROOT/pins.txt" "$FIXTURE/pins.txt"
cp "$ROOT/scripts/build-suites.sh" "$FIXTURE/scripts/"
cp "$ROOT/scripts/generate-suite-oracle.sh" "$FIXTURE/scripts/"
cp "$ROOT/fixtures/suites/cpu-wrapper.s" "$ROOT/fixtures/suites/timers-wrapper.s" \
  "$ROOT/fixtures/suites/audio-wrapper.s" "$ROOT/fixtures/suites/audio2-wrapper.s" \
  "$ROOT/fixtures/suites/bll.cfg" "$ROOT/fixtures/suites/NOTICE" "$FIXTURE/fixtures/suites/"
cp "$ROOT/build/pins/lynx-tests/audio/main.c" "$WORK/audio-main.clean"
cp "$ROOT/build/pins/lynx-tests/audio2/main.c" "$WORK/audio2-main.clean"

check_clean_shape
expect_success 'clean source build bracket' 'audio2_object_sha256=' \
  sh "$FIXTURE/scripts/build-suites.sh" --out "$OUTPUT"
expect_success 'clean local build identity bracket' 'verified local objects, labels and source/tool identities' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
expect_success 'clean source oracle bracket' 'suite oracle generated' \
  sh "$FIXTURE/scripts/generate-suite-oracle.sh" --source "$SOURCE" --include-audio --out "$FIXTURE/build/check/scratch/oracles/positive.tsv"

cc65_build_record=$CC65/.lynx-build
cp "$cc65_build_record" "$WORK/cc65-build.clean"
sed 's/^recipe = 1$/recipe = 9/' "$WORK/cc65-build.clean" > "$cc65_build_record"
expect_failure 'unrecognized local tool build recipe' 'not a successful pinned recipe-1 build' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
cp "$WORK/cc65-build.clean" "$cc65_build_record"
expect_success 'restored local tool build identity bracket' 'verified local objects, labels and source/tool identities' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"

main=$SOURCE/audio/main.c
audio2_main=$SOURCE/audio2/main.c

sed 's/EXPECT(0x08), EXPECT(0x3F), EXPECT(0x00),/EXPECT(0x08), EXPECT(0x3F),/' \
  "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'missing audio result row' 'audio must have 18 expectations and six cases' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"
check_clean_shape

sed 's/{ 0, 3, 6, 11, 14, 15 }/{ 0, 3, 3, 11, 14, 15 }/' \
  "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'duplicate result-row mapping' 'overlapping test result offsets' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"
check_clean_shape

sed 's/EXPECT(0x08), EXPECT(0x3F), EXPECT(0x00),/EXPECT(0x08), EXPECT(0x3F), EXPECT(0x00), EXPECT(0x01),/' \
  "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'extra audio result row' 'audio must have 18 expectations and six cases' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"
awk '
  /k_expected_results\[RESULT_COUNT\]/ { in_expected = 1 }
  in_expected && /};/ { sub(/};/, ""); print; exit }
  { print }
' "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'truncated EXPECT array' 'missing or truncated EXPECT array' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"
check_clean_shape

sed 's/#define RESULT_COUNT 18/#define RESULT_COUNT 17/' "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'changed result extent' 'result extent must be 18 bytes' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"

sed 's/EXPECT(0x08)/EXPECT(0x100)/' "$WORK/audio-main.clean" > "$main"
expect_shape_failure 'unsupported EXPECT value' 'unexpected EXPECT token or nonbyte value' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio-main.clean" "$main"
check_clean_shape

sed 's/EXPECT(0x30, 0x34)/EXPECT(0x30)/' "$WORK/audio2-main.clean" > "$audio2_main"
expect_shape_failure 'lost audio2 index 9 alternative' 'audio2 index 9 source alternative 30|34 lost' "$FIXTURE/scripts/generate-suite-oracle.sh"
cp "$WORK/audio2-main.clean" "$audio2_main"
check_clean_shape

printf '\n/* private source identity mutation */\n' >> "$main"
expect_failure 'changed pinned audio source identity' 'pinned source hash mismatch: audio/main.c' \
  sh "$FIXTURE/scripts/generate-suite-oracle.sh" --source "$SOURCE" --include-audio --out "$FIXTURE/build/check/scratch/oracles/identity.tsv"
expect_failure 'builder rejects changed pinned audio source' 'pinned source hash mismatch: audio/main.c' \
  sh "$FIXTURE/scripts/build-suites.sh" --out "$FIXTURE/build/check/scratch/identity-build"
cp "$WORK/audio-main.clean" "$main"
expect_success 'restored source identity bracket' 'suite oracle generated' \
  sh "$FIXTURE/scripts/generate-suite-oracle.sh" --source "$SOURCE" --include-audio --out "$FIXTURE/build/check/scratch/oracles/restored.tsv"

cp "$OUTPUT/audio.bll" "$WORK/audio.bll.clean"
printf '\001' >> "$OUTPUT/audio.bll"
expect_failure 'stale audio object' 'stale object: audio.bll' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
cp "$WORK/audio.bll.clean" "$OUTPUT/audio.bll"
cp "$OUTPUT/audio2.lbl" "$WORK/audio2.lbl.clean"
printf '\n; stale labels control\n' >> "$OUTPUT/audio2.lbl"
expect_failure 'stale audio2 labels' 'stale labels: audio2.lbl' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"
cp "$WORK/audio2.lbl.clean" "$OUTPUT/audio2.lbl"
expect_success 'final clean object and label bracket' 'verified local objects, labels and source/tool identities' \
  sh "$FIXTURE/scripts/build-suites.sh" --verify "$OUTPUT"

printf 'audio script controls: source identity, object and label controls passed\n'
