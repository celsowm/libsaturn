# B3 selective skinning consumer qualification

The public plain-deform path is the only experimental dispatch change.
Two matched Robot Police builds use the existing fixed 48-step viewer,
serial skinning and identical original hierarchy instrumentation
(`SAT_HIERARCHY_EXPERIMENT=1`, not the optimized B2 path); B1 disabled.
`SAT_SKIN_CHARACTER_EXPERIMENT=1` is the original control, 2 is
selective V2. Default is 0. Placed/projected paths are unchanged.
Mode is reported from the linked library, rather than a viewer-only flag.

Before the consumer test, run the extended synthetic V3 gate: unchanged
V2 scenarios plus Q10/Q12/Q0 unequal pairs and a 96-vertex Q10 proxy
(22 rigid,40 pairs,34 triples), matched V1/V2/control with guard checks.
The report grows to 2048 bytes, 16 scenarios, version 3. The old V1/V2
reports remain documented; current decoder expects version 3. Q10 proxy
is a distribution approximation, not the actual asset or joint locality.

Compare all 48 step hashes and command/face counts, final full matrices
and deformed vertices, independent Ymir / native Mednafen timing,
lossless late images and Ymir done-triggered image stability. Save exact
generated Robot Police C SHA256 and weight inventory per build, reject
changed source between builds. The skin interval includes copying joint
matrices as in B2; no inference of isolated kernel speed or physical FPS.

Run `python3 harness/scripts/run_weighted_character.py`.
One `harness/build/weighted_b3-qualification-return.zip` contains nested
synthetic and character archives, logs, images and local changes.
A failed synthetic gate prevents the character run and returns
failure evidence. No default changes. Build and target execution pending.
