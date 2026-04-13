#!/usr/bin/env bash
set -euo pipefail

if ! command -v pacman >/dev/null 2>&1; then
  echo "This script must run inside MSYS2."
  exit 1
fi

echo "[1/3] Updating MSYS2 base..."
pacman -Syu --noconfirm || true
pacman -Su --noconfirm

echo "[2/3] Installing build dependencies..."
pacman -S --needed --noconfirm \
  base-devel \
  git \
  wget \
  curl \
  tar \
  xz \
  make \
  python \
  mingw-w64-ucrt-x86_64-python \
  mingw-w64-ucrt-x86_64-python-pip \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-binutils \
  mingw-w64-ucrt-x86_64-make \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  xorriso

echo "[3/3] Installing Mednafen (if available in repo)..."
if pacman -Si mingw-w64-ucrt-x86_64-mednafen >/dev/null 2>&1; then
  pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-mednafen
else
  echo "mednafen package not found in current repo. Install manually."
fi

echo "Kronos is usually installed via binary/manual release on Windows."
echo "Base setup completed."
