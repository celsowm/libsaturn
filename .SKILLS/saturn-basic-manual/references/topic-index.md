# Saturn Basic Manual Topic Index

Use this file to jump quickly to relevant parts of `docs/saturn-basic-manual.txt`.

## Source
- `docs/saturn-basic-manual.pdf`
- `docs/saturn-basic-manual.txt` (generated from PDF)

## Fast Search
```powershell
rg -n "SMPC|SCU|memory map|CD-ROM subsystem|VBL interrupt|HBL interrupt|VDP 1|VDP 2|0x06000000|0x5900000" docs/saturn-basic-manual.txt -S
```

## Entry Points
- System overview and chapter map:
  - `Chapter 1: The Saturn System`
  - `Chapter 2: Overview of VDP 1`
  - `Chapter 3: Overview of VDP 2`
  - `Chapter 4: Developing for Saturn`
- SCU architecture and DMA context:
  - Search: `System Control Unit (SCU)`
  - Search: `DMA`
  - Search: `integrate the A bus and B bus`
- SMPC reset/input behavior:
  - Search: `System Manager and Peripheral Control (SMPC)`
  - Search: `direct mode`
  - Search: `indirect mode`
- CD subsystem context:
  - Search: `CD-ROM subsystem`
  - Search: `SH-1`
  - Search: `512-KB data cache`
- Memory-map sanity checks:
  - Search: `Memory configuration`
  - Search: `preliminary Saturn memory map`
  - Search: `0x06000000`
  - Search: `0x5900000`
- VDP timing/high-level behavior:
  - Search: `VBL interrupt`
  - Search: `HBL interrupt`
  - Search: `command list`
  - Search: `scroll plane`

## Usage Notes
- Treat this manual as architecture-level guidance.
- Confirm all final register values and exact addresses in chip-specific manuals before coding.
