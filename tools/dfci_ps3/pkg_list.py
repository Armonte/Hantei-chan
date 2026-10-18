#!/usr/bin/env python3
"""List / extract a PS3 .pkg (debug revision 0x0000, or retail 0x8000).

Debug pkgs use the SHA1 keystream derived from the header QA digest; retail
pkgs use AES-128-CTR with the public PS3 pkg key and the header IV.

  pkg_list.py FILE.pkg                 # list items
  pkg_list.py FILE.pkg --extract DIR   # extract (DIR must be outside any repo/game dir)
  pkg_list.py FILE.pkg --npd           # also report NPD (EDATA/SDAT) headers
"""
import argparse, hashlib, os, struct, sys

PS3_PKG_KEY = bytes.fromhex("2E7B71D7C9C9A14EA3221F188828B8F8")


class Pkg:
    def __init__(self, path):
        self.f = open(path, "rb")
        h = self.f.read(0x80)
        if h[:4] != b"\x7fPKG":
            raise SystemExit("not a PKG")
        self.revision, self.type = struct.unpack(">HH", h[4:8])
        self.meta_off, self.meta_cnt, self.hdr_size, self.item_cnt = struct.unpack(">IIII", h[8:0x18])
        self.total_size, self.data_off, self.data_size = struct.unpack(">QQQ", h[0x18:0x30])
        self.content_id = h[0x30:0x60].rstrip(b"\0").decode("ascii", "replace")
        self.digest = h[0x60:0x70]
        self.riv = h[0x70:0x80]
        self.debug = (self.revision & 0x8000) == 0
        if not self.debug:
            from Crypto.Cipher import AES
            self._aes = AES.new(PS3_PKG_KEY, AES.MODE_ECB)

    def _keystream(self, block0, nblocks):
        out = bytearray()
        if self.debug:
            d = self.digest
            base = d[0:8] + d[0:8] + d[8:16] + d[8:16] + b"\0" * 24
            for i in range(block0, block0 + nblocks):
                out += hashlib.sha1(base + struct.pack(">Q", i)).digest()[:16]
        else:
            iv = int.from_bytes(self.riv, "big")
            ctrs = b"".join(((iv + i) & ((1 << 128) - 1)).to_bytes(16, "big")
                            for i in range(block0, block0 + nblocks))
            out += self._aes.encrypt(ctrs)
        return bytes(out)

    def read(self, rel, size):
        """Read+decrypt `size` bytes at data-relative offset `rel`."""
        start = rel & ~15
        end = (rel + size + 15) & ~15
        self.f.seek(self.data_off + start)
        enc = self.f.read(end - start)
        ks = self._keystream(start // 16, len(enc) // 16)
        dec = bytes(a ^ b for a, b in zip(enc, ks)) if len(enc) < 4096 else \
            (int.from_bytes(enc, "big") ^ int.from_bytes(ks[:len(enc)], "big")).to_bytes(len(enc), "big")
        return dec[rel - start: rel - start + size]

    def items(self):
        tbl = self.read(0, self.item_cnt * 32)
        for i in range(self.item_cnt):
            no, ns, do, ds, fl, _ = struct.unpack(">IIQQII", tbl[i * 32:(i + 1) * 32])
            name = self.read(no, ns).decode("utf-8", "replace")
            yield name, do, ds, fl


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkg")
    ap.add_argument("--extract")
    ap.add_argument("--npd", action="store_true")
    a = ap.parse_args()
    p = Pkg(a.pkg)
    print(f"# revision=0x{p.revision:04x} ({'debug' if p.debug else 'retail'}) type={p.type} "
          f"items={p.item_cnt} data_off=0x{p.data_off:x} data_size=0x{p.data_size:x} content_id={p.content_id}")
    for name, do, ds, fl in p.items():
        kind = fl & 0xff
        tag = ""
        if (a.npd or a.extract) and kind != 4 and ds >= 4:
            if p.read(do, 4) == b"NPD\0":
                tag = " NPD"
        print(f"{fl:08x} {ds:10d} {name}{tag}")
        if a.extract:
            dst = os.path.join(a.extract, name)
            if kind == 4:
                os.makedirs(dst, exist_ok=True)
                continue
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "wb") as o:
                pos = 0
                while pos < ds:
                    n = min(1 << 20, ds - pos)
                    o.write(p.read(do + pos, n))
                    pos += n


if __name__ == "__main__":
    main()
