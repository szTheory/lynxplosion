#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
SOURCE_ARG=
OUT_ARG=
SHAPE_ONLY=0
usage() { printf 'usage: sh scripts/generate-page-mode-oracle.sh --source DIR --out FILE [--test-source-shape-only]\n' >&2; exit 2; }
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
[ -d "$SOURCE" ] || { printf 'page-mode oracle: source directory missing\n' >&2; exit 1; }
SOURCE=$(CDPATH='' cd -P -- "$SOURCE" && pwd -P)

sha256() { shasum -a 256 "$1" | awk '{print $1}'; }
pin_value() { awk -v name="$1" '$1 == name { print $4; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"; }
pin_url() { awk -v name="$1" '$1 == name { print $3; found++ } END { if (found != 1) exit 1 }' "$ROOT/pins.txt"; }
verify_source() {
  relative=$1
  [ -f "$SOURCE/$relative" ] || { printf 'page-mode oracle: pinned source missing: %s\n' "$relative" >&2; return 1; }
  actual=$(sha256 "$SOURCE/$relative")
  expected=$(awk -v path="$relative" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$SOURCE/.lynx-pin-files")
  [ "$actual" = "$expected" ] || { printf 'page-mode oracle: pinned source hash mismatch: %s\n' "$relative" >&2; return 1; }
  printf '%s\n' "$actual"
}
if [ "$SHAPE_ONLY" -eq 0 ]; then
  EXPECTED_SOURCE=$(CDPATH='' cd -P -- "$ROOT/build/pins/lynx-tests" && pwd -P)
  [ "$SOURCE" = "$EXPECTED_SOURCE" ] || { printf 'page-mode oracle: production source is not the pinned checkout\n' >&2; exit 1; }
  LYNX_PIN=$(pin_value lynx-tests)
  CC65_PIN=$(pin_value cc65)
  [ "$(cat "$SOURCE/.lynx-pin")" = "$LYNX_PIN" ] || { printf 'page-mode oracle: lynx-tests pin marker mismatch\n' >&2; exit 1; }
  CC65_DIR="$ROOT/build/pins/cc65"
  [ "$(cat "$CC65_DIR/.lynx-pin")" = "$CC65_PIN" ] || { printf 'page-mode oracle: cc65 pin marker mismatch\n' >&2; exit 1; }
  for tool in bin/ca65 bin/ld65; do
    [ -x "$CC65_DIR/$tool" ] || { printf 'page-mode oracle: pinned tool missing: %s\n' "$tool" >&2; exit 1; }
    actual=$(sha256 "$CC65_DIR/$tool")
    expected=$(awk -v path="$tool" '$2 == path { print $1; found++ } END { if (found != 1) exit 1 }' "$CC65_DIR/.lynx-build-files")
    [ "$actual" = "$expected" ] || { printf 'page-mode oracle: pinned cc65 tool mismatch: %s\n' "$tool" >&2; exit 1; }
    if [ "$tool" = bin/ca65 ]; then CA65_BEFORE=$actual; else LD65_BEFORE=$actual; fi
  done
  MAIN_SHA=$(verify_source page-mode/main.c)
  TESTS_SHA=$(verify_source page-mode/tests.s)
  NOTICE_SHA=$(verify_source LICENSE)
  REVISION=$(pin_url lynx-tests | sed 's#.*/archive/##; s/\.tar\.gz$//')
  RECIPE_SHA=$(sha256 "$ROOT/scripts/generate-page-mode-oracle.sh")
else
  MAIN_SHA=$(sha256 "$SOURCE/page-mode/main.c")
  TESTS_SHA=$(sha256 "$SOURCE/page-mode/tests.s")
  NOTICE_SHA=$(sha256 "$SOURCE/LICENSE")
  REVISION=test-source-shape-only
  RECIPE_SHA=$(sha256 "$ROOT/scripts/generate-page-mode-oracle.sh")
fi

PARSED=$(awk -v mainfile="$SOURCE/page-mode/main.c" -v asmfile="$SOURCE/page-mode/tests.s" '
  function reject(message) { print "page-mode oracle: source shape: " message > "/dev/stderr"; bad=1; exit 1 }
  function byte_value(text,    i,ch,n,v) {
    if (text !~ /^0[xX][0-9A-Fa-f][0-9A-Fa-f]$/) return -1
    v=0
    for (i=3;i<=4;i++) { ch=tolower(substr(text,i,1)); n=index("0123456789abcdef",ch)-1; v=v*16+n }
    return v
  }
  function collect_expected(text,    rest,token,parts,n,i,v) {
    gsub(/\/\*[^*]*\*\//,"",text); rest=text
    while (match(text,/EXPECT\([^()]*\)/)) {
      token=substr(text,RSTART+7,RLENGTH-8); gsub(/[[:space:]]/,"",token)
      n=split(token,parts,","); if (n<1 || n>2) reject("unsupported EXPECT alternatives")
      result_count++
      for (i=1;i<=n;i++) {
        v=byte_value(parts[i]); if (v<0) reject("malformed EXPECT byte")
        for (j=1;j<i;j++) if (values[result_count-1,j]==v) reject("duplicate EXPECT alternative")
        values[result_count-1,i]=v
      }
      sizes[result_count-1]=n
      text=substr(text,RSTART+RLENGTH)
    }
    gsub(/EXPECT\([^()]*\)/,"",rest); gsub(/[[:space:]{},;]/,"",rest)
    if (rest!="") reject("unexpected token in EXPECT array")
  }
  BEGIN { result_count=0; expected_open=0; expected_seen=0; offsets_seen=0; counts_seen=0; result_extent=0; main_count=0; test_count=0; main_defs=0; test_defs=0 }
  FILENAME==mainfile {
    if ($0 ~ /^[[:space:]]*#define[[:space:]]+RESULT_COUNT[[:space:]]+[0-9]+[[:space:]]*$/) { if(main_defs++) reject("duplicate RESULT_COUNT"); line=$0; gsub(/[[:space:]]/,"",line); sub(/^#defineRESULT_COUNT/,"",line); main_count=line+0 }
    if ($0 ~ /^[[:space:]]*#define[[:space:]]+TEST_COUNT[[:space:]]+[0-9]+[[:space:]]*$/) { if(test_defs++) reject("duplicate TEST_COUNT"); line=$0; gsub(/[[:space:]]/,"",line); sub(/^#defineTEST_COUNT/,"",line); test_count=line+0 }
    if (index($0,"k_expected_results[RESULT_COUNT]")!=0) { if(expected_seen++) reject("duplicate EXPECT array"); expected_open=1; next }
    if (expected_open) { expected_text=expected_text $0 "\n"; if($0~/};/) { expected_open=0; collect_expected(expected_text) } }
    if (index($0,"k_test_offsets[TEST_COUNT]")!=0) { if(offsets_seen++) reject("duplicate offset array"); line=$0; sub(/^.*=/,"",line); gsub(/[[:space:]{};]/,"",line); offset_n=split(line,offsets,","); for(i=1;i<=offset_n;i++) if(offsets[i]!~/^(0|[1-9][0-9]*)$/) reject("malformed decimal test offset") }
    if (index($0,"k_test_counts[TEST_COUNT]")!=0) { if(counts_seen++) reject("duplicate count array"); line=$0; sub(/^.*=/,"",line); gsub(/[[:space:]{};]/,"",line); count_n=split(line,counts,","); for(i=1;i<=count_n;i++) if(counts[i]!~/^(0|[1-9][0-9]*)$/) reject("malformed decimal test count") }
    next
  }
  FILENAME==asmfile {
    line=$0; gsub(/^[[:space:]]+|[[:space:]]+$/,"",line)
    if(line~/^_g_results:[[:space:]]*\.res[[:space:]]+[0-9]+$/) { sub(/^_g_results:[[:space:]]*\.res[[:space:]]+/,"",line); if(result_extent++) reject("duplicate assembly result extent"); result_extent=line+0 }
  }
  END {
    if(bad) exit 1
    if(expected_open || expected_seen!=1) reject("missing or truncated EXPECT array")
    if(main_defs!=1 || test_defs!=1 || main_count!=6 || test_count!=6 || result_count!=6 || result_extent!=6) reject("page-mode result/test extent must be six")
    if(offsets_seen!=1 || counts_seen!=1 || offset_n!=6 || count_n!=6) reject("six offset/count rows required")
    for(i=1;i<=6;i++) if(offsets[i]!=(i-1) || counts[i]!=1) reject("offset/count shape must be six contiguous single-byte rows")
    if(sizes[0]!=1 || sizes[1]!=1 || sizes[2]!=1 || sizes[3]!=1 || sizes[4]!=2 || sizes[5]!=1 || values[4,1]!=116 || values[4,2]!=117) reject("source alternative at result 4 must remain EXPECT(0x74,0x75)")
    for(i=0;i<6;i++) { printf "result\t%d\t%d",i,sizes[i]; for(j=1;j<=sizes[i];j++) printf "\t%02x",values[i,j]; printf "\n" }
  }
' "$SOURCE/page-mode/main.c" "$SOURCE/page-mode/tests.s") || exit 1

if [ "$SHAPE_ONLY" -eq 0 ]; then
  [ "$MAIN_SHA" = "$(verify_source page-mode/main.c)" ] &&
    [ "$TESTS_SHA" = "$(verify_source page-mode/tests.s)" ] &&
    [ "$NOTICE_SHA" = "$(verify_source LICENSE)" ] || {
    printf 'page-mode oracle: pinned source changed during parsing\n' >&2; exit 1;
  }
  CA65_AFTER=$(sha256 "$CC65_DIR/bin/ca65")
  LD65_AFTER=$(sha256 "$CC65_DIR/bin/ld65")
  [ "$CA65_BEFORE" = "$CA65_AFTER" ] && [ "$LD65_BEFORE" = "$LD65_AFTER" ] || {
    printf 'page-mode oracle: pinned tools changed during parsing\n' >&2; exit 1;
  }
  [ "$RECIPE_SHA" = "$(sha256 "$ROOT/scripts/generate-page-mode-oracle.sh")" ] || {
    printf 'page-mode oracle: recipe changed during parsing\n' >&2; exit 1;
  }
fi

if [ "$SHAPE_ONLY" -eq 1 ]; then
  printf '%s\n' "$PARSED"
  exit 0
fi

case "$OUT_ARG" in /*) OUT_PATH=$OUT_ARG ;; *) OUT_PATH=$ROOT/$OUT_ARG ;; esac
OUT_PARENT=$(dirname -- "$OUT_PATH")
[ -d "$OUT_PARENT" ] || { printf 'page-mode oracle: output parent must already exist\n' >&2; exit 1; }
cursor=$OUT_PARENT
while :; do [ ! -L "$cursor" ] || { printf 'page-mode oracle: refusing symlink output ancestor\n' >&2; exit 1; }; parent=$(dirname -- "$cursor"); [ "$parent" != "$cursor" ] || break; cursor=$parent; done
PARENT_REAL=$(CDPATH='' cd -P -- "$OUT_PARENT" && pwd -P)
SCRATCH_ROOT="$ROOT/build/check/scratch"
[ -d "$SCRATCH_ROOT" ] || { printf 'page-mode oracle: invocation-owned scratch root is missing\n' >&2; exit 1; }
SCRATCH_REAL=$(CDPATH='' cd -P -- "$SCRATCH_ROOT" && pwd -P)
case "$PARENT_REAL" in "$SCRATCH_REAL"|"$SCRATCH_REAL"/*) ;; *) printf 'page-mode oracle: output is outside invocation-owned scratch\n' >&2; exit 1 ;; esac
OUT="$PARENT_REAL/$(basename -- "$OUT_PATH")"
[ ! -e "$OUT" ] && [ ! -L "$OUT" ] && [ ! -e "$OUT.owner" ] || { printf 'page-mode oracle: refusing existing output or owner marker\n' >&2; exit 1; }
nonce=$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')
[ "${#nonce}" -eq 48 ] || { printf 'page-mode oracle: invocation nonce unavailable\n' >&2; exit 1; }
tmp="$OUT.tmp-$nonce"
{
  printf 'LYNX-PAGE-MODE-ORACLE\t1\n'
  printf 'revision\t%s\nmain_sha256\t%s\ntests_sha256\t%s\nnotice_sha256\t%s\nrecipe_sha256\t%s\n' "$REVISION" "$MAIN_SHA" "$TESTS_SHA" "$NOTICE_SHA" "$RECIPE_SHA"
  printf 'result_count\t6\ntest_count\t6\nextent\t6\n'
  printf '%s\n' "$PARSED"
} > "$tmp"
(umask 077; printf 'nonce=%s\npath=%s\n' "$nonce" "$OUT" > "$OUT.owner")
mv -- "$tmp" "$OUT"
cat "$OUT"
