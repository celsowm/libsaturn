# Infinite Explorer source assets

The Saturn-ready assets in this example are generated during the build. Visual and audio sources below are either CC0/public-domain or NASA material already used by the example. LibSaturn records provenance even when attribution is not legally required.

## Visuals

### Ground

- `rocks_ground_02_cc0.jpg`: **Rocks Ground 02**
- Author: Rob Tuytel / Poly Haven
- License: **CC0 1.0**
- Source: https://polyhaven.com/a/rocks_ground_02

### 360-degree horizon

- **Dikhololo Night** HDRI / tonemapped equirectangular panorama
- Author: Greg Zaal / Poly Haven
- License: **CC0 1.0**
- Source: https://polyhaven.com/a/dikhololo_night
- Build source: https://dl.polyhaven.org/file/ph-assets/HDRIs/extra/Tonemapped%20JPG/dikhololo_night.jpg

`tools/generate_infinite_explorer_horizon.py` downloads the real 360-degree equirectangular panorama, vertically crops it to the sky/horizon band, downsamples it to the Saturn NBG0 plane, and quantizes the complete wrapped strip with one 256-color palette. Horizontal scrolling therefore follows the original 360 panorama rather than repeating a small decorative tile.

### Spacecraft

- `nasa_psyche_spacecraft.jpg`: **Psyche Spacecraft (Artist's Concept)**
- NASA/JPL-Caltech
- Source: https://images.nasa.gov/details-PIA23875

NASA imagery is used under NASA's media-use guidelines. The spacecraft source has its near-black background mapped to VDP1 colour zero at conversion time.

The former Peony Nebula image is no longer used as the gameplay horizon; it was a normal image tiled repeatedly and did not represent a true seamless 360-degree environment.

## Audio

All audio below is published as **CC0 / public domain** by its source page. The build downmixes to mono, resamples to 11025 Hz, bounds clip duration, and emits signed PCM8 so several looping and one-shot resources can coexist in the Saturn's Sound RAM.

### Rocket Engine

- Author: theMinesAreShakin
- Purpose: ship engine loop and low-pitched portal hum
- License: **CC0**
- Source: https://opengameart.org/content/rocket-engine
- WAV: https://opengameart.org/sites/default/files/rocket_engine.001.wav

### Click

- Author: qubodup
- Purpose: scanner / interaction ping
- License: **CC0**
- Source: https://opengameart.org/content/click
- WAV: https://opengameart.org/sites/default/files/click.wav

### Laser

- Author: frosty ham
- Purpose: fire SFX
- License: **CC0**
- Source: https://opengameart.org/content/laser
- WAV: https://opengameart.org/sites/default/files/laserthing.wav

### Explosion

- Author: TinyWorlds
- Purpose: damage / collision / portal event
- License: **CC0**
- Source: https://opengameart.org/content/explosion-0
- WAV: https://opengameart.org/sites/default/files/explosion.wav

### 8 Bit Disco Loop

- Author: cosmac
- Purpose: temporary resident BGM until `sat_music_t` streaming replaces it
- License: **CC0**
- Source: https://opengameart.org/content/8-bit-disco-loop
- WAV: https://opengameart.org/sites/default/files/title_1.wav

### wind1

- Author: Luke.RUSTLTD
- Purpose: storm ambience
- License: **CC0**
- Source: https://opengameart.org/content/wind1
- WAV: https://lpc.opengameart.org/sites/default/files/wind1.wav

## Offline fallback and acceptance mode

Network access is not required for an ordinary repository build. Both generators use deterministic fallbacks if an external CC0 source cannot be downloaded, and the example shows `ASSET FALLBACK` on its HUD if any fallback was used.

For a content/licensing acceptance build, set:

```text
INFINITE_EXPLORER_REQUIRE_REAL=1
```

In that mode asset generation fails instead of substituting a fallback. Once downloaded, source files are cached under the generated build directory.
