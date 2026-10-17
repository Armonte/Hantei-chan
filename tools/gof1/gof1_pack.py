#!/usr/bin/env python3
"""Glove on Fight 1 (gof.exe, 2002) .p archive reader.  See docs/formats/gof1.md.

Layout (all little endian):
  +0   u32 plain_flag   0 = file data enciphered (gof_00/01/03), non-zero = plain (gof_02, mp3)
  +4   u32 count ^ 0xFA261EFB
  +8   count * 64-byte entries:
         +0   char name[56]   byte j ^= (3*(j*entry_index - 28)) & 0xFF   (zero padded, SJIS)
         +56  u32 size ^ 0xFA261EFB
         +60  u32 absolute file offset (plain)
  data: if plain_flag == 0, the first min(size, 9696) bytes of each file are
        XORed:  b[i] ^= (i + upper(name)[i % len(name)]) & 0xFF   (upper = CharUpperA, SJIS aware)
(Evidence: LoadArchiveAndDecryptIndex 0x423B50, Archive_XOR_Decrypt_With_Filename 0x423D60.)
"""
import collections, os, struct, sys

KEY = 0xFA261EFB
CIPHER_SPAN = 9696


def sjis_upper(b):
    """CharUpperA under code page 932: ASCII a-z become A-Z, double-byte (lead 0x81-0x9F/0xE0-0xFC + trail) characters are left alone."""
    out = bytearray(b)
    i = 0
    while i < len(out):
        c = out[i]
        if 0x81 <= c <= 0x9F or 0xE0 <= c <= 0xFC:
            i += 2
            continue
        if 0x61 <= c <= 0x7A:
            out[i] = c - 0x20
        i += 1
    return bytes(out)


class Entry:
    __slots__ = ("index", "name", "size", "offset")

    def __init__(self, index, name, size, offset):
        self.index, self.name, self.size, self.offset = index, name, size, offset


class Archive:
    def __init__(self, path):
        self.path = path
        self.fsize = os.path.getsize(path)
        with open(path, "rb") as f:
            self.plain_flag, c = struct.unpack("<II", f.read(8))
            self.count = c ^ KEY
            if self.count > 100000:
                raise ValueError("implausible entry count (not a GOF1 archive?)")
            raw = f.read(self.count * 64)
        self.entries = []
        for i in range(self.count):
            e = bytearray(raw[i * 64:i * 64 + 64])
            for j in range(56):
                e[j] ^= (3 * (j * i - 28)) & 0xFF
            name = bytes(e[:56]).split(b"\0")[0]
            size, off = struct.unpack_from("<II", e, 56)
            self.entries.append(Entry(i, name, size ^ KEY, off))

    @property
    def data_start(self):
        return 8 + self.count * 64

    def validate(self):
        """Return list of problems (empty = consistent)."""
        bad = []
        pos = self.data_start
        for e in self.entries:
            if e.offset != pos:
                bad.append("entry %d %r: offset %#x != running %#x" % (e.index, e.name, e.offset, pos))
            pos = e.offset + e.size
        if pos != self.fsize:
            bad.append("sum of entries ends at %d but file is %d" % (pos, self.fsize))
        return bad

    def read(self, e):
        with open(self.path, "rb") as f:
            f.seek(e.offset)
            d = bytearray(f.read(e.size))
        if self.plain_flag == 0:
            key = sjis_upper(e.name)
            n = min(len(d), CIPHER_SPAN)
            kl = len(key)
            for i in range(n):
                d[i] ^= (i + key[i % kl]) & 0xFF
        return bytes(d)

    def find(self, name):
        n = sjis_upper(name.encode("cp932", "replace") if isinstance(name, str) else name)
        for e in self.entries:
            if sjis_upper(e.name) == n:
                return e
        return None


# --- character .dat second cipher (SpriteDataSlot_LoadCharacterDat 0x42B330) ---
# Crypto_XorWithKeyString(buf, len, limit, key): buf[p] ^= (p + key[p % len(key)]) & 0xFF, p relative to the section start.
# Keys are NUL-terminated Shift-JIS strings in .data (unk_464568 / 464540 / 464588).
DAT_KEY_HEADER = bytes.fromhex("4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e")
DAT_KEY_PATTERN = bytes.fromhex("4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb")
DAT_KEY_BLOB = bytes.fromhex("686982dcc56e6f834a82c982e581482082b28bea984a836982b1546f82be82c982e5")
DAT_HEADER_SIZE = 0x444


