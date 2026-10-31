# Dino fidelity improvement plan

## Objective and baseline

Improve the Saturn reconstruction against the original PS1 demo, keeping
stock RAM and the existing runtime performance budget. The corrected views
use 90-degree yaw and zero pitch. Their animation phases, perspective and
apparent model scale are still independent. A single pair cannot establish
an anatomical or stride error.

The current Walk lasts approximately 0.952 seconds at 140% playback, versus
0.955 seconds in the recorded PS1 emulation. Keep that setting during pose
work. Treat the earlier three-quarter silhouette conclusions as invalid.

## 1. Establish a synchronized comparison

- Match apparent model length and projection as closely as practical, with
  neutral lighting and no red sweep. Align the visible ground/contact line.
- Capture at least two complete strides and align by corresponding foot
  contact, accounting for the PS1 framebuffer and Saturn pose pipeline delay.
- Produce eight phase-aligned lateral pairs per cycle, plus front and rear
  views to assess leg separation and tail sway.
- Record head, shoulder, hip, knee, ankle, toe and tail-tip screen landmarks.
  Normalize distances by body length; do not compare raw pixel positions.

Acceptance: repeated captures preserve camera, scale and phase alignment;
reported differences survive comparison at corresponding gait events.

## 2. Evaluate recovering the original deformation

The original demo exposes nine VDF weights and already deforms its source
mesh. Investigate capturing its deformed vertices or decoding the local
TMD/VDF inputs over a complete stride. Establish vertex correspondence with
the preserved source mesh before transferring motion to the reduced mesh.
The Saturn runtime already consumes baked vertex poses, so an original
deformation bake could avoid reconstructing the motion through a skeleton.

Acceptance: one replayed cycle matches source landmarks and loops cleanly,
without requiring original-disc access at Saturn runtime. Keep the existing
reconstructed rig and Roar available while evaluating this path.

## 3. Correct the reconstructed gait if needed

If original deformation cannot be transferred reliably, fit the existing
Blender reconstruction to the synchronized reference in this order:

1. Mean torso pitch, neck/head height and tail-root posture.
2. Foot trajectories, stance/swing durations and knee/ankle articulation.
3. Pelvis bob/roll, head counter-motion and delayed tail movement.

Use measured gait events to tune `FOOT_TARGETS` and body parameters. Modify
skin weights only where the pose targets are correct but the surface deforms
incorrectly. Prefer smaller, independently reviewable changes to a wholesale
rig rebuild.

Acceptance: reduced landmark error over a full cycle, stable support height,
and no new knee pops, sliding artifacts or loop discontinuity.

## 4. Improve temporal sampling and rendering

- Evaluate 32 baked Walk poses per stride against the current 20. Check
  pose-stream bytes, linker RAM placement and runtime cost before retaining it.
- Match neutral shading before changing texture color or palette conversion.
- Inspect the head, feet, tail tip and UV boundaries at equal apparent size.
  Change simplification locally only where the reduced mesh causes a visible
  error relative to the full reconstruction.

Acceptance: smoother observed motion and clearer surface detail without
exceeding stock RAM, texture VRAM or command capacity.

## 5. Validate the result

Build using project scripts where available, otherwise the established local
Makefile fallback. Run Mednafen for the visible result and Ymir for identical
input sequences and diagnostics. Preserve before/after phase sheets and the
cadence measurement. Require zero panic, draw overflow and failed tasks;
compare completed frames and late swaps against the same baseline sequence.

Do not change library rendering or scheduling code unless measurements
identify a library defect or an actual bottleneck.

## Execution result

Original deformation recovery succeeded. The default asset now uses its
32-pose Walk transferred to the prepared mesh, while Roar keeps its existing
20-pose sampling. Manual reconstruction fitting was unnecessary for this
walking pass. Aligned host geometry sheets cover eight phases from side,
front and back, including the previous reconstruction with a best-fit phase
offset. Original deformation replay, GLB roundtrip, importer tests, stock-RAM
build, Mednafen launch and Ymir runtime validation passed. See
[the recovery procedure and measured results](DINO_PS1_REFERENCE.md).

Texture/shading differences and the remaining geometric approximation are
separate from motion fidelity; neither is claimed to be identical to PS1.


The subsequent quality pass supersedes the reduced active surface: the current
asset preserves all 2724 original OBJ triangles and 1376 TMD points, with corner
UVs intact. Motion/UV-aware quad adjacency across normal splits allows the
unreduced source to fit 1899 VDP1 commands. The entrance is directly frontal
(yaw/pitch 0), and the renderer's lighting remains a separate fidelity gap.
