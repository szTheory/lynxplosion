#!/bin/sh
# Negative controls for scripts/check-manifests.sh, run as
#   tests/gates/manifests_controls.sh REPO_ROOT WORK [--guard-probe]
#
# Fault prevented: a manifest check that accepts a missing, malformed, repeated
# or ambiguous manifest, or a fixture nobody has recorded. Every tree is built
# under a fresh, exclusive WORK directory from the two real scripts, one text
# fixture and a good manifest, and every file written here is text. Each reject
# case asserts exit 1 and the finding's wording.
set -eu

guard_probe=0
case $# in
  2) ;;
  3)
    [ "$3" = "--guard-probe" ] || {
      echo "usage: manifests_controls.sh REPO_ROOT WORK [--guard-probe]" >&2
      exit 2
    }
    guard_probe=1
    ;;
  *)
    echo "usage: manifests_controls.sh REPO_ROOT WORK [--guard-probe]" >&2
    exit 2
    ;;
esac

repo=$(cd "$1" && pwd -P)
requested_work=$2

refuse_work() {
  printf 'manifests_controls: refusing unsafe WORK %s: %s\n' "$requested_work" "$1" >&2
  exit 2
}

# Resolve and validate WORK before creating or removing anything. CTest uses
# build/check/scratch/gates/manifests; direct invocations can use a fresh child
# of the system temporary directory. Existing targets are caller-owned and are
# never cleared or reused.
case $requested_work in
  ''|/|.|..|*/|*//*|./*|../*|*/./*|*/../*|*/.|*/..) refuse_work "path is empty, aliased, or contains traversal" ;;
esac
caller_dir=$(pwd -P)
case $requested_work in
  /*) requested_abs=$requested_work ;;
  *) requested_abs=$caller_dir/$requested_work ;;
esac
work_leaf=${requested_abs##*/}
work_parent=${requested_abs%/*}
if [ -z "$work_leaf" ] || [ -z "$work_parent" ]; then refuse_work "path has no final directory name"; fi
[ -d "$work_parent" ] || refuse_work "parent directory does not exist"
canonical_parent=$(CDPATH='' cd -P "$work_parent" 2>/dev/null && pwd -P) || refuse_work "parent cannot be resolved"
work=$canonical_parent/$work_leaf

allowed=0
case $work/ in
  "$repo"/*)
    repo_scratch=$repo/build/check/scratch
    if [ -d "$repo_scratch" ]; then
      canonical_repo_scratch=$(CDPATH='' cd -P "$repo_scratch" 2>/dev/null && pwd -P) || canonical_repo_scratch=
      if [ "$canonical_repo_scratch" = "$repo_scratch" ] &&
         [ "$work" != "$canonical_repo_scratch" ]; then
        case $work/ in "$canonical_repo_scratch"/*) allowed=1 ;; esac
      fi
    fi
    [ "$allowed" -eq 1 ] || refuse_work "repository and source paths are not scratch"
    ;;
  *)
    for temp_root in "${TMPDIR:-/tmp}" /tmp /private/tmp; do
      [ -d "$temp_root" ] || continue
      canonical_temp_root=$(CDPATH='' cd -P "$temp_root" 2>/dev/null && pwd -P) || continue
      if [ "$work" != "$canonical_temp_root" ]; then
        case $work/ in "$canonical_temp_root"/*) allowed=1 ;; esac
      fi
      [ "$allowed" -eq 1 ] && break
    done
    [ "$allowed" -eq 1 ] || refuse_work "WORK must be below a dedicated project or system temporary scratch root"
    ;;
esac

if [ -e "$work" ] || [ -L "$work" ]; then
  refuse_work "target already exists; stale or caller-owned scratch is not reusable"
fi

# The optional probe mode runs only the same validation path and creates
# nothing. It lets the control below exercise unsafe inputs in a synthetic
# repository without ever directing a destructive command at the real tree.
if [ "$guard_probe" -eq 1 ]; then
  exit 0
fi

umask 077
if ! mkdir "$work"; then
  refuse_work "exclusive directory creation failed"
fi
work_owner=$work/.manifests-controls-owner
owner_token=$$-$(date +%s)
printf '%s\n' "$owner_token" >"$work_owner"

assert_owned_work() {
  if [ ! -d "$work" ] || [ -L "$work" ]; then fail ownership "scratch directory changed type"; fi
  actual_work=$(CDPATH='' cd -P "$work" 2>/dev/null && pwd -P) || fail ownership "scratch directory cannot be resolved"
  [ "$actual_work" = "$work" ] || fail ownership "scratch directory no longer has its original identity"
  if [ ! -f "$work_owner" ] || [ -L "$work_owner" ]; then fail ownership "ownership marker is missing"; fi
  actual_token=$(cat "$work_owner")
  [ "$actual_token" = "$owner_token" ] || fail ownership "ownership marker does not match this invocation"
}

remove_owned_child() {
  child=$1
  assert_owned_work
  case $child in
    "$work"/*)
      child_leaf=${child#"$work"/}
      case $child_leaf in ''|*/*|.|..) fail ownership "refusing non-child scratch removal: $child" ;; esac
      rm -rf "$child"
      ;;
    *) fail ownership "refusing scratch removal outside invocation directory: $child" ;;
  esac
}

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
  remove_owned_child "$dir"
  mkdir -p "$dir/scripts" "$dir/fixtures/manifests"
  cp "$repo/scripts/validate-kv.sh" "$repo/scripts/check-manifests.sh" "$dir/scripts/"
  printf 'text fixture a\n' >"$dir/fixtures/a.txt"
  write_manifest "$dir/fixtures/manifests/a.manifest" "$hash64" fixtures/a.txt
}

