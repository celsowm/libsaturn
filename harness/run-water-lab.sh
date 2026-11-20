#!/bin/sh
# Capture native Water Lab modes and the standalone compositor probe in Ymir.
set -eu
repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$repo"
out=build/water_lab_evidence
mkdir -p "$out"

symbol() {
    if [ -n "${LIBSATURN_TOOLCHAIN_ROOT:-}" ]; then
        nm="$LIBSATURN_TOOLCHAIN_ROOT/bin/sh2eb-elf-nm"
    else
        nm=sh2eb-elf-nm
    fi
    "$nm" "build/$1.elf" | awk -v name="$2" '$NF == name {print "0x"$1}'
}

./build-example.sh water_lab
./build-example.sh water_lab_probe
native=$(symbol water_lab _water_lab_telemetry)
fx=$(symbol water_lab _compositor)
probe=$(symbol water_lab_probe _water_lab_probe)
[ -n "$native" ] && [ -n "$probe" ] && [ -n "$fx" ]

./harness/run-harness.sh water_lab --frames 170 --out "$out/hud.json" -- \
    --pad-button Z --pad-press-at 38 --pad-release-at 52 \
    --screenshot "16:$out/rippling-water.png" --screenshot "150:$out/compact-hud.png" \
    --dump-mem "0x05c00000:164000:$out/hud-commands.bin" \
    --dump-mem "$native:80:$out/hud-telemetry.bin"

./harness/run-harness.sh water_lab --frames 430 --out "$out/native.json" -- \
    --pad-script harness/scripts/water_lab.pad \
    --screenshot "99:$out/matched-dry.png" --screenshot "124:$out/opaque.png" \
    --screenshot "149:$out/mesh.png" --screenshot "174:$out/half.png" \
    --screenshot "199:$out/underwater.png" --screenshot "224:$out/multipass-fallback.png" \
    --screenshot "270:$out/stress-256.png" --screenshot "289:$out/stress-512.png" \
    --screenshot "420:$out/underwater-low-camera.png" \
    --dump-mem "$native:80:$out/native-telemetry.bin"

# Capture the command banks with both buffers drawing the same native mode.
for sample in '149 mesh' '174 half'; do
    set -- $sample
    ./harness/run-harness.sh water_lab --frames "$1" --out "$out/$2.json" -- \
        --pad-script harness/scripts/water_lab.pad \
        --dump-mem "0x05c00000:164000:$out/$2-commands.bin" \
        --dump-mem "$native:80:$out/$2-telemetry.bin"
done

./harness/run-harness.sh water_lab --frames 180 --out "$out/pal.json" -- --pal \
    --pad-button X --pad-press-at 18 --pad-release-at 26 \
    --screenshot "179:$out/pal-dry.png" --dump-mem "$native:80:$out/pal-telemetry.bin"

./harness/run-harness.sh water_lab --frames 600 --out "$out/scene-fx.json" -- \
    --pad-script harness/scripts/water_lab_fx.pad \
    --screenshot "104:$out/scene-fx-native.png" --screenshot "196:$out/scene-fx-wide.png" \
    --screenshot "258:$out/scene-fx-near.png" --screenshot "302:$out/scene-fx-follow.png" \
    --screenshot "435:$out/scene-fx-moving.png" --screenshot "500:$out/scene-fx-moving-later.png" \
    --screenshot "592:$out/scene-fx-exit.png" --dump-mem "$fx:48:$out/scene-fx-telemetry.bin"

late_draw=$(printf '0x%x' "$((probe + 56))")
late_upload=$(printf '0x%x' "$((probe + 60))")
partial=$(printf '0x%x' "$((probe + 64))")
mode_switch=$(printf '0x%x' "$((probe + 80))")
./harness/run-harness.sh water_lab_probe --frames "${WATER_LAB_PROBE_FIELDS:-8000}" \
    --out "$out/composition.json" -- \
    --screenshot "13:$out/synthetic.png" --screenshot "229:$out/composed.png" \
    --screenshot-when "$late_draw:1:$out/held-draw.png" \
    --screenshot-when "$late_upload:1:$out/held-upload.png" \
    --screenshot-when "$partial:1:$out/held-partial.png" \
    --screenshot-when "$mode_switch:1:$out/composition-disabled.png" \
    --video-observations "$out/composition-fields.csv" --video-crop 159:40:2:1 \
    --dump-mem "$probe:84:$out/composition-telemetry.bin"

pal_out="$out/pal-composition"
mkdir -p "$pal_out"
./harness/run-harness.sh water_lab_probe --frames 800 --out "$pal_out/composition.json" -- --pal \
    --video-observations "$pal_out/composition-fields.csv" --video-crop 159:40:2:1 \
    --dump-mem "$probe:84:$pal_out/composition-telemetry.bin"
