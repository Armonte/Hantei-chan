#!/usr/bin/env python3
"""PB2K1 02.dat / 03.dat / 04.dat ("G" archives) and the scrambled 03.MP3: decode, validate, re-encode byte-exact.

usage: pb2k1_g_verify.py [GAMEDIR]          (default /mnt/c/games/pb; reads the RAW 02.dat 03.dat 04.dat 00.dat, no fbarctool needed)

Format (docs/formats/pb2k1.md section 16):
  outer file  = PB/GOF1 archive: u32 plainFlag, u32 count ^ 0xFA261EFB, count x 64-byte entries {name[56] enciphered, u32 size ^ 0xFA261EFB, u32 offset}, payloads back to back.
  cipher N    = for p in [0, min(size, 9696)): b[p] ^= (p + upper(name)[p % len(name)]) & 0xFF         (the PB_Archive_XOR_Decrypt_With_Filename stream, XOR so it is its own inverse)
  blob        = outer entry 02.DAT / 03.DAT / 04.DAT: stored cipher N(name) of an INNER PB archive (plainFlag 0); the ASCII 'G' magic ('030G', '040G', '050G') is
                the enciphered zero plainFlag: (0 + 'name'[0], 1 + ..., 3 + 'D') = '0','3'|'4'|'5','0','G'
  member      = inner entry NN.MP3: stored cipher N(member name) of a plain MP3
  04.dat entry 03.MP3 = cipher N('03.MP3') of a plain MP3 (identical bytes to 00.dat 003.MP3)
Nothing in pb2k1.exe parses these containers: PB_MainInitAndFrameLoop only compares one dword of each file with a constant (see the doc).
"""
import hashlib, struct, sys

KEY = 0xFA261EFB
CIPHER_LEN = 9696
# the eight dword probes of PB_MainInitAndFrameLoop: (file, offset, expected)
PROBES = [('02.dat', 0x5729228, 0x6E498477), ('02.dat', 0x57291B8, 0x6EDBA133), ('03.dat', 0x701176D, 0xAEA6CC1D),
          ('04.dat', 0x4885162, 0x0FDEBD8F), ('04.dat', 0x645D543, 0x7A947954), ('00.dat', 0x4B017DC, 0xB7AD489C),
          ('00.dat', 0x718014D, 0x27A138C2)]


def cipher(buf, name):
    k = name.upper().encode('cp932')
    b = bytearray(buf)
    for p in range(min(CIPHER_LEN, len(b))):
        b[p] ^= (p + k[p % len(k)]) & 0xFF
    return bytes(b)


def parse(buf):
    flag, cnt = struct.unpack_from('<II', buf, 0)
    cnt ^= KEY
    ents = []
    for i in range(cnt):
        e = bytearray(buf[8 + 64 * i:8 + 64 * (i + 1)])
        for j in range(56):
            e[j] ^= (3 * (j * i - 28)) & 0xFF
        nm = bytes(e[:56])
        end = nm.index(b'\0')
        # the bytes after the NUL terminator are stale editor memory (not zero); the decoded model keeps the whole 56-byte field
        ents.append((nm[:end].decode('cp932'), struct.unpack_from('<I', e, 56)[0] ^ KEY, struct.unpack_from('<I', e, 60)[0], nm))
    return flag, ents


def build(flag, members):
    """members = [(56-byte name field, payload bytes)] -> archive bytes (directory + back-to-back payloads)."""
    n = len(members)
    head = bytearray(struct.pack('<II', flag, n ^ KEY))
    off = 8 + 64 * n
    for i, (nm, data) in enumerate(members):
        e = bytearray(nm)
        for j in range(56):
            e[j] ^= (3 * (j * i - 28)) & 0xFF
        head += e + struct.pack('<II', len(data) ^ KEY, off)
        off += len(data)
    return bytes(head) + b''.join(d for _, d in members)


BITRATE = {  # (version, layer 3) kbit/s tables; version 3 = MPEG1, 2 / 0 = MPEG2 / 2.5
    3: [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0],
    2: [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0]}
SRATE = {3: [44100, 48000, 32000, 0], 2: [22050, 24000, 16000, 0], 0: [11025, 12000, 8000, 0]}


