#!/bin/sh
set -eu

REPO_ROOT=$1
WORK=$2
FIXTURE="$WORK/project"
SOURCE="$FIXTURE/build/pins/lynx-tests"
CC65="$FIXTURE/build/pins/cc65"
SCRATCH="$FIXTURE/build/check/scratch"
BUILD_SCRIPT="$FIXTURE/scripts/build-page-mode.sh"
ORACLE_SCRIPT="$FIXTURE/scripts/generate-page-mode-oracle.sh"

fail() {
  printf 'page-mode script controls: %s\n' "$1" >&2
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
    *"$expected"*) printf 'page-mode script control: refused %s\n' "$label" ;;
    *) fail "$label omitted '$expected': $output" ;;
  esac
}

expect_success() {
  label=$1
  expected=$2
  shift 2
  if output=$("$@" 2>&1); then
    case "$output" in
      *"$expected"*) printf 'page-mode script control: accepted %s\n' "$label" ;;
      *) fail "$label omitted '$expected': $output" ;;
    esac
  else
    result=$?
    fail "$label exited $result: $output"
  fi
}

run_build_failure() {
  label=$1
  expected=$2
  out=$3
  expect_failure "builder $label" "$expected" sh "$BUILD_SCRIPT" --out "$out"
}

run_oracle_failure() {
  label=$1
  expected=$2
  out=$3
  expect_failure "oracle $label" "$expected" sh "$ORACLE_SCRIPT" \
    --source "$SOURCE" --out "$out"
}

rewrite_marker() {
  marker=$1
  relative=$2
  replacement=$3
  temporary="$marker.control-tmp"
  awk -v path="$relative" -v value="$replacement" \
    '$2 == path { $1=value; found++ } { print } END { if (found != 1) exit 1 }' \
    "$marker" > "$temporary" || fail "could not mutate exactly one marker row: $relative"
  mv "$temporary" "$marker"
}

mkdir -p "$FIXTURE/build/pins" "$FIXTURE/build/check/scratch" \
  "$FIXTURE/fixtures/suites" "$FIXTURE/fixtures/manifests" "$FIXTURE/scripts"
cp -R "$REPO_ROOT/build/pins/lynx-tests" "$FIXTURE/build/pins/"
cp -R "$REPO_ROOT/build/pins/cc65" "$FIXTURE/build/pins/"
cp "$REPO_ROOT/pins.txt" "$FIXTURE/pins.txt"
cp "$REPO_ROOT/fixtures/suites/page-mode-wrapper.s" "$FIXTURE/fixtures/suites/"
cp "$REPO_ROOT/fixtures/suites/bll.cfg" "$FIXTURE/fixtures/suites/"
cp "$REPO_ROOT/fixtures/manifests/page-mode-tests.manifest" "$FIXTURE/fixtures/manifests/"
cp "$REPO_ROOT/scripts/build-page-mode.sh" "$FIXTURE/scripts/"
cp "$REPO_ROOT/scripts/generate-page-mode-oracle.sh" "$FIXTURE/scripts/"

expect_success "positive page-mode build bracket" "result_count=6" \
  sh "$BUILD_SCRIPT" --out "$SCRATCH/positive-build"
expect_success "positive page-mode oracle bracket" "LYNX-PAGE-MODE-ORACLE" \
  sh "$ORACLE_SCRIPT" --source "$SOURCE" --out "$SCRATCH/positive-oracle.tsv"

main="$SOURCE/page-mode/main.c"
tests="$SOURCE/page-mode/tests.s"
notice="$SOURCE/LICENSE"
ca65="$CC65/bin/ca65"
main_saved="$WORK/main.c.saved"
tests_saved="$WORK/tests.s.saved"
notice_saved="$WORK/LICENSE.saved"
ca65_saved="$WORK/ca65.saved"
cp "$main" "$main_saved"
cp "$tests" "$tests_saved"
cp "$notice" "$notice_saved"
cp "$ca65" "$ca65_saved"

