#!/bin/sh
# Strict validator for `key = value` files: run manifests, fixture manifests
# and claims.
#
#   sh scripts/validate-kv.sh --required "k1 k2 ..." [--optional "k3 ..."]
#                             [--no-host-data] FILE...
#
# Rules, applied to every line (no comments, no blank lines, no merging):
#   - the line is "key = value": one " = " separator, split at the first one;
#   - the key is lowercase letters, digits and underscores, starting with a letter;
#   - the value is not empty;
#   - the key is one of the required or optional keys, and appears once;
#   - every required key is present;
#   - "sha256", any key ending "_sha256" and "input_digest" hold 64 lowercase
#     hex digits (length and character class; no regex interval counts);
#   - startup_tier is A; evidence_tier is one of T1 T2 T3 T4 T5 T6 T7 R;
#   - with --no-host-data, a value must not start with "/" or "~", contain a
#     home-directory path, or contain a YYYY-MM-DD date.
# Every finding is printed as "validate-kv: <file>: ..." on standard error.
# Exit status: 0 clean, 1 findings, 2 usage error.
#
# Fault prevented: a manifest that is accepted although it merges a repeated
# key, omits the build identity or leaks a host path or a date into evidence.
LC_ALL=C
export LC_ALL

usage() {
  printf 'usage: validate-kv.sh --required "k1 k2 ..." [--optional "k3 ..."] [--no-host-data] FILE...\n' >&2
  exit 2
}

required=""
optional=""
nohost=0
have_required=0
while [ $# -gt 0 ]; do
  case "$1" in
    --required)
      [ $# -ge 2 ] || usage
      required=$2
      have_required=1
      shift 2
      ;;
    --optional)
      [ $# -ge 2 ] || usage
      optional=$2
      shift 2
      ;;
    --no-host-data)
      nohost=1
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      usage
      ;;
    *)
      break
      ;;
  esac
done
[ "$have_required" -eq 1 ] || usage
[ $# -ge 1 ] || usage

status=0
for file in "$@"; do
  if [ ! -f "$file" ] || [ ! -r "$file" ]; then
    printf 'validate-kv: %s: cannot read file\n' "$file" >&2
    status=1
    continue
  fi
  # The values travel through the environment so awk never interprets escapes.
  KV_FILE=$file KV_REQUIRED=$required KV_OPTIONAL=$optional KV_NOHOST=$nohost \
    awk '
    function bad(n, key, msg) {
      if (n > 0 && key != "") {
        printf "validate-kv: %s: line %d: %s: %s\n", file, n, key, msg > "/dev/stderr"
      } else if (n > 0) {
        printf "validate-kv: %s: line %d: %s\n", file, n, msg > "/dev/stderr"
      } else {
        printf "validate-kv: %s: %s\n", file, msg > "/dev/stderr"
      }
      failed = 1
    }
    function is_hash_key(k) {
      return k == "sha256" || k == "input_digest" || (length(k) > 7 && substr(k, length(k) - 6) == "_sha256")
    }
    BEGIN {
      file = ENVIRON["KV_FILE"]
      nohost = ENVIRON["KV_NOHOST"] == "1"
      nreq = split(ENVIRON["KV_REQUIRED"], reqlist, " ")
      nopt = split(ENVIRON["KV_OPTIONAL"], optlist, " ")
      for (i = 1; i <= nreq; i++) { known[reqlist[i]] = 1 }
      for (i = 1; i <= nopt; i++) { known[optlist[i]] = 1 }
      failed = 0
    }
    {
      sep = index($0, " = ")
      if (sep == 0) {
        bad(NR, "", "malformed line")
        next
      }
      key = substr($0, 1, sep - 1)
      value = substr($0, sep + 3)
      if (key !~ /^[a-z][a-z0-9_]*$/) {
        bad(NR, "", "malformed line")
        next
      }
      if (index(value, " = ") != 0) {
        bad(NR, key, "malformed line")
        next
      }
      if (value == "") {
        bad(NR, key, "empty value")
        next
      }
      if (!(key in known)) {
        bad(NR, key, "unknown key")
      }
      if (key in seen) {
        bad(NR, key, "duplicate key")
      }
      seen[key] = 1
      if (is_hash_key(key) && (length(value) != 64 || value ~ /[^0-9a-f]/)) {
        bad(NR, key, "bad sha256")
      }
      if (key == "startup_tier" && value != "A") {
        bad(NR, key, "bad startup_tier")
      }
      if (key == "evidence_tier" && value !~ /^(T[1-7]|R)$/) {
        bad(NR, key, "bad evidence_tier")
      }
      if (nohost) {
        first = substr(value, 1, 1)
        if (first == "/" || first == "~" || index(value, "/Users/") != 0 ||
            index(value, "/home/") != 0 || index(value, "\\Users\\") != 0 ||
            value ~ /[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]/) {
          bad(NR, key, "host data")
        }
      }
    }
    END {
      for (i = 1; i <= nreq; i++) {
        if (!(reqlist[i] in seen)) {
          bad(0, "", "missing key: " reqlist[i])
        }
      }
      exit failed
    }
  ' "$file" || status=1
done
exit "$status"
