# Skybridge 3D pig — attribution and modified source

"Low-poly Pig" by **AlexEsfell**: https://skfb.ly/6SSRH

Original work licensed under **Creative Commons Attribution 4.0
International (CC BY 4.0)**: https://creativecommons.org/licenses/by/4.0/

The mesh used in this game is a **user-modified derivative** supplied by
the Skybridge project author (including the voxel-style tail). The user
provided the GLB and explicitly supplied the attribution/license above.
The original artist did not create or endorse LibSaturn, Skybridge, the
model conversion or the user's modifications.

Supplied modified GLB SHA-256:
`4aa80747d7ca0d0205f7e6beebe909d48d8d141df3273e03ace8267649f1814c`

For the generic LibSaturn animated-GLB importer, the supplied GLB was
**repackaged without artistic remodelling** into one rigidly-skinned
mesh. This preserves the visible source surfaces, embedded material
colors and its authored Walk/Idle/Jump clips; original independent
animated mesh parts become rigid skin joints and tiny single-color
embedded atlases represent the source GLB's five PBR base colors.
Two invisible zero-scale helper meshes are omitted. The normalized GLB
is the losslessly compressed source reconstructed from
`pig.source.zlib.b64.part01` through `part07` by
`tools/unpack_model_asset.py` (GitHub's connector currently supports
text uploads, not direct GLB binary uploads).

Normalized GLB SHA-256:
`335dfb81cd8774ceb6c7db5eacd068c180c979d55f097f38da7d190169fe3e69`

`examples/skybridge_3d/Makefile.inc` invokes the **existing**
`tools/import_model.py --target saturn`, including its native
animation-aware, silhouette-aware polygon simplification, 16.16 vertex
conversion and solid face color baking. The generation report and
importer's validation are build artifacts, not evidence of visual
correctness in a Saturn emulator. Do not remove this attribution when
reusing or distributing the normalized GLB, compiled mesh, baked poses
or the finished example.

This is attribution for the *artwork*; the LibSaturn source code's own
license is separate.
