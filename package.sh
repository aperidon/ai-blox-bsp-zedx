#!/bin/bash
# Make the kernel for NVIDIA Developer Kit
# Copyright (c) 2021-23 Stereolabs
# MIT License

# set -e

DEST_TARGET="$(dirname "$(realpath "$0")")"
DEBIAN_ROOT_TARGET=$DEST_TARGET/repack/debian
SRC_ROOT_TARGET=$DEST_TARGET/sources
## For JP5.1.2 --> use 5.10.120
## For JP6.0 --> use 5.10.136
MODULES_PATH=/usr/lib/modules/5.15.148-tegra/kernel/
SL_PATH="/stereolabs/drivers/stereolabs/"
SL_REPACK_PATH="/drivers/stereolabs/"
REPACK_TARGET=$DEST_TARGET/repack/debian/out
ZEDLINK_NAME="ai-blox"
OVERLAY_PATH=("$DEST_TARGET/sources/hardware/stereolabs/AIBLOX")
ZEDX_INC=$SRC_ROOT_TARGET/kernel/stereolabs/drivers/stereolabs/zedx/zedx_mode_tbls.h
NVIDIA_DT_ROOT=$DEST_TARGET/sources/hardware/nvidia/
DTBO_NAME=tegra234-p3768-camera-ai-blox-sl-overlay.dtbo

mkdir -p $DEST_TARGET/build_R364/
mkdir -p $DEST_TARGET/output/

rm -r $DEST_TARGET/output/* || true

mkdir -p $REPACK_TARGET/$MODULES_PATH
mkdir -p $REPACK_TARGET/$MODULES_PATH/$IMU_PATH
rm -R $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/* || true
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96712
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96724
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9296
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedx
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedone4k
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedxpro
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9295
mkdir -p $REPACK_TARGET/boot/stereolabs/
mkdir -p $REPACK_TARGET/var/nvidia/nvcam/settings/

echo $DEST_TARGET

### Create directory in repack for daemon 
mkdir -p $REPACK_TARGET/usr/sbin/
mkdir -p $REPACK_TARGET/tmp/
mkdir -p $REPACK_TARGET/etc/modprobe.d/

### Remove existing files
rm  $REPACK_TARGET/usr/sbin/ZEDX_Daemon || true
rm  $REPACK_TARGET/boot/tegra*.dtbo || true

### Build Daemon
### Get L4T version
V_JETPACK=$(awk '/X-Jetpack_Base:/ { print $2 }' $DEBIAN_ROOT_TARGET/control)
#./build_Daemon.sh $V_JETPACK

### Copy Daemon bin in repack
cp $DEST_TARGET/Daemon/ZEDX_Daemon $REPACK_TARGET/usr/sbin/
### Copy Daemon service in repack
cp $DEST_TARGET/Daemon/zed_x_daemon.service $REPACK_TARGET/tmp/
cp $DEST_TARGET/Daemon/blacklist-zed.conf $REPACK_TARGET/etc/modprobe.d/

cd sources

export TEGRA_KERNEL_OUT=$DEST_TARGET/${BUILD_DIR}/
# For R36.2 and onward$

if [[ -z "$CI_BUILD" ]]; then
	export CROSS_COMPILE=$DEST_TARGET/sources/toolchain/bin/aarch64-buildroot-linux-gnu-
fi

export TEGRA_KERNEL_OUT=$DEST_TARGET/build_R364/
export ARCH=arm64
export LOCALVERSION=-tegra

rm $TEGRA_KERNEL_OUT/$SL_PATH/max96712/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/max96724/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9296/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedx/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedone4k/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedxpro/*
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9295/*

cp ${OVERLAY_PATH}/* ${NVIDIA_DT_ROOT}/t23x/nv-public/overlay/

### Modify defconfig accordingly

echo "###  build MAX96724 ###"

sed -i "s/export CONFIG_SL_DESER_.*/export CONFIG_SL_DESER_MAX96724=m/g" ${SRC_ROOT_TARGET}/kernel/stereolabs/Makefile

rm -r $REPACK_TARGET/boot/stereolabs/* || true

./nvbuild.sh -o $TEGRA_KERNEL_OUT


if [ $? -eq 0 ]; then
	echo "BUILD success"
	## Copy needed
	
	## Image
	cp $TEGRA_KERNEL_OUT/kernel/kernel-jammy-src/arch/arm64/boot/Image $DEST_TARGET/output/
	
 	cp $TEGRA_KERNEL_OUT/kernel-devicetree/generic-dts/dtbs/$DTBO_NAME $DEST_TARGET/output/

	### Copy overlays in repack
	cp $DEST_TARGET/output/$DTBO_NAME $REPACK_TARGET/boot/

	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedx/*.ko $DEST_TARGET/output/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/max96724/*.ko $DEST_TARGET/output/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/max9295/*.ko $DEST_TARGET/output/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedone4k/*.ko $DEST_TARGET/output/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedxpro/*.ko $DEST_TARGET/output/
	
	cp -R $DEST_TARGET/ISP/* $DEST_TARGET/output/
 
	### Copy the specific extlinux-xxxx.conf into deb package
	### --> The correct extlinux will be taken during the deb installation by parsing the ls /boot/kernel_xxx name used (see posint)
	cp  -r $DEST_TARGET/extlinux/* $REPACK_TARGET/boot/stereolabs/
	cp  -r $DEST_TARGET/output/Image $REPACK_TARGET/boot/stereolabs/
	
	### Copy drivers in repack
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedx/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedx/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/max96724/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96724/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/max9295/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9295/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedone4k/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedone4k/
	cp -R $TEGRA_KERNEL_OUT/$SL_PATH/zedxpro/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedxpro/


	### Copy ISP files in repack
	cp  $DEST_TARGET/ISP/* $REPACK_TARGET/var/nvidia/nvcam/settings/

	cp -r $DEST_TARGET/nvidia_364_fix/* $REPACK_TARGET/tmp/

	# ## Modify modules.order to have the correct path
	# while read p; do
	# 	sed -i "s+$p+kernel/$p+" $REPACK_TARGET/$MODULES_PATH/../modules.order
	# done <$REPACK_TARGET/$MODULES_PATH/../modules.order

	### Get Version
	V_MAJOR=$(awk '/ZEDX_DRIVER_VERSION_MAJOR/ { print $3 }' $ZEDX_INC)
	V_MINOR=$(awk '/ZEDX_DRIVER_VERSION_MINOR/ { print $3 }' $ZEDX_INC)
	V_PATCH=$(awk '/ZEDX_DRIVER_VERSION_PATCH/ { print $3 }' $ZEDX_INC)

	## Modify version in changelog so that it get reported on the deb name
	### Use VERSION and EP/not EP parameter
	DRIVER_VERSION="$V_MAJOR.$V_MINOR.$V_PATCH-$1"
	sed -i 's/stereolabs-zedx (.*L4T/stereolabs-zedx ('$DRIVER_VERSION'L4T/g' $DEBIAN_ROOT_TARGET/changelog

	## ----> Create Deb file <-------
	cd $DEST_TARGET/repack
	echo "$DTBO_NAME" > $REPACK_TARGET/boot/stereolabs/utils/zlconfig
	export CC=aarch64-linux-gnu-gcc
	dpkg-buildpackage -uc -b -d -a arm64
	cd ..

	echo "#######################################################"
   	echo "####################### OK ############################"
   	echo "#######################################################"
else
   	echo "FAIL TO BUILD !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
   	exit 1
fi

