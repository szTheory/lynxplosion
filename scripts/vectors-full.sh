#!/bin/sh
# Full vector lane for the pinned SingleStepTests/65x02 per-opcode data.
#
#   sh scripts/vectors-full.sh MODE [MODE...]
#
#   pin     download the 512 upstream files (two variants, 256 opcodes each),
#           write fixtures/vectors/sst65x02/upstream.sha256 (one SHA-256 per whole
#           upstream file, sorted) and the upstream LICENSE verbatim.
#   sample  verify every download against upstream.sha256, then write the two
#           committed sample containers fixtures/vectors/sst65x02/<variant>.sample
#           (the first 32 objects of each file, upstream bytes verbatim).
#   run     verify every download against upstream.sha256, rebuild both sample
#           containers and require them to equal the committed ones byte for
#           byte, then run all 10,000 vectors of each of the 256 whole files
#           through lynx-test-cpu-vectors (complete mode, the check preset's
#           build, always rebuilt here first: a stale or locally edited binary
#           must not carry the evidence of a 2 GB lane) under none (synertek data, 32 rows
#           asserted undefined), lynx1 (synertek) and lynx2 (rockwell), and the
#           two cross pairings (rockwell under lynx1, synertek under lynx2) to
#           record the full-data failing opcode sets. Exit 0 only if the three
#           same-profile runs have zero failures, both cross pairings fail on
#           exactly the 32 bit-operation opcodes ($x7 and $xF) and both samples
#           are equal. Results files (op, passed, failed per run row) are copied
#           to build/vectors-full/<name>.results with their SHA-256 printed.
#           This is the 2 GB network lane: run it by hand, never from the check.
#   build   only the build step of run: configure and build the check preset
#           (incremental), no network. tests/cpu/vector_controls.sh lane_script
#           runs it against a fake cmake to prove the runner is always rebuilt.
#
# Several modes in one invocation (for example "pin sample") run in the order
# given and share one work directory and one verified download set, so the large
# download happens once. This script is not part of scripts/check.sh and not of
# CTest: the check never touches the network (CONTEXT, "Vector sourcing,
# inventory and controls"). Run it at phase verification, on every pin bump and
# before any claim.
#
# Fault prevented: unpinned or tampered vector data reaching the repository or
# a run. URLs are commit-pinned and https only (redirects too), downloads are
# size-capped and land in a mktemp directory outside the repository and outside
# build/, every file is hashed against the pin record before it is read, file
# names are generated as two lowercase hex digits (never taken from a listing)
# and any download, hash or layout error fails closed with exit status 1. Exit
# status 2 is misuse. Only the two variants below are used.
set -eu
LC_ALL=C
export LC_ALL

REPO=SingleStepTests/65x02
COMMIT=2f6980a2d95757486c7bee24355c360e40e2a224
VARIANTS="synertek65c02 rockwell65c02"
HEXDIGITS="0 1 2 3 4 5 6 7 8 9 a b c d e f"
LICENSE_SHA256=5fb64d1f2e4d74fabc85c768e1f6ffa73b95dc93e217c55acfe7e4a89ba32b04
BASE=https://raw.githubusercontent.com/$REPO/$COMMIT

usage() {
  echo "usage: vectors-full.sh MODE [MODE...]   (MODE: pin | sample | run | build)" >&2
  exit 2
}

fail() {
  echo "vectors-full: $*" >&2
  exit 1
}

if [ $# -lt 1 ]; then
  usage
fi
for mode in "$@"; do
  case $mode in
    pin | sample | run | build) ;;
    *) usage ;;
  esac
done

here=$(cd "$(dirname "$0")" && pwd -P)
root=$(dirname "$here")
out=$root/fixtures/vectors/sst65x02
record=$out/upstream.sha256

# sha256 of one file (hex only), the same idiom as scripts/check.sh.
sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha_out=$(sha256sum "$1")
  else
    sha_out=$(shasum -a 256 "$1")
  fi
  printf '%s\n' "${sha_out%% *}"
}

# The work directory must be new, empty, and outside the repository (build/ is
# inside it); the trap removes it however the run ends.
work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-vectors.XXXXXX")
if [ -z "$work" ] || [ ! -d "$work" ]; then
  fail "could not create a work directory"
