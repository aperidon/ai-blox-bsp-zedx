#!/bin/bash

DEST_TARGET="$(dirname "$(realpath "$0")")"
REPACK_TARGET=$DEST_TARGET/repack/debian/out
SL_REPACK_PATH="/drivers/stereolabs/"
DEBIAN_ROOT_TARGET=$DEST_TARGET/repack/debian
SL_PATH="/stereolabs/drivers/stereolabs/"

### Get L4T version
V_JETPACK=$(awk '
/X-Jetpack_Base:/ {
    match($0, /L4T([0-9]+)\.([0-9]+)/, a)
    print a[1] "." a[2]
}' "$DEBIAN_ROOT_TARGET/control")

V_JETPACK_NUM="${V_JETPACK//./}"

function usage {
        cat <<EOM
Usage: ./${SCRIPT_NAME} [OPTIONS]
This script builds kernel sources in this directory.
It supports following options.
OPTIONS:
        -h              Displays this help
        -r              Build RT Kernel compatible driver
        -ov             Build overlay 
        -dt             SL camera master device tree
        -pdt            Parent Device Tree (if .dtb build)
		-n 				Package name
EOM
}

# parse input parameters
function parse_input_param {
	while [ $# -gt 0 ]; do
		case ${1} in
			-h)
				usage
				exit 0
				;;
			-r)
				ENABLE_RT=1
				shift 1
				;;
			-ov)
                BUILD_OVERLAY=1
				shift 1
				;;
			-dt)
				SL_DEVICETREE="${2}"
                SL_DEVICETREE_NAME="$(basename "$SL_DEVICETREE")"
				shift 2
				;;
            -pdt)
				PARENT_DEVICETREE="${2}"
                PARENT_DEVICETREE_NAME="$(basename "$PARENT_DEVICETREE")"
                PARENT_DEVICETREE_DIR="$(dirname "$PARENT_DEVICETREE")"
				shift 2
				;;
			-n)
				PACKAGE_NAME="${2}"
				shift 2
				;;
			*)
				echo "Error: Invalid option ${1}"
				usage
				exit 1
				;;
			esac
	done
}

# Call parser with script arguments
parse_input_param "$@"

BUILD_DIR=build_R${V_JETPACK}${RT_SUFFIX}
TEGRA_KERNEL_OUT=$DEST_TARGET/${BUILD_DIR}/

if [[ $V_JETPACK_NUM -gt 360 ]]; then
	KERNEL_VERSION=$(awk '/nvidia-l4t-kernel/ { print $5 }' $DEBIAN_ROOT_TARGET/control)
	IFS='=-'; KERNEL_VERSION=( ${KERNEL_VERSION} ); unset IFS
	if [[ "${KERNEL_VERSION[1]}" =~ ^[0-9]+$ ]]; then
		ABI_SUFFIX="-${KERNEL_VERSION[1]}"
	else
		ABI_SUFFIX=""
	fi
	# Check if the kernel is to be compiled for real-time or standard
	if [[ $ENABLE_RT -eq 1 ]]; then
		# For real-time kernel, include the third component
		MODULES_PATH="/usr/lib/modules/${KERNEL_VERSION[0]}${ABI_SUFFIX}-rt-tegra/kernel/"
	else
		# For standard kernel, use the default path
		MODULES_PATH="/usr/lib/modules/${KERNEL_VERSION[0]}${ABI_SUFFIX}-tegra/kernel/"
	fi
else 
	KERNEL_VERSION=$(awk '/nvidia-l4t-kernel/ { print $4 }' $DEBIAN_ROOT_TARGET/control)
	IFS='=-'; KERNEL_VERSION=( ${KERNEL_VERSION} ); unset IFS
	MODULES_PATH="/usr/lib/modules/${KERNEL_VERSION[1]}-${KERNEL_VERSION[2]}/kernel/"
fi 

echo $MODULES_PATH

