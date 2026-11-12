#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
CC65_DIR="$ROOT/build/pins/cc65"
SUITES_DIR="$ROOT/build/pins/lynx-tests"
OUT_ARG=

usage() {
  printf 'usage: sh scripts/build-page-mode.sh --out DIR\n' >&2
  exit 2
}
while [ "$#" -gt 0 ]; do
  case "$1" in
    --out) [ "$#" -ge 2 ] && [ -z "$OUT_ARG" ] || usage; OUT_ARG=$2; shift 2 ;;
    *) usage ;;
  esac
done
[ -n "$OUT_ARG" ] || usage
case "$OUT_ARG" in
  /) printf 'page-mode build: refusing root output path\n' >&2; exit 1 ;;
  *'/../'*|../*|*/..|..) printf 'page-mode build: refusing parent traversal in output path\n' >&2; exit 1 ;;
esac
case "$OUT_ARG" in /*) OUT_PATH=$OUT_ARG ;; *) OUT_PATH=$ROOT/$OUT_ARG ;; esac
OUT_PARENT=$(dirname -- "$OUT_PATH")
OUT_BASE=$(basename -- "$OUT_PATH")
[ -d "$OUT_PARENT" ] || { printf 'page-mode build: output parent must already exist\n' >&2; exit 1; }
cursor=$OUT_PARENT
while :; do
  [ ! -L "$cursor" ] || { printf 'page-mode build: refusing symlink output ancestor\n' >&2; exit 1; }
  parent=$(dirname -- "$cursor")
  [ "$parent" != "$cursor" ] || break
  cursor=$parent
done
PARENT_REAL=$(CDPATH='' cd -P -- "$OUT_PARENT" && pwd -P)
SCRATCH_ROOT="$ROOT/build/check/scratch"
[ -d "$SCRATCH_ROOT" ] || { printf 'page-mode build: invocation-owned scratch root is missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH='' cd -P -- "$SCRATCH_ROOT" && pwd -P)
case "$PARENT_REAL" in "$SCRATCH_REAL"|"$SCRATCH_REAL"/*) ;; *) printf 'page-mode build: output is outside invocation-owned scratch\n' >&2; exit 1 ;; esac
OUT="$PARENT_REAL/$OUT_BASE"
[ ! -e "$OUT" ] && [ ! -L "$OUT" ] || { printf 'page-mode build: output already exists; refusing unowned reuse\n' >&2; exit 1; }

sha256() { shasum -a 256 "$1" | awk '{print $1}'; }
pin_value() { awk -v name="$1" '$1 == name { print $4; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"; }
pin_url() { awk -v name="$1" '$1 == name { print $3; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"; }
verify_pinned_file() {
  relative=$1
  [ -f "$SUITES_DIR/$relative" ] || { printf 'page-mode build: pinned source missing: %s\n' "$relative" >&2; return 1; }
  actual=$(sha256 "$SUITES_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$SUITES_DIR/.lynx-pin-files")
  [ "$actual" = "$expected" ] || { printf 'page-mode build: pinned source hash mismatch: %s\n' "$relative" >&2; return 1; }
  printf '%s\n' "$actual"
}
verify_tool() {
  relative=$1
  [ -x "$CC65_DIR/$relative" ] || { printf 'page-mode build: pinned tool missing: %s\n' "$relative" >&2; return 1; }
  actual=$(sha256 "$CC65_DIR/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$CC65_DIR/.lynx-build-files")
  [ "$actual" = "$expected" ] || { printf 'page-mode build: pinned cc65 tool hash mismatch: %s\n' "$relative" >&2; return 1; }
  printf '%s\n' "$actual"
}

LYNX_PIN=$(pin_value lynx-tests)
CC65_PIN=$(pin_value cc65)
LYNX_URL=$(pin_url lynx-tests)
LYNX_REVISION=$(printf '%s\n' "$LYNX_URL" | sed 's#.*/archive/##; s/\.tar\.gz$//')
[ "$(cat "$SUITES_DIR/.lynx-pin")" = "$LYNX_PIN" ] || { printf 'page-mode build: lynx-tests pin marker mismatch\n' >&2; exit 1; }
[ "$(cat "$CC65_DIR/.lynx-pin")" = "$CC65_PIN" ] || { printf 'page-mode build: cc65 pin marker mismatch\n' >&2; exit 1; }
[ -f "$CC65_DIR/.lynx-build" ] && [ -f "$CC65_DIR/.lynx-build-files" ] || { printf 'page-mode build: verified cc65 build records are missing\n' >&2; exit 1; }
CA65_SHA=$(verify_tool bin/ca65)
LD65_SHA=$(verify_tool bin/ld65)
MAIN_BEFORE=$(verify_pinned_file page-mode/main.c)
TESTS_BEFORE=$(verify_pinned_file page-mode/tests.s)
NOTICE_SHA=$(verify_pinned_file LICENSE)
WRAPPER_SHA=$(sha256 "$ROOT/fixtures/suites/page-mode-wrapper.s")
MANIFEST_SHA=$(sha256 "$ROOT/fixtures/manifests/page-mode-tests.manifest")
LAYOUT_SHA=$(sha256 "$ROOT/fixtures/suites/bll.cfg")
RECIPE_SHA=$(sha256 "$ROOT/scripts/build-page-mode.sh")

