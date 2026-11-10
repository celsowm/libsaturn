#!/bin/sh
# POSIX entry point for the Makefile-owned example build.
set -eu
LC_ALL=C
export LC_ALL

usage() {
    cat <<'EOF'
Usage: ./build-example.sh EXAMPLE [options]

Build options:
  --force                         Rebuild all targets (-B)
  --ip-profile current|safe       Boot image profile (default: current)
  --ip-template KIND              IP template (default: native)
  --parallel-mode MASTER|SLAVE|AUTO
  --skybridge-validation
  --force-gem-split
  --parallel-runtime-validation
  --geometry-objects 0|12|24|48|96|192
  --geometry-faces-per-object 0|1|2|4
  --parallel-runtime-default-mode MASTER|SLAVE|AUTO
  -h, --help

LIBSATURN_TOOLCHAIN_ROOT may point to the directory containing sh2eb-elf-gcc.
EOF
}

fail() {
    printf '%s\n' "[build-example] $*" >&2
    exit 2
}

repo_root=$(CDPATH= cd "$(dirname "$0")" && pwd)
example=
force=0
ip_profile=current
ip_template=native
parallel_mode=
skybridge_validation=0
force_gem_split=0
parallel_runtime_validation=0
geometry_objects=0
geometry_faces_per_object=0
parallel_runtime_default_mode=

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --force) force=1; shift ;;
        --ip-profile)
            [ "$#" -ge 2 ] || fail '--ip-profile requires a value'
            ip_profile=$2; shift 2 ;;
        --ip-template)
            [ "$#" -ge 2 ] || fail '--ip-template requires a value'
            ip_template=$2; shift 2 ;;
        --parallel-mode)
            [ "$#" -ge 2 ] || fail '--parallel-mode requires a value'
            parallel_mode=$2; shift 2 ;;
        --skybridge-validation) skybridge_validation=1; shift ;;
        --force-gem-split) force_gem_split=1; shift ;;
        --parallel-runtime-validation) parallel_runtime_validation=1; shift ;;
        --geometry-objects)
            [ "$#" -ge 2 ] || fail '--geometry-objects requires a value'
            geometry_objects=$2; shift 2 ;;
        --geometry-faces-per-object)
            [ "$#" -ge 2 ] || fail '--geometry-faces-per-object requires a value'
            geometry_faces_per_object=$2; shift 2 ;;
        --parallel-runtime-default-mode)
            [ "$#" -ge 2 ] || fail '--parallel-runtime-default-mode requires a value'
            parallel_runtime_default_mode=$2; shift 2 ;;
        --) shift; [ "$#" -eq 0 ] || fail 'unexpected arguments after --'; break ;;
        -* ) fail "unknown option: $1" ;;
        * )
            [ -z "$example" ] || fail "unexpected argument: $1"
            example=$1
            shift ;;
    esac
done

