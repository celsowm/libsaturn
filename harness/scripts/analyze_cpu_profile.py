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

def symbol_resolver(symbols):
    """Resolve code aliases without treating zero-size labels as sized functions."""
    sized=[s for s in symbols if s[1]];starts=[s[0] for s in sized]
    all_starts=sorted(set(s[0] for s in symbols))
    zero={s[0]:s[2] for s in symbols if not s[1]}
    def resolve(address):
        pc=address&0x1fffffff
        i=bisect.bisect_right(starts,pc)-1
        if i>=0 and pc<sum(sized[i][:2]):return sized[i][2],'elf_size'
        i=bisect.bisect_right(all_starts,pc)-1
        if i>=0 and i+1<len(all_starts) and all_starts[i] in zero:
            return zero[all_starts[i]],'inferred_next_text_symbol'
        return 'unresolved','unresolved'
    return resolve


def attribute(rows,symbols,totals):
    resolve=symbol_resolver(symbols)
    costs=collections.defaultdict(lambda:collections.Counter())
    for row in rows:
        pc=int(row['pc'])&0x1fffffff
        cpu=row['cpu'];n=int(row['instructions']);cycles=int(row['execution_cycles'])
        if cpu not in ('master','slave') or min(n,cycles)<0:raise ValueError('invalid CPU cost row')
        name,boundary=resolve(pc)
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

def call_edges(rows,symbols):
    """Count observed call edges; incomplete capture boundaries need no fake stack."""
    resolve=symbol_resolver(symbols);counts=collections.Counter()
    for row in rows:
        if row['event']!='call':continue
        cpu=row['cpu']
        if cpu not in ('master','slave'):raise ValueError('invalid call CPU')
        pc=int(row['pc'])&0x1fffffff;target=int(row['target'])&0x1fffffff
        caller,caller_boundary=resolve(pc);callee,callee_boundary=resolve(target)
        counts[(cpu,caller,callee,caller_boundary,callee_boundary)]+=1
    return [dict(cpu=cpu,caller=caller,callee=callee,calls=count,
                 caller_boundary=cb,callee_boundary=tb)
            for (cpu,caller,callee,cb,tb),count in counts.most_common()]


def memory_region(address):
    address&=0x1fffffff
    if 0x00200000<=address<0x00300000:return 'work_ram_low'
    if 0x06000000<=address<0x06100000:return 'work_ram_high'
    return 'other'


def memory_snapshots(rows,symbols):
    """SH-2 ABI arguments at entry, never guessed from a PC histogram.

    r4/r5/r6 carry memcpy's destination/source/length. PR is call PC+4 for
    SH-2 BSR/BSRF/JSR; the delay-slot instruction has already executed.
    Only exact memcpy entry checkpoints are accepted; interior PCs are ignored.
    """
    entries={address&0x1fffffff for address,size,name in symbols if name=='memcpy'}
    resolve=symbol_resolver(symbols);groups={}
    for row in rows:
        if (int(row['pc'])&0x1fffffff) not in entries:continue
        cpu=row['cpu']
        if cpu not in ('master','slave'):raise ValueError('invalid snapshot CPU')
        destination,source,length=(int(row['r'+str(i)]) for i in (4,5,6))
        if any(not 0<=value<=0xffffffff for value in (destination,source,length)):
            raise ValueError('invalid SH-2 copy argument')
        caller,boundary=resolve((int(row['pr'])-4)&0xffffffff)
        key=(cpu,caller,boundary,memory_region(source),memory_region(destination))
        if key not in groups:groups[key]=dict(calls=0,bytes=0,lengths=collections.Counter(),
            same_word_alignment_calls=0,same_halfword_alignment_calls=0,aligned_word_calls=0)
        item=groups[key];item['calls']+=1;item['bytes']+=length
        item['lengths'][length]+=1
        item['same_word_alignment_calls']+=((destination^source)&3)==0
        item['same_halfword_alignment_calls']+=((destination^source)&1)==0
        item['aligned_word_calls']+=((destination|source)&3)==0
    result=[]
    for (cpu,caller,boundary,source,destination),item in groups.items():
        item['lengths']=[dict(bytes=n,calls=count) for n,count in sorted(item['lengths'].items())]
        result.append(dict(cpu=cpu,caller=caller,caller_boundary=boundary,
                           source_region=source,destination_region=destination,**item))
    return sorted(result,key=lambda x:-x['bytes'])


def analyze(prefix,elf,nm):
    prefix=Path(prefix);elf=Path(elf)
    meta=json.loads(Path(str(prefix)+'.json').read_text())
    if meta.get('version')!=1 or meta.get('truncated',True):raise ValueError('unsupported or truncated CPU profile')
    symbols=read_symbols(elf,nm)
    with Path(str(prefix)+'.pc.csv').open() as f:
        cpus=attribute(list(csv.DictReader(f)),symbols,meta)
    with Path(str(prefix)+'.events.csv').open() as f:
        edges=call_edges(csv.DictReader(f),symbols)
    with Path(str(prefix)+'.snapshots.csv').open() as f:
        copies=memory_snapshots(csv.DictReader(f),symbols)
    return dict(version=1,elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
                scope='Exclusive interpreter-step cost by PC, not inclusive call time; inferred assembler boundaries require disassembly review.',
                profile=meta,cpus=cpus,call_edges=edges,memory_copies=copies,
                memory_copy_scope='Observed memcpy entry snapshots only; bytes are requested lengths, not bus traffic. '
                    'Empty snapshots do not establish absence of copies. No inclusive timing inferred.')

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
