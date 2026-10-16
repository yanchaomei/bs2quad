#!/usr/bin/env bash
# Run all available checks, retain each exit status, and never turn a failed
# contest performance requirement into a successful correctness test.
set -uo pipefail
project=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$project/build"}
receipts=${2:-"$project/evidence/verification-$(date -u +%Y%m%dT%H%M%SZ)"}
if [[ -d "$receipts" ]] && [[ -n $(ls -A "$receipts") ]]; then
    echo "refusing to overwrite nonempty evidence directory: $receipts" >&2
    exit 2
fi
mkdir -p "$receipts"
failed=0
step() {
    local name=$1 rc
    shift
    "$@" >"$receipts/$name.log" 2>&1
    rc=$?
    printf '%s\t%s\n' "$name" "$rc" >>"$receipts/status.tsv"
    echo "$name exit=$rc"
    if [[ $rc -ne 0 ]]; then failed=1; fi
    return "$rc"
}
: >"$receipts/status.tsv"
step configure cmake -S "$project" -B "$build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON || exit 1
step build cmake --build "$build" -j 4 || exit 1
step correctness ctest --test-dir "$build" --output-on-failure --no-tests=error
step surface1000 "$build/bs2quad_evidence" --count 1000 --output-dir "$receipts/surface1000"
step compatibility_target grep -Eq '"sgk_1_7_verified"[[:space:]]*:[[:space:]]*true' "$receipts/surface1000/summary.json"
step decision_target grep -Eq '"decision_10ms_pass"[[:space:]]*:[[:space:]]*true' "$receipts/surface1000/summary.json"
step cli bash "$project/scripts/check_cli.sh" "$build/bs2quad_main" "$receipts/surface1000/inputs" "$receipts/cli"
if [[ -x "$build/bs2quad_cad_benchmark" ]]; then
    step cad_operations "$build/bs2quad_cad_benchmark" --output-dir "$receipts/cad" --iterations 5
    step cad_target grep -Eq '"all_operation_targets_pass"[[:space:]]*:[[:space:]]*true' "$receipts/cad/summary.json"
else
    printf 'cad_operations\t127\n' >>"$receipts/status.tsv"
    failed=1
fi
exit "$failed"
