#!/usr/bin/env bash
set +e
cd /work/evidence/occt-step-control || exit 90
printf 'mode,count,exit_code\n' > status.csv
for spec in no_step:0 controller:0 init:0 writes:1 writes:10 writes:100 clear1:1 clear7:1 purge:1 nullalg:1; do
    mode=${spec%:*}
    count=${spec#*:}
    ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 LD_LIBRARY_PATH=/work/occt770/lib /work/evidence/occt-step-control/occt-step-lsan-probe "$mode" "$count" > "${mode}-${count}.log" 2>&1
    result=$?
    printf '%s,%s,%s\n' "$mode" "$count" "$result" >> status.csv
done
cat status.csv
for log in no_step-0.log controller-0.log init-0.log writes-1.log writes-10.log writes-100.log clear1-1.log clear7-1.log purge-1.log nullalg-1.log; do
    printf '%s ' "$log"
    grep 'SUMMARY:' "$log" || tail -1 "$log"
done
