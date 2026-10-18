#!/usr/bin/env python3
"""Attribute harness interpreter costs to a matching ELF. No retail data needed."""
import argparse,bisect,collections,csv,hashlib,json,struct,subprocess,tempfile
from pathlib import Path

def read_symbols(elf,nm):
    text=subprocess.check_output([str(nm),'-n','-S','-C','--defined-only',str(elf)],text=True)
    result=[]
    for line in text.splitlines():
        parts=line.split(None,3)
        if len(parts)==4 and parts[2].lower() in ('t','w'):
            result.append((int(parts[0],16),int(parts[1],16),parts[3]))
        elif len(parts)==3 and parts[1].lower() in ('t','w'):
            result.append((int(parts[0],16),0,parts[2]))
    return sorted(result)

def attribute(rows,symbols,totals):
    # Sized ELF functions take precedence over zero-sized assembler aliases.
    sized=[s for s in symbols if s[1]];starts=[s[0] for s in sized]
    all_starts=sorted(set(s[0] for s in symbols))
    zero={s[0]:s[2] for s in symbols if not s[1]}
    costs=collections.defaultdict(lambda:collections.Counter())
    for row in rows:
        pc=int(row['pc'])&0x1fffffff
        cpu=row['cpu'];n=int(row['instructions']);cycles=int(row['execution_cycles'])
        if cpu not in ('master','slave') or min(n,cycles)<0:raise ValueError('invalid CPU cost row')
        i=bisect.bisect_right(starts,pc)-1
        if i>=0 and pc<sum(sized[i][:2]):
            name=sized[i][2];boundary='elf_size'
        else:
            i=bisect.bisect_right(all_starts,pc)-1
            if i>=0 and i+1<len(all_starts) and all_starts[i] in zero:
                name=zero[all_starts[i]];boundary='inferred_next_text_symbol'
            else:name='unresolved';boundary='unresolved'
        key=(cpu,name,boundary);costs[key]['instructions']+=n;costs[key]['execution_cycles']+=cycles
    result={}
    for cpu in ('master','slave'):
        items=[dict(symbol=name,boundary=boundary,**cost) for (target,name,boundary),cost in costs.items() if target==cpu]
        instruction_sum=sum(x['instructions'] for x in items)
        cycle_sum=sum(x['execution_cycles'] for x in items)
        total=totals[cpu]
        if instruction_sum!=total['instructions'] or cycle_sum+total['interrupt_entry_cycles']!=total['execution_cycles']:
            raise ValueError(cpu+' histogram does not reconcile with profile totals')
        for item in items:
            item['percent_execution_cycles']=100*item['execution_cycles']/total['execution_cycles'] if total['execution_cycles'] else 0
        result[cpu]=dict(totals=total,functions=sorted(items,key=lambda x:-x['execution_cycles']))
    return result

def analyze(prefix,elf,nm):
    prefix=Path(prefix);elf=Path(elf)
    meta=json.loads(Path(str(prefix)+'.json').read_text())
    if meta.get('version')!=1 or meta.get('truncated',True):raise ValueError('unsupported or truncated CPU profile')
    with Path(str(prefix)+'.pc.csv').open() as f:
        cpus=attribute(list(csv.DictReader(f)),read_symbols(elf,nm),meta)
    return dict(version=1,elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
                scope='Exclusive interpreter-step cost by PC, not inclusive call time; inferred assembler boundaries require disassembly review.',
                profile=meta,cpus=cpus)

def disassemble_snapshots(prefix,objdump,limit=64):
    """Decode captured words, not a later overlay or an ELF guessed to match it."""
    prefix=Path(prefix);seen=set();count=0;omitted=0
    with Path(str(prefix)+'.snapshots.csv').open() as source, \
         Path(str(prefix)+'.disassembly.txt').open('w') as out, \
         tempfile.TemporaryDirectory() as scratch:
        out.write('Captured pre-instruction words; at most 64 distinct CPU/PC/code combinations.\n')
        binary=Path(scratch)/'instructions.bin'
        for row in csv.DictReader(source):
            pc=int(row['pc']);words=tuple(int(row['opcode'+str(i)]) for i in range(8))
            key=(row['cpu'],pc,words)
            if key in seen:continue
            if count>=limit:omitted+=1;continue
            seen.add(key);count+=1
            binary.write_bytes(struct.pack('>8H',*words))
            out.write(f"\n{row['cpu']} frame={row['frame']} pc={pc:#x} pr={int(row['pr']):#x}\n")
            out.write(subprocess.check_output([str(objdump),'-D','-b','binary','-m','sh2','-EB',
                '--adjust-vma='+hex(pc),str(binary)],text=True))
        out.write(f'\nDecoded {count} combinations; omitted observations after limit: {omitted}\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prefix',required=True,type=Path);p.add_argument('--elf',required=True,type=Path)
    p.add_argument('--nm',default='sh2eb-elf-nm');p.add_argument('--out',required=True,type=Path)
    p.add_argument('--objdump',help='also disassemble captured checkpoint instruction words')
    a=p.parse_args();a.out.write_text(json.dumps(analyze(a.prefix,a.elf,a.nm),indent=2)+'\n')
    if a.objdump:disassemble_snapshots(a.prefix,a.objdump)
