#!/bin/sh
# Native launcher for the Dino viewer with native, aspect-correct display.
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mednafen_bin=${MEDNAFEN_BIN:-$(command -v mednafen || true)}
if [ -z "$mednafen_bin" ] && [ -x /opt/homebrew/bin/mednafen ]; then
    mednafen_bin=/opt/homebrew/bin/mednafen
fi
if [ -z "$mednafen_bin" ]; then
    echo 'Mednafen not found; set MEDNAFEN_BIN to the installed executable.' >&2
    exit 1
fi
disc_path="$project_root/build/dino_demo.cue"
if [ ! -f "$disc_path" ]; then
    echo "Build dino_demo first: missing $disc_path" >&2
    exit 1
fi
exec "$mednafen_bin" -nothrottle 0 -sound 1 -video.driver softfb -ss.videoip 0 \
    -ss.xscale 2 -ss.yscale 2 -ss.stretch aspect -ss.cart none \
    "$@" "$disc_path"
