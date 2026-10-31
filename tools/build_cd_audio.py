"""Build a MODE1/2352 data track followed by one unmodified CD-DA track."""
import argparse
import hashlib
from pathlib import Path

from iso_to_raw import ISO_SECTOR_BYTES, RAW_SECTOR_BYTES, encode_mode1_sector, validate_mode1_sector

PREGAP_SECTORS = 150


def msf(sectors):
    minutes, rest = divmod(sectors, 75 * 60)
    seconds, frames = divmod(rest, 75)
    return f'{minutes:02d}:{seconds:02d}:{frames:02d}'


def build(iso, audio, output, cue):
    if len({p.resolve() for p in (iso, audio, output, cue)}) != 4:
        raise ValueError('Input and output paths must be distinct')
    iso_size, audio_size = iso.stat().st_size, audio.stat().st_size
    if iso_size == 0 or iso_size % ISO_SECTOR_BYTES:
        raise ValueError('ISO must contain whole 2048-byte sectors')
    if audio_size == 0 or audio_size % RAW_SECTOR_BYTES:
        raise ValueError('Audio must contain whole 2352-byte CD-DA sectors')
    data_sectors = iso_size // ISO_SECTOR_BYTES
    audio_sectors = audio_size // RAW_SECTOR_BYTES
    text = (f'FILE "{output.name}" BINARY\n'
            '  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n'
            f'  TRACK 02 AUDIO\n    INDEX 00 {msf(data_sectors)}\n'
            f'    INDEX 01 {msf(data_sectors + PREGAP_SECTORS)}\n')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + '.tmp')
    try:
        with iso.open('rb') as source, audio.open('rb') as pcm, temporary.open('wb') as raw:
            for lba in range(data_sectors):
                raw.write(encode_mode1_sector(lba, source.read(ISO_SECTOR_BYTES)))
            raw.write(bytes(PREGAP_SECTORS * RAW_SECTOR_BYTES))
            while chunk := pcm.read(1024 * RAW_SECTOR_BYTES):
                raw.write(chunk)
        # Verify the complete filesystem and exact PCM before publishing.
        audio_hash = hashlib.sha256()
        with iso.open('rb') as source, temporary.open('rb') as raw:
            for lba in range(data_sectors):
                if validate_mode1_sector(lba, raw.read(RAW_SECTOR_BYTES)) != source.read(ISO_SECTOR_BYTES):
                    raise ValueError(f'Data mismatch at LBA {lba}')
            if raw.read(PREGAP_SECTORS * RAW_SECTOR_BYTES) != bytes(PREGAP_SECTORS * RAW_SECTOR_BYTES):
                raise ValueError('Invalid audio pregap')
            while chunk := raw.read(1024 * RAW_SECTOR_BYTES):
                audio_hash.update(chunk)
        with audio.open('rb') as source:
            expected_hash = hashlib.file_digest(source, 'sha256').hexdigest()
        if audio_hash.hexdigest() != expected_hash:
            raise ValueError('CD-DA samples differ from the source')
        temporary.replace(output)
        cue.write_text(text, encoding='ascii')
    finally:
        temporary.unlink(missing_ok=True)
    return {'data_sectors': data_sectors, 'audio_sectors': audio_sectors,
            'audio_sha256': expected_hash, 'duration_seconds': audio_sectors / 75}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--audio', type=Path, required=True)
    parser.add_argument('--output-bin', type=Path, required=True)
    parser.add_argument('--output-cue', type=Path, required=True)
    args = parser.parse_args()
    print('[cd-audio]', build(args.iso, args.audio, args.output_bin, args.output_cue))
