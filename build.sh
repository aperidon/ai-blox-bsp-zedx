#!/bin/bash
# Make the kernel for NVIDIA Developer Kit
# Copyright (c) 2016-21 Jetsonhacks
# MIT License


BUILD_OVERLAY=0
ENABLE_RT=0

DEST_TARGET="$(dirname "$(realpath "$0")")"
SRC_ROOT=${DEST_TARGET}/src
NVIDIA_DT_ROOT=$DEST_TARGET/src/hardware/nvidia
DEBIAN_ROOT_TARGET=$DEST_TARGET/repack/debian
# Correspondance between Nvidia's and Stereolabs' BSP
SL_DT_PATH=("$DEST_TARGET/src/hardware/stereolabs")
CAM_MODES_PATH=("$DEST_TARGET/src/hardware/stereolabs/Utils")
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

function reset_build {
    ##Reset Makefiles
    mv ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/overlay/Makefile.sl-bkp  ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/overlay/Makefile 2> /dev/null || true
    mv $SL_DT_PATH/Makefile.sl-bkp  $SL_DT_PATH/Makefile 2> /dev/null || true
    mv ${PARENT_DEVICETREE}.sl-bkp ${PARENT_DEVICETREE} 2> /dev/null || true
    mv $NVIDIA_DT_ROOT/$DT_SUB_PATH/nv-platform/Makefile.sl-bkp $NVIDIA_DT_ROOT/$DT_SUB_PATH/nv-platform/Makefile 2> /dev/null || true
    sed -i "s/export CONFIG_SL_DESER_.*/export CONFIG_SL_DESER_MAX96712=m/g" ${SRC_ROOT}/kernel/stereolabs/drivers/Makefile
}


# Call parser with script arguments
parse_input_param "$@"

