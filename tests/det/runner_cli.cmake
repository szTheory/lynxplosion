# runner.cli: the runner's defaults, usage errors and refusals.
# Usage: cmake -DRUNNER=... -DCONTENT=... -DWORK=... -P runner_cli.cmake
#
# Fault prevented: a runner that crashes, hangs or writes on bad input, or that
# silently defaults to a seeded RAM policy, would corrupt the evidence.
foreach(var RUNNER CONTENT WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "runner_cli: -D${var}= is required")
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

# The full tick range is accepted. The largest budget needs content that halts
# at once (0xdb), or the run would simulate for geological time.
file(WRITE "${WORK}/halt.hex" "db\n")
expect("zero ticks" 0 ""
  --content "${CONTENT}" --load-addr 0200 --ticks 0 --out "${WORK}/zero")
expect("largest tick budget" 0 ""
  --content "${WORK}/halt.hex" --load-addr 0200 --ticks 18446744073709551615 --out "${WORK}/max")
file(STRINGS "${WORK}/max/run.manifest" max_lines REGEX "^(tick_budget|stop_reason) = ")
if(NOT max_lines STREQUAL "tick_budget = 18446744073709551615;stop_reason = halt")
  message(FATAL_ERROR "runner_cli: largest tick budget: manifest says '${max_lines}'")
endif()
message(STATUS "runner_cli: largest tick budget recorded, halt reported: ok")

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
