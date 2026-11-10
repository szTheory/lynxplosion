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
HOME_PATH='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:[\\/]{1,2}Users[\\/]{1,2}[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'
HOME_OK='(/Users/|/home/)runner'
# Any address except GitHub noreply, the AI co-author noreply, documentation
# domains and SSH remotes.
EMAIL='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z][A-Za-z]+'
EMAIL_OK='[A-Za-z0-9._%+-]+@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|[A-Za-z0-9._%+-]+@example\.(com|org|net)|git@github\.com'
# Credential-shaped tokens: GitHub, AWS, PEM private keys, Slack, Google API,
# Anthropic and generic sk- keys, npm tokens and JSON web tokens.
CREDENTIAL='ghp_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{22,}|gh[ousr]_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}|sk-ant-[A-Za-z0-9_-]{20,}|(^|[^A-Za-z0-9])sk-[A-Za-z0-9_-]{32,}|npm_[A-Za-z0-9]{36}|eyJ[A-Za-z0-9_-]{10,}\.eyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}'

# stdin is text. True if some match of $1 is not, as a whole, an allowed form $2.
# Only the matches are tested (-o), and an allowed form must be the entire match,
# so an allowed token cannot hide a leak on the same line or inside a longer name.
leaks() { grep -E -o -e "$1" | grep -v -E -e "^($2)\$" | grep -q .; }
# stdin is a list of matches (git grep -o). True if one is not, as a whole, allowed.
unallowed() { grep -v -E -e "^($1)\$" | grep -q .; }
# stdin is text. True if some line matches $1.
has() { grep -q -E -e "$1"; }

# True when the file named by $1 is non-text: a control byte (NUL, a C0 control
# other than tab, LF, FF and CR, or DEL) or invalid UTF-8. A file holds a control
# byte exactly when the file with those bytes deleted differs from it (cmp).
# Status 2 means the test itself could not run; the caller fails closed.
is_nontext() {
  nt_rc=0
  nt_in=$1 # the same file twice: read by tr, compared by cmp (never written)
  nt_ref=$1
  tr -d '\000-\010\013\016-\037\177' <"$nt_in" 2>/dev/null | cmp -s - "$nt_ref" 2>/dev/null || nt_rc=$?
  case $nt_rc in
    0) ;;
    1) return 0 ;;
    *) return 2 ;;
  esac
  if iconv -f UTF-8 -t UTF-8 <"$1" >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

# A necessary condition for every match of the three patterns: a match contains
# one of these fixed strings (Users and /home/ for a home path, @ for an address,
# and the literal opening of each credential form). A file without any of them
# cannot hold a finding, so the regular-expression passes, which cost far more
# than a fixed-string search, are skipped for it; a file with one is scanned by
# the full rules exactly as before. This is what keeps a multi-megabyte data
# file cheap. The strings sit one per line in three files in the scratch
# directory (grep -F -f), so the tree scan and the history scan share one list.
# Keep them in step with the patterns above (tests/hygiene/pii_control.sh seeds
# every form).
printf '%s\n' 'Users' '/home/' >"$work/lits.home"
printf '%s\n' '@' >"$work/lits.email"
printf '%s\n' 'ghp_' 'gho_' 'ghu_' 'ghs_' 'ghr_' 'github_pat_' 'AKIA' '-----BEGIN ' \
  'xox' 'AIza' 'sk-' 'npm_' 'eyJ' >"$work/lits.cred"
cat "$work/lits.home" "$work/lits.email" "$work/lits.cred" >"$work/lits.all"
maybe_sensitive() { grep -q -F -f "$work/lits.all"; }
# File $1 holds a match of a rule, as leaks and has decide it: the fixed-string
# search for the rule's list ($2, a lits.* file) comes first, the expression
# only when a string is there. file_leaks is leaks and file_has is has, each over
# a file, with the rule's pattern $3 (and allowed form $4 for file_leaks).
file_leaks() { grep -q -F -f "$2" "$1" && leaks "$3" "$4" <"$1"; }
file_has() { grep -q -F -f "$2" "$1" && has "$3" <"$1"; }

# $1 is a path; $2 is a command that prints that path's content. A file whose
# content cannot be read is a finding, so the scan fails closed. The content is
# read once into a private copy and every rule reads that copy.
check_file() {
  cf_reader=$2
  # A symlink is scanned by its target text, which names a path and can hold a
  # home directory; the file it points at is its own entry when it is tracked.
  if [ -L "$1" ]; then
    cf_reader=link_show
  fi
  if ! $cf_reader "$1" >"$work/content" 2>/dev/null; then
    found 'cannot read file' "$1"
    return 0
  fi
  cf_nt=0
  is_nontext "$work/content" || cf_nt=$?
  case $cf_nt in
    0) return 0 ;;
    1) ;;
    *)
      found 'cannot read file' "$1"
      return 0
      ;;
  esac
  if ! maybe_sensitive <"$work/content"; then
    return 0
  fi
  if file_leaks "$work/content" "$work/lits.home" "$HOME_PATH" "$HOME_OK"; then found 'home path' "$1"; fi
  if file_leaks "$work/content" "$work/lits.email" "$EMAIL" "$EMAIL_OK"; then found 'email' "$1"; fi
  if file_has "$work/content" "$work/lits.cred" "$CREDENTIAL"; then found 'credential' "$1"; fi
}

# The reader for a symlink, called indirectly as check_file's reader.
# shellcheck disable=SC2317,SC2329
link_show() { readlink -- "$1"; }

