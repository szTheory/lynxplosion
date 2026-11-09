#!/bin/sh
# Offline negative control for scripts/fetch-pin.sh, run as
#   tests/gates/fetch_pin_control.sh REPO_ROOT
#
# Fault prevented: a fetch that unpacks before it verifies, a cache that is
# trusted without its per-file list, or a list that omits files. Every payload,
# pins file and destination is built under a fresh temporary directory outside
# the repository and the build tree, and every URL is file://, so nothing
# touches the network and nothing is left behind.
set -eu

if [ $# -ne 1 ]; then
  echo "usage: fetch_pin_control.sh REPO_ROOT" >&2
  exit 2
fi
root=$(cd "$1" && pwd -P)
fetch=$root/scripts/fetch-pin.sh

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-fetchpin.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
work=$(cd "$work" && pwd -P)
case $work/ in
  "$root"/*)
    echo "fetch_pin_control: the work directory $work is inside the repository $root" >&2
    exit 1
    ;;
esac

fail() {
  echo "fetch_pin_control: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "fetch_pin_control: $1: ok"
}

# A payload: one top directory, a text file, a file with a space in its name
# and a small non-text file written with octal escapes.
mkdir -p "$work/src/top/sub"
printf 'hello pinned world\n' >"$work/src/top/readme.txt"
printf 'spaced\n' >"$work/src/top/sub/a b.txt"
printf '\000\001\377\200binary\000\n' >"$work/src/top/sub/blob.bin"
tar -czf "$work/payload.tar.gz" -C "$work/src" top
if command -v sha256sum >/dev/null 2>&1; then
  true_digest=$(sha256sum "$work/payload.tar.gz")
else
  true_digest=$(shasum -a 256 "$work/payload.tar.gz")
fi
true_digest=${true_digest%% *}
zeros=0000000000000000000000000000000000000000000000000000000000000000

pins_bad=$work/pins-bad.txt
pins_ok=$work/pins-ok.txt
printf 'payload 1 file://%s/payload.tar.gz %s\n' "$work" "$zeros" >"$pins_bad"
printf 'payload 1 file://%s/payload.tar.gz %s\n' "$work" "$true_digest" >"$pins_ok"
# No row may point anywhere but a local file.
if grep -v ' file://' "$pins_bad" "$pins_ok" >/dev/null; then
  fail pins_are_local "a control pins row does not use a file:// URL"
fi

dest=$work/dest

# 1. A wrong digest stops before DEST exists.
rc=0
PINS=$pins_bad sh "$fetch" payload "$dest" >"$work/out1" 2>&1 || rc=$?
[ "$rc" -eq 1 ] || fail mismatch "expected exit 1, got $rc"
grep -q 'payload: sha256 mismatch' "$work/out1" || fail mismatch "no 'payload: sha256 mismatch' in: $(cat "$work/out1")"
[ ! -e "$dest" ] || fail mismatch_dest_untouched "DEST exists after a mismatch"
ok mismatch
ok mismatch_dest_untouched

# 2. The true digest unpacks, lists every file and stamps last.
rc=0
PINS=$pins_ok sh "$fetch" payload "$dest" >"$work/out2" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || fail verified "expected exit 0, got $rc: $(cat "$work/out2")"
[ -f "$dest/readme.txt" ] || fail verified "readme.txt was not extracted with the top directory stripped"
[ -f "$dest/sub/a b.txt" ] || fail verified "the file with a space was not extracted"
[ -f "$dest/sub/blob.bin" ] || fail verified "the non-text file was not extracted"
[ "$(cat "$dest/.lynx-pin")" = "$true_digest" ] || fail stamp "the stamp does not hold the archive digest"
[ "$(wc -c <"$dest/.lynx-pin" | tr -d ' ')" -eq 65 ] || fail stamp "the stamp is not 64 hex digits and a newline"
ok verified
ok stamp

list=$dest/.lynx-pin-files
[ -s "$list" ] || fail list "DEST/.lynx-pin-files is missing or empty"
grep -q "^# sha256 of the verified archive: $true_digest\$" "$list" || fail list "the header does not name the archive digest"
rows=$(grep -vc '^#' "$list")
[ "$rows" -eq 3 ] || fail list "expected 3 file rows, got $rows"
for rel in readme.txt "sub/a b.txt" sub/blob.bin; do
  if command -v sha256sum >/dev/null 2>&1; then
    h=$(sha256sum "$dest/$rel")
  else
    h=$(shasum -a 256 "$dest/$rel")
  fi
  h=${h%% *}
  grep -qxF "$h $rel" "$list" || fail list "no row '$h $rel'"
done
grep -v '^#' "$list" | cut -d ' ' -f 2- >"$work/paths"
LC_ALL=C sort -c "$work/paths" || fail list "the rows are not sorted by path"
ok list

# 3. A second run reuses the verified extraction.
rc=0
PINS=$pins_ok sh "$fetch" payload "$dest" >"$work/out3" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || fail cached "expected exit 0, got $rc"
grep -q 'fetch-pin: payload: cached (verified at fetch)' "$work/out3" || fail cached "no cached line in: $(cat "$work/out3")"
ok cached

# 4. Without its list the cache is not trusted: the run re-fetches.
rm -f "$list"
rc=0
PINS=$pins_ok sh "$fetch" payload "$dest" >"$work/out4" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || fail refetch_without_list "expected exit 0, got $rc"
if grep -q 'cached' "$work/out4"; then
  fail refetch_without_list "the cache was reused without its list"
fi
[ -s "$list" ] || fail refetch_without_list "the list was not written again"
ok refetch_without_list

# 5. A wrong stamp is not trusted either.
printf '%s\n' "$zeros" >"$dest/.lynx-pin"
rc=0
PINS=$pins_ok sh "$fetch" payload "$dest" >"$work/out5" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || fail refetch_wrong_stamp "expected exit 0, got $rc"
if grep -q 'cached' "$work/out5"; then
  fail refetch_wrong_stamp "the cache was reused with a wrong stamp"
fi
[ "$(cat "$dest/.lynx-pin")" = "$true_digest" ] || fail refetch_wrong_stamp "the stamp was not restored"
ok refetch_wrong_stamp

# 6. An unknown name is refused.
rc=0
PINS=$pins_ok sh "$fetch" nosuch "$work/other" >"$work/out6" 2>&1 || rc=$?
[ "$rc" -eq 1 ] || fail unknown_name "expected exit 1, got $rc"
grep -q 'nosuch: not in pins.txt' "$work/out6" || fail unknown_name "no 'not in pins.txt' in: $(cat "$work/out6")"
[ ! -e "$work/other" ] || fail unknown_name "DEST exists after an unknown name"
ok unknown_name

# 7. A path with a newline cannot be listed one per line: fail closed, DEST untouched.
mkdir -p "$work/src2/top"
nlname=$(printf 'two\nlines')
printf 'x\n' >"$work/src2/top/$nlname"
tar -czf "$work/odd.tar.gz" -C "$work/src2" top
if command -v sha256sum >/dev/null 2>&1; then
  odd_digest=$(sha256sum "$work/odd.tar.gz")
else
  odd_digest=$(shasum -a 256 "$work/odd.tar.gz")
fi
odd_digest=${odd_digest%% *}
printf 'odd 1 file://%s/odd.tar.gz %s\n' "$work" "$odd_digest" >"$work/pins-odd.txt"
rc=0
PINS=$work/pins-odd.txt sh "$fetch" odd "$work/odd-dest" >"$work/out7" 2>&1 || rc=$?
[ "$rc" -eq 1 ] || fail newline_path "expected exit 1, got $rc"
grep -q 'odd: the archive holds a path with a newline' "$work/out7" || fail newline_path "no newline finding in: $(cat "$work/out7")"
[ ! -e "$work/odd-dest" ] || fail newline_path "DEST exists after a refused listing"
ok newline_path
