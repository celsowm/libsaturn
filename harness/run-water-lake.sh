#!/bin/sh
# Replay the Water Lake camera route in Ymir four times, NAIVE and FULL with flat
# and with textured water, and write
# telemetry and screenshots to build/water_lake_evidence/. Each replay runs 390
# simulation ticks, so the results do not depend on frame pacing. Pressing C
# beforehand sets the section a replay pauses at (30 ticks each), which gives
# the pinned captures: the same tick in either mode.
set -eu
repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$repo"
out=build/water_lake_evidence
mkdir -p "$out"

if [ -n "${LIBSATURN_TOOLCHAIN_ROOT:-}" ]; then
    nm="$LIBSATURN_TOOLCHAIN_ROOT/bin/sh2eb-elf-nm"
else
    nm="sh2eb-elf-nm"
fi

./build-example.sh water_lake
telemetry=0x$("$nm" build/water_lake.elf | awk '$NF == "_water_lake_telemetry" {print $1}')
[ "$telemetry" != "0x" ]

# A pad script that presses C `sections` times from field 24, then starts the
# replay with the chord for the mode (each press held 10 fields).
pad_script() {
    mode=$1 sections=$2 file=$3
    case $mode in
        full) chord=Z+Y ;;
        naive) chord=Z+X ;;
        full_tex) chord=Z+A ;;
        naive_tex) chord=Z+B ;;
        full_dry) chord=Z+R ;;
        naive_dry) chord=Z+L ;;
    esac
    {
        echo "# Water Lake $mode replay, paused at section $sections (generated)."
        echo "0 NONE"
        field=24
        n=0
        while [ "$n" -lt "$sections" ]; do
            # Held longer than a frame, so one poll always sees the press.
            echo "$field C"
            echo "$((field + 10)) NONE"
            field=$((field + 20))
            n=$((n + 1))
        done
        echo "$((field + 8)) $chord"
        echo "$((field + 12)) NONE"
    } > "$file"
}

# Full replays: the telemetry of every section.
for mode in naive full naive_tex full_tex; do
    pad_script "$mode" 0 "$out/$mode-run.pad"
    ./harness/run-harness.sh water_lake --frames 2400 --out "$out/$mode.json" -- \
        --pad-script "$out/$mode-run.pad" \
        --dump-mem "$telemetry:524:$out/$mode-telemetry.bin"
done

# Without the water surface: what the rest of the frame clips on its own.
for mode in naive_dry full_dry; do
    pad_script "$mode" 0 "$out/$mode-run.pad"
    ./harness/run-harness.sh water_lake --frames 2400 --out "$out/$mode.json" -- \
        --pad-script "$out/$mode-run.pad" \
        --dump-mem "$telemetry:524:$out/$mode-telemetry.bin"
done

# Pinned captures at fixed ticks: the approach over the shore (section 4, tick
# 120), the bridge (8, tick 240) and under the water (11, tick 330), in each mode. The replay is paused at the
# start of the section, so the screenshot is taken well after it.
for section in 4 8 11; do
    for mode in naive full naive_tex full_tex; do
        pad_script "$mode" "$section" "$out/$mode-s$section.pad"
        ./harness/run-harness.sh water_lake --frames 1800 --out "$out/$mode-s$section.json" -- \
            --pad-script "$out/$mode-s$section.pad" \
            --screenshot "1700:$out/$mode-s$section.png" \
            --dump-mem "$telemetry:524:$out/$mode-s$section-telemetry.bin"
    done
done

# The same replay in PAL: the route follows ticks, so it stops on the same tick.
pad_script full 8 "$out/full-s8-pal.pad"
./harness/run-harness.sh water_lake --frames 2200 --out "$out/full-s8-pal.json" -- --pal \
    --pad-script "$out/full-s8-pal.pad" \
    --screenshot "2100:$out/full-s8-pal.png" \
    --dump-mem "$telemetry:524:$out/full-s8-pal-telemetry.bin"
