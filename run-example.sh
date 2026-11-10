#!/bin/sh
# POSIX build-and-run entry point for Saturn examples.
set -eu

usage() {
    cat <<'EOF'
Usage: ./run-example.sh EXAMPLE [options] [-- MEDNAFEN_OPTIONS...]

Options:
  --build-first                   Force a rebuild before launch
  --ip-profile current|safe       Boot image profile
  --ip-template KIND              IP template (default: native)
  --bios BIOS.bin                 Caller-owned 512 KiB Saturn BIOS dump
  --emulator mednafen             Emulator (default: mednafen)
  -h, --help

LIBSATURN_BIOS may provide the BIOS path. Any arguments after -- are passed to
Mednafen. LIBSATURN_TOOLCHAIN_ROOT may point to the SH-2 toolchain.
EOF
}

fail() {
    printf '%s\n' "[run-example] $*" >&2
    exit 2
}

repo_root=$(CDPATH= cd "$(dirname "$0")" && pwd)
example=
build_first=0
ip_profile=current
ip_template=native
bios=${LIBSATURN_BIOS:-}
emulator=mednafen

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --build-first) build_first=1; shift ;;
        --ip-profile)
            [ "$#" -ge 2 ] || fail '--ip-profile requires a value'
            ip_profile=$2; shift 2 ;;
        --ip-template)
            [ "$#" -ge 2 ] || fail '--ip-template requires a value'
            ip_template=$2; shift 2 ;;
        --bios)
            [ "$#" -ge 2 ] || fail '--bios requires a path'
            bios=$2; shift 2 ;;
        --emulator)
            [ "$#" -ge 2 ] || fail '--emulator requires a value'
            emulator=$2; shift 2 ;;
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
[ "$emulator" = mednafen ] || fail "unsupported emulator: $emulator (available: mednafen)"

if [ "$build_first" -eq 1 ] || [ ! -f "$repo_root/build/$example.cue" ] || [ ! -f "$repo_root/build/$example.iso" ]; then
    if [ "$build_first" -eq 1 ]; then
        "$repo_root/build-example.sh" "$example" --force --ip-profile "$ip_profile" --ip-template "$ip_template"
    else
        "$repo_root/build-example.sh" "$example" --ip-profile "$ip_profile" --ip-template "$ip_template"
    fi
fi

cue="$repo_root/build/$example.cue"
[ -f "$cue" ] || fail "CUE image not found: $cue"
printf '[run-example] Running %s on Mednafen\n' "$example"
if [ -n "$bios" ]; then
    exec "$repo_root/emulators/mednafen/run-mednafen.sh" "$cue" --bios "$bios" -- "$@"
fi
exec "$repo_root/emulators/mednafen/run-mednafen.sh" "$cue" -- "$@"
