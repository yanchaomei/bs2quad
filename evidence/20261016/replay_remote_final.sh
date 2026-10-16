#!/usr/bin/env bash
set -uo pipefail
root=/home/kvgroup/chaomei/g_bs2quad_20261016
cd "$root"
python3 source/scripts/source_manifest.py --root source --output evidence/source-final.json
docker exec bs2quad_20261016 bash -c 'export OpenCASCADE_DIR=/work/occt770/lib/cmake/opencascade LD_LIBRARY_PATH=/work/occt770/lib OMP_NUM_THREADS=1; bash /work/source/scripts/verify.sh /work/build /work/evidence/final-release-v3' >evidence/final-release-v3-driver.log 2>&1
printf '%s\n' "$?" >evidence/final-release-v3.exit
cp build/unit-results.xml evidence/unit-final.xml
docker exec bs2quad_20261016 bash -c 'export OpenCASCADE_DIR=/work/occt770/lib/cmake/opencascade LD_LIBRARY_PATH=/work/occt770/lib; cmake -S /work/source -B /work/build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON -DBS2QUAD_SANITIZERS=ON && cmake --build /work/build-asan -j4' >evidence/asan-build.log 2>&1
printf '%s\n' "$?" >evidence/asan-build.exit
docker exec bs2quad_20261016 bash -c 'export LD_LIBRARY_PATH=/work/occt770/lib OMP_NUM_THREADS=1 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; ctest --test-dir /work/build-asan --output-on-failure --no-tests=error' >evidence/asan-ctest.log 2>&1
printf '%s\n' "$?" >evidence/asan-ctest.exit
docker exec bs2quad_20261016 bash -c 'export LD_LIBRARY_PATH=/work/occt770/lib OMP_NUM_THREADS=1 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1; /work/build-asan/bs2quad_evidence --count 1000 --output-dir /work/evidence/asan1000 --no-save-inputs' >evidence/asan1000.log 2>&1
printf '%s\n' "$?" >evidence/asan1000.exit
docker exec bs2quad_20261016 bash -c 'export LD_LIBRARY_PATH=/work/occt770/lib OMP_NUM_THREADS=1; timeout 300 valgrind --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=99 --log-file=/work/evidence/valgrind1000.log /work/build/bs2quad_evidence --count 1000 --output-dir /work/evidence/valgrind1000 --no-save-inputs' >evidence/valgrind1000-driver.log 2>&1
printf '%s\n' "$?" >evidence/valgrind1000.exit
docker exec bs2quad_20261016 bash -c 'export LD_LIBRARY_PATH=/work/occt770/lib OMP_NUM_THREADS=1; timeout 240 valgrind --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=99 --log-file=/work/evidence/valgrind-interfaces.log /work/build/unit_tests --gtest_filter=ConversionContract.*:PointFitting.*:ValidationRegression.*' >evidence/valgrind-interfaces-driver.log 2>&1
printf '%s\n' "$?" >evidence/valgrind-interfaces.exit
printf 'complete\n' >evidence/final-checks.complete
