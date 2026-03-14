# VRAM Cycle Patterns

VRAM on the VDP2 is split into banks. Each bank must have a **Cycle Pattern** defined to coordinate access between the VDP2 (renderer), the CPU, and DMA.

## Cycle Pattern Encoding (4-bits per access)
- `0x0`: NBG0 Character/Pattern Name
- `0x1`: NBG1 Character/Pattern Name
- `0x2`: NBG2 Character/Pattern Name
- `0x3`: NBG3 Character/Pattern Name
- `0x4`: RBG0 Character/Pattern Name
- `0x7`: CPU/DMA Access
- `0xE`: VDP1 Draw (for VDP1 integration)
- `0xF`: No Access (Idle)

## Register Mapping
- `VRAM_CYC0`: [Access 7][Access 6][Access 5][Access 4][Access 3][Access 2][Access 1][Access 0] for Bank A0.
- `VRAM_CYC1`: Same for Bank A1.

## Typical Setup (NBG0 enabled, 256 colors)
- Pattern: `0x00777777` (Two cycles for NBG0, six for CPU/DMA).
- If NBG0 uses more bandwidth (e.g. 16bit bitmap), more `0x0` cycles are needed.
- Failure to set patterns correctly results in "garbled" graphics or "tearing".
