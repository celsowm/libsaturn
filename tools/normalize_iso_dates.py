"""Normalize ISO9660 metadata dates; application/IP/file bytes stay unchanged.

Normalize directory and Rock Ridge TF dates, including SUSP continuations.
This runs before raw-sector EDC/ECC generation, for either build path.
"""
import argparse
from datetime import datetime, timezone
from pathlib import Path
try:
    from boot_image import SECTOR, both, root_records
except ImportError:
    from tools.boot_image import SECTOR, both, root_records


def normalize(data: bytes, epoch: int) -> bytes:
    root_records(data)  # Validate descriptor and directory bounds first.
    out=bytearray(data)
    date=datetime.fromtimestamp(epoch,timezone.utc)
    short=bytes((date.year-1900,date.month,date.day,date.hour,date.minute,date.second,0))
    long=date.strftime('%Y%m%d%H%M%S').encode('ascii')+b'00\0'
    visited=set()
    seen_susp=set()
    def susp(start,size):
        if (start,size) in seen_susp:return
        seen_susp.add((start,size))
        if start<0 or start+size>len(out):raise ValueError('SUSP continuation outside image')
        pos=start
        while pos+4<=start+size:
            count=out[pos+2]
            if not count:break
            if count<4 or pos+count>start+size:raise ValueError('invalid SUSP record')
            kind=out[pos:pos+2]
            if kind==b'TF':
                flags=out[pos+4]
                stamp=pos+5
                width=17 if flags&128 else 7
                for bit in range(7):
                    if flags&(1<<bit):
                        if stamp+width>pos+count:raise ValueError('truncated Rock Ridge timestamp')
                        out[stamp:stamp+width]=long if width==17 else short
                        stamp+=width
            elif kind==b'CE':
                if count!=28:raise ValueError('invalid SUSP continuation')
                susp(both(out,pos+4,4)*SECTOR+both(out,pos+12,4),both(out,pos+20,4))
            pos+=count
    def directory(lba,size):
        if (lba,size) in visited:return
        visited.add((lba,size))
        start=lba*SECTOR
        if start+size>len(out):raise ValueError('directory outside image')
        pos=0
        while pos<size:
            count=out[start+pos]
            if not count:pos=(pos//SECTOR+1)*SECTOR;continue
            if count<34 or pos+count>size or pos%SECTOR+count>SECTOR:
                raise ValueError('invalid directory while normalizing dates')
            offset=start+pos
            out[offset+18:offset+25]=short
            names=out[offset+32]
            system_use=33+names+(1 if names%2==0 else 0)
            if system_use>count:raise ValueError('invalid directory name length')
            susp(offset+system_use,count-system_use)
            if out[offset+25]&2 and out[offset+33:offset+34] not in (b'\0',b'\1'):
                directory(both(out,offset+2,4),both(out,offset+10,4))
            pos+=count
    for lba in range(16,256):
        offset=lba*SECTOR
        kind=out[offset]
        if kind==255:break
        if kind in (1,2):
            for slot in (813,830,864):out[offset+slot:offset+slot+17]=long
            out[offset+847:offset+864]=b'0'*16+b'\0' # No volume expiration.
            out[offset+174:offset+181]=short
            directory(both(out,offset+158,4),both(out,offset+166,4))
    return bytes(out)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso',type=Path,required=True)
    p.add_argument('--epoch',type=int,required=True)
    args=p.parse_args()
    args.iso.write_bytes(normalize(args.iso.read_bytes(),args.epoch))


if __name__=='__main__':
    main()
