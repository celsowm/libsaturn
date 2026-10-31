# Smooth indexed texture lighting

The current Dino build preserves 640x448 double-density interlace and the
original mesh, while baking smooth directional lighting into its textures.
This approximates the volume visible in the PS1 reference without hardware
Gouraud or a per-frame CPU shading pass.

## Implementation and limits

`MODEL_TEXTURE_LIGHTING=baked-smooth` selects a host-only texture bake.
Area-weighted normals are shared by vertices that represent the same moving
point, including compatible UV-split copies. Each face patch interpolates
its corner illumination over the same A/B/C/D rectangle used for UV sampling.
Encoded RGB channels are modulated before indexed palette quantization;
alpha is preserved. The resulting texture gradients are ordinary VDP1 texels.

The Dino uses a light toward `(0.48,0.70,0.52)`, ambient `0.30` and diffuse
strength `1.35`. This emphasizes the shoulder, back and upper head while
leaving the underside shaded. The previous red screen-line sweep is off at
startup; Z still toggles it. The existing entrance palette fade remains.

Lighting is fixed in the model's bind space. It does not move with the camera,
recompute deformed normals, reproduce shadow casting or implement the original
GTE lighting matrices. It is a visual approximation, not a claim of identical
PS1 lighting. The model's geometry, UVs, 1376 moving points, 1899 runtime faces
and 32/20 Walk/Roar samples are unchanged.

The bake raises the resident texture estimate from 241088 to 250696 bytes
and unique textures from 1726 to 1822. The normal memory/build gates pass on
stock RAM. No Gouraud table or runtime normal stream is added.

## Controlled comparison

The comparison uses the same held original Walk pose and GTE-derived camera
projection as the earlier [fair comparison](DINO_FAIR_COMPARISON.md). It holds
four poses in profile and front views, with the red sweep disabled on both
Saturn versions. No color corrections or smoothing are applied to the images.

Artifacts are local under `harness/build/dino_lighting/`:

- `before-after.png`: profile and front, before left and after right.
- `profile-ps1-before-after.png` and `front-ps1-before-after.png`: PS1 reference,
  previous Saturn and new Saturn, with matching pose/projection.
- `capture.map`, `capture.json` and `camera.bin`: controlled capture evidence.
- `runtime_analysis.json`: final production control run.

The temporary held-pose camera patch was removed before the production build.
The final control run uses the normal frontal entrance, stock RAM, both SH-2s
and the existing 1200-field input script. Host checks cover UV-copy normal
continuity, alpha preservation, light/shadow response and existing animated
import/quad-merge behavior. These are emulator/host checks; no physical Saturn
lighting or performance measurements are claimed.

## Clean music reference

The final soundtrack is the user's clean
[Demo 1 - T-Rex Demo Music (OST)](https://www.youtube.com/watch?v=N1LF8YjifOc)
reference. It replaces the earlier video containing incidental roars and the
different tribal music from the US Developer's Demo CD.

The complete YouTube AAC recording is decoded to stereo 44.1 kHz 16-bit PCM,
without gain processing, trimming, remixing, crossfades or another lossy encode.
The last CD sector adds 320 silent stereo frames, giving a 141.44-second track.
The CUE/BIN contains the filesystem and audio track; the ISO contains only
the filesystem. The Saturn reads the TOC once and repeats the audio's explicit
FAD range. SCSP EXTS0 and EXTS1 route the two channels to their corresponding
outputs at unity; no PCM sample voices, 68000 score or streaming decoder run.

A 9300-field capture verifies a complete pass and the next pass sample for
sample against the decoded reference. The 141.44-second repeat is continuous
in that Ymir capture. The final combined lighting build is also checked against
the same decoded reference during its control run. This establishes digital
passthrough of the selected recording, not pristine original-disc/SPU fidelity
or a real-drive gapless-playback guarantee. Persistent evidence is recorded in
`harness/studies/dino-demo1-audio.json` and `harness/studies/dino-texture-lighting.json`.
