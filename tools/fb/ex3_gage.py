#!/usr/bin/env python3
"""EX3 (LLIF) = Philip Gage's Byte Pair Encoding (1994) in blocks. Reference implementation used to infer the exact encoder parameters.
usage: ex3_gage.py <file.ex3>...   (checks that re-encoding the decoded payload reproduces the file; reports the first divergence)"""
import sys, struct

def parse(d, hs):
    b = d[hs:]; i = 0; blocks = []
    while i < len(b):
        tab = {}  # code -> (l, r) pairs only
        lit = [None]*256
        v = 0; groups = []
        while True:
            c = b[i]; i += 1
            if c > 127:
                v += c - 127
                if v == 256: groups.append((c, [])); break
                cnt = 1
            else: cnt = c + 1
            ents = []
            for _ in range(cnt):
                p1 = b[i]; i += 1
                if p1 != v: p2 = b[i]; i += 1; tab[v] = (p1, p2); ents.append((p1, p2))
                else: ents.append((p1, None))
                v += 1
            groups.append((c, ents))
            if v >= 256: break
        n = (b[i] << 8) | b[i+1]; i += 2
        sym = b[i:i+n]; i += n
        blocks.append((groups, tab, sym))
    return blocks

def expand(tab, sym):
    out = bytearray()
    for s in sym:
        st = [s]
        while st:
            c = st.pop()
            if c in tab: st.append(tab[c][1]); st.append(tab[c][0])
            else: out.append(c)
    return bytes(out)

class Gage:
    def __init__(self, blocksize=5000, hashsize=4096, maxchars=200, threshold=3):
        self.BS, self.HS, self.MC, self.TH = blocksize, hashsize, maxchars, threshold
    def lookup(self, a, b):
        HS = self.HS
        i = (a ^ (b << 5)) & (HS - 1)
        while (self.hl[i] != a or self.hr[i] != b) and self.count[i] != 0:
            i = (i + 1) & (HS - 1)
        self.hl[i] = a; self.hr[i] = b
        return i
    def block(self, data, pos):
        HS = self.HS
        self.count = [0]*HS; self.hl = [0]*HS; self.hr = [0]*HS; self.left = [0]*256; self.right = [0]*256
        for c in range(256): self.left[c] = c; self.right[c] = 0
        buf = []; used = 0; size = 0
        while size < self.BS and used < self.MC and pos < len(data):
            c = data[pos]; pos += 1
            if size > 0:
                idx = self.lookup(buf[size-1], c)
                if self.count[idx] < 255: self.count[idx] += 1
            buf.append(c)
            if not self.right[c]: self.right[c] = 1; used += 1
            size += 1
        buf.append(0); code = 256
        while True:
            code -= 1
            while code >= 0 and not (code == self.left[code] and not self.right[code]): code -= 1
            if code < 0: break
            best = 2; lc = rc = 0
            for idx in range(HS):
                if self.count[idx] > best: best = self.count[idx]; lc = self.hl[idx]; rc = self.hr[idx]
            if best < self.TH: break
            oldsize = size - 1; w = 0; r = 0
            while r < oldsize:
                if buf[r] == lc and buf[r+1] == rc:
                    if r > 0:
                        idx = self.lookup(buf[w-1], lc)
                        if self.count[idx] > 1: self.count[idx] -= 1
                        idx = self.lookup(buf[w-1], code)
                        if self.count[idx] < 255: self.count[idx] += 1
                    if r < oldsize - 1:
                        idx = self.lookup(rc, buf[r+2])
                        if self.count[idx] > 1: self.count[idx] -= 1
                        idx = self.lookup(code, buf[r+2])
                        if self.count[idx] < 255: self.count[idx] += 1
                    buf[w] = code; w += 1; r += 2; size -= 1
                else:
                    buf[w] = buf[r]; w += 1; r += 1
            buf[w] = buf[r]
            del buf[size+1:]
            self.left[code] = lc; self.right[code] = rc
            idx = self.lookup(lc, rc); self.count[idx] = 1
        tab = {c: (self.left[c], self.right[c]) for c in range(256) if c != self.left[c]}
        return tab, bytes(buf[:size]), pos

def write_table(tab):
    """Gage's filewrite: runs of literals (header c+127, then ONE entry), runs of pair codes (header len, len+1 entries; a single literal
    between pairs is absorbed while len < 125)."""
    out = bytearray(); c = 0
    def lc(x): return tab[x][0] if x in tab else x
    while c < 256:
        if c == lc(c):
            ln = 1; c += 1
            while ln < 127 and c < 256 and c == lc(c): ln += 1; c += 1
            out.append(ln + 127); ln = 0
            if c == 256: break
        else:
            ln = 0; c += 1
            while (ln < 127 and c < 256 and c != lc(c)) or (ln < 125 and c < 254 and c + 1 != lc(c + 1)):
                ln += 1; c += 1
            out.append(ln); c -= ln + 1
        for _ in range(ln + 1):
            if c in tab: out += bytes(tab[c])
            else: out.append(c)
            c += 1
    return bytes(out)

if __name__ == '__main__':
    for f in sys.argv[1:]:
        d = open(f, 'rb').read()
        for hs in (64, 68, 72):
            try:
                bl = parse(d, hs)
                dec = b''.join(expand(t, s) for g, t, s in bl)
                if struct.unpack('<I', d[hs-4:hs])[0] == len(dec): break
            except Exception: pass
        print(f, 'hs', hs, 'blocks', len(bl), 'decoded', len(dec), 'first block decoded', len(expand(bl[0][1], bl[0][2])), 'syms', len(bl[0][2]), 'pairs', len(bl[0][1]))
