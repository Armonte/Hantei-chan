#!/usr/bin/env python3
"""BMP Cutter3/2 (.cg) reader for the fxrecolor analysis tools (read-only, numpy).
Layout per docs/cg/cg_manager_design.md section 1."""
import struct, numpy as np

class Img:
    __slots__ = ('idx','name','type','w','h','bpp','bounds','as_','al','off','blob_off','blob_len')

class Bank:
    def __init__(self, path):
        b = self.b = open(path, 'rb').read()
        self.path = path
        assert b[:11] == b'BMP Cutter3', path
        u = lambda o: struct.unpack_from('<I', b, o)[0]
        self.pal = [np.frombuffer(b, np.uint8, 1024, 0x14 + 1024 * i).reshape(256, 4) for i in range(8)]  # BGRA
        d = 0x2014
        self.pages, self.nalign, self.nimg, self.cu = u(d) + 1, u(d + 8), u(d + 12), u(d + 16)
        self.idxtab = [u(d + 48 + 4 * i) for i in range(3000)]
        self.alignoff = u(d + 48 + 12000)
        self.images = {}
        offs = sorted((o, i) for i, o in enumerate(self.idxtab) if o != 0xFFFFFFFF and i < self.nimg)
        ends = {i: (offs[k + 1][0] if k + 1 < len(offs) else self.alignoff) for k, (o, i) in enumerate(offs)}
        for o, i in offs:
            m = Img(); m.idx = i; m.off = o
            m.name = b[o:o + 32].split(b'\0')[0].decode('latin1')
            m.type, m.w, m.h, m.bpp, x1, y1, x2, y2, m.as_, m.al = struct.unpack_from('<iIIIiiiiII', b, o + 32)
            m.bounds = (x1, y1, x2, y2); m.blob_off = o + 72; m.blob_len = ends[i] - o - 72
            self.images[i] = m
    def blocks(self, m):
        for j in range(m.al):
            yield struct.unpack_from('<iiiihhhh', self.b, self.alignoff + 24 * (m.as_ + j))  # dx dy w h sx sy page copy
    def embedded_palette(self, m):
        """type 2 / 4: 1024-byte BGRA palette at the start of the blob, else None."""
        if m.type in (2, 4):
            return np.frombuffer(self.b, np.uint8, 1024, m.blob_off).reshape(256, 4)
        return None

TYPE_NAMES = {0: 'idx(bank pal)', 1: 'BGRA', 2: 'idx+own pal', 3: 'flat+alpha', 4: 'idx+own pal+alpha', -1: 'empty'}
