#!/usr/bin/env python3
"""Replace one image record (header fields + data blob) of a BMP Cutter3 bank, keeping every other byte.
Used by the type-5 in-game proof (docs/cg/type5_report.md). Image offsets / align offset / total size are re-derived."""
import struct

IDX = 0x2014 + 48            # image offset table
TAIL = IDX + 12000           # align offset dword
FIRST = TAIL + 12            # first image record

def patch_image(data, n, new_type=None, new_bpp=None, new_blob=None):
    b = bytearray(data)
    offs = [struct.unpack_from('<I', b, IDX + 4 * i)[0] for i in range(3000)]
    present = [(i, o) for i, o in enumerate(offs) if o != 0xFFFFFFFF]
    alignoff = struct.unpack_from('<I', b, TAIL)[0]
    ends = {}
    for k, (i, o) in enumerate(present):
        ends[i] = present[k + 1][1] if k + 1 < len(present) else alignoff
    o = offs[n]
    if new_type is not None: struct.pack_into('<i', b, o + 32, new_type)
    if new_bpp is not None: struct.pack_into('<I', b, o + 44, new_bpp)
    if new_blob is None: return bytes(b)
    out = bytes(b[:o + 72]) + bytes(new_blob) + bytes(b[ends[n]:])
    out = bytearray(out)
    delta = len(new_blob) - (ends[n] - o - 72)
    for i, oo in present:
        if oo > o: struct.pack_into('<I', out, IDX + 4 * i, oo + delta)
    struct.pack_into('<I', out, TAIL, alignoff + delta)
    struct.pack_into('<I', out, TAIL + 8, len(out))
    return bytes(out)

def patch_images(data, edits):
    """edits: {n: (type, bpp, blob)}; one pass, every other byte preserved."""
    offs = [struct.unpack_from('<I', data, IDX + 4 * i)[0] for i in range(3000)]
    present = [(i, o) for i, o in enumerate(offs) if o != 0xFFFFFFFF]
    alignoff = struct.unpack_from('<I', data, TAIL)[0]
    out = bytearray(data[:FIRST]); newoff = {}
    for k, (i, o) in enumerate(present):
        end = present[k + 1][1] if k + 1 < len(present) else alignoff
        rec = bytearray(data[o:end]); newoff[i] = len(out)
        if i in edits:
            t, bpp, blob = edits[i]
            struct.pack_into('<i', rec, 32, t); struct.pack_into('<I', rec, 44, bpp)
            rec[72:] = blob
        out += rec
    newalign = len(out)
    out += data[alignoff:]
    for i, o in newoff.items(): struct.pack_into('<I', out, IDX + 4 * i, o)
    struct.pack_into('<I', out, TAIL, newalign)
    struct.pack_into('<I', out, TAIL + 8, len(out))
    return bytes(out)
