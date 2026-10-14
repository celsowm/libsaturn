"""Native composer fixtures contain no authentic security or BIOS bytes."""
import json
import struct
import tempfile
import unittest
from pathlib import Path
from tools.boot_image import digest, parse_ip
from tools.build_native_ip import compose
from tools.generate_optical_fixture import fixture
from tools.check_native_ip import validate
from tools.normalize_iso_dates import normalize
from tests.test_boot_image import fixture as iso_fixture
from tools.check_boot_memory import validate as validate_memory


class NativeIPTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        def item(name,data):
            (self.root/name).write_bytes(data)
            return dict(file=name,sha256=digest(data),origin='Synthetic host fixture',license='Test only')
        self.manifest=dict(schema_version=1,redistributable=False,
            security=item('security.bin',b'X'*0xd00),
            regions=[dict(symbol='U',**item('area.bin',b'A'*32))])
        self.path=self.root/'payloads.json'
        self.save()
        self.aip=bytes.fromhex('d10421188903e0011103e0021105d002402b0009')+struct.pack('>II',0,0x06004000)

    def save(self):
        self.path.write_text(json.dumps(self.manifest),encoding='utf-8')

    def test_spec_offsets_and_repeatability(self):
        data,report=compose(self.path,self.aip,b'APP!',date='20261014')
        self.assertEqual(compose(self.path,self.aip,b'APP!',date='20261014')[0],data)
        self.assertEqual(len(data),32768)
        self.assertEqual(data[0x10:0x20],b'LIBSATURN       ')
        self.assertEqual(data[0x20:0x2a],b'T-00000G  ')
        self.assertEqual(data[0x2a:0x30],b'V0.001')
        self.assertEqual(data[0x30:0x38],b'20261014')
        self.assertEqual(data[0xe20:0xe20+len(self.aip)],self.aip)
        self.assertEqual(parse_ip(data)['issues'],[])
        self.assertEqual(report['ip']['ip_size'],4096)
        self.assertFalse(report['payloads']['redistributable'])

    def test_independent_decoder(self):
        data,_=compose(self.path,self.aip,b'APP!')
        validate(data,b'APP!',self.path)
        broken=bytearray(data);broken[0xe20+4]^=1
        with self.assertRaisesRegex(ValueError,'flow|branch'):
            validate(broken,b'APP!',self.path)

    def test_normalize_dates_preserves_file_data_and_is_idempotent(self):
        original=bytes(iso_fixture())
        normalized=normalize(original,1791936000)
        self.assertEqual(normalize(normalized,1791936000),normalized)
        self.assertEqual(normalized[:32768],original[:32768])
        self.assertEqual(normalized[21*2048:],original[21*2048:])
        self.assertEqual(normalized[16*2048+813:16*2048+817],b'2026')

    def test_rock_ridge_dates_are_normalized(self):
        iso=iso_fixture()
        pos=20*2048+68
        count=iso[pos]
        tf=b'TF'+bytes((26,1,14))+bytes(range(21))
        iso[pos]=count+len(tf)
        iso[pos+count:pos+count+len(tf)]=tf
        result=normalize(iso,1791936000)
        stamps=result[pos+count+5:pos+count+26]
        self.assertEqual(stamps[:7],stamps[7:14])
        self.assertEqual(stamps[:7],stamps[14:])
        self.assertEqual(normalize(result,1791936000),result)

    def test_reject_hash_missing_policy_and_jump_mutation(self):
        (self.root/'area.bin').write_bytes(b'B'*32)
        with self.assertRaisesRegex(ValueError,'hash'):
            compose(self.path,self.aip,b'APP!')
        (self.root/'area.bin').write_bytes(b'A'*32)
        broken=bytearray(self.aip);broken[-1]^=1
        with self.assertRaisesRegex(ValueError,'AIP'):
            compose(self.path,broken,b'APP!')
        del self.manifest['security']['license'];self.save()
        with self.assertRaisesRegex(ValueError,'license'):
            compose(self.path,self.aip,b'APP!')

    def test_trace_must_be_inside_initialized_payload(self):
        traced=self.aip[:20]+struct.pack('>II',0x26004000,0x06004000)
        with self.assertRaisesRegex(ValueError,'trace'):
            compose(self.path,traced,b'APP!')
        compose(self.path,traced,bytes(40))

    def test_oversize_and_invalid_metadata(self):
        for app in (b'',bytes(0xfc001)):
            with self.assertRaisesRegex(ValueError,'overlap|empty'):
                compose(self.path,self.aip,app)
        with self.assertRaises(ValueError):
            compose(self.path,self.aip,b'APP!',title='X'*113)
        with self.assertRaises(ValueError):
            compose(self.path,self.aip,b'APP!',date='20260230')

    def test_sector_fixture_is_exactly_32_sectors(self):
        self.assertEqual(len(fixture()),32*2048)
        self.assertEqual(fixture()[:16],b'LIBSATURN ODE V1')

    def test_elf_initialized_bytes_exclude_bss(self):
        elf=bytearray(136)
        elf[:7]=b'\x7fELF\x01\x02\x01'
        struct.pack_into('>HHIIIIIHHHHHH',elf,16,2,42,1,0x06004000,52,0,0,52,32,2,40,0,0)
        struct.pack_into('>8I',elf,52,1,128,0x06004000,0x06004000,4,4,5,4)
        struct.pack_into('>8I',elf,84,1,132,0x06004008,0x06004008,4,64,6,4)
        elf[128:136]=b'CODEDATA'
        app=b'CODE'+bytes(4)+b'DATA'
        self.assertEqual(validate_memory(elf,app)['bytes'],12)
        for broken in (app[:-1],app+bytes(60),b'FAIL'+app[4:]):
            with self.assertRaises(ValueError):validate_memory(elf,broken)
        struct.pack_into('>I',elf,24,0x06004004)
        with self.assertRaisesRegex(ValueError,'entry'):validate_memory(elf,app)


if __name__=='__main__':
    unittest.main()
