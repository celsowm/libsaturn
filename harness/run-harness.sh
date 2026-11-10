#!/bin/sh
# Native POSIX wrapper for the GPL Ymir integration probe.
set -eu

usage() {
    cat <<'EOF'
Usage: ./harness/run-harness.sh EXAMPLE [options] [-- PROBE_OPTIONS...]

Options:
  --bios BIOS.bin                 Caller-owned 512 KiB Saturn BIOS dump
  --frames N                      Injected program fields (default: 60)
  --boot-frames N                 BIOS setup fields (default: 90)
  --ram-cart none|1m|4m           Expansion RAM cart (default: none)
  --out PATH                      JSON output path
  --build-first                   Rebuild the example first
  --build-dir PATH                Ymir CMake build directory
  -h, --help

Arguments after -- are passed directly to probe, including repeatable
--screenshot, --dump-vram, --pad-script and telemetry options.
LIBSATURN_BIOS may provide the BIOS path.
EOF
}

fail() {
    printf '%s\n' "[ymir] $*" >&2
    exit 2
}

repo_root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
example=
bios=${LIBSATURN_BIOS:-$repo_root/bios/firmware/mpr-17933.bin}
frames=60
boot_frames=90
ram_cart=none
output=
build_first=0
build_dir=${LIBSATURN_YMIR_BUILD_DIR:-$repo_root/harness/build/ymir-baseline}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --bios)
            [ "$#" -ge 2 ] || fail '--bios requires a path'
            bios=$2; shift 2 ;;
        --frames)
            [ "$#" -ge 2 ] || fail '--frames requires a value'
            frames=$2; shift 2 ;;
        --boot-frames)
            [ "$#" -ge 2 ] || fail '--boot-frames requires a value'
            boot_frames=$2; shift 2 ;;
        --ram-cart)
            [ "$#" -ge 2 ] || fail '--ram-cart requires a value'
            ram_cart=$2; shift 2 ;;
        --out)
            [ "$#" -ge 2 ] || fail '--out requires a path'
            output=$2; shift 2 ;;
        --build-first) build_first=1; shift ;;
        --build-dir)
            [ "$#" -ge 2 ] || fail '--build-dir requires a path'
            build_dir=$2; shift 2 ;;
        --) shift; break ;;
        -* ) fail "unknown option: $1" ;;
        * )
            [ -z "$example" ] || fail "unexpected argument: $1"
            example=$1; shift ;;
    esac
done
[ -n "$example" ] || { usage >&2; exit 2; }
case "$example" in examples/*) example=${example#examples/} ;; esac
case "$example" in ''|*[!A-Za-z0-9_]*) fail "invalid example name: $example" ;; esac
[ -f "$repo_root/examples/$example/main.c" ] || fail "example not found: examples/$example/main.c"
case "$frames:$boot_frames" in *[!0-9:]*) fail 'frame counts must be non-negative integers' ;; esac
case "$ram_cart" in none|1m|4m) ;; *) fail "invalid RAM cart: $ram_cart" ;; esac
[ -f "$bios" ] || fail "BIOS file not found: $bios (set LIBSATURN_BIOS or pass --bios)"
bios_bytes=$(wc -c < "$bios" | tr -d '[:space:]')
[ "$bios_bytes" = 524288 ] || fail "BIOS must be 524288 bytes; got $bios_bytes"

if [ "$build_first" -eq 1 ] || [ ! -f "$repo_root/build/$example.cue" ] || [ ! -f "$repo_root/build/$example.app.bin" ]; then
    if [ "$build_first" -eq 1 ]; then
        "$repo_root/build-example.sh" "$example" --force
    else
        "$repo_root/build-example.sh" "$example"
    fi
fi

if [ ! -x "$build_dir/probe" ]; then
    command -v cmake >/dev/null 2>&1 || fail 'cmake is required to build the Ymir probe'
    printf '[ymir] Configuring pinned Ymir harness in %s\n' "$build_dir"
    cmake -S "$repo_root/harness" -B "$build_dir" -G Ninja
    cmake --build "$build_dir" --parallel 4
fi

if [ -z "$output" ]; then
    output="$repo_root/harness/build/$example/probe.json"
fi
case "$output" in /*) ;; *) output="$repo_root/$output" ;; esac
mkdir -p "$(dirname "$output")"

set -- --dump-vram 0x10000:48 --dump-vram 0x12000:448 --fb-sample 256 "$@"
if [ "$ram_cart" != none ]; then set -- --ram-cart "$ram_cart" "$@"; fi
if [ -f "$repo_root/build/$example.map" ]; then
    slave_entry=$(awk '$NF == "_saturn_slave_entry" { print $1; exit }' "$repo_root/build/$example.map")
    if [ -n "$slave_entry" ]; then set -- --slave-reset-entry "$slave_entry" "$@"; fi
fi

printf '[ymir] Running %s for %s fields; output %s\n' "$example" "$frames" "$output"
exec "$build_dir/probe" --boot-mode injection \
    --iso "$repo_root/build/$example.cue" \
    --bios "$bios" \
    --bin "$repo_root/build/$example.app.bin" \
    --frames "$frames" \
    --boot-frames "$boot_frames" \
    --out "$output" "$@"
