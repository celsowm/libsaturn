import copy,struct,sys,unittest
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from cube_pipeline import HEADER,FIELDS,ROWS,decode,symbols,working_buffers,RAM_ACTORS,RAM_FACES,RAM_ORDER
from analyze_cpu_profile import attribute
from compare_cube_pipeline import compare_case


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

    def test_symbols_include_unsized_assembly_slave_entry(self):
        with patch('cube_pipeline.subprocess.check_output',return_value=
                   '060040b0 T _saturn_slave_entry\n06020000 0000000c b _g_anim\n'):
            result=symbols(Path('example.elf'),'nm')
        self.assertEqual(result['saturn_slave_entry'],(0x060040b0,0))
        self.assertEqual(result['g_anim'],(0x06020000,12))

    def test_independent_placement_groups_certify_actual_elf_addresses(self):
        for mask in (0,RAM_ACTORS,RAM_FACES,RAM_ORDER,RAM_ACTORS|RAM_FACES|RAM_ORDER):
            syms={name:(0x06010000 if mask & bit else 0x00210000,4096)
                  for name,bit in [('actors',RAM_ACTORS),('faces',RAM_FACES),
                      ('worker_faces',RAM_FACES),('keys',RAM_ORDER),('order',RAM_ORDER),
                      ('worker_keys',RAM_ORDER)]}
            with self.subTest(mask=mask):
                result=working_buffers(syms,mask)
                self.assertEqual(len(result),6)
                broken=dict(syms);broken['actors']=(0x00210000 if mask & RAM_ACTORS else 0x06010000,4096)
                with self.assertRaises(ValueError):working_buffers(broken,mask)
        for syms,mask in [({},0),({'actors':(0x00210000,0)},0),
                ({'actors':(0x002ff000,4097)},0),({'actors':(0x00210000,1)},8)]:
            with self.assertRaises(ValueError):working_buffers(syms,mask)

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

    def comparison_fixture(self):
        result=decode(self.fixture())
        result.update(provenance=dict(bios='bios',probe='probe'),working_ram='low',
                      working_buffers=dict(faces=dict(size=10400)))
        return result

    def test_comparison_separates_memory_cost_and_cadence(self):
        before=self.comparison_fixture();after=copy.deepcopy(before)
        after['stage_us']['work']['median']=800
        after['working_buffers']['faces']['size']=8800
        result=compare_case(before,after)
        self.assertEqual(result['costs']['work']['reduction_percent'],20)
        self.assertEqual(result['before_working_bytes']-result['after_working_bytes'],1600)
        self.assertEqual(result['before_submit_s'],result['after_submit_s'])

    def test_calibration_normalization_requires_explicit_choice(self):
        before=self.comparison_fixture();after=copy.deepcopy(before)
        after['header']['frequency']+=60
        with self.assertRaises(ValueError):compare_case(before,after)
        result=compare_case(before,after,normalize_calibration=True)
        self.assertNotEqual(result['before_frt_hz'],result['after_frt_hz'])

    def test_comparison_rejects_incompatible_controls(self):
        before=self.comparison_fixture()
        for section,key,value in [('header','divisions',2),('header','hud',0),
                ('header','errors',1),('header','frequency',0),('provenance','probe','other'),
                ('commands','maximum',51)]:
            after=copy.deepcopy(before);after[section][key]=value
            with self.assertRaises(ValueError):compare_case(before,after)
        after=copy.deepcopy(before);after['measured_fields']=599
        with self.assertRaises(ValueError):compare_case(before,after)

    def test_placement_control_requires_explicit_choice_and_actual_matching_buffers(self):
        before=self.comparison_fixture();after=copy.deepcopy(before)
        before['working_buffers']['faces']['address']=0x00200000
        after['working_ram']='high';after['working_buffers']['faces']['address']=0x06010000
        with self.assertRaisesRegex(ValueError,'working_ram'):compare_case(before,after)
        result=compare_case(before,after,allow_working_ram_change=True)
        self.assertEqual(result['before_working_ram'],'low')
        self.assertEqual(result['after_working_ram'],'high')
        for key,value in (('address',0x00200000),('size',8800)):
            changed=copy.deepcopy(after);changed['working_buffers']['faces'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):
                compare_case(before,changed,allow_working_ram_change=True)
        after['commands']['maximum']+=1
        with self.assertRaises(ValueError):compare_case(before,after,allow_working_ram_change=True)

    def test_mixed_placement_checks_each_buffer_and_rejects_implicit_changes(self):
        before=self.comparison_fixture()
        before['working_buffers']={'faces':dict(size=10400,address=0x00200000,ram='low'),
                                   'actors':dict(size=4096,address=0x00210000,ram='low')}
        after=copy.deepcopy(before)
        after['working_ram']='mixed'
        after['working_buffers']['actors'].update(address=0x06010000,ram='high')
        with self.assertRaises(ValueError):compare_case(before,after)
        compare_case(before,after,allow_working_ram_change=True)
        malformed=copy.deepcopy(after);malformed['working_buffers']['actors']['ram']='low'
        with self.assertRaises(ValueError):compare_case(before,malformed,allow_working_ram_change=True)
        other=copy.deepcopy(after)
        other['working_buffers']['actors'].update(address=0x00210000,ram='low')
        other['working_buffers']['faces'].update(address=0x06010000,ram='high')
        with self.assertRaises(ValueError):compare_case(after,other)
        compare_case(after,other,allow_working_ram_change=True)


if __name__=='__main__':unittest.main()
