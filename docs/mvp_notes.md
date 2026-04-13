# MVP Notes

## Implemented Scope

- Video: NTSC 320x224.
- Fixed endianness: SH2 big-endian (`-mb`).
- Public API without `float`.
- No implicit dynamic allocation in core runtime.

## Out of MVP

- Complete 3D pipeline.
- Advanced SCU DSP.
- Complete M68k/SCSP driver.
- Master/Slave SH2 multiprocessing for gameplay.

## Acceptance criteria for manual execution

1. Complete build without manual intervention after setup.
2. ISO starts in emulator and enters main loop.
3. Stable 2D sprite rendering for 1800 frames.
4. Input without ghost presses for 5 minutes.
5. Asset converter covered by automated test.