fi
work=$(cd "$work" && pwd -P)
trap 'rm -rf "$work"' EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$root"/*) fail "the work directory $work is inside the repository" ;;
esac

# Every expected path "<variant>/v1/<op>.json", variants in the order above.
list=$work/list
: >"$list"
for variant in $VARIANTS; do
  for h in $HEXDIGITS; do
    for l in $HEXDIGITS; do
      printf '%s/v1/%s%s.json\n' "$variant" "$h" "$l" >>"$list"
    done
  done
done
want_files=$(wc -l <"$list")
want_files=$((want_files + 0))
if [ "$want_files" -ne 512 ]; then
  fail "internal error: $want_files expected files, not 512"
fi

downloaded=0

# The record must exist, name the pinned commit and hold 512 rows.
check_record() {
  if [ ! -f "$record" ]; then
    fail "no pin record at fixtures/vectors/sst65x02/upstream.sha256; run the pin mode first"
  fi
  if ! grep -q "^# commit $COMMIT\$" "$record"; then
    fail "the pin record does not name commit $COMMIT"
  fi
  rows=$(awk '$1 !~ /^#/ { n++ } END { print n + 0 }' "$record")
  if [ "$rows" -ne 512 ]; then
    fail "the pin record has $rows rows, not 512"
  fi
}

# Download every file once per invocation, size-capped, https only. Any failure
# aborts the run naming the file.
ensure_downloads() {
  if [ "$downloaded" -eq 1 ]; then
    return 0
  fi
  for variant in $VARIANTS; do
    mkdir -p "$work/dl/$variant/v1"
  done
  started=$(date +%s)
  LV_WORK=$work
  LV_BASE=$BASE
  export LV_WORK LV_BASE
  # shellcheck disable=SC2016 # the child shell, not this one, expands the variables
  xargs -P 8 -n 1 sh -c 'curl -fsSL --proto "=https" --proto-redir "=https" --max-filesize 8000000 --retry 2 -o "$LV_WORK/dl/$1" -- "$LV_BASE/$1" || { echo "vectors-full: download failed: $1" >&2; exit 1; }' sh <"$list" || fail "a download failed"
  while IFS= read -r rel; do
    if [ ! -s "$work/dl/$rel" ]; then
      fail "missing or empty download: $rel"
    fi
  done <"$list"
  bytes=$(find "$work/dl" -type f -exec cat {} + | wc -c)
  bytes=$((bytes + 0))
  elapsed=$(($(date +%s) - started))
  echo "vectors-full: downloaded $want_files files, $bytes bytes, in $elapsed s"
  downloaded=1
}

# Verify one download against the pin record; the record must name the path
# exactly once with a 64-digit lowercase hex digest.
verify_against_record() {
  rel=$1
  want=$(awk -v p="$rel" '$1 !~ /^#/ && $2 == p { print $1 }' "$record")
  case $want in
    '' | *[!0-9a-f]*)
      fail "the pin record has no unique 64-digit digest for $rel"
      ;;
  esac
  if [ "${#want}" -ne 64 ]; then
    fail "the pin record has no unique 64-digit digest for $rel"
  fi
  got=$(sha_of "$work/dl/$rel")
  if [ "$got" != "$want" ]; then
    fail "sha256 mismatch for $rel: the pin record has $want, the download is $got"
  fi
}

# Build one sample container on stdout: three header lines, then per opcode a
# "#file" header and the first lines of the verified upstream file. L is 33 when
# line 1 is exactly "[" (standard layout) and 32 when line 1 starts with
# '[{ "name"' (regenerated layout): the first 32 objects in both.
build_sample() {
  variant=$1
  printf '#sst65x02-sample v1\n'
  printf '#source https://github.com/%s %s %s/v1\n' "$REPO" "$COMMIT" "$variant"
  printf '#rule first 32 objects of each file, upstream bytes verbatim, files in opcode order\n'
  for h in $HEXDIGITS; do
    for l in $HEXDIGITS; do
      op=$h$l
      file=$work/dl/$variant/v1/$op.json
      first=$(head -n 1 "$file")
      case $first in
        '[') lines=33 ;;
        '[{ "name"'*) lines=32 ;;
        *) fail "unknown layout in $variant/v1/$op.json" ;;
      esac
      printf '#file %s.json lines %s\n' "$op" "$lines"
      head -n "$lines" "$file"
    done
  done
}

