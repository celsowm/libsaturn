#!/usr/bin/env bash
set -euo pipefail

if ! command -v pacman >/dev/null 2>&1; then
  echo "Este script deve rodar dentro do MSYS2."
  exit 1
fi

echo "[1/3] Atualizando base MSYS2..."
pacman -Syu --noconfirm || true
pacman -Su --noconfirm

echo "[2/3] Instalando dependencias de build..."
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

echo "[3/3] Instalando Mednafen (se disponivel no repo)..."
if pacman -Si mingw-w64-ucrt-x86_64-mednafen >/dev/null 2>&1; then
  pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-mednafen
else
  echo "Pacote mednafen nao encontrado no repo atual. Instale manualmente."
fi

echo "Kronos normalmente e instalado via release binaria/manual no Windows."
echo "Setup base concluido."
