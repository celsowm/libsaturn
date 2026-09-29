"""Ikemen SFF v2 -> LibSaturn asset pipeline.

Package layout (one responsibility per module):
  sff.py         -- SFF v2 container: header, palette nodes, sprite nodes.
  codecs.py      -- Pixel codec strategies (LZ5, RLE8, RLE5, raw, PNG).
  palettes.py    -- Palette materialization (SFFv2 RGBA -> BGR555).
  air.py         -- AIR animation parsing (frames, times, CLSN boxes).
  stage.py       -- Stage DEF-subset composition onto a Saturn plane.
  emit.py        -- C source emission for the example runtime.
  __main__.py    -- CLI wiring (modes: char, stage).

The example runtime never depends on any of this: it consumes the emitted
C tables only (dependency inversion).
"""
