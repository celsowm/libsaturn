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

All runtime audio sources below are **CC0 / public domain**. The build prefers immutable GitHub-hosted copies or mirrors so building the example does not depend on OpenGameArt's file CDN being reachable. Original provenance is preserved here.

`tools/generate_infinite_explorer_audio.py` downmixes source WAV files to mono, resamples to 11025 Hz, bounds clip duration, crossfades loops where appropriate, and emits signed PCM8 so several looping and one-shot resources can coexist in the Saturn's Sound RAM.

### Rocket Engine

- Runtime symbol: `explorer_engine`
- Author: theMinesAreShakin
- Purpose: ship engine loop and low-pitched portal hum
- License: **CC0 1.0**
- Original source: https://opengameart.org/content/rocket-engine
- Original WAV: https://opengameart.org/sites/default/files/rocket_engine.001.wav
- GitHub mirror used first: `Damerlan/polar-assault`, pinned commit `6a4bd8330c06241df27991ae72bed992376c6621`
- Original SHA-256: `dc82ef86d6af2278d6592d20372737c32620f1c1957290c79f6643af5381f277`

The original hash is independently recorded by the CC0-source manifest in `ExCodeCowboy/StudentGrouper`; the LibSaturn generator verifies the downloaded mirror against that SHA-256 before accepting it.

### Scanner ping

- Runtime symbol: `explorer_scanner`
- File: `ricochet_ping.wav`
- Author: GameAudio
- Source: https://freesound.org/people/GameAudio/sounds/220204/
- License: **CC0 1.0**
- GitHub copy: `euuuuuuan/voidclad-public`, pinned commit `440916aabc30abe014cb33ad90bd150bfbf22dd0`
- SHA-256: `971a301fa89fd51a7ceeb4d8e948de393af5edd1112cd00e77090aa7624a3556`

The public repository's media-license register explicitly records this file as CC0 and redistribution-safe.

### Fire SFX

- Runtime symbol: `explorer_fire`
- File: `fire_heavy.wav`
- Author: qubodup
- Source: https://opengameart.org/content/tiny-naval-battle-sounds-set
- Source sound: `GunShotGverb`
- License: **CC0 1.0**
- GitHub copy: `euuuuuuan/voidclad-public`, pinned commit `440916aabc30abe014cb33ad90bd150bfbf22dd0`
- SHA-256: `cfbbaeb156bb9992ac571d6dd820fcff2a5702b276c9bb59f12a38f6ad7eadde`

### Impact SFX

- Runtime symbol: `explorer_impact`
- File: `impact_pen.wav`
- Author: qubodup
- Source: https://opengameart.org/content/tiny-naval-battle-sounds-set
- Source sound: `ExplosionMetalGverb`
- License: **CC0 1.0**
- GitHub copy: `euuuuuuan/voidclad-public`, pinned commit `440916aabc30abe014cb33ad90bd150bfbf22dd0`
- SHA-256: `935cbabff9aa83176bf946db7a00204def63a4ae7fa83b64f92d6732a63f74f4`

### Music

- Runtime symbol: `explorer_music`
- File: `distant_flute.wav`
- Author: Beatscribe
- Collection: **Homebrew Free Jingle and Fanfare Music Assets**
- License: **CC0 1.0**
- Source repository: https://github.com/Beatscribe/homebrew_vgm
- GitHub source pinned to commit `5a82f88b87bb442499685c494b7a96278121b4c7`

The upstream README states that the collection is CC0 and may be used in any project. This short real composition is used as the resident exploratory BGM until `sat_music_t` streaming replaces the resident-Sound-RAM path.

### Storm ambience

- Runtime symbol: `explorer_storm`
- Build file: `wave.wav`
- Original asset: **Water Waves**
- Original creator: transitking; submitted by qubodup
- License: **CC0 1.0**
- Original source: https://opengameart.org/content/water-waves
- GitHub processed CC0 copy: `ExCodeCowboy/StudentGrouper`, pinned commit `75cc0b3e101bb6cfb824ac9149b46361e91991d9`
- Processed-file SHA-256: `23eed75efff0950354376a7073917839ea80711d9f78724d519fa2a643e9aaa9`

The source repository records both the original CC0 recording and the processing manifest for `wave.wav`.

## Offline fallback and acceptance mode

Network access is not required for an ordinary repository build. Both generators use deterministic fallbacks if an external CC0 source cannot be downloaded, and the example shows `ASSET FALLBACK` on its HUD if any fallback was used.

Audio sources intentionally prefer `raw.githubusercontent.com` before any original CDN URL because GitHub access is already required to obtain LibSaturn itself. Cached downloads remain under the generated build directory and are reused on later builds.

For a content/licensing acceptance build, set:

```text
INFINITE_EXPLORER_REQUIRE_REAL=1
```

In that mode asset generation fails instead of substituting a fallback. Once downloaded, source files are cached under the generated build directory.
