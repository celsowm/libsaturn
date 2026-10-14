"""Independent malformed-image and ST-040 layout fixtures."""
import struct
import unittest
from tools.boot_image import SECTOR, inspect, parse_ip
from tools.build_boot_header_variants import variants


def both32(buf, offset, value):
    struct.pack_into('<I', buf, offset, value)
    struct.pack_into('>I', buf, offset+4, value)


def record(name, lba, size, flags=0):
    result = bytearray(33+len(name)+(1 if len(name)%2 == 0 else 0))
    result[0] = len(result)
    both32(result, 2, lba)
    both32(result, 10, size)
    result[25], result[32] = flags, len(name)
    result[33:33+len(name)] = name
    return result


def fixture():
    iso = bytearray(23*SECTOR)
    iso[:16] = b'SEGA SEGASATURN '
    iso[0x10:0xd0] = b' '*0xc0
    iso[0x2a:0x30] = b'V0.001'
    iso[0x30:0x38] = b'20261014'
    iso[0x40:0x4a] = b'JTUE      '
    struct.pack_into('>I', iso, 0xe0, 0x1000)
    struct.pack_into('>I', iso, 0xe8, 0x060ffffc)
    struct.pack_into('>II', iso, 0xf0, 0x06004000, 4)
    pvd = bytearray(SECTOR)
    pvd[:7] = b'\1CD001\1'
    both32(pvd, 80, 23)
    pvd[128:132] = b'\0\x08\x08\0'
    pvd[156:190] = record(b'\0', 20, SECTOR, 2)
    iso[16*SECTOR:17*SECTOR] = pvd
    iso[17*SECTOR:17*SECTOR+7] = b'\xffCD001\1'
    root = record(b'\0', 20, SECTOR, 2)+record(b'\1', 20, SECTOR, 2)+record(b'0.BIN;1', 21, 4)
    iso[20*SECTOR:20*SECTOR+len(root)] = root
    iso[21*SECTOR:21*SECTOR+4] = b'APP!'
    return iso


class BootImageTests(unittest.TestCase):
    def test_valid_actual_payload_and_native_header(self):
        result = inspect(fixture(), b'APP!', True)
        self.assertEqual(result['root'][0]['lba'], 21)
        self.assertEqual(result['ip']['aip_offset'], 0xe80)

    def test_wrong_content(self):
        with self.assertRaisesRegex(ValueError, 'contents'):
            inspect(fixture(), b'BAD!')

    def test_wrong_first_file(self):
        iso = fixture()
        off = 20*SECTOR+68+33
        iso[off] = ord('1')
        with self.assertRaisesRegex(ValueError, 'first root'):
            inspect(iso)

    def test_file_id_requires_both_dot_entries(self):
        iso=fixture()
        iso[20*SECTOR+33]=1
        with self.assertRaisesRegex(ValueError,'dot'):
            inspect(iso)

    def test_invalid_extent_and_endian_copies(self):
        for size in (23, 0xffffffff):
            iso = fixture()
            off = 20*SECTOR+68+2
            both32(iso, off, size)
            with self.assertRaisesRegex(ValueError, 'outside'):
                inspect(iso)
        iso = fixture()
        iso[16*SECTOR+80] ^= 1
        with self.assertRaisesRegex(ValueError, 'endian'):
            inspect(iso)

    def test_truncated_descriptors_and_directory(self):
        for length in (255, 16*SECTOR, 18*SECTOR, 21*SECTOR+2):
            with self.assertRaises(ValueError):
                inspect(fixture()[:length])
        iso = fixture()
        iso[20*SECTOR+68] = 34
        with self.assertRaisesRegex(ValueError, 'identifier'):
            inspect(iso)

    def test_ip_size_is_distinct_from_container(self):
        iso = fixture()
        struct.pack_into('>I', iso, 0xe0, 0x8000)
        self.assertIn('invalid_first_read_address_or_ip_overlap', parse_ip(iso)['issues'])
        with self.assertRaisesRegex(ValueError, 'overlap'):
            inspect(iso, strict_ip=True)

    def test_independent_reserved_and_stack_variants(self):
        iso = fixture()
        struct.pack_into('>I', iso, 0xe4, 0x060ffffc)
        struct.pack_into('>I', iso, 0xec, 0x06001000)
        outputs = dict(variants(iso))
        self.assertEqual(len(parse_ip(outputs['original'])['issues']), 2)
        self.assertEqual(parse_ip(outputs['both_zero'])['issues'], [])
        self.assertEqual(len(parse_ip(outputs['reserved_e4_zero'])['issues']), 1)
        self.assertEqual(len(parse_ip(outputs['slave_ec_default'])['issues']), 1)


if __name__ == '__main__':
    unittest.main()
