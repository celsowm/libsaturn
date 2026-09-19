# Skybridge 3D pig — attribution and modified source

"Low-poly Pig" by **AlexEsfell**: https://skfb.ly/6SSRH

Original work licensed under **Creative Commons Attribution 4.0
International (CC BY 4.0)**: https://creativecommons.org/licenses/by/4.0/

The mesh used in this game is a **user-modified derivative** supplied by
the Skybridge project author (including the voxel-style tail). The user
provided the GLB and explicitly supplied the attribution/license above.
The original artist did not create or endorse LibSaturn, Skybridge, the
model conversion or the user's modifications.

Supplied native GLB SHA-256:
`4aa80747d7ca0d0205f7e6beebe909d48d8d141df3273e03ace8267649f1814c`

`examples/skybridge_3d/Makefile.inc` feeds this native GLB directly to the
**existing**
`tools/import_model.py --target saturn`, including its native
animation-aware, silhouette-aware polygon simplification, 16.16 vertex
conversion and solid face color baking. The generation report and
importer's validation are build artifacts, not evidence of visual
correctness in a Saturn emulator. Do not remove this attribution when
reusing or distributing the supplied GLB, compiled mesh, baked poses
or the finished example.

This is attribution for the *artwork*; the LibSaturn source code's own
license is separate.
