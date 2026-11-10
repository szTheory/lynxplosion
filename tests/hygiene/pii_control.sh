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
fake_ant=s$(printf 'k-an')t-$(rep c 24)
fake_sk=s$(printf 'k-')$(rep d 40)
fake_npm=np$(printf 'm_')$(rep e 36)
fake_jwt=ey$(printf 'J')$(rep f 12).ey$(printf 'J')$(rep g 12).$(rep h 12)
fake_root=/ro$(printf 'ot')/$home_c
fake_win=C:/Us$(printf 'ers')/$home_c
bs=$(printf '\134')
fake_win_json=C:$bs${bs}Us$(printf 'ers')$bs$bs$home_c

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

# The scan keeps its file list in a private scratch directory with an
# unpredictable name and removes it; it never uses a fixed, guessable name that a
# planted symlink could redirect (WR-09). A git wrapper on PATH records what the
# scratch area holds at the moment the scan lists the files.
shim=$work/shim
scratch=$work/scratch
mkdir -p "$shim" "$scratch"
real_git=$(command -v git)
# shellcheck disable=SC2016 # the wrapper's own variables are written literally
{
  printf '#!/bin/sh\n'
  printf 'if [ -n "${LYNX_FAIL:-}" ]; then case " $* " in *" $LYNX_FAIL "*) exit 2 ;; esac; fi\n'
  printf 'case " $* " in *" ls-files "*) ls -A "$LYNX_SCRATCH" >"$LYNX_SNAP" ;; esac\n'
  printf 'exec "$LYNX_REAL_GIT" "$@"\n'
} >"$shim/git"
chmod +x "$shim/git"
(
  cd "$iso"
  PATH="$shim:$PATH" TMPDIR="$scratch" LYNX_SCRATCH="$scratch" LYNX_SNAP="$work/snap" \
    LYNX_REAL_GIT="$real_git" sh scripts/hygiene.sh --tree
) >"$work/out" 2>"$work/err" || {
  cat "$work/err" >&2
  fail scratch_tree "the scan failed with a private TMPDIR"
}
if grep -q '^hygiene-list' "$work/snap"; then
  fail scratch_name "the file list used a fixed, guessable name"
fi
if ! grep -q '^hygiene\.' "$work/snap"; then
  fail scratch_dir "no private scratch directory existed during the scan"
fi
if [ -n "$(ls -A "$scratch")" ]; then
  fail scratch_cleanup "the scan left files in TMPDIR"
fi
ok scratch_private_and_removed

case_text home_path notes.txt "see $fake_home/project for details" 'hygiene: home path: '
case_text home_path_linux notes.txt "cd $fake_home2/work" 'hygiene: home path: '
case_text email notes.txt "contact $fake_email today" 'hygiene: email: '
case_text credential_github_token notes.txt "token $fake_ghp" 'hygiene: credential: '
case_text credential_fine_grained notes.txt "token $fake_pat" 'hygiene: credential: '
case_text credential_aws notes.txt "key $fake_aws" 'hygiene: credential: '
case_text credential_pem notes.txt "$fake_pem" 'hygiene: credential: '
case_text credential_slack notes.txt "token $fake_slack" 'hygiene: credential: '

case_text credential_anthropic notes.txt "token $fake_ant" 'hygiene: credential: '
case_text credential_sk notes.txt "key=$fake_sk" 'hygiene: credential: '
case_text credential_npm notes.txt "token $fake_npm" 'hygiene: credential: '
case_text credential_jwt notes.txt "bearer $fake_jwt" 'hygiene: credential: '
# The root account's home is not a person's, and a work directory named root is
# common in scripts, so /root/ is deliberately not a pattern.
printf '%s\n' "cp x $fake_root/.cache" >"$iso/rootdir.txt"
accept root_home_not_personal tree
rm -f "$iso/rootdir.txt"
ok root_home_not_a_pattern
case_text home_path_windows notes.txt "open $fake_win/file" 'hygiene: home path: '
case_text home_path_windows_json notes.txt "{\"path\": \"$fake_win_json\\\\x\"}" 'hygiene: home path: '
# Words that merely contain sk- or end in a short token are not credentials.
printf '%s\n' "the risk-adjusted-plan-for-the-whole-walking-skeleton-build and task-$(rep d 40)" >"$iso/words.txt"
accept sk_inside_word tree
rm -f "$iso/words.txt"
ok sk_inside_word_accepted

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

