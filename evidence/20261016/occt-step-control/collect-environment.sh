#!/usr/bin/env bash
set -euo pipefail
PROBE_DIR=/work/evidence/occt-step-control
SDK_DIR=/work/occt770
{
    date -u '+%Y-%m-%d %H:%M:%S UTC'
    uname -a
    g++ --version
    cat /etc/os-release
    printf 'ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0\n'
    printf 'LD_LIBRARY_PATH=/work/occt770/lib\n'
    LD_LIBRARY_PATH="$SDK_DIR/lib" ldd "$PROBE_DIR/occt-step-lsan-probe"
} > "$PROBE_DIR/environment.txt"
sha256sum "$PROBE_DIR/probe.cpp" "$PROBE_DIR/build.sh" "$PROBE_DIR/run-controls.sh" \
    "$PROBE_DIR/occt-step-lsan-probe" \
    "$SDK_DIR/lib/libTKernel.so.7.7" "$SDK_DIR/lib/libTKXSBase.so.7.7" "$SDK_DIR/lib/libTKSTEP.so.7.7" \
    "$SDK_DIR/include/opencascade/Standard_Version.hxx" \
    "$SDK_DIR/include/opencascade/NCollection_Array1.hxx" \
    /work/occt-7.7.0-verified.tar.gz > "$PROBE_DIR/sdk-sha256.txt"
{
    printf 'STEPControl_Controller.cxx:423-433 (one-time global controller registration)\n'
    nl -ba /work/OCCT-7_7_0/src/STEPControl/STEPControl_Controller.cxx | sed -n '423,433p'
    printf 'XSControl_WorkSession.hxx:44-63 (documented RAII destructor and cleanup modes)\n'
    nl -ba "$SDK_DIR/include/opencascade/XSControl_WorkSession.hxx" | sed -n '44,63p'
    printf 'XSControl_WorkSession.cxx:490-525 (destructor cleanup implementation)\n'
    nl -ba /work/OCCT-7_7_0/src/XSControl/XSControl_WorkSession.cxx | sed -n '490,525p'
    printf 'STEPConstruct_ContextTool.cxx:445-452 (cached default axis)\n'
    nl -ba /work/OCCT-7_7_0/src/STEPConstruct/STEPConstruct_ContextTool.cxx | sed -n '445,452p'
    printf 'NCollection_Array1.hxx:153-164,426-431 (shifted data pointer and RAII release)\n'
    nl -ba "$SDK_DIR/include/opencascade/NCollection_Array1.hxx" | sed -n '153,164p;426,431p'
} > "$PROBE_DIR/pinned-source-excerpts.txt"
