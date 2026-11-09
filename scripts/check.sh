#!/bin/sh
# The single local command and the CI gate: scripts/check.sh
#
# Runs its steps in a fixed order. The first failing step stops the run with a
# non-zero exit and a message naming the step. Each step prints its wall time.
set -eu

cd "$(dirname "$0")/.."

total_start=$(date +%s)

# step NAME CMD... : print the step banner, run CMD, print the wall time.
step() {
  step_name=$1
  shift
  printf '== step %s\n' "$step_name"
  step_start=$(date +%s)
  if ! "$@"; then
    printf 'check: step %s failed\n' "$step_name" >&2
    exit 1
  fi
  step_end=$(date +%s)
  printf -- '-- step %s: %ss\n' "$step_name" "$((step_end - step_start))"
}

# Prints the identity of every tool the check depends on.
identities() {
  printf 'cc: %s\n' "$(cc --version | head -n 1)"
  printf 'cmake: %s\n' "$(cmake --version | head -n 1)"
  printf 'ninja: %s\n' "$(ninja --version)"
  printf 'shellcheck: %s\n' "$(shellcheck --version | grep '^version:')"
  printf 'git: %s\n' "$(git --version)"
  if [ ! -f pins.txt ]; then
    printf 'check: pins.txt is missing\n' >&2
    return 1
  fi
  awk '!/^[ \t]*(#|$)/ { printf "pin: %s %s\n", $1, $2 }' pins.txt
  printf 'runner image: ImageOS=%s ImageVersion=%s\n' \
    "${ImageOS:-not-provided}" "${ImageVersion:-not-provided}"
}

# Fetches every pinned source into build/pins/NAME, verified by SHA-256 before it
# is unpacked; a cached extraction is reused (scripts/fetch-pin.sh).
run_pins() {
  if [ ! -f pins.txt ]; then
    printf 'check: pins.txt is missing\n' >&2
    return 1
  fi
  pin_names=$(awk '!/^[ \t]*(#|$)/ { print $1 }' pins.txt)
  if [ -z "$pin_names" ]; then
    printf 'check: pins.txt has no rows\n' >&2
    return 1
  fi
  for pin_name in $pin_names; do
    sh scripts/fetch-pin.sh "$pin_name" "build/pins/$pin_name" || return 1
  done
}

run_shellcheck() {
  # Scripts under scripts/ always; tests/*/*.sh only where they exist.
  set -- scripts/*.sh
  for extra in tests/*/*.sh; do
    if [ -f "$extra" ]; then
      set -- "$@" "$extra"
    fi
  done
  shellcheck -s sh "$@"
}

run_workflow() {
  cmake --workflow --preset check
}

run_junit() {
  cmake -DJUNIT=build/check/junit.xml -DINVENTORY=tests/inventory/fast.txt \
    -DSKIPS=tests/inventory/skips.txt -P tests/cmake/check_junit.cmake
}

step identities identities

# --- insertion point: pins, cc65 and probe steps go here, before shellcheck ---
step pins run_pins

step shellcheck run_shellcheck
step workflow run_workflow

step junit run_junit

# --- insertion point: artifacts and history steps go here, after junit ---

total_end=$(date +%s)
printf 'check: total %ss\n' "$((total_end - total_start))"
