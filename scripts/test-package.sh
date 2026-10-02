#!/usr/bin/env bash
# Runs the package-boundary gates (the same ones CI runs):
#   1. host-tests        public headers compile with the host compiler, the
#                        source manifest matches src/
#   2. package-consumer  builds LibSaturn with the SH-2 toolchain, installs it
#                        to a temporary prefix, links a standalone consumer
#                        against that prefix only, and checks the contract
#   3. --conan           repeats the consumer build through a Conan package
#
# The sh2eb-elf toolchain must be on PATH (or LIBSATURN_TOOLCHAIN_ROOT set).
# On Windows run it from an MSYS2 UCRT64 shell, or use scripts/test-package.ps1.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

run_conan=0
for arg in "$@"; do
    case "$arg" in
        --conan) run_conan=1 ;;
        -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

if ! command -v sh2eb-elf-gcc >/dev/null 2>&1 && [ -z "${LIBSATURN_TOOLCHAIN_ROOT:-}" ]; then
    echo "error: sh2eb-elf-gcc not found on PATH (set LIBSATURN_TOOLCHAIN_ROOT)" >&2
    exit 1
fi

echo "[package] host-tests"
cmake --preset host-tests
ctest --preset host-tests

echo "[package] package-consumer"
cmake --preset package-consumer
cmake --build --preset package-consumer
ctest --preset package-consumer

if [ "$run_conan" -eq 1 ]; then
    echo "[package] conan"
    conan config install packaging/conan/config
    conan create . --profile:host=saturn-sh2eb --profile:build=default --build=missing
fi

echo "[package] all package gates passed"
