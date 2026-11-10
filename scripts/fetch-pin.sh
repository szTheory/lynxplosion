#!/bin/sh
# Adapted from muhtari (MIT, owner project) at commit 7fd0386.
# Download a pinned source archive by name and verify it before anything uses it.
#
#   scripts/fetch-pin.sh NAME DEST
#
# Fault prevented: an unpinned or tampered download reaching a build. NAME is a
# row of pins.txt ("name version url sha256"; $PINS names another file). The
# download lands in a temporary directory; only when its SHA-256 equals the
# row's is it unpacked (.tar.gz or .tgz, top directory stripped), and DEST is
# never touched before that. On a mismatch DEST is not created.
#
# Before any download the row and DEST are validated: NAME is [A-Za-z0-9._-]+,
# the digest is 64 lowercase hex digits, the URL is https:// (and redirects stay
# https), and DEST is never empty, the root, the working directory or its
# parents, the home directory or its parents.
#
# Two files are written inside DEST, in this order:
#   .lynx-pin-files  a comment header naming the archive SHA-256, then one
#                    "<sha256> <relative path>" line for every regular file of
#                    the verified archive, sorted by path (LC_ALL=C). The rights
#                    scan reads it to derive the allowlist of the pin zone.
#   .lynx-pin        the stamp: the archive SHA-256 as 64 lowercase hex
#                    characters and a newline. Readers trim surrounding
#                    whitespace before comparing; anything else is no stamp.
# An existing DEST is reused only when the stamp equals the row's digest and
# the list is present. Exit status 1 on any failure, 2 on misuse.
set -eu
set -f

if [ $# -ne 2 ]; then
  echo "usage: fetch-pin.sh NAME DEST" >&2
  exit 2
fi
name=$1
dest=$2
pins=${PINS:-$(dirname "$0")/../pins.txt}

# DEST is removed with rm -rf once the archive verifies, so a path that can name
# a directory that matters is refused here, before anything else happens: the
# empty path, the root, the working directory and its parents as spelled, a
# leading tilde (never expanded by this script), and below, as resolved, the
# working directory, any parent of it, the home directory and any parent of it.
case $name in
  '' | *[!A-Za-z0-9._-]*)
    echo "fetch-pin: NAME must match [A-Za-z0-9._-]+" >&2
    exit 2
    ;;
esac
case $dest in
  '' | / | . | .. | ./ | ../ | */. | */.. | */./ | */../ | '~'*)
    echo "fetch-pin: $name: DEST must be a directory path, not '$dest'" >&2
    exit 2
    ;;
