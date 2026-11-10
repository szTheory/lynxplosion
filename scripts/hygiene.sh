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

# A private scratch directory with an unpredictable name (never a fixed name a
# planted symlink could redirect), removed on every exit and on a signal.
work=$(mktemp -d "${TMPDIR:-/tmp}/hygiene.XXXXXX") || {
  printf 'hygiene: cannot create a scratch directory\n' >&2
  exit 1
}
trap 'rm -rf "$work"' EXIT
trap 'exit 1' INT TERM

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

# $1 and $2 as in leaks (an empty $2 allows nothing); $3 is a commit and $4 the
# finding name. A git grep failure (status above 1) is itself a finding: git's
# status is never discarded behind a pipe.
scan_commit() {
  sc_rc=0
  git grep -I -h -o -E -e "$1" "$3" -- . ":!$SELF" >"$work/matches" 2>/dev/null || sc_rc=$?
  if [ "$sc_rc" -gt 1 ]; then
    found 'git grep failed' "commit $3"
    return 0
  fi
  if [ "$sc_rc" -eq 1 ]; then
    return 0
  fi
  if [ -z "$2" ] || unallowed "$2" <"$work/matches"; then
    found "$4" "commit $3"
  fi
}

mode=${1:---tree}
[ $# -gt 0 ] && shift
case $mode in
  --tree)
    if ! git ls-files -z --cached --others --exclude-standard >"$work/list"; then
      printf 'hygiene: cannot list files: git ls-files failed\n' >&2
      exit 1
    fi
    tree_list=$(tr '\0' '\n' <"$work/list")
    check_files "$tree_list" tree_show
    ;;
  --history)
    [ $# -gt 0 ] || set -- --all
    # A failing rev-list is a failure of the scan, never an empty history.
    if ! git rev-list "$@" >"$work/revs" 2>/dev/null; then
      printf 'hygiene: cannot list commits: git rev-list failed\n' >&2
      exit 1
    fi
    if [ ! -s "$work/revs" ]; then
      printf 'hygiene: history: no commits selected, nothing scanned (visible)\n' >&2
    fi
    while IFS= read -r c; do
      scan_commit "$HOME_PATH" "$HOME_OK" "$c" 'home path'
      scan_commit "$EMAIL" "$EMAIL_OK" "$c" 'email'
      scan_commit "$CREDENTIAL" '' "$c" 'credential'
      # A squash merge copies the pull request's title and body into the
      # message, so messages are scanned as well.
      if git log -1 --format=%B "$c" >"$work/msg" 2>/dev/null; then
        if leaks "$HOME_PATH" "$HOME_OK" <"$work/msg"; then found 'home path' "message of $c"; fi
        if leaks "$EMAIL" "$EMAIL_OK" <"$work/msg"; then found 'email' "message of $c"; fi
        if has "$CREDENTIAL" <"$work/msg"; then found 'credential' "message of $c"; fi
      else
        found 'cannot read message' "commit $c"
      fi
      if author=$(git log -1 --format=%ae "$c" 2>/dev/null); then
        check_identity "$author" "$c"
      else
        found 'cannot read identity' "commit $c"
      fi
      if committer=$(git log -1 --format=%ce "$c" 2>/dev/null); then
        check_identity "$committer" "$c"
      else
        found 'cannot read identity' "commit $c"
      fi
    done <"$work/revs"
    ;;
  *)
    printf 'usage: %s --tree | --history [rev-list args]\n' "$0" >&2
    exit 2
    ;;
esac

exit "$fail"
