#!/usr/bin/env python3
"""Convert pinned hardware PDFs into searchable, page-addressable Markdown."""
import argparse
import hashlib
import json
import re
import urllib.request
from pathlib import Path

import pypdf
import fontTools
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'docs/sega_saturn_hardware/manuals/sources.json'


def slugify(title):
    return re.sub(r'[^a-z0-9]+', '-', title.lower()).strip('-') or 'section'


def outlines(reader, nodes=None, depth=0):
    """Retain bookmark nesting and physical PDF page numbers."""
    result = []
    for node in reader.outline if nodes is None else nodes:
        if isinstance(node, list):
            result.extend(outlines(reader, node, depth + 1))
        else:
            page = reader.get_destination_page_number(node)
            if page is not None:
                result.append((page + 1, str(node['/Title']), depth))
    return result


def sections_for(reader, spec):
    entries = spec.get('sections') or [
        (page, title) for page, title, depth in outlines(reader) if depth == 0]
    entries = sorted(entries or [(1, 'Manual')])
    if entries[0][0] != 1:
        entries.insert(0, (1, 'Front matter'))
    if len({page for page, _ in entries}) != len(entries):
        raise ValueError('chapter boundaries must have distinct pages')
    if any(not 1 <= page <= len(reader.pages) for page, _ in entries):
        raise ValueError('chapter boundary outside PDF')
    return [dict(title=title, first=first,
                 last=entries[i+1][0]-1 if i+1 < len(entries) else len(reader.pages),
                 file=f'{i:02d}-{slugify(title)}.md')
            for i, (first, title) in enumerate(entries)]


def convert(spec, source, output):
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != spec['sha256']:
        raise ValueError(f'{source}: source hash differs from catalog')
    reader = PdfReader(source)
    sections = sections_for(reader, spec)
    bookmarks = outlines(reader)
    output.mkdir(parents=True, exist_ok=True)
    blank_pages = []
    characters = 0
    for section in sections:
        first, last = section['first'], section['last']
        lines = [f"# {section['title']}", '', f"{spec['title']} — {spec['edition']}.", '',
                 f"[Manual index](README.md) · [Source PDF]({spec['url']}#page={first})", '',
                 f'Physical PDF pages {first}–{last}. Text extraction; consult the PDF for figures.', '']
        children = [(page, title, depth) for page, title, depth in bookmarks
                    if first <= page <= last and depth > 0]
        if children:
            lines += ['## Bookmarks', '']
            lines += [f"- [{title}](#pdf-page-{page})" for page, title, _ in children]
            lines += ['']
        for page_number in range(first, last + 1):
            page = reader.pages[page_number - 1]
            # Layout mode keeps instruction examples, register bit columns and
            # table rows aligned instead of pretending to reconstruct semantics.
            extracted = page.extract_text(extraction_mode='layout',
                                          layout_mode_strip_rotated=False) or ''
            text = '\n'.join(line.rstrip() for line in extracted.splitlines()).rstrip()
            characters += len(text)
            if not text.strip():
                blank_pages.append(page_number)
            fence = '`' * max(3, max((len(m[0]) + 1 for m in re.finditer(r'`+', text)), default=3))
            lines += [f'## PDF page {page_number}', '',
                      f"[Original page]({spec['url']}#page={page_number})", '',
                      f'{fence}text', text or '[No extractable text on this page.]', fence, '']
        (output / section['file']).write_text('\n'.join(lines), encoding='utf-8')
    if characters == 0:
        raise ValueError(f'{source}: no extractable text; OCR is required')
    manifest = dict(spec, pages=len(reader.pages), sections=sections,
                    extractor='pypdf ' + pypdf.__version__, fonttools=fontTools.__version__,
                    blank_text_pages=blank_pages,
                    text_characters=characters)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False)+'\n')
    index = [f"# {spec['title']}", '', spec['edition'] + '.', '',
             f"[Original PDF]({spec['url']}) · [Collection index](../README.md)", '',
             f"Source SHA-256: `{spec['sha256']}`.", '',
             f"All {len(reader.pages)} physical PDF pages are represented below, including front matter.", '',
             'This is a searchable text transcription, not a facsimile. Layout blocks preserve',
             'the extracted spacing of tables and assembly listings. Vector diagrams, arrows,',
             'fonts and spatial relationships may be absent or imperfect; each page links to',
             'its PDF original. PDF page numbers differ from printed manual page numbers.',
             'Original authorship and notices remain with the publisher.', '',
             '| Chapter | PDF pages |', '| --- | --- |']
    index += [f"| [{s['title']}]({s['file']}) | {s['first']}–{s['last']} |" for s in sections]
    index += ['', f"Pages without extractable text: {', '.join(map(str, blank_pages)) or 'none'}.", '']
    (output / 'README.md').write_text('\n'.join(index), encoding='utf-8')
    return manifest


def check(output):
    """Fail on missing/duplicated pages, broken chapter links or stale files."""
    manifest = json.loads((output / 'manifest.json').read_text())
    pages = []
    expected = {'README.md'}
    index = (output / 'README.md').read_text()
    for section in manifest['sections']:
        expected.add(section['file'])
        text = (output / section['file']).read_text()
        actual = [int(p) for p in re.findall(r'^## PDF page (\d+)$', text, re.M)]
        if actual != list(range(section['first'], section['last']+1)):
            raise ValueError('missing, duplicate or reordered chapter pages')
        if f"]({section['file']})" not in index:
            raise ValueError('missing chapter index link')
        for target in re.findall(r'\]\(#pdf-page-(\d+)\)', text):
            if int(target) not in actual:
                raise ValueError('bookmark points outside chapter')
        pages.extend(actual)
    if pages != list(range(1, manifest['pages']+1)):
        raise ValueError('manual page coverage is incomplete')
    if {p.name for p in output.glob('*.md')} != expected:
        raise ValueError('unexpected Markdown file; remove stale chapters explicitly')
    return manifest['pages']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path, default=ROOT/'build/manuals/source')
    parser.add_argument('--output-dir', type=Path, default=CATALOG.parent)
    parser.add_argument('--download', action='store_true', help='download missing pinned PDFs')
    parser.add_argument('--check', action='store_true', help='validate existing output without PDFs')
    parser.add_argument('--manual', action='append', help='only process the named catalog slug')
    args = parser.parse_args()
    catalog = json.loads(CATALOG.read_text())
    names = {m['slug'] for m in catalog}
    if args.manual and set(args.manual)-names:
        parser.error('unknown manual slug')
    for spec in catalog:
        if args.manual and spec['slug'] not in args.manual:
            continue
        output = args.output_dir/spec['slug']
        if not args.check:
            source = args.source_dir/spec['filename']
            if args.download and not source.exists():
                source.parent.mkdir(parents=True, exist_ok=True)
                with urllib.request.urlopen(spec['url'], timeout=60) as response:
                    data = response.read()
                if hashlib.sha256(data).hexdigest() != spec['sha256']:
                    raise ValueError('download hash differs from catalog')
                source.write_bytes(data)
            convert(spec, source, output)
        count = check(output)
        print(f"{spec['slug']}: {count} pages OK")


if __name__ == '__main__':
    main()
