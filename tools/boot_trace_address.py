"""Resolve the opt-in application trace symbol from an ELF using target nm."""
import argparse
import re
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--nm',required=True)
    p.add_argument('--elf',required=True)
    args=p.parse_args()
    text=subprocess.check_output([args.nm,args.elf],text=True)
    match=re.search(r'^([0-9a-fA-F]+)\s+[DdVv]\s+_sat_boot_trace_record$',text,re.M)
    address=int(match[1],16) if match else 0
    if address and not 0x06004000<=address<0x060fc000:
        raise ValueError('trace address outside loaded application')
    print(hex(address|0x20000000 if address else 0))


if __name__=='__main__':
    main()