mv "$tests" "$tests.control-missing"
run_build_failure "missing source" "pinned source missing: page-mode/tests.s" "$SCRATCH/missing-source-build"
run_oracle_failure "missing source" "pinned source missing: page-mode/tests.s" "$SCRATCH/missing-source-oracle.tsv"
mv "$tests.control-missing" "$tests"

mv "$notice" "$notice.control-missing"
run_build_failure "missing notice" "pinned source missing: LICENSE" "$SCRATCH/missing-notice-build"
run_oracle_failure "missing notice" "pinned source missing: LICENSE" "$SCRATCH/missing-notice-oracle.tsv"
mv "$notice.control-missing" "$notice"

mv "$ca65" "$ca65.control-missing"
run_build_failure "missing tool" "pinned tool missing: bin/ca65" "$SCRATCH/missing-tool-build"
run_oracle_failure "missing tool" "pinned tool missing: bin/ca65" "$SCRATCH/missing-tool-oracle.tsv"
mv "$ca65.control-missing" "$ca65"

printf '\n/* isolated source mutation */\n' >> "$main"
run_build_failure "modified source" "pinned source hash mismatch: page-mode/main.c" "$SCRATCH/modified-source-build"
run_oracle_failure "modified source" "pinned source hash mismatch: page-mode/main.c" "$SCRATCH/modified-source-oracle.tsv"
cp "$main_saved" "$main"

printf '\n; isolated tool mutation\n' >> "$ca65"
run_build_failure "modified tool" "pinned cc65 tool hash mismatch: bin/ca65" "$SCRATCH/modified-tool-build"
run_oracle_failure "modified tool" "pinned cc65 tool mismatch: bin/ca65" "$SCRATCH/modified-tool-oracle.tsv"
cp "$ca65_saved" "$ca65"

pin_files="$SOURCE/.lynx-pin-files"
pin_files_saved="$WORK/lynx-pin-files.saved"
cp "$pin_files" "$pin_files_saved"
rewrite_marker "$pin_files" "page-mode/main.c" 0000000000000000000000000000000000000000000000000000000000000000
run_build_failure "modified source hash marker" "pinned source hash mismatch: page-mode/main.c" "$SCRATCH/modified-source-marker-build"
run_oracle_failure "modified source hash marker" "pinned source hash mismatch: page-mode/main.c" "$SCRATCH/modified-source-marker-oracle.tsv"
cp "$pin_files_saved" "$pin_files"

build_files="$CC65/.lynx-build-files"
build_files_saved="$WORK/lynx-build-files.saved"
cp "$build_files" "$build_files_saved"
rewrite_marker "$build_files" "bin/ca65" 0000000000000000000000000000000000000000000000000000000000000000
run_build_failure "modified tool hash marker" "pinned cc65 tool hash mismatch: bin/ca65" "$SCRATCH/modified-tool-marker-build"
run_oracle_failure "modified tool hash marker" "pinned cc65 tool mismatch: bin/ca65" "$SCRATCH/modified-tool-marker-oracle.tsv"
cp "$build_files_saved" "$build_files"

source_pin="$SOURCE/.lynx-pin"
cc65_pin="$CC65/.lynx-pin"
source_pin_saved="$WORK/lynx-pin.saved"
cc65_pin_saved="$WORK/cc65-pin.saved"
cp "$source_pin" "$source_pin_saved"
cp "$cc65_pin" "$cc65_pin_saved"
printf 'wrong-source-pin\n' > "$source_pin"
run_build_failure "modified source pin marker" "lynx-tests pin marker mismatch" "$SCRATCH/modified-source-pin-build"
run_oracle_failure "modified source pin marker" "lynx-tests pin marker mismatch" "$SCRATCH/modified-source-pin-oracle.tsv"
cp "$source_pin_saved" "$source_pin"
printf 'wrong-cc65-pin\n' > "$cc65_pin"
run_build_failure "modified tool pin marker" "cc65 pin marker mismatch" "$SCRATCH/modified-tool-pin-build"
run_oracle_failure "modified tool pin marker" "cc65 pin marker mismatch" "$SCRATCH/modified-tool-pin-oracle.tsv"
cp "$cc65_pin_saved" "$cc65_pin"

