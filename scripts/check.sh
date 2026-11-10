#!/bin/sh
# The single local command and the CI gate: scripts/check.sh
#
# Runs its steps in a fixed order. The first failing step stops the run with a
# non-zero exit and a message naming the step. Each step prints its wall time.
set -eu

cd "$(dirname "$0")/.."

total_start=$(date +%s)

# --- step machinery (tests/gates/check_controls.sh runs this block as written) ---
# A step runs bare, never under `if`, `!` or `||`: set -e is ignored inside any
# function whose status is tested, which would let every command but the last of
# a step fail silently. A failing step therefore ends the shell, and the EXIT
# trap names the step that was running.
current_step=
on_exit() {
  exit_rc=$?
  if [ "$exit_rc" -ne 0 ] && [ -n "$current_step" ]; then
    printf 'check: step %s failed\n' "$current_step" >&2
  fi
}
trap on_exit EXIT
trap 'exit 1' INT TERM

# step NAME CMD... : print the step banner, run CMD, print the wall time.
step() {
  step_name=$1
  shift
  current_step=$step_name
  printf '== step %s\n' "$step_name"
  step_start=$(date +%s)
  "$@"
  step_end=$(date +%s)
  current_step=
  printf -- '-- step %s: %ss\n' "$step_name" "$((step_end - step_start))"
}
# --- end of step machinery ---

# Prints the identity of every tool the check depends on. A missing tool fails
# the step; it never prints an empty identity.
identities() {
  id_cc=$(cc --version) || return 1
  id_cmake=$(cmake --version) || return 1
  id_ninja=$(ninja --version) || return 1
  id_shellcheck=$(shellcheck --version) || return 1
  id_git=$(git --version) || return 1
  id_shellcheck=$(printf '%s\n' "$id_shellcheck" | grep '^version:') || return 1
  printf 'cc: %s\n' "$(printf '%s\n' "$id_cc" | head -n 1)"
  printf 'cmake: %s\n' "$(printf '%s\n' "$id_cmake" | head -n 1)"
  printf 'ninja: %s\n' "$id_ninja"
  printf 'shellcheck: %s\n' "$id_shellcheck"
  printf 'git: %s\n' "$id_git"
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

# --- cc65: the pinned guest toolchain, built in two make runs -----------------
# The recipe is two sequential invocations, never one: libsrc/Makefile looks for
# the freshly built tools when it is parsed, so "make bin lib" races.
CC65_DIR=build/pins/cc65
CC65_RECIPE=1
CC65_FRESH=0
CC65_BUILD_SECONDS=0
CC65_ROW=

# sha256 of one file (hex only).
sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha_out=$(sha256sum "$1")
  else
    sha_out=$(shasum -a 256 "$1")
  fi
  printf '%s\n' "${sha_out%% *}"
}

# Writes "<sha256> <path>" for every regular file under CC65_DIR except the
# .lynx-* metadata files, sorted by line (LC_ALL=C), to the file named by $1.
cc65_hash_tree() {
  hash_nl=$(printf '\n_')
  hash_nl=${hash_nl%_}
  hash_odd=$(cd "$CC65_DIR" && find . -path "*${hash_nl}*" -print | head -n 1)
  if [ -n "$hash_odd" ]; then
    printf 'check: cc65: a path with a newline is under %s\n' "$CC65_DIR" >&2
    return 1
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    hash_cmd="sha256sum"
  else
    hash_cmd="shasum -a 256"
  fi
  # shellcheck disable=SC2086 # $hash_cmd is a command and its fixed option
  (cd "$CC65_DIR" && find . -type f ! -path ./.lynx-pin ! -path ./.lynx-pin-files \
    ! -path ./.lynx-build-files ! -path ./.lynx-build -exec $hash_cmd {} +) |
    sed 's|^\([0-9a-f]\{64\}\)  \./|\1 |' | LC_ALL=C sort >"$1"
  # A listing that lost a file (an unreadable file, a failed hash) is refused.
  hash_want=$(cd "$CC65_DIR" && find . -type f ! -path ./.lynx-pin ! -path ./.lynx-pin-files \
    ! -path ./.lynx-build-files ! -path ./.lynx-build | wc -l)
  hash_got=$(wc -l <"$1")
  if [ "$((hash_want + 0))" -ne "$((hash_got + 0))" ]; then
    printf 'check: cc65: hashed %s of %s files under %s\n' "$hash_got" "$hash_want" "$CC65_DIR" >&2
    return 1
  fi
}

