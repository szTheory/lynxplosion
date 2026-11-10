# runner.cli: the runner's defaults, usage errors and refusals.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DWORK=... -DTRAP_HEX=... -DTRAP_TICKS=...
#        -DTRAP_STOP=... -P runner_cli.cmake
#
# Fault prevented: a runner that crashes, hangs or writes on bad input, or that
# silently defaults to a seeded RAM policy, would corrupt the evidence.
foreach(var RUNNER CONTENT WORK TRAP_HEX TRAP_TICKS TRAP_STOP)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "runner_cli: -D${var}= is required and must not be empty")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# Malformed content is written at test time; the text is not a hex pair.
file(WRITE "${WORK}/bad.hex" "zz\n")

# expect(CASE CODE ERR_REGEX ARGS...) runs the runner and checks the exit code
# exactly and, when ERR_REGEX is not empty, that stderr matches it.
function(expect case code err_regex)
  execute_process(COMMAND "${RUNNER}" ${ARGN}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc STREQUAL "${code}")
    message(FATAL_ERROR "runner_cli: ${case}: expected exit ${code}, got ${rc}: ${err}")
  endif()
  if(NOT err_regex STREQUAL "" AND NOT err MATCHES "${err_regex}")
    message(FATAL_ERROR "runner_cli: ${case}: expected stderr matching '${err_regex}', got '${err}'")
  endif()
  if(NOT code EQUAL 0 AND NOT out STREQUAL "")
    message(FATAL_ERROR "runner_cli: ${case}: expected no stdout on failure, got '${out}'")
  endif()
  message(STATUS "runner_cli: ${case}: ok")
endfunction()

set(good --content "${CONTENT}" --load-addr 0200 --ticks 16 --out "${WORK}/unused")

expect("no arguments" 2 "usage:")
expect("unknown flag" 2 "usage:" --bogus)
expect("missing value" 2 "usage:" --content)
expect("missing required flag" 2 "usage:"
  --content "${CONTENT}" --load-addr 0200 --out "${WORK}/unused")
expect("repeated flag" 2 "usage:" ${good} --ticks 8)
expect("ticks with a letter" 2 "usage:"
  --content "${CONTENT}" --load-addr 0200 --ticks 12x --out "${WORK}/unused")
expect("ticks above uint64" 2 "usage:"
  --content "${CONTENT}" --load-addr 0200 --ticks 18446744073709551616 --out "${WORK}/unused")
expect("ticks negative" 2 "usage:"
  --content "${CONTENT}" --load-addr 0200 --ticks -1 --out "${WORK}/unused")
expect("bad load address" 2 "usage:"
  --content "${CONTENT}" --load-addr 02zz --ticks 16 --out "${WORK}/unused")
expect("bad ram policy" 2 "usage:" ${good} --ram-policy random)
expect("bad ram seed" 2 "usage:" ${good} --ram-seed 12345678901234567)
expect("unknown profile" 2 "usage:" ${good} --profile foo)
expect("profile with a capital" 2 "usage:" ${good} --profile LYNX1)
expect("empty profile" 2 "usage:" ${good} --profile "")
expect("repeated profile" 2 "usage:" ${good} --profile none --profile lynx1)
expect("missing content file" 1 "^lynx-run: "
  --content "${WORK}/absent.hex" --load-addr 0200 --ticks 16 --out "${WORK}/unused")
expect("malformed content" 1 "^lynx-run: "
  --content "${WORK}/bad.hex" --load-addr 0200 --ticks 16 --out "${WORK}/unused")
expect("load past the address space" 1 "^lynx-run: "
  --content "${CONTENT}" --load-addr fff0 --ticks 16 --out "${WORK}/unused")
expect("load address out of range" 1 "^lynx-run: "
  --content "${CONTENT}" --load-addr 10000 --ticks 16 --out "${WORK}/unused")

# A refusal leaves no output directory behind.
if(EXISTS "${WORK}/unused")
  message(FATAL_ERROR "runner_cli: refused runs created the output directory")
endif()
message(STATUS "runner_cli: refusals leave no output: ok")

# The full tick range is accepted. The largest budget needs content that stops
# at once (TRAP_HEX, the bit-op slot 0x07 that profile none stops before), or
# the run would simulate for geological time.
file(WRITE "${WORK}/halt.hex" "${TRAP_HEX}\n")
expect("zero ticks" 0 ""
  --content "${CONTENT}" --load-addr 0200 --ticks 0 --out "${WORK}/zero")
expect("largest tick budget" 0 ""
  --content "${WORK}/halt.hex" --load-addr 0200 --ticks 18446744073709551615 --out "${WORK}/max")
file(STRINGS "${WORK}/max/run.manifest" max_lines REGEX "^(tick_budget|ticks_consumed|stop_reason) = ")
set(want_max "tick_budget = 18446744073709551615;ticks_consumed = ${TRAP_TICKS};stop_reason = ${TRAP_STOP}")
if(NOT max_lines STREQUAL want_max)
  message(FATAL_ERROR "runner_cli: largest tick budget: manifest says '${max_lines}', expected '${want_max}'")
endif()
message(STATUS "runner_cli: largest tick budget recorded, ${TRAP_STOP} reported: ok")

# Default RAM policy is fixed 0x00 with seed 0 (CONTEXT: the CLI default).
expect("default ram policy" 0 ""
  --content "${CONTENT}" --load-addr 0200 --ticks 16 --out "${WORK}/default")