mkdir -- "$OUT"
OUT_REAL=$(CDPATH='' cd -P -- "$OUT" && pwd -P)
[ "$OUT_REAL" = "$OUT" ] || { printf 'page-mode build: output resolved through a symlink\n' >&2; exit 1; }
nonce=$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')
[ "${#nonce}" -eq 48 ] || { printf 'page-mode build: invocation nonce unavailable\n' >&2; exit 1; }
(umask 077; printf 'nonce=%s\npath=%s\n' "$nonce" "$OUT" > "$OUT/.lynx-page-owner")
cp "$SUITES_DIR/LICENSE" "$OUT/NOTICE"
cmp "$SUITES_DIR/LICENSE" "$OUT/NOTICE" || { printf 'page-mode build: source notice copy mismatch\n' >&2; exit 1; }

(
  cd "$SUITES_DIR/page-mode"
  env -i PATH=/usr/bin:/bin LC_ALL=C CC65_HOME="$CC65_DIR" \
    "$CC65_DIR/bin/ca65" --cpu 65C02 -I "$SUITES_DIR/page-mode" -I "$CC65_DIR/asminc" \
      -o "$OUT/page-mode.o" "$ROOT/fixtures/suites/page-mode-wrapper.s"
)
env -i PATH=/usr/bin:/bin LC_ALL=C CC65_HOME="$CC65_DIR" \
  "$CC65_DIR/bin/ld65" -C "$ROOT/fixtures/suites/bll.cfg" \
    -Ln "$OUT/page-mode.lbl" -o "$OUT/page-mode.bll" "$OUT/page-mode.o"
[ -s "$OUT/page-mode.bll" ] && [ -s "$OUT/page-mode.lbl" ] || { printf 'page-mode build: linked BLL or labels missing\n' >&2; exit 1; }
MAIN_AFTER=$(verify_pinned_file page-mode/main.c)
TESTS_AFTER=$(verify_pinned_file page-mode/tests.s)
NOTICE_AFTER=$(verify_pinned_file LICENSE)
[ "$MAIN_BEFORE" = "$MAIN_AFTER" ] && [ "$TESTS_BEFORE" = "$TESTS_AFTER" ] && [ "$NOTICE_SHA" = "$NOTICE_AFTER" ] || {
  printf 'page-mode build: pinned source changed during build\n' >&2; exit 1;
}
CA65_AFTER=$(verify_tool bin/ca65)
LD65_AFTER=$(verify_tool bin/ld65)
[ "$CA65_SHA" = "$CA65_AFTER" ] && [ "$LD65_SHA" = "$LD65_AFTER" ] || {
  printf 'page-mode build: pinned tools changed during build\n' >&2; exit 1;
}
[ "$WRAPPER_SHA" = "$(sha256 "$ROOT/fixtures/suites/page-mode-wrapper.s")" ] &&
  [ "$MANIFEST_SHA" = "$(sha256 "$ROOT/fixtures/manifests/page-mode-tests.manifest")" ] &&
  [ "$LAYOUT_SHA" = "$(sha256 "$ROOT/fixtures/suites/bll.cfg")" ] &&
  [ "$RECIPE_SHA" = "$(sha256 "$ROOT/scripts/build-page-mode.sh")" ] || {
  printf 'page-mode build: recipe, wrapper, manifest, or layout changed during build\n' >&2; exit 1;
}

{
  printf 'lynx_tests_revision=%s\n' "$LYNX_REVISION"
  printf 'lynx_tests_pin=%s\ncc65_pin=%s\n' "$LYNX_PIN" "$CC65_PIN"
  printf 'main_sha256=%s\ntests_sha256=%s\nnotice_sha256=%s\n' "$MAIN_AFTER" "$TESTS_AFTER" "$NOTICE_AFTER"
  printf 'wrapper_sha256=%s\nmanifest_sha256=%s\nlayout_sha256=%s\nrecipe_sha256=%s\n' "$WRAPPER_SHA" "$MANIFEST_SHA" "$LAYOUT_SHA" "$RECIPE_SHA"
  printf 'ca65_sha256=%s\nld65_sha256=%s\n' "$CA65_SHA" "$LD65_SHA"
  printf 'object_sha256=%s\nlabels_sha256=%s\n' "$(sha256 "$OUT/page-mode.bll")" "$(sha256 "$OUT/page-mode.lbl")"
  printf 'result_count=6\nresult_extent=6\nstartup_tier=BS93-A\nprofiles=lynx1,lynx2\n'
} > "$OUT/build-record.txt"
cat "$OUT/build-record.txt"