# True when the archive list, the build list and the stamp all agree on the pin
# digest and recipe version, and the built tools are present.
cc65_cache_ok() {
  [ -f "$CC65_DIR/.lynx-pin" ] || return 1
  [ "$(tr -d ' \t\r\n' <"$CC65_DIR/.lynx-pin")" = "$CC65_ROW" ] || return 1
  [ -f "$CC65_DIR/.lynx-pin-files" ] || return 1
  [ -f "$CC65_DIR/.lynx-build-files" ] || return 1
  [ -f "$CC65_DIR/.lynx-build" ] || return 1
  [ "$(sed -n 1p "$CC65_DIR/.lynx-build-files")" = "# pin_sha256 = $CC65_ROW" ] || return 1
  [ "$(sed -n 2p "$CC65_DIR/.lynx-build-files")" = "# recipe = $CC65_RECIPE" ] || return 1
  grep -qx "pin_sha256 = $CC65_ROW" "$CC65_DIR/.lynx-build" || return 1
  grep -qx "recipe = $CC65_RECIPE" "$CC65_DIR/.lynx-build" || return 1
  grep -q '^build_seconds = [0-9][0-9]*$' "$CC65_DIR/.lynx-build" || return 1
  for cc65_tool in cc65 ca65 ld65 cl65; do
    [ -x "$CC65_DIR/bin/$cc65_tool" ] || return 1
  done
  [ -f "$CC65_DIR/lib/lynx.lib" ] || return 1
  return 0
}

# True when the extraction holds exactly the files of the verified archive, each
# with its listed hash: nothing extra, nothing changed, nothing missing.
cc65_pristine() {
  [ -f "$CC65_DIR/.lynx-pin-files" ] || return 1
  pristine_tmp=$(mktemp -d)
  grep -v '^#' "$CC65_DIR/.lynx-pin-files" | LC_ALL=C sort >"$pristine_tmp/listed"
  if ! cc65_hash_tree "$pristine_tmp/present"; then
    rm -rf "$pristine_tmp"
    return 1
  fi
  if cmp -s "$pristine_tmp/listed" "$pristine_tmp/present"; then
    pristine_rc=0
  else
    pristine_rc=1
  fi
  rm -rf "$pristine_tmp"
  return "$pristine_rc"
}

run_cc65() {
  if [ ! -f pins.txt ]; then
    printf 'check: pins.txt is missing\n' >&2
    return 1
  fi
  CC65_ROW=$(awk '$1 == "cc65" { print $4; exit }' pins.txt)
  if [ -z "$CC65_ROW" ]; then
    printf 'check: pins.txt has no cc65 row\n' >&2
    return 1
  fi
  CC65_FRESH=0
  if cc65_cache_ok; then
    CC65_BUILD_SECONDS=$(sed -n 's/^build_seconds = //p' "$CC65_DIR/.lynx-build")
    printf 'cc65: cached build (built in %ss)\n' "$CC65_BUILD_SECONDS"
    return 0
  fi

  if ! cc65_pristine; then
    printf 'cc65: the extraction is not pristine against its archive list; fetching it again\n'
    rm -rf "$CC65_DIR"
    sh scripts/fetch-pin.sh cc65 "$CC65_DIR" || return 1
    if ! cc65_pristine; then
      printf 'check: cc65: a fresh extraction is still not pristine\n' >&2
      return 1
    fi
  fi
  rm -f "$CC65_DIR/.lynx-build-files" "$CC65_DIR/.lynx-build"

  CC65_HOME=$PWD/$CC65_DIR
  export CC65_HOME
  jobs=$(getconf _NPROCESSORS_ONLN) || jobs=
  case $jobs in
    '' | *[!0-9]* | 0)
      printf 'cc65: getconf gave no usable processor count; building with -j1\n'
      jobs=1
      ;;
  esac
  # The cc65 Makefile embeds "git rev-parse --short HEAD" of whatever repository
  # encloses the tree (here: this one), so the tool bytes would change with every
  # commit of this repository. BUILD_ID is fixed to the pinned cc65 commit.
  cc65_commit=$(awk '$1 == "cc65" { print substr($2, 1, 7); exit }' pins.txt)
  cc65_build_id="Git $cc65_commit"
  build_start=$(date +%s)
  make -C build/pins/cc65 -j"$jobs" BUILD_ID="$cc65_build_id" bin || return 1
  bin_end=$(date +%s)
  printf 'cc65: make bin %ss\n' "$((bin_end - build_start))"
  make -C build/pins/cc65 -j"$jobs" lib TARGETS=lynx || return 1
  build_end=$(date +%s)
  printf 'cc65: make lib TARGETS=lynx %ss\n' "$((build_end - bin_end))"
  CC65_BUILD_SECONDS=$((build_end - build_start))
  CC65_FRESH=1
  printf 'cc65: built in %ss\n' "$CC65_BUILD_SECONDS"
}

