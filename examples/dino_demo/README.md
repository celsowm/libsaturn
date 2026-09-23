# Dino Demo

An interactive Sega Saturn take on the original PlayStation T-Rex tech demo.
It converts the attributed walking GLB into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts in a large side view against a dark screen, with its
original warm texture and a compact control display. LEFT/RIGHT orbit,
UP/DOWN change the viewing angle, L/R zoom, A pauses the walk, B resets the
camera, and C toggles slow automatic orbit. START hides or restores the text.

Build with `.\build-example.ps1 dino_demo`. Run it in Ymir's modified
integration harness with `.\harness\run-harness.ps1 dino_demo -Bios <your-BIOS>`.
The original GLB, its CC BY 4.0 attribution, and conversion details are in
[`assets/LICENSE.txt`](assets/LICENSE.txt).
