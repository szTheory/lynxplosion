cmake_minimum_required(VERSION 3.25)
project(lynxplosion LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(LYNX_TESTS "Build and register the tests" OFF)

# Warnings are errors. Applied per target.
function(lynx_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wshadow -Werror)
endfunction()

# AddressSanitizer and UndefinedBehaviorSanitizer that stop the program on a
# finding: without -fno-sanitize-recover the default UBSan prints and exits 0.
# Set per target, never through CMAKE_C_FLAGS, so an uninstrumented twin of the
# core can be built later for symbol checks.
function(lynx_sanitize target)
  target_compile_options(${target} PRIVATE
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()

# Build identity, baked into the core as read-only data at configure time:
#   git=<rev>[+dirty] cc=<id>-<version> sys=<name>-<processor>
# "nogit" replaces <rev> when there is no git checkout (a tarball build). A
# tracked-file change appends +dirty. No path, date or clock time may enter:
# the identity is recorded in public evidence and compared by snapshots.
set(lynx_git_rev nogit)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE lynx_rev_out RESULT_VARIABLE lynx_rev_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(lynx_rev_rc EQUAL 0 AND NOT lynx_rev_out STREQUAL "")
    set(lynx_git_rev "${lynx_rev_out}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" status --porcelain --untracked-files=no
      OUTPUT_VARIABLE lynx_dirty_out RESULT_VARIABLE lynx_dirty_rc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT lynx_dirty_rc EQUAL 0 OR NOT lynx_dirty_out STREQUAL "")
      string(APPEND lynx_git_rev "+dirty")
    endif()
  endif()
endif()
set(LYNX_BUILD_ID
  "git=${lynx_git_rev} cc=${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION} sys=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
string(LENGTH "${LYNX_BUILD_ID}" lynx_build_id_length)
# The snapshot header reserves 128 bytes for this string (with its NUL).
if(lynx_build_id_length GREATER 127)
  message(FATAL_ERROR "build identity is ${lynx_build_id_length} characters, limit is 127: ${LYNX_BUILD_ID}")
endif()
configure_file(core/build_id.c.in "${PROJECT_BINARY_DIR}/generated/build_id.c" @ONLY)
message(STATUS "build identity: ${LYNX_BUILD_ID}")

# Every source of the core. Later plans append to this list.
set(LYNX_CORE_SOURCES core/machine.c core/sha256.c core/snapshot.c "${PROJECT_BINARY_DIR}/generated/build_id.c")

add_library(lynx_core STATIC ${LYNX_CORE_SOURCES})
target_include_directories(lynx_core PUBLIC include PRIVATE core)
# No absolute host path reaches an object file.
target_compile_options(lynx_core PRIVATE
  -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.
  -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)
lynx_warnings(lynx_core)
lynx_sanitize(lynx_core)
# The sanitizer runtime must be linked by whatever links the core.
target_link_options(lynx_core PUBLIC -fsanitize=address,undefined)

# Uninstrumented twin of the core, for the link-surface checks. Sanitized
# objects carry writable data and runtime symbols, so every nm check reads this
# library and never lynx_core. It is built from the same sources, with the
# flags a release build would use, and never with a sanitizer.
add_library(lynx_core_plain STATIC ${LYNX_CORE_SOURCES})
target_include_directories(lynx_core_plain PUBLIC include PRIVATE core)
target_compile_options(lynx_core_plain PRIVATE
  -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector
  -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.
  -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)
lynx_warnings(lynx_core_plain)
set_target_properties(lynx_core_plain PROPERTIES
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON)

# The public header compiled alone: warnings only, no sanitizer. It links the
# plain twin because its function-pointer table names every public function.
add_executable(lynx-test-header tests/header/header.c)
target_link_libraries(lynx-test-header PRIVATE lynx_core_plain)
lynx_warnings(lynx-test-header)
set_target_properties(lynx-test-header PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

# The runner is host side: it may use stdio, stdlib and POSIX mkdir, so it is
# defined here and never inside core/. _POSIX_C_SOURCE is set on the runner
# targets only, because CMAKE_C_EXTENSIONS is OFF and hides POSIX declarations.
add_library(lynx_bundle STATIC runner/bundle.c)
# core/ is private: the bundle writer needs sha256.h and nothing else from it.
target_include_directories(lynx_bundle PUBLIC runner PRIVATE core)
target_link_libraries(lynx_bundle PUBLIC lynx_core)
target_compile_definitions(lynx_bundle PRIVATE _POSIX_C_SOURCE=200809L)
lynx_warnings(lynx_bundle)
lynx_sanitize(lynx_bundle)

add_executable(lynx-run runner/main.c)
target_link_libraries(lynx-run PRIVATE lynx_bundle)
target_compile_definitions(lynx-run PRIVATE _POSIX_C_SOURCE=200809L)
lynx_warnings(lynx-run)
lynx_sanitize(lynx-run)
set_target_properties(lynx-run PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/runner)

if(LYNX_TESTS)
  enable_testing()
  include(cmake/Tests.cmake)
  include(tests/tests.cmake)
endif()
