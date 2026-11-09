#!/bin/sh
# Rights and firmware scan: no BIOS or firmware image, commercial ROM bytes or
# other unlisted binary content in the source tree or in the build tree.
#
#   scan-binaries.sh --source [--root DIR]
#   scan-binaries.sh --artifacts BUILD_DIR --cmake-binary-dir DIR [--root DIR]
#
# A file is non-text when it holds a control byte (NUL, a C0 control other than
# tab, line feed, form feed and carriage return, or DEL) or is not valid UTF-8.
# NUL alone is never the test: a blob without a NUL byte is still a blob.
#
# The rule is the CONTEXT rule as amended on 2026-11-09 (plan-check B-1 and
# RC-1). "Listed in a manifest" is satisfied in exactly three derived ways and
# nothing else is exempt (no file-name or directory exemptions):
#   1. Fixture manifest: the file's SHA-256 is a `sha256 = ` value in a
#      fixtures/manifests/*.manifest file (tracked and untracked-not-ignored
#      files, and any build path outside the two zones below).
#   2. Pin zone, build/pins/<name>/: the file's path and SHA-256 are on
#      <name>/.lynx-pin-files (written by fetch-pin.sh from the SHA-256-verified
#      archive) or on <name>/.lynx-build-files (written by the cc65 step after a
#      successful build, honoured only when its `pin_sha256` header equals the
#      digest in <name>/.lynx-pin). A listed file passes even with toolchain
#      magic; an added, modified or unlisted file fails.
#   3. Toolchain output: ELF, Mach-O and `ar` objects, archives and
#      executables, and ninja state files (identified by magic bytes) pass only
#      inside the CMake binary dir, outside its scratch/ subtree. The same
#      magic in the source tree, in build/pins/, in scratch/ or in any other
#      build path fails, whether or not the file is manifested.
# A non-text file of exactly 512 bytes fails in every zone with no exemption
# (a Lynx boot ROM, and so any firmware-like image, has that size).
#
# Findings name the rule and the path, never the content. Exit 1 on any
# finding, 2 on a usage error.
set -eu
set -f

LC_ALL=C
export LC_ALL

nl=$(printf '\n_')
nl=${nl%_}
IFS=$nl

usage() {
  printf 'usage: scan-binaries.sh --source [--root DIR]\n' >&2
  printf '       scan-binaries.sh --artifacts BUILD_DIR --cmake-binary-dir DIR [--root DIR]\n' >&2
  exit 2
}

mode=
root=
build=
cbin=
while [ $# -gt 0 ]; do
  case $1 in
    --source) mode=source ;;
    --artifacts)
      [ $# -ge 2 ] || usage
      mode=artifacts
      build=$2
      shift
      ;;
    --cmake-binary-dir)
      [ $# -ge 2 ] || usage
      cbin=$2
      shift
      ;;
    --root)
      [ $# -ge 2 ] || usage
      root=$2
      shift
      ;;
    *) usage ;;
  esac
  shift
done
[ -n "$mode" ] || usage
if [ "$mode" = artifacts ] && { [ -z "$build" ] || [ -z "$cbin" ]; }; then
  usage
fi
if [ -z "$root" ]; then
  root=$(git rev-parse --show-toplevel)
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/lynx-scanbin.XXXXXX")
cleanup() { rm -rf "$tmp"; }
trap cleanup EXIT
trap 'exit 1' INT TERM
: >"$tmp/findings"

found() {
  printf 'scan-binaries: %s: %s\n' "$1" "$2" >>"$tmp/findings"
}

if command -v sha256sum >/dev/null 2>&1; then
  hash_cmd="sha256sum"
else
  hash_cmd="shasum -a 256"
fi

# sha256 of one file (hex only).
sha_of() {
  sha_out=$(IFS=' ' && $hash_cmd "$1") || return 1
  printf '%s\n' "${sha_out%% *}"
}

# True when the file is non-text: a control byte, or not valid UTF-8.
is_nontext() {
  nt_size=$(wc -c <"$1" | tr -d ' ')
  nt_text=$(tr -d '\000-\010\013\016-\037\177' <"$1" | wc -c | tr -d ' ')
  if [ "$nt_size" != "$nt_text" ]; then
    return 0
  fi
  if iconv -f UTF-8 -t UTF-8 <"$1" >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

# True when the file starts with ELF, Mach-O, ar or ninja-deps magic.
toolchain_magic() {
  tm_hex=$(od -An -tx1 -N11 <"$1" | tr -d ' \n')
  case $tm_hex in
    7f454c46* | cffaedfe* | cefaedfe* | cafebabe* | 213c617263683e* | 23206e696e6a6164657073*) return 0 ;;
  esac
  return 1
}

# The SHA-256 values of every fixture manifest, one per line, sorted.
manifest_dir=$root/fixtures/manifests
: >"$tmp/manifested"
if [ -d "$manifest_dir" ]; then
  find "$manifest_dir" -type f -name '*.manifest' -exec \
    sed -n 's/^sha256 = \([0-9a-f]\{64\}\)$/\1/p' {} + |
    LC_ALL=C sort -u >"$tmp/manifested"
fi

# True when the file's SHA-256 is a fixture manifest sha256.
in_manifest() {
  im_sha=$(sha_of "$1") || return 1
  grep -qx "$im_sha" "$tmp/manifested"
}

nfiles=0
n512=0

