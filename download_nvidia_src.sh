#!/usr/bin/env bash
set -e

RELEASE=$1

if [[ -z $RELEASE ]]
then
	echo "Please precise L4T release you want to setup: R32.7 / R35.{1,2,3,4} / R36.{2,3,4,5} / R38.{2,4} / R39.2"
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

if [[ -d "nvidia_kernel/src_archives/${RELEASE}" ]]
then
    rm -r nvidia_kernel/src_archives/${RELEASE}
fi

if [[ -f "nvidia_kernel/src_archives/kernel_${RELEASE}.tar" ]]
then
    rm nvidia_kernel/src_archives/kernel_${RELEASE}.tar
fi

mkdir -p nvidia_kernel/src_archives/$RELEASE
TARGET_DW_PATH="nvidia_kernel/src_archives/$RELEASE"

SRC_ARCHIVE=public_sources.tbz2
TOOLCHAIN_ARCHIVE=aarch64--glibc--stable-2022.08-1.tar.bz2

## Upstream NVIDIA location and sha256 for every release we can build. This replaces the
## elif chain: the same release list was repeated further down to pick the archive layout,
## and the two copies drifted -- which is how R36.2 became downloadable but not
## extractable. One table plus one layout predicate (needs_toolchain), no duplication.
declare -A SRC_URL SRC_SHA

SRC_URL[R32.7]="https://developer.download.nvidia.com/embedded/L4T/r32_Release_v7.1/Sources/T186/public_sources.tbz2"
SRC_URL[R35.1]="https://developer.nvidia.com/embedded/l4t/r35_release_v1.0/sources/public_sources.tbz2"
SRC_URL[R35.2]="https://developer.download.nvidia.com/embedded/L4T/r35_Release_v2.1/sources/public_sources.tbz2"
SRC_URL[R35.3]="https://developer.nvidia.com/downloads/embedded/l4t/r35_release_v3.1/sources/public_sources.tbz2"
SRC_URL[R35.4]="https://developer.nvidia.com/downloads/embedded/l4t/r35_release_v4.1/sources/public_sources.tbz2"
SRC_URL[R36.2]="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v2.0/sources/public_sources.tbz2"
SRC_URL[R36.3]="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v3.0/sources/public_sources.tbz2"
SRC_URL[R36.4]="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v4.3/sources/public_sources.tbz2"
SRC_URL[R36.5]="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v5.0/sources/public_sources.tbz2"
SRC_URL[R38.2]="https://developer.nvidia.com/downloads/embedded/L4T/r38_Release_v2.1/sources/public_sources.tbz2"
SRC_URL[R38.4]="https://developer.nvidia.com/downloads/embedded/L4T/r38_Release_v4.0/source/public_sources.tbz2"
SRC_URL[R39.2]="https://developer.nvidia.com/downloads/embedded/L4T/r39_Release_v2.0/sources/public_sources.tbz2"

SRC_SHA[R32.7]="3f551de576e0eb0397a8679aed760e53433fbd9c90b1d008caae364f3b7569f9"
SRC_SHA[R35.1]="1d955ecafe65c69526b79042e7dcd0bdae8ef32e75b6262287094d91796c8f6a"
SRC_SHA[R35.2]="ae9d2f903347013a915b128cf311899a24c6ba21e13607cdbde785e1f0557449"
SRC_SHA[R35.3]="cd914110043cdb2a19a298fefc52d9dacbbcd560f781955fe03a1e98b470f2ae"
SRC_SHA[R35.4]="cad6179ae16cc23720dc019f3a36f054df5d153ce833f45cf1bd09a376f5442c"
SRC_SHA[R36.2]="d1dae4cddc5e6055e988bbe79e6ea23a90b3c8fe515e564e6347e391e2a6b184"
SRC_SHA[R36.3]="190f499f5cff26f62a911d25a8e4c384db613a1887cbce17bcbe5810be0f36c9"
SRC_SHA[R36.4]="2c177804679e3ed650dabec6fa958388579896f170570c6171a1b6c386669216"
SRC_SHA[R36.5]="d0acb2187786d6ba9f012dd72ee7005309903944c4e8f3d649d7dabd3aebba42"
SRC_SHA[R38.2]="460b0e9143cde12bbb83d74e464e1992596265ec57fc3ca08bf57b4dd6b6bb16"
SRC_SHA[R38.4]="6c8504c5e2d90b394ec223511ff892c4854ae615a955c166d3057a8d9abe54b6"
SRC_SHA[R39.2]="87d2e31ff55beaf2373e2f288538585995b231fd5745ec21f39a668e36efab2f"

TOOLCHAIN_URL="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v3.0/toolchain/aarch64--glibc--stable-2022.08-1.tar.bz2"
TOOLCHAIN_SHA="8af54f268c462b2d0737df8789b5e35db03a2d1ecbec90e20948f66f9244fcdd"

