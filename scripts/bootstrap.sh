#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-host}"

case "$MODE" in
  host)
    bash scripts/setup-msys2.sh
    ;;
  full)
    bash scripts/setup-msys2.sh
    bash scripts/build-toolchain.sh
    ;;
  *)
    echo "Uso: bash scripts/bootstrap.sh [host|full]"
    exit 1
    ;;
esac

echo "Bootstrap concluido ($MODE)."

