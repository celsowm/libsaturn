#!/bin/sh
set -eu

root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
bios=${LIBSATURN_BIOS:-$root/bios/firmware/mpr-17933.bin}
out=$root/harness/build/world-lod3d-demo
map_path=

if [ ! -f "$bios" ]; then
    printf '[world-lod3d-demo] BIOS not found: %s\n' "$bios" >&2
    printf 'Set LIBSATURN_BIOS to a local 512 KiB Saturn BIOS dump.\n' >&2
    exit 2
fi

"$root/build-example.sh" world_lod3d_demo
mkdir -p "$out"
"$root/harness/run-harness.sh" world_lod3d_demo \
    --bios "$bios" --frames 400 --out "$out/run.json" -- \
    --pad-script "$root/harness/scripts/world_lod3d_demo_route.pad" \
    --dump-wram-high "$out/wramh.bin" \
    --vdp2-trace "$out/vdp2.csv" \
    --screenshot "30:$out/f30.png" \
    --screenshot "100:$out/f100.png" \
    --screenshot "150:$out/f150.png" \
    --screenshot "220:$out/f220.png" \
    --screenshot "240:$out/f240.png" \
    --screenshot "280:$out/f280.png" \
    --screenshot "340:$out/f340.png"

map_path=$(find "$root/build/variants/world_lod3d_demo" \
    -type f -name world_lod3d_demo.map -print | LC_ALL=C sort | tail -n 1)
if [ -z "$map_path" ]; then
    printf '%s\n' '[world-lod3d-demo] link map not found' >&2
    exit 2
fi

LIBSATURN_WORLDLOD_MAP=$map_path \
LIBSATURN_WORLDLOD_DUMP="$out/wramh.bin" \
LIBSATURN_WORLDLOD_SHOTS="$out" \
    python3 -m unittest harness.tests.test_world_lod3d_demo -v
