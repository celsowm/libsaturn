# Original character example family

Shared application code for `skeletal_animation_3d`, `fighter_arena_3d` and
`skinning_stress_3d`. This directory is not a standalone executable.
`demo.c` owns presentation and instrumentation; `combat.c` contains only
example-local deterministic fighting rules. The reusable runtime stays in
LibSaturn's animation, scene, texture, asset and parallel APIs.

`tools/generate_character_examples.py` authors the humanoid, 24 indexed
texture patterns, two costume palettes, nine motion clips and three LODs. There are
24 named joints, directional baked surface tones and a single face panel; connected elbow/knee rings use two joint-local anchors.
The source flows through the existing `SourceModel` and skeletal compiler.
Geometry, textures and motion are original project content under the
[MIT license](../../LICENSE). No commercial game data is required or included.
The optional robot viewer retains its separate CC BY 4.0 attribution.

The standard Makefile generates C geometry and relocatable SKA1 archives in
`build/generated/character_examples/`, packages the six costume/LOD archives
on disc, and keeps animation data resident with arbitrary clip-time access.
The normal automated configuration reads the same archive through a registered
blob and the existing asset cache. Set `CHARACTER_DEMO_DISC_ASSETS=1` when
building to use CDFS and the actual disc files instead. This explicit setting
avoids assuming that Ymir's injected-app CD path can boot/read the disc.
The disc path was separately exercised in Mednafen without expansion RAM.

All actor, mesh, scene, job, archive and descriptor storage is bounded and
caller-owned. Textures use the existing managed allocator and upload once.
Round reset waits at a frame boundary, releases texture owners and asset views,
then reloads the selected two character resources. It does not reset the global
texture registry or invalidate another live character's resources.

Simulation uses a fixed 60 Hz accumulator, independent of rendering. No
elapsed steps are dropped to improve display throughput. Clip sampling,
skeleton evaluation and deformation are performed for the current render pose;
animation events and combat advance on simulation ticks. Shader/LOD changes
do not rebuild textures per frame.

The HUD reports submitted-loop throughput, not physical presentation FPS.
Diagnostic snapshots publish completed frames. The stress log records FRT,
timer and VBlank measurements with a bounded sample capacity; analysis never
counts intervening configurations between separated runs of the same setting.

See [CPU skinning](../../docs/CPU_SKINNING_3D.md),
[character rendering](../../docs/CHARACTER_RENDERING_3D.md) and
[fighting-game readiness](../../docs/FIGHTING_GAME_READINESS.md).
