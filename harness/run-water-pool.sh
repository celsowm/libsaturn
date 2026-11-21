#!/bin/sh
# Capture the Water Pool example in Ymir: a ripple drop, the ball and the
# 17-node grid. Writes screenshots and telemetry to build/water_pool_evidence/.
set -eu
repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$repo"
out=build/water_pool_evidence
mkdir -p "$out"

if [ -n "${LIBSATURN_TOOLCHAIN_ROOT:-}" ]; then
    nm="$LIBSATURN_TOOLCHAIN_ROOT/bin/sh2eb-elf-nm"
else
    nm=sh2eb-elf-nm
fi

./build-example.sh water_pool
telemetry=0x$("$nm" build/water_pool.elf | awk '$NF == "_water_pool_telemetry" {print $1}')
[ "$telemetry" != "0x" ]

# A drops one ripple at the cursor at field 30; paused captures compare the
# same state. Z+Y replays a ripple for a fixed number of simulation ticks and
# pauses, so that capture does not depend on frame pacing.
./harness/run-harness.sh water_pool --frames 60 --out "$out/rest.json" -- \
    --screenshot "50:$out/rest.png" --dump-mem "$telemetry:64:$out/rest-telemetry.bin"
./harness/run-harness.sh water_pool --frames 70 --out "$out/drop.json" -- \
    --pad-button A --pad-press-at 30 --pad-release-at 33 \
    --screenshot "45:$out/drop-early.png" \
    --dump-mem "$telemetry:64:$out/drop-telemetry.bin"
./harness/run-harness.sh water_pool --frames 220 --out "$out/replay.json" -- \
    --pad-script harness/scripts/water_pool_replay.pad \
    --screenshot "215:$out/replay.png" --dump-mem "$telemetry:64:$out/replay-telemetry.bin"
./harness/run-harness.sh water_pool --frames 260 --out "$out/replay-pal.json" -- --pal \
    --pad-script harness/scripts/water_pool_replay.pad \
    --screenshot "255:$out/replay-pal.png" --dump-mem "$telemetry:64:$out/replay-pal-telemetry.bin"
./harness/run-harness.sh water_pool --frames 160 --out "$out/grid.json" -- \
    --pad-script harness/scripts/water_pool_grid.pad \
    --screenshot "25:$out/grid13.png" --screenshot "150:$out/grid17.png" \
    --dump-mem "$telemetry:64:$out/grid17-telemetry.bin"
