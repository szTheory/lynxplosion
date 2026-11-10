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

# Build identity (cmake/BuildId.cmake): git=<rev>[+dirty] cc=<id>-<version>
# sys=<name>-<processor>, baked into the core as read-only data. It is recomputed
# whenever the checkout's HEAD, branch ref or index changes, not only at the first
# configure.
set(LYNX_BUILD_ID_TEMPLATE "${PROJECT_SOURCE_DIR}/core/build_id.c.in")
include(cmake/BuildId.cmake)

# Every source of the core. Later plans append to this list.
set(LYNX_CORE_SOURCES core/cpu.c core/cpu_profile.c core/machine.c core/sha256.c core/snapshot.c "${PROJECT_BINARY_DIR}/generated/build_id.c")

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
