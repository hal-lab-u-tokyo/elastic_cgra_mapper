#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VTR_DIR="${VTR_DIR:-${REPO_ROOT}/third_party/vtr}"
JOBS="${JOBS:-}"
VTR_CXX_FLAGS="${VTR_CXX_FLAGS:--include limits}"

if [ -z "${JOBS}" ]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS="$(nproc)"
    else
        JOBS="4"
    fi
fi

cd "${REPO_ROOT}"

if [ ! -d "${VTR_DIR}/.git" ]; then
    git submodule update --init --recursive --depth 1 third_party/vtr
fi

if [ -f "${VTR_DIR}/.gitmodules" ]; then
    git -C "${VTR_DIR}" submodule update --init --recursive --depth 1
fi

VTR_REAL="$(cd "${VTR_DIR}" && pwd -P)"
VTR_CACHE="${VTR_REAL}/build/CMakeCache.txt"
if [ -f "${VTR_CACHE}" ] && ! grep -Fq "CMAKE_HOME_DIRECTORY:INTERNAL=${VTR_REAL}" "${VTR_CACHE}"; then
    echo "Removing stale VTR build directory configured for another source tree."
    rm -rf "${VTR_REAL}/build"
fi

cmake -S "${VTR_REAL}" -B "${VTR_REAL}/build" -DCMAKE_BUILD_TYPE=release -DCMAKE_CXX_FLAGS="${VTR_CXX_FLAGS}" -DVTR_IPO_BUILD=off
cmake --build "${VTR_REAL}/build" --target vpr -j "${JOBS}"

VPR_PATH="${VTR_REAL}/build/vpr/vpr"
if [ ! -x "${VPR_PATH}" ]; then
    echo "VPR binary was not produced at ${VPR_PATH}" >&2
    exit 1
fi

echo "VPR_BIN=${VPR_PATH}"
echo "VPR_ARCH_XML=${VTR_REAL}/vtr_flow/arch/timing/k6_N10_40nm.xml"
