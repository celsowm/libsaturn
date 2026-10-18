# Additional primary manuals

These page-addressable Markdown transcriptions extend the existing Sega HTML
manual collection with CPU documentation and Saturn-specific supplements.

| Manual | Pages | Use |
| --- | ---: | --- |
| [SH7604 Hardware Manual](sh7604/README.md) | 633 | Cache/CCR, bus arbitration, DIVU, on-chip DMAC, FRT and register maps |
| [SH-1/SH-2 Programming Manual](sh1-sh2-programming/README.md) | 293 | Instructions, delay slots, multiplication, execution states and pipelines |
| [Saturn Dual CPU User's Guide](saturn-dual-cpu/README.md) | 12 | Sega's Master/Slave usage guidance and shared-bus considerations |
| [VDP1 User's Manual Supplement](vdp1-supplement/README.md) | 14 | Corrections and additions to the VDP1 manual |

There are 952 PDF pages in total. Each original page has an explicit heading,
a link back to that physical PDF page, and an extracted layout block. The
SH7604's PDF bookmarks are retained as chapter and subsection links. The
programming manual lacks PDF bookmarks; its section boundaries are recorded
explicitly in [sources.json](sources.json).

## Reading path for renderer performance

1. SH7604 sections 7 and 8: external bus behavior, cache control and coherence.
2. The Dual CPU User's Guide: which work to delegate and how CPUs share buses.
3. Programming Manual section 7: instruction scheduling and pipeline costs.
4. SH7604 sections 9 and 10: distinguish the on-chip DMAC and DIVU from SCU DMA.
5. The existing [VDP1 manual](../hard/vdp1/index.md), together with its supplement:
   command-table access, drawing completion and framebuffer switching.
6. The existing [SCU manual](../hard/scu_/index.md): DMA transfer restrictions.

The SH7604 is a general-purpose chip manual. Saturn wiring, memory layout and
SCU restrictions must also be checked against Sega's system manuals. Instruction
execution states do not by themselves account for Saturn bus contention.

## Reproduce

From the library root, use a Python environment with the pinned dependency:

```sh
python3 -m pip install -r tools/requirements-manuals.txt
python3 tools/convert_hardware_manuals.py --download
python3 tools/convert_hardware_manuals.py --check
```

The converter downloads only missing PDFs, verifies their catalog SHA-256
before extraction, and stores them under ignored `build/manuals/source/`.
Only Markdown, provenance manifests and the source catalog are committed.
To convert an existing local source, use `--source-dir /path/to/pdfs`.
Use `--manual sh7604` to select a single manual. Conversion uses the same
validation as `--check`: every page must occur exactly once in chapter order,
all chapter/bookmark links must resolve, and stale chapter files are rejected.

## Extraction quality

These are searchable text transcriptions, not facsimiles or manually verified
technical editions. Monospaced blocks preserve extracted table and assembly
spacing without inventing table cells or register interpretations. Diagrams,
arrows, symbols and rotated text can be missing or imperfect. Check the linked
PDF before relying on visual relationships, pinouts or timing diagrams.
Each manifest lists pages without extractable text; they are retained with
an explicit placeholder instead of being silently omitted. No OCR is claimed.

The sources are original Hitachi and Sega manuals mirrored by
[Antime's Saturn archive](https://antime.kapsi.fi/sega/docs.html). Original
notices and publisher attribution remain in the transcriptions; the library's
code license does not replace the publishers' notices.