# stdin is a list of file names; $1 says where they came from. A name can carry
# a home directory (an agent tool's dashed project folder) or a token, so names
# are scanned with the same rules as content, and a finding never prints the
# name itself.
check_names() {
  cn_names=$(cat)
  printf '%s\n' "$cn_names" >"$work/cn"
  if file_leaks "$work/cn" "$work/lits.home" "$HOME_PATH" "$HOME_OK"; then found 'home path' "a file name in $1"; fi
  if file_leaks "$work/cn" "$work/lits.email" "$EMAIL" "$EMAIL_OK"; then found 'email' "a file name in $1"; fi
  if file_has "$work/cn" "$work/lits.cred" "$CREDENTIAL"; then found 'credential' "a file name in $1"; fi
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
tree_show() { if [ -f "$1" ]; then cat -- "$1"; else git show ":$1"; fi; }

# $1 and $2 as in leaks (an empty $2 allows nothing); $3 the finding name; $4 a
# commit; the remaining arguments are pathspecs. A git grep failure (status
# above 1) is itself a finding: git's status is never discarded behind a pipe.
scan_rule() {
  sr_re=$1
  sr_allow=$2
  sr_name=$3
  sr_c=$4
  shift 4
  sr_rc=0
  git grep -I -h -o -E -e "$sr_re" "$sr_c" -- "$@" >"$work/matches" 2>/dev/null || sr_rc=$?
  if [ "$sr_rc" -gt 1 ]; then
    found 'git grep failed' "commit $sr_c"
    return 0
  fi
  if [ "$sr_rc" -eq 1 ]; then
    return 0
  fi
  if [ -z "$sr_allow" ] || unallowed "$sr_allow" <"$work/matches"; then
    found "$sr_name" "commit $sr_c"
  fi
}

# $1 is a commit: its tree holds no home path, address or credential. git grep
# first lists the files of the commit that hold one of the fixed strings of
# lits.all, which is cheap even over a large data file; only those files go
# through the three regular expressions, since no other file can hold a match.
# A list that cannot be turned into exact pathspecs (a quoted name, or more than
# 200 files) falls back to the whole tree, which is the plain scan. A failing
# listing is the same finding the plain scan reports, once per rule.
scan_commit() {
  sc_c=$1
  sc_rc=0
  git grep -I -l -F -f "$work/lits.all" "$sc_c" -- . >"$work/cands" 2>/dev/null || sc_rc=$?
  if [ "$sc_rc" -gt 1 ]; then
    found 'git grep failed' "commit $sc_c"
    found 'git grep failed' "commit $sc_c"
    found 'git grep failed' "commit $sc_c"
    return 0
  fi
  if [ "$sc_rc" -eq 1 ]; then
    return 0
  fi
  set --
  sc_n=0
  sc_exact=1
  while IFS= read -r sc_line; do
    case $sc_line in
      "$sc_c":\"*) sc_exact=0 ;;
      "$sc_c":*) ;;
      *) sc_exact=0 ;;
    esac
    sc_n=$((sc_n + 1))
    if [ "$sc_exact" -eq 0 ] || [ "$sc_n" -gt 200 ]; then
      sc_exact=0
      break
    fi
    set -- "$@" ":(literal)${sc_line#"$sc_c":}"
  done <"$work/cands"
  if [ "$sc_exact" -eq 0 ]; then
    set -- .
  fi
  scan_rule "$HOME_PATH" "$HOME_OK" 'home path' "$sc_c" "$@"
  scan_rule "$EMAIL" "$EMAIL_OK" 'email' "$sc_c" "$@"
  scan_rule "$CREDENTIAL" '' 'credential' "$sc_c" "$@"
}

# $1 is a commit. Scans the names of the files it changed and the target text of
# the symlinks it adds or changes (git grep does not read a symlink).
scan_changes() {
  if ! git diff-tree -r --root --no-commit-id "$1" >"$work/raw" 2>/dev/null; then
    found 'cannot read file names' "commit $1"
    return 0
  fi
  cut -f2- "$work/raw" >"$work/names"
  check_names "commit $1" <"$work/names"
  : >"$work/links"
  awk '$2 == "120000" { print $4 }' "$work/raw" >"$work/blobs"
  while IFS= read -r sc_blob; do
    if git cat-file blob "$sc_blob" >>"$work/links" 2>/dev/null; then
      printf '\n' >>"$work/links"
    else
      found 'cannot read symlink' "commit $1"
    fi
  done <"$work/blobs"
  if file_leaks "$work/links" "$work/lits.home" "$HOME_PATH" "$HOME_OK"; then found 'home path' "commit $1"; fi
  if file_leaks "$work/links" "$work/lits.email" "$EMAIL" "$EMAIL_OK"; then found 'email' "commit $1"; fi
  if file_has "$work/links" "$work/lits.cred" "$CREDENTIAL"; then found 'credential' "commit $1"; fi
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
    printf '%s\n' "$tree_list" >"$work/names"
    check_names 'the tree' <"$work/names"
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
      scan_commit "$c"
      scan_changes "$c"
      # A squash merge copies the pull request's title and body into the
      # message, so messages are scanned as well.
      if git log -1 --format=%B "$c" >"$work/msg" 2>/dev/null; then
        if file_leaks "$work/msg" "$work/lits.home" "$HOME_PATH" "$HOME_OK"; then found 'home path' "message of $c"; fi
        if file_leaks "$work/msg" "$work/lits.email" "$EMAIL" "$EMAIL_OK"; then found 'email' "message of $c"; fi
        if file_has "$work/msg" "$work/lits.cred" "$CREDENTIAL"; then found 'credential' "message of $c"; fi
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
