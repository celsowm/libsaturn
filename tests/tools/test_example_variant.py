import contextlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
import build_example_variant as variant
from cube_pipeline import make_program
from dino_pipeline import table_placement


class ExampleVariantTests(unittest.TestCase):
    def fixture(self,root):
        source=root/'examples/demo';source.mkdir(parents=True)
        (source/'main.c').write_text('int main(void) { return 0; }\n')
        (root/'Makefile').write_text('all:\n')
        (root/'build').mkdir()
        for suffix in variant.ARTIFACTS:(root/'build'/('demo'+suffix)).write_bytes(b'retained')
        real=root/'real-python';real.touch()
        python=root/'venv/bin/python';python.parent.mkdir(parents=True);python.symlink_to(real)
        return python

    def run_build(self,root,python,side_effect=None):
        with patch.object(variant,'ROOT',root),\
             patch.object(variant,'make_program',return_value='/gnu/make'),\
             patch.object(variant,'build_lock',return_value=contextlib.nullcontext()),\
             patch.object(variant.subprocess,'check_output',return_value='revision'),\
             patch.object(variant.subprocess,'run',side_effect=side_effect) as run:
            variant.build('demo',root/'retained',[],root/'tools',python)
        return run

    def test_retains_exact_outputs_and_preserves_virtual_environment_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);python=self.fixture(root)
            run=self.run_build(root,python)
            self.assertIn('PYTHON='+str(python),run.call_args.args[0])
            manifest=json.loads((root/'retained/variant.json').read_text())
            self.assertTrue(manifest['completed'])
            self.assertEqual(len(manifest['hashes']),len(variant.ARTIFACTS))
            for suffix in variant.ARTIFACTS:
                self.assertEqual((root/'retained'/('demo'+suffix)).read_bytes(),b'retained')

    def test_failed_compile_keeps_manifest_incomplete(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);python=self.fixture(root)
            with self.assertRaises(subprocess.CalledProcessError):
                self.run_build(root,python,subprocess.CalledProcessError(2,['make']))
            self.assertFalse(json.loads((root/'retained/variant.json').read_text())['completed'])

    def test_source_change_during_build_rejects_completion(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);python=self.fixture(root)
            def change(*args,**kwargs):(root/'examples/demo/main.c').write_text('changed')
            with self.assertRaisesRegex(ValueError,'source changed'):
                self.run_build(root,python,change)
            self.assertFalse(json.loads((root/'retained/variant.json').read_text())['completed'])

    def test_make_selection_rejects_old_macos_make(self):
        with patch('cube_pipeline.shutil.which',side_effect=lambda name:'/bin/'+name),\
             patch('cube_pipeline.subprocess.check_output',return_value='GNU Make 4.4'):
            self.assertEqual(make_program(),'/bin/gmake')
        with patch('cube_pipeline.shutil.which',side_effect=lambda name:None if name=='gmake' else '/usr/bin/make'),\
             patch('cube_pipeline.subprocess.check_output',return_value='GNU Make 3.81'):
            with self.assertRaisesRegex(RuntimeError,'4.3'):make_program()


class DinoTablePlacementTests(unittest.TestCase):
    def test_independent_banks_and_expected_placement(self):
        syms={'g_material_program_storage':(0x06010000,16000),
              'g_model_textures':(0x00210000,12000)}
        result=table_placement(syms,'high','low')
        self.assertEqual(result['g_material_program_storage']['ram'],'high')
        with self.assertRaises(ValueError):table_placement(syms,'low','low')
        with self.assertRaises(ValueError):table_placement({},'high')

    def test_invalid_ranges_and_empty_tables_reject(self):
        for address,size in ((0x060ff000,4097),(0x26010000,16000),(0x00200000,0)):
            with self.assertRaises(ValueError):
                table_placement({'g_material_program_storage':(address,size)})


if __name__=='__main__':unittest.main()
