#!/bin/bash
#
# Native cross-compile for Daemon binaries (no Docker/QEMU)
# Uses the same toolchain as kernel build
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Auto-detect toolchain location and prefix (differs between L4T releases)
# R36.x: src/toolchain/bin/aarch64-buildroot-linux-gnu-*
# R35.x: /opt/aarch64--glibc--stable-final/bin/aarch64-buildroot-linux-gnu-*
# R32.7: src/toolchain/bin/aarch64-linux-gnu-*

TOOLCHAIN_PATH=""
CROSS_PREFIX=""
HOST_ARCH=$(uname -m)
CMAKE_CROSS_ARGS=""

if [ "$HOST_ARCH" = "aarch64" ]; then
    echo "Detected native ARM64 environment. Using system compiler."
    export CC="gcc"
    export CXX="g++"
    export AR="ar"
    export STRIP="strip"
else
    if [[ -x "${SCRIPT_DIR}/src/toolchain/bin/aarch64-buildroot-linux-gnu-gcc" ]]; then
        # R36.x
        TOOLCHAIN_PATH="${SCRIPT_DIR}/src/toolchain/bin"
        CROSS_PREFIX="aarch64-buildroot-linux-gnu-"
    elif [[ -x "${SCRIPT_DIR}/src/toolchain/bin/aarch64-linux-gnu-gcc" ]]; then
        # R32.7
        TOOLCHAIN_PATH="${SCRIPT_DIR}/src/toolchain/bin"
        CROSS_PREFIX="aarch64-linux-gnu-"
    elif [[ -x "/opt/aarch64--glibc--stable-final/bin/aarch64-buildroot-linux-gnu-gcc" ]]; then
        # R35.x (installed by prepare.sh)
        TOOLCHAIN_PATH="/opt/aarch64--glibc--stable-final/bin"
        CROSS_PREFIX="aarch64-buildroot-linux-gnu-"
    else
        echo "ERROR: No cross-compiler found"
        echo "Checked:"
        echo "  - ${SCRIPT_DIR}/src/toolchain/bin/"
        echo "  - /opt/aarch64--glibc--stable-final/bin/"
        echo "Run ./switch_release.sh <version> or ./prepare.sh first"
        exit 1
    fi

    export CC="${TOOLCHAIN_PATH}/${CROSS_PREFIX}gcc"
    export CXX="${TOOLCHAIN_PATH}/${CROSS_PREFIX}g++"
    export AR="${TOOLCHAIN_PATH}/${CROSS_PREFIX}ar"
    export STRIP="${TOOLCHAIN_PATH}/${CROSS_PREFIX}strip"
    
    CMAKE_CROSS_ARGS="-DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY"
fi

echo "Using compiler: ${CC}"
${CC} --version | head -1

OUTPUT_DIR="${SCRIPT_DIR}/Daemon"
mkdir -p "${OUTPUT_DIR}"

# Build IMU_Daemon
echo ""
echo "=== Building IMU_Daemon ==="
IMU_BUILD="${SCRIPT_DIR}/Daemon/imu-daemon/build_native"
rm -rf "${IMU_BUILD}"
mkdir -p "${IMU_BUILD}"
cd "${IMU_BUILD}"

cmake .. \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_AR="${AR}" \
    $CMAKE_CROSS_ARGS

make -j$(nproc)
cp IMU_Daemon "${OUTPUT_DIR}/"
echo "Built: ${OUTPUT_DIR}/IMU_Daemon"

# Build ZEDX_Daemon
echo ""
echo "=== Building ZEDX_Daemon ==="
ZEDX_BUILD="${SCRIPT_DIR}/Daemon/zed_x_daemon/build_native"
rm -rf "${ZEDX_BUILD}"
mkdir -p "${ZEDX_BUILD}"
cd "${ZEDX_BUILD}"

# Determine L4T version from argument or default
L4T_VERSION="${1:-364}"
echo "L4T Version: ${L4T_VERSION}"

cmake .. \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_AR="${AR}" \
    $CMAKE_CROSS_ARGS \
    -DRT_L4T_VERSION="${L4T_VERSION}"

make -j$(nproc)
cp ZEDX_Daemon "${OUTPUT_DIR}/"
echo "Built: ${OUTPUT_DIR}/ZEDX_Daemon"

# Build ZEDX_Driver
echo ""
echo "=== Building ZEDX_Driver ==="
DRIVER_BUILD="${SCRIPT_DIR}/Daemon/driver_zed_loader/build_native"
rm -rf "${DRIVER_BUILD}"
mkdir -p "${DRIVER_BUILD}"
cd "${DRIVER_BUILD}"

cmake .. \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_AR="${AR}" \
    $CMAKE_CROSS_ARGS \
    -DRT_L4T_VERSION="${L4T_VERSION}"

make -j$(nproc)
cp ZEDX_Driver "${OUTPUT_DIR}/"
echo "Built: ${OUTPUT_DIR}/ZEDX_Driver"

# Verify binaries are ARM64
echo ""
echo "=== Verifying binaries ==="
# file "${OUTPUT_DIR}/IMU_Daemon"
file "${OUTPUT_DIR}/ZEDX_Daemon"
file "${OUTPUT_DIR}/ZEDX_Driver"

echo ""
echo "=== Build complete ==="
echo "Binaries in: ${OUTPUT_DIR}/"