rm -r $REPACK_TARGET/boot/stereolabs/* 2> /dev/null || true
rm -R $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/* 2> /dev/null || true
rm -R $REPACK_TARGET/out 2> /dev/null || true
rm -r $REPACK_TARGET/stereolabs-* 2> /dev/null || true

## Reshape the repack directory
mkdir -p $REPACK_TARGET/$MODULES_PATH
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/bmi088
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96712
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max96724
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9296
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedx
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedone4k
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/zedxhdr
mkdir -p $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/max9295
mkdir -p $REPACK_TARGET/usr/sbin/
mkdir -p $REPACK_TARGET/tmp
mkdir -p $REPACK_TARGET/tmp/nvidia-capture-patches
mkdir -p $REPACK_TARGET/boot/stereolabs/
mkdir -p $REPACK_TARGET/boot/stereolabs/utils/
mkdir -p $REPACK_TARGET/var/nvidia/nvcam/settings/
mkdir -p $REPACK_TARGET/etc/modprobe.d/
## Prepare repack directory for deb packages
mkdir -p $REPACK_TARGET/var/nvidia/nvcam/settings/

### Build Daemon
if [[ ! -f "$DEST_TARGET/Daemon/.DAEMON_$V_JETPACK" ]]; then 
	rm $DEST_TARGET/Daemon/.DAEMON_*
	./build_Daemon_CC.sh $V_JETPACK_NUM

	BUILD_DAEMON=$?

	if [ $BUILD_DAEMON -eq 0 ]; then 
		touch $DEST_TARGET/Daemon/.DAEMON_$V_JETPACK 
	fi
fi

source ./build.sh $@

BUILD_SUCESS=$?

if [ $BUILD_SUCESS -eq 0 ]; then

	### Copy necessary DTB files into deb package
	cp  $DEST_TARGET/output/*.dtb* $REPACK_TARGET/boot/stereolabs/

	### Copy extlinux modifications tools into deb package
	cp  -r $DEST_TARGET/extlinux/* $REPACK_TARGET/boot/stereolabs/

	if [[ $L4T_MAJ -gt 38 ]]; then
		SL_PATH="/kernel/stereolabs/drivers/stereolabs/"
	fi

	### Copy drivers into deb package
	for folder in $(ls $TEGRA_KERNEL_OUT/$SL_PATH/)
	do
		if [[ -d $TEGRA_KERNEL_OUT/$SL_PATH/$folder && $(ls $TEGRA_KERNEL_OUT/$SL_PATH/$folder/*.ko) ]]
		then
			cp -R $TEGRA_KERNEL_OUT/$SL_PATH/$folder/*.ko $REPACK_TARGET/$MODULES_PATH/$SL_REPACK_PATH/$folder
		fi
	done

	### Stage NVIDIA patched camera/IVC modules into /tmp for postinst (R36.4 only)
	if [[ "$L4T_MAJ.$L4T_MIN" == "36.4" ]]; then

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
	fi

	### Copy ISP files into deb package
	cp  $DEST_TARGET/ISP/* $REPACK_TARGET/var/nvidia/nvcam/settings/
	cp $DEST_TARGET/Daemon/blacklist-zed.conf $REPACK_TARGET/etc/modprobe.d/

	### Copy ISP patches for R36.4.3
	cp -r $DEST_TARGET/nvidia_364_fix/* $REPACK_TARGET/tmp/
	
	### Copy Daemon bin into deb package
	cp $DEST_TARGET/Daemon/ZEDX_Daemon $REPACK_TARGET/usr/sbin/
	cp $DEST_TARGET/Daemon/ZEDX_Driver $REPACK_TARGET/usr/sbin/
	cp $DEST_TARGET/Daemon/IMU_Daemon $REPACK_TARGET/usr/sbin/

	### Copy Daemon service into deb package. Will be copied by postint scrpt in /etc/systemd/system
	cp $DEST_TARGET/Daemon/zed_x_daemon/zed_x_daemon.service $REPACK_TARGET/tmp/
	cp $DEST_TARGET/Daemon/driver_zed_loader/driver_zed_loader.service $REPACK_TARGET/tmp/
	cp $DEST_TARGET/Daemon/imu-daemon/IMU_Daemon.service $REPACK_TARGET/tmp/
	
	### Get Version
	MYVERSION=$(sed -n 's/^##### v\(.*\)$/\1/p' ${DEST_TARGET}/CHANGELOG.md | head -n 1)
	VERSION="$MYVERSION$RT_SUFFIX"

	if [[ -z "$PACKAGE_NAME" ]]; then 
		PACKAGE_NAME="zedx"
	fi

	sed -i 's/stereolabs-zedx (.*L4T/stereolabs-'$PACKAGE_NAME' ('$VERSION'-L4T/g' $DEBIAN_ROOT_TARGET/changelog
	sed -i 's/stereolabs-zedx/stereolabs-'$PACKAGE_NAME'/g' $DEBIAN_ROOT_TARGET/control

	## Sed preinst with version
	sed -i 's/^PKG_VERSION=""/PKG_VERSION="'$VERSION'"/' $DEBIAN_ROOT_TARGET/preinst
	## ----> Create Deb file <-------
	cd $DEST_TARGET/repack

	## If you're building an overlay, Parent_devicetree will be null and sl_devicetree will be taken into account
	## To setup extlinux. If you're using dtbs, parent_devicetree will be used
	echo "$BUILD_OVERLAY" "${PARENT_DEVICETREE_NAME:+${PARENT_DEVICETREE_NAME%.dts}.dtb}" "${SL_DEVICETREE_NAME:+${SL_DEVICETREE_NAME%.dts}.dtbo}" > $REPACK_TARGET/boot/stereolabs/utils/zlconfig
	if [[ -z "$CI_BUILD" ]]; then
		export CC=aarch64-linux-gnu-gcc
	fi
	dpkg-buildpackage -uc -b -d -a arm64
	cd ..

	## Reset the content of changelog and preinst to avoid git commit issues
	sed -i 's/stereolabs-'$PACKAGE_NAME' (.*L4T/stereolabs-zedx ('0.4.9-MAX96712'-L4T/g' $DEBIAN_ROOT_TARGET/changelog
	sed -i 's/stereolabs-'$PACKAGE_NAME'/stereolabs-zedx/g' $DEBIAN_ROOT_TARGET/control
	sed -i 's/^PKG_VERSION=".*"/PKG_VERSION=""/' $DEBIAN_ROOT_TARGET/preinst

	echo "#######################################################"
   	echo "####################### OK ############################"
   	echo "#######################################################"
else
   	echo "FAIL TO PACKAGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    exit 1
fi