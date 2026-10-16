#!/usr/bin/env bash
# Negative controls for delayed buffered write failures. Linux /dev/full does
# not consume disk capacity; it rejects every write with ENOSPC.
set -euo pipefail
cli=${1:?CLI path required}
evidence=${2:?evidence runner path required}
cad=${3:?CAD benchmark path required}
input=${4:?valid NURBS BRep input required}
logs=${5:?log directory required}
mkdir -p "$logs"
logs=$(mktemp -d "$logs/run-XXXXXXXX")
check() {
    local name=$1 expected=$2 actual
    shift 2
    if "$@" >"$logs/$name.log" 2>&1; then actual=0; else actual=$?; fi
    if [[ $actual != "$expected" ]]; then
        echo "FAIL $name: expected exit $expected, received $actual"
        cat "$logs/$name.log"
        exit 1
    fi
    echo "PASS $name (exit $actual)"
}
check cad_unknown_case 2 "$cad" --case unknown --output-dir "$logs/unknown-case"
check cad_conflicting_subset 2 "$cad" --pilot --case torus --output-dir "$logs/conflicting"
check cli_report_flush 2 "$cli" --input "$input" --output "$logs/converted.brep" --report /dev/full
mkdir "$logs/evidence-summary"
ln -s /dev/full "$logs/evidence-summary/summary.json"
check summary_flush 2 "$evidence" --count 6 --no-save-inputs --output-dir "$logs/evidence-summary"
mkdir "$logs/evidence-csv"
ln -s /dev/full "$logs/evidence-csv/cases.csv"
check csv_flush 2 "$evidence" --count 6 --no-save-inputs --output-dir "$logs/evidence-csv"
mkdir "$logs/cad-summary"
ln -s /dev/full "$logs/cad-summary/summary.json"
check cad_summary_flush 2 "$cad" --pilot --output-dir "$logs/cad-summary" --iterations 5
echo 'Buffered output failure checks passed: 6'
