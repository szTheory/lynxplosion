#!/bin/sh
# The build identity follows the checkout without a manual reconfigure, run as
#   tests/api/build_id_refresh.sh REPO_ROOT
#
# Fault prevented: a build identity computed once at the first configure. A commit
# followed by a plain build would keep the old revision and dirty flag in
# lynx_build_id(), which goes into run manifests and snapshot compatibility. The
# real cmake/BuildId.cmake is included from a tiny project in a temporary git
# repository outside the checkout (an ordinary checkout and a linked worktree,
# where .git is a file), and the identity is read after each plain build. Ninja
# is the project's generator and compares file times finely; a make that rounds
# times to the second can miss a rebuild inside one second.
set -eu

if [ $# -ne 1 ]; then
  echo "usage: build_id_refresh.sh REPO_ROOT" >&2
  exit 2
fi
repo=$(cd "$1" && pwd -P)

work=$(mktemp -d "${TMPDIR:-/tmp}/lynx-buildid.XXXXXX")
work=$(cd "$work" && pwd -P)
trap 'rm -rf "$work"' EXIT
trap 'exit 1' INT TERM
case $work/ in
  "$repo"/*)
    echo "build_id_refresh: the work directory $work is inside the repository" >&2
    exit 1
    ;;
esac

fail() {
  echo "build_id_refresh: $1: FAILED: $2" >&2
  exit 1
}
ok() {
  echo "build_id_refresh: $1: ok"
}

proj=$work/proj
mkdir -p "$proj"
# shellcheck disable=SC2016 # CMake variables are written literally
{
  printf 'cmake_minimum_required(VERSION 3.25)\n'
  printf 'project(idcheck LANGUAGES C)\n'
  printf 'set(LYNX_BUILD_ID_TEMPLATE "${PROJECT_SOURCE_DIR}/id.c.in")\n'
  printf 'include("%s/cmake/BuildId.cmake")\n' "$repo"
  printf 'add_executable(idprint main.c "${PROJECT_BINARY_DIR}/generated/build_id.c")\n'
} >"$proj/CMakeLists.txt"
printf 'const char lynx__build_id[] = "@LYNX_BUILD_ID@";\n' >"$proj/id.c.in"
{
  printf '#include <stdio.h>\n'
  printf 'extern const char lynx__build_id[];\n'
  printf 'int main(void) { puts(lynx__build_id); return 0; }\n'
} >"$proj/main.c"
printf 'one\n' >"$proj/tracked.txt"

git_in() { # DIR git-args...: a commit identity that is not a person's
  gi_dir=$1
  shift
  git -C "$gi_dir" -c user.name=control -c user.email=control@users.noreply.github.com \
    -c commit.gpgsign=false "$@"
}

git -C "$proj" init -q .
git_in "$proj" add CMakeLists.txt id.c.in main.c tracked.txt
git_in "$proj" commit -q -m one

# build_id BUILD_DIR: a plain build (no configure step of its own), then the identity.
build_id() {
  cmake --build "$1" >"$work/build.log" 2>&1 || {
    cat "$work/build.log" >&2
    fail build "the plain build failed in $1"
  }
  "$1/idprint"
}

# want_rev LABEL BUILD_DIR GIT_DIR [+dirty]: the identity names the checkout's HEAD.
want_rev() {
  wr_rev=$(git -C "$3" rev-parse HEAD)
  wr_id=$(build_id "$2")
  case $wr_id in
    "git=$wr_rev${4-} "*) ok "$1" ;;
    *) fail "$1" "expected git=$wr_rev${4-} in: $wr_id" ;;
  esac
}

cmake -G Ninja -S "$proj" -B "$work/bld" >"$work/configure.log" 2>&1 || {
  cat "$work/configure.log" >&2
  fail configure "the first configure failed"
}
want_rev first_build "$work/bld" "$proj"

# A commit that changes only the branch ref (an empty commit), then a plain build.
git_in "$proj" commit -q --allow-empty -m two
want_rev commit_then_build "$work/bld" "$proj"

# A staged change to a tracked file, then a plain build: the dirty flag appears.
printf 'two\n' >"$proj/tracked.txt"
git_in "$proj" add tracked.txt
want_rev staged_then_build "$work/bld" "$proj" +dirty

# Committing it clears the flag again.
git_in "$proj" commit -q -m three
want_rev commit_clears_dirty "$work/bld" "$proj"

# A linked worktree, where .git is a file naming the real git directory.
git_in "$proj" worktree add -q -b other "$work/wt"
[ -f "$work/wt/.git" ] || fail worktree "the linked worktree has no .git file"
cmake -G Ninja -S "$work/wt" -B "$work/bld-wt" >"$work/configure.log" 2>&1 || {
  cat "$work/configure.log" >&2
  fail worktree "the worktree configure failed"
}
want_rev worktree_first "$work/bld-wt" "$work/wt"
git_in "$work/wt" commit -q --allow-empty -m four
want_rev worktree_commit_then_build "$work/bld-wt" "$work/wt"
