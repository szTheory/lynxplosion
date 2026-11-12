#!/bin/sh
# Fixture manifest validation and fixture coverage.
#
#   sh scripts/check-manifests.sh [--root DIR]
#
# DIR defaults to the repository root. Every DIR/fixtures/manifests/*.manifest
# is a strict "key = value" file (scripts/validate-kv.sh) with the keys
#   source revision recipe sha256 licence notices allowed_use startup_tier
#   profile evidence_tier          (required; startup_tier is A or none, because a
#                                   software-model vector injects state and is not tier A)
#   files independence wrapper_sha256 and the complete input-probe provenance group (optional)
# and the checks below hold across the whole set:
#   - no two manifests carry the same sha256 (an ambiguous identity);
#   - every "files" entry is a relative path under fixtures/ and exists;
#   - every file under fixtures/ other than the manifests themselves (the
#     *.manifest files directly in fixtures/manifests) is named by some
#     manifest's "files" value; any other file there is uncovered.
# Manifests are visited in LC_ALL=C sorted order, so findings are stable, and
# validation does not depend on the key order inside a file.
#
# Every finding is printed as "check-manifests: ..." on standard error.
# Exit status: 0 clean, 1 findings, 2 usage error.
#
# Fault prevented: a fixture whose identity, source, licence or allowed use is
# unrecorded, or two records claiming one identity.
LC_ALL=C
export LC_ALL
set -u
set -f

root=$(dirname "$0")/..
case ${1-} in
  --root)
    if [ $# -ne 2 ]; then
      echo "usage: check-manifests.sh [--root DIR]" >&2
      exit 2
    fi
    root=$2
    ;;
  "") ;;
  *)
    echo "usage: check-manifests.sh [--root DIR]" >&2
    exit 2
    ;;
esac
if [ ! -d "$root" ]; then
  echo "check-manifests: $root is not a directory" >&2
  exit 2
fi
root=$(cd "$root" && pwd -P)
validator=$(dirname "$0")/validate-kv.sh

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-manifests.XXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'exit 1' INT TERM
findings=$work/findings
: >"$findings"
find "$root/fixtures/manifests" -maxdepth 1 -type f -name '*.manifest' 2>/dev/null |
  sed "s|^$root/||" | sort >"$work/manifests"

if [ ! -s "$work/manifests" ]; then
  echo "check-manifests: no manifests under fixtures/manifests" >>"$findings"
fi

: >"$work/shas"
: >"$work/covered"
while IFS= read -r manifest; do
  # Key validation: findings arrive on standard error, one per line.
  sh "$validator" --startup-tiers "A none" \
    --required "source revision recipe sha256 licence notices allowed_use startup_tier profile evidence_tier" \
    --optional "files independence wrapper_sha256 config_sha256 ca65_sha256 ld65_sha256 legacy_source legacy_source_sha256 source_built_legacy_tracer_guest_sha256 source_built_legacy_tracer_guest_bytes source_built_guest_sha256_lefthand_0 source_built_guest_bytes_lefthand_0 source_built_guest_sha256_lefthand_1 source_built_guest_bytes_lefthand_1" "$root/$manifest" 2>&1 |
    sed "s|$root/||" |
    sed 's|^validate-kv: |check-manifests: |' >>"$findings"

  # The identity, looked up by key so the order inside the file is free.
  awk -v m="$manifest" '
    { i = index($0, " = ") }
    i > 0 && substr($0, 1, i - 1) == "sha256" { print substr($0, i + 3) " " m }
  ' "$root/$manifest" >>"$work/shas"

  # The files this manifest covers, one per line.
  awk '
    { i = index($0, " = ") }
    i > 0 && substr($0, 1, i - 1) == "files" {
      n = split(substr($0, i + 3), parts, " ")
      for (k = 1; k <= n; k++) { print parts[k] }
    }
  ' "$root/$manifest" >"$work/entries"
  while IFS= read -r entry; do
    case $entry in
      fixtures/manifests/* | /* | *..*)
        printf 'check-manifests: %s: files entry not a fixture path: %s\n' \
          "$manifest" "$entry" >>"$findings"
        continue
        ;;
      fixtures/*) ;;
      *)
        printf 'check-manifests: %s: files entry not a fixture path: %s\n' \
          "$manifest" "$entry" >>"$findings"
        continue
        ;;
    esac
    if [ ! -f "$root/$entry" ]; then
      printf 'check-manifests: %s: files entry missing: %s\n' \
        "$manifest" "$entry" >>"$findings"
    else
      printf '%s\n' "$entry" >>"$work/covered"
    fi
  done <"$work/entries"
done <"$work/manifests"

# One identity, one manifest.
sort -k1,1 -k2,2 "$work/shas" |
  awk '
    $1 == prev { printf "check-manifests: duplicate sha256 in %s and %s\n", first, $2 }
    $1 != prev { prev = $1; first = $2 }
  ' >>"$findings"

# Every fixture is covered.
sort -u "$work/covered" >"$work/covered.sorted"
# Only the manifests themselves, at the top of fixtures/manifests, are exempt:
# any other file there, or in a directory below it, must be covered like the rest.
find "$root/fixtures" -type f 2>/dev/null |
  sed "s|^$root/||" | grep -v -x 'fixtures/manifests/[^/][^/]*\.manifest' | sort >"$work/present"
comm -23 "$work/present" "$work/covered.sorted" |
  sed 's|^|check-manifests: uncovered fixture: |' >>"$findings"

count=$(wc -l <"$findings")
count=$((count + 0))
if [ "$count" -gt 0 ]; then
  sort "$findings" >&2
  printf 'check-manifests: %s finding(s)\n' "$count" >&2
  exit 1
fi
printf 'check-manifests: %s manifest(s), %s fixture file(s) covered\n' \
  "$(wc -l <"$work/manifests" | tr -d ' ')" "$(wc -l <"$work/present" | tr -d ' ')"
