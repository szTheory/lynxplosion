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
#   files independence wrapper_sha256, the complete input-probe provenance group,
#   and the complete A/V provenance group (optional)
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

# A fixture path (including a manifest) must be represented by bytes owned by
# the real fixture tree. Reject every symlink without traversing it before the
# manifest finder or validator can open any candidate path. The command-line
# fixtures component needs its own check because find does not descend through
# a symlink used as its starting point on every supported implementation.
fixture_root=$root/fixtures
fixture_links=$work/fixture-links
: >"$fixture_links"
if [ -L "$fixture_root" ]; then
  printf '%s\n' fixtures >"$fixture_links"
elif [ -d "$fixture_root" ]; then
  if ! find "$fixture_root" -type l -print >"$fixture_links" 2>/dev/null; then
    printf 'check-manifests: fixture tree could not be scanned for symbolic links\n' \
      >>"$findings"
  fi
fi
while IFS= read -r link; do
  case $link in
    "$root"/*) link=${link#"$root"/} ;;
  esac
  printf 'check-manifests: symbolic link in fixture tree: %s\n' "$link" >>"$findings"
done <"$fixture_links"
if [ -s "$findings" ]; then
  sort "$findings" >&2
  printf 'check-manifests: %s finding(s)\n' "$(wc -l <"$findings" | tr -d ' ')" >&2
  exit 1
fi

canonical_fixture_root=
if [ -d "$fixture_root" ]; then
  canonical_fixture_root=$(CDPATH='' cd -P "$fixture_root" 2>/dev/null && pwd -P) || {
    echo 'check-manifests: fixture root cannot be resolved' >&2
    exit 1
  }
fi

# Check each path component without following it. The tree scan above protects
# the validation reads; this per-entry check keeps the regular-file test safe
# if a path changes between that scan and validation.
has_symlink_component() {
  remaining=$1
  case $remaining in
    "$root"/*) remaining=${remaining#"$root"/} ;;
    *) return 1 ;;
  esac
  current=$root
  while [ -n "$remaining" ]; do
    case $remaining in
      */*) component=${remaining%%/*}; remaining=${remaining#*/} ;;
      *) component=$remaining; remaining= ;;
    esac
    current=$current/$component
    if [ -L "$current" ]; then return 0; fi
  done
  return 1
}

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
    --optional "files independence wrapper_sha256 config_sha256 cc65_pin_sha256 cc65_build_recipe legacy_source legacy_source_sha256 source_built_legacy_tracer_guest_sha256 source_built_legacy_tracer_guest_bytes source_built_guest_sha256_lefthand_0 source_built_guest_bytes_lefthand_0 source_built_guest_sha256_lefthand_1 source_built_guest_bytes_lefthand_1 av_config_sha256 av_guest_sha256 av_guest_bytes" "$root/$manifest" 2>&1 |
    sed "s|$root/||" |
    sed 's|^validate-kv: |check-manifests: |' >>"$findings"

  # A/V build provenance is independent of the input-probe group: its
  # config identity and source-built guest identity must travel together.
  awk -v m="$manifest" '
    {
      i = index($0, " = ")
      if (i > 0) {
        key = substr($0, 1, i - 1)
        value = substr($0, i + 3)
        if (key == "av_config_sha256" || key == "av_guest_sha256" || key == "av_guest_bytes") {
          seen[key] = value
          count++
        }
      }
    }
    END {
      if (count > 0) {
        split("av_config_sha256 av_guest_sha256 av_guest_bytes", keys, " ")
        for (i = 1; i <= 3; i++) {
          if (!(keys[i] in seen)) {
            printf "check-manifests: %s: incomplete A/V provenance group: missing key: %s\n", m, keys[i]
          }
        }
        if ("av_guest_bytes" in seen && seen["av_guest_bytes"] !~ /^[1-9][0-9]*$/) {
          printf "check-manifests: %s: av_guest_bytes must be a positive decimal integer\n", m
        }
      }
    }
  ' "$root/$manifest" >>"$findings"

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
    entry_path=$root/$entry
    if has_symlink_component "$entry_path"; then
      printf 'check-manifests: %s: files entry contains a symbolic link: %s\n' \
        "$manifest" "$entry" >>"$findings"
    elif [ ! -f "$entry_path" ]; then
      printf 'check-manifests: %s: files entry missing: %s\n' \
        "$manifest" "$entry" >>"$findings"
    else
      entry_parent=$(dirname "$entry_path")
      canonical_entry_parent=$(CDPATH='' cd -P "$entry_parent" 2>/dev/null && pwd -P)
      resolved_entry=$canonical_entry_parent/${entry_path##*/}
      case $resolved_entry in
        "$canonical_fixture_root"/*)
          printf '%s\n' "$entry" >>"$work/covered"
          ;;
        *)
          printf 'check-manifests: %s: files entry is outside canonical fixtures root: %s\n' \
            "$manifest" "$entry" >>"$findings"
          ;;
      esac
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
find "$fixture_root" -type f 2>/dev/null |
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