[ -n "$example" ] || { usage >&2; exit 2; }
case "$example" in examples/*) example=${example#examples/} ;; esac
case "$example" in
    ''|*[!A-Za-z0-9_]*) fail "invalid example name: $example" ;;
esac
[ -f "$repo_root/examples/$example/main.c" ] || fail "example not found: examples/$example/main.c"

case "$ip_profile" in current|safe) ;; *) fail "invalid IP profile: $ip_profile" ;; esac
case "$ip_template" in
    native|yaul|sbl|minimal|yaul_fixed|region_free|minimal_boot|correct|final) ;;
    *) fail "invalid IP template: $ip_template" ;;
esac
case "$parallel_mode" in ''|MASTER|SLAVE|AUTO) ;; *) fail "invalid parallel mode: $parallel_mode" ;; esac
case "$parallel_runtime_default_mode" in ''|MASTER|SLAVE|AUTO) ;; *) fail "invalid parallel runtime mode: $parallel_runtime_default_mode" ;; esac
case "$geometry_objects" in 0|12|24|48|96|192) ;; *) fail "invalid geometry object count: $geometry_objects" ;; esac
case "$geometry_faces_per_object" in 0|1|2|4) ;; *) fail "invalid faces-per-object value: $geometry_faces_per_object" ;; esac

if [ -n "${LIBSATURN_TOOLCHAIN_ROOT:-}" ]; then
    [ -x "$LIBSATURN_TOOLCHAIN_ROOT/bin/sh2eb-elf-gcc" ] || fail "toolchain not found under LIBSATURN_TOOLCHAIN_ROOT=$LIBSATURN_TOOLCHAIN_ROOT"
    PATH="$LIBSATURN_TOOLCHAIN_ROOT/bin:$PATH"
    export PATH
fi
command -v sh2eb-elf-gcc >/dev/null 2>&1 || fail 'sh2eb-elf-gcc is not on PATH; set LIBSATURN_TOOLCHAIN_ROOT or add the toolchain bin directory'

if command -v gmake >/dev/null 2>&1; then
    make_cmd=$(command -v gmake)
else
    make_cmd=$(command -v make) || fail 'GNU Make 4.3 or newer is required (install gmake on macOS)'
fi

make_args="EXAMPLE=$example IP_PROFILE=$ip_profile IP_TEMPLATE_KIND=$ip_template"
if [ -n "$parallel_mode" ]; then
    case "$example" in skybridge_3d|parallel_runtime) ;; *) fail '--parallel-mode applies to skybridge_3d or parallel_runtime' ;; esac
    case "$parallel_mode" in MASTER) mode_value=0 ;; SLAVE) mode_value=1 ;; AUTO) mode_value=2 ;; esac
    if [ "$example" = skybridge_3d ]; then make_args="$make_args SAT_SKYBRIDGE_PARALLEL_MODE=$mode_value"; fi
    if [ "$example" = parallel_runtime ]; then make_args="$make_args PARALLEL_RUNTIME_DEFAULT_MODE=$mode_value"; fi
fi
if [ "$skybridge_validation" -eq 1 ]; then
    [ "$example" = skybridge_3d ] || fail '--skybridge-validation applies only to skybridge_3d'
    make_args="$make_args SAT_SKYBRIDGE_VALIDATION=1"
fi
if [ "$force_gem_split" -eq 1 ]; then
    [ "$example" = skybridge_3d ] || fail '--force-gem-split applies only to skybridge_3d'
    make_args="$make_args SAT_SKYBRIDGE_VALIDATION=1 SAT_SKYBRIDGE_FORCE_GEM_SPLIT=1"
fi
if [ "$parallel_runtime_validation" -eq 1 ]; then
    [ "$example" = parallel_runtime ] || fail '--parallel-runtime-validation applies only to parallel_runtime'
    make_args="$make_args SAT_PARALLEL_RUNTIME_VALIDATION=1"
fi
if [ "$geometry_objects" -ne 0 ]; then
    [ "$example" = parallel_runtime ] || fail '--geometry-objects applies only to parallel_runtime'
    make_args="$make_args PARALLEL_RUNTIME_GEOMETRY_OBJECTS=$geometry_objects"
fi
if [ "$geometry_faces_per_object" -ne 0 ]; then
    [ "$example" = parallel_runtime ] || fail '--geometry-faces-per-object applies only to parallel_runtime'
    make_args="$make_args PARALLEL_RUNTIME_FACES_PER_OBJECT=$geometry_faces_per_object"
fi
if [ -n "$parallel_runtime_default_mode" ]; then
    [ "$example" = parallel_runtime ] || fail '--parallel-runtime-default-mode applies only to parallel_runtime'
    case "$parallel_runtime_default_mode" in MASTER) mode_value=0 ;; SLAVE) mode_value=1 ;; AUTO) mode_value=2 ;; esac
    make_args="$make_args PARALLEL_RUNTIME_DEFAULT_MODE=$mode_value"
fi

printf '[build-example] Building %s with %s\n' "$example" "$make_cmd"
set -f
if [ "$force" -eq 1 ]; then
    # make_args contains only validated, whitespace-free Makefile assignments.
    # shellcheck disable=SC2086
    (cd "$repo_root" && "$make_cmd" -B $make_args all)
else
    # shellcheck disable=SC2086
    (cd "$repo_root" && "$make_cmd" $make_args all)
fi

safe_name=$(printf '%s' "$example" | tr '/' '_')
artifact_dir="$repo_root/build/examples"
mkdir -p "$artifact_dir"
for extension in iso cue bin app.bin elf map IP.BIN; do
    source_file="$repo_root/build/$example.$extension"
    if [ -f "$source_file" ]; then
        cp "$source_file" "$artifact_dir/$safe_name.$extension"
    fi
done
printf '[build-example] Artifacts staged under %s\n' "$artifact_dir/$safe_name.*"
