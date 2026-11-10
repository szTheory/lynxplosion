#!/bin/sh
# Negative controls for scripts/check-claims.sh, run as
#   tests/gates/claims_controls.sh REPO_ROOT WORK
#
# Fault prevented: a claim check that accepts a claim without a profile, bound to
# a stale inventory, naming a hardware profile, with a repeated key or an empty
# value, or a template that has lost a key. Every tree is built under WORK
# (replaced on each run) from the real check-claims.sh, the real validate-kv.sh,
# the real template and a small inventory of its own. Each reject case asserts
# exit 1 and the finding's wording.
set -eu

if [ $# -ne 2 ]; then
  echo "usage: claims_controls.sh REPO_ROOT WORK" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)
work=$2
if [ -z "$work" ] || [ "$work" = "/" ]; then
  echo "claims_controls: WORK must be a directory path" >&2
  exit 2
fi
rm -rf "$work"
mkdir -p "$work"
work=$(cd "$work" && pwd -P)
case $work/ in
  "$repo"/docs/* | "$repo"/scripts/* | "$repo"/tests/inventory/*)
    echo "claims_controls: WORK $work is inside a checked directory" >&2
    exit 1
    ;;
esac

fail() {
  echo "claims_controls: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "claims_controls: $1: ok"
}

zeros64=0000000000000000000000000000000000000000000000000000000000000000
content64=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef

# make_tree DIR: a clean tree with the real scripts and template, a two-row
# inventory and one good claim bound to that inventory.
make_tree() {
  dir=$1
  rm -rf "$dir"
  mkdir -p "$dir/scripts" "$dir/docs/claims" "$dir/tests/inventory"
  cp "$repo/scripts/validate-kv.sh" "$repo/scripts/check-claims.sh" "$dir/scripts/"
  cp "$repo/docs/claims/claim.template" "$dir/docs/claims/claim.template"
  printf 'a.one\ta\nb.two\tb\n' >"$dir/tests/inventory/fast.txt"
  invsha=$(sh "$dir/scripts/check-claims.sh" --root "$dir" --inventory-sha256)
  write_claim "$dir/docs/claims/good.claim" "$invsha"
}

# write_claim FILE INVENTORY_SHA256
write_claim() {
  {
    printf 'claim = A control claim about a control workload\n'
    printf 'profile = none\n'
    printf 'content_sha256 = %s\n' "$content64"
    printf 'startup_tier = A\n'
    printf 'evidence_tier = R\n'
    printf 'inventory_sha256 = %s\n' "$2"
    printf 'evidence = control.evidence\n'
  } >"$1"
}

# accept CASE DIR: the check passes.
accept() {
  if ! sh "$2/scripts/check-claims.sh" --root "$2" >"$work/out.txt" 2>&1; then
    cat "$work/out.txt" >&2
    fail "$1" "expected exit 0"
  fi
  if [ -s "$work/out.txt" ]; then
    cat "$work/out.txt" >&2
    fail "$1" "expected no output"
  fi
  ok "$1"
}

# reject CASE DIR PATTERN: exit 1 and an output line matching the grep pattern.
reject() {
  rc=0
  sh "$2/scripts/check-claims.sh" --root "$2" >"$work/out.txt" 2>&1 || rc=$?
  if [ "$rc" -ne 1 ]; then
    cat "$work/out.txt" >&2
    fail "$1" "expected exit 1, got $rc"
  fi
  if ! grep -q "$3" "$work/out.txt"; then
    cat "$work/out.txt" >&2
    fail "$1" "output does not match: $3"
  fi
  ok "$1"
}

t=$work/t

make_tree "$t"
accept good_claim "$t"

# --- the claim file --------------------------------------------------------
make_tree "$t"
grep -v '^profile = ' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject missing_profile "$t" 'missing key: profile'

make_tree "$t"
write_claim "$t/docs/claims/good.claim" "$zeros64"
reject stale_inventory "$t" 'check-claims: good.claim: inventory_sha256 is stale'

make_tree "$t"
sed 's/^profile = none$/profile = lynx2/' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject lynx_profile "$t" 'check-claims: good.claim: profile lynx2 not allowed while O1 is open'

make_tree "$t"
sed 's/^startup_tier = A$/startup_tier = none/' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
accept startup_tier_none "$t"

make_tree "$t"
sed 's/^startup_tier = A$/startup_tier = Z/' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject startup_tier_unknown "$t" 'good.claim: line 4: startup_tier: bad startup_tier'

make_tree "$t"
printf 'profile = none\n' >>"$t/docs/claims/good.claim"
reject duplicate_key "$t" 'duplicate key'

make_tree "$t"
sed 's/^evidence = .*$/evidence = /' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject empty_value "$t" 'empty value'

make_tree "$t"
grep -v '^inventory_sha256 = ' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject missing_inventory_sha256 "$t" 'missing key: inventory_sha256'

make_tree "$t"
printf 'extra = 1\n' >>"$t/docs/claims/good.claim"
reject unknown_key "$t" 'extra: unknown key'

make_tree "$t"
sed 's/^inventory_sha256 = .*$/inventory_sha256 = abc/' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
reject short_inventory_sha256 "$t" 'inventory_sha256: bad sha256'

# --- the inventory binding (ordering edge) ---------------------------------
# The claim holds the hash of the committed bytes. Re-sorting or reformatting the
# inventory changes the bytes and so invalidates it.
make_tree "$t"
printf 'b.two\tb\na.one\ta\n' >"$t/tests/inventory/fast.txt"
reject inventory_reordered "$t" 'good.claim: inventory_sha256 is stale'

make_tree "$t"
printf 'a.one\ta\nb.two\tb\nc.three\tc\n' >"$t/tests/inventory/fast.txt"
reject inventory_row_added "$t" 'good.claim: inventory_sha256 is stale'

make_tree "$t"
rm "$t/tests/inventory/fast.txt"
reject inventory_missing "$t" 'cannot read the inventory'

# --- the allowed profile list is the one switch O1 flips -------------------
make_tree "$t"
sed 's/^profile = none$/profile = lynx2/' "$t/docs/claims/good.claim" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/good.claim"
if ! ALLOWED_PROFILES="none lynx2" sh "$t/scripts/check-claims.sh" --root "$t" >"$work/out.txt" 2>&1; then
  cat "$work/out.txt" >&2
  fail allowed_profiles_extended "expected exit 0 when the list names lynx2"
fi
ok allowed_profiles_extended

# --- the template ----------------------------------------------------------
make_tree "$t"
grep -v '^startup_tier = ' "$t/docs/claims/claim.template" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/claim.template"
reject template_missing_key "$t" 'check-claims: claim.template: missing template key: startup_tier'

make_tree "$t"
printf 'profile = <again>\n' >>"$t/docs/claims/claim.template"
reject template_duplicate_key "$t" 'claim.template: duplicate template key: profile'

make_tree "$t"
printf 'surprise = <x>\n' >>"$t/docs/claims/claim.template"
reject template_extra_key "$t" 'claim.template: unexpected template key: surprise'

make_tree "$t"
sed 's/repeatability (R), not accuracy/something else/' "$t/docs/claims/claim.template" >"$t/docs/claims/x" && mv "$t/docs/claims/x" "$t/docs/claims/claim.template"
reject template_lost_repeatability_statement "$t" 'claim.template: missing the statement'

make_tree "$t"
rm "$t/docs/claims/claim.template"
reject template_missing "$t" 'claim.template: cannot read the template'

# --- the empty edge: a claims directory holding only the template ----------
make_tree "$t"
rm "$t/docs/claims/good.claim"
accept template_only "$t"

# --- the printed binding is the SHA-256 of the committed bytes -------------
make_tree "$t"
if command -v sha256sum >/dev/null 2>&1; then
  want=$(sha256sum "$t/tests/inventory/fast.txt")
else
  want=$(shasum -a 256 "$t/tests/inventory/fast.txt")
fi
got=$(sh "$t/scripts/check-claims.sh" --root "$t" --inventory-sha256)
[ "$got" = "${want%% *}" ] || fail inventory_sha256_printed "got $got"
ok inventory_sha256_printed


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

echo "claims_controls: all cases passed"