file(READ "${WORK}/default/run.manifest" manifest)
foreach(line "ram_policy = zero" "ram_seed = 0x0000000000000000")
  string(FIND "${manifest}" "\n${line}\n" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "runner_cli: default ram policy: manifest lacks '${line}':\n${manifest}")
  endif()
endforeach()
message(STATUS "runner_cli: default ram policy recorded as zero and seed 0: ok")

# A bundle written into a directory that holds an earlier bundle.
# A write that fails part way must not leave the earlier digests.txt beside new
# files: obs.regs.hex is made a non-empty directory so its file cannot be created.
expect("first bundle" 0 ""
  --content "${CONTENT}" --load-addr 0200 --ticks 16 --out "${WORK}/stale")
if(NOT EXISTS "${WORK}/stale/digests.txt")
  message(FATAL_ERROR "runner_cli: first bundle: digests.txt was not written")
endif()
file(REMOVE "${WORK}/stale/obs.regs.hex")
file(MAKE_DIRECTORY "${WORK}/stale/obs.regs.hex")
file(WRITE "${WORK}/stale/obs.regs.hex/blocker" "x\n")
expect("failed rewrite" 1 "^lynx-run: cannot write the artifact bundle"
  --content "${CONTENT}" --load-addr 0200 --ticks 32 --out "${WORK}/stale")
if(EXISTS "${WORK}/stale/digests.txt")
  message(FATAL_ERROR "runner_cli: failed rewrite: a stale digests.txt is left beside new files")
endif()
message(STATUS "runner_cli: a failed rewrite leaves no stale digests.txt: ok")

# A symlink planted at a bundle file name is replaced, never followed: the file
# it points at keeps its content.
if(NOT CMAKE_HOST_WIN32)
  file(MAKE_DIRECTORY "${WORK}/linked")
  file(WRITE "${WORK}/victim.txt" "victim content\n")
  file(CREATE_LINK "${WORK}/victim.txt" "${WORK}/linked/obs.ram.hex" SYMBOLIC)
  file(CREATE_LINK "${WORK}/victim.txt" "${WORK}/linked/digests.txt" SYMBOLIC)
  expect("symlinked output names" 0 ""
    --content "${CONTENT}" --load-addr 0200 --ticks 16 --out "${WORK}/linked")
  file(READ "${WORK}/victim.txt" victim)
  if(NOT victim STREQUAL "victim content\n")
    message(FATAL_ERROR "runner_cli: symlinked output names: the link target was overwritten")
  endif()
  foreach(name obs.ram.hex digests.txt)
    if(IS_SYMLINK "${WORK}/linked/${name}")
      message(FATAL_ERROR "runner_cli: symlinked output names: ${name} is still a symlink")
    endif()
  endforeach()
  message(STATUS "runner_cli: symlinks at bundle names are replaced, not followed: ok")
endif()

# The profile is an explicit host choice: none without the flag, and the
# manifest line comes from the core's own name for the id (neither Lynx profile
# is ever a default). The content has no bit-operation opcode, so all three
# profiles run it.
foreach(profile none lynx1 lynx2)
  expect("profile ${profile}" 0 ""
    --content "${CONTENT}" --load-addr 0200 --ticks 16 --profile ${profile}
    --out "${WORK}/profile-${profile}")
  file(STRINGS "${WORK}/profile-${profile}/run.manifest" profile_lines REGEX "^profile = ")
  if(NOT profile_lines STREQUAL "profile = ${profile}")
    message(FATAL_ERROR "runner_cli: profile ${profile}: manifest says '${profile_lines}'")
  endif()
endforeach()
file(STRINGS "${WORK}/default/run.manifest" default_profile REGEX "^profile = ")
if(NOT default_profile STREQUAL "profile = none")
  message(FATAL_ERROR "runner_cli: no flag: manifest says '${default_profile}', expected 'profile = none'")
endif()
message(STATUS "runner_cli: --profile selects the manifest profile, none by default: ok")

# An UNDEFINED stop writes the bundle, exits 0 and tells the user which opcode
# and PC stopped it and how to choose a profile. The content is the one-byte
# bit-operation image (0x07 at 0x0200). Under a Lynx profile the same content
# is defined and no note is printed.
expect("undefined note" 0 "undefined opcode .*07.* at PC .*0200.*--profile"
  --content "${WORK}/halt.hex" --load-addr 0200 --ticks 100 --out "${WORK}/undef")
file(STRINGS "${WORK}/undef/run.manifest" undef_lines REGEX "^(profile|stop_reason|ticks_consumed) = ")
if(NOT undef_lines STREQUAL "profile = none;ticks_consumed = 0;stop_reason = undefined")
  message(FATAL_ERROR "runner_cli: undefined note: manifest says '${undef_lines}'")
endif()
if(NOT EXISTS "${WORK}/undef/digests.txt")
  message(FATAL_ERROR "runner_cli: undefined note: the bundle was not written")
endif()
execute_process(COMMAND "${RUNNER}" --content "${WORK}/halt.hex" --load-addr 0200 --ticks 100
  --profile lynx2 --out "${WORK}/undef2" RESULT_VARIABLE rc2 ERROR_VARIABLE err2 OUTPUT_QUIET)
if(NOT rc2 EQUAL 0 OR err2 MATCHES "undefined opcode")
  message(FATAL_ERROR "runner_cli: the bit-op image under lynx2: exit ${rc2}, stderr '${err2}'")
endif()
message(STATUS "runner_cli: the UNDEFINED stop writes a bundle, exits 0 and prints the note: ok")