## Mirror of every archive above on our own asset bucket, reached through a vanity path so
## the storage backend can be swapped with one web-server rule instead of an edit here and
## on every branch. Objects are content-addressed by the sha256 already pinned above, so no
## second lookup table is needed. The direct bucket URL stays as a last resort.
MIRROR_BASE="${MIRROR_BASE:-https://download.stereolabs.com/lfs/objects}"
MIRROR_FALLBACK="${MIRROR_FALLBACK:-https://f003.backblazeb2.com/file/gitlab-com-lfs/objects}"
mirror_url()          { echo "$MIRROR_BASE/${1:0:2}/${1:2:2}/$1"; }
mirror_fallback_url() { echo "$MIRROR_FALLBACK/${1:0:2}/${1:2:2}/$1"; }

if [[ -z ${SRC_URL[$RELEASE]} ]]
then
    echo "ERROR: no NVIDIA source archive is registered for ${RELEASE}"
    echo "       add it to SRC_URL/SRC_SHA in $(basename $0)"
    exit 1
fi

## L4T 36.2 and newer ship the kernel sources directly under Linux_for_Tegra/source/ and
## need NVIDIA's toolchain; 35.x and older nest them under source/public/ instead.
needs_toolchain() {
    case "$1" in
        R36.2|R36.3|R36.4|R36.5|R38.2|R38.4|R39.2) return 0 ;;
        *) return 1 ;;
    esac
}

## Download to $1/$2 and check it against sha256 $3, trying each following URL in turn. A
## checksum mismatch counts as a failed source rather than a hard error, so an archive
## NVIDIA re-rolled in place falls through to the pinned copy on our bucket instead of
## quietly changing what gets built.
fetch_verified() {
    local dest="$1" fname="$2" want="$3" out
    shift 3
    out="$dest/$fname"

    for url in "$@"
    do
        [[ -n $url ]] || continue
        echo "Fetching $fname from $url"
        rm -f "$out"
        if ! wget -nv -O "$out" "$url"
        then
            echo "  WARNING: download failed"
            continue
        fi
        if echo "$want  $out" | sha256sum -c - > /dev/null 2>&1
        then
            echo "  sha256 OK"
            return 0
        fi
        echo "  WARNING: sha256 mismatch (expected $want)"
    done

    rm -f "$out"
    echo "ERROR: could not obtain a valid $fname for ${RELEASE}"
    echo "       tried NVIDIA and the Stereolabs mirror"
    return 1
}

fetch_verified "$TARGET_DW_PATH" "$SRC_ARCHIVE" "${SRC_SHA[$RELEASE]}" \
    "${SRC_URL[$RELEASE]}" "$(mirror_url "${SRC_SHA[$RELEASE]}")" \
    "$(mirror_fallback_url "${SRC_SHA[$RELEASE]}")"

if needs_toolchain "$RELEASE"
then
    fetch_verified "$TARGET_DW_PATH" "$TOOLCHAIN_ARCHIVE" "$TOOLCHAIN_SHA" \
        "$TOOLCHAIN_URL" "$(mirror_url "$TOOLCHAIN_SHA")" \
        "$(mirror_fallback_url "$TOOLCHAIN_SHA")"
fi

tar -xf $TARGET_DW_PATH/$SRC_ARCHIVE -C $TARGET_DW_PATH/
if [[ $? -ne 0 ]]
then
    echo "ERROR: Could not untar NVIDIA's sources"
    exit 1
fi

if needs_toolchain "$RELEASE"; then
    mkdir -p nvidia_kernel/src_archives/$RELEASE/toolchain
    tar --strip-components=1 -xf $TARGET_DW_PATH/$TOOLCHAIN_ARCHIVE -C $TARGET_DW_PATH/toolchain
    if [[ $? -ne 0 ]]
    then
        echo "ERROR: Could not untar NVIDIA's toolchain sources"
        exit 1
    fi
    tar xf $TARGET_DW_PATH/Linux_for_Tegra/source/kernel_src.tbz2 -C $TARGET_DW_PATH/
    tar xf $TARGET_DW_PATH/Linux_for_Tegra/source/kernel_oot_modules_src.tbz2 -C $TARGET_DW_PATH/
    tar xf $TARGET_DW_PATH/Linux_for_Tegra/source/nvidia_kernel_display_driver_source.tbz2 -C $TARGET_DW_PATH/
    if [[ $RELEASE == "R38.2" || $RELEASE == "R38.4" || $RELEASE == "R39.2" ]];then
        tar xf $TARGET_DW_PATH/Linux_for_Tegra/source/nvidia_unified_gpu_display_driver_source.tbz2 -C $TARGET_DW_PATH/
    fi

    rm $TARGET_DW_PATH/$TOOLCHAIN_ARCHIVE
else
    tar xf $TARGET_DW_PATH/Linux_for_Tegra/source/public/kernel_src.tbz2 -C $TARGET_DW_PATH/
fi

rm $TARGET_DW_PATH/$SRC_ARCHIVE
rm -r $TARGET_DW_PATH/Linux_for_Tegra/

pushd $TARGET_DW_PATH > /dev/null
tar cf kernel_$RELEASE.tar *
mv kernel_$RELEASE.tar ../
popd > /dev/null

rm -r $TARGET_DW_PATH