# Dino Demo

An interactive Sega Saturn take on the original PlayStation T-Rex tech demo.
It converts the attributed walking GLB into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts in a large side view against a dark screen, with its
original warm texture and a compact control display. L/R rotate the camera,
UP/DOWN change the viewing angle, X zooms in, and Y zooms out. A pauses the
walk, B resets the camera, and C toggles slow automatic orbit. START hides or
restores the text.

## How it stays close to the source

- **Texture at source resolution.** Faces bake 1:1 with the GLB's 512x512
  texture (`MODEL_TEXTURE_SCALE 1.0`) as 4-bit texels with a 15-color VDP1
  lookup table per face (`MODEL_TEXTURE_FORMAT lut4`). That is half the VRAM
  of one shared 256-color bank, and each face's colors fit its own patch.
- **Quads, not triangles.** Adjacent triangle pairs whose texture mapping and
  fold survive the change draw as one VDP1 quad (`MODEL_MERGE_QUADS`): 2400
  simplified triangles become about 1670 commands.
- **Triangles where they show.** The 46 teeth and claws are a third of the
  source mesh but a few pixels on screen; `MODEL_MATERIAL_WEIGHTS` spends
  that budget on the textured body. Solid-color faces draw as RGB polygons.
- **One vertex per moving point.** UV-split vertex copies are welded
  (`MODEL_WELD_VERTICES`), about 1300 runtime vertices instead of 1900.

## Both SH-2s

The face list is split between the CPUs: the Slave prepares 40% of the faces
while the Master prepares the rest, and the Master merges both into one
painter queue. `MODEL_LOCALITY_ORDER` orders faces along the body so each
half projects only its own window of vertices. While the Master sorts and
emits, the Slave decodes the next pose. In Ymir this runs at 20 fps from the
side and 15 fps in the busiest views.

Build with `.\build-example.ps1 dino_demo`. Run it in Ymir's modified
integration harness with `.\harness\run-harness.ps1 dino_demo -Bios <your-BIOS>`
(the harness hands the Slave its entry point, which the BIOS normally does;
without it everything runs on the Master at about 12 fps).
The original GLB, its CC BY 4.0 attribution, and conversion details are in
[`assets/LICENSE.txt`](assets/LICENSE.txt).
