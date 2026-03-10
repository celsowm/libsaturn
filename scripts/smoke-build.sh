#!/usr/bin/env bash
set -euo pipefail

echo "Validando ferramentas obrigatorias..."
for bin in make python; do
  if ! command -v "$bin" >/dev/null 2>&1; then
    echo "Faltando: $bin"
    exit 1
  fi
done

if ! command -v sh2eb-elf-gcc >/dev/null 2>&1; then
  echo "Aviso: sh2eb-elf-gcc nao encontrado no PATH. Build completa sera ignorada."
  exit 0
fi

if ! command -v mkisofs >/dev/null 2>&1 && ! command -v genisoimage >/dev/null 2>&1 && ! command -v xorrisofs >/dev/null 2>&1; then
  echo "Aviso: mkisofs/genisoimage/xorrisofs nao encontrado. Build de ISO pode falhar."
fi

echo "Executando build limpa..."
make clean
make all

echo "Smoke build concluido."
