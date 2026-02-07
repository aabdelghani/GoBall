#!/usr/bin/env bash
# Cross-build SDL2_mixer (WAV only) for Raspberry Pi 5 (aarch64) on Ubuntu 24.04
# Installs into: /home/q/Projects/SquareLine_Project/aarch64-prefix
set -euo pipefail

PROJECT_ROOT="/home/q/Projects/SquareLine_Project"
SYSROOT="${PROJECT_ROOT}/rpi5-sysroot"
SDL2_ROOT="${PROJECT_ROOT}/sdl2-dev-rpi64"
PREFIX="${PROJECT_ROOT}/aarch64-prefix"
DL_DIR="${PROJECT_ROOT}/.downloads"

SDL2MIXER_VER="2.8.1"
SDL2MIXER_URL="https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2MIXER_VER}/SDL2_mixer-${SDL2MIXER_VER}.tar.gz"

need_cmd() { command -v "$1" >/dev/null 2>&1 || { echo "ERROR: missing command '$1'" >&2; exit 1; }; }
need_file() { [ -e "$1" ] || { echo "ERROR: missing file/dir: $1" >&2; exit 1; }; }

need_cmd curl
need_cmd aarch64-linux-gnu-gcc
need_cmd aarch64-linux-gnu-g++
need_cmd aarch64-linux-gnu-ar
need_cmd aarch64-linux-gnu-ranlib
need_cmd aarch64-linux-gnu-strip
need_cmd pkg-config

need_file "${SDL2_ROOT}/bin/sdl2-config"
need_file "${SDL2_ROOT}/lib/pkgconfig/sdl2.pc"

mkdir -p "${PREFIX}" "${DL_DIR}"

export CC="aarch64-linux-gnu-gcc"
export CXX="aarch64-linux-gnu-g++"
export LD="aarch64-linux-gnu-ld"
export AR="aarch64-linux-gnu-ar"
export RANLIB="aarch64-linux-gnu-ranlib"
export STRIP="aarch64-linux-gnu-strip"

# IMPORTANT: no --sysroot here (we link against host cross libc)
export CFLAGS="-I${PREFIX}/include"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="-L${PREFIX}/lib"

# pkg-config: our prefix then the prebuilt SDL2
export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${SDL2_ROOT}/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="${PKG_CONFIG_PATH}"

BUILD_TRIPLET="$(dpkg-architecture -qDEB_BUILD_GNU_TYPE 2>/dev/null || echo x86_64-pc-linux-gnu)"
HOST_TRIPLET="aarch64-linux-gnu"

fetch_extract () {
  local url="$1"
  local tarname="$(basename "$url")"
  pushd "${DL_DIR}" >/dev/null
  if [ ! -f "$tarname" ]; then
    echo "Downloading: $url" >&2
    curl -L -o "$tarname" "$url"
  else
    echo "Using cached: $tarname" >&2
  fi
  local topdir
  topdir="$(tar -tf "$tarname" | head -1 | cut -d/ -f1)"
  if [ ! -d "${topdir}" ]; then
    case "$tarname" in
      *.tar.bz2) tar xjf "$tarname" ;;
      *.tar.gz)  tar xzf "$tarname" ;;
      *.tar.xz)  tar xJf "$tarname" ;;
      *) echo "Unknown archive type: $tarname" >&2; exit 1 ;;
    esac
  else
    echo "Reusing existing directory: ${topdir}" >&2
  fi
  echo "${DL_DIR}/${topdir}"
  popd >/dev/null
}

echo "=== Build: SDL2_mixer ${SDL2MIXER_VER} (WAV only) ===" >&2
SRC_DIR="$(fetch_extract "${SDL2MIXER_URL}")"
pushd "${SRC_DIR}" >/dev/null

[ -f Makefile ] && make distclean || true
autoreconf -fi || true

./configure \
  --build="${BUILD_TRIPLET}" \
  --host="${HOST_TRIPLET}" \
  --prefix="${PREFIX}" \
  --with-sdl-prefix="${SDL2_ROOT}" \
  --disable-music-flac \
  --disable-music-ogg  \
  --disable-music-mp3  \
  --disable-music-mod  \
  --disable-music-midi \
  --enable-music-wav

make -j"$(nproc)"
make install

popd >/dev/null

echo
echo "=== SUCCESS ==="
echo "Installed SDL2_mixer (ARM64, WAV-only) to: ${PREFIX}"
echo "pkg-config path: ${PKG_CONFIG_PATH}"
echo
echo "Next:"
echo "  cmake -S ${PROJECT_ROOT} -B ${PROJECT_ROOT}/build -G Ninja \\"
echo "        -DCMAKE_TOOLCHAIN_FILE=${PROJECT_ROOT}/cmake/toolchain-aarch64-rpi.cmake"
echo "  cmake --build ${PROJECT_ROOT}/build"

