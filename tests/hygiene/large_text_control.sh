#!/bin/sh
# Seeded control for the large text fixtures, run as
#   tests/hygiene/large_text_control.sh REPO_ROOT
#
# Fault prevented: a privacy or rights scan that stops reading a large text
# file (a size cut-off, a prefilter that skips it, a buffer limit) and so passes
# a leak that sits deep inside the pinned vector samples under fixtures/vectors/.
# The other self-tests use small synthetic repositories on purpose (a scan of a
# few bytes is fast); this is the one control that carries the real multi-megabyte
# files. It copies them into a temporary repository outside the checkout, asserts
# that scripts/hygiene.sh and scripts/scan-binaries.sh accept them as they are,
# appends seeded faults to the END of each copy (the last bytes of a file of
# several megabytes) and asserts the rejection names that file. The real
# repository is never modified. Every fake value is assembled from fragments at
# run time and is never written to this file.
set -eu

if [ $# -ne 1 ]; then
  echo "usage: large_text_control.sh REPO_ROOT" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)
vec=fixtures/vectors/sst65x02

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-large.XXXXXX")
work=$(cd "$work" && pwd -P)
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$repo"/*)
    echo "large_text_control: the work directory $work is inside the repository" >&2
    exit 1
    ;;
esac

fail() {
  echo "large_text_control: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "large_text_control: $1: ok"
}

# The real large text fixtures: every *.sample under the vector directory, at
# least two, each at least a megabyte (otherwise this control proves nothing).
n=0
for s in "$repo/$vec"/*.sample; do
  if [ ! -f "$s" ]; then
    fail real_fixtures_present "no $vec/*.sample in the repository"
  fi
  size=$(wc -c <"$s" | tr -d ' ')
  if [ "$size" -lt 1000000 ]; then
    fail real_fixtures_large "$s is only $size bytes"
  fi
  n=$((n + 1))
done
if [ "$n" -lt 2 ]; then
  fail real_fixtures_present "expected at least two $vec/*.sample files, found $n"
fi
ok real_fixtures_large

iso=$work/repo
mkdir -p "$iso/scripts" "$iso/$vec"
cp "$repo/scripts/hygiene.sh" "$iso/scripts/hygiene.sh"
cp "$repo/scripts/scan-binaries.sh" "$iso/scripts/scan-binaries.sh"
for s in "$repo/$vec"/*.sample; do
  cp "$s" "$iso/$vec/"
done
(
  cd "$iso"
  git init -q .
  git config user.email control@users.noreply.github.com
  git config user.name control
  git config commit.gpgsign false
  printf 'plain text\n' >readme.txt
  git add readme.txt scripts
  git commit -q -m initial
)
# The samples stay untracked: both scans read every file git would track, which
# includes untracked files that are not ignored.
set -- "$iso/$vec"/*.sample
first=$1
second=$2

hyg() { (cd "$iso" && sh scripts/hygiene.sh --tree); }
scan() { sh "$iso/scripts/scan-binaries.sh" --source --root "$iso"; }

accept() {
  if ! "$2" >"$work/out" 2>"$work/err"; then
    cat "$work/err" >&2
    fail "$1" "expected a pass, got exit 1"
  fi
}

# reject NAME FUNCTION TEXT...: exit 1 and every TEXT in the findings, from one run.
reject() {
  rj_name=$1
  rj_fn=$2
  shift 2
  rc=0
  "$rj_fn" >"$work/out" 2>"$work/err" || rc=$?
  if [ "$rc" -ne 1 ]; then
    cat "$work/err" >&2
    fail "$rj_name" "expected exit 1, got $rc"
  fi
  for rj_text in "$@"; do
    if ! grep -F -q "$rj_text" "$work/err"; then
      cat "$work/err" >&2
      fail "$rj_name" "expected finding text: $rj_text"
    fi
  done
}

# --- fake values, assembled from fragments --------------------------------------
rep() { head -c "$2" /dev/zero | tr '\000' "$1"; }
fake_home=/Us$(printf 'ers')/someone/project
at=@
fake_email=jane.doe${at}company.io
fake_ghp=gh$(printf 'p')_$(rep a 36)

accept hygiene_real_samples hyg
ok hygiene_accepts_real_samples
accept scan_real_samples scan
ok scan_accepts_real_samples

# Every fault goes at the very end of a multi-megabyte copy, so a scan that stops
# early, or reads only part of a large file, misses it. A third file is the start
# of a real sample with a control byte at its end: non-text, so the privacy scan
# leaves it to the binary scan.
third=$iso/$vec/zz-binary.sample
head -c 1000000 "$first" >"$third"
printf '\001' >>"$third"
printf '%s\n' "see $fake_home for details" "contact $fake_email today" >>"$first"
printf '%s\n' "token $fake_ghp" "$(rep Q 300)" >>"$second"

reject hygiene_deep_leaks hyg \
  "hygiene: home path: $vec/${first##*/}" \
  "hygiene: email: $vec/${first##*/}" \
  "hygiene: credential: $vec/${second##*/}"
if grep -F -q "zz-binary" "$work/err"; then
  fail hygiene_binary_not_read "the privacy scan reported a binary file it must not read"
fi
ok hygiene_finds_leaks_at_end_of_large_files

reject scan_deep_findings scan \
  "scan-binaries: long base64 or hex run in a text file: $iso/$vec/${second##*/}" \
  "scan-binaries: unmanifested binary: $third"
if grep -F -q "${first##*/}" "$work/err"; then
  fail scan_text_not_binary "the rights scan reported a text sample"
fi
ok scan_finds_base64_run_and_binary_in_large_files
