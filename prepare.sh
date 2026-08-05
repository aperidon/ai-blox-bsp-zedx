#!/bin/bash
# Make the kernel for NVIDIA Developer Kit
# Copyright (c) 2016-21 Jetsonhacks
# MIT License
set -e

SUDO_CMD=
if [[ -z ${CI_COMMIT_REF_NAME} ]]; then
    SUDO_CMD=sudo
fi

$SUDO_CMD apt-get update
$SUDO_CMD apt-get install -y flex bison make libssl-dev bc openssl xxd dpkg-dev dh-make git git-lfs rsync cmake wget

DEST_TARGET="$(dirname "$(realpath "$0")")"
COMPILER_NAME=aarch64--glibc--stable-final
COMPILER_TAR=$COMPILER_NAME.tar.gz
echo $DEST_TARGET
## The compiler tarball is no longer stored in this repo. It is a CI tool, so it lives in
## the CI bucket; fetch it when missing or still an unresolved git-lfs pointer.
if [ ! -s Resources/$COMPILER_TAR ] || head -c 12 Resources/$COMPILER_TAR | grep -q "^version http"; then
    mkdir -p Resources
    wget -q -O Resources/$COMPILER_TAR https://download.stereolabs.com/utils/ci/$COMPILER_TAR
    echo "dc038af2769059e15cf8767ca2adc3f830a139d25f67eb19bcd7bbb0539d9988  Resources/$COMPILER_TAR" | sha256sum -c -
fi

## Copy resources to /opt/
$SUDO_CMD mkdir /opt/$COMPILER_NAME/
$SUDO_CMD cp Resources/$COMPILER_TAR /opt/$COMPILER_NAME
cd /opt/$COMPILER_NAME
$SUDO_CMD tar xpf $COMPILER_TAR
echo "installation compiler done"
