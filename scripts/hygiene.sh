#!/bin/sh
# Privacy hygiene for a public repository: no personal data and no credentials
# in any file git would track, and no non-noreply commit identity.
#
#   scripts/hygiene.sh --tree               every file git would track  (CTest)
#   scripts/hygiene.sh --history [revs...]  commits; default --all      (check.sh)
#
# Binary and firmware content is owned by scripts/scan-binaries.sh; this scan
# reads text files only (the same control-byte and UTF-8 test), so a binary is
# never half-read here. Findings name the rule and the file or commit, never
# the matched text, so the output is safe to paste anywhere. Exit status is 1
# if anything was found, 2 on a usage error.
# Adapted from the owner's muhtari project (MIT), ~/projects/muhtari/scripts/hygiene.sh.
set -eu

# The C locale for every command, so a byte above 0x7F in a name or a file never
# makes grep skip it.
LC_ALL=C
export LC_ALL

# Path quoting off, so a name with bytes above 0x7F is scanned, not skipped.
git() { command git -c core.quotePath=false "$@"; }

cd "$(git rev-parse --show-toplevel)"

SELF=scripts/hygiene.sh
fail=0
found() {
  printf 'hygiene: %s: %s\n' "$1" "$2" >&2
  fail=1
}

# A home directory names a person (also in the dashed form agent tools use for
# project folders). Hosted CI runner homes are the exception.
HOME_PATH='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:\\Users\\[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'
HOME_OK='(/Users/|/home/)runner'
# Any address except GitHub noreply, the AI co-author noreply, documentation
# domains and SSH remotes.
EMAIL='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z][A-Za-z]+'
EMAIL_OK='[A-Za-z0-9._%+-]+@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|[A-Za-z0-9._%+-]+@example\.(com|org|net)|git@github\.com'
# Credential-shaped tokens: GitHub, AWS, PEM private keys, Slack, Google API.
CREDENTIAL='ghp_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{22,}|gh[ousr]_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}'

# stdin is text. True if some match of $1 is not, as a whole, an allowed form $2.
# Only the matches are tested (-o), and an allowed form must be the entire match,
# so an allowed token cannot hide a leak on the same line or inside a longer name.
leaks() { grep -E -o -e "$1" | grep -v -E -e "^($2)\$" | grep -q .; }
# stdin is a list of matches (git grep -o). True if one is not, as a whole, allowed.
unallowed() { grep -v -E -e "^($1)\$" | grep -q .; }
# stdin is text. True if some line matches $1.
has() { grep -q -E -e "$1"; }

# True when the file named by $1 is non-text: a control byte (NUL, a C0 control
# other than tab, LF, FF and CR, or DEL) or invalid UTF-8.
is_nontext() {
  nt_size=$($2 "$1" 2>/dev/null | wc -c | tr -d ' ')
  nt_text=$($2 "$1" 2>/dev/null | tr -d '\000-\010\013\016-\037\177' | wc -c | tr -d ' ')
  if [ "$nt_size" != "$nt_text" ]; then
    return 0
  fi
  if $2 "$1" 2>/dev/null | iconv -f UTF-8 -t UTF-8 >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

# $1 is a path; $2 is a command that prints that path's content. A file whose
# content cannot be read is a finding, so the scan fails closed.
check_file() {
  if [ "$1" = "$SELF" ]; then
    return 0
  fi
  if [ -L "$1" ]; then
    return 0
  fi
  if ! $2 "$1" >/dev/null 2>&1; then
    found 'cannot read file' "$1"
    return 0
  fi
  if is_nontext "$1" "$2"; then
    return 0
  fi
  if $2 "$1" 2>/dev/null | leaks "$HOME_PATH" "$HOME_OK"; then found 'home path' "$1"; fi
  if $2 "$1" 2>/dev/null | leaks "$EMAIL" "$EMAIL_OK"; then found 'email' "$1"; fi
  if $2 "$1" 2>/dev/null | has "$CREDENTIAL"; then found 'credential' "$1"; fi
}

# $1 is a newline-separated list; $2 as in check_file. A name holding a newline
# splits into parts that name no file, and each part fails closed.
check_files() {
  old_ifs=$IFS
  IFS='
'
  set -f
  for f in $1; do check_file "$f" "$2"; done
  set +f
  IFS=$old_ifs
}

check_identity() { # $1 is an email; $2 says where it came from
  case $1 in
    *@users.noreply.github.com | noreply@github.com) ;;
    *) found 'identity' "$2" ;;
  esac
}

# The reader is called indirectly, as check_file's $2. A tracked file deleted
# from the working tree is still read from the index.
# shellcheck disable=SC2317,SC2329
tree_show() { if [ -f "$1" ]; then cat "$1"; else git show ":$1"; fi; }

mode=${1:---tree}
[ $# -gt 0 ] && shift
case $mode in
  --tree)
    if ! git ls-files -z --cached --others --exclude-standard >"${TMPDIR:-/tmp}/hygiene-list.$$"; then
      rm -f "${TMPDIR:-/tmp}/hygiene-list.$$"
      printf 'hygiene: cannot list files: git ls-files failed\n' >&2
      exit 1
    fi
    tree_list=$(tr '\0' '\n' <"${TMPDIR:-/tmp}/hygiene-list.$$")
    rm -f "${TMPDIR:-/tmp}/hygiene-list.$$"
    check_files "$tree_list" tree_show
    ;;
  --history)
    [ $# -gt 0 ] || set -- --all
    for c in $(git rev-list "$@"); do
      if git grep -I -h -o -E -e "$HOME_PATH" "$c" -- . ":!$SELF" 2>/dev/null | unallowed "$HOME_OK"; then
        found 'home path' "commit $c"
      fi
      if git grep -I -h -o -E -e "$EMAIL" "$c" -- . ":!$SELF" 2>/dev/null | unallowed "$EMAIL_OK"; then
        found 'email' "commit $c"
      fi
      if git grep -I -h -E -e "$CREDENTIAL" "$c" -- . ":!$SELF" 2>/dev/null | grep -q .; then
        found 'credential' "commit $c"
      fi
      # A squash merge copies the pull request's title and body into the
      # message, so messages are scanned as well.
      if git log -1 --format=%B "$c" | leaks "$HOME_PATH" "$HOME_OK"; then
        found 'home path' "message of $c"
      fi
      if git log -1 --format=%B "$c" | leaks "$EMAIL" "$EMAIL_OK"; then
        found 'email' "message of $c"
      fi
      if git log -1 --format=%B "$c" | has "$CREDENTIAL"; then
        found 'credential' "message of $c"
      fi
      check_identity "$(git log -1 --format=%ae "$c")" "$c"
      check_identity "$(git log -1 --format=%ce "$c")" "$c"
    done
    ;;
  *)
    printf 'usage: %s --tree | --history [rev-list args]\n' "$0" >&2
    exit 2
    ;;
esac

exit "$fail"