# Black-box controls for the cleanup guard. Every marker lives in a synthetic
# replica inside this invocation-owned scratch. The child calls exit during
# path validation, before it can create or remove its WORK target.
run_work_guard_controls() {
  guard_repo=$work/guard-repo
  mkdir -p "$guard_repo/scripts" "$guard_repo/fixtures"
  cp "$repo/scripts/validate-kv.sh" "$repo/scripts/check-manifests.sh" "$guard_repo/scripts/"
  printf 'synthetic-root-marker\n' >"$guard_repo/root.marker"
  printf 'synthetic-fixture-marker\n' >"$guard_repo/fixtures/marker.txt"
  printf 'synthetic-script-marker\n' >"$guard_repo/scripts/marker.txt"

  expect_guard_refusal() {
    guard_name=$1
    guard_root=$2
    guard_target=$3
    marker=$4
    rc=0
    sh "$script_path" "$guard_root" "$guard_target" --guard-probe \
      >"$work/guard-$guard_name.out" 2>&1 || rc=$?
    [ "$rc" -eq 2 ] || fail "work_guard_$guard_name" "expected safe refusal 2, got $rc"
    grep -qF 'refusing unsafe WORK' "$work/guard-$guard_name.out" ||
      fail "work_guard_$guard_name" "guard refusal did not identify unsafe WORK"
    [ -f "$marker" ] || fail "work_guard_$guard_name" "synthetic marker was changed or removed"
    ok "work_guard_$guard_name"
  }

  expect_guard_refusal repo_root "$guard_repo" "$guard_repo" "$guard_repo/root.marker"
  expect_guard_refusal source_fixtures "$guard_repo" "$guard_repo/fixtures" "$guard_repo/fixtures/marker.txt"
  expect_guard_refusal source_scripts "$guard_repo" "$guard_repo/scripts" "$guard_repo/scripts/marker.txt"

  stale=$work/stale-caller-scratch
  mkdir "$stale"
  printf 'stale-caller-marker\n' >"$stale/marker.txt"
  expect_guard_refusal stale_target "$repo" "$stale" "$stale/marker.txt"

  expect_guard_refusal traversal_alias "$guard_repo" "$guard_repo/fixtures/../fixtures" \
    "$guard_repo/fixtures/marker.txt"
  ln -s "$guard_repo/fixtures" "$work/fixture-alias"
  expect_guard_refusal symlink_alias "$guard_repo" "$work/fixture-alias/new-work" \
    "$guard_repo/fixtures/marker.txt"

  positive=$work/fresh-positive
  sh "$script_path" "$repo" "$positive" --guard-probe >"$work/guard-positive.out" 2>&1 ||
    fail work_guard_positive "fresh project scratch was rejected: $(cat "$work/guard-positive.out")"
  [ ! -e "$positive" ] || fail work_guard_positive "probe mode created its candidate"
  ok work_guard_positive
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

# Add the exact optional provenance extension introduced for the input-probe
# frame-replay fixture. Hash values exercise the common SHA-256 field contract;
# byte counts and the authored legacy-source identity are fixed by the captured
# source-build records in fixtures/manifests/input-probe.manifest.
append_input_probe_fields() {
  cat >>"$1" <<EOF2
config_sha256 = $hash64
cc65_pin_sha256 = $hash64
cc65_build_recipe = 1
legacy_source = fixtures/diagnostic/input_probe_tracer.s (project-authored)
legacy_source_sha256 = $hash64
source_built_legacy_tracer_guest_sha256 = $hash64
source_built_legacy_tracer_guest_bytes = 172
source_built_guest_sha256_lefthand_0 = $hash64
source_built_guest_bytes_lefthand_0 = 201
source_built_guest_sha256_lefthand_1 = $hash64
source_built_guest_bytes_lefthand_1 = 201
EOF2
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

script_path=$0
case $script_path in
  /*) ;;
  *) script_path=$(pwd -P)/$script_path ;;
esac
if [ "$guard_probe" -eq 0 ]; then
  run_work_guard_controls
fi

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

# 8b. startup_tier: a fixture manifest may record "none" (a software-model
# vector injects CPU state and is not tier A); any other value is refused.
make_tree "$t"
sed 's/^startup_tier = A$/startup_tier = none/' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
run_check "$t"
[ "$rc" -eq 0 ] || fail startup_tier_none "a fixture manifest with startup_tier none was rejected: $out"
ok startup_tier_none
make_tree "$t"
sed 's/^startup_tier = A$/startup_tier = Z/' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject startup_tier_unknown "$t" "fixtures/manifests/a.manifest: line 8: startup_tier: bad startup_tier"

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

# WR-05 and IN-09: the validator this check runs refuses a glob character in the
# tier list and an empty required list as usage errors (exit 2). The directory
# holds letters-only file names that an unquoted expansion would have turned
# into tier words.
mkdir -p "$work/glob"
: >"$work/glob/A"
: >"$work/glob/Abc"
: >"$work/glob/empty.kv"
for bad_list in "A*" "*" "[A]"; do
  rc=0
  (cd "$work/glob" && sh "$repo/scripts/validate-kv.sh" --required "claim" --startup-tiers "$bad_list" "$work/glob/empty.kv") >/dev/null 2>&1 || rc=$?
  [ "$rc" -eq 2 ] || fail validator_glob "tier list \"$bad_list\": expected exit 2, got $rc"
done
rc=0
(cd "$work/glob" && sh "$repo/scripts/validate-kv.sh" --required "" "$work/glob/empty.kv") >/dev/null 2>&1 || rc=$?
[ "$rc" -eq 2 ] || fail validator_required "an empty required list: expected exit 2, got $rc"
ok validator_glob

# 17. The complete 05-07 input-probe identity extension is accepted, while its
# key set, identity types and source-built lengths remain strict.
make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
run_check "$t"
[ "$rc" -eq 0 ] || fail input_probe_extension "complete provenance group was rejected: $out"
ok input_probe_extension

for malformed_pin in 0123 g000000000000000000000000000000000000000000000000000000000000000; do
  make_tree "$t"
  append_input_probe_fields "$t/fixtures/manifests/a.manifest"
  sed "s/^cc65_pin_sha256 = .*/cc65_pin_sha256 = $malformed_pin/" \
    "$t/fixtures/manifests/a.manifest" >"$work/m"
  cp "$work/m" "$t/fixtures/manifests/a.manifest"
  expect_reject input_probe_bad_tool_pin "$t" "cc65_pin_sha256: bad sha256"
done
make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
sed 's/^cc65_build_recipe = 1$/cc65_build_recipe = 2/' \
  "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject input_probe_bad_tool_recipe "$t" "cc65_build_recipe: unexpected cc65 build recipe"

make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
sed 's/^source_built_guest_bytes_lefthand_0 = 201$/source_built_guest_bytes_lefthand_0 = 202/' \
  "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject input_probe_wrong_length "$t" "source_built_guest_bytes_lefthand_0: unexpected source-built guest byte count"

make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
sed 's/^config_sha256 = .*/config_sha256 = 0123/' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject input_probe_bad_hash "$t" "config_sha256: bad sha256"

short_hash=$(printf '%s' "$hash64" | cut -c 1-63)
nonhex_hash=g$(printf '%063d' 0)
for handedness in 0 1; do
  hash_key=source_built_guest_sha256_lefthand_$handedness

  make_tree "$t"
  append_input_probe_fields "$t/fixtures/manifests/a.manifest"
  sed "s/^${hash_key} = .*/${hash_key} = ${short_hash}/" \
    "$t/fixtures/manifests/a.manifest" >"$work/m"
  cp "$work/m" "$t/fixtures/manifests/a.manifest"
  expect_reject "input_probe_bad_hash_short_lefthand_$handedness" "$t" "$hash_key: bad sha256"

  make_tree "$t"
  append_input_probe_fields "$t/fixtures/manifests/a.manifest"
  sed "s/^${hash_key} = .*/${hash_key} = ${nonhex_hash}/" \
    "$t/fixtures/manifests/a.manifest" >"$work/m"
  cp "$work/m" "$t/fixtures/manifests/a.manifest"
  expect_reject "input_probe_bad_hash_nonhex_lefthand_$handedness" "$t" "$hash_key: bad sha256"
done

make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
sed '/^legacy_source_sha256 = /d' "$t/fixtures/manifests/a.manifest" >"$work/m"
cp "$work/m" "$t/fixtures/manifests/a.manifest"
expect_reject input_probe_incomplete "$t" "incomplete input-probe provenance group: missing key: legacy_source_sha256"

make_tree "$t"
append_input_probe_fields "$t/fixtures/manifests/a.manifest"
printf 'source_built_guest_sha256_lefthand_2 = %s\n' "$hash64" >>"$t/fixtures/manifests/a.manifest"
expect_reject input_probe_unknown_key "$t" "source_built_guest_sha256_lefthand_2: unknown key"
assert_owned_work
rm -rf "$work"