malformed="$WORK/malformed-source"
mkdir -p "$malformed/page-mode"
cp "$main_saved" "$malformed/page-mode/main.c"
cp "$tests_saved" "$malformed/page-mode/tests.s"
cp "$notice_saved" "$malformed/LICENSE"
sed 's/EXPECT(0x74, 0x75)/EXPECT(0x73, 0x75)/' "$malformed/page-mode/main.c" > "$malformed/main.changed"
cmp -s "$malformed/page-mode/main.c" "$malformed/main.changed" && fail "malformed source control did not mutate the alternative"
mv "$malformed/main.changed" "$malformed/page-mode/main.c"
expect_failure "shape-only malformed source alternative" \
  "source alternative at result 4 must remain EXPECT(0x74,0x75)" \
  sh "$ORACLE_SCRIPT" --source "$malformed" --test-source-shape-only

mkdir -p "$FIXTURE/build/check/outside" "$WORK/external"
printf 'preserve outside target\n' > "$WORK/external/target.txt"
cp "$WORK/external/target.txt" "$WORK/external/target.before"
mkdir "$SCRATCH/stale-build"
printf 'nonce=stale\npath=stale\n' > "$SCRATCH/stale-build/.lynx-page-owner"
run_build_failure "stale owner output" "output already exists; refusing unowned reuse" "$SCRATCH/stale-build"
ln -s "$WORK/external" "$SCRATCH/symlink-build"
run_build_failure "symlink output" "output already exists; refusing unowned reuse" "$SCRATCH/symlink-build"
ln -s "$WORK/external" "$SCRATCH/symlink-parent"
run_build_failure "symlink output ancestor" "refusing symlink output ancestor" "$SCRATCH/symlink-parent/build"
run_build_failure "outside output" "output is outside invocation-owned scratch" "$FIXTURE/build/check/outside/build"

printf 'stale\n' > "$SCRATCH/stale-oracle.tsv.owner"
run_oracle_failure "stale owner output" "refusing existing output or owner marker" "$SCRATCH/stale-oracle.tsv"
printf 'preserve oracle target\n' > "$WORK/external/oracle-target.tsv"
cp "$WORK/external/oracle-target.tsv" "$WORK/external/oracle-target.before"
ln -s "$WORK/external/oracle-target.tsv" "$SCRATCH/symlink-oracle.tsv"
run_oracle_failure "symlink output" "refusing existing output or owner marker" "$SCRATCH/symlink-oracle.tsv"
ln -s "$WORK/external" "$SCRATCH/symlink-oracle-parent"
run_oracle_failure "symlink output ancestor" "refusing symlink output ancestor" "$SCRATCH/symlink-oracle-parent/oracle.tsv"
run_oracle_failure "outside output" "output is outside invocation-owned scratch" "$FIXTURE/build/check/outside/oracle.tsv"
cmp -s "$WORK/external/target.txt" "$WORK/external/target.before" || fail "builder followed a symlink target"
cmp -s "$WORK/external/oracle-target.tsv" "$WORK/external/oracle-target.before" || fail "oracle followed a symlink target"

expect_success "restored page-mode build bracket" "result_count=6" \
  sh "$BUILD_SCRIPT" --out "$SCRATCH/restored-build"
expect_success "restored page-mode oracle bracket" "LYNX-PAGE-MODE-ORACLE" \
  sh "$ORACLE_SCRIPT" --source "$SOURCE" --out "$SCRATCH/restored-oracle.tsv"
printf 'page-mode script controls: isolated source, tool, marker, notice, shape, and output hostility controls refused; positive brackets remained valid\n'
