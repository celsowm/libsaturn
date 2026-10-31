# `vdp2_dual_rotation` — RBG0 + RBG1 on the Sega Saturn

A live tour of the Saturn VDP2's "second rotation screen" (manual p06_20):
RBG0 carrying the slate-blue framed grid, RBG1 carrying the
orange/dark-red fine checker on top of it.

The scene cycles through three modes that exercise every live path of
`sat_vdp2_rotation_*`:

| Mode | What's on screen | Live path |
| - | - | - |
| `0` A-ONLY | The grid alone, spinning slowly. | The implicit RBG0 cell layer. |
| `1` B-ON-RBG0 | The same grid with the orange checker pattern through the grid's transparent holes, the orange checker yawed 30° (parameter B). | `sat_vdp2_rotation_set_parameter_mode(B)`. |
| `2` DUAL | The same grid (parameter A on RBG0) with the orange checker pattern (RBG1, fixed parameter B) through the grid's transparent holes. | RBG1's separate VRAM-B image banks come on; RBG0's parameter mode is locked back to A. |

`g_dual_rotation` records the six refusal codes the manual mandates:

```c
a_out.rotation.refused_configure_mode2       /* RBG1 with parameter mode != A            -> SAT_ERR_INVALID_ARG */
a_out.rotation.refused_banks_in_vram_b    /* RBG0's image banks in VRAM-B             -> SAT_ERR_INVALID_ARG */
a_out.rotation.refused_map_outside_b1     /* parameter-set-B's map outside VRAM-B1    -> SAT_ERR_INVALID_ARG */
a_out.rotation.refused_nbg_while_dual     /* sat_vdp2_layer_configure(NBG0) while RBG1 -> SAT_ERR_BUSY        */
a_out.rotation.refused_rbg1_enable_in_mode_b /* sat_vdp2_rotation_rbg1_set_enabled(1) in mode B                 */
a_out.rotation.refused_mode_switch_while_rbg1 /* sat_vdp2_rotation_set_parameter_mode(B) while RBG1 displays   */
```

The VRAM layout (the manual's separate-image-bank contract):

- RBG0 pattern names in VRAM-A0 (bank mask 1)
- RBG0 character window in VRAM-A1 (bank mask 2)
- parameter-set-B (RBG1's names) in VRAM-B1 (forced by validation)
- RBG1 character window in VRAM-B0 (forced by validation)
- parameter region (`RPTA`) outside the RBG0 banks; with RBG1 hidden it can
  sit in VRAM-B0, the bank not claimed by either RBG0 layer

`A` on the pad skips ahead one mode, `B` toggles the debug overlay
(draws RBG1 / RBG0 VRAM-region and parameter mode on top of the picture),
`START` exits.

## Files

| Path | What it does |
| - | - |
| `examples/vdp2_dual_rotation/main.c` | the scene |
| `examples/vdp2_dual_rotation/README.md` | this document |
| `harness/run-vdp2-dual-rotation.ps1` | Ymir acceptance (BIOS-dependent) |
| `harness/tests/test_vdp2_dual_rotation.py` | reads `g_dual_rotation` through the map, takes the four per-mode screenshots |
| `tests/host/test_vdp2_rotation_rbg1.cpp` | the validation rules the manual mandates, as pure register expectations (35/35) |

## Build

```
make EXAMPLE=vdp2_dual_rotation all
```

produces `build/vdp2_dual_rotation.{iso,bin,elf,map}`.

## What it proves

- The manual's "RBG1 requires RBG0 to display and excludes any normal
  scroll screen" (p06_20) is enforced by `rot::validate` and the
  `nbg_configure` Busy check, not just left for the user to discover.
- The B-plane map lives in VRAM-B1 only when RBG1 is enabled; with
  RBG1 hidden, the same map can sit anywhere a name bank can (the
  E02 scene's B-mode switch still does).
- The HAL replay paints only the takeover registers when RBG1 is on
  (BGON R1ON + N0TPON, CHCTLA N0CHCN + N0CHSZ, PNCN0 N0PNB + palette +
  character-supplementary, PLSZ RBPLSZ + RBOVR, MPOFR RBMP, MPABRB..
  MPOPRB, OVPNRB, PRINA N0PRIN) and clears them when RBG1 hides so
  NBG0's bits aren't left stale on the shared register.