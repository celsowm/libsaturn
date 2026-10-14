"""Produce a read-only machine-readable Saturn disc report."""
import argparse
import json
from pathlib import Path
from boot_image import inspect, read_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--app', type=Path)
    parser.add_argument('--strict-ip', action='store_true')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    report = inspect(read_image(args.image), args.app.read_bytes() if args.app else None, args.strict_ip)
    report['source'] = str(args.image)
    text = json.dumps(report, indent=2)+'\n'
    if args.out:
        args.out.write_text(text, encoding='utf-8')
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
