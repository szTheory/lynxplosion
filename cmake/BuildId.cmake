# Build identity, baked into a source file as read-only data:
#   git=<rev>[+dirty] cc=<id>-<version> sys=<name>-<processor>
# "nogit" replaces <rev> when there is no git checkout (a tarball build). A
# tracked-file change appends +dirty. No path, date or clock time may enter:
# the identity is recorded in public evidence and compared by snapshots.
#
# Inputs: LYNX_BUILD_ID_TEMPLATE, a configure_file template that uses
# @LYNX_BUILD_ID@. Output: LYNX_BUILD_ID and the generated file
# ${PROJECT_BINARY_DIR}/generated/build_id.c.
#
# The revision and the dirty flag come from git at configure time, so every file
# that changes when HEAD moves or the index changes is a configure dependency: a
# later commit followed by a plain build reconfigures and the identity follows.
# The paths come from `git rev-parse --git-path`, which is right for a worktree
# (where .git is a file) too. A change to a tracked file that is neither staged
# nor committed does not touch any of them, so the +dirty flag then updates at the
# next reconfigure only (scripts/check.sh always reconfigures).
set(lynx_git_rev nogit)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE lynx_rev_out RESULT_VARIABLE lynx_rev_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(lynx_rev_rc EQUAL 0 AND NOT lynx_rev_out STREQUAL "")
    set(lynx_git_rev "${lynx_rev_out}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" status --porcelain --untracked-files=no
      OUTPUT_VARIABLE lynx_dirty_out RESULT_VARIABLE lynx_dirty_rc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT lynx_dirty_rc EQUAL 0 OR NOT lynx_dirty_out STREQUAL "")
      string(APPEND lynx_git_rev "+dirty")
    endif()
    # The files whose change means a new revision or a new dirty state.
    set(lynx_git_paths HEAD index packed-refs)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" symbolic-ref -q HEAD
      OUTPUT_VARIABLE lynx_head_ref RESULT_VARIABLE lynx_head_ref_rc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(lynx_head_ref_rc EQUAL 0 AND NOT lynx_head_ref STREQUAL "")
      list(APPEND lynx_git_paths "${lynx_head_ref}")
    endif()
    foreach(lynx_git_path IN LISTS lynx_git_paths)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --git-path "${lynx_git_path}"
        OUTPUT_VARIABLE lynx_git_file RESULT_VARIABLE lynx_git_file_rc
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(lynx_git_file_rc EQUAL 0 AND NOT lynx_git_file STREQUAL "")
        # git prints the path relative to its -C directory unless it is absolute.
        get_filename_component(lynx_git_file "${lynx_git_file}" ABSOLUTE
          BASE_DIR "${PROJECT_SOURCE_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${lynx_git_file}")
      endif()
    endforeach()
  endif()
endif()
set(LYNX_BUILD_ID
  "git=${lynx_git_rev} cc=${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION} sys=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
string(LENGTH "${LYNX_BUILD_ID}" lynx_build_id_length)
# The snapshot header reserves 128 bytes for this string (with its NUL).
if(lynx_build_id_length GREATER 127)
  message(FATAL_ERROR "build identity is ${lynx_build_id_length} characters, limit is 127: ${LYNX_BUILD_ID}")
endif()
configure_file("${LYNX_BUILD_ID_TEMPLATE}" "${PROJECT_BINARY_DIR}/generated/build_id.c" @ONLY)
message(STATUS "build identity: ${LYNX_BUILD_ID}")