V_JETPACK=$(awk '
/X-Jetpack_Base:/ {
    match($0, /L4T([0-9]+)\.([0-9]+)/, a)
    print a[1] "." a[2]
}' "$DEBIAN_ROOT_TARGET/control")
V_JETPACK_NUM="${V_JETPACK//./}"
echo "🚀 Current Jetpack Version: $V_JETPACK 🚀"

if [[ $V_JETPACK_NUM -gt 390 ]]; then
    SL_PATH="/kernel/stereolabs/drivers/stereolabs/"
elif [[ $V_JETPACK_NUM -gt 360 ]]; then
    SL_PATH="/stereolabs/drivers/stereolabs/"
else
    SL_PATH="/drivers/stereolabs/"
fi

# Check for real-time kernel build (explicit "rt" or "RT" required)
if [[ $ENABLE_RT -eq 1 ]]
then
	echo "Building for real-time kernel"
    RT_FLAG="-r"
    RT_SUFFIX="-rt"
else
	echo "Building for non-real-time kernel"
    RT_FLAG=""
    RT_SUFFIX=""
fi


BUILD_DIR=build_R${V_JETPACK}${RT_SUFFIX}


## Prepare build and output directories
## Build is for kernel binaries
## Output is where we cp binaries of interest for us
mkdir -p $DEST_TARGET/${BUILD_DIR}/
mkdir -p $DEST_TARGET/output/

## Get Deserializer model from device tree 
DESER=$(grep -o -m1 -E 'max9(6712|296|6724)' $SL_DEVICETREE)
DSR=${DESER^^}

export TEGRA_KERNEL_OUT=$DEST_TARGET/${BUILD_DIR}/

prefix=${SL_DEVICETREE_NAME%%-*}
if [[ $prefix == "tegra264" ]]; then
    DT_SUB_PATH="t264/nv-public"
elif [[ $prefix == "tegra234" ]]; then
    DT_SUB_PATH="t23x/nv-public"
elif [[ $prefix == "tegra194" ]]; then
    DT_SUB_PATH="t19x/nv-public"
fi

## Reset old build 
reset_build
## Clean the compiled binaries
rm $TEGRA_KERNEL_OUT/$SL_PATH/max96712/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max96724/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9296/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedx/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedone4k/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/zedxhdr/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/max9295/* 2> /dev/null || true
rm $TEGRA_KERNEL_OUT/$SL_PATH/bmi088* 2> /dev/null || true

## Remove stereolabs DT
rm ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/*-sl-* 2> /dev/null || true

## Remove the result of a previous compilation
rm -r $DEST_TARGET/output/* 2> /dev/null  || true

## Prepare DTBO build
if [[ BUILD_OVERLAY -eq 1 ]]; then

    if [[ "${V_JETPACK%%.*}" == "35" ]]; then
        echo "Overlays not supported under JetPack 5" 
        reset_build
        exit 1
    fi

    cp ${NVIDIA_DT_ROOT}/t23x/nv-public/overlay/Makefile ${NVIDIA_DT_ROOT}/t23x/nv-public/overlay/Makefile.sl-bkp
    cp $SL_DT_PATH/Makefile $SL_DT_PATH/Makefile.sl-bkp

    sed -i "s|//sl_placeholder|dtbo-y += ${SL_DEVICETREE_NAME%.dts}.dtbo|g" "$SL_DT_PATH/Makefile"
    sed -i "s|makefile-path := .*/overlay|makefile-path := ${DT_SUB_PATH}/overlay|" "$SL_DT_PATH/Makefile"
    
    sed -i "s|#DTC_CPP_FLAGS += -DBUILDOVERLAY|DTC_CPP_FLAGS += -DBUILDOVERLAY|g" "$SL_DT_PATH/Makefile"
    cp -r ${CAM_MODES_PATH} ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/overlay
    cp ${SL_DT_PATH}/tegra* ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/overlay
    cp ${SL_DT_PATH}/Makefile ${NVIDIA_DT_ROOT}/${DT_SUB_PATH}/overlay

## Prepare DTB build
else

    cp ${PARENT_DEVICETREE} ${PARENT_DEVICETREE}.sl-bkp
    if [[ $SL_DEVICETREE_NAME =~ \.(dts)$ ]]; then
        sed -i "$(grep -n '^#include' "$PARENT_DEVICETREE" | tail -1 | cut -d: -f1)a #include \"${SL_DEVICETREE_NAME%.dts}.dtsi\"" "$PARENT_DEVICETREE"
    else 
        sed -i "$(grep -n '^#include' "$PARENT_DEVICETREE" | tail -1 | cut -d: -f1)a #include \"${SL_DEVICETREE_NAME}\"" "$PARENT_DEVICETREE"
    fi
    cp $PARENT_DEVICETREE_DIR/Makefile $PARENT_DEVICETREE_DIR/Makefile.sl-bkp
    sed -i '$a\override DTC_CPP_FLAGS := $(filter-out -DBUILDOVERLAY,$(DTC_CPP_FLAGS))' $PARENT_DEVICETREE_DIR/Makefile
    cp -r ${CAM_MODES_PATH} "$(dirname "$PARENT_DEVICETREE")"

    if [[ $SL_DEVICETREE_NAME =~ \.(dts)$ ]]; then
        mv $SL_DEVICETREE "${SL_DEVICETREE%.dts}.dtsi"
    fi

    cp ${SL_DT_PATH}/tegra* "$(dirname "$PARENT_DEVICETREE")"

fi

echo ""
echo "###  Build SL_DESER_$DSR ###"
echo ""

cd src

## Modify defconfig according to the driver you want to compile
sed -i "s/export CONFIG_SL_DESER_.*/export CONFIG_SL_DESER_${DSR}=m/g" ${SRC_ROOT}/kernel/stereolabs/drivers/Makefile

# For R36.2 and onward$
if [[ -z "$CI_BUILD" ]]; then
	export CROSS_COMPILE=${DEST_TARGET}/src/toolchain/bin/aarch64-buildroot-linux-gnu-
    export CROSS_COMPILE_AARCH64_PATH=/opt/aarch64--glibc--stable-final
fi

export ARCH=arm64
export LOCALVERSION=-tegra


# BUILD THE KERNEL!!!
if [[ -f ${TEGRA_KERNEL_OUT}/.BUILD_ONCE && -z ${COMPILE_ALL_MODULES} ]]
then
	# Here you build only SL OOT sources. Please note that any modifications
	# done on nvidia-oot, or other sources, will be skipped here.
	echo "######## COMPILING ONLY SL MODULES ########"
	export KERNEL_HEADERS=${TEGRA_KERNEL_OUT}/kernel/kernel-jammy-src
	./nvbuild.sh -mr ${RT_FLAG} -o ${TEGRA_KERNEL_OUT}
elif [[ -f ${TEGRA_KERNEL_OUT}/.BUILD_ONCE && -n ${COMPILE_ALL_MODULES} ]]
then
	# In case you want to recompile NVIDIA OOT and stuff, you must set
	# COMPILE_ALL_MODULES in your environment. Just using the default 'else'
	# condition does almost the same.
	echo "######## COMPILING ALL MODULES ########"
	export KERNEL_HEADERS=${TEGRA_KERNEL_OUT}/kernel/kernel-jammy-src
	./nvbuild.sh -m ${RT_FLAG} -o ${TEGRA_KERNEL_OUT}
else
	# If it is the first time you compile, just do it from scratch
	echo "######## COMPILING FROM SCRATCH ########"
	./nvbuild.sh ${RT_FLAG} -o  ${TEGRA_KERNEL_OUT}
fi

NVSUCCESS=$?

## Copy compiled binaries and build to output
if [ $NVSUCCESS -eq 0 ]; then
	echo "BUILD success"
	## Copy needed
    if [[ $V_JETPACK_NUM -gt 360 ]]; then
	    cp $TEGRA_KERNEL_OUT/kernel/kernel-jammy-src/arch/arm64/boot/Image $DEST_TARGET/output/
        if [[ $V_JETPACK_NUM -lt 364 ]]; then
            DTB_PATH="/nvidia-oot/device-tree/platform/generic-dts/dtbs"
        elif [[ $V_JETPACK_NUM -lt 390 ]]; then
            DTB_PATH="/kernel-devicetree/generic-dts/dtbs"
        else #For 39.2 and above, the DTB path has changed
	        DTB_PATH="/build/nvidia-public/devicetree/generic-dtbs"
        fi
	else 
        cp $TEGRA_KERNEL_OUT/arch/arm64/boot/Image  $DEST_TARGET/output/
        DTB_PATH="$TEGRA_KERNEL_OUT/arch/arm64/boot/dts/nvidia/"
    fi

    if [[ BUILD_OVERLAY -eq 1 ]]; then 
	    cp $TEGRA_KERNEL_OUT/$DTB_PATH/*-sl-overlay.dtbo $DEST_TARGET/output/
    else
	    cp $TEGRA_KERNEL_OUT/$DTB_PATH/${PARENT_DEVICETREE_NAME%.dts}.dtb $DEST_TARGET/output/
    fi

	for module in $(find $TEGRA_KERNEL_OUT/$SL_PATH -name "*.ko")
	do
		cp -R $module $DEST_TARGET/output/
	done

    reset_build
    echo "#######################################################"
   	echo "####################### OK ############################"
   	echo "#######################################################"

else
   	echo "FAIL TO BUILD !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	reset_build
    exit 1
fi