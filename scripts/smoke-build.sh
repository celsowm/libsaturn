#!/usr/bin/env bash
set -euo pipefail

echo "Validating required tools..."
for bin in make python; do
  if ! command -v "$bin" >/dev/null 2>&1; then
    echo "Missing: $bin"
    exit 1
  fi
done

if ! command -v sh2eb-elf-gcc >/dev/null 2>&1; then
  echo "Warning: sh2eb-elf-gcc not found in PATH. Full build will be skipped."
  exit 0
fi

if ! command -v mkisofs >/dev/null 2>&1 && ! command -v genisoimage >/dev/null 2>&1 && ! command -v xorrisofs >/dev/null 2>&1; then
  echo "Warning: mkisofs/genisoimage/xorrisofs not found. ISO build may fail."
fi

echo "Running clean build..."
make clean
make all

echo "Smoke build completed."
