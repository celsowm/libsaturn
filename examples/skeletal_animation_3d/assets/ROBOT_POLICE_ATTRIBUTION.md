# Robot Police Unit (animated) — attribution and modified source

"Robot Police Unit (animated)" by **Jungle Jim**:
https://skfb.ly/pMLX7

Original work licensed under **Creative Commons Attribution 4.0
International (CC BY 4.0)**: https://creativecommons.org/licenses/by/4.0/

This example redistributes the original GLB and the converted Saturn
skeletal data under that license. The original GLB is kept unchanged.
The generated C/H asset (`gen/robot_police_skeletal.{h,c}`) is a converted
derivative: the rig is preserved as a parent-first skeleton with 102
joints, the single `3903460652032_TempMotion` animation is sampled at
60 fps for 84 frames with sign-continuous Q15 quaternions, the skin
weights are quantised to Q15 with the last weight implied per vertex, and
vertex local positions are stored at `pos_shift = 10` (Q10.10). The
libsaturn importer bakes the 3D coordinates and skinning maths but does
not edit the visual mesh or modify the gesture itself. Jungle Jim did not
create or endorse the conversion or demo.

The asset carries 102 joints and one looping pose; the plan's Example A
spec lists six named clips (idle, walk, run, jump, punch, kick) and the
free rigged asset only ships one. Adding the remaining clips without an
inappropriate source asset is left to follow-up work; this MVP delivers
the integration against one real rigged model.

Attribution and license apply to the artwork and converted model data.
The example source code is covered by the repository's root LICENSE.

The original GLB SHA-256 (from `C:\Users\celso\Downloads\...`) is captured
here for provenance:

`09085C8EB2A386E4259D5F3C8726CCD5D218E5147BCB486216736CE857C511AF`