# The failing opcodes of a results file (op, passed, failed), space separated.
failing_set() {
  awk -F'\t' '$3 > 0 { printf "%s%s", sep, $1; sep = " " } END { print "" }' "$1"
}

# The 32 bit-operation opcodes, $x7 and $xF, in ascending order.
bitop_set() {
  s=""
  for h in $HEXDIGITS; do
    for l in 7 f; do
      s="$s${s:+ }$h$l"
    done
  done
  printf '%s\n' "$s"
}

lane_status=0

# run_same NAME VARIANT --profile P: all vectors of the variant under the profile;
# the runner must exit 0 (zero failures).
run_same() {
  rs_name=$1
  rs_started=$(date +%s)
  rs_rc=0
  "$runner" complete "$3" "$4" --dir "$work/dl" --variant "$2" --results "$work/$rs_name.results" \
    >"$work/$rs_name.log" 2>&1 || rs_rc=$?
  cat "$work/$rs_name.log"
  if [ ! -f "$work/$rs_name.results" ]; then
    echo "vectors-full: run: $rs_name: FAILED, the runner wrote no results file (runner exit $rs_rc)" >&2
    lane_status=1
    return 0
  fi
  cp "$work/$rs_name.results" "$root/build/vectors-full/$rs_name.results"
  echo "vectors-full: run: $rs_name ($2 under $4): runner exit $rs_rc in $(($(date +%s) - rs_started)) s, results sha256 $(sha_of "$work/$rs_name.results")"
  if [ "$rs_rc" -ne 0 ]; then
    echo "vectors-full: run: $rs_name: FAILED, expected zero failing vectors" >&2
    lane_status=1
  fi
}

# run_cross NAME VARIANT --profile P: the other variant's data under the profile; the
# runner must exit 1 and the failing opcodes must be exactly the 32 bit-operation
# opcodes. The set is printed with what is extra or absent (the profile-diff spike).
run_cross() {
  rc_name=$1
  rc_started=$(date +%s)
  rc_rc=0
  "$runner" complete "$3" "$4" --dir "$work/dl" --variant "$2" --results "$work/$rc_name.results" \
    >"$work/$rc_name.log" 2>&1 || rc_rc=$?
  cat "$work/$rc_name.log"
  if [ ! -f "$work/$rc_name.results" ]; then
    echo "vectors-full: run: $rc_name: FAILED, the runner wrote no results file (runner exit $rc_rc)" >&2
    lane_status=1
    return 0
  fi
  cp "$work/$rc_name.results" "$root/build/vectors-full/$rc_name.results"
  rc_got=$(failing_set "$work/$rc_name.results")
  rc_want=$(bitop_set)
  rc_count=$(printf '%s\n' "$rc_got" | wc -w | tr -d ' ')
  rc_extra=""
  for rc_op in $rc_got; do
    case " $rc_want " in
      *" $rc_op "*) ;;
      *) rc_extra="$rc_extra $rc_op" ;;
    esac
  done
  rc_absent=""
  for rc_op in $rc_want; do
    case " $rc_got " in
      *" $rc_op "*) ;;
      *) rc_absent="$rc_absent $rc_op" ;;
    esac
  done
  echo "vectors-full: run: $rc_name ($2 under $4): runner exit $rc_rc in $(($(date +%s) - rc_started)) s, results sha256 $(sha_of "$work/$rc_name.results")"
  echo "vectors-full: run: $rc_name failing opcodes ($rc_count): $rc_got"
  echo "vectors-full: run: $rc_name beyond the 32 bit-operation opcodes:${rc_extra:- none}; bit-operation opcodes not failing:${rc_absent:- none}"
  if [ "$rc_rc" -ne 1 ] || [ "$rc_got" != "$rc_want" ]; then
    echo "vectors-full: run: $rc_name: FAILED, expected exit 1 and exactly the 32 bit-operation opcodes failing" >&2
    lane_status=1
  fi
}

