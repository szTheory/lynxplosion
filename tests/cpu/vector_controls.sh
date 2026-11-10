#!/bin/sh
# Named-fault controls for the per-opcode vector gates, run as
#   tests/cpu/vector_controls.sh CASE ROOT RUNNER SCRATCH
#
# CASE is one of: wrong_profile wrong_profile_reverse no_step corrupt_expect
# malformed inventory_drift sample_identity mutant_profile complete_mode lane_script.
# ROOT is the repository root, RUNNER the lynx-test-cpu-vectors executable and
# SCRATCH a directory under the build tree.
#
# Fault prevented: a vector gate that cannot fail. Each case builds its faulty
# input at run time under a fresh directory below SCRATCH (nothing faulty is
# committed, the real samples are never modified), runs the real runner and
# requires the exact exit code and message of the fault. The profile controls
# also compare the exact set of opcodes that have a failing vector, parsed from
# the runner's --results file, with the set the profile difference predicts:
# the 32 bit-operation opcodes ($x7 and $xF) in both directions. Only the
# profile controls, no_step and sample_identity read a full 3 MB sample; the
# others cut mini-samples (a few "#file" sections) from it with awk and pair
# them with a fresh SHA-256 manifest and a plan the runner prints itself.
set -eu
LC_ALL=C
export LC_ALL

if [ $# -ne 4 ]; then
  echo "usage: vector_controls.sh CASE ROOT RUNNER SCRATCH" >&2
  exit 2
fi
name=$1
root=$(cd "$2" && pwd -P)
runner=$3
mkdir -p "$4"
scratch=$(cd "$4" && pwd -P)

if [ ! -x "$runner" ]; then
  echo "vector_controls: $runner is not an executable" >&2
  exit 2
fi

syn=$root/fixtures/vectors/sst65x02/synertek65c02.sample
roc=$root/fixtures/vectors/sst65x02/rockwell65c02.sample
man_syn=$root/fixtures/manifests/sst65x02-synertek65c02.manifest
man_roc=$root/fixtures/manifests/sst65x02-rockwell65c02.manifest
inv=$root/tests/inventory/vectors

work=$(mktemp -d "$scratch/vc.XXXXXX")
work=$(cd "$work" && pwd -P)
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
trap 'exit 1' INT TERM

tab=$(printf '\t')

fail() {
  echo "vector_controls: $name: FAILED: $*" >&2
  exit 1
}
ok() {
  echo "vector_controls: $name: $1: ok"
}

sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha_out=$(sha256sum "$1")
  else
    sha_out=$(shasum -a 256 "$1")
  fi
  printf '%s\n' "${sha_out%% *}"
}

# ---- builders ---------------------------------------------------------------

# cut_sample SRC DST "OPS": the header and the sections of the listed opcodes.
cut_sample() {
  awk -v ops="$3" '
    NR <= 3 { print; next }
    /^#file / { keep = (index(" " ops " ", " " substr($2, 1, 2) " ") > 0) }
    keep { print }
  ' "$1" >"$2" || fail "could not cut a mini-sample"
}

# manifest_for SAMPLE DST: a manifest whose sha256 line is the sample's hash.
manifest_for() {
  printf 'sha256 = %s\n' "$(sha_of "$1")" >"$2"
}

