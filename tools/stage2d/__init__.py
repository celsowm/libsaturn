"""Generic offline tools for the 2D runtime modules (terrain2, stage_map2, entity_stream2,
sprite_clip, path2).

Everything here is game-neutral: it reads a JSON spec and writes C arrays that match the runtime
structs one to one. Importers for any particular game belong outside this package (tools/import)
and emit this spec.
"""

from .build import build_stage, load_spec
from .emit_c import emit_c
from .errors import Stage2dError

__all__ = ["build_stage", "load_spec", "emit_c", "Stage2dError"]
