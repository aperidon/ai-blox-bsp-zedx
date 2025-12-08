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
$SUDO_CMD apt-get install -y flex bison make libssl-dev bc openssl xxd dpkg-dev dh-make rsync

DEST_TARGET="$(dirname "$(realpath "$0")")"
COMPILER_NAME=aarch64--glibc--stable-final
COMPILER_TAR=$COMPILER_NAME.tar.gz
echo $DEST_TARGET
## Copy resources to /opt/
$SUDO_CMD mkdir /opt/$COMPILER_NAME/
$SUDO_CMD cp Resources/$COMPILER_TAR /opt/$COMPILER_NAME
cd /opt/$COMPILER_NAME
$SUDO_CMD tar xpf $COMPILER_TAR
echo "installation compiler done"
