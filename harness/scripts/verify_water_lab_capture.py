"""Verify scene/effect pairs in a 320×224 RGB24 Water Lab probe recording."""

import argparse
import json
from pathlib import Path
import sys


def layers(serial):
    base = ((5 + (serial & 7) * 3) * 8,
            ((10 + (serial >> 3)) & 31) * 8,
            ((18 + (serial >> 8)) & 31) * 8)
    effect = tuple(((serial >> shift) & 3) * 64 for shift in (0, 2, 4))
    return base, tuple((a + b) // 2 for a, b in zip(base, effect))


def check_pairs(samples, expected_pair, minimum_pairs, refresh):
    serial, disabled_fields, first_field = 0, 0, None
    base, _ = layers(32)
    disabled = expected_pair(base, base)
    held = {6: 0, 10: 0, 14: 0}
    last_field = -1
    prior_pair, next_pair = None, expected_pair(*layers(1))
    for field, sample in samples:
        if field != last_field + 1:
            raise ValueError(f"Missing field before {field}")
        last_field = field
        if sample == next_pair:
            if serial == 0:
                first_field = field
            serial += 1
            prior_pair, next_pair = next_pair, expected_pair(*layers(serial + 1))
        elif serial == 32 and sample == disabled:
            disabled_fields += 1
        elif serial:
            if sample != prior_pair:
                raise ValueError(f"Mismatched scene/effect in field {field} after pair {serial}")
            if serial == 32 and disabled_fields:
                raise ValueError(f"Old effect reappeared during the mode change in field {field}")
            if serial in held:
                held[serial] += 1
    if serial < minimum_pairs:
        raise ValueError(f"Expected {minimum_pairs} matched pairs, recorded {serial}")
    if disabled_fields < 3 or any(count < 1 for count in held.values()):
        raise ValueError("Capture did not show the three holds and the disabled layer")
    cadence = (serial - 1) * refresh / (last_field - first_field)
    if cadence < 20:
        raise ValueError(f"Measured presentation rate {cadence:.2f} pairs/s is below 20")
    return {
        "fields": last_field + 1,
        "first_pair_field": first_field,
        "matched_pairs": serial,
        "pairs_per_second": round(cadence, 2),
        "held_fields": held,
        "disabled_fields": disabled_fields,
    }


def raw_samples(source):
    width, height = 320, 224

    def pixel(frame, x, y):
        start = (y * width + x) * 3
        return tuple(frame[start:start + 3])

    visible = False
    frame_size = width * height * 3
    field = 0
    while frame := source.read(frame_size):
        if len(frame) != frame_size:
            raise ValueError(f"Incomplete RGB24 field {field}")
        base, blend = pixel(frame, 160, 40), pixel(frame, 159, 40)
        visible |= (base, blend) == layers(1)
        if visible:
            if pixel(frame, 280, 16) != (224, 232, 232):
                raise ValueError(f"Missing HUD marker in field {field}")
            if (pixel(frame, 200, 40), pixel(frame, 120, 40)) != (base, blend):
                raise ValueError(f"Misaligned composition edge in field {field}")
        yield field, (base, blend)
        field += 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="Headerless RGB24, one frame per field, or - for stdin")
    parser.add_argument("--refresh", type=int, choices=(50, 60), required=True)
    args = parser.parse_args()
    try:
        if args.capture == "-":
            result = check_pairs(raw_samples(sys.stdin.buffer), tuple_pair, 3600, args.refresh)
        else:
            with Path(args.capture).open("rb") as source:
                result = check_pairs(raw_samples(source), tuple_pair, 3600, args.refresh)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Water Lab capture: {error}\n")
    print(json.dumps(result, indent=2))


def tuple_pair(base, blend):
    return base, blend


if __name__ == "__main__":
    main()
