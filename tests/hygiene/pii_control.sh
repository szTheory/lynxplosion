#!/bin/sh
# Seeded controls for scripts/hygiene.sh, run as
#   tests/hygiene/pii_control.sh REPO_ROOT
#
# Fault prevented: a privacy scan that passes a home path, a non-noreply
# address, a credential-shaped token or a non-noreply commit identity, or that
# rejects the allowed noreply forms. Every fake value is assembled from
# fragments at run time inside a temporary repository outside the checkout and
# is never written to this file, so the repository stays clean for its own
# scan and for push protection (no complete token is ever committed).
set -eu

if [ $# -ne 1 ]; then
  echo "usage: pii_control.sh REPO_ROOT" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-pii.XXXXXX")
work=$(cd "$work" && pwd -P)
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$repo"/*)
    echo "pii_control: the work directory $work is inside the repository" >&2
    exit 1
    ;;
esac

fail() {
  echo "pii_control: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "pii_control: $1: ok"
}

iso=$work/repo
mkdir -p "$iso/scripts"
cp "$repo/scripts/hygiene.sh" "$iso/scripts/hygiene.sh"
(
  cd "$iso"
  git init -q .
  git config user.email control@users.noreply.github.com
  git config user.name control
  git config commit.gpgsign false
  printf 'plain text\n' >readme.txt
  git add readme.txt scripts/hygiene.sh
  git commit -q -m initial
)

hyg() { (cd "$iso" && sh scripts/hygiene.sh "$@"); }
tree() { hyg --tree; }
history() { hyg --history; }

accept() {
  if ! "$2" >"$work/out" 2>"$work/err"; then
    cat "$work/err" >&2
    fail "$1" "expected a pass, got exit 1"
  fi
}

reject() {
  rc=0
  "$2" >"$work/out" 2>"$work/err" || rc=$?
  if [ "$rc" -ne 1 ]; then
    cat "$work/err" >&2
    fail "$1" "expected exit 1, got $rc"
  fi
  if ! grep -F -q "$3" "$work/err"; then
    cat "$work/err" >&2
    fail "$1" "expected finding text: $3"
  fi
}

# rep CHAR N: N copies of CHAR.
rep() { head -c "$2" /dev/zero | tr '\000' "$1"; }

# --- fake values, assembled from fragments --------------------------------------
home_a=/Us
home_b=ers/
home_c=someone
fake_home=$home_a$home_b$home_c
fake_home2=/ho$(printf 'me')/$home_c
at=@
fake_email=jane.doe${at}company.io
fake_ghp=gh$(printf 'p')_$(rep a 36)
fake_pat=github_$(printf 'pat')_$(rep b 30)
fake_aws=AK$(printf 'IA')$(rep A 16)
fake_pem=-----BEGIN$(printf ' RSA ')PRIVATE$(printf ' KEY')-----
fake_slack=xo$(printf 'xb')-$(rep 1 12)

# case_text NAME FILE CONTENT FINDING: write, expect FINDING, remove, expect a pass.
case_text() {
  printf '%s\n' "$3" >"$iso/$2"
  reject "$1" tree "$4$2"
  rm -f "$iso/$2"
  accept "$1_removed" tree
  ok "$1"
}

accept clean tree
ok clean_tree

case_text home_path notes.txt "see $fake_home/project for details" 'hygiene: home path: '
case_text home_path_linux notes.txt "cd $fake_home2/work" 'hygiene: home path: '
case_text email notes.txt "contact $fake_email today" 'hygiene: email: '
case_text credential_github_token notes.txt "token $fake_ghp" 'hygiene: credential: '
case_text credential_fine_grained notes.txt "token $fake_pat" 'hygiene: credential: '
case_text credential_aws notes.txt "key $fake_aws" 'hygiene: credential: '
case_text credential_pem notes.txt "$fake_pem" 'hygiene: credential: '
case_text credential_slack notes.txt "token $fake_slack" 'hygiene: credential: '

# The allowed forms pass: the AI co-author noreply, the GitHub noreply domain,
# a hosted runner home, an example domain, a tilde path and a short prefix.
# shellcheck disable=SC2088 # the tilde path is literal text on purpose
printf '%s\n' \
  "Co-Authored-By: Someone <noreply${at}anthropic.com>" \
  "12345+user${at}users.noreply.github.com" \
  "/home/runner/work/x" \
  "user${at}example.com" \
'~/projects/x and /usr/local/bin and ghp_short' >"$iso/allowed.txt"
accept allowed_forms tree
rm -f "$iso/allowed.txt"
ok allowed_forms_accepted

# A binary file is owned by the binary scan; this scan does not read it.
{
  printf '\000\001 %s\n' "$fake_home"
} >"$iso/blob.dat"
accept binary_skipped tree
rm -f "$iso/blob.dat"
ok binary_not_read

# A file named in the scan's own skip (scripts/hygiene.sh) is the only exclusion:
# the same text elsewhere is a finding even when the name looks similar.
printf '%s\n' "$fake_home" >"$iso/scripts/hygiene.sh.txt"
reject only_self_excluded tree 'hygiene: home path: scripts/hygiene.sh.txt'
rm -f "$iso/scripts/hygiene.sh.txt"
ok only_self_excluded

# --- history ------------------------------------------------------------------

accept history_clean history
ok history_clean

# A commit whose message carries the AI co-author trailer passes.
(
  cd "$iso"
  printf 'one\n' >a.txt
  git add a.txt
  git commit -q -m "add a

Co-Authored-By: Someone <noreply${at}anthropic.com>"
)
accept history_trailer history
ok history_coauthor_trailer_accepted

# A non-noreply author, then a non-noreply committer.
(
  cd "$iso"
  printf 'two\n' >b.txt
  git add b.txt
  git -c "user.email=$fake_email" commit -q -m "add b"
)
reject history_identity history 'hygiene: identity: '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_identity_removed history
ok history_author_identity

(
  cd "$iso"
  printf 'two\n' >b.txt
  git add b.txt
  GIT_COMMITTER_EMAIL=$fake_email git commit -q -m "add b"
)
reject history_committer history 'hygiene: identity: '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_committer_removed history
ok history_committer_identity

# Content removed by a later commit is still in history and is still found.
(
  cd "$iso"
  printf '%s\n' "$fake_ghp" >secret.txt
  git add secret.txt
  git commit -q -m "add secret"
  git rm -q secret.txt
  git commit -q -m "remove secret"
)
reject history_removed_credential history 'hygiene: credential: commit '
(
  cd "$iso"
  git reset -q --hard HEAD~2
)
accept history_credential_removed history
ok history_credential_in_old_commit

(
  cd "$iso"
  printf 'three\n' >c.txt
  git add c.txt
  git commit -q -m "path $fake_home/x in the message"
)
reject history_message_path history 'hygiene: home path: message of '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_message_removed history
ok history_home_path_in_message

# The repository itself was not touched.
if [ -n "$(find "$repo" -name 'secret.txt' -print 2>/dev/null)" ]; then
  fail clean "a seeded file reached the repository"
fi
ok isolated_tree
