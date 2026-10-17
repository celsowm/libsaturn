"""GPL-3.0: validate the opt-in startup RAM experiment's CLI without BIOS."""
import os,subprocess,unittest
from pathlib import Path

class LowWramFillCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=Path(os.environ.get('LIBSATURN_PROBE','harness/build-ci/probe'))
        if not cls.probe.is_file():raise unittest.SkipTest('set LIBSATURN_PROBE to the built probe')

    def test_invalid_bytes_are_refused_before_loading_firmware(self):
        for value in ('-1','256','0x100','junk','12junk'):
            with self.subTest(value=value):
                r=subprocess.run([str(self.probe),'--wram-low-fill',value],capture_output=True,text=True)
                self.assertNotEqual(r.returncode,0)
                self.assertIn('invalid --wram-low-fill byte',r.stderr)

    def test_bios_boot_cannot_be_modified(self):
        r=subprocess.run([str(self.probe),'--iso','unused.iso','--bios','unused.bin',
            '--boot-mode','bios','--wram-low-fill','0xA5'],capture_output=True,text=True)
        self.assertNotEqual(r.returncode,0)
        self.assertIn('--wram-low-fill requires injection mode',r.stderr)

if __name__=='__main__':unittest.main()
