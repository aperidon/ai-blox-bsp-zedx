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
CAM_MODES_PATH=("$DEST_TARGET/sources/hardware/stereolabs/Utils")
ZEDX_INC=$SRC_ROOT_TARGET/kernel/stereolabs/drivers/stereolabs/zedx/zedx_mode_tbls.h
NVIDIA_DT_ROOT=$DEST_TARGET/sources/hardware/nvidia/
DTBO_NAME=tegra234-p3768-camera-ai-blox-sl-overlay.dtbo

mkdir -p $DEST_TARGET/build_R364/
mkdir -p $DEST_TARGET/output/

rm -r $DEST_TARGET/output/* || true

mkdir -p $REPACK_TARGET/$MODULES_PATH
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/bmi088
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96712
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96724
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9296
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedx
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedone4k
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedxhdr
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9295
mkdir -p $REPACK_TARGET/tmp/nvidia-capture-patches
mkdir -p $REPACK_TARGET/boot/stereolabs/
mkdir -p $REPACK_TARGET/var/nvidia/nvcam/settings/

echo $DEST_TARGET

### Create directory in repack for daemon 
mkdir -p $REPACK_TARGET/usr/sbin/
mkdir -p $REPACK_TARGET/tmp/
mkdir -p $REPACK_TARGET/etc/modprobe.d/

### Remove existing files
rm  $REPACK_TARGET/usr/sbin/ZEDX_Daemon 2> /dev/null || true
rm  $REPACK_TARGET/usr/sbin/ZEDX_Driver 2> /dev/null || true
rm  $REPACK_TARGET/usr/sbin/IMU_Daemon 2> /dev/null || true
rm  $REPACK_TARGET/boot/tegra*.dtbo || true

### Build Daemon
### Get L4T version
V_JETPACK=$(awk '/X-Jetpack_Base:/ { print $2 }' $DEBIAN_ROOT_TARGET/control)
V_JETPACK_NUM=$(echo "$V_JETPACK" | sed -E 's/^L4T([0-9]+)\.([0-9]+)(\.[0-9]+)?$/\1\2/')
./build_Daemon_CC.sh $V_JETPACK_NUM


### Copy Daemon bin in repack
cp $DEST_TARGET/Daemon/ZEDX_Daemon $REPACK_TARGET/usr/sbin/
cp $DEST_TARGET/Daemon/ZEDX_Driver $REPACK_TARGET/usr/sbin/
cp $DEST_TARGET/Daemon/IMU_Daemon $REPACK_TARGET/usr/sbin/

### Copy Daemon service in repack/tmp directory. Will be copied by postint scrpt in /etc/systemd/system
cp $DEST_TARGET/Daemon/zed_x_daemon/zed_x_daemon.service $REPACK_TARGET/tmp/
cp $DEST_TARGET/Daemon/driver_zed_loader/driver_zed_loader.service $REPACK_TARGET/tmp/
cp $DEST_TARGET/Daemon/imu-daemon/IMU_Daemon.service $REPACK_TARGET/tmp/

### Copy Daemon service in repack
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

rm $TEGRA_KERNEL_OUT/$SL_PATH/max96712/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max96724/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9296/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedx/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedone4k/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedxhdr/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9295/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/bmi088* 2> /dev/null || true

cp ${OVERLAY_PATH}/* ${NVIDIA_DT_ROOT}/t23x/nv-public/overlay/
cp -r ${CAM_MODES_PATH} ${NVIDIA_DT_ROOT}/t23x/nv-public/overlay/

### Modify defconfig accordingly

echo "###  build MAX96724 ###"

sed -i "s/export CONFIG_SL_DESER_.*/export CONFIG_SL_DESER_MAX96724=m/g" ${SRC_ROOT_TARGET}/kernel/stereolabs/Makefile

rm -r $REPACK_TARGET/boot/stereolabs/* || true

./nvbuild.sh -o $TEGRA_KERNEL_OUT


if [ $? -eq 0 ]; then
	echo "BUILD success"
	## Copy needed
	
	## Image
	#cp $TEGRA_KERNEL_OUT/kernel/kernel-jammy-src/arch/arm64/boot/Image $DEST_TARGET/output/
	
 	cp $TEGRA_KERNEL_OUT/kernel-devicetree/generic-dts/dtbs/$DTBO_NAME $DEST_TARGET/output/

	for module in $(find $TEGRA_KERNEL_OUT/$SL_PATH -name "*.ko")
	do
		cp -R $module $DEST_TARGET/output/
	done

	cp -R $DEST_TARGET/ISP/* $DEST_TARGET/output/
 
	### Copy the specific extlinux-xxxx.conf into deb package
	### --> The correct extlinux will be taken during the deb installation by parsing the ls /boot/kernel_xxx name used (see posint)
	cp  -r $DEST_TARGET/extlinux/* $REPACK_TARGET/boot/stereolabs/
	cp  -r $DEST_TARGET/output/Image $REPACK_TARGET/boot/stereolabs/

	### Copy overlays in repack
	cp $DEST_TARGET/output/$DTBO_NAME $REPACK_TARGET/boot/
	
	### Copy drivers in repack
	for folder in $(ls $TEGRA_KERNEL_OUT/$SL_PATH/)
	do
		if [[ -n $DEBUG ]]
		then
			echo "$TEGRA_KERNEL_OUT/$SL_PATH/$folder/"
			echo $(ls $TEGRA_KERNEL_OUT/$SL_PATH/$folder/)
		fi
		if [[ -d $TEGRA_KERNEL_OUT/$SL_PATH/$folder && $(ls $TEGRA_KERNEL_OUT/$SL_PATH/$folder/*.ko) ]]
		then
			cp -R $TEGRA_KERNEL_OUT/$SL_PATH/$folder/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/$folder
		fi
	done

	### Copy ISP files in repack
	cp  $DEST_TARGET/ISP/* $REPACK_TARGET/var/nvidia/nvcam/settings/

	## Add nvidia kernel patch
	cp -r $DEST_TARGET/nvidia_364_fix/* $REPACK_TARGET/tmp/

	NV_OOT_DIR="$DEST_TARGET/${BUILD_DIR}/nvidia-oot"

	CAPTURE_IVC_SRC="$NV_OOT_DIR/drivers/platform/tegra/rtcpu/capture-ivc.ko"
	NVHOST_VI5_SRC="$NV_OOT_DIR/drivers/video/tegra/host/vi/nvhost-vi5.ko"

	CAPTURE_IVC_TMP="$REPACK_TARGET/tmp/nvidia-capture-patches/capture-ivc.ko"
	NVHOST_VI5_TMP="$REPACK_TARGET/tmp/nvidia-capture-patches/nvhost-vi5.ko"

	if [[ -f "$CAPTURE_IVC_SRC" ]]; then
		cp -v "$CAPTURE_IVC_SRC" "$CAPTURE_IVC_TMP"
	else
		echo "Warning: capture-ivc.ko not found at $CAPTURE_IVC_SRC"
	fi

	if [[ -f "$NVHOST_VI5_SRC" ]]; then
		cp -v "$NVHOST_VI5_SRC" "$NVHOST_VI5_TMP"
	else
		echo "Warning: nvhost-vi5.ko not found at $NVHOST_VI5_SRC"
	fi


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

