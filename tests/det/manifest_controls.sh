#!/bin/sh
# Negative and positive controls for scripts/validate-kv.sh, run as
#   sh tests/det/manifest_controls.sh WORKDIR
#
# Fault prevented: a validator that accepts a manifest it is meant to reject.
# Every faulty manifest is built here from parts at run time; nothing faulty is
# committed, and the home-directory value is assembled from pieces so this file
# holds no literal home path.
set -eu

[ $# -eq 1 ] || {
  printf 'usage: manifest_controls.sh WORKDIR\n' >&2
  exit 2
}
work=$1
here=$(dirname "$0")
validator="$here/../../scripts/validate-kv.sh"
required="profile startup_tier content_sha256 content_load_addr input_digest tick_budget ticks_consumed stop_reason core_build ram_policy ram_seed evidence_tier"

rm -rf "$work"
mkdir -p "$work"

hex64=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef

# The valid manifest, in the documented key order.
base() {
  printf 'profile = none\n'
  printf 'startup_tier = A\n'
  printf 'content_sha256 = %s\n' "$hex64"
  printf 'content_load_addr = 0x0200\n'
  printf 'input_digest = %s\n' "$hex64"
  printf 'tick_budget = 4096\n'
  printf 'ticks_consumed = 4096\n'
  printf 'stop_reason = budget\n'
  printf 'core_build = git=abc123 cc=gcc-13 sys=Linux-x86_64\n'
  printf 'ram_policy = splitmix64\n'
  printf 'ram_seed = 0x0123456789abcdef\n'
  printf 'evidence_tier = R\n'
}

# reject CASE MESSAGE [validator flags...]: the manifest in $work/CASE.kv must
# be refused with exit 1 and a line "validate-kv: <file>: ...MESSAGE...".
reject() {
  name=$1
  message=$2
  shift 2
  file="$work/$name.kv"
  status=0
  sh "$validator" --required "$required" "$@" "$file" >"$work/$name.out" 2>&1 || status=$?
  if [ "$status" -ne 1 ]; then
    printf 'manifest_controls: %s: expected exit 1, got %s\n' "$name" "$status" >&2
    cat "$work/$name.out" >&2
    exit 1
  fi
  if ! grep -F "validate-kv: $file: " "$work/$name.out" | grep -qF "$message"; then
    printf 'manifest_controls: %s: expected a finding containing "%s", got\n' "$name" "$message" >&2
    cat "$work/$name.out" >&2
    exit 1
  fi
  printf 'manifest_controls: %s: ok\n' "$name"
}

accept() {
  name=$1
  shift
  file="$work/$name.kv"
  status=0
  sh "$validator" --required "$required" "$@" "$file" >"$work/$name.out" 2>&1 || status=$?
  if [ "$status" -ne 0 ]; then
    printf 'manifest_controls: %s: expected exit 0, got %s\n' "$name" "$status" >&2
    cat "$work/$name.out" >&2
    exit 1
  fi
  printf 'manifest_controls: %s: ok\n' "$name"
}

# Positive control: the valid manifest, with and without the host-data check.
base >"$work/valid.kv"
accept valid --no-host-data
accept valid

base | grep -v '^core_build = ' >"$work/missing_key.kv"
reject missing_key 'missing key: core_build'

{ base; printf 'surprise_key = 1\n'; } >"$work/unknown_key.kv"
reject unknown_key 'surprise_key: unknown key'

{ base; printf 'tick_budget = 1\n'; } >"$work/duplicate_key.kv"
reject duplicate_key 'tick_budget: duplicate key'

base | sed 's/^tick_budget = 4096$/tick_budget = 4096\ntick_budget = 4096/' >"$work/duplicate_adjacent.kv"
reject duplicate_adjacent 'tick_budget: duplicate key'

base | sed 's/^ram_policy = .*$/ram_policy = /' >"$work/empty_value.kv"
reject empty_value 'ram_policy: empty value'

{ base; printf 'no separator on this line\n'; } >"$work/malformed.kv"
reject malformed 'malformed line'

base | sed "s/^content_sha256 = .*\$/content_sha256 = $(printf '%s' "$hex64" | cut -c 1-63)/" >"$work/short_sha.kv"
reject short_sha 'content_sha256: bad sha256'

upper=$(printf '%s' "$hex64" | tr 'a-f' 'A-F')
base | sed "s/^content_sha256 = .*\$/content_sha256 = $upper/" >"$work/upper_sha.kv"
reject upper_sha 'content_sha256: bad sha256'

base | sed 's/^startup_tier = A$/startup_tier = B/' >"$work/startup_tier.kv"
reject startup_tier 'startup_tier: bad startup_tier'

