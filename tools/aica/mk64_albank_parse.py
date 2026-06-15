#!/usr/bin/env python3
"""
MK64 ALBankFile/AudioBank parser. Parses the raw big-endian audiobanks.us.bin
(.ctl) + audiotables.bin (.tbl) and enumerates/dedups every referenced VADPCM
sample, producing transcode-compatible Sample objects (same interface as the
OoT audiobank_parse.Sample so transcode.py/yamaha_adpcm.py can be reused).

Both files are ALSeqFile wrappers:
  header { s16 revision; s16 seqCount } then seqArray[seqCount]{ u32 off; u32 len }  (big-endian)
audiotables: all entries -> one shared VADPCM blob at offset 0xB0.
audiobank i: ctl region at seqArray[i].off begins [u32 numInstruments][u32 numDrums];
  AudioBank proper at +0x10; internal offsets are relative to that bankBase.
"""

import struct

FRAME = {0: (16, 9)}   # VADPCM: 16 samples / 9 bytes


def be16(b, o):
    return struct.unpack_from(">h", b, o)[0]

def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]

def bef32(b, o):
    return struct.unpack_from(">f", b, o)[0]


class Sample:
    __slots__ = ("bank", "addr", "size", "codec", "has_loop", "loop_start",
                 "loop_end", "loop_count", "nsamples", "order", "npredictors",
                 "book", "data", "banks", "tunings")

    def __init__(self, addr, size, codec, loop, book, data):
        self.bank = 0          # shared tbl -> single bank space (key = addr)
        self.addr = addr
        self.size = size
        self.codec = codec
        if loop is None:
            self.has_loop, self.loop_start, self.loop_end, self.loop_count = False, 0, 0, 0
        else:
            self.loop_start, self.loop_end, self.loop_count = loop
            self.has_loop = self.loop_count != 0
        smp_per, byte_per = FRAME[codec]
        self.nsamples = (size // byte_per) * smp_per
        self.order, self.npredictors, self.book = book
        self.data = data
        self.banks = set()
        self.tunings = set()


def _read_alseqfile(buf):
    """Return (revision, seqCount, [(off,len)...])."""
    rev = be16(buf, 0)
    n = be16(buf, 2)
    entries = []
    o = 4
    for _ in range(n):
        off = be32(buf, o)
        ln = be32(buf, o + 4)
        entries.append((off, ln))
        o += 8
    return rev, n, entries


def _read_book(ab, off):
    order = be32(ab, off)
    npred = be32(ab, off + 4)
    n = 8 * order * npred
    coeffs = struct.unpack_from(f">{n}h", ab, off + 8)
    return order, npred, coeffs


def _read_loop(ab, off):
    start, end, count, _pad = struct.unpack_from(">IIII", ab, off)
    return (start, end, count)


def parse_all(audiobanks_path, audiotables_path, with_data=True):
    with open(audiobanks_path, "rb") as f:
        ab = f.read()
    with open(audiotables_path, "rb") as f:
        tbl_bytes = f.read()
    tbl = tbl_bytes if with_data else None

    _, _, ctl_entries = _read_alseqfile(ab)
    _, _, tbl_entries = _read_alseqfile(tbl_bytes)
    tbl_base = tbl_entries[0][0]   # shared blob base (0xB0)

    samples = {}   # addr -> Sample

    def read_sound(bank_base, sound_off, bank_idx, tuning):
        if sound_off == 0:
            return
        # AudioBankSample at bank_base + sample_off
        samp_off = be32(ab, bank_base + sound_off + 0)
        # tuning already read by caller
        if samp_off == 0:
            return
        sh = bank_base + samp_off
        # AudioBankSample: u8 unused, u8 loaded, pad2, u32 sampleAddr, u32 loop, u32 book, u32 sampleSize
        sample_addr = be32(ab, sh + 4)
        loop_off = be32(ab, sh + 8)
        book_off = be32(ab, sh + 12)
        sample_size = be32(ab, sh + 16)
        key = sample_addr
        if key in samples:
            samples[key].banks.add(bank_idx)
            samples[key].tunings.add(tuning)
            return
        book = _read_book(ab, bank_base + book_off)
        loop = _read_loop(ab, bank_base + loop_off) if loop_off else None
        data = None
        if tbl is not None:
            data = tbl[tbl_base + sample_addr: tbl_base + sample_addr + sample_size]
        s = Sample(key, sample_size, 0, loop, book, data)
        s.banks.add(bank_idx)
        s.tunings.add(tuning)
        samples[key] = s

    for i, (boff, blen) in enumerate(ctl_entries):
        if blen == 0:
            continue
        num_inst = be32(ab, boff + 0)
        num_drums = be32(ab, boff + 4)
        bank_base = boff + 0x10
        # AudioBank: u32 drumsOff; u32 instPtr[num_inst]
        drums_off = be32(ab, bank_base + 0)
        for j in range(num_inst):
            ip = be32(ab, bank_base + 4 + 4 * j)
            if ip == 0:
                continue
            inst = bank_base + ip
            # Instrument: loaded,lo,hi,release (4); u32 env; 3x AudioBankSound{u32 sample, f32 tuning}
            for k in range(3):
                so = inst + 8 + k * 8
                samp = be32(ab, so)
                tun = bef32(ab, so + 4)
                if samp != 0 and tun != 0.0:
                    read_sound(bank_base, so - bank_base, i, tun)
        if drums_off:
            for d in range(num_drums):
                dp = be32(ab, bank_base + drums_off + 4 * d)
                if dp == 0:
                    continue
                drum = bank_base + dp
                # Drum: u8 release,pan,loaded,pad; AudioBankSound sound (off4); u32 env
                so = drum + 4
                samp = be32(ab, so)
                tun = bef32(ab, so + 4)
                if samp != 0 and tun != 0.0:
                    read_sound(bank_base, so - bank_base, i, tun)

    return tbl_base, samples


if __name__ == "__main__":
    import sys
    tbl_base, samples = parse_all(sys.argv[1], sys.argv[2])
    print(f"tbl_base=0x{tbl_base:X}  unique samples={len(samples)}")
