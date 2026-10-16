#!/usr/bin/env bash
# Reproducible CLI behavior checks. Run next to saved NURBS input fixtures.
set -euo pipefail
binary=${1:?absolute executable path required}
inputs=${2:?directory containing case_0.brep and case_5.brep required}
logs=${3:?output log directory required}
mkdir -p "$logs"
logs=$(mktemp -d "$logs/run-XXXXXXXX")
checks=0
check() {
    local name=$1 expected=$2 actual
    shift 2
    if "$binary" "$@" >"$logs/$name.log" 2>&1; then actual=0; else actual=$?; fi
    if [[ "$actual" != "$expected" ]]; then
        echo "FAIL $name: expected exit $expected, received $actual"
        cat "$logs/$name.log"
        exit 1
    fi
    checks=$((checks+1))
    echo "PASS $name (exit $actual)"
}
check help 0 --help
check invalid_nan 2 --demo --tolerance nan
check invalid_inf 2 --demo --tolerance inf
check invalid_negative 2 --demo --tolerance -1
check invalid_zero 2 --demo --tolerance 0
check trailing_text 2 --demo --tolerance 1e-5garbage
check missing_tolerance 2 --tolerance
check missing_count 2 --generate
check invalid_count 2 --generate -1
check overflow_count 2 --generate 999999999999999999999999
check unknown_option 2 --unknown-option
check missing_input 2 --input "$logs/absent.brep" --output "$logs/absent-out.brep"
printf 'damaged NURBS file\n' >"$logs/corrupt.brep"
check corrupt_input 2 --input "$logs/corrupt.brep" --output "$logs/corrupt-out.brep"
check same_input_output 2 --input "$inputs/case_0.brep" --output "$inputs/case_0.brep"
check same_input_report 2 --input "$inputs/case_0.brep" --output "$logs/should-not-write.brep" --report "$inputs/case_0.brep"
ln -s "$inputs/case_0.brep" "$logs/alias.brep"
check aliased_input_output 2 --input "$inputs/case_0.brep" --output "$logs/alias.brep"
mkdir -p "$logs/real"
ln -s "$logs/real" "$logs/parent-alias"
check aliased_new_output_report 2 --input "$inputs/case_0.brep" --output "$logs/real/new.brep" --report "$logs/parent-alias/new.brep"
test ! -e "$logs/real/new.brep"
check valid_brep 0 --input "$inputs/case_0.brep" --output "$logs/converted.brep" --report "$logs/result.json"
test -s "$logs/converted.brep"
grep -q '"success":true' "$logs/result.json"
check valid_step_export 0 --input "$inputs/case_0.brep" --output "$logs/converted.step"
test -s "$logs/converted.step"
check freeform_rejected 1 --input "$inputs/case_5.brep" --output "$logs/rejected.brep" --report "$logs/rejected.json"
test ! -e "$logs/rejected.brep"
grep -q '"success":false' "$logs/rejected.json"
check contradictory_modes 2 --demo --generate 5
check six_family_step_generation 0 --generate 6 --output-dir "$logs/generated" --seed 42
test -s "$logs/generated/torus_4.step"
echo "CLI checks passed: $checks"
