#!/usr/bin/env python3
"""Keep pure playback policies separate from hardware and shared runtime state."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
base = root / "src/audio/playback"
pure = (
    "ram_allocator.hpp",
    "voice_policy.hpp",
    "clock.hpp",
    "generation.hpp",
    "sound_registry.hpp",
    "voice_registry.hpp",
)
for filename in pure:
    source = (base / filename).read_text(encoding="utf-8")
    for forbidden in (
        '#include "src/hal/',
        '#include "src/core/runtime/',
        "saturn::hal::",
        "sat_frame_count(",
        "g_audio_streams",
        "g_state",
        "SAT_SKYBRIDGE",
    ):
        assert forbidden not in source, f"{filename}: pure policy depends on {forbidden}"

facade = (base / "api.cpp").read_text(encoding="utf-8")
for dependency in (
    '#include "src/audio/playback/ram_allocator.hpp"',
    '#include "src/audio/playback/voice_policy.hpp"',
    '#include "src/audio/playback/clock.hpp"',
    '#include "src/audio/playback/sound_registry.hpp"',
    '#include "src/audio/playback/voice_registry.hpp"',
):
    assert dependency in facade, f"audio facade missing {dependency}"
for leaked in ("g_audio_last_vblank", "g_audio_clock_valid", "g_sounds[", "g_voices["):
    assert leaked not in facade, f"audio facade reintroduced {leaked}"
print("audio pure-policy/runtime boundaries: OK")
