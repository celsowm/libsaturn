"""GPL-3.0: reject malformed diagnostic requests before firmware access."""
import os,subprocess,unittest
from pathlib import Path

class DebugProfileCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=Path(os.environ.get('LIBSATURN_PROBE','harness/build-ci/probe'))
        if not cls.probe.is_file():raise unittest.SkipTest('set LIBSATURN_PROBE')

    def test_invalid_values(self):
        for flag,values in {'--sh2-cache':['true','1','garbage'],
            '--trace-from':['-1','4294967296','1garbage'],
            '--trace-frames':['0','-1','garbage'],
            '--trace-capacity':['0','1048577','-1'],
            '--snapshot-pc':['-1','4294967296','bad'],
            '--video-crop':['0:0:0:1','0:0:1','-1:0:1:1','0:0:1:1x','0:0:4294967296:1']}.items():
            for value in values:
                with self.subTest(flag=flag,value=value):
                    r=subprocess.run([str(self.probe),flag,value],capture_output=True,text=True)
                    self.assertNotEqual(r.returncode,0)
                    self.assertIn('invalid '+flag,r.stderr)

    def test_window_must_intersect_program_frames(self):
        r=subprocess.run([str(self.probe),'--iso','unused','--bios','unused','--bin','unused',
            '--frames','10','--debug-profile','unused','--trace-from','10'],capture_output=True,text=True)
        self.assertNotEqual(r.returncode,0)
        self.assertIn('nonempty frame window',r.stderr)

    def test_trace_requires_output(self):
        r=subprocess.run([str(self.probe),'--iso','unused','--bios','unused','--bin','unused',
            '--snapshot-pc','0x6004000'],capture_output=True,text=True)
        self.assertNotEqual(r.returncode,0)
        self.assertIn('trace options require --debug-profile',r.stderr)

    def test_crop_requires_video_output(self):
        r=subprocess.run([str(self.probe),'--iso','unused','--bios','unused','--bin','unused',
            '--video-crop','0:0:1:1'],capture_output=True,text=True)
        self.assertNotEqual(r.returncode,0)
        self.assertIn('--video-crop requires --video-observations',r.stderr)

if __name__=='__main__':unittest.main()
