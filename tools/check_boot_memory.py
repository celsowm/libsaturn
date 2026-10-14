"""Verify a flat first-read file against an SH-2 ELF's initialized PT_LOAD bytes."""
import argparse
import json
import struct
from pathlib import Path


def validate(elf: bytes, app: bytes) -> dict:
    if len(elf)<52 or elf[:7]!=b'\x7fELF\x01\x02\x01':
        raise ValueError('expected ELF32 big-endian')
    header=struct.unpack_from('>HHIIIIIHHHHHH',elf,16)
    _,machine,_,entry,phoff,_,_,_,phsize,phcount,_,_,_=header
    if machine!=42 or entry!=0x06004000 or phsize!=32 or phoff+phcount*32>len(elf):
        raise ValueError('invalid SH-2 entry/program headers')
    segments=[]
    for i in range(phcount):
        kind,offset,address,physical,size,memory,flags,alignment=struct.unpack_from('>8I',elf,phoff+i*32)
        if kind!=1 or not size:continue
        if physical!=address or address<entry or address+memory>0x06100000 or size>memory or offset+size>len(elf):
            raise ValueError('initialized segment outside the first-read Work RAM contract')
        relative=address-entry
        if relative+size>len(app) or app[relative:relative+size]!=elf[offset:offset+size]:
            raise ValueError('first-read binary does not cover ELF initialized bytes')
        segments.append(dict(address=address,bytes=size,memory_bytes=memory,flags=flags))
    if not segments or min(s['address'] for s in segments)!=entry or \
            max(s['address']+s['bytes'] for s in segments)-entry!=len(app):
        raise ValueError('first-read length includes missing or extra load bytes')
    return dict(entry=entry,bytes=len(app),segments=segments)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',required=True,type=Path)
    parser.add_argument('--app',required=True,type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    report=validate(args.elf.read_bytes(),args.app.read_bytes())
    if args.out:args.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'[boot-memory] _start=0x{report["entry"]:08X}, {report["bytes"]} initialized bytes match ELF')


if __name__=='__main__':
    main()