# The rules shared by a non-pin file. $1 path, $2 zone: source, toolchain, other.
check_plain() {
  cp_path=$1
  cp_zone=$2
  if [ -L "$cp_path" ] || [ ! -f "$cp_path" ]; then
    return 0
  fi
  cp_size=$(wc -c <"$cp_path" | tr -d ' ')
  nfiles=$((nfiles + 1))
  if [ "$cp_size" -eq 512 ]; then
    n512=$((n512 + 1))
  fi
  if [ "$cp_size" -eq 0 ]; then
    return 0
  fi
  if ! is_nontext "$cp_path"; then
    return 0
  fi
  if [ "$cp_size" -eq 512 ]; then
    found 'firmware-like 512-byte binary' "$cp_path"
    return 0
  fi
  if toolchain_magic "$cp_path"; then
    if [ "$cp_zone" = toolchain ]; then
      return 0
    fi
    found 'toolchain output outside build output paths' "$cp_path"
    return 0
  fi
  if in_manifest "$cp_path"; then
    return 0
  fi
  found 'unmanifested binary' "$cp_path"
}

# One pin directory: bulk-hash it and compare (path, sha256) with the lists.
scan_pin_dir() {
  pd=$1
  pd_n=$(find "$pd" -type f | wc -l | tr -d ' ')
  nfiles=$((nfiles + pd_n))
  : >"$tmp/allow"
  if [ -f "$pd/.lynx-pin-files" ]; then
    grep '^[0-9a-f]\{64\} .' "$pd/.lynx-pin-files" >>"$tmp/allow" || true
  fi
  if [ -f "$pd/.lynx-build-files" ] && [ -f "$pd/.lynx-pin" ]; then
    pd_stamp=$(tr -d ' \t\r\n' <"$pd/.lynx-pin")
    pd_head=$(sed -n 1p "$pd/.lynx-build-files")
    if printf '%s\n' "$pd_stamp" | grep -qx '[0-9a-f]\{64\}' &&
      [ "$pd_head" = "# pin_sha256 = $pd_stamp" ]; then
      grep '^[0-9a-f]\{64\} .' "$pd/.lynx-build-files" >>"$tmp/allow" || true
    fi
  fi
  LC_ALL=C sort -u "$tmp/allow" >"$tmp/allow.sorted"
  # shellcheck disable=SC2086 # $hash_cmd is a command and its fixed option
  (IFS=' ' && cd "$pd" && find . -type f -exec $hash_cmd {} +) >"$tmp/raw"
  sed 's|^\([0-9a-f]\{64\}\)  \./|\1 |' "$tmp/raw" | LC_ALL=C sort >"$tmp/present"
  pd_got=$(grep -c '^[0-9a-f]\{64\} [^ ]' "$tmp/present" || true)
  if [ "$pd_got" -ne "$pd_n" ]; then
    found 'unreadable pin-zone listing' "$pd"
    return 0
  fi
  # Rule 1 everywhere: a non-text file of exactly 512 bytes, listed or not.
  find "$pd" -type f -size 512c >"$tmp/pd512"
  while IFS= read -r pd_f; do
    n512=$((n512 + 1))
    if is_nontext "$pd_f"; then
      found 'firmware-like 512-byte binary' "$pd_f"
    fi
  done <"$tmp/pd512"
  # The list rule: a non-text file whose pair is on neither list.
  LC_ALL=C comm -23 "$tmp/present" "$tmp/allow.sorted" >"$tmp/unlisted"
  while IFS= read -r pd_row; do
    pd_rel=${pd_row#* }
    pd_f=$pd/$pd_rel
    pd_size=$(wc -c <"$pd_f" | tr -d ' ')
    if [ "$pd_size" -eq 512 ]; then
      continue
    fi
    if is_nontext "$pd_f"; then
      found 'unlisted pin-zone binary' "$pd_f"
    fi
  done <"$tmp/unlisted"
}

case $mode in
  source)
    if ! git -C "$root" ls-files -z --cached --others --exclude-standard >"$tmp/list0"; then
      printf 'scan-binaries: git ls-files failed\n' >&2
      exit 1
    fi
    # A path holding a newline cannot be listed one per line: fail closed.
    if [ "$(tr -cd '\n' <"$tmp/list0" | wc -c | tr -d ' ')" != 0 ]; then
      printf 'scan-binaries: path with a newline in the file listing\n' >&2
      exit 1
    fi
    tr '\0' '\n' <"$tmp/list0" >"$tmp/list"
    while IFS= read -r sf; do
      check_plain "$root/$sf" source
    done <"$tmp/list"
    ;;
  artifacts)
    if [ ! -d "$build" ]; then
      printf 'scan-binaries: no such build directory: %s\n' "$build" >&2
      exit 1
    fi
    build=$(cd "$build" && pwd -P)
    if [ -d "$cbin" ]; then
      cbin=$(cd "$cbin" && pwd -P)
    fi
    if [ -n "$(find "$build" -path "*${nl}*" -print | head -n 1)" ]; then
      printf 'scan-binaries: path with a newline under %s\n' "$build" >&2
      exit 1
    fi
    find "$build" -type f >"$tmp/all"
    # Pin zone: one bulk pass per build/pins/<name>/.
    if [ -d "$build/pins" ]; then
      find "$build/pins" -mindepth 1 -maxdepth 1 -type d >"$tmp/pindirs"
      while IFS= read -r pin_dir; do
        scan_pin_dir "$pin_dir"
      done <"$tmp/pindirs"
    fi
    # Everything else, by zone.
    while IFS= read -r af; do
      case $af in
        "$build"/pins/*/*) continue ;;
        "$cbin"/scratch/*) az=other ;;
        "$cbin"/*) az=toolchain ;;
        *) az=other ;;
      esac
      check_plain "$af" "$az"
    done <"$tmp/all"
    ;;
esac

if [ -s "$tmp/findings" ]; then
  LC_ALL=C sort -u "$tmp/findings" >&2
  exit 1
fi
printf 'scan-binaries: %s: %s files, %s of exactly 512 bytes\n' "$mode" "$nfiles" "$n512"
