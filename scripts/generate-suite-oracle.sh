#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
SOURCE_ARG=
OUT_ARG=
SHAPE_ONLY=0

usage() {
  printf 'usage: sh scripts/generate-suite-oracle.sh --source DIR --out FILE\n' >&2
  exit 2
}
while [ "$#" -gt 0 ]; do
  case "$1" in
    --source) [ "$#" -ge 2 ] && [ -z "$SOURCE_ARG" ] || usage; SOURCE_ARG=$2; shift 2 ;;
    --out) [ "$#" -ge 2 ] && [ -z "$OUT_ARG" ] || usage; OUT_ARG=$2; shift 2 ;;
    --test-source-shape-only) [ "$SHAPE_ONLY" -eq 0 ] || usage; SHAPE_ONLY=1; shift ;;
    *) usage ;;
  esac
done
[ -n "$SOURCE_ARG" ] || usage
[ "$SHAPE_ONLY" -eq 1 ] || [ -n "$OUT_ARG" ] || usage
[ "$SHAPE_ONLY" -eq 0 ] || [ -z "$OUT_ARG" ] || usage

case "$SOURCE_ARG" in /*) SOURCE=$SOURCE_ARG ;; *) SOURCE=$ROOT/$SOURCE_ARG ;; esac
[ -d "$SOURCE" ] || { printf 'suite oracle: source directory missing\n' >&2; exit 1; }
SOURCE=$(CDPATH='' cd -P -- "$SOURCE" && pwd -P)

# This parser is also exercised against private mutated copies by the CTest
# source-shape controls. Production generation always verifies every input
# against the exact archive pin and the extracted per-file manifest below.
parse_source() {
  suite=$1
  main=$2
  asm=$3
  awk -v suite="$suite" -v mainfile="$main" -v asmfile="$asm" '
    function reject(message) {
      print "suite oracle: source shape: " message > "/dev/stderr"
      bad = 1
      exit 1
    }
    function hexval(text, i, ch, n, value) {
      if (text !~ /^0[xX][0-9A-Fa-f]+$/) return -1
      text = substr(text, 3)
      value = 0
      for (i = 1; i <= length(text); i++) {
        ch = tolower(substr(text, i, 1))
        n = index("0123456789abcdef", ch) - 1
        value = value * 16 + n
      }
      return value <= 255 ? value : -1
    }
    function decimal_values(text, out, allow_hex, count, parts, i, value) {
      gsub(/[[:space:]]/, "", text)
      sub(/^[^{]*\{/, "", text)
      sub(/\}[^}]*$/, "", text)
      if (text == "") return -1
      count = split(text, parts, ",")
      for (i = 1; i <= count; i++) {
        if (parts[i] ~ /^0[xX]/ && allow_hex) value = hexval(parts[i])
        else if (parts[i] ~ /^(0|[1-9][0-9]*)$/) value = parts[i] + 0
        else return -1
        if (value < 0) return -1
        if (value > 255) return -1
        out[i] = value
      }
      return count
    }
    function bytehex(value) { return sprintf("%02x", value) }
    function consume_expected(text, rest, token, args, n, i, value) {
      gsub(/\/\*[^*]*\*\//, "", text)
      rest = text
      while (match(text, /EXPECT\([^()]*\)/)) {
        token = substr(text, RSTART + 7, RLENGTH - 8)
        gsub(/[[:space:]]/, "", token)
        n = split(token, args, ",")
        if (n < 1 || n > 8) reject("unsupported EXPECT value count")
        for (i = 1; i <= n; i++) {
          value = hexval(args[i])
          if (value < 0) reject("unexpected EXPECT token or nonbyte value")
          for (prior = 1; prior < i; prior++) {
            if (values_for_expect[prior] == value) reject("duplicate EXPECT alternative")
          }
          values_for_expect[i] = value
          expected[expect_count, i] = value
        }
        expect_sizes[expect_count] = n
        expect_count++
        text = substr(text, RSTART + RLENGTH)
      }
      gsub(/EXPECT\([^()]*\)/, "", rest)
      gsub(/[[:space:]{},;]/, "", rest)
      if (rest != "") reject("unexpected token in EXPECT array")
    }
    BEGIN {
      result_count = 0
      test_count = 0
      array_open = 0
      found_expect = 0
      found_offsets = 0
      found_counts = 0
      found_i = 0
      found_ii = 0
      assembly_extent = 0
      expect_count = 0
    }
    FILENAME == mainfile {
      if ($0 ~ /^[[:space:]]*#define[[:space:]]+RESULT_COUNT[[:space:]]+[0-9]+[[:space:]]*$/) {
        line = $0
        gsub(/[[:space:]]/, "", line)
        sub(/^#defineRESULT_COUNT/, "", line)
        if (result_count != 0) reject("duplicate RESULT_COUNT")
        result_count = line + 0
      }
      if ($0 ~ /^[[:space:]]*#define[[:space:]]+TEST_COUNT[[:space:]]+[0-9]+[[:space:]]*$/) {
        line = $0
        gsub(/[[:space:]]/, "", line)
        sub(/^#defineTEST_COUNT/, "", line)
        if (test_count != 0) reject("duplicate TEST_COUNT")
        test_count = line + 0
      }
      if (index($0, "k_expected_results[RESULT_COUNT]") != 0) {
        if (found_expect != 0) reject("duplicate EXPECT array")
        found_expect = 1
        array_open = 1
        next
      }
      if (array_open != 0) {
        expected_text = expected_text $0 "\n"
        if ($0 ~ /};/) {
          array_open = 0
          consume_expected(expected_text)
        }
      }
      if (suite == "cpu" && index($0, "k_lynx_i_results[4]") != 0) {
        line = $0
        sub(/^.*=/, "", line)
        if (decimal_values(line, model_i, 1) != 4 || found_i != 0) reject("CPU Lynx I tuple shape")
        found_i = 1
      }
      if (suite == "cpu" && index($0, "k_lynx_ii_results[4]") != 0) {
        line = $0
        sub(/^.*=/, "", line)
        if (decimal_values(line, model_ii, 1) != 4 || found_ii != 0) reject("CPU Lynx II tuple shape")
        found_ii = 1
      }
      if (index($0, "k_test_offsets[TEST_COUNT]") != 0) {
        line = $0
        sub(/^.*=/, "", line)
        if (decimal_values(line, offsets) < 1 || found_offsets != 0) reject("test-offset shape")
        found_offsets = 1
        offset_count = decimal_values(line, offsets_copy)
      }
      if (index($0, "k_test_counts[TEST_COUNT]") != 0) {
        line = $0
        sub(/^.*=/, "", line)
        if (decimal_values(line, test_lengths) < 1 || found_counts != 0) reject("test-count shape")
        found_counts = 1
        length_count = decimal_values(line, lengths_copy)
      }
      next
    }
    FILENAME == asmfile {
      line = $0
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
      if (line ~ /^_g_results:[[:space:]]*\.res[[:space:]]+[0-9]+$/) {
        sub(/^_g_results:[[:space:]]*\.res[[:space:]]+/, "", line)
        if (assembly_extent != 0) reject("duplicate assembly result extent")
        assembly_extent = line + 0
      }
    }
    END {
      if (bad) exit 1
      if (array_open != 0 || found_expect != 1) reject("missing or truncated EXPECT array")
      if (result_count != 18 || assembly_extent != 18) reject("result extent must be 18 bytes")
      if (found_offsets != 1 || found_counts != 1 || offset_count != test_count ||
          length_count != test_count || offset_count != length_count) reject("test offsets/counts shape")
      if (suite == "cpu") {
        meaningful = 18
        if (expect_count != 18 || found_i != 1 || found_ii != 1) reject("CPU must have 18 expectations and both model tuples")
      } else if (suite == "timers") {
        meaningful = 16
        if (expect_count != 16) reject("timers must have 16 meaningful expectations")
        if (expect_sizes[8] != 2 || expect_sizes[11] != 3) reject("accepted EXPECT alternatives lost")
      } else reject("unknown suite")
      for (i = 1; i <= test_count; i++) {
        start = offsets[i]
        span = test_lengths[i]
        if (span < 1 || start + span > meaningful) reject("test extent exceeds meaningful results")
        for (j = start; j < start + span; j++) if (covered[j]++) reject("overlapping test result offsets")
      }
      for (i = 0; i < meaningful; i++) if (covered[i] != 1) reject("missing test result offset")
      if (suite == "cpu") {
        for (i = 1; i <= 4; i++) {
          if (expect_sizes[11+i] != 2 ||
              model_i[i] != expected[11+i, 1] || model_ii[i] != expected[11+i, 2]) {
            reject("CPU model tuple differs from accepted EXPECT values")
          }
        }
      }
      printf("suite\t%s\n", suite)
      printf("result_count\t%d\n", result_count)
      printf("extent\t%d\n", assembly_extent)
      printf("meaningful\t%d\n", meaningful)
      if (suite == "cpu") {
        tuple = ""
        for (i = 1; i <= 4; i++) tuple = tuple (i == 1 ? "" : ",") bytehex(model_i[i])
        printf("tuple\tlynx1\t%s\n", tuple)
        tuple = ""
        for (i = 1; i <= 4; i++) tuple = tuple (i == 1 ? "" : ",") bytehex(model_ii[i])
        printf("tuple\tlynx2\t%s\n", tuple)
      }
      for (i = 0; i < expect_count; i++) {
        printf("result\t%d\t%d", i, expect_sizes[i])
        for (j = 1; j <= expect_sizes[i]; j++) printf("\t%s", bytehex(expected[i, j]))
        printf("\n")
      }
    }
  ' "$main" "$asm"
}

CPU_MAIN=$SOURCE/cpu/main.c
CPU_ASM=$SOURCE/cpu/tests.s
TIMER_MAIN=$SOURCE/timers/main.c
TIMER_ASM=$SOURCE/timers/tests.s
if [ "$SHAPE_ONLY" -eq 1 ]; then
  parse_source cpu "$CPU_MAIN" "$CPU_ASM" >/dev/null
  parse_source timers "$TIMER_MAIN" "$TIMER_ASM" >/dev/null
  exit 0
fi

sha256() { shasum -a 256 "$1" | awk '{print $1}'; }
PIN=$(awk '$1 == "lynx-tests" { print $4; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt")
REVISION=$(awk '$1 == "lynx-tests" { print $3; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt" | sed 's#.*/archive/##; s/\.tar\.gz$//')
CANONICAL=$ROOT/build/pins/lynx-tests
[ "$SOURCE" = "$CANONICAL" ] || { printf 'suite oracle: production source must be the pinned build/pins/lynx-tests tree\n' >&2; exit 1; }
[ "$(cat "$SOURCE/.lynx-pin")" = "$PIN" ] || { printf 'suite oracle: source pin marker mismatch\n' >&2; exit 1; }
[ -f "$SOURCE/.lynx-pin-files" ] || { printf 'suite oracle: source file manifest missing\n' >&2; exit 1; }
for relative in cpu/main.c cpu/tests.s timers/main.c timers/tests.s LICENSE; do
  [ -f "$SOURCE/$relative" ] || { printf 'suite oracle: pinned source missing: %s\n' "$relative" >&2; exit 1; }
  actual=$(sha256 "$SOURCE/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$SOURCE/.lynx-pin-files")
  [ "$actual" = "$expected" ] || { printf 'suite oracle: pinned source hash mismatch: %s\n' "$relative" >&2; exit 1; }
