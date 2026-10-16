#!/usr/bin/env bash
# Build the exact upstream dependency in a caller-selected task directory.
# Run this on the remote build host; it does not replace system libraries.
set -euo pipefail
root=${1:?usage: bootstrap_occt.sh /absolute/task/dependencies [jobs]}
jobs=${2:-8}
mkdir -p "$root"
root=$(cd "$root" && pwd)
archive="$root/occt-7.7.0.tar.gz"
if [[ -n ${OCCT_ARCHIVE:-} ]]; then
    cp "$OCCT_ARCHIVE" "$archive"
else
    curl -fL --retry 2 --connect-timeout 15 --max-time 300 \
        https://codeload.github.com/Open-Cascade-SAS/OCCT/tar.gz/refs/tags/V7_7_0 -o "$archive"
fi
printf '%s  %s\n' '4cadb6d0a9e92dbb1a4eeb48d497b009d8138ffde64f2468d4a1c3f10a052722' "$archive" | sha256sum -c -
tar -xzf "$archive" -C "$root"
cmake -S "$root/OCCT-7_7_0" -B "$root/occt-build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$root/occt770" \
    -DBUILD_MODULE_Draw=OFF -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_ApplicationFramework=ON -DBUILD_MODULE_DataExchange=ON \
    -DUSE_TBB=OFF -DUSE_TCL=OFF -DUSE_VTK=OFF -DUSE_FREETYPE=OFF
cmake --build "$root/occt-build" -j "$jobs"
cmake --install "$root/occt-build"
echo "OpenCASCADE_DIR=$root/occt770/lib/cmake/opencascade"
echo "LD_LIBRARY_PATH=$root/occt770/lib"