# Builds the project-authored probe with the pinned cc65 in a clean environment
# and prints the identity of the tools and of the linked output. lynx.lib itself
# is never compared: it embeds build-time data; the tools and the linked output
# are reproducible.
run_probe() {
  if [ ! -x "$CC65_DIR/bin/cl65" ]; then
    printf 'check: probe: %s/bin/cl65 is missing\n' "$CC65_DIR" >&2
    return 1
  fi
  CC65_HOME=$PWD/$CC65_DIR
  export CC65_HOME
  mkdir -p build/toolchain
  rm -f build/toolchain/probe.c build/toolchain/probe.lnx build/toolchain/probe.sym \
    build/toolchain/probe.o build/toolchain/probe.s
  cp fixtures/toolchain/probe.c build/toolchain/probe.c
  (
    cd build/toolchain &&
      env -i PATH="$CC65_HOME/bin:/usr/bin:/bin" CC65_HOME="$CC65_HOME" \
        cl65 -t lynx -Ln probe.sym -o probe.lnx probe.c
  ) || return 1
  rm -f build/toolchain/probe.o build/toolchain/probe.s
  if [ ! -f build/toolchain/probe.lnx ]; then
    printf 'check: probe: cl65 produced no build/toolchain/probe.lnx\n' >&2
    return 1
  fi
  for probe_tool in cc65 ca65 ld65 cl65; do
    probe_tool_sha=$(sha_of "$CC65_DIR/bin/$probe_tool") || return 1
    printf 'cc65 tool sha256 %s %s\n' "$probe_tool" "$probe_tool_sha"
  done
  probe_sha=$(sha_of build/toolchain/probe.lnx) || return 1
  printf 'probe sha256 = %s\n' "$probe_sha"
  probe_size=$(wc -c <build/toolchain/probe.lnx | tr -d ' ')
  printf 'probe size = %s\n' "$probe_size"
  if [ "$probe_size" -eq 512 ]; then
    printf 'check: probe: the probe is exactly 512 bytes; change the g_results size in fixtures/toolchain/probe.c\n' >&2
    return 1
  fi
  if [ "$CC65_FRESH" -eq 1 ]; then
    cc65_finalize || return 1
  fi
}

