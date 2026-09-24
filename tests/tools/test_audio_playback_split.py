#!/usr/bin/env python3
"""Compile and enforce the hardware/audio facade ownership boundary."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
base=root / "src/audio/playback"
owners={
    "api.cpp":(
        "sat_audio_init", "sat_audio_shutdown", "sat_audio_update",
        "sat_audio_get_stats", "sat_audio_set_master_volume",
    ),
    "sound.cpp":("sat_sound_create", "sat_sound_unload"),
    "voice.cpp":(
        "sat_sound_play", "sat_sound_stop_all_instances", "sat_voice_stop",
        "sat_voice_set_volume", "sat_voice_set_pan", "sat_voice_is_playing",
    ),
    "state.cpp":(
        "g_sound_registry = {}", "g_voice_registry = {}",
        "void reset_runtime_state(", "void release_voice(",
        "int32_t choose_voice(",
    ),
}
files={name:(base/name).read_text(encoding="utf-8") for name in owners}
for filename,symbols in owners.items():
    for symbol in symbols:
        marker=(f" {symbol}(" if filename!="state.cpp" else symbol)
        if filename!="state.cpp":
            marker=f'extern "C" {("uint8_t" if symbol in ("sat_audio_is_initialized","sat_voice_is_playing") else "sat_result_t")} {symbol}('
        assert marker in files[filename], f"{filename}: missing {symbol}"
        for other,body in files.items():
            if other!=filename:
                assert marker not in body, f"{other}: {symbol} belongs to {filename}"
    subprocess.run(
        ["g++","-std=c++20","-Wall","-Wextra","-Werror",
         "-Iinclude","-I.","-fsyntax-only",str(base/name)],
        cwd=root,check=True,
    )
print("audio playback split: ownership and four C++ compile gates OK")
