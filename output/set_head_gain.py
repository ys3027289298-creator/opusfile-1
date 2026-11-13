#!/usr/bin/env python3
"""Rewrite the output_gain Q8 field of the first OpusHead packet in an Ogg Opus
file, rebuilding every affected page's CRC. No re-encode is performed."""
import struct
import sys


def crc32(data):
    """Ogg framing CRC: MSB-first, polynomial 0x04c11db7, init 0."""
    c = 0
    for b in data:
        c ^= b << 24
        for _ in range(8):
            if c & 0x80000000:
                c = ((c << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                c = (c << 1) & 0xFFFFFFFF
    return c


def main(path, gain_q8):
    data = bytearray(open(path, 'rb').read())
    i = 0
    patched = False
    while i < len(data):
        if data[i:i + 4] != b'OggS':
            i += 1
            continue
        nseg = data[i + 26]
        segs = data[i + 27:i + 27 + nseg]
        body_start = i + 27 + nseg
        body_end = body_start + sum(segs)
        body = data[body_start:body_end]
        if body.startswith(b'OpusHead') and not patched:
            struct.pack_into('<h', data, body_start + 16, gain_q8)
            patched = True
        # Recompute this page's CRC regardless, since the body may have changed.
        struct.pack_into('<I', data, i + 22, 0)
        struct.pack_into('<I', data, i + 22, crc32(data[i:body_end]))
        i = body_end
    if not patched:
        raise SystemExit('OpusHead packet not found')
    open(path, 'wb').write(data)


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]))