# Always build (incremental, cheap): an older or locally edited binary must not
# carry the evidence of the lane. Sets runner.
build_runner() {
  runner=$root/build/check/tests/lynx-test-cpu-vectors
  (cd "$root" && cmake --preset check && cmake --build --preset check) || fail "the check preset did not build"
  if [ ! -x "$runner" ]; then
    fail "$runner is missing after the build"
  fi
}

for mode in "$@"; do
  case $mode in
    build)
      build_runner
      echo "vectors-full: build: $runner is built from the working tree"
      ;;
    pin)
      ensure_downloads
      lic=$work/LICENSE
      curl -fsSL --proto '=https' --proto-redir '=https' --max-filesize 100000 --retry 2 -o "$lic" -- "$BASE/LICENSE" || fail "the LICENSE download failed"
      got=$(sha_of "$lic")
      if [ "$got" != "$LICENSE_SHA256" ]; then
        fail "the upstream LICENSE changed: expected $LICENSE_SHA256, got $got"
      fi
      rows=$work/rows
      : >"$rows"
      while IFS= read -r rel; do
        sum=$(sha_of "$work/dl/$rel")
        printf '%s  %s\n' "$sum" "$rel" >>"$rows"
      done <"$list"
      new=$work/upstream.sha256
      {
        printf '# SingleStepTests/65x02 pin record: https://github.com/%s\n' "$REPO"
        printf '# commit %s\n' "$COMMIT"
        printf '# rule: sha256 of each whole upstream file, one row per file, sorted by path (LC_ALL=C)\n'
        sort -k 2,2 "$rows"
      } >"$new"
      mkdir -p "$out"
      cp "$new" "$record"
      cp "$lic" "$out/LICENSE"
      echo "vectors-full: pin: wrote $want_files rows and the LICENSE"
      ;;
    sample)
      check_record
      if [ ! -f "$out/LICENSE" ] || [ "$(sha_of "$out/LICENSE")" != "$LICENSE_SHA256" ]; then
        fail "fixtures/vectors/sst65x02/LICENSE is missing or is not the upstream text"
      fi
      ensure_downloads
      while IFS= read -r rel; do
        verify_against_record "$rel"
      done <"$list"
      echo "vectors-full: sample: all $want_files downloads match the pin record"
      for variant in $VARIANTS; do
        build_sample "$variant" >"$work/$variant.a"
        build_sample "$variant" >"$work/$variant.b"
        if ! cmp -s "$work/$variant.a" "$work/$variant.b"; then
          fail "the $variant container differs between two builds from the same files"
        fi
        mkdir -p "$out"
        cp "$work/$variant.a" "$out/$variant.sample"
        size=$(wc -c <"$out/$variant.sample")
        echo "vectors-full: sample: wrote $variant.sample ($((size + 0)) bytes, sha256 $(sha_of "$out/$variant.sample"))"
      done
      ;;
    run)
      check_record
      ensure_downloads
      while IFS= read -r rel; do
        verify_against_record "$rel"
      done <"$list"
      echo "vectors-full: run: all $want_files downloads match the pin record"
      for variant in $VARIANTS; do
        build_sample "$variant" >"$work/$variant.sample"
        if ! cmp -s "$work/$variant.sample" "$out/$variant.sample"; then
          fail "the committed $variant.sample is not what the verified files give"
        fi
      done
      echo "vectors-full: run: both samples equal the committed ones byte for byte"
      build_runner
      mkdir -p "$root/build/vectors-full"
      run_same none synertek65c02 --profile none
      run_same lynx1 synertek65c02 --profile lynx1
      run_same lynx2 rockwell65c02 --profile lynx2
      run_cross cross-lynx1-rockwell65c02 rockwell65c02 --profile lynx1
      run_cross cross-lynx2-synertek65c02 synertek65c02 --profile lynx2
      if [ "$lane_status" -ne 0 ]; then
        fail "run: at least one lane did not give its expected result (see the lines above)"
      fi
      echo "vectors-full: run: three same-profile runs clean, both cross pairings fail on exactly the 32 bit-operation opcodes"
      ;;
  esac
done
