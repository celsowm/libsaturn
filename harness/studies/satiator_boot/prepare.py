"""Prepare loading-screen and legacy CD boot comparisons without changing builds."""
import argparse
import hashlib
import json
from pathlib import Path

# Relative branches retain their meaning when the IP hook copies this code.
# At IP offset 0xED8: read HIRQ; tst #CMOK; bf done; dt r7; bf poll.
HOOK_OFFSET = 0xED8
EXPECTED_POLL = bytes.fromhex('6051c8018b0147108bfa')
# Replace the counter expiration path with bra poll / nop. The preceding
# CMOK branch remains the only successful exit. This is now the official
# default hook; retain the old loop only for regression comparison.
WAIT_FOR_CMOK = bytes.fromhex('6051c8018b01affb0009')


def prepare(with_bar: Path, without_bar: Path, output: Path):
    output.mkdir(parents=True, exist_ok=True)
    original = with_bar.read_bytes()
    assert original[:16] == b'SEGA SEGASATURN '
    assert original[HOOK_OFFSET:HOOK_OFFSET + len(EXPECTED_POLL)] in (EXPECTED_POLL, WAIT_FOR_CMOK), \
        'Unexpected boot hook: refusing to patch a different template'
    candidate = bytearray(original)
    candidate[HOOK_OFFSET:HOOK_OFFSET + len(EXPECTED_POLL)] = WAIT_FOR_CMOK
    assert candidate[:HOOK_OFFSET] == original[:HOOK_OFFSET]
    assert candidate[HOOK_OFFSET + len(EXPECTED_POLL):] == original[HOOK_OFFSET + len(EXPECTED_POLL):]
    legacy = bytearray(original)
    legacy[HOOK_OFFSET:HOOK_OFFSET + len(EXPECTED_POLL)] = EXPECTED_POLL
    images = {'with_bar.iso': original, 'without_bar.iso': without_bar.read_bytes(),
              'cmok_wait.iso': candidate, 'legacy_poll.iso': legacy}
    metadata = {}
    for name, data in images.items():
        (output / name).write_bytes(data)
        metadata[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (output / 'images.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--with-bar', required=True, type=Path)
    parser.add_argument('--without-bar', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    prepare(args.with_bar, args.without_bar, args.output)
