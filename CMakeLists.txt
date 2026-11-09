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

# Every source of the core. Later plans append to this list.
set(LYNX_CORE_SOURCES core/machine.c core/sha256.c)

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

if(LYNX_TESTS)
  enable_testing()
  include(cmake/Tests.cmake)
  include(tests/tests.cmake)
endif()