done

case "$OUT_ARG" in /*) OUT=$OUT_ARG ;; *) OUT=$ROOT/$OUT_ARG ;; esac
OUT_PARENT=$(dirname -- "$OUT")
[ -d "$OUT_PARENT" ] && [ ! -L "$OUT_PARENT" ] || { printf 'suite oracle: output parent missing or symlinked\n' >&2; exit 1; }
PARENT_REAL=$(CDPATH='' cd -P -- "$OUT_PARENT" && pwd -P)
SCRATCH_ROOT=$ROOT/build/check/scratch
[ -d "$SCRATCH_ROOT" ] || { printf 'suite oracle: owned output root missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH='' cd -P -- "$SCRATCH_ROOT" && pwd -P)
case "$PARENT_REAL" in "$SCRATCH_REAL"|"$SCRATCH_REAL"/*) ;; *) printf 'suite oracle: output outside owned scratch root\n' >&2; exit 1 ;; esac
OUT=$PARENT_REAL/$(basename -- "$OUT")
[ ! -L "$OUT" ] || { printf 'suite oracle: refusing symlink output\n' >&2; exit 1; }
rm -f -- "$OUT"
TEMP=$OUT.$$
trap 'rm -f -- "$TEMP"' EXIT HUP INT TERM

{
  printf 'LYNX-SUITE-ORACLE\t1\n'
  printf 'archive\t%s\nrevision\t%s\n' "$PIN" "$REVISION"
  printf 'cpu_main\t%s\ncpu_asm\t%s\n' "$(sha256 "$CPU_MAIN")" "$(sha256 "$CPU_ASM")"
  printf 'timers_main\t%s\ntimers_asm\t%s\n' "$(sha256 "$TIMER_MAIN")" "$(sha256 "$TIMER_ASM")"
  printf 'license\t%s\n' "$(sha256 "$SOURCE/LICENSE")"
  parse_source cpu "$CPU_MAIN" "$CPU_ASM"
  parse_source timers "$TIMER_MAIN" "$TIMER_ASM"
} > "$TEMP"
mv -- "$TEMP" "$OUT"
trap - EXIT HUP INT TERM
printf 'suite oracle generated: %s\n' "$OUT"
