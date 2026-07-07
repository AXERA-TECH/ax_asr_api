#!/bin/bash
# Build ax_asr wheel on aarch64.
#
# Usage:
#   bash build_whl.sh [INSTALL_DIR]
#
# INSTALL_DIR defaults to ../install/ax650 when building for AX650,
# or can point to any install directory containing lib/libax_asr_api.a
# and the BSP SDK lib directory.
#
# Prerequisites:
#   - Python >= 3.8, pip, build
#   - pybind11 (pip install pybind11)
#   - CMake >= 3.13
#   - libax_asr_api.a built (run build_ax650.sh first)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_DIR="${1:-$REPO_ROOT/install/ax650}"

AX_ASR_LIB_DIR="$INSTALL_DIR/lib"
AX_ASR_INCLUDE_DIR="$INSTALL_DIR/include"

# Detect BSP library directory from install path
# AX650/AX630C/AX620Q use ax650n_bsp_sdk or ax620e_bsp_sdk
# AX8850 uses axcl_bsp_sdk
if [[ "$INSTALL_DIR" == *"ax8850"* ]]; then
    MSP_LIB_DIR="$REPO_ROOT/axcl_bsp_sdk/out/lib"
else
    MSP_LIB_DIR="$REPO_ROOT/ax650n_bsp_sdk/msp/out/lib"
    if [[ ! -d "$MSP_LIB_DIR" ]]; then
        MSP_LIB_DIR="$REPO_ROOT/ax620e_bsp_sdk/msp/out/arm64_glibc"
    fi
fi

echo "=== Build Configuration ==="
echo "INSTALL_DIR:   $INSTALL_DIR"
echo "AX_ASR_LIB_DIR: $AX_ASR_LIB_DIR"
echo "AX_ASR_INCLUDE_DIR: $AX_ASR_INCLUDE_DIR"
echo "MSP_LIB_DIR:   $MSP_LIB_DIR"
echo

# Verify prerequisites
if [[ ! -f "$AX_ASR_LIB_DIR/libax_asr_api.a" ]]; then
    echo "ERROR: libax_asr_api.a not found in $AX_ASR_LIB_DIR"
    echo "Build the C++ library first: bash build_ax650.sh"
    exit 1
fi

if [[ ! -d "$MSP_LIB_DIR" ]]; then
    echo "ERROR: BSP library directory not found: $MSP_LIB_DIR"
    echo "Run download_bsp.sh first"
    exit 1
fi

# Build wheel
cd "$SCRIPT_DIR"

pip install build -q 2>&1 | tail -1

CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=Release"
    "-DAX_ASR_LIB_DIR=$AX_ASR_LIB_DIR"
    "-DAX_ASR_INCLUDE_DIR=$AX_ASR_INCLUDE_DIR"
    "-DMSP_LIB_DIR=$MSP_LIB_DIR"
)

echo "Building wheel with scikit-build-core..."
python -m build --wheel \
    --config-setting="cmake.define.AX_ASR_LIB_DIR=$AX_ASR_LIB_DIR" \
    --config-setting="cmake.define.AX_ASR_INCLUDE_DIR=$AX_ASR_INCLUDE_DIR" \
    --config-setting="cmake.define.MSP_LIB_DIR=$MSP_LIB_DIR"

WHEEL=$(ls dist/*.whl 2>/dev/null | head -1)
if [[ -n "$WHEEL" ]]; then
    echo
    echo "=== Build successful ==="
    echo "Wheel: $WHEEL"
else
    echo "ERROR: Wheel not found after build"
    exit 1
fi
