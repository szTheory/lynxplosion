#!/bin/sh
# Negative controls for the step machinery of scripts/check.sh, run as
#   tests/gates/check_controls.sh REPO_ROOT
#
# Fault prevented: a gate step that fails in the middle and still passes. set -e
# is ignored inside a function whose status is tested, so a step run under
# `if !` could only fail through its last command. The machinery block of
# check.sh is run as written, against a step that fails before its last command,
# and the identities step is run with a tool that fails; both must stop the run
# with a non-zero status and the message naming the step. Everything is built
# under a temporary directory outside the repository.
set -eu

if [ $# -ne 1 ]; then
  echo "usage: check_controls.sh REPO_ROOT" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-checkctl.XXXXXX")
work=$(cd "$work" && pwd -P)
trap 'rm -rf "$work"' EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$repo"/*)
    echo "check_controls: the work directory $work is inside the repository" >&2
    exit 1
    ;;
esac

fail() {
  echo "check_controls: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "check_controls: $1: ok"
}

# 1. The machinery, run as written. A step whose first command fails must stop
# the run before its second command and before the next step.
sed -n '/^# --- step machinery/,/^# --- end of step machinery/p' "$repo/scripts/check.sh" >"$work/machinery.sh"
[ -s "$work/machinery.sh" ] || fail machinery_block "scripts/check.sh has no step machinery block"
{
  printf 'set -eu\n'
  cat "$work/machinery.sh"
  printf 'good() { echo good-ran; }\n'
  printf 'bad() { false; echo NOT-REACHED-IN-STEP; }\n'
  printf 'step first good\n'
  printf 'step second bad\n'
  printf 'echo NOT-REACHED-AFTER\n'
} >"$work/run.sh"
rc=0
sh "$work/run.sh" >"$work/out1" 2>"$work/err1" || rc=$?
[ "$rc" -ne 0 ] || fail mid_step_failure "a step that failed before its last command passed"
grep -q 'good-ran' "$work/out1" || fail mid_step_failure "the passing step did not run"
grep -q '^-- step first: ' "$work/out1" || fail mid_step_failure "no wall time for the passing step"
grep -q 'NOT-REACHED' "$work/out1" && fail mid_step_failure "a command ran after the failure"
grep -qx 'check: step second failed' "$work/err1" || fail mid_step_failure "no 'check: step second failed' in: $(cat "$work/err1")"
if grep -q 'check: step first failed' "$work/err1"; then
  fail mid_step_failure "the passing step was reported as failed"
fi
ok mid_step_failure

# 2. A tool that fails in the identities step fails the step; it never prints an
# empty identity and moves on. The real script runs in a tree of its own with a
# compiler stand-in that fails.
mkdir -p "$work/root/scripts" "$work/bin"
cp "$repo/scripts/check.sh" "$work/root/scripts/check.sh"
printf '#!/bin/sh\nexit 1\n' >"$work/bin/cc"
chmod +x "$work/bin/cc"
rc=0
PATH="$work/bin:$PATH" sh "$work/root/scripts/check.sh" >"$work/out2" 2>"$work/err2" || rc=$?
[ "$rc" -eq 1 ] || fail identities_tool_failure "expected exit 1, got $rc: $(cat "$work/out2")"
grep -qx 'check: step identities failed' "$work/err2" ||
  fail identities_tool_failure "no 'check: step identities failed' in: $(cat "$work/err2")"
if grep -q '^== step pins' "$work/out2"; then
  fail identities_tool_failure "the run went on past the failed identities step"
fi
ok identities_tool_failure
