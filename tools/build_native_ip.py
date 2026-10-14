"""Compose a native Saturn IP using explicit provenance-tracked input payloads."""
import argparse
import json
import struct
from datetime import datetime, timezone
from pathlib import Path
import os
try:
    from boot_image import APP_LOAD, IP_LOAD, SYSTEM_AREA, MAGIC, digest, parse_ip
except ImportError:
    from tools.boot_image import APP_LOAD, IP_LOAD, SYSTEM_AREA, MAGIC, digest, parse_ip


def compose(manifest_path: Path, aip: bytes, app: bytes, title='LIBSATURN', product='T-00000G',
            version='V0.001', date='20261014') -> tuple[bytes, dict]:
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest.get('schema_version') != 1 or not isinstance(manifest.get('redistributable'), bool):
        raise ValueError('unsupported/missing payload manifest policy')
    def load(item, size):
        if not item.get('origin') or not item.get('license'):
            raise ValueError('payload requires origin and license policy')
        data = (manifest_path.parent / item['file']).read_bytes()
        if len(data) != size or digest(data) != item['sha256'] or not any(data):
            raise ValueError('payload length/hash/content mismatch')
        return data
    security = load(manifest['security'], 0xd00)
    regions = manifest['regions']
    symbols = ''.join(r['symbol'] for r in regions)
    if not symbols or any(c not in 'JTUE' for c in symbols) or len(set(symbols)) != len(symbols):
        raise ValueError('invalid region symbols')
    area = b''.join(load(r, 32) for r in regions)
    offset = 0xe00+len(area)
    ip_size = max(0x1000, (offset+len(aip)+3)&~3)
    if ip_size > APP_LOAD-IP_LOAD or not aip or len(app) == 0 or APP_LOAD+len(app) > 0x060fc000:
        raise ValueError('IP/application/stack overlap or empty application')
    # Verify the complete independently authored entry, not an arbitrary blob.
    expected = bytes.fromhex('d10421188903e0011103e0021105d002402b0009')
    if len(aip)!=28 or aip[:20]!=expected or struct.unpack_from('>I',aip,24)[0]!=APP_LOAD:
        raise ValueError('unexpected AIP instructions or application branch target')
    trace=struct.unpack_from('>I',aip,20)[0]
    if trace and not (trace%4==0 and APP_LOAD<=trace-0x20000000 and
                      trace-0x20000000+40<=APP_LOAD+len(app)):
        raise ValueError('trace does not fit loaded application data')
    out = bytearray(SYSTEM_AREA)
    out[:16] = MAGIC
    def field(offset, size, text):
        raw = text.encode('ascii')
        if len(raw) > size or any(c < 32 or c > 126 for c in raw):
            raise ValueError('invalid/oversized metadata field')
        out[offset:offset+size] = raw.ljust(size, b' ')
    field(0x10,16,'LIBSATURN')
    field(0x20,10,product)
    if len(version) != 6 or version[0] != 'V' or version[2] != '.' or not (version[1]+version[3:]).isdigit():
        raise ValueError('version must be Vn.nnn')
    datetime.strptime(date, '%Y%m%d')
    field(0x2a,6,version); field(0x30,8,date); field(0x38,8,'CD-1/1')
    field(0x40,10,symbols); field(0x50,16,'J'); field(0x60,112,title)
    struct.pack_into('>8I',out,0xe0,ip_size,0,0x060ffffc,0,APP_LOAD,len(app),0,0)
    out[0x100:0xe00] = security
    out[0xe00:offset] = area
    out[offset:offset+len(aip)] = aip
    result = parse_ip(out)
    if result['issues']:
        raise ValueError(str(result['issues']))
    return bytes(out), dict(schema_version=1, profile='native', payloads=manifest,
                           aip_sha256=digest(aip), app_sha256=digest(app), ip=result)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--payloads', required=True, type=Path)
    p.add_argument('--aip', required=True, type=Path)
    p.add_argument('--app', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--title', default='LIBSATURN')
    p.add_argument('--profile', default='current', choices=['current'])
    args = p.parse_args()
    date = datetime.fromtimestamp(int(os.environ.get('SOURCE_DATE_EPOCH','1791936000')), timezone.utc).strftime('%Y%m%d')
    data, report = compose(args.payloads,args.aip.read_bytes(),args.app.read_bytes(),args.title,date=date)
    args.output.write_bytes(data)
    args.output.with_suffix('.provenance.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'[native] validated IP size={report["ip"]["ip_size"]} bytes; redistributable={report["payloads"]["redistributable"]}')


if __name__ == '__main__':
    main()
