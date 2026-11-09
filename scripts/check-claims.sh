#!/bin/sh
# Claim validator: every docs/claims/*.claim is a valid claim bound to the exact
# test inventory, and docs/claims/claim.template carries exactly the claim keys.
#
#   sh scripts/check-claims.sh [--root DIR]     check the claims under DIR (default: repository)
#   sh scripts/check-claims.sh --inventory-sha256 [--root DIR]
#                                               print the value a claim must hold for
#                                               inventory_sha256, then exit
#
# Rules:
#   - the template has exactly the keys claim profile content_sha256 startup_tier
#     evidence_tier inventory_sha256 evidence, each once, and says that a same-core
#     digest is repeatability (R), not accuracy;
#   - each claim passes scripts/validate-kv.sh with those keys (no repeated key,
#     no empty value, content_sha256 and inventory_sha256 are 64 hex digits,
#     startup_tier A, evidence_tier T1..T7 or R);
#   - profile is one of ALLOWED_PROFILES (environment, default "none");
#   - inventory_sha256 equals the SHA-256 of ROOT/tests/inventory/fast.txt, computed
#     over its exact bytes, so a change to the inventory (a test added or removed,
#     or the order or format touched) makes every existing claim stale.
# Findings are printed as "check-claims: <file>: ..." on standard error.
# Exit status: 0 clean, 1 findings, 2 usage error.
#
# Fault prevented: a claim that no longer describes the test set that backed it,
# or that names a hardware profile before the owner has chosen one.
LC_ALL=C
export LC_ALL

KEYS="claim profile content_sha256 startup_tier evidence_tier inventory_sha256 evidence"
# O1 (the baseline model) is open, so the only profile a claim may name is "none".
# When O1 is decided, the owner extends this list (or sets ALLOWED_PROFILES).
ALLOWED_PROFILES=${ALLOWED_PROFILES:-none}

usage() {
  printf 'usage: check-claims.sh [--inventory-sha256] [--root DIR]\n' >&2
  exit 2
}

here=$(cd "$(dirname "$0")" && pwd -P)
root=$(cd "$here/.." && pwd -P)
print_sha=0
while [ $# -gt 0 ]; do
  case "$1" in
    --root)
      [ $# -ge 2 ] || usage
      root=$(cd "$2" 2>/dev/null && pwd -P) || {
        printf 'check-claims: --root %s: not a directory\n' "$2" >&2
        exit 2
      }
      shift 2
      ;;
    --inventory-sha256)
      print_sha=1
      shift
      ;;
    *)
      usage
      ;;
  esac
done

# sha256 of one file (hex only).
sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha_out=$(sha256sum "$1")
  else
    sha_out=$(shasum -a 256 "$1")
  fi
  printf '%s\n' "${sha_out%% *}"
}

inventory=$root/tests/inventory/fast.txt
if [ ! -f "$inventory" ]; then
  printf 'check-claims: tests/inventory/fast.txt: cannot read the inventory\n' >&2
  exit 1
fi
inventory_sha=$(sha_of "$inventory")
if [ "$print_sha" -eq 1 ]; then
  printf '%s\n' "$inventory_sha"
  exit 0
fi

status=0
claims_dir=$root/docs/claims

# The template: exact key set, once each, plus the repeatability statement.
template=$claims_dir/claim.template
if [ ! -f "$template" ]; then
  printf 'check-claims: claim.template: cannot read the template\n' >&2
  status=1
else
  if ! KV_KEYS=$KEYS awk '
    BEGIN {
      n = split(ENVIRON["KV_KEYS"], list, " ")
      for (i = 1; i <= n; i++) { want[list[i]] = 1 }
      bad = 0
    }
    /^[ \t]*(#|$)/ { next }
    {
      sep = index($0, " = ")
      key = (sep == 0) ? $0 : substr($0, 1, sep - 1)
      if (sep == 0) {
        printf "check-claims: claim.template: malformed line: %s\n", $0 > "/dev/stderr"; bad = 1; next
      }
      if (!(key in want)) {
        printf "check-claims: claim.template: unexpected template key: %s\n", key > "/dev/stderr"; bad = 1
      }
      if (key in seen) {
        printf "check-claims: claim.template: duplicate template key: %s\n", key > "/dev/stderr"; bad = 1
      }
      seen[key] = 1
    }
    END {
      for (i = 1; i <= n; i++) {
        if (!(list[i] in seen)) {
          printf "check-claims: claim.template: missing template key: %s\n", list[i] > "/dev/stderr"; bad = 1
        }
      }
      exit bad
    }
  ' "$template"; then
    status=1
  fi
  if ! grep -q 'repeatability (R), not accuracy' "$template"; then
    printf 'check-claims: claim.template: missing the statement that a same-core digest is repeatability (R), not accuracy\n' >&2
    status=1
  fi
fi

# The claims. A directory holding only the template has no claim to check.
for claim in "$claims_dir"/*.claim; do
  [ -e "$claim" ] || continue
  name=$(basename "$claim")
  if ! sh "$here/validate-kv.sh" --required "$KEYS" "$claim"; then
    status=1
    continue
  fi
  profile=$(awk 'index($0, "profile = ") == 1 { print substr($0, 11) }' "$claim")
  case " $ALLOWED_PROFILES " in
    *" $profile "*) ;;
    *)
      printf 'check-claims: %s: profile %s not allowed while O1 is open\n' "$name" "$profile" >&2
      status=1
      ;;
  esac
  claimed=$(awk 'index($0, "inventory_sha256 = ") == 1 { print substr($0, 20) }' "$claim")
  if [ "$claimed" != "$inventory_sha" ]; then
    printf 'check-claims: %s: inventory_sha256 is stale (re-run the evidence and update inventory_sha256; sh scripts/check-claims.sh --inventory-sha256 prints the current value)\n' "$name" >&2
    status=1
  fi
done
exit "$status"
