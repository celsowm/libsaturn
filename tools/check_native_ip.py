"""Independently decode the native AIP and verify its composed input ranges."""
import argparse
import json
import struct
from pathlib import Path
try:
    from boot_image import APP_LOAD, IP_LOAD, SYSTEM_AREA, digest, parse_ip
except ImportError:
    from tools.boot_image import APP_LOAD, IP_LOAD, SYSTEM_AREA, digest, parse_ip


def validate(data: bytes, app: bytes, manifest_path: Path) -> dict:
    info=parse_ip(data)
    if len(data)!=SYSTEM_AREA or info['issues']:
        raise ValueError('native System ID/container violation')
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    regions=''.join(r['symbol'] for r in manifest['regions'])
    if regions!=info['regions']:raise ValueError('region order differs from manifest')
    for start,end,item in [(0x100,0xe00,manifest['security'])]+[
            (0xe00+i*32,0xe20+i*32,r) for i,r in enumerate(manifest['regions'])]:
        source=(manifest_path.parent/item['file']).read_bytes()
        if data[start:end]!=source or digest(source)!=item['sha256']:
            raise ValueError('payload differs from declared input')
    offset=info['aip_offset']
    # Decode the PC-relative literal loads and the non-delayed conditional
    # branch, then verify both writes stay inside the application's trace.
    code=data[offset:offset+28]
    words=struct.unpack('>10H',code[:20])
    if words[0]>>8!=0xd1 or words[7]>>8!=0xd0 or words[2]>>8!=0x89:
        raise ValueError('invalid native AIP literal loads/branch')
    trace_literal=((offset+4)&~3)+(words[0]&255)*4
    app_literal=((offset+14+4)&~3)+(words[7]&255)*4
    branch_target=offset+4+4+(words[2]&255)*2
    if (trace_literal,app_literal,branch_target)!=(offset+20,offset+24,offset+14):
        raise ValueError('invalid native AIP control flow')
    if tuple(words[i] for i in (1,3,4,5,6,8,9))!=(0x2118,0xe001,0x1103,0xe002,0x1105,0x402b,0x0009):
        raise ValueError('unexpected native AIP register/memory operations')
    trace,target=struct.unpack_from('>II',code,20)
    if target!=APP_LOAD or info['first_read_address']!=target or info['first_read_size']!=len(app):
        raise ValueError('native application handoff mismatch')
    if info['ip_size']<offset+28 or IP_LOAD+info['ip_size']>target:
        raise ValueError('native AIP extent/overlap')
    if trace and (trace%4 or not target<=trace-0x20000000<=target+len(app)-40):
        raise ValueError('native trace outside application')
    return info


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ip',type=Path,required=True)
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--payloads',type=Path,required=True)
    args=parser.parse_args()
    validate(args.ip.read_bytes(),args.app.read_bytes(),args.payloads)
    print('[check] native AIP instructions, System ID, payloads and handoff OK')


if __name__=='__main__':
    main()
