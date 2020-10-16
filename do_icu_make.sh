#! /bin/bash

set -x
set -e

if [ -z "${APPORTABLE_DIR}" ]; then
    echo "APPORTABLE_DIR must be set"
    exit 1
fi

ICU_DIR="${APPORTABLE_DIR}/System/icu"

rm -rf external/icu
mkdir -p external/icu/32/build
mkdir -p external/icu/64/build
mkdir -p external/icu/32/lib
mkdir -p external/icu/64/lib
cp -r "$ICU_DIR/include" external/icu/64
cp -r "$ICU_DIR/include" external/icu/32
cd external/icu/64/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Staging -DSET_PROJECT=1 -DANDROID=1 -DAPPORTABLE_MULTIPLE_SO_BUILD=1 -DHANDLE_NATIVE_WINDOW=1 -DPRECOMPILE_PREFIX_HEADER=1 -DNDK_BASE="${ANDROID_NDK}" -DMANTIS_TOOLCHAIN_LIB_DIR="${MANTIS_TOOLCHAIN_DIR}/cmake_toolchains" -DCMAKE_TOOLCHAIN_FILE="${APPORTABLE_DIR}/android.toolchain" -DARM64=1 "${APPORTABLE_DIR}"
ninja libicu.so
cp System/icu/libicu.so ../lib/libicuuc.so
cd ../../32/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Staging -DSET_PROJECT=1 -DANDROID=1 -DAPPORTABLE_MULTIPLE_SO_BUILD=1 -DHANDLE_NATIVE_WINDOW=1 -DPRECOMPILE_PREFIX_HEADER=1 -DNDK_BASE="${ANDROID_NDK}" -DMANTIS_TOOLCHAIN_LIB_DIR="${MANTIS_TOOLCHAIN_DIR}/cmake_toolchains" -DCMAKE_TOOLCHAIN_FILE="${APPORTABLE_DIR}/android.toolchain" -DARM32=1 "${APPORTABLE_DIR}"
ninja libicu.so
cp System/icu/libicu.so ../lib/libicuuc.so
