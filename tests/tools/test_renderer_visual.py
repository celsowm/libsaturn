import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"harness/scripts"))
from renderer_visual import crop,diff,model_box,write_rgb,load_rgb,compare_pair

class RendererVisualTests(unittest.TestCase):
    def test_diff_and_crop(self):
        a=bytes(18);b=bytearray(a);b[3]=5;b[15]=200
        report,mask=diff(a,b,3,2)
        self.assertEqual(report,{"different_pixels":2,"total_pixels":6,
                                 "max_channel_delta":200,"bounds":[1,0,3,2]})
        self.assertEqual(mask[3:6],bytes.fromhex("ff00ff"))
        self.assertEqual(crop(b,3,2,(1,0,3,2)),bytes(b[3:9]+b[12:18]))
        self.assertEqual(model_box(320,224),(0,48,320,192))
        self.assertEqual(model_box(640,448),(0,96,640,384))
        self.assertIsNone(model_box(352,240))
        self.assertIsNone(model_box(682,448))
        self.assertEqual(model_box(682,448,"mednafen"),(0,96,682,384))
        self.assertIsNone(model_box(683,448,"mednafen"))
        with self.assertRaises(ValueError):diff(a,b,4,2)

    @unittest.skipUnless(shutil.which("ffmpeg"),"ffmpeg not available")
    def test_png_reports(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); a=root/"a.png";b=root/"b.png"
            pixels=bytearray(320*224*3);pixels[(100*320+100)*3]=255
            write_rgb(a,320,224,pixels)
            self.assertEqual(load_rgb(a),(320,224,bytes(pixels)))
            pixels[0]=200
            write_rgb(b,320,224,pixels)
            result=compare_pair(a,b,root/"report",root)
            self.assertTrue(result["passed"])
            self.assertEqual(result["screen"]["different_pixels"],1)
            self.assertEqual(result["model"]["different_pixels"],0)
            pixels[(100*320+101)*3]=255
            write_rgb(b,320,224,pixels)
            result=compare_pair(a,b,root/"report",root)
            self.assertFalse(result["passed"])
            self.assertEqual(result["model"]["different_pixels"],1)
            self.assertEqual(load_rgb(root/"report/diff-crop.png")[:2],(320,144))
            write_rgb(a,320,224,bytes(320*224*3))
            write_rgb(b,320,224,bytes(320*224*3))
            self.assertFalse(compare_pair(a,b,root/"report",root)["passed"])

    @unittest.skipUnless(shutil.which("ffmpeg"),"ffmpeg not available")
    def test_mednafen_recorded_raster(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);a=root/"a.png";b=root/"b.png"
            w,h=682,448
            pixels=bytearray(w*h*3)
            pixels[(200*w+170)*3]=248
            write_rgb(a,w,h,pixels)
            pixels[(64*w+134)*3]=248 # frame counter outside model band
            write_rgb(b,w,h,pixels)
            result=compare_pair(a,b,root/"report",root,"mednafen")
            self.assertTrue(result["passed"])
            self.assertEqual(result["screen"]["different_pixels"],1)
            self.assertEqual(result["model"]["different_pixels"],0)
            self.assertEqual(load_rgb(root/"report/diff-crop.png")[:2],(682,288))
            pixels[(200*w+171)*3]=248
            write_rgb(b,w,h,pixels)
            self.assertFalse(compare_pair(a,b,root/"report",root,"mednafen")["passed"])

if __name__=="__main__":unittest.main()
