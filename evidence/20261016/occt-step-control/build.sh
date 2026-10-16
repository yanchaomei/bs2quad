#!/usr/bin/env bash
set -euo pipefail
PROBE_DIR=/work/evidence/occt-step-control
SDK_DIR=/work/occt770
g++ "$PROBE_DIR/probe.cpp" -std=c++17 -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$SDK_DIR/include/opencascade" -L"$SDK_DIR/lib" \
  -Wl,-rpath-link,"$SDK_DIR/lib" \
  -lTKSTEP -lTKSTEP209 -lTKSTEPBase -lTKSTEPAttr -lTKXSBase \
  -lTKShHealing -lTKTopAlgo -lTKBRep -lTKGeomAlgo -lTKGeomBase \
  -lTKG3d -lTKG2d -lTKMath -lTKernel \
  -o "$PROBE_DIR/occt-step-lsan-probe"
