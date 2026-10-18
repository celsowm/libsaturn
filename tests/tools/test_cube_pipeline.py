import struct,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from cube_pipeline import HEADER,FIELDS,ROWS,decode
from analyze_cpu_profile import attribute


class CubeTelemetryTests(unittest.TestCase):
    def fixture(self,count=900):
        raw=bytearray(4*(len(HEADER)+ROWS*len(FIELDS)))
        header=dict.fromkeys(HEADER,0)
        header.update(magic=0x43334231,version=1,stage=4,divisions=1,hud=1,
                      frequency=200000,count=count,cursor=count%ROWS,serial=count,
                      vertices_per_actor=8,faces_per_actor=6)
        struct.pack_into('>'+str(len(HEADER))+'I',raw,0,*(header[k] for k in HEADER))
        for i in range(max(0,count-ROWS),count):
            sample=dict.fromkeys(FIELDS,0)
            sample.update(serial=i+1,field_begin=i+10,field_end=i+10,work=200,
                          geometry=100,flush=50,commands=50,faces=6)
            struct.pack_into('>'+str(len(FIELDS))+'I',raw,
                            4*len(HEADER)+(i%ROWS)*4*len(FIELDS),*(sample[k] for k in FIELDS))
        return raw

    def test_rates_and_frt_units(self):
        report=decode(self.fixture())
        self.assertEqual(report['measured_fields'],789)
        self.assertEqual(report['submissions_per_nominal_second'],60)
        self.assertEqual(report['stage_us']['geometry']['median'],500)

    def test_ring_skips_the_active_overwrite_slot(self):
        count=ROWS+33;raw=self.fixture(count)
        struct.pack_into('>I',raw,4*len(HEADER)+(count%ROWS)*4*len(FIELDS),0xA5A5A5A5)
        report=decode(raw,0);self.assertEqual(report['samples'],ROWS-1)

    def test_guest_failures_and_partial_metadata_fail_closed(self):
        for key,value in [('errors',1),('timeouts',1),('cursor',3),('vertices_per_actor',24)]:
            raw=self.fixture();struct.pack_into('>I',raw,4*HEADER.index(key),value)
            with self.assertRaises(ValueError):decode(raw)
        with self.assertRaises(ValueError):decode(self.fixture()[:-1])

    def test_timeout_and_over_budget_commands_reject(self):
        for key,value in [('commands',2048),('draw_timeouts',1),('geometry',201)]:
            raw=self.fixture();offset=4*len(HEADER)+899*4*len(FIELDS)+4*FIELDS.index(key)
            struct.pack_into('>I',raw,offset,value)
            with self.assertRaises(ValueError):decode(raw)

    def test_profile_costs_reconcile_and_resolve_assembly_aliases(self):
        rows=[dict(cpu='master',pc=0x26001004,instructions=2,execution_cycles=4),
              dict(cpu='master',pc=0x06001010,instructions=1,execution_cycles=3)]
        totals=dict(master=dict(instructions=3,execution_cycles=9,interrupt_entry_cycles=2),
                    slave=dict(instructions=0,execution_cycles=0,interrupt_entry_cycles=0))
        symbols=[(0x06001000,16,'function'),(0x06001010,0,'asm'),(0x06001020,16,'next')]
        result=attribute(rows,symbols,totals)
        self.assertEqual(result['master']['functions'][0]['symbol'],'function')
        self.assertEqual(result['master']['functions'][1]['boundary'],'inferred_next_text_symbol')
        totals['master']['execution_cycles']+=1
        with self.assertRaises(ValueError):attribute(rows,symbols,totals)


if __name__=='__main__':unittest.main()
