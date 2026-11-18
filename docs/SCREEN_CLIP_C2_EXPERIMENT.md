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

## V1 target results — 2026-11-17

Checkout `129f7d52214f9e8967758026d8bb44d65f81a8a9`.
Both 1664-byte reports independently decoded as big-endian words
and matched against JSON header, lane and scenario fields. Separate
Mednafen archive duplicates all Mednafen members of combined archive.
Complete local manifest, empty staged/unstaged/combined tracked patches,
no nonignored untracked files.

Six lanes completed, 6144 batches, 196608 input quads and 363516
successful output primitives per implementation, zero recorded errors.
Parallel init and execution status zero, slave ready, CCR 1 and SR.S
clear on lane entry. Both timers 210540 Hz. Ymir 9000 fields, target
unittest pass, no reported misaligned access on either CPU. Mednafen
native CD PASS, exit 0, swapped16 WRAM normalization, ELF SHA256
`abbcf5cd463712c25fbaf9fa533194345531ffc338497e1ec588d07f64f8c727`.
No explicit panic dump, Ymir ELF identity check or real scene comparison.

### Serial Master FRT sums (64 batches of 32)

| Scenario | Ymir control / planes / bounded | Mednafen control / planes / bounded | Bounded reduction Ymir / Mednafen |
|---|---:|---:|---:|
| inside | 6312 / 6348 / 6384 | 6544 / 6488 / 6580 | -1.14% / -0.55% |
| left | 42256 / 26706 / 20919 | 43360 / 27857 / 21750 | 50.49% / 49.84% |
| right | 42480 / 26360 / 20704 | 43535 / 27505 / 21520 | 51.26% / 50.57% |
| top | 42036 / 26294 / 20660 | 43192 / 27378 / 21540 | 50.85% / 50.13% |
| bottom | 41911 / 26253 / 20492 | 43100 / 27339 / 21398 | 51.11% / 50.35% |
| corner | 51843 / 42530 / 30790 | 53474 / 44308 / 31941 | 40.61% / 40.27% |
| enclosing | 71088 / 73889 / 50435 | 73662 / 76981 / 52178 | 29.05% / 29.17% |
| outside | 14594 / 9627 / 9623 | 14993 / 10056 / 10082 | 34.06% / 32.76% |
| diamond | 84607 / 88125 / 64078 | 87451 / 91697 / 66381 | 24.26% / 24.09% |
| triangle | 52764 / 44194 / 31951 | 54491 / 46044 / 33186 | 39.45% / 39.10% |
| degenerate | 5289 / 5421 / 5386 | 5567 / 5612 / 5656 | -1.83% / -1.60% |
| int16_extreme | 69032 / 71846 / 56202 | 71587 / 74924 / 58543 | 18.59% / 18.22% |
| arbitrary | 53158 / 54854 / 40587 | 54954 / 57213 / 42078 | 23.65% / 23.43% |
| max_viewport | 24654 / 16713 / 13789 | 25288 / 17360 / 14290 | 44.07% / 43.49% |
| odd_viewport | 24386 / 16800 / 13848 | 25019 / 17434 / 14323 | 43.21% / 42.75% |
| tiny_viewport | 70967 / 73761 / 50439 | 73532 / 76845 / 52187 | 28.93% / 29.03% |

Maximum bounded reduction 51.275% (Ymir parallel slave, right)
and 50.733% (Mednafen parallel master, right). Across equally
repeated synthetic scenarios, sums of raw ticks give bounded
reductions 34.571–34.578% Ymir and 34.195–34.348% Mednafen,
versus planes-only 12.569–12.582% and 11.769–11.811%.
These are not representative game weights or measured frame savings.
Output primitive counts match, including 1453 per lane in arbitrary
input; status/output equivalence is against the original, not proof of
geometric correctness for malformed or nonconvex inputs.

**Disposition:** combined mode 2 passes differential synthetic gate
and merits a matched real consumer A/B. Keep default dispatch unchanged.
First identify actual screen-clip call sites and representative partially
visible scenes. Measure clip incidence, clip stage and total work, compare
full emitted VDP1 command bytes/order and fixed-pose screenshots,
preserve texture/UV rules. Separately qualify near-plane and world
visibility; this experiment did not optimize those or textured cell
clipping. No need to introduce DIVU or additional assembly if portable
bounded arithmetic already achieves the gain. Explicit IRQ/ABI and
physical console proof remain pending.
