#!/usr/bin/env bash
# Cross-compiles the native libraries LMMS needs for Android (arm64-v8a).
# UNTESTED starting point - expect to adjust versions/flags.
#
# Usage: ANDROID_NDK=/path/to/ndk android/build-deps.sh /path/to/prefix
set -euo pipefail

PREFIX="$(realpath -m "${1:?usage: $0 <install-prefix>}")"
: "${ANDROID_NDK:?set ANDROID_NDK to the NDK root}"
ABI="${ABI:-arm64-v8a}"
API="${API:-24}"
WORK="$(mktemp -d)"
mkdir -p "$PREFIX"

cmake_common=(
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$API"
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
)

build() { # name git-url tag [extra cmake args...]
  local name=$1 url=$2 tag=$3; shift 3
  git clone --depth 1 --branch "$tag" "$url" "$WORK/$name"
  cmake -S "$WORK/$name" -B "$WORK/$name/build" "${cmake_common[@]}" "$@"
  cmake --build "$WORK/$name/build" -j"$(nproc)"
  cmake --install "$WORK/$name/build"
}

build oboe        https://github.com/google/oboe.git                1.9.3
build libsamplerate https://github.com/libsndfile/libsamplerate.git 0.2.2 \
  -DLIBSAMPLERATE_EXAMPLES=OFF -DBUILD_TESTING=OFF
build libsndfile  https://github.com/libsndfile/libsndfile.git      1.2.2 \
  -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
  -DENABLE_EXTERNAL_LIBS=OFF -DENABLE_MPEG=OFF

# fftw3f: Release-Tarball nutzen (Git-Repo enthaelt die Codelets nicht)
curl -L https://www.fftw.org/fftw-3.3.10.tar.gz | tar -xz -C "$WORK"
cmake -S "$WORK/fftw-3.3.10" -B "$WORK/fftw-3.3.10/build" "${cmake_common[@]}" \
  -DENABLE_FLOAT=ON -DENABLE_THREADS=OFF -DBUILD_TESTS=OFF
cmake --build "$WORK/fftw-3.3.10/build" -j"$(nproc)"
cmake --install "$WORK/fftw-3.3.10/build"

echo "Dependencies installed to $PREFIX"
