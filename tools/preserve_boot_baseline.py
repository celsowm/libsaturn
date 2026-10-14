"""Archive original releases and report their boot data without rebuilding them."""
import argparse
import json
import shutil
import zipfile
from pathlib import Path
from boot_image import digest, inspect, read_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', action='append', type=Path, default=[])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    reports = []
    for archive in args.archive:
        sha = digest(archive.read_bytes())
        dest = args.out / sha
        dest.mkdir(exist_ok=True)
        saved = dest / archive.name
        if saved.exists() and digest(saved.read_bytes()) != sha:
            raise ValueError('refusing to overwrite changed archive')
        shutil.copyfile(archive, saved)
        report = dict(source=str(archive.resolve()), archive_sha256=sha,
                      physical_satiator_pass='unverified', physical_saroo_pass='unverified', files=[])
        with zipfile.ZipFile(saved) as zip_file:
            for entry in zip_file.infolist():
                if entry.is_dir():
                    continue
                name = Path(entry.filename)
                if name.name != entry.filename or entry.file_size > 64*1024*1024:
                    raise ValueError('unsupported archive member')
                payload = zip_file.read(entry)
                target = dest / name
                if target.exists() and target.read_bytes() != payload:
                    raise ValueError('immutable archive extraction changed')
                target.write_bytes(payload)
                report['files'].append(dict(name=str(name), bytes=len(payload), sha256=digest(payload)))
        image = next(iter(sorted(dest.glob('*.iso'))), None) or next(iter(sorted(dest.glob('*.cue'))), None)
        if image:
            logical = read_image(image)
            report['boot'] = inspect(logical)
            (dest / 'system-area.bin').write_bytes(logical[:32768])
            first = report['boot']['root'][0]
            (dest / 'first-read.bin').write_bytes(logical[first['lba']*2048: first['lba']*2048+first['size']])
        (dest / 'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        reports.append(report)
    (args.out / 'originals.json').write_text(json.dumps(reports, indent=2)+'\n', encoding='utf-8')
    print(f'[baseline] preserved {len(reports)} original archives in {args.out}')


if __name__ == '__main__':
    main()
