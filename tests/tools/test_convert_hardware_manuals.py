#!/usr/bin/env python3
"""Page coverage and provenance checks for the optional PDF documentation tool."""
import hashlib
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
AVAILABLE = all(importlib.util.find_spec(name) is not None for name in ('pypdf', 'fontTools'))
if AVAILABLE:
    from pypdf import PdfWriter
    from pypdf.generic import DictionaryObject, NameObject, DecodedStreamObject
    from convert_hardware_manuals import convert, check


@unittest.skipUnless(AVAILABLE, 'install tools/requirements-manuals.txt for PDF checks')
class ManualConversionTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.source = self.root/'manual.pdf'
        self.output = self.root/'markdown'
        writer = PdfWriter()
        for number in range(1, 4):
            page = writer.add_blank_page(width=300, height=300)
            font = DictionaryObject({NameObject('/Type'): NameObject('/Font'),
                                    NameObject('/Subtype'): NameObject('/Type1'),
                                    NameObject('/BaseFont'): NameObject('/Helvetica')})
            page[NameObject('/Resources')] = DictionaryObject({
                NameObject('/Font'): DictionaryObject({NameObject('/F1'): font})})
            stream = DecodedStreamObject()
            stream.set_data(f'BT /F1 12 Tf 20 260 Td (Page {number} unique text) Tj ET'.encode())
            page[NameObject('/Contents')] = writer._add_object(stream)
        writer.add_outline_item('Chapter one', 0)
        writer.add_outline_item('Chapter two', 2)
        writer.write(self.source)
        self.spec = dict(slug='fixture', title='Fixture', edition='Test edition',
                         filename='manual.pdf', url='https://example.invalid/manual.pdf',
                         sha256=hashlib.sha256(self.source.read_bytes()).hexdigest())

    def test_unbookmarked_middle_page_survives(self):
        manifest = convert(self.spec, self.source, self.output)
        self.assertEqual(check(self.output), 3)
        self.assertIn('Page 2 unique text',
                      (self.output/manifest['sections'][0]['file']).read_text())

    def test_missing_page_is_rejected(self):
        manifest = convert(self.spec, self.source, self.output)
        path = self.output/manifest['sections'][0]['file']
        path.write_text(path.read_text().replace('## PDF page 2', '## Missing page'))
        with self.assertRaises(ValueError):
            check(self.output)

    def test_source_hash_is_required(self):
        self.source.write_bytes(self.source.read_bytes()+b'changed')
        with self.assertRaisesRegex(ValueError, 'hash'):
            convert(self.spec, self.source, self.output)

    def test_explicit_sections_and_stale_chapters(self):
        self.spec['sections'] = [[1, 'Front matter'], [2, 'Body']]
        convert(self.spec, self.source, self.output)
        self.assertEqual(check(self.output), 3)
        (self.output/'stale.md').write_text('Old content')
        with self.assertRaisesRegex(ValueError, 'stale'):
            check(self.output)


if __name__ == '__main__':
    unittest.main()
