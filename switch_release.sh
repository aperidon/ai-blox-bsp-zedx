#!/bin/bash

## This script allows the user to easily switch between L4T release and modify
## the DTS/drivers only once. Usage is as follow:
## $./switch_release.sh R35.1 # switch to L4T R35.1
## $./switch_release.sh R36.5 # switch to L4T R36.5 (downloads sources automatically)
## $./switch_release.sh R36.5 --skip-download # switch without re-downloading sources
##
## The script works in 3 steps:
## 1/ Retrieve the release's kernel sources (from archive or download)
## 2/ Unpack them in src, along with Stereolabs driver sources
## 3/ Patch them to compile Stereolabs' BSP
set -e

FORCE_DOWNLOAD=false
RELEASE=""

## Parse arguments
for arg in "$@"; do
	case "$arg" in
		--force-download)
			FORCE_DOWNLOAD=true
			;;
		-*)
			echo "Unknown option: $arg"
			echo "Usage: $0 [RELEASE] [--force-download]"
			exit 1
			;;
		*)
			if [[ -z "$RELEASE" ]]; then
				RELEASE="$arg"
			else
				echo "Unexpected argument: $arg"
				echo "Usage: $0 [RELEASE] [--skip-download]"
				exit 1
			fi
			;;
	esac
done

if [[ -z $RELEASE ]]
then
	echo "Please precise L4T release you want to setup: R35.{1,2,3,4} / R36.{3,4,5} / R38.{2,4} / R39.2"
	exit 1
fi

REL=$(ls nvidia_kernel/kernel_patches/)

for r in ${REL[@]}
do
	if [[ $RELEASE == $r ]]
	then
		echo "Setting up L4T ${r}"
		VALID_PARAM=true
	fi
done

if [[ -z $VALID_PARAM ]]
then
	echo "You asked for an invalid release"
	echo "Check avail. release in nvidia_kernel/kernel_patches"
	exit 1
fi

chmod -R u+w src/ 2>/dev/null || true

## Now we know the parsed entry is valid, let's remove the old sources
rm ./.L4T_* 2> /dev/null || true
for f in $(cat .gitignore)
do
	IFS='/'; is_src=( ${f} ); unset IFS
	if [[ ${is_src[0]} == "src" ]]
	then
		rm -r ${f} 2> /dev/null || true
	fi
done
rm -r repack 2> /dev/null || true

git restore repack

if [[ $FORCE_DOWNLOAD == true || ! -f "nvidia_kernel/src_archives/kernel_${RELEASE}.tar" ]]; then
	echo "Downloading NVIDIA kernel sources for ${RELEASE}..."
	bash download_nvidia_src.sh "$RELEASE"
else
	echo "Sources archive already exists for ${RELEASE}, skipping download."
fi

## Unpack the kernel sources into src/
tar -xf "nvidia_kernel/src_archives/kernel_${RELEASE}.tar" -C src/

## Now we just need to patch them
for patch in $(find nvidia_kernel/kernel_patches/${RELEASE}/ -name "0*.patch" -o -name "0*.diff" | sort -n)
do
	# Apply only if it still cleanly applies; otherwise assume already applied
	if git apply --check "$patch" >/dev/null 2>&1; then
		echo "Applying patch: $patch"
		git apply "$patch"
	else
		echo "Skipping already-applied or conflicting patch: $patch"
	fi
done

touch ./.L4T_${RELEASE}_READY