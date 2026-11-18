# C2 V1 — native screen clipping

Scope: existing uniform sprite/polygon screen clipper, not textured
arbitrary UV, world backface, near-plane or texture grid clipping.
Mode 0 preserves the original algorithm. Mode 1 tracks the four initial
outside-plane bits and skips a plane if all original points were inside.
Intersections cannot leave the coordinate interval of their endpoints
under truncation; deduplication is unchanged. No common-outcode early
reject that could hide existing unsupported cases for malformed quads.
Mode 2 additionally uses bounded signed 32-bit interpolation if both
multiplicands have magnitude <=32767, otherwise original widened
arithmetic, and 32-bit fan area once all vertices are inside <=2048
viewport. No new DIVU usage or default dispatch. No precision change.

16 workloads, 64 batches of 32 faces, serial master and two parallel
lanes in Ymir and Mednafen. Compare status, count, exact output slots,
unused output and guards against original for both winding directions,
inside, each edge, corners, outside, degenerate, repeated vertices,
int16 extremes, arbitrary input and viewport 2..2048. Rotate call
order. Timing is complete clipper, excluding setup/verification.
Differential baseline, not an independent geometric oracle. Measure
before pursuing implementation-specific Assembly. World near clip,
visibility, textured cells, real command stream and visual gates
remain separate. Target execution pending.

Run `python3 harness/scripts/run_screen_clip.py`. Return
`harness/build/screen_clip_bench-return.zip`, including local changes.
