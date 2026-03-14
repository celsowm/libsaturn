---
name: saturn-basic-manual
description: Use this skill as a system-level Sega Saturn reference from `docs/saturn-basic-manual.pdf` when working on boot/debug and platform architecture topics. Trigger for issues like boot stuck on the Sega license screen, SMPC reset/input mode sequencing, SCU DMA and bus behavior, SH-2 memory map assumptions, CD subsystem interactions, and high-level VDP1/VDP2 pipeline decisions before low-level register coding.
---

# Saturn Basic Manual Reference

## Overview

Use this skill to ground Saturn bring-up decisions in the basic manual before touching low-level code.
Use it for architecture, sequencing, and debugging hypotheses; use dedicated hardware manuals for exact register bitfields.

## Quick Start

1. Ensure text extraction exists:
```powershell
C:\Tools\MuPDF\mupdf-1.25.0-windows\mutool.exe draw -F txt -o docs/saturn-basic-manual.txt docs/saturn-basic-manual.pdf
```
2. Search the extracted manual:
```powershell
rg -n "SMPC|SCU|memory map|CD-ROM subsystem|VBL interrupt|HBL interrupt|VDP 1|VDP 2" docs/saturn-basic-manual.txt -S
```
3. If you need precise registers, switch to the deeper technical skills/manuals after identifying the right subsystem here.

## Workflow

### 1. Boot and Bring-Up Diagnosis
- Use this manual to validate system assumptions first:
- SMPC role for reset, RTC, and controller polling modes.
- SCU role as bus bridge/DMA and interrupt path.
- CD subsystem as independent SH-1 + cache path.
- Then correlate those assumptions with current `IP.BIN/1ST_READ.BIN` packaging scripts and runtime behavior.

### 2. Memory-Map Sanity Check
- Use the manual's memory-configuration chapter to validate address-space assumptions before adding DMA/VRAM writes.
- Treat addresses in this manual as introductory/preliminary; confirm final addresses in dedicated chip manuals when implementing.

### 3. Video Pipeline Context
- Use VDP1/VDP2 overview chapters to reason about:
- VBL/HBL update timing.
- Command-list and framebuffer flow for VDP1.
- Scroll/background composition priorities for VDP2.
- For exact register programming, immediately follow with the specialized VDP manuals/skills.

## Limits

- This manual is not a complete register reference.
- Do not treat preliminary diagrams as authoritative for final bit-level implementation.
- Always escalate to chip-specific manuals for final code.

## References

- Use [references/topic-index.md](references/topic-index.md) for search patterns and section entry points.
- Source files:
  - `docs/saturn-basic-manual.pdf`
  - `docs/saturn-basic-manual.txt`