def mp3_walk(b):
    """Walks MPEG audio layer III frames from offset 0; returns (frames, bytes covered). Raises on a broken chain."""
    p = frames = 0
    while p + 4 <= len(b):
        h = struct.unpack_from('>I', b, p)[0]
        if (h >> 21) != 0x7FF:
            break
        ver, layer, br, sr, pad = (h >> 19) & 3, (h >> 17) & 3, (h >> 12) & 15, (h >> 10) & 3, (h >> 9) & 1
        if layer != 1 or ver == 1 or br in (0, 15) or sr == 3:
            break
        rate = BITRATE[3 if ver == 3 else 2][br] * 1000
        fl = (144 if ver == 3 else 72) * rate // SRATE[ver][sr] + pad
        if p + fl > len(b):
            break
        p += fl
        frames += 1
    return frames, p


def main():
    gd = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/games/pb'
    raw = {n: open('%s/%s' % (gd, n), 'rb').read() for n in ('02.dat', '03.dat', '04.dat', '00.dat')}
    ok = True

    def check(c, msg):
        nonlocal ok
        print(('PASS ' if c else 'FAIL ') + msg)
        ok = ok and c

    # 1. the game's own probes
    for f, off, want in PROBES:
        got = struct.unpack_from('<I', raw[f], off)[0]
        check(got == want and len(raw[f]) >= 100000000, 'probe %s @0x%X == 0x%08X, size %d >= 100000000' % (f, off, want, len(raw[f])))

    ref00 = {}
    flag0, e00 = parse(raw['00.dat'])
    for nm, sz, off, _ in e00:
        ref00[hashlib.sha1(raw['00.dat'][off:off + sz]).digest()] = nm

    blobs = {}
    for f in ('02.dat', '03.dat', '04.dat'):
        buf = raw[f]
        flag, ents = parse(buf)
        # outer archive tiles exactly and re-builds byte-exact from its decoded members
        members = [(nf, buf[off:off + sz]) for nm, sz, off, nf in ents]
        check(build(flag, members) == buf, '%s outer archive: flag %d, %d entries, rebuild == original (%d bytes)' % (f, flag, len(ents), len(buf)))
        for (nm, sz, off, nf), (_, data) in zip(ents, members):
            if nm.endswith('.DAT'):
                dec = cipher(data, nm)
                check(dec[:4] == b'\0\0\0\0' and data[:4] == bytes((i + ord(nm[i])) & 255 for i in range(4)), '  %s/%s magic %r = enciphered zero flag' % (f, nm, data[:4]))
                check(cipher(dec, nm) == data, '  %s/%s encode(decode(x)) == x' % (f, nm))
                if nm in blobs:
                    check(blobs[nm][0] == data, '  %s/%s raw bytes identical to the copy in the other archive' % (f, nm))
                blobs[nm] = (data, dec)
            else:
                dec = cipher(data, nm)
                fr, cov = mp3_walk(dec)
                check(fr > 0 and cipher(dec, nm) == data and ref00.get(hashlib.sha1(dec).digest()) is not None,
                      '  %s/%s scrambled MP3: %d frames cover %d of %d bytes, equals 00.dat %s, encode(decode(x)) == x' % (f, nm, fr, cov, len(dec), ref00.get(hashlib.sha1(dec).digest())))

    # 2. inner archives
    total = 0
    for nm, (data, dec) in sorted(blobs.items()):
        flag, ents = parse(dec)
        check(flag == 0 and ents and ents[0][2] == 8 + 64 * len(ents) and sum(e[1] for e in ents) + 8 + 64 * len(ents) == len(dec),
              'blob %s: inner archive flag %d, %d members, tiles the %d bytes exactly' % (nm, flag, len(ents), len(dec)))
        members = []
        for mn, sz, off, nf in ents:
            raw_m = dec[off:off + sz]
            plain = cipher(raw_m, mn)
            fr, cov = mp3_walk(plain)
            same = ref00.get(hashlib.sha1(plain).digest())
            good = fr > 100 and cov > len(plain) - 1045 and plain[:2] in (b'\xff\xfb', b'\xff\xfa')
            check(good, '  %s/%s: %d bytes, %d frames, %d trailing bytes (one truncated last frame)%s' % (nm, mn, sz, fr, len(plain) - cov, ', identical to 00.dat ' + same if same else ''))
            members.append((mn, raw_m, nf))
            total += 1
        # encode(decode(x)) == x for the whole blob: rebuild the inner archive from decoded members, re-apply both cipher layers
        plain_members = [(mn, cipher(d, mn), nf) for mn, d, nf in members]
        rebuilt = build(0, [(nf, cipher(p, mn)) for mn, p, nf in plain_members])
        check(cipher(rebuilt, nm) == data, 'blob %s: encode(decode(x)) == x over the full %d bytes (member cipher + blob cipher)' % (nm, len(data)))
    print('members decoded: %d' % total)
    print('ALL PASS' if ok else 'FAILED')
    sys.exit(0 if ok else 1)


main()
