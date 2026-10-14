"""RGB555 output uses Saturn word order and an exact SLZ1 round trip."""
import sys
import tempfile
import unittest
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from convert_vdp2_rgb555 import convert
from satlz import decompress


class BitmapTests(unittest.TestCase):
    def test_color_word_order_and_exact_compression(self):
        with tempfile.TemporaryDirectory() as folder:
            directory=Path(folder)
            image=Image.new('RGB',(512,256),(255,0,0))
            image.putpixel((1,0),(0,255,0));image.putpixel((2,0),(0,0,255))
            source=directory/'source.png';image.save(source)
            prefix=directory/'crowd';convert(source,prefix,'crowd')
            c=prefix.with_suffix('.c').read_text()
            import re
            packed=bytes(int(word,16) for word in re.findall(r'0x([0-9A-Fa-f]{2})\b',c))
            decoded=decompress(packed)
            self.assertEqual(len(decoded),512*256*2)
            self.assertEqual(decoded[:6],bytes.fromhex('801f83e0fc00'))
            self.assertIn('CROWD_WIDTH 512u',prefix.with_suffix('.h').read_text())

    def test_unsupported_bitmap_dimensions_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'source.png';Image.new('RGB',(320,224)).save(source)
            with self.assertRaises(ValueError):convert(source,Path(folder)/'crowd','crowd')


if __name__=='__main__':unittest.main()
