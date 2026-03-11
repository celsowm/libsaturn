import importlib.util
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
TOOL = REPO / "tools" / "gen_ip_bin.py"

SPEC = importlib.util.spec_from_file_location("gen_ip_bin", TOOL)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Nao foi possivel carregar tools/gen_ip_bin.py")
GEN_IP_BIN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN_IP_BIN)


class GenIpBinTests(unittest.TestCase):
    def test_build_ip_bin_uses_expected_offsets(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            stub_path = tmp_path / "stub.bin"
            stub_payload = bytes(range(64))
            stub_path.write_bytes(stub_payload)

            data = GEN_IP_BIN.build_ip_bin(
                maker="SEGA ENTERPRISES",
                product="T-00000G  ",
                version="V1.000",
                date_yyyymmdd="20260311",
                game_name="LIBSATURN MVP",
                ip_load_address=0x06004000,
                first_read_address=0x06010000,
                first_read_size=0x1234,
                master_stack=0x060FFFFC,
                slave_stack=0x06001000,
                stub_path=str(stub_path),
            )

        self.assertEqual(len(data), 0x8000)
        self.assertEqual(data[0x000:0x010], b"SEGA SEGASATURN ")
        self.assertEqual(data[0x03C:0x046], b"JT  UB E  ")
        self.assertEqual(data[0x046:0x04C], b"J     ")
        self.assertEqual(struct.unpack(">I", data[0x0D0:0x0D4])[0], 0x06004000)
        self.assertEqual(struct.unpack(">I", data[0x0D4:0x0D8])[0], 0x800)
        self.assertEqual(struct.unpack(">I", data[0x0E0:0x0E4])[0], 0x06010000)
        self.assertEqual(struct.unpack(">I", data[0x0E4:0x0E8])[0], 0x1234)
        self.assertEqual(data[0x600:0x640], bytes(range(64)))
        self.assertEqual(set(data[0x100:0x600]), {0})

    def test_entry_alias_sets_first_read_address(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            out_path = tmp_path / "ip.bin"
            result = subprocess.run(
                [
                    "python",
                    str(TOOL),
                    "--output",
                    str(out_path),
                    "--entry",
                    "06012000",
                ],
                cwd=REPO,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            raw = out_path.read_bytes()

        self.assertEqual(struct.unpack(">I", raw[0x0E0:0x0E4])[0], 0x06012000)

    def test_first_read_file_sets_size(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            out_path = tmp_path / "ip.bin"
            first_read = tmp_path / "0.BIN"
            first_read.write_bytes(bytes([0xAA]) * 321)

            result = subprocess.run(
                [
                    "python",
                    str(TOOL),
                    "--output",
                    str(out_path),
                    "--first-read-file",
                    str(first_read),
                ],
                cwd=REPO,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            raw = out_path.read_bytes()

        self.assertEqual(struct.unpack(">I", raw[0x0E4:0x0E8])[0], 321)


if __name__ == "__main__":
    unittest.main()
