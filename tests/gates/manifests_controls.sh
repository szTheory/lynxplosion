#!/bin/sh
# Negative controls for scripts/check-manifests.sh, run as
#   tests/gates/manifests_controls.sh REPO_ROOT WORK
#
# Fault prevented: a manifest check that accepts a missing, malformed, repeated
# or ambiguous manifest, or a fixture nobody has recorded. Every tree is built
# under WORK (replaced on each run) from the two real scripts, one text fixture
# and a good manifest, and every file written here is text. Each reject case
# asserts exit 1 and the finding's wording.
set -eu

if [ $# -ne 2 ]; then
  echo "usage: manifests_controls.sh REPO_ROOT WORK" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)
work=$2
if [ -z "$work" ] || [ "$work" = "/" ]; then
  echo "manifests_controls: WORK must be a directory path" >&2
  exit 2
fi
rm -rf "$work"
mkdir -p "$work"
work=$(cd "$work" && pwd -P)
case $work/ in
  "$repo"/fixtures/* | "$repo"/scripts/*)
    echo "manifests_controls: WORK $work is inside a checked directory" >&2
    exit 1
    ;;
esac

fail() {
  echo "manifests_controls: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "manifests_controls: $1: ok"
}

hash64=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
other64=fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210

# make_tree DIR: a clean tree whose one fixture a.txt is covered by a.manifest.
make_tree() {
  dir=$1
  rm -rf "$dir"
  mkdir -p "$dir/scripts" "$dir/fixtures/manifests"
  cp "$repo/scripts/validate-kv.sh" "$repo/scripts/check-manifests.sh" "$dir/scripts/"
  printf 'text fixture a\n' >"$dir/fixtures/a.txt"
  write_manifest "$dir/fixtures/manifests/a.manifest" "$hash64" fixtures/a.txt
}

# write_manifest FILE SHA256 FILES
write_manifest() {
  {
    printf 'source = fixtures/a.txt (control, project-authored)\n'
    printf 'revision = in-repo\n'
    printf 'recipe = written by the control\n'
    printf 'sha256 = %s\n' "$2"
    printf 'licence = control only\n'
    printf 'notices = none\n'
    printf 'allowed_use = control only\n'
    printf 'startup_tier = A\n'
    printf 'profile = none\n'
    printf 'evidence_tier = R\n'
    printf 'files = %s\n' "$3"
  } >"$1"
}

# run_check DIR: sets rc and out (both streams).
run_check() {
  rc=0
  out=$(sh "$1/scripts/check-manifests.sh" --root "$1" 2>&1) || rc=$?
}

# expect_reject NAME DIR TEXT: exit 1 and TEXT present in the output.
expect_reject() {
  run_check "$2"
  [ "$rc" -eq 1 ] || fail "$1" "expected exit 1, got $rc: $out"
  printf '%s\n' "$out" | grep -qF -- "$3" || fail "$1" "no '$3' in: $out"
  ok "$1"
}

t=$work/t

# 1. A clean tree passes.
make_tree "$t"
run_check "$t"
[ "$rc" -eq 0 ] || fail pass "expected exit 0, got $rc: $out"
ok pass

# 2. Key order inside a file does not matter (ordering edge).
make_tree "$t"
sed '1!G;h;$!d' "$t/fixtures/manifests/a.manifest" >"$work/reversed"
cp "$work/reversed" "$t/fixtures/manifests/a.manifest"
run_check "$t"
[ "$rc" -eq 0 ] || fail key_order "a reversed manifest was rejected: $out"
ok key_order

# 3. A missing key.
make_tree "$t"
sed '/^licence = /d' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject missing_key "$t" "missing key: licence"

# 4. An unknown key.
make_tree "$t"
printf 'bogus = x\n' >>"$t/fixtures/manifests/a.manifest"
expect_reject unknown_key "$t" "unknown key"

# 5. A duplicated key.
make_tree "$t"
printf 'revision = in-repo\n' >>"$t/fixtures/manifests/a.manifest"
expect_reject duplicate_key "$t" "duplicate key"

# 6. An empty value (empty edge).
make_tree "$t"
sed 's/^notices = none$/notices = /' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject empty_value "$t" "empty value"

# 7. A 63-digit identity.
make_tree "$t"
short=$(printf '%s' "$hash64" | cut -c 1-63)
write_manifest "$t/fixtures/manifests/a.manifest" "$short" fixtures/a.txt
expect_reject bad_sha256 "$t" "bad sha256"

# 8. An evidence tier outside the vocabulary.
make_tree "$t"
sed 's/^evidence_tier = R$/evidence_tier = T9/' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject bad_evidence_tier "$t" "bad evidence_tier"

# 9. Two manifests with one identity (adjacency edge).
make_tree "$t"
printf 'text fixture b\n' >"$t/fixtures/b.txt"
write_manifest "$t/fixtures/manifests/b.manifest" "$hash64" fixtures/b.txt
expect_reject duplicate_sha256 "$t" \
  "check-manifests: duplicate sha256 in fixtures/manifests/a.manifest and fixtures/manifests/b.manifest"

# 10. A fixture no manifest lists (empty edge: no manifest at all for it).
make_tree "$t"
printf 'text fixture c\n' >"$t/fixtures/c.txt"
expect_reject uncovered_fixture "$t" "check-manifests: uncovered fixture: fixtures/c.txt"

# 11. A files entry naming a missing file.
make_tree "$t"
write_manifest "$t/fixtures/manifests/a.manifest" "$hash64" "fixtures/a.txt fixtures/gone.txt"
expect_reject files_entry_missing "$t" \
  "check-manifests: fixtures/manifests/a.manifest: files entry missing: fixtures/gone.txt"

# 12. A files entry that leaves fixtures/.
make_tree "$t"
printf 'outside\n' >"$t/outside.txt"
write_manifest "$t/fixtures/manifests/a.manifest" "$hash64" "fixtures/a.txt ../outside.txt"
expect_reject files_entry_escape "$t" "files entry not a fixture path: ../outside.txt"

# 13. No manifests at all.
make_tree "$t"
rm "$t/fixtures/manifests/a.manifest"
expect_reject no_manifests "$t" "no manifests under fixtures/manifests"

# 14. Findings are stable: manifests are visited in sorted order whatever the
# order they were created in.
make_tree "$t"
rm "$t/fixtures/manifests/a.manifest"
printf 'text fixture z\n' >"$t/fixtures/z.txt"
printf 'text fixture y\n' >"$t/fixtures/y.txt"
write_manifest "$t/fixtures/manifests/zzz.manifest" "$hash64" "fixtures/a.txt fixtures/z.txt"
write_manifest "$t/fixtures/manifests/aaa.manifest" "$other64" "fixtures/y.txt"
for m in zzz aaa; do
  sed '/^licence = /d' "$t/fixtures/manifests/$m.manifest" >"$work/m"
  cp "$work/m" "$t/fixtures/manifests/$m.manifest"
done
run_check "$t"
[ "$rc" -eq 1 ] || fail sorted_findings "expected exit 1, got $rc: $out"
first=$(printf '%s\n' "$out" | grep -F 'missing key' | head -n 1)
case $first in
  *aaa.manifest*) ok sorted_findings ;;
  *) fail sorted_findings "the first finding is not for aaa.manifest: $first" ;;
esac
# 15. A file under fixtures/manifests/ that is not a manifest is an uncovered
# fixture, at the top level and below it (the directory is not an exemption).
make_tree "$t"
printf 'not a manifest\n' >"$t/fixtures/manifests/blob.dat"
expect_reject manifests_dir_other_file "$t" \
  "check-manifests: uncovered fixture: fixtures/manifests/blob.dat"
make_tree "$t"
mkdir "$t/fixtures/manifests/sub"
write_manifest "$t/fixtures/manifests/sub/b.manifest" "$other64" fixtures/a.txt
expect_reject manifests_dir_nested_manifest "$t" \
  "check-manifests: uncovered fixture: fixtures/manifests/sub/b.manifest"

# 16. A termination signal in the middle of a run ends the check with a failing
# status and removes its scratch directory; it never completes as a pass. The
# validator is replaced by one that signals the check that started it, then
# returns success, which is exactly what let the old trap fall through to a
# success line and exit 0.
make_tree "$t"
cat >"$t/scripts/validate-kv.sh" <<'EOF2'
#!/bin/sh
n=0
while [ ! -s "$LYNX_PIDFILE" ] && [ "$n" -lt 100 ]; do
  sleep 0.1
  n=$((n + 1))
done
kill -TERM "$(cat "$LYNX_PIDFILE")"
sleep 1
exit 0
EOF2
mkdir -p "$work/tmp"
rm -f "$work/pid"
rc=0
(
  TMPDIR=$work/tmp LYNX_PIDFILE=$work/pid sh "$t/scripts/check-manifests.sh" --root "$t" >"$work/sig.out" 2>&1 &
  printf '%s\n' "$!" >"$work/pid"
  wait "$!"
) || rc=$?
[ "$rc" -ne 0 ] || fail signal_exit "a terminated check exited 0: $(cat "$work/sig.out")"
if grep -q 'covered' "$work/sig.out"; then
  fail signal_exit "a terminated check printed its success line"
fi
[ -z "$(ls -A "$work/tmp")" ] || fail signal_cleanup "the scratch directory was left behind"
ok signal_exit
rm -rf "$work"
