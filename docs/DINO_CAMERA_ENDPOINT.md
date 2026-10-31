# Original PS1 endpoint framing

The Dino viewer now ends its entrance with the original PS1 program's default
camera framing. The calibration is applied to the shared orbit state at
initialization, so normal camera updates preserve it and B restores it after
zooming or rotating. Bounds still supply safe zoom and clipping limits.

## Measured reference

The local Developer's Demo CD DINO.EXE was run in PCSX-Redux without camera
writes. Its default view position is `(0,0,-40000)`, rotation is zero, and the
model projection uses GTE H=1000, rotation diagonal `(-4098,2048,-4098)` in
Q12, and translation `(0,0,42019)`. Its screen center is `(320,120)` within
each 640x240 framebuffer. The two source buffers are identical in the held-pose
reference capture.

The source OBJ/GLB coordinate conversion is unchanged. In Saturn coordinates,
the orbit target is approximately `(0,1.8390055,1.5776986)` and default orbit
distance is `10.4996232`. The perspective uses the same X/Y projection scale
as the PS1 after normalizing the display viewports. Calibration against the
SDK's fixed-point perspective gives Q16 FOV `1757444` and aspect `87340`;
the generated matrix has Pxx=3.125 and Pyy approximately 4.1647.

These replace the former 60-degree automatic framing. The comparison studies
used a deliberately more distant PS1 view `(0,0,-50000)` to keep the profile
tail inside the viewport; that diagnostic zoom is not the original default
and is not used for the production endpoint.

The six-second approach and palette reveal remain authored presentation.
This change matches the endpoint, not every intermediate step of the original
PS1 entrance script. Lighting, textures and rasterization are still different.

## Verification

Four production-build Ymir runs check:

- natural entrance completion with no input;
- zooming, B reset and natural completion of the replayed entrance;
- B+A together, holding original Walk pose 0 at the restored endpoint;
- the existing 1200-field control/animation regression script.

The first three runs have identical camera state and matrix at their endpoints,
intro inactive, full palette gain, and the same default distance. The pose
capture uses ordinary controls; it does not override the camera or projection.
All four runs pass panic, task, draw-overflow and music-error checks on stock RAM.

An independent projection check compares the captured production matrix with
the observed GTE matrix across all 1376 original points in all 32 Walk poses.
The maximum ideal display-coordinate deviation is below 0.003 pixels. This is
a camera-equivalence check before GPU rounding and pose quantization, not a
rendered-pixel error score. The native Mednafen launcher was also executed.

The persistent result, original register values and application hash are in
`harness/studies/dino-camera-endpoint.json`. Local captures, camera snapshots,
runtime maps and logs are under `harness/build/dino_camera/`.
`endpoint-comparison.png` shows the original PS1 default view on the left and
the normal Saturn executable at the same Walk pose on the right. The complete
PS1 viewport is reflected horizontally to match coordinate conventions, and
both displays use nearest scaling. No independent silhouette resizing or
camera patch is used.
