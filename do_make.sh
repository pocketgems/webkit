#! /bin/bash

set -x
set -e

if [ -z "${ANDROID_NDK}" ]; then
    echo "ANDROID_NDK must be set to build JavascriptCore"
    exit 1
fi

if [ -z "${MANTIS_TOOLCHAIN_DIR}" ]; then
    echo "MANTIS_TOOLCHAIN_DIR must be set to upload symbols"
    exit 3
fi

if [ -z "$1" ]; then
    echo "Please give destination directory on the command line"
    echo "$0 {path to .static_apportable_libs}"
    exit 2
fi

DEST="$1"
mkdir -p "${DEST}/arm64-v8a"
mkdir -p "${DEST}/armeabi-v7a"

./do_icu_make.sh

rm -rf symbols
mkdir -p symbols/arm64-v8a
mkdir -p symbols/armeabi-v7a

rm -rf WebKitBuild

#EXTRA="-DENABLE_JIT=ON -DENABLE_DFG_JIT=ON -DENABLE_FAST_JIT_PERMISSIONS=ON -DUSE_SYSTEM_MALLOC=ON -DDEVELOPER_MODE=OFF -DENABLE_API_TESTS=OFF"
EXTRA="-DENABLE_JIT=ON -DENABLE_FAST_JIT_PERMISSIONS=ON -DUSE_SYSTEM_MALLOC=ON -DDEVELOPER_MODE=OFF -DENABLE_API_TESTS=OFF"

LINK_DIRS=$(pwd)/external/icu/64/build/System/*
LINK_FLAGS=$(for d in ${LINK_DIRS}; do echo -n " -rpath $d "; done)
LINK_FLAGS="$LINK_FLAGS -rpath ${ANDROID_NDK}/platforms/android-21/arch-arm64/usr/lib -rpath ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/arm64-v8a"

Tools/Scripts/build-jsc --jsc-only --jit --release --minimal --cmakeargs " -DANDROID_ABI=arm64-v8a -DANDROID_NDK='${ANDROID_NDK}' -DCMAKE_TOOLCHAIN_FILE='${ANDROID_NDK}/build/cmake/android.toolchain.cmake' -DCMAKE_CXX_FLAGS='${LINK_FLAGS}' -DANDROID_NATIVE_API_LEVEL=21 -DANDROID_TOOLCHAIN=clang -DCMAKE_FIND_ROOT_PATH='$(pwd)/external/icu/64' ${EXTRA}"

# Change strip to cp if you need debug symbols
STRIP_FLAGS="-gx --strip-unneeded"
HOST=linux
if [ $(uname) = "Darwin" ]; then
    HOST=darwin
fi
"${ANDROID_NDK}/toolchains/llvm/prebuilt/$HOST-x86_64/aarch64-linux-android/bin/strip" WebKitBuild/Release/lib/libJavaScriptCore.so ${STRIP_FLAGS} -o "${DEST}/arm64-v8a/libJavascriptCore.so"
cp WebKitBuild/Release/lib/libJavaScriptCore.so symbols/arm64-v8a/libJavascriptCore.so

rm -rf WebKitBuild

LINK_DIRS=$(pwd)/external/icu/32/build/System/*
LINK_FLAGS=$(for d in ${LINK_DIRS}; do echo -n " -rpath $d "; done)
LINK_FLAGS="$LINK_FLAGS -rpath ${ANDROID_NDK}/platforms/android-21/arch-arm/usr/lib -rpath ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a"

Tools/Scripts/build-jsc --ftl-jit --jsc-only --jit --release --minimal --cmakeargs " -DANDROID_ABI=armeabi-v7a -DANDROID_NDK='${ANDROID_NDK}' -DCMAKE_TOOLCHAIN_FILE='${ANDROID_NDK}/build/cmake/android.toolchain.cmake' -DCMAKE_CXX_FLAGS='${LINK_FLAGS}' -DANDROID_NATIVE_API_LEVEL=21 -DANDROID_TOOLCHAIN=clang -DCMAKE_FIND_ROOT_PATH='$(pwd)/external/icu/32' ${EXTRA}"

"${ANDROID_NDK}/toolchains/llvm/prebuilt/$HOST-x86_64/arm-linux-androideabi/bin/strip" WebKitBuild/Release/lib/libJavaScriptCore.so ${STRIP_FLAGS} -o "${DEST}/armeabi-v7a/libJavascriptCore.so"
cp WebKitBuild/Release/lib/libJavaScriptCore.so symbols/armeabi-v7a/libJavascriptCore.so

SYMBOL_ARCHIVE="javascriptcore-symbols-$(git rev-parse HEAD).tar.gz"
tar -c -C symbols . | gzip > "${SYMBOL_ARCHIVE}"

./do_symbol_upload.py "${SYMBOL_ARCHIVE}"

git rev-parse HEAD > "${DEST}/../.jscore_commit"
