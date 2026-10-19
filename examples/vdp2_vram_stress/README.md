# vdp2_vram_stress

The transactional VDP2 resource plan (`sat_vdp2_plan_*` in
`include/saturn/vdp2_alloc.h`) under load: seven deterministic scenarios drive
every documented success and refusal path on a live four-layer picture, and
the per-bank usage bars show the VRAM state as it changes.

## What runs, in frame order

| Frame | Scenario | What it proves |
|---|---|---|
| boot | 0 — legal layout | A `begin/reserve/reserve_at/commit` transaction spreads four layers over the banks (names at 0x00000/0x20000/0x40000/0x02000, character windows pinned 0x8000-aligned in each layer's bank); all four `sat_vdp2_layer_configure` calls succeed and the cycle patterns are snapshotted. |
| 90 | 1 — oversized refusal | A 0x40000 bitmap reservation is refused with `SAT_ERR_CAPACITY`; `rollback` restores the exact free space. |
| 120 | 2 — bank claim | B1 is whole-claimed for rotation: pattern names inside it are refused with `SAT_ERR_BUSY`, a line scroll table is accepted (it needs no cycle pattern, manual 5.3), the claim is released. |
| 150 | 3 — claim conflict | Claiming B0, which hosts committed NBG reservations, is refused with `SAT_ERR_BUSY` and changes nothing. |
| 180 | 4 — fragmentation | After committing two scratch blocks and releasing one, a 0x38000 request is refused although the total free space is larger: the fragmentation is reported, not hidden. |
| 210 | 5 - reclaim and reconfigure | Everything is released and a second, different layout (two banks instead of three) is planned, committed and configured live; the cycle patterns change and the picture keeps drawing. |
| 240 | 6 - CRAM transaction | A 16-code palette range is reserved and rolled back; a second transaction gets the same bank/code, commits it and releases it. Filling all seven CRAM banks makes the eighth request return `SAT_ERR_CAPACITY`; rollback leaves zero codes allocated. |

`g_vram_stress` (read by `harness/tests/test_vdp2_vram_stress.py` through the
Ymir probe) records every step, including the invariant that none of the
refusals touched the display registers: the cycle pattern snapshot after
every refusal scenario equals the one layout A committed, and the colour
bands are pixel-identical before and after them.

## Controls

- **A** advance to the next scenario at once (they also advance on their own
  at the fixed frames above, so an unattended run sees all of them)
- **B** toggle the debug numbers overlay (per-bank free/reserved KiB,
  scenario bitmask, refusal count, last status, CRAM codes used)
- **START** exit

## Limits

- The planner's byte ownership is the free list; VRAM ranges that a program
  never reserved through the plan are respected but carry no kind and appear
  in the telemetry only as missing free space.
- CRAM ranges now join the same transaction: the plan snapshots the
  seven-bank bitmap at `sat_vdp2_plan_begin()`, and `rollback` restores it
  exactly. The legacy `sat_vdp2_palette_*` pair shares the same bitmap and
  frees ranges for real.
- Character windows must be 0x8000-aligned (32 KiB windows, manual 4.x) and
  are pinned with `reserve_at`; the planner does not invent per-layer banks,
  the scene decides them.
- The cycle planning itself stays in the layer manager
  (`sat_vdp2_layer_configure`): this demo demonstrates that the plan and the
  manager compose, not that the plan replaces the manager. The cycle
  patterns are the remaining non-transactional part of the unified planner.
- Emulator evidence only: no claim about real hardware. The acceptance
  checklist row stays pending until a physical run is recorded.
