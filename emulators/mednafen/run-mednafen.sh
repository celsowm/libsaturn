#!/bin/sh
# POSIX Mednafen launcher. BIOS firmware remains caller-provided and ignored.
set -eu

usage() {
    cat <<'EOF'
Usage: emulators/mednafen/run-mednafen.sh DISC.cue [--bios BIOS.bin] [-- MEDNAFEN_OPTIONS...]

LIBSATURN_MEDNAFEN_HOME selects Mednafen's writable state/firmware directory.
The default is the repository's ignored bios/ directory.
EOF
}

fail() {
    printf '%s\n' "[mednafen] $*" >&2
    exit 2
}

repo_root=$(CDPATH= cd "$(dirname "$0")/../.." && pwd)
disc=
bios=
while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --bios)
            [ "$#" -ge 2 ] || fail '--bios requires a path'
            bios=$2; shift 2 ;;
        --)
            shift
            break ;;
        -* ) fail "unknown option: $1" ;;
        * )
            [ -z "$disc" ] || fail "unexpected argument: $1"
            disc=$1; shift ;;
    esac
done
[ -n "$disc" ] || { usage >&2; exit 2; }
[ -f "$disc" ] || fail "disc image not found: $disc"

mednafen_cmd=${MEDNAFEN:-mednafen}
command -v "$mednafen_cmd" >/dev/null 2>&1 || fail 'mednafen is not on PATH; set MEDNAFEN to its executable path'
mednafen_home=${LIBSATURN_MEDNAFEN_HOME:-$repo_root/bios}
firmware_dir="$mednafen_home/firmware"
firmware="$firmware_dir/mpr-17933.bin"
mkdir -p "$firmware_dir"

if [ -n "$bios" ]; then
    [ -f "$bios" ] || fail "BIOS file not found: $bios"
    bios_bytes=$(wc -c < "$bios" | tr -d '[:space:]')
    [ "$bios_bytes" = 524288 ] || fail "BIOS must be 524288 bytes; got $bios_bytes"
    if ! cmp -s "$bios" "$firmware"; then
        cp "$bios" "$firmware"
    fi
fi
[ -f "$firmware" ] || fail "Mednafen BIOS not found at $firmware; pass --bios or install your own dump there"

export MEDNAFEN_HOME="$mednafen_home"
printf '[mednafen] Disc: %s\n[mednafen] BIOS: %s\n' "$disc" "$firmware"
exec "$mednafen_cmd" "$disc" "$@"
