"""The existing importer can compile a GLB with no authored animations."""
import sys
import tempfile
import unittest
from pathlib import Path

try:
    import numpy  # noqa: F401  (the importer's quality metrics need it)
    HAVE_NUMPY=True
except ImportError:
    HAVE_NUMPY=False

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tests'))
from test_gltf_pipeline import build_skinned_gltf
from model_pipeline.gltf import parse_glb_bytes
from model_pipeline.animated_import import import_animated_model


class RestTests(unittest.TestCase):
    def test_lut4_ignores_hidden_colors_and_accepts_empty_mask(self):
        from model_pipeline.texture_bake import quantize_face_lut4
        hidden=[(i*17,255-i*17,0,0) for i in range(7)]
        lut,pixels=quantize_face_lut4(hidden+[(255,255,255,255)],8,1)
        self.assertEqual(lut[0],0)
        self.assertEqual(set(lut[1:]),{0xffff})
        self.assertEqual(pixels,bytes([0,0,0,1]))
        lut,pixels=quantize_face_lut4(hidden+[(0,0,0,0)],8,1)
        self.assertEqual(lut,[0]*16)
        self.assertEqual(pixels,bytes(4))

    @unittest.skipUnless(HAVE_NUMPY,'importer quality metrics need numpy')
    def test_lut4_export_preserves_masked_holes(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'masked.glb';path.write_bytes(build_skinned_gltf())
            face=[(255,0,0,0),(255,0,0,255)]*4
            with patch('model_pipeline.animated_import.estimate_face_size',return_value=(8,1)), \
                 patch('model_pipeline.animated_import.bake_face_rgba',return_value=face):
                result=import_animated_model(path,animation='rest',simplify='off',
                                             animation_fps='1',texture_format='lut4')
        for texture in result.static.textures:
            self.assertEqual(texture['flags']&1,0)
            self.assertTrue(texture['flags']&0x8000)
            self.assertEqual(bytes(texture['pixels']),bytes([0x01]*4))
        self.assertEqual(result.static.luts_rgb555[0],0)

    @unittest.skipUnless(HAVE_NUMPY,'importer quality metrics need numpy')
    def test_no_authored_clips_can_export_one_static_pose(self):
        import json, struct
        parsed=parse_glb_bytes(build_skinned_gltf())
        parsed.json.pop('animations')
        json_bytes=json.dumps(parsed.json,separators=(',',':')).encode()
        json_bytes+=b' '*((-len(json_bytes))%4)
        binary=parsed.bin+b'\0'*((-len(parsed.bin))%4)
        data=(struct.pack('<4sII',b'glTF',2,28+len(json_bytes)+len(binary))+
              struct.pack('<II',len(json_bytes),0x4E4F534A)+json_bytes+
              struct.pack('<II',len(binary),0x004E4942)+binary)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'static.glb';path.write_bytes(data)
            result=import_animated_model(path,animation='rest',simplify='off',animation_fps='1')
        self.assertEqual(len(result.animations),1)
        self.assertEqual(result.animations[0]['frame_count'],1)
        self.assertEqual(len(result.static.indices_abcd),1)


if __name__=='__main__':unittest.main()
