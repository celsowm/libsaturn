"""Validate Saturn metadata and the actual ISO9660 first-read file."""
import argparse
import sys
from pathlib import Path
from boot_image import inspect, read_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', required=True, type=Path)
    parser.add_argument('--expected-size', required=True, type=int)
    parser.add_argument('--profile', default='current')
    parser.add_argument('--load-addr', default='0x06004000')
    parser.add_argument('--app', type=Path)
    parser.add_argument('--strict-ip', action='store_true')
    args = parser.parse_args()
    try:
        report = inspect(read_image(args.iso), args.app.read_bytes() if args.app else None, args.strict_ip)
        ip = report['ip']
        if ip['first_read_address'] != int(args.load_addr, 16):
            raise ValueError('wrong first-read address')
        if ip['first_read_size'] != (0 if args.profile == 'safe' else args.expected_size):
            raise ValueError('wrong first-read header size')
        if report['root'][0]['size'] != args.expected_size:
            raise ValueError('wrong actual first-read length')
        print(f"[check] ISO first-read OK: {report['root'][0]} IP observations={ip['issues']}")
    except (OSError, ValueError) as error:
        print(f'[check] ISO FAIL: {error}', file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == '__main__':
    main()