# After both make runs and the probe succeeded: write the build list (every file
# the recipe added or changed, relative to the archive list), then the stamp.
cc65_finalize() {
  fin_start=$(date +%s)
  fin_tmp=$(mktemp -d)
  grep -v '^#' "$CC65_DIR/.lynx-pin-files" | LC_ALL=C sort >"$fin_tmp/listed"
  if ! cc65_hash_tree "$fin_tmp/present"; then
    rm -rf "$fin_tmp"
    return 1
  fi
  LC_ALL=C comm -13 "$fin_tmp/listed" "$fin_tmp/present" |
    LC_ALL=C sort -t ' ' -k2 >"$fin_tmp/added"
  fin_count=$(wc -l <"$fin_tmp/added" | tr -d ' ')
  {
    printf '# pin_sha256 = %s\n' "$CC65_ROW"
    printf '# recipe = %s\n' "$CC65_RECIPE"
    cat "$fin_tmp/added"
  } >"$CC65_DIR/.lynx-build-files"
  rm -rf "$fin_tmp"
  {
    printf 'pin_sha256 = %s\n' "$CC65_ROW"
    printf 'recipe = %s\n' "$CC65_RECIPE"
    printf 'build_seconds = %s\n' "$CC65_BUILD_SECONDS"
  } >"$CC65_DIR/.lynx-build"
  fin_end=$(date +%s)
  printf 'cc65: build list %s files (written in %ss)\n' "$fin_count" "$((fin_end - fin_start))"
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

# Assembles the two CPU fixture programs with the pinned ca65/ld65 and the
# committed linker config. The compare against the committed hex runs in the
# fixtures.cpu_asm gate (tests/cmake/cpu_fixtures.cmake); this step only builds
# build/toolchain/<name>.bin and prints its SHA-256.
run_cpu_fixtures() {
  if [ ! -x "$CC65_DIR/bin/ca65" ] || [ ! -x "$CC65_DIR/bin/ld65" ]; then
    printf 'check: cpu-fixtures: %s/bin/ca65 or ld65 is missing\n' "$CC65_DIR" >&2
    return 1
  fi
  CC65_HOME=$PWD/$CC65_DIR
  export CC65_HOME
  mkdir -p build/toolchain
  cpu_fx_cfg=build/toolchain/raw-0200.cfg
  for cpu_fx in sum-trap churn-loop; do
    rm -f "build/toolchain/$cpu_fx.bin" "build/toolchain/$cpu_fx.o" "build/toolchain/$cpu_fx.s"
    cp "fixtures/cpu/$cpu_fx.s" "build/toolchain/$cpu_fx.s"
    cp fixtures/cpu/raw-0200.cfg "$cpu_fx_cfg"
    (
      cd build/toolchain &&
        env -i PATH="$CC65_HOME/bin:/usr/bin:/bin" CC65_HOME="$CC65_HOME" \
          ca65 --cpu 65SC02 -o "$cpu_fx.o" "$cpu_fx.s" &&
        env -i PATH="$CC65_HOME/bin:/usr/bin:/bin" CC65_HOME="$CC65_HOME" \
          ld65 -C raw-0200.cfg -o "$cpu_fx.bin" "$cpu_fx.o"
    ) || return 1
    rm -f "build/toolchain/$cpu_fx.o" "build/toolchain/$cpu_fx.s"
    if [ ! -f "build/toolchain/$cpu_fx.bin" ]; then
      printf 'check: cpu-fixtures: no build/toolchain/%s.bin was produced\n' "$cpu_fx" >&2
      return 1
    fi
    cpu_fx_sha=$(sha_of "build/toolchain/$cpu_fx.bin") || return 1
    printf 'cpu fixture sha256 %s %s\n' "$cpu_fx" "$cpu_fx_sha"
  done
  rm -f "$cpu_fx_cfg"
}

# Scans the build tree for rights-bearing or unlisted binaries (scripts/scan-binaries.sh).
# Runs only after the workflow and JUnit steps, never while tests may be writing
# files. Finder metadata files are removed first, visibly: they are the one
# stray file macOS adds to build/ and nothing else is exempt from the scan.
run_artifacts() {
  if [ ! -d build ]; then
    printf 'check: artifacts: build/ is missing\n' >&2
    return 1
  fi
  find build -type f -name .DS_Store -print | while IFS= read -r finder_file; do
    rm -f "$finder_file"
    printf 'artifacts: removed Finder metadata %s\n' "$finder_file"
  done
  sh scripts/scan-binaries.sh --artifacts build --cmake-binary-dir build/check
}

# The commit history carries no home path, non-noreply identity or credential.
# A single-commit history is skipped, visibly.
run_history() {
  history_count=$(git rev-list --count HEAD)
  if [ "$history_count" -gt 1 ]; then
    sh scripts/hygiene.sh --history
  else
    printf 'history: single commit, skipped (visible)\n'
  fi
}

run_junit() {
  cmake -DJUNIT=build/check/junit.xml -DINVENTORY=tests/inventory/fast.txt \
    -DSKIPS=tests/inventory/skips.txt -P tests/cmake/check_junit.cmake
}

step identities identities

# --- insertion point: nothing else goes between pins/cc65/probe and shellcheck ---
step pins run_pins
step cc65 run_cc65
step probe run_probe
step cpu-fixtures run_cpu_fixtures

step shellcheck run_shellcheck
step workflow run_workflow

step junit run_junit

step artifacts run_artifacts
# --- insertion point: nothing else goes between junit and history ---
step history run_history

total_end=$(date +%s)
printf 'check: total %ss\n' "$((total_end - total_start))"
