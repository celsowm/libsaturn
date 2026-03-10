#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-$HOME/saturn-tools}"
TARGET="${TARGET:-sh2eb-elf}"
GCC_VER="${GCC_VER:-13.2.0}"
BINUTILS_VER="${BINUTILS_VER:-2.41}"
JOBS="${JOBS:-$(nproc)}"
ROOT="${ROOT:-$PWD/toolchain-src}"

mkdir -p "$ROOT"
cd "$ROOT"

echo "[1/6] Preparando fontes..."
if [[ ! -f "binutils-$BINUTILS_VER.tar.xz" ]]; then
  wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
fi
if [[ ! -d "binutils-$BINUTILS_VER" ]]; then
  tar xf "binutils-$BINUTILS_VER.tar.xz"
fi

if [[ ! -f "gcc-$GCC_VER.tar.xz" ]]; then
  wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
fi
if [[ ! -d "gcc-$GCC_VER" ]]; then
  tar xf "gcc-$GCC_VER.tar.xz"
fi

echo "[2/6] Baixando prerequisitos do GCC..."
pushd "gcc-$GCC_VER" >/dev/null
./contrib/download_prerequisites
popd >/dev/null

echo "[3/6] Build binutils..."
mkdir -p build-binutils
pushd build-binutils >/dev/null
../binutils-$BINUTILS_VER/configure \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --with-sysroot \
  --disable-nls \
  --disable-werror
make -j"$JOBS"
make install
popd >/dev/null

echo "[4/6] Build GCC (stage freestanding)..."
mkdir -p build-gcc
pushd build-gcc >/dev/null
../gcc-$GCC_VER/configure \
  --target="$TARGET" \
  --prefix="$PREFIX" \
  --without-headers \
  --disable-nls \
  --disable-shared \
  --disable-multilib \
  --disable-decimal-float \
  --disable-threads \
  --disable-libatomic \
  --disable-libgomp \
  --disable-libquadmath \
  --disable-libssp \
  --disable-libvtv \
  --disable-libstdcxx \
  --enable-languages=c,c++ \
  --with-endian=big \
  --with-cpu=sh2
make -j"$JOBS" all-gcc all-target-libgcc
make install-gcc install-target-libgcc
popd >/dev/null

echo "[5/6] Toolchain instalada em: $PREFIX"
echo "[6/6] Adicione ao PATH:"
echo "export PATH=\"$PREFIX/bin:\$PATH\""

