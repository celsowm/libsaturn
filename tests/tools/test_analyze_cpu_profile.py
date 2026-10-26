#!/usr/bin/env python3
"""Check call attribution and SH-2 memcpy entry argument decoding."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from analyze_cpu_profile import symbol_resolver,call_edges,memory_snapshots

SYMBOLS=[(0x06001000,16,'caller_a'),(0x06001010,16,'caller_b'),
         (0x06002000,32,'memcpy'),(0x06003000,0,'assembler'),
         (0x06003010,4,'next')]

def snapshot(**changes):
    row=dict(cpu='master',pc=0x06002000,pr=0x06001010,
             r4=0x26010000,r5=0x20200100,r6=36)
    row.update(changes)
    return {k:str(v) for k,v in row.items()}

class AttributionTests(unittest.TestCase):
    def test_sized_aliases_and_inferred_boundaries(self):
        resolve=symbol_resolver(SYMBOLS)
        self.assertEqual(resolve(0x2600100f),('caller_a','elf_size'))
        self.assertEqual(resolve(0x06001010),('caller_b','elf_size'))
        self.assertEqual(resolve(0x06003008),('assembler','inferred_next_text_symbol'))
        self.assertEqual(resolve(0x06004000),('unresolved','unresolved'))

    def test_edges_ignore_returns_and_separate_cpus(self):
        call=dict(cpu='master',event='call',pc=str(0x2600100c),target=str(0x26002000))
        rows=[call,call,dict(call,cpu='slave'),dict(call,event='return')]
        edges=call_edges(rows,SYMBOLS)
        self.assertEqual(edges[0]['caller'],'caller_a')
        self.assertEqual(edges[0]['callee'],'memcpy')
        self.assertEqual([r['calls'] for r in edges],[2,1])

    def test_pr_points_four_bytes_after_call(self):
        result=memory_snapshots([snapshot()],SYMBOLS)[0]
        self.assertEqual(result['caller'],'caller_a')
        self.assertEqual(result['bytes'],36)
        self.assertEqual(result['source_region'],'work_ram_low')
        self.assertEqual(result['destination_region'],'work_ram_high')
        self.assertEqual(result['aligned_word_calls'],1)

    def test_lengths_alignment_zero_and_unsigned_extreme(self):
        rows=[snapshot(),snapshot(r4=0x06010001,r5=0x00200101,r6=0),
              snapshot(r4=0x06010001,r5=0x00200102,r6=0xffffffff)]
        result=memory_snapshots(rows,SYMBOLS)[0]
        self.assertEqual(result['calls'],3)
        self.assertEqual(result['bytes'],36+0xffffffff)
        self.assertEqual(result['same_word_alignment_calls'],2)
        self.assertEqual(result['same_halfword_alignment_calls'],2)
        self.assertEqual(result['aligned_word_calls'],1)
        self.assertEqual(result['lengths'],[dict(bytes=0,calls=1),dict(bytes=36,calls=1),
                                           dict(bytes=0xffffffff,calls=1)])

    def test_only_exact_entry_snapshots_count(self):
        self.assertEqual(memory_snapshots([snapshot(pc=0x06002002)],SYMBOLS),[])
        self.assertEqual(memory_snapshots([],SYMBOLS),[])

    def test_separate_caller_cpu_and_regions(self):
        rows=[snapshot(),snapshot(cpu='slave'),snapshot(pr=0x06001018),
              snapshot(r5=0x05a00000,r6=4)]
        result=memory_snapshots(rows,SYMBOLS)
        self.assertEqual(len(result),4)
        self.assertEqual(sum(r['bytes'] for r in result),112)
        self.assertIn('other',[r['source_region'] for r in result])
        self.assertIn('caller_b',[r['caller'] for r in result])

    def test_invalid_cpu_and_arguments_rejected(self):
        for row in [snapshot(cpu='other'),snapshot(r6=-1),snapshot(r4=0x100000000)]:
            with self.assertRaises(ValueError):memory_snapshots([row],SYMBOLS)
        with self.assertRaises(ValueError):
            call_edges([dict(cpu='other',event='call',pc='1',target='2')],SYMBOLS)

if __name__=='__main__':unittest.main()