# startup_tier vocabulary: a run manifest keeps the default (A only), so "none"
# is refused there; a caller that passes --startup-tiers "A none" (fixture
# manifests and claims) accepts it but still refuses anything outside the list.
base | sed 's/^startup_tier = A$/startup_tier = none/' >"$work/tier_none_run.kv"
reject tier_none_run 'startup_tier: bad startup_tier'
cp "$work/tier_none_run.kv" "$work/tier_none_listed.kv"
accept tier_none_listed --startup-tiers "A none"
cp "$work/tier_none_run.kv" "$work/tier_none_a_only.kv"
reject tier_none_a_only 'startup_tier: bad startup_tier' --startup-tiers "A"
cp "$work/startup_tier.kv" "$work/tier_b_listed.kv"
reject tier_b_listed 'startup_tier: bad startup_tier' --startup-tiers "A none"
base >"$work/tier_a_listed.kv"
accept tier_a_listed --startup-tiers "A none"

# A malformed tier list is a usage error (exit 2), never an empty vocabulary.
for bad_list in "" "1x" "a-b"; do
  status=0
  sh "$validator" --required "$required" --startup-tiers "$bad_list" "$work/valid.kv" >"$work/tier_list.out" 2>&1 || status=$?
  if [ "$status" -ne 2 ]; then
    printf 'manifest_controls: tier_list_usage: list "%s": expected exit 2, got %s\n' "$bad_list" "$status" >&2
    exit 1
  fi
done
printf 'manifest_controls: tier_list_usage: ok\n'

# WR-05: a glob character in the tier list is a usage error. Unquoted, the list
# would be expanded against the current directory, so the directory below holds
# letters-only file names that an expansion would turn into "valid" tier words
# (the shell and the awk validator would then disagree about the list).
abs_validator=$(cd "$(dirname "$validator")" && pwd -P)/validate-kv.sh
abs_work=$(cd "$work" && pwd -P)
mkdir -p "$work/globdir"
: >"$work/globdir/A"
: >"$work/globdir/Abc"
for bad_list in "A*" "*" "A?" "[A]" "A [x"; do
  status=0
  (cd "$work/globdir" && sh "$abs_validator" --required "$required" --startup-tiers "$bad_list" "$abs_work/valid.kv") \
    >"$work/tier_glob.out" 2>&1 || status=$?
  if [ "$status" -ne 2 ]; then
    printf 'manifest_controls: tier_list_glob: list "%s": expected exit 2, got %s\n' "$bad_list" "$status" >&2
    exit 1
  fi
done
printf 'manifest_controls: tier_list_glob: ok\n'

# IN-09: an empty --required list is a usage error, so an empty file cannot pass
# a check that requires nothing.
: >"$work/empty.kv"
for bad_required in "" " "; do
  status=0
  sh "$validator" --required "$bad_required" "$work/empty.kv" >"$work/required_empty.out" 2>&1 || status=$?
  if [ "$status" -ne 2 ]; then
    printf 'manifest_controls: required_empty: list "%s": expected exit 2, got %s\n' "$bad_required" "$status" >&2
    exit 1
  fi
done
printf 'manifest_controls: required_empty: ok\n'

base | sed 's/^evidence_tier = R$/evidence_tier = T9/' >"$work/evidence_tier.kv"
reject evidence_tier 'evidence_tier: bad evidence_tier'

# Host data, each value assembled from parts.
root_part=/opt
base | sed "s#^core_build = .*\$#core_build = ${root_part}/toolchain/cc#" >"$work/host_absolute.kv"
reject host_absolute 'core_build: host data' --no-host-data

tilde=\~
base | sed "s#^core_build = .*\$#core_build = ${tilde}/work/cc#" >"$work/host_tilde.kv"
reject host_tilde 'core_build: host data' --no-host-data

seg_a=/Us
seg_b=ers/
base | sed "s#^core_build = .*\$#core_build = cc=${seg_a}${seg_b}someone/cc#" >"$work/host_home_mac.kv"
reject host_home_mac 'core_build: host data' --no-host-data

seg_c=/ho
seg_d=me/
base | sed "s#^core_build = .*\$#core_build = cc=${seg_c}${seg_d}someone/cc#" >"$work/host_home_linux.kv"
reject host_home_linux 'core_build: host data' --no-host-data

year=2026
base | sed "s#^core_build = .*\$#core_build = built ${year}-11-09#" >"$work/host_date.kv"
reject host_date 'core_build: host data' --no-host-data

# A URL is not host data and is accepted without --no-host-data.
base | sed 's#^core_build = .*$#core_build = https://example.org/x#' >"$work/url.kv"
accept url
