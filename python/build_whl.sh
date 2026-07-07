#!/bin/bash
# Build ax_asr wheel on aarch64.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_DIR="${1:-$REPO_ROOT/install/ax650}"

AX_ASR_LIB_DIR="$INSTALL_DIR/lib"
AX_ASR_INCLUDE_DIR="$INSTALL_DIR/include"

# Detect BSP library directory and platform-specific libs
if [[ "$INSTALL_DIR" == *"ax8850"* ]]; then
    MSP_LIB_DIR="$REPO_ROOT/axcl_bsp_sdk/out/lib"
    MSP_LIBS='axcl_rt;axcl_pkg;axcl_comm;axcl_npu;spdlog;axcl_token;axcl_pcie_msg;axcl_pcie_dma'
elif [[ "$INSTALL_DIR" == *"ax650"* ]]; then
    MSP_LIB_DIR="$REPO_ROOT/ax650n_bsp_sdk/msp/out/lib"
    MSP_LIBS='ax_sys;ax_engine;ax_interpreter;ax_dmadim'
elif [[ "$INSTALL_DIR" == *"ax630c"* ]]; then
    MSP_LIB_DIR="$REPO_ROOT/ax620e_bsp_sdk/msp/out/arm64_glibc"
    MSP_LIBS='ax_sys;ax_engine;ax_interpreter'
elif [[ "$INSTALL_DIR" == *"ax620q"* ]]; then
    MSP_LIB_DIR="$REPO_ROOT/ax620e_bsp_sdk/msp/out/arm_uclibc"
    MSP_LIBS='ax_sys;ax_engine;ax_interpreter'
else
    echo "ERROR: unknown platform in INSTALL_DIR: $INSTALL_DIR"
    exit 1
fi

echo "=== Build Configuration ==="
echo "INSTALL_DIR:   $INSTALL_DIR"
echo "AX_ASR_LIB_DIR: $AX_ASR_LIB_DIR"
echo "MSP_LIB_DIR:   $MSP_LIB_DIR"
echo "MSP_LIBS:      $MSP_LIBS"
echo

if [[ ! -f "$AX_ASR_LIB_DIR/libax_asr_api.a" ]]; then
    echo "ERROR: libax_asr_api.a not found in $AX_ASR_LIB_DIR"
    exit 1
fi
if [[ ! -d "$MSP_LIB_DIR" ]]; then
    echo "ERROR: BSP library directory not found: $MSP_LIB_DIR"
    exit 1
fi

cd "$SCRIPT_DIR"
pip install build -q 2>&1 | tail -1

echo "Building wheel..."
python -m build --wheel \
    --config-setting="cmake.define.AX_ASR_LIB_DIR=$AX_ASR_LIB_DIR" \
    --config-setting="cmake.define.AX_ASR_INCLUDE_DIR=$AX_ASR_INCLUDE_DIR" \
    --config-setting="cmake.define.MSP_LIB_DIR=$MSP_LIB_DIR" \
    --config-setting="cmake.define.MSP_LIBS=$MSP_LIBS"

WHEEL=$(ls dist/*.whl 2>/dev/null | head -1)
if [[ -n "$WHEEL" ]]; then
    echo "=== Build successful ==="
    echo "Wheel: $WHEEL"
else
    echo "ERROR: Wheel not found"
    exit 1
fi