# An allowed token never hides a leak on the same line, and an allowed form is
# matched as a whole token, not as a substring of a longer name (CR-01).
case_text same_line_email notes.txt "contact $fake_email or git${at}github.com" 'hygiene: email: '
case_text same_line_email_reversed notes.txt "git${at}github.com or $fake_email" 'hygiene: email: '
case_text same_line_noreply notes.txt "noreply${at}anthropic.com and $fake_email" 'hygiene: email: '
case_text same_line_home notes.txt "/home/runner/x and $fake_home/y" 'hygiene: home path: '
case_text same_line_home_reversed notes.txt "$fake_home/y and /home/runner/x" 'hygiene: home path: '
case_text email_suffix_of_allowed notes.txt "digit${at}github.com" 'hygiene: email: '
case_text email_prefix_of_allowed notes.txt "user${at}example.com.$home_c.io" 'hygiene: email: '
case_text home_prefix_of_allowed notes.txt "/home/runner$home_c/x" 'hygiene: home path: '

# A binary file is owned by the binary scan; this scan does not read it.
{
  printf '\000\001 %s\n' "$fake_home"
} >"$iso/blob.dat"
accept binary_skipped tree
rm -f "$iso/blob.dat"
ok binary_not_read

# No file is excluded from the scan, the script included: the same text in
# scripts/hygiene.sh or in a file with a similar name is a finding (WR-10).
printf '%s\n' "$fake_home" >"$iso/scripts/hygiene.sh.txt"
reject similar_name_not_excluded tree 'hygiene: home path: scripts/hygiene.sh.txt'
rm -f "$iso/scripts/hygiene.sh.txt"
cp "$iso/scripts/hygiene.sh" "$work/hygiene.orig"
printf '# %s\n' "$fake_home" >>"$iso/scripts/hygiene.sh"
reject script_itself_scanned tree 'hygiene: home path: scripts/hygiene.sh'
cp "$work/hygiene.orig" "$iso/scripts/hygiene.sh"
accept script_restored tree
ok scan_excludes_no_file

# A tracked symlink is read by its target text, in tree and history modes.
ln -s "$fake_home/target" "$iso/link"
reject symlink_target_home tree 'hygiene: home path: link'
(
  cd "$iso"
  git add link
  git commit -q -m "add link"
)
reject symlink_target_history history 'hygiene: home path: commit '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
rm -f "$iso/link"
accept symlink_removed tree
ln -s ../readme.txt "$iso/okay_link"
accept symlink_harmless tree
rm -f "$iso/okay_link"
ok symlink_target_scanned

# A file name is scanned like content, and the finding does not print it.
dashed=-Us${home_b%/}-$home_c-
: >"$iso/$dashed-notes.txt"
reject name_dashed_home tree 'hygiene: home path: a file name in the tree'
if grep -q "$home_c" "$work/err"; then
  fail name_not_printed "the finding printed the file name"
fi
(
  cd "$iso"
  git add -- "$dashed-notes.txt"
  git commit -q -m "add named file"
)
reject name_history history 'hygiene: home path: a file name in commit '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
rm -f "$iso/$dashed-notes.txt"
accept name_removed tree
ok file_names_scanned

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

# The same-line cases in history: a blob and a message (CR-01).
(
  cd "$iso"
  printf '%s\n' "$fake_home/y and /home/runner/x" >d.txt
  git add d.txt
  git commit -q -m "add d"
)
reject history_blob_same_line_home history 'hygiene: home path: commit '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_blob_same_line_removed history
(
  cd "$iso"
  printf '%s\n' "contact $fake_email or git${at}github.com" >d.txt
  git add d.txt
  git commit -q -m "add d"
)
reject history_blob_same_line_email history 'hygiene: email: commit '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_blob_same_line_email_removed history
(
  cd "$iso"
  printf 'four\n' >e.txt
  git add e.txt
  git commit -q -m "mail $fake_email or git${at}github.com, dir /home/runner/x and $fake_home/z"
)
reject history_message_same_line_email history 'hygiene: email: message of '
reject history_message_same_line_home history 'hygiene: home path: message of '
(
  cd "$iso"
  git reset -q --hard HEAD~1
)
accept history_message_same_line_removed history
ok history_same_line_leaks

# A git failure is never an empty or clean history (WR-01): a revision that does
# not exist, a failing git grep and a failing git log each fail the scan.
bogus_rev() { hyg --history bogusrev; }
with_failing_git() { (cd "$iso" && PATH="$shim:$PATH" LYNX_FAIL="$1" LYNX_REAL_GIT="$real_git" LYNX_SCRATCH="$scratch" LYNX_SNAP="$work/snap2" sh scripts/hygiene.sh --history); }
failing_grep() { with_failing_git grep; }
failing_log() { with_failing_git log; }
reject history_bogus_revision bogus_rev 'hygiene: cannot list commits'
reject history_failing_grep failing_grep 'hygiene: git grep failed: commit '
reject history_failing_log failing_log 'hygiene: cannot read message: commit '
accept history_still_clean history
ok history_git_failure_fails_closed

# The repository itself was not touched.
if [ -n "$(find "$repo" -name 'secret.txt' -print 2>/dev/null)" ]; then
  fail clean "a seeded file reached the repository"
fi
ok isolated_tree