esac
if [ -d "$dest" ]; then
  dest_real=$(cd "$dest" && pwd -P)
  here_real=$(pwd -P)
  case $here_real/ in
    "$dest_real"/*)
      echo "fetch-pin: $name: DEST is the working directory or a parent of it" >&2
      exit 2
      ;;
  esac
  if [ -n "${HOME:-}" ] && [ -d "$HOME" ]; then
    home_real=$(cd "$HOME" && pwd -P)
    case $home_real/ in
      "$dest_real"/*)
        echo "fetch-pin: $name: DEST is the home directory or a parent of it" >&2
        exit 2
        ;;
    esac
  fi
fi
if [ ! -f "$pins" ]; then
  echo "fetch-pin: $name: $pins is missing" >&2
  exit 1
fi

row=$(awk -v n="$name" '$1 == n { print; exit }' "$pins")
if [ -z "$row" ]; then
  echo "fetch-pin: $name: not in pins.txt" >&2
  exit 1
fi
# shellcheck disable=SC2086 # split the row into its four fields
set -- $row
if [ $# -ne 4 ]; then
  echo "fetch-pin: $name: the pins row must have four fields: name version url sha256" >&2
  exit 1
fi
url=$3
want=$4
case $want in
  *[!0-9a-f]*)
    echo "fetch-pin: $name: the pinned digest must be 64 lowercase hex digits" >&2
    exit 1
    ;;
esac
if [ "${#want}" -ne 64 ]; then
  echo "fetch-pin: $name: the pinned digest must be 64 lowercase hex digits" >&2
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  hasher="sha256sum"
else
  hasher="shasum -a 256"
fi

# The stamp, trimmed of surrounding whitespace.
have=
if [ -f "$dest/.lynx-pin" ]; then
  have=$(tr -d ' \t\r\n' <"$dest/.lynx-pin")
fi
if [ "$have" = "$want" ] && [ -f "$dest/.lynx-pin-files" ]; then
  echo "fetch-pin: $name: cached (verified at fetch)"
  exit 0
fi

# https only. A file:// URL is accepted solely when LYNX_FETCH_PIN_ALLOW_FILE=1,
# which only the offline control sets; the pins step never does.
proto='=https'
case $url in
  https://?*) ;;
  file://?*)
    if [ "${LYNX_FETCH_PIN_ALLOW_FILE:-}" != 1 ]; then
      echo "fetch-pin: $name: only https URLs are fetched: $url" >&2
      exit 1
    fi
    proto='=https,file'
    ;;
  *)
    echo "fetch-pin: $name: only https URLs are fetched: $url" >&2
    exit 1
    ;;
esac
case $url in
  *.tar.gz | *.tgz) ;;
  *)
    echo "fetch-pin: $name: only .tar.gz and .tgz archives are supported: $url" >&2
    exit 1
    ;;
esac

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
file=$tmp/archive.tar.gz
curl -fsSL --proto "$proto" --proto-redir '=https' --retry 3 -o "$file" -- "$url"
# shellcheck disable=SC2086 # $hasher is a command and its fixed option
got=$($hasher "$file")
got=${got%% *}
if [ "$got" != "$want" ]; then
  echo "fetch-pin: $name: sha256 mismatch: pins.txt has $want, the download is $got; fallback identity: git clone the repository and compare git rev-parse HEAD with the pinned commit" >&2
  exit 1
fi

# Verified: unpack and list in the temporary directory, and move both into
# place together.
x=$tmp/x
mkdir "$x"
tar -xzf "$file" -C "$x" --strip-components=1
if [ -e "$x/.lynx-pin-files" ] || [ -e "$x/.lynx-pin" ]; then
  echo "fetch-pin: $name: the archive holds a file named like a pin record" >&2
  exit 1
fi

# A path with a newline cannot be listed one per line: fail closed.
nl=$(printf '\n_')
nl=${nl%_}
odd=$(cd "$x" && find . -path "*${nl}*" -print | head -n 1)
if [ -n "$odd" ]; then
  echo "fetch-pin: $name: the archive holds a path with a newline" >&2
  exit 1
fi

files=$(cd "$x" && find . -type f | wc -l)
files=$((files + 0))
# shellcheck disable=SC2086 # $hasher is a command and its fixed option
(cd "$x" && find . -type f -exec $hasher {} +) >"$tmp/raw"
sed 's|^\([0-9a-f]\{64\}\)  \./|\1 |' "$tmp/raw" | LC_ALL=C sort -t ' ' -k2 >"$tmp/sorted"
listed=$(grep -c '^[0-9a-f]\{64\} [^ ]' "$tmp/sorted" || true)
if [ "$listed" -ne "$files" ]; then
  echo "fetch-pin: $name: listed $listed of $files files; refusing an unreadable listing" >&2
  exit 1
fi
{
  printf '# sha256 of the verified archive: %s\n' "$want"
  printf '# %s\n' "one line per regular file: sha256, a space, the path relative to the archive top"
  cat "$tmp/sorted"
} >"$x/.lynx-pin-files"

rm -rf "$dest"
mkdir -p "$(dirname "$dest")"
mv "$x" "$dest"
printf '%s\n' "$want" >"$dest/.lynx-pin"
echo "fetch-pin: $name: verified $want, $files files listed"
