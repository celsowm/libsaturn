"""Read guest diagnostic records; distinguish BIOS boot from direct injection."""
import argparse
import json
import re
import struct
from pathlib import Path

RESULT_FIELDS=('magic version frames profile build_id selected current completed result passes failures '
               'lba requested transferred crc elapsed_ms timeouts hirq cr1 cr2 cr3 cr4 wait_mask '
               'response_valid sustained_bytes sustained_ms cancelled').split()
TRACE_FIELDS='magic version bytes stage completed reached result entry_sr entry_vbr entry_sp'.split()


def symbol(mapping: str, name: str) -> int:
    match=re.search(r'^\s*0x([\da-fA-F]+)\s+_'+re.escape(name)+r'\s*$',mapping,re.M)
    if not match:raise ValueError('missing map symbol '+name)
    return int(match[1],16)


def decode(ram: bytes, mapping: str, name: str, fields: list[str]) -> dict:
    offset=symbol(mapping,name)-0x06000000
    if offset<0 or offset+4*len(fields)>len(ram):raise ValueError('record outside RAM dump')
    return dict(zip(fields,struct.unpack_from('>'+str(len(fields))+'I',ram,offset)))


def analyze(prefix: Path, require_tests: bool=False) -> dict:
    probe=json.loads(prefix.with_suffix('.json').read_text(encoding='utf-8'))
    mapping=prefix.with_suffix('.map').read_text(encoding='utf-8')
    ram=prefix.with_suffix('.ram').read_bytes()
    results=decode(ram,mapping,'g_optical_results',RESULT_FIELDS)
    trace=decode(ram,mapping,'sat_boot_trace_record',TRACE_FIELDS)
    if results['magic']!=0x4f455431 or results['version']!=1 or trace['magic']!=0x4c534254:
        return dict(status='UNKNOWN',reason='diagnostic record not initialized',boot=probe['boot'])
    if results['frames']<30 or not trace['reached']&(1<<9):
        return dict(status='UNKNOWN',reason='first rendered frame not confirmed',results=results,trace=trace,boot=probe['boot'])
    results['fad']=results['lba']+150 if results['requested'] else None
    if probe['boot']['injected']:
        mode='injection'
    else:
        mode='bios'
        if probe['boot'].get('mode')!='bios':raise ValueError('ambiguous boot mode')
        if not probe['boot'].get('entry_observed'):
            return dict(status='UNKNOWN',reason='BIOS instruction entry not observed',results=results,trace=trace,boot=probe['boot'])
    if require_tests:
        if not results['profile'] or results['passes']<5 or results['failures'] or results['result']:
            raise ValueError('full CD/ISO/read acceptance failed: '+str(results))
        if results['transferred']!=65536 or not results['response_valid']:
            raise ValueError('missing completed 32-sector read')
    elif results['passes'] or results['failures'] or results['transferred']:
        raise ValueError('idle run unexpectedly executed a diagnostic')
    return dict(status='PASS',mode=mode,cd_tests=require_tests,results=results,trace=trace,
                note='Emulator evidence; physical Satiator/SAROO acceptance pending')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    parser.add_argument('--require-cd-tests',action='store_true')
    args=parser.parse_args()
    report=analyze(args.prefix,args.require_cd_tests)
    args.prefix.with_suffix('.analysis.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    if report['status']!='PASS':raise SystemExit(2)


if __name__=='__main__':
    main()