def xor_key_string(buf, key):
    """Crypto_XorWithKeyString 0x4238C0: buf[p] ^= (p + key[p % len(key)]) & 0xFF (p = offset inside the buffer)."""
    n = len(buf)
    if n == 0:
        return b""
    try:
        import numpy as np
        p = np.arange(n, dtype=np.uint32)
        k = np.frombuffer(bytes(key), dtype=np.uint8)[p % len(key)]
        ks = ((p + k) & 0xFF).astype(np.uint8)
        return (np.frombuffer(bytes(buf), dtype=np.uint8) ^ ks).tobytes()
    except ImportError:
        b = bytearray(buf)
        kl = len(key)
        for i in range(n):
            b[i] ^= (i + key[i % kl]) & 0xFF
        return bytes(b)


def decrypt_dat(raw):
    """Decrypt a (stage-1 decrypted) character .DAT.  Returns plain bytes of the same length."""
    d = bytearray(raw)
    d[:DAT_HEADER_SIZE] = xor_key_string(d[:DAT_HEADER_SIZE], DAT_KEY_HEADER)
    pat_end, parts_size, cg_off, cg_size = struct.unpack_from("<IIII", d, 0x14)
    d[DAT_HEADER_SIZE:pat_end] = xor_key_string(d[DAT_HEADER_SIZE:pat_end], DAT_KEY_PATTERN)
    d[pat_end:pat_end + parts_size] = xor_key_string(d[pat_end:pat_end + parts_size], DAT_KEY_BLOB)
    d[cg_off:cg_off + cg_size] = xor_key_string(d[cg_off:cg_off + cg_size], DAT_KEY_BLOB)
    # editor-only tail: 256 x 64-byte pattern names, enciphered with the blob key from the tail start (the game never reads it)
    tail = cg_off + cg_size
    if len(d) - tail == 0x4000:
        d[tail:] = xor_key_string(d[tail:], DAT_KEY_BLOB)
    return bytes(d)


def _dec(b):
    return b.decode("cp932", "replace")


def _safe(b):
    return _dec(b).replace("/", "_").replace("\\", "_")


def main(argv):
    if len(argv) < 3 or argv[1] not in ("ls", "extract", "decdat", "chars"):
        print(__doc__)
        print("usage: gof1_pack.py ls <archive>\n       gof1_pack.py extract <archive> <name|-all> <outdir>")
        return 2
    if argv[1] == "decdat":   # decdat <stage1.DAT> <out>
        open(argv[3], "wb").write(decrypt_dat(open(argv[2], "rb").read()))
        return 0
    a = Archive(argv[2])
    if argv[1] == "ls":
        print("# %s: %d entries, plain_flag=%d, file size %d" % (argv[2], a.count, a.plain_flag, a.fsize))
        for e in a.entries:
            print("%5d %#010x %10d %s" % (e.index, e.offset, e.size, _dec(e.name)))
        for p in a.validate():
            print("WARN", p)
        return 0
    if argv[1] == "chars":    # chars <archive> <outdir>: extract every *.DAT entry and run the character cipher
        os.makedirs(argv[3], exist_ok=True)
        n = 0
        for e in a.entries:
            if e.name.upper().endswith(b".DAT"):
                name = _safe(e.name)
                if sum(1 for x in a.entries if x.name.upper() == e.name.upper()) > 1:
                    name = "%03d_%s" % (e.index, name)
                with open(os.path.join(argv[3], name), "wb") as f:
                    f.write(decrypt_dat(a.read(e)))
                n += 1
        print("decrypted", n, "character .DAT files")
        return 0
    if len(argv) < 5:
        print("usage: extract <archive> <name|-all> <outdir>")
        return 2
    out = argv[4]
    os.makedirs(out, exist_ok=True)
    todo = a.entries if argv[3] == "-all" else [a.find(argv[3])]
    if todo == [None]:
        print("not found")
        return 1
    seen = collections.Counter(e.name.upper() for e in a.entries)
    for e in todo:
        name = _safe(e.name)
        if seen[e.name.upper()] > 1:      # gof_01.p holds several entries with the same name
            name = "%03d_%s" % (e.index, name)
        with open(os.path.join(out, name), "wb") as f:
            f.write(a.read(e))
        print("extracted", name, e.size)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
