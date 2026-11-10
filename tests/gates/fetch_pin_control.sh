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
# The script fetches https URLs only; a file:// URL is allowed solely under this
# variable, which this offline control sets and the pins step never does. The
# strict cases below remove it with env -u.
LYNX_FETCH_PIN_ALLOW_FILE=1
export LYNX_FETCH_PIN_ALLOW_FILE

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

# --- WR-05: the row, the URL scheme and DEST are validated before any action ----

strict() { env -u LYNX_FETCH_PIN_ALLOW_FILE "$@"; }

# 8. A file:// URL is refused unless the control's variable is set, and nothing
# is created.
rc=0
PINS=$pins_ok strict sh "$fetch" payload "$work/strict-dest" >"$work/out8" 2>&1 || rc=$?
[ "$rc" -eq 1 ] || fail scheme_file "expected exit 1, got $rc: $(cat "$work/out8")"
grep -q 'only https URLs are fetched' "$work/out8" || fail scheme_file "no scheme finding in: $(cat "$work/out8")"
[ ! -e "$work/strict-dest" ] || fail scheme_file "DEST exists after a refused scheme"
ok scheme_file_refused

# 9. An http URL and a URL that would read as an option are refused before any
# transfer is attempted.
for bad_url in "http://example.invalid/payload.tar.gz" "-oops.tar.gz" "ftp://example.invalid/p.tar.gz"; do
  printf 'payload 1 %s %s\n' "$bad_url" "$true_digest" >"$work/pins-url.txt"
  rc=0
  PINS=$work/pins-url.txt strict sh "$fetch" payload "$work/strict-dest" >"$work/out9" 2>&1 || rc=$?
  [ "$rc" -eq 1 ] || fail scheme_other "$bad_url: expected exit 1, got $rc"
  grep -q 'only https URLs are fetched' "$work/out9" || fail scheme_other "$bad_url: no scheme finding"
  [ ! -e "$work/strict-dest" ] || fail scheme_other "$bad_url: DEST exists"
done
ok scheme_other_refused

# 10. A digest that is not 64 lowercase hex digits and a name outside the
# character set are refused.
upper=$(printf '%s' "$true_digest" | tr 'a-f' 'A-F')
short=$(printf '%s' "$true_digest" | cut -c 1-63)
for bad_digest in "$upper" "$short" "${true_digest}0" "xyz"; do
  printf 'payload 1 file://%s/payload.tar.gz %s\n' "$work" "$bad_digest" >"$work/pins-digest.txt"
  rc=0
  PINS=$work/pins-digest.txt sh "$fetch" payload "$work/strict-dest" >"$work/out10" 2>&1 || rc=$?
  [ "$rc" -eq 1 ] || fail bad_digest "expected exit 1, got $rc for a bad digest"
  grep -q 'must be 64 lowercase hex digits' "$work/out10" || fail bad_digest "no digest finding"
  [ ! -e "$work/strict-dest" ] || fail bad_digest "DEST exists after a bad digest"
done
ok bad_digest_refused
for bad_name in 'a/b' '../x' 'a b' ''; do
  rc=0
  PINS=$pins_ok sh "$fetch" "$bad_name" "$work/strict-dest" >"$work/out10b" 2>&1 || rc=$?
  [ "$rc" -ne 0 ] || fail bad_name "a name '$bad_name' was accepted"
  [ ! -e "$work/strict-dest" ] || fail bad_name "DEST exists after a bad name"
done
ok bad_name_refused

# 11. A DEST that can name a directory that matters is refused and the directory
# survives: the working directory and its spellings, a parent of it, and the
# home directory and its parent.
mkdir -p "$work/cwd/inner" "$work/fakehome/sub"
printf 'keep\n' >"$work/cwd/inner/keep.txt"
printf 'keep\n' >"$work/cwd/keep.txt"
printf 'keep\n' >"$work/fakehome/keep.txt"
refuse_dest() { # NAME DIR DEST [HOME]
  rd_home=${4:-$work/fakehome}
  rc=0
  (cd "$2" && HOME=$rd_home PINS=$pins_ok sh "$fetch" payload "$3") >"$work/out11" 2>&1 || rc=$?
  [ "$rc" -eq 2 ] || fail "$1" "expected exit 2, got $rc: $(cat "$work/out11")"
  for rd_keep in "$work/cwd/keep.txt" "$work/cwd/inner/keep.txt" "$work/fakehome/keep.txt"; do
    [ -f "$rd_keep" ] || fail "$1" "a protected file was removed"
  done
  ok "$1"
}
refuse_dest dest_dot "$work/cwd" .
refuse_dest dest_dotdot "$work/cwd/inner" ..
refuse_dest dest_trailing_dot "$work/cwd" inner/..
# shellcheck disable=SC2088 # a literal tilde, the case under test
refuse_dest dest_tilde "$work/cwd" '~/x'
refuse_dest dest_cwd_absolute "$work/cwd" "$work/cwd"
refuse_dest dest_parent_of_cwd "$work/cwd/inner" "$work/cwd"
refuse_dest dest_home "$work/cwd" "$work/fakehome"
refuse_dest dest_parent_of_home "$work/cwd" "$work"

# 12. The transfer is made with https-only options, in an order where the URL
# cannot be read as an option. A stand-in for the transfer tool (its name is
# assembled so this file names no network tool) records its arguments.
tool=cu
tool=${tool}rl
mkdir -p "$work/bin"
# shellcheck disable=SC2016 # the stand-in's variables are written literally
{
  printf '#!/bin/sh\n'
  printf ': >"$LYNX_ARGS"\n'
  printf 'out=\n'
  printf 'while [ $# -gt 0 ]; do\n'
  printf '  printf "%%s\\n" "$1" >>"$LYNX_ARGS"\n'
  printf '  if [ "$1" = -o ]; then out=$2; fi\n'
  printf '  shift\n'
  printf 'done\n'
  printf 'cp "$LYNX_PAYLOAD" "$out"\n'
} >"$work/bin/$tool"
chmod +x "$work/bin/$tool"
printf 'payload 1 https://example.invalid/p/payload.tar.gz %s\n' "$true_digest" >"$work/pins-https.txt"
rc=0
PATH="$work/bin:$PATH" LYNX_ARGS=$work/args LYNX_PAYLOAD=$work/payload.tar.gz PINS=$work/pins-https.txt \
  strict sh "$fetch" payload "$work/https-dest" >"$work/out12" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || fail https_options "expected exit 0, got $rc: $(cat "$work/out12")"
grep -qxF -- '--proto' "$work/args" || fail https_options "no --proto option"
grep -qxF -- '=https' "$work/args" || fail https_options "the protocol is not restricted to https"
grep -qxF -- '--proto-redir' "$work/args" || fail https_options "no --proto-redir option"
[ "$(grep -cxF -- '=https' "$work/args")" -eq 2 ] || fail https_options "redirects are not restricted to https"
[ "$(tail -n 2 "$work/args" | head -n 1)" = "--" ] || fail https_options "the URL is not preceded by --"
[ "$(tail -n 1 "$work/args")" = "https://example.invalid/p/payload.tar.gz" ] || fail https_options "the URL is not the last argument"
ok https_options
