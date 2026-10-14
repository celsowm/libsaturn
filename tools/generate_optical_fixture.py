"""Generate a deterministic 32-sector optical-test fixture and its C metadata."""
import argparse
import hashlib
import zlib
from pathlib import Path


def fixture():
    data = bytearray((i*37+(i>>8)*11+0x5a)&255 for i in range(65536))
    data[:16] = b'LIBSATURN ODE V1'
    return bytes(data)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--header',type=Path,required=True)
    args=p.parse_args()
    data=fixture()
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_bytes(data)
    args.header.write_text('#define OET_FIXTURE_BYTES 65536u\n'
        f'#define OET_FIXTURE_CRC 0x{zlib.crc32(data):08x}u\n'
        f'#define OET_FIXTURE_SHA256 "{hashlib.sha256(data).hexdigest()}"\n',encoding='ascii')


if __name__=='__main__':
    main()