# mutate_object SRC DST OP FROM TO: in the first object of section OP, the first
# match of the awk regex FROM becomes TO. The pattern must exist.
mutate_object() {
  awk -v op="$3" -v from="$4" -v to="$5" '
    /^#file / { inop = (substr($2, 1, 2) == op) }
    inop && !done && /^[{]/ { if (sub(from, to) == 0) { exit 9 } done = 1 }
    { print }
    END { if (!done) { exit 9 } }
  ' "$1" >"$2" || fail "mutation $4 -> $5 did not apply in section $3"
}

# corrupt_final SRC DST OP KEY: one value in the final state of the first object
# of section OP is raised by one (KEY a: the accumulator, KEY ram: the first
# final.ram value), so the CPU's correct result no longer matches the expectation.
corrupt_final() {
  awk -v op="$3" -v key="$4" '
    /^#file / { inop = (substr($2, 1, 2) == op) }
    inop && !done && /^[{]/ {
      idx = index($0, "\"final\"")
      head = substr($0, 1, idx - 1)
      tail = substr($0, idx)
      if (key == "a") {
        if (!match(tail, /"a": [0-9]+/)) { exit 9 }
        num = substr(tail, RSTART + 5, RLENGTH - 5) + 0
        tail = substr(tail, 1, RSTART + 4) ((num + 1) % 256) substr(tail, RSTART + RLENGTH)
      } else {
        if (!match(tail, /"ram": [[] [[][0-9]+, [0-9]+[]]/)) { exit 9 }
        m = substr(tail, RSTART, RLENGTH)
        n = split(m, parts, ", ")
        val = parts[n] + 0
        m = substr(m, 1, length(m) - length(parts[n])) ((val + 1) % 256) "]"
        tail = substr(tail, 1, RSTART - 1) m substr(tail, RSTART + RLENGTH)
      }
      $0 = head tail
      done = 1
    }
    { print }
    END { if (!done) { exit 9 } }
  ' "$1" >"$2" || fail "could not corrupt final.$4 in section $3"
}

# change_pc_byte SRC DST OP: the initial.ram byte at pc of the first object of
# section OP no longer equals the opcode.
change_pc_byte() {
  awk -v op="$3" '
    /^#file / { inop = (substr($2, 1, 2) == op) }
    inop && !done && /^[{]/ {
      if (!match($0, /"pc": [0-9]+/)) { exit 9 }
      pc = substr($0, RSTART + 6, RLENGTH - 6)
      pat = "[[]" pc ", [0-9]+[]]"
      if (!match($0, pat)) { exit 9 }
      m = substr($0, RSTART, RLENGTH)
      n = split(m, parts, ", ")
      val = parts[n] + 0
      m = substr(m, 1, length(m) - length(parts[n])) ((val + 1) % 256) "]"
      $0 = substr($0, 1, RSTART - 1) m substr($0, RSTART + RLENGTH)
      done = 1
    }
    { print }
    END { if (!done) { exit 9 } }
  ' "$1" >"$2" || fail "could not change the pc byte in section $3"
}

# header_delta SRC DST OP DELTA: the "lines" count of section OP changes by DELTA.
header_delta() {
  awk -v op="$3" -v d="$4" '
    /^#file / && substr($2, 1, 2) == op { $4 = $4 + d; done = 1 }
    { print }
    END { if (!done) { exit 9 } }
  ' "$1" >"$2" || fail "could not change the lines header of section $3"
}

# drop_second_object SRC DST OP: section OP loses its second object and its
# header count drops by one, so the section is self-consistent but holds 31.
drop_second_object() {
  awk -v op="$3" '
    /^#file / {
      inop = (substr($2, 1, 2) == op)
      seen = 0
      if (inop) { $4 = $4 - 1; done = 1 }
    }
    inop && /^[{]/ { seen++; if (seen == 2) { next } }
    { print }
    END { if (!done) { exit 9 } }
  ' "$1" >"$2" || fail "could not drop an object from section $3"
}

# plan_set PLAN DST OP COL VAL [COL VAL]...: sets tab-separated columns of the row OP.
plan_set() {
  src=$1
  dst=$2
  op=$3
  shift 3
  cp "$src" "$dst.tmp"
  while [ $# -ge 2 ]; do
    awk -F'\t' -v OFS='\t' -v op="$op" -v col="$1" -v val="$2" '
      $1 == op { $col = val; hit = 1 }
      { print }
      END { if (!hit) { exit 9 } }
    ' "$dst.tmp" >"$dst.tmp2" || fail "no row $op in $src"
    mv "$dst.tmp2" "$dst.tmp"
    shift 2
  done
  mv "$dst.tmp" "$dst"
}

# plan_line PLAN OP: the line number of the row OP.
plan_line() {
  awk -F'\t' -v op="$2" '$1 == op { print NR; exit }' "$1"
}

# ---- running ------------------------------------------------------------------

# run_runner OUT ARGS...: merges stdout and stderr in OUT and sets rc.
run_runner() {
  out=$1
  shift
  rc=0
  "$runner" "$@" >"$out" 2>&1 || rc=$?
}

# expect LABEL WANT_RC REGEX OUT: the exact exit code and a matching line.
expect() {
  if [ "$rc" -ne "$2" ]; then
    cat "$4" >&2
    fail "$1: expected exit $2, got $rc"
  fi
  if ! grep -Eq -- "$3" "$4"; then
    cat "$4" >&2
    fail "$1: the output has no line matching /$3/"
  fi
}

# print_plan PROFILE SAMPLE DST [partial]: the plan the runner derives itself.
print_plan() {
  if [ "${4:-}" = partial ]; then
    "$runner" run --profile "$1" --sample "$2" --partial --print-plan >"$3" || fail "no plan from $2"
  else
    "$runner" run --profile "$1" --sample "$2" --print-plan >"$3" || fail "no plan from $2"
  fi
}

# failing_set RESULTS: opcodes with at least one failing vector, space separated.
failing_set() {
  awk -F'\t' '$3 > 0 { printf "%s%s", sep, $1; sep = " " } END { print "" }' "$1"
}

# bitop_set: the 32 $x7 and $xF opcodes in ascending order.
bitop_set() {
  s=""
  for h in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do
    for l in 7 f; do
      s="$s${s:+ }$h$l"
    done
  done
  printf '%s\n' "$s"
}

# fail_lines OUT: the number of lines the runner printed as an assertion failure.
fail_lines() {
  grep -c '^FAIL opcode ' "$1" || true
}

# field OUT KEY N: the Nth word of the line starting with KEY.
field() {
  awk -v k="$2" -v n="$3" '$1 == k { print $n; exit }' "$1"
}

# A mini-sample with its manifest and plan; sets m_sample, m_manifest, m_plan.
mini() {
  m_dir=$1
  m_profile=$2
  m_ops=$3
  mkdir -p "$m_dir"
  m_sample=$m_dir/mini.sample
  m_manifest=$m_dir/mini.manifest
  m_plan=$m_dir/mini.plan
  cut_sample "$syn" "$m_sample" "$m_ops"
  manifest_for "$m_sample" "$m_manifest"
  print_plan "$m_profile" "$m_sample" "$m_plan" partial
}

# baseline: the unmodified mini-sample passes, so a later failure is the fault's.
baseline() {
  run_runner "$m_dir/baseline.out" run --profile "$m_profile" --sample "$m_sample" --manifest "$m_manifest" \
    --expect-plan "$m_plan" --partial
  expect baseline 0 'failed 0' "$m_dir/baseline.out"
}

# ---- cases --------------------------------------------------------------------

# A profile control: the other variant's data under this profile must fail on
# exactly the 32 bit-operation opcodes, each with at least one failing vector.
profile_control() {
  pc_profile=$1
  pc_sample=$2
  pc_manifest=$3
  print_plan "$pc_profile" "$pc_sample" "$work/plan.txt"
  run_runner "$work/out.txt" run --profile "$pc_profile" --sample "$pc_sample" --manifest "$pc_manifest" \
    --expect-plan "$work/plan.txt" --results "$work/results.txt"
  expect "$name" 1 '^asserted [0-9]+ passed [0-9]+ failed [1-9][0-9]* identity ' "$work/out.txt"
  got=$(failing_set "$work/results.txt")
  want=$(bitop_set)
  count=$(printf '%s\n' "$got" | wc -w | tr -d ' ')
  echo "vector_controls: $name: failing set ($count opcodes): $got"
  if [ "$got" != "$want" ]; then
    echo "vector_controls: $name: expected exactly: $want" >&2
    fail "the failing opcode set differs from the 32 bit-operation opcodes"
  fi
  [ "$count" -eq 32 ] || fail "expected 32 failing opcodes, got $count"
  ok "exactly the 32 bit-operation opcodes fail"
}

case_wrong_profile() {
  profile_control lynx1 "$roc" "$man_roc"
}

case_wrong_profile_reverse() {
  profile_control lynx2 "$syn" "$man_syn"
}

# no_step_one PROFILE VARIANT SAMPLE MANIFEST: state loaded, the CPU never called.
# Every vector the CPU would have to change fails; only vectors whose final
# state equals the loaded state exactly (pc included, "unmoved") can pass.
no_step_one() {
  ns_profile=$1
  ns_variant=$2
  ns_sample=$3
  ns_manifest=$4
  print_plan "$ns_profile" "$ns_sample" "$work/$ns_profile.plan"
  run_runner "$work/$ns_profile.out" run --profile "$ns_profile" --sample "$ns_sample" \
    --manifest "$ns_manifest" --expect-plan "$work/$ns_profile.plan" --fault no-step \
    --results "$work/$ns_profile.res"
  expect "$ns_variant" 1 '^fault no-step$' "$work/$ns_profile.out"
  asserted=$(field "$work/$ns_profile.out" asserted 2)
  passed=$(field "$work/$ns_profile.out" asserted 4)
  failed=$(field "$work/$ns_profile.out" asserted 6)
  identity=$(field "$work/$ns_profile.out" asserted 8)
  unmoved=$(field "$work/$ns_profile.out" unmoved 2)
  echo "vector_controls: $name: $ns_variant under $ns_profile: asserted $asserted passed $passed failed $failed identity $identity unmoved $unmoved"
  [ "$asserted" -gt 8000 ] || fail "$ns_variant: only $asserted vectors asserted"
  [ "$failed" -eq $((asserted - unmoved)) ] || fail "$ns_variant: failed $failed is not asserted $asserted minus unmoved $unmoved"
  [ "$passed" -eq "$unmoved" ] || fail "$ns_variant: passed $passed is not unmoved $unmoved"
  [ "$failed" -gt $((asserted * 9 / 10)) ] || fail "$ns_variant: the no-step run failed only $failed of $asserted vectors"
  rows=$(wc -l <"$work/$ns_profile.res" | tr -d ' ')
  nfail=$(awk -F'\t' '$3 > 0 { n++ } END { print n + 0 }' "$work/$ns_profile.res")
  [ "$nfail" -eq "$rows" ] || fail "$ns_variant: $((rows - nfail)) of $rows run rows still passed every vector with no CPU step"
  ok "$ns_variant: failures equal asserted minus unmoved, all $rows run rows fail"
}

case_no_step() {
  no_step_one lynx1 synertek65c02 "$syn" "$man_syn"
  no_step_one lynx2 rockwell65c02 "$roc" "$man_roc"
}

case_corrupt_expect() {
  mini "$work/m" lynx1 "00 69 8d ea"
  baseline
  # one final.a value
  corrupt_final "$m_sample" "$work/a.sample" ea a
  manifest_for "$work/a.sample" "$work/a.manifest"
  print_plan lynx1 "$work/a.sample" "$work/a.plan" partial
  run_runner "$work/a.out" run --profile lynx1 --sample "$work/a.sample" --manifest "$work/a.manifest" \
    --expect-plan "$work/a.plan" --partial --results "$work/a.res"
  expect final.a 1 '^FAIL opcode ea vector 0 a: got [0-9]+ expected [0-9]+$' "$work/a.out"
  [ "$(fail_lines "$work/a.out")" -eq 1 ] || fail "final.a: not exactly one failure"
  [ "$(failing_set "$work/a.res")" = "ea" ] || fail "final.a: the failing set is not {ea}"
  ok "one final.a value gives exactly one failure naming opcode ea, vector 0, field a"
  # one final.ram value
  corrupt_final "$m_sample" "$work/r.sample" 8d ram
  manifest_for "$work/r.sample" "$work/r.manifest"
  print_plan lynx1 "$work/r.sample" "$work/r.plan" partial
  run_runner "$work/r.out" run --profile lynx1 --sample "$work/r.sample" --manifest "$work/r.manifest" \
    --expect-plan "$work/r.plan" --partial --results "$work/r.res"
  expect final.ram 1 '^FAIL opcode 8d vector 0 ram: got [0-9]+ expected [0-9]+$' "$work/r.out"
  [ "$(fail_lines "$work/r.out")" -eq 1 ] || fail "final.ram: not exactly one failure"
  [ "$(failing_set "$work/r.res")" = "8d" ] || fail "final.ram: the failing set is not {8d}"
  ok "one final.ram value gives exactly one failure naming opcode 8d, vector 0, field ram"
  # a write outside the listed addresses
  run_runner "$work/s.out" run --profile lynx1 --sample "$m_sample" --manifest "$m_manifest" \
    --expect-plan "$m_plan" --partial --fault stray-write --results "$work/s.res"
  expect stray-write 1 '^FAIL opcode 00 vector 0 nowrite: got [0-9]+ expected 0$' "$work/s.out"
  [ "$(fail_lines "$work/s.out")" -eq 1 ] || fail "stray-write: not exactly one failure"
  [ "$(failing_set "$work/s.res")" = "00" ] || fail "stray-write: the failing set is not {00}"
  ok "one stray write gives exactly one nowrite failure"
}

# malformed_case LABEL FILE REGEX: the faulty mini-sample must exit 3 with the
# sample's file name and REGEX in the message and no success line.
malformed_case() {
  mc_label=$1
  mc_file=$work/mal_$mc_label.sample
  manifest_for "$mc_file" "$work/mal_$mc_label.manifest"
  run_runner "$work/mal_$mc_label.out" run --profile lynx1 --sample "$mc_file" \
    --manifest "$work/mal_$mc_label.manifest" --expect-plan "$m_plan" --partial
  expect "$mc_label" 3 "mal_${mc_label}[.]sample: .*$2" "$work/mal_$mc_label.out"
  if grep -Eq '^(asserted|totals|plan matches)|vectors 0' "$work/mal_$mc_label.out"; then
    cat "$work/mal_$mc_label.out" >&2
    fail "$mc_label: the runner printed a success or zero-vector line"
  fi
  ok "$mc_label exits 3 naming the file"
}

case_malformed() {
  mini "$work/m" lynx1 "ea"
  baseline
  mutate_object "$m_sample" "$work/mal_truncated_object.sample" ea ' },$' ''
  malformed_case truncated_object 'opcode ea vector 0'
  mutate_object "$m_sample" "$work/mal_unknown_key.sample" ea '"final"' '"finale"'
  malformed_case unknown_key 'opcode ea vector 0: .*expected final'
  mutate_object "$m_sample" "$work/mal_duplicate_key.sample" ea '"s": ' '"pc": 1, "s": '
  malformed_case duplicate_key 'opcode ea vector 0: .*expected s'
  mutate_object "$m_sample" "$work/mal_missing_final.sample" ea '"final": .*"cycles"' '"cycles"'
  malformed_case missing_final 'opcode ea vector 0: .*expected final'
  mutate_object "$m_sample" "$work/mal_pc_70000.sample" ea '"pc": [0-9]+' '"pc": 70000'
  malformed_case pc_70000 'opcode ea vector 0: number out of range for pc'
  mutate_object "$m_sample" "$work/mal_value_256.sample" ea '"a": [0-9]+' '"a": 256'
  malformed_case value_256 'opcode ea vector 0: number out of range for a'
  mutate_object "$m_sample" "$work/mal_negative.sample" ea '"x": [0-9]+' '"x": -2'
  malformed_case negative 'opcode ea vector 0: expected unsigned number for x'
  change_pc_byte "$m_sample" "$work/mal_pc_byte.sample" ea
  malformed_case pc_byte 'opcode ea vector 0: initial.ram byte at pc is not the file opcode'
  mutate_object "$m_sample" "$work/mal_name_token.sample" ea '"name": "ea ' '"name": "eb '
  malformed_case name_token 'opcode ea vector 0: .*first token is not the file opcode'
  mutate_object "$m_sample" "$work/mal_split_object.sample" ea '"initial": ' '"initial":\n'
  malformed_case split_object 'opcode ea'
  header_delta "$m_sample" "$work/mal_lines_plus.sample" ea 1
  malformed_case lines_plus "opcode ea.*shorter than its 'lines' header"
  header_delta "$m_sample" "$work/mal_lines_minus.sample" ea -1
  malformed_case lines_minus 'opcode ea'
}

# drift_case LABEL PROFILE PLAN LINE: exit 4 at exactly that line of the plan.
drift_case() {
  run_runner "$work/d.out" run --profile "$2" --sample "$m_sample" --manifest "$m_manifest" \
    --expect-plan "$3" --partial
  expect "$1" 4 "^inventory mismatch: first difference at line $4\$" "$work/d.out"
  ok "$1 exits 4 at plan line $4"
}

case_inventory_drift() {
  mini "$work/m" lynx1 "00 07 69 cb ea"
  baseline
  cp "$m_plan" "$work/lynx1.plan"
  mini "$work/n" none "00 07 69 cb ea"
  cp "$m_plan" "$work/none.plan"
  # a row missing
  line=$(plan_line "$work/lynx1.plan" 69)
  grep -v "^69$tab" "$work/lynx1.plan" >"$work/d1.plan"
  drift_case missing_opcode_row lynx1 "$work/d1.plan" "$line"
  # an excluded row without a reason
  line=$(plan_line "$work/lynx1.plan" cb)
  plan_set "$work/lynx1.plan" "$work/d2.plan" cb 6 -
  drift_case excluded_without_reason lynx1 "$work/d2.plan" "$line"
  # a reason outside the closed list
  line=$(plan_line "$work/lynx1.plan" 00)
  plan_set "$work/lynx1.plan" "$work/d3.plan" 00 6 made-up-reason
  drift_case reason_outside_list lynx1 "$work/d3.plan" "$line"
  # an exclusion in the table that the file does not carry
  line=$(plan_line "$work/lynx1.plan" cb)
  plan_set "$work/lynx1.plan" "$work/d4.plan" cb 2 run 3 full 6 -
  drift_case table_exclusion_missing lynx1 "$work/d4.plan" "$line"
  # an exclusion in the file that the table does not carry
  line=$(plan_line "$work/lynx1.plan" ea)
  plan_set "$work/lynx1.plan" "$work/d5.plan" ea 2 excluded 3 - 6 source-disagrees-with-hardware-informed-test
  drift_case stale_exclusion lynx1 "$work/d5.plan" "$line"
  # wrong totals
  line=$(awk '/^# totals/ { print NR; exit }' "$work/lynx1.plan")
  awk '/^# totals/ { if (match($0, /asserted [0-9]+/)) { n = substr($0, RSTART + 9, RLENGTH - 9) + 1; $0 = substr($0, 1, RSTART + 8) n substr($0, RSTART + RLENGTH) } } { print }' \
    "$work/lynx1.plan" >"$work/d6.plan"
  drift_case wrong_totals lynx1 "$work/d6.plan" "$line"
  # the reason that is legal only in a none plan, in a lynx1 plan
  line=$(plan_line "$work/lynx1.plan" 07)
  plan_set "$work/lynx1.plan" "$work/d7.plan" 07 6 profile-none-stops-undefined
  drift_case none_reason_in_lynx1 lynx1 "$work/d7.plan" "$line"
  # profile none: an undefined row changed to run, a shared row to undefined
  line=$(plan_line "$work/none.plan" 07)
  plan_set "$work/none.plan" "$work/d8.plan" 07 2 run 3 full 6 -
  drift_case undefined_to_run none "$work/d8.plan" "$line"
  line=$(plan_line "$work/none.plan" 00)
  plan_set "$work/none.plan" "$work/d9.plan" 00 2 undefined 3 - 6 profile-none-stops-undefined
  drift_case shared_to_undefined none "$work/d9.plan" "$line"
  # sample-side faults are data errors (exit 3), not inventory drift
  drop_second_object "$m_sample" "$work/ds1.sample" 00
  manifest_for "$work/ds1.sample" "$work/ds1.manifest"
  run_runner "$work/ds1.out" run --profile lynx1 --sample "$work/ds1.sample" --manifest "$work/ds1.manifest" \
    --expect-plan "$work/lynx1.plan" --partial
  expect section_31_objects 3 'opcode 00 section: .*different object count than 32: 31' "$work/ds1.out"
  ok "a section with 31 objects exits 3"
  cut_sample "$m_sample" "$work/dup_a.sample" "00"
  cut_sample "$m_sample" "$work/dup_b.sample" "07 69 cb ea"
  { cat "$work/dup_a.sample"; tail -n +4 "$work/dup_a.sample"; tail -n +4 "$work/dup_b.sample"; } >"$work/ds2.sample"
  manifest_for "$work/ds2.sample" "$work/ds2.manifest"
  run_runner "$work/ds2.out" run --profile lynx1 --sample "$work/ds2.sample" --manifest "$work/ds2.manifest" \
    --expect-plan "$work/lynx1.plan" --partial
  expect duplicated_file 3 'sections are not in opcode order' "$work/ds2.out"
  ok "a duplicated #file section exits 3"
}

case_sample_identity() {
  copy=$work/sample_identity.sample
  awk '
    !done && /^[{]/ && match($0, /"x": [0-9]+,/) {
      digits = substr($0, RSTART + 5, RLENGTH - 6)
      last = substr(digits, length(digits), 1) + 0
      last = (last == 0) ? 1 : last - 1
      $0 = substr($0, 1, RSTART + 4) substr(digits, 1, length(digits) - 1) last "," substr($0, RSTART + RLENGTH)
      done = 1
    }
    { print }
    END { if (!done) { exit 9 } }
  ' "$syn" >"$copy" || fail "could not change one byte"
  differing=$(cmp -l "$syn" "$copy" | wc -l | tr -d ' ')
  [ "$differing" -eq 1 ] || fail "the copy differs from the sample in $differing bytes, not 1"
  # the changed copy is still well-formed data, so only the hash can reject it
  run_runner "$work/pc.out" parse-check "$copy"
  expect parse_check_of_copy 0 '^sections 256 vectors 8192$' "$work/pc.out"
  run_runner "$work/id.out" run --profile lynx1 --sample "$copy" --manifest "$man_syn" \
    --expect-plan "$inv/lynx1.txt"
  expect sample_identity 3 'sha256 mismatch: sample [0-9a-f]{64}, manifest 2fabae3f' "$work/id.out"
  if grep -Eq 'inventory mismatch|^FAIL|^asserted' "$work/id.out"; then
    cat "$work/id.out" >&2
    fail "the runner went past the hash check"
  fi
  ok "one changed byte exits 3 with sha256 mismatch before any parse"
}

case_mutant_profile() {
  mini "$work/m" lynx1 "00 69 e9 ea"
  baseline
  run_runner "$work/mu.out" run --profile lynx1 --sample "$m_sample" --manifest "$m_manifest" \
    --expect-plan "$m_plan" --partial --fault mutant-69 --results "$work/mu.res"
  expect mutant_69 1 '^fault mutant-69$' "$work/mu.out"
  got=$(failing_set "$work/mu.res")
  echo "vector_controls: $name: failing set: $got"
  [ "$got" = "69" ] || fail "the failing set is not exactly {69}"
  ok "decoding \$69 as SBC fails exactly opcode 69"
}

# synth_file SAMPLE OP COUNT DST: a whole-file layout with COUNT objects cycling
# the 32 sampled objects of section OP.
synth_file() {
  awk -v op="$2" -v count="$3" '
    /^#file / { inop = (substr($2, 1, 2) == op); next }
    inop && /^[{]/ { sub(/,$/, ""); objs[n++] = $0 }
    END {
      print "["
      for (i = 0; i < count; i++) { printf "%s%s\n", objs[i % n], (i < count - 1 ? "," : "") }
      print "]"
    }
  ' "$1" >"$4" || fail "could not build a whole-file layout for $2"
}

# The complete-file mode of the full lane, on synthetic whole files (10,000
# objects each) built from the sample, so the network lane's runner path is
# exercised by the check.
case_complete_mode() {
  dl=$work/dl
  mkdir -p "$dl/synertek65c02/v1"
  for op in 07 cb ea; do
    synth_file "$syn" "$op" 10000 "$dl/synertek65c02/v1/$op.json"
  done
  run_runner "$work/c1.out" complete --profile lynx1 --dir "$dl" --variant synertek65c02 --partial \
    --results "$work/c1.res"
  expect lynx1 0 '^totals files 3 run 2 excluded 1 undefined 0 vectors 30000$' "$work/c1.out"
  expect lynx1 0 '^asserted 20000 passed 20000 failed 0 identity [0-9]+$' "$work/c1.out"
  expect lynx1 0 '^excluded_vectors parsed 10000 asserted 0$' "$work/c1.out"
  [ "$(failing_set "$work/c1.res")" = "" ] || fail "lynx1: a failing opcode in a clean run"
  [ "$(wc -l <"$work/c1.res" | tr -d ' ')" -eq 2 ] || fail "lynx1: the results file has not 2 rows"
  ok "lynx1: 30,000 vectors in 3 whole files, 20,000 asserted and passed, 10,000 excluded"
  run_runner "$work/c0.out" complete --profile none --dir "$dl" --variant synertek65c02 --partial
  expect none 0 '^totals files 3 run 1 excluded 1 undefined 1 vectors 30000$' "$work/c0.out"
  expect none 0 '^asserted 10000 passed 10000 failed 0 identity [0-9]+$' "$work/c0.out"
  expect none 0 '^undefined_vectors 10000 stopped 10000$' "$work/c0.out"
  ok "none: the 10,000 vectors of the undefined row stop, 10,000 pass"
  run_runner "$work/cn.out" complete --profile lynx1 --dir "$dl" --variant synertek65c02 --partial --fault no-step
  expect no_step 1 '^asserted 20000 passed 0 failed 20000 ' "$work/cn.out"
  ok "no-step fails every asserted vector of the whole files"
  run_runner "$work/cm.out" complete --profile lynx1 --dir "$dl" --variant synertek65c02
  expect missing_file 3 'cannot open .*/v1/00[.]json' "$work/cm.out"
  ok "a missing file of the 256 exits 3 without --partial"
  synth_file "$syn" ea 9999 "$dl/synertek65c02/v1/ea.json"
  run_runner "$work/ck.out" complete --profile lynx1 --dir "$dl" --variant synertek65c02 --partial
  expect short_file 3 'ea[.]json: .*different object count than 10000: 9999' "$work/ck.out"
  ok "a file of 9,999 objects exits 3"
}

# ---- the full-lane script (scripts/vectors-full.sh), offline -------------------
#
# The lane itself needs 2 GB of network data, so these controls run copies of
# the real script in a fake repository under the work directory, with a fake
# cmake on PATH where a build is involved, and never touch the network.

# lane_root DIR: a fake repository root with a copy of the lane script.
lane_root() {
  mkdir -p "$1/scripts" "$1/build/check/tests" "$1/fixtures/vectors/sst65x02"
  cp "$root/scripts/vectors-full.sh" "$1/scripts/vectors-full.sh" || fail "could not copy the lane script"
}

# WR-04: the run mode must build the runner before running it, always. A stale
# binary (older, or edited locally) must never carry the evidence of a 2 GB lane.
lane_runner_build() {
  fr=$work/lane-build
  lane_root "$fr"
  mkdir -p "$fr/bin"
  printf '#!/bin/sh\necho stale\n' >"$fr/build/check/tests/lynx-test-cpu-vectors"
  chmod +x "$fr/build/check/tests/lynx-test-cpu-vectors"
  # The fake cmake logs its arguments; its build step replaces the runner.
  # shellcheck disable=SC2016 # the stub, not this shell, expands these
  {
    printf '#!/bin/sh\n'
    printf 'printf "%%s\\n" "$*" >>"%s/cmake.log"\n' "$fr"
    printf 'case $1 in\n'
    printf '  --build) printf "#!/bin/sh\\necho fresh\\n" >"%s/build/check/tests/lynx-test-cpu-vectors"\n' "$fr"
    printf '    chmod +x "%s/build/check/tests/lynx-test-cpu-vectors" ;;\n' "$fr"
    printf 'esac\n'
  } >"$fr/bin/cmake"
  chmod +x "$fr/bin/cmake"
  rc=0
  PATH="$fr/bin:$PATH" sh "$fr/scripts/vectors-full.sh" build >"$work/lane-build.out" 2>&1 || rc=$?
  expect lane_build 0 'vectors-full: build: ' "$work/lane-build.out"
  [ -f "$fr/cmake.log" ] || fail "lane_build: cmake was never called"
  grep -qx -- '--preset check' "$fr/cmake.log" || fail "lane_build: no configure of the check preset"
  grep -qx -- '--build --preset check' "$fr/cmake.log" || fail "lane_build: no build of the check preset"
  [ "$("$fr/build/check/tests/lynx-test-cpu-vectors")" = fresh ] || fail "lane_build: the stale runner was not replaced"
  # The build must also fail closed: a failing cmake ends the mode with exit 1.
  printf '#!/bin/sh\nexit 1\n' >"$fr/bin/cmake"
  rc=0
  PATH="$fr/bin:$PATH" sh "$fr/scripts/vectors-full.sh" build >"$work/lane-build2.out" 2>&1 || rc=$?
  expect lane_build_fail 1 'the check preset did not build' "$work/lane-build2.out"
  # The run mode calls the same build unconditionally, before the first lane:
  # no guard around the call, and it precedes run_same.
  awk '
    /^    run\)$/ { inrun = 1; next }
    inrun && /^      ;;$/ { exit }
    inrun && /^      build_runner$/ { b = NR }
    inrun && /^      run_same none / { if (!r) { r = NR } }
    END { exit (b > 0 && r > b) ? 0 : 1 }
  ' "$fr/scripts/vectors-full.sh" || fail "lane_build: the run mode does not call build_runner unconditionally before its first lane"
  ok "the run mode builds the check preset before running; a stale runner is replaced; a failed build is fatal"
}

# lane_harness DIR MODE: the real run_same and run_cross of the lane script, cut
# out of it and driven against a fake runner (the script's own helper functions
# are cut out with them). MODE tells the fake runner what to do. The caller
# appends the calls to the harness on stdin and runs it under set -eu, as the
# script does.
lane_harness() {
  lh=$1
  mkdir -p "$lh/root/build/vectors-full" "$lh/work"
  {
    printf 'set -eu\nLC_ALL=C\nexport LC_ALL\n'
    printf 'HEXDIGITS="0 1 2 3 4 5 6 7 8 9 a b c d e f"\n'
    printf 'root=%s\nwork=%s\nlane_status=0\nrunner=%s\n' "$lh/root" "$lh/work" "$lh/runner"
    for fn in sha_of failing_set bitop_set run_same run_cross; do
      sed -n "/^$fn() {\$/,/^}\$/p" "$root/scripts/vectors-full.sh"
    done
    cat
  } >"$lh/harness.sh"
  # shellcheck disable=SC2016 # the fake runner, not this shell, expands these
  {
    printf '#!/bin/sh\n'
    printf 'res=\n'
    printf 'while [ $# -gt 0 ]; do [ "$1" = --results ] && res=$2; shift; done\n'
    printf 'case "%s" in\n' "$2"
    printf '  crash) exit 3 ;;\n'
    printf '  same) rc=0 ;;\n'
    printf '  cross*) rc=1 ;;\n'
    printf 'esac\n'
    printf 'for h in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do for l in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do\n'
    printf '  op=$h$l; pass=10000; fail=0\n'
    printf '  case "%s$l" in cross*7|cross*f) pass=5000; fail=5000 ;; esac\n' "$2"
    printf '  case "%s$op" in cross_short07) pass=4000 ;; esac\n' "$2"
    printf '  printf "%%s\\t%%s\\t%%s\\n" "$op" "$pass" "$fail"\n'
    printf 'done; done >"$res"\n'
    printf 'exit $rc\n'
  } >"$lh/runner"
  chmod +x "$lh/runner"
}

# IN-04: a runner that dies without writing its results file must be reported as
# a failed lane, not end the script through the unchecked cp of the file.
lane_results_missing() {
  lh=$work/lane-results
  lane_harness "$lh" crash <<'EOF'
run_same none synertek65c02 --profile none
echo "lane_status=$lane_status"
EOF
  rc=0
  sh "$lh/harness.sh" >"$work/lane-results.out" 2>&1 || rc=$?
  expect lane_results_missing 0 'FAILED, the runner wrote no results file' "$work/lane-results.out"
  expect lane_results_missing 0 '^lane_status=1$' "$work/lane-results.out"
  ok "a runner without a results file is a failed lane, reported, not a silent exit"
}

case_lane_script() {
  lane_runner_build
  lane_results_missing
}

case $name in
  wrong_profile) case_wrong_profile ;;
  wrong_profile_reverse) case_wrong_profile_reverse ;;
  no_step) case_no_step ;;
  corrupt_expect) case_corrupt_expect ;;
  malformed) case_malformed ;;
  inventory_drift) case_inventory_drift ;;
  sample_identity) case_sample_identity ;;
  mutant_profile) case_mutant_profile ;;
  complete_mode) case_complete_mode ;;
  lane_script) case_lane_script ;;
  *)
    echo "vector_controls: unknown case '$name'" >&2
    exit 2
    ;;
esac
echo "vector_controls: $name: all ok"
