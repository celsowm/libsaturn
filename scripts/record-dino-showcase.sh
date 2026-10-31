#!/bin/sh
# Record the Dino viewer to an MP4: builds a counter-free variant, drives the
# camera with harness/scripts/dino_showcase.pad in the Ymir probe (frame-exact,
# offline), then muxes every frame with the captured SCSP/CD-DA audio.
# Usage: scripts/record-dino-showcase.sh [output.mp4] [frames]
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:-$root/build/dino_showcase.mp4}
frames=${2:-3600}
bios=${SATURN_BIOS:-/Users/celso/dev/tekken3-saturn/assets/imported/ymir/profile/roms/ipl/saturn_101a_us.bin}
pad=${DINO_PAD_SCRIPT:-$root/harness/scripts/dino_showcase.pad}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cd "$root"
make all EXAMPLE=dino_demo DINO_SHOWCASE_HUD=1 >"$work/build.log" 2>&1 ||
    { tail -20 "$work/build.log"; exit 1; }
slave=$(awk '/ _saturn_slave_entry$/ {print $1; exit}' $(ls -t build/variants/dino_demo/*/dino_demo.map | head -1))

set --
i=0
while [ "$i" -lt "$frames" ]; do
    set -- "$@" --screenshot "$i:$work/$(printf %05d "$i").png"
    i=$((i + 1))
done
harness/build-macos/probe --iso build/dino_demo.cue --bin build/dino_demo.app.bin \
    --bios "$bios" --frames "$frames" --slave-reset-entry "$slave" --ram-cart none \
    --out "$work/run.json" --dump-audio "$work/audio.raw" --pad-script "$pad" \
    "$@" >"$work/probe.log" 2>&1 || { tail -20 "$work/probe.log"; exit 1; }

# 640x448 interlaced displays at 4:3.
ffmpeg -loglevel error -y -framerate 60 -i "$work/%05d.png" \
    -f s16le -ar 44100 -ac 2 -i "$work/audio.raw" \
    -vf "scale=1280:960:flags=lanczos,format=yuv420p" \
    -c:v libx264 -preset slow -crf 16 -c:a aac -b:a 192k -shortest \
    -movflags +faststart "$out"

# Leave the normal viewer build in build/dino_demo.cue.
make all EXAMPLE=dino_demo >"$work/build.log" 2>&1 || { tail -20 "$work/build.log"; exit 1; }
echo "wrote $out"
