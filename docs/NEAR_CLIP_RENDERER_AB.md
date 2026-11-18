# C2 — near-plane clipping renderer A/B

The synthetic mode 2 candidate is tested through the production
`sat_clip_quad_near` public API, with matching instrumented mode 1
(original implementation). The Make flag SAT_NEAR_CLIP_EXPERIMENT
defaults to zero, preserving the production build. The flag participates
in the existing library build key. No changes to screen clip dispatch
in this experiment: screen clip is original in both builds, as are
projection, skinning and hierarchy. Direct internal near-clip calls
elsewhere are not redirected by the public API flag.

The 320x224 example draws the same 29 world faces through the
public RGB, indexed solid and Gouraud immediate renderer. It uses a
Q16 camera basis with forward +Z or an oblique yaw basis (sine 39322,
cosine 52429), a matching perspective projection, and near depth 2
world units. Geometry is defined in camera coordinates and transformed
into world coordinates; all input points have positive W, some lie
behind the near plane. Two parallel world edges at depths 1 and 4
form the actual straddling faces. One wholly rejected face is present
in every pose.

24 measured frames, 12 poses, each repeated twice, in three phases:
- poses 0–3 axial crossing: 18 straddling faces;
- poses 4–7 oblique inside: 28 all-inside, one all-outside, no crossings;
- poses 8–11 oblique crossing: 18 straddling faces.
Each crossing emits two near-clipped fan pieces. Thus 29 near calls,
18 multi-piece calls and 46 pieces in crossing frames; 29, 0 and 28
in non-crossing frames. The decoder enforces these counts. This does
not claim that every near output is a visible screen primitive.

Reuse the screen renderer capture method: current VDP1 live command
words including END and all Gouraud tables for every measured frame,
zero capture tails. Two matched builds, each with Ymir, Mednafen timing
and Mednafen lossless visual execution. Require identical literal
command/table snapshots across builds and emulators, zero panic and
draw timeouts. Compare final frozen oblique crossing screenshots in
each emulator and Ymir done-triggered image. Intermediate poses are
checked by command bytes, not individual screenshots.

Report median clip, submission and end interval overall (exclude first
pair), and separately for each phase (exclude first pair in each phase).
No input-dependent simulation or asset randomness. Submission excludes
begin-frame wait and diagnostic snapshot; end is presentation call
only. Do not equate with FPS. Ymir DIVU immediate-result timing is not
physical divider latency; both emulators and eventually hardware are
needed. The scene deliberately stresses clipping; no claim about
Skybridge queued rendering or actual gameplay frequency. The experiment
does not qualify IRQ reentrancy of DIVU, combined C2 optimizations,
textured cells or physical console performance.

Run `python3 harness/scripts/run_near_clip_renderer.py`, return
`harness/build/near_clip_renderer-return.zip`. Failure logs and local
source changes included. Compilation and target execution pending.
