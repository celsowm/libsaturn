import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"harness/scripts"))
from local_changes import capture,add_to_zip

class LocalChangesTests(unittest.TestCase):
    def test_staged_unstaged_and_untracked(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def git(*args):
                return subprocess.check_output(["git",*args],cwd=root)
            git("init","-q")
            git("config","user.name","Test")
            git("config","user.email","test@example.invalid")
            (root/"src").mkdir()
            p=root/"src/a.cpp";p.write_text("base\n")
            git("add",".");git("commit","-qm","initial")
            p.write_text("staged\n");git("add","src/a.cpp")
            p.write_text("unstaged\n")
            (root/"src/new.cpp").write_text("new source\n")
            (root/"src/credential.txt").write_text("do not capture")
            (root/".DS_Store").write_bytes(b"\0")
            (root/"src/link.cpp").symlink_to("/etc/passwd")
            before=git("status","--porcelain")
            members=capture(root)
            self.assertEqual(before,git("status","--porcelain"))
            self.assertIn(b"+staged",members["staged.patch"])
            self.assertIn(b"+unstaged",members["unstaged.patch"])
            self.assertIn(b"+unstaged",members["tracked.patch"])
            self.assertIn("untracked/src/new.cpp",members)
            self.assertNotIn("untracked/src/credential.txt",members)
            self.assertNotIn("untracked/src/link.cpp",members)
            self.assertNotIn("untracked/.DS_Store",members)
            info=json.loads(members["manifest.json"])
            self.assertTrue(info["complete"])
            self.assertEqual(len(info["untracked"]),4)
            dest=root/"return.zip"
            with zipfile.ZipFile(dest,"w") as z:add_to_zip(z,members)
            with zipfile.ZipFile(dest) as z:
                self.assertEqual(z.read("local-changes/tracked.patch"),members["tracked.patch"])
if __name__=="__main__":unittest.main()
