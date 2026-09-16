# Audio Showcase Third-Party Assets

The `audio_showcase` build attempts to download the following source WAV files from OpenGameArt.org. Each source page publishes the corresponding asset under **CC0 1.0 / public-domain dedication**.

CC0 does not require attribution, but LibSaturn keeps the original author, source page, and direct source file here for provenance and reproducibility.

The build does not depend on these desktop WAV files at runtime. `tools/generate_audio_showcase_assets.py` downloads them at build time, downmixes them to mono when necessary, resamples them to the showcase sample rate, and emits bounded signed 16-bit PCM data for the Saturn build.

## Sound effects

### Click

- Title: **Click**
- Author: **qubodup**
- License: **CC0 1.0**
- Source: https://opengameart.org/content/click
- Source WAV: https://opengameart.org/sites/default/files/click.wav
- Showcase symbol: `showcase_click`

### Laser

- Title: **Laser**
- Author: **frosty ham**
- License: **CC0 1.0**
- Source: https://opengameart.org/content/laser
- Source WAV: https://opengameart.org/sites/default/files/laserthing.wav
- Showcase symbol: `showcase_laser`

### Explosion

- Title: **Explosion**
- Author: **TinyWorlds**
- License: **CC0 1.0**
- Source: https://opengameart.org/content/explosion-0
- Source WAV: https://opengameart.org/sites/default/files/explosion.wav
- Showcase symbol: `showcase_explosion`

## Music

### 8 Bit Disco Loop

- Title: **8 Bit Disco Loop**
- Author: **cosmac**
- License: **CC0 1.0**
- Source: https://opengameart.org/content/8-bit-disco-loop
- Source WAV: https://opengameart.org/sites/default/files/title_1.wav
- Showcase symbol: `showcase_music`

For this early SCSP checkpoint the converted track is resident in Sound RAM and played as a looping `sat_sound_t`. Once `sat_music_t` and the streaming phase of `AUDIO_MUSIC_RUNTIME_PLAN.md` are implemented, the showcase BGM should move to the streaming path so the example also validates long-form file/CD playback.

## Offline fallback

Network availability is deliberately **not** required to compile the repository. If a CC0 download is unavailable, the generator emits a deterministic synthetic substitute for that item and the HUD's `REAL ASSETS` counter reflects how many of the four external CC0 assets were actually used.

For a licensing/content acceptance build, invoke the generator through the example Makefile with:

```text
AUDIO_SHOWCASE_REQUIRE_REAL=1
```

That mode fails the asset-generation step rather than silently using a fallback.

`showcase_reference` is always generated locally and is intentionally synthetic. It provides a simple known waveform for pitch and distortion diagnosis alongside the real-world audio assets.
