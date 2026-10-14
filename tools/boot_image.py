"""Read-only Saturn IP/ISO inspection, following SEGA ST-040 section 4.

The system-area container is distinct from the declared IP load size.
Historical header violations are reported rather than silently repaired.
"""
from __future__ import annotations
import hashlib
import re
import struct
from datetime import datetime
from pathlib import Path

SECTOR = 2048
SYSTEM_AREA = 16 * SECTOR
IP_LOAD = 0x06002000
APP_LOAD = 0x06004000
MAGIC = b"SEGA SEGASATURN "


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_image(path: Path) -> bytes:
    """Read logical ISO or single MODE1/2352 BIN/CUE without modifying it."""
    if path.suffix.lower() == '.cue':
        text = path.read_text(encoding='utf-8-sig')
        files = re.findall(r'^\s*FILE\s+"([^"]+)"\s+BINARY\s*$', text, re.M | re.I)
        if len(files) != 1 or len(re.findall(r'^\s*TRACK\s', text, re.M | re.I)) != 1:
            raise ValueError('inspection supports one binary track only')
        if not re.search(r'TRACK\s+01\s+MODE1/2352', text, re.I):
            raise ValueError('expected MODE1/2352 CUE')
        if not re.search(r'INDEX\s+01\s+00:00:00', text, re.I):
            raise ValueError('nonzero track index unsupported')
        child = (path.parent / files[0]).resolve()
        if child.parent != path.parent.resolve():
            raise ValueError('CUE file must be adjacent to BIN')
        path = child
    data = path.read_bytes()
    if data[:12] == b'\x00' + b'\xff'*10 + b'\x00':
        if len(data) % 2352:
            raise ValueError('truncated raw sector')
        try:
            from iso_to_raw import validate_mode1_sector
        except ImportError:
            from tools.iso_to_raw import validate_mode1_sector
        return b''.join(validate_mode1_sector(i//2352, data[i:i+2352])
                        for i in range(0, len(data), 2352))
    if len(data) % SECTOR:
        raise ValueError('logical image is not sector aligned')
    return data


def parse_ip(data: bytes) -> dict:
    if len(data) < 256 or data[:16] != MAGIC:
        raise ValueError('missing/truncated Saturn System ID')
    words = dict(zip(('ip_size', 'reserved_e4', 'master_stack', 'slave_stack',
                      'first_read_address', 'first_read_size', 'reserved_f8', 'reserved_fc'),
                     struct.unpack_from('>8I', data, 0xe0)))
    issues = []
    if any(words[k] for k in ('reserved_e4', 'reserved_f8', 'reserved_fc')) or any(data[0xd0:0xe0]):
        issues.append('nonzero_reserved_field')
    size = words['ip_size']
    if size < 0x1000 or size > SYSTEM_AREA or size % 4:
        issues.append('invalid_ip_size')
    if len(data) < size:
        issues.append('truncated_declared_ip')
    for field, minimum in (('master_stack', 0x0600b000), ('slave_stack', 0x0600a200)):
        value = words[field]
        if value and (value % 4 or not minimum <= value <= 0x06100000):
            issues.append('invalid_' + field)
    address = words['first_read_address']
    if address and (address % 4 or not (0x00200000 <= address < 0x00300000 or
                                       IP_LOAD+size <= address < 0x06100000)):
        issues.append('invalid_first_read_address_or_ip_overlap')
    regions = data[0x40:0x4a].decode('ascii', errors='replace').rstrip(' ')
    if not regions or any(c not in 'JTUE' for c in regions) or len(set(regions)) != len(regions):
        issues.append('invalid_regions')
    text_fields={name:data[start:start+size].decode('ascii',errors='replace').rstrip(' ')
                 for name,start,size in (('manufacturer',0x10,16),('product',0x20,10),
                     ('version',0x2a,6),('date',0x30,8),('device',0x38,8),
                     ('peripherals',0x50,16),('title',0x60,112))}
    if any(any(ord(c)<32 or ord(c)>126 for c in value) for value in text_fields.values()):
        issues.append('invalid_header_ascii')
    if not re.fullmatch(r'V[0-9]\.[0-9]{3}',text_fields['version']):
        issues.append('invalid_version')
    try:
        if not re.fullmatch(r'[0-9]{8}',text_fields['date']):raise ValueError('date format')
        datetime.strptime(text_fields['date'],'%Y%m%d')
    except ValueError:
        issues.append('invalid_release_date')
    return dict(**words, regions=regions, aip_offset=0xe00+len(regions)*0x20,
                metadata=text_fields,
                issues=issues, system_area_sha256=digest(data[:SYSTEM_AREA]),
                security_sha256=digest(data[0x100:0xe00]))


def both(data: bytes, offset: int, width: int) -> int:
    little = int.from_bytes(data[offset:offset+width], 'little')
    big = int.from_bytes(data[offset+width:offset+2*width], 'big')
    if little != big:
        raise ValueError('ISO9660 endian copies disagree')
    return big


def root_records(iso: bytes) -> list[dict]:
    if len(iso) < 18*SECTOR:
        raise ValueError('truncated ISO volume descriptors')
    pvd = None
    terminated = False
    for lba in range(16, min(len(iso)//SECTOR, 256)):
        block = iso[lba*SECTOR:(lba+1)*SECTOR]
        if block[1:6] != b'CD001' or block[6] != 1:
            raise ValueError('invalid ISO9660 volume descriptor')
        if block[0] == 1:
            if pvd is not None:
                raise ValueError('duplicate primary descriptor')
            pvd = block
        if block[0] == 255:
            terminated = True
            break
    if pvd is None or not terminated:
        raise ValueError('missing primary descriptor or terminator')
    if both(pvd, 128, 2) != SECTOR:
        raise ValueError('unsupported logical block size')
    if both(pvd, 80, 4)*SECTOR != len(iso):
        raise ValueError('volume space size differs from image')
    root = pvd[156:190]
    if root[0] < 34 or not root[25] & 2:
        raise ValueError('invalid root directory')
    start, length = both(root, 2, 4)*SECTOR, both(root, 10, 4)
    if start < 18*SECTOR or start+length > len(iso):
        raise ValueError('root outside image')
    records, pos, entry_index = [], 0, 0
    while pos < length:
        count = iso[start+pos]
        if not count:
            pos = (pos//SECTOR+1)*SECTOR
            continue
        if count < 34 or pos+count > length or pos%SECTOR+count > SECTOR:
            raise ValueError('truncated/cross-sector directory record')
        row = iso[start+pos:start+pos+count]
        name_size = row[32]
        if not name_size or 33+name_size > count:
            raise ValueError('invalid directory identifier')
        name = row[33:33+name_size]
        extent, size = both(row, 2, 4), both(row, 10, 4)
        if extent*SECTOR+size > len(iso):
            raise ValueError('file extent outside image')
        if entry_index<2:
            if name!=bytes((entry_index,)) or not row[25]&2:
                raise ValueError('first two root records must be dot and dot-dot')
        elif name in (b'\0',b'\1'):
            raise ValueError('misplaced dot directory entry')
        else:
            records.append(dict(identifier=name.decode('ascii'), lba=extent,
                                size=size, flags=row[25], record_offset=start+pos))
        pos += count
        entry_index += 1
    return records


def inspect(iso: bytes, app: bytes | None = None, strict_ip: bool = False) -> dict:
    ip = parse_ip(iso[:SYSTEM_AREA])
    if strict_ip and ip['issues']:
        raise ValueError('IP: ' + ', '.join(ip['issues']))
    rows = root_records(iso)
    if not rows or rows[0]['identifier'] not in ('0.BIN;1', '0.BIN') or rows[0]['flags'] & (2 | 128):
        raise ValueError('first root record after dot entries must be contiguous 0.BIN')
    first = rows[0]
    start, size = first['lba']*SECTOR, first['size']
    if size == 0 or start < SYSTEM_AREA:
        raise ValueError('empty or overlapping first-read extent')
    payload = iso[start:start+size]
    address = ip['first_read_address']
    end = 0x00300000 if 0x00200000 <= address < 0x00300000 else 0x06100000
    if address and address+size > end:
        raise ValueError('first-read payload overflows Work RAM')
    if app is not None and payload != app:
        raise ValueError('0.BIN contents differ from built application')
    return dict(schema_version=1, ip=ip, root=rows,
                first_read_sha256=digest(payload), iso_sha256=digest(iso))
