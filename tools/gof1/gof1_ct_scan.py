#!/usr/bin/env python3
"""Data-side proof for Gof1CommandFlags / CT header (docs/formats/gof1.md 12.1, 12.4).

Scans every top-level entry of gof_00..03.p (the engine's Load_File_From_Archive searches the archive slots by plain file name, top level only) for
command tables (`<CHAR>_C.CT`, 4632 bytes, Gof1CtFile) and also for any entry of exactly 4632 bytes that is not named *.CT.  Prints, per file, the entry
count, the OR/AND of command `flags` / `flags2` and a histogram of the flag bytes; exit status 1 when a file does not parse as a CT.

usage: gof1_ct_scan.py [run_dir]     (default /mnt/c/games/gof1/run)
"""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gof1_pack import Archive

CT_SIZE = 4632
REC = 46


def parse_ct(d):
    count = struct.unpack_from("<I", d, 0)[0]
    cmds = []
    for i in range(100):
        o = 4 + i * REC
        cmds.append(dict(id=d[o], cls=d[o + 0x23], flags=d[o + 0x2C], flags2=d[o + 0x2D], seq=bytes(d[o + 2:o + 34])))
    hdr = d[0x11FC:0x11FC + 28]
    return count, cmds, hdr


def main(argv):
    run = argv[1] if len(argv) > 1 else "/mnt/c/games/gof1/run"
    tot_entries = tot_defined = tot_slots = 0
    flag_hist = collections.Counter()
    flag2_hist = collections.Counter()
    orf = orf2 = 0
    bad = 0
    nonct4632 = []
    scanned = 0
    print("%-12s %-16s %5s %7s %9s %9s  flags histogram (defined commands)" % ("archive", "entry", "count", "defined", "OR flags", "OR flags2"))
    for i in range(4):
        a = Archive(os.path.join(run, "gof_%02d.p" % i))
        for e in a.entries:
            scanned += 1
            nm = e.name.decode("cp932", "replace")
            if nm.upper().endswith(".CT") and e.size == CT_SIZE:
                d = a.read(e)
                count, cmds, hdr = parse_ct(d)
                defined = [c for c in cmds if c["id"] != 0xFF]
                ok = count == len(defined) and all(c["id"] in (k, 0xFF) for k, c in enumerate(cmds)) and count <= 100
                if not ok:
                    bad += 1
                f = collections.Counter(c["flags"] for c in defined)
                f2 = collections.Counter(c["flags2"] for c in defined)
                flag_hist.update(f)
                flag2_hist.update(f2)
                of = 0
                of2 = 0
                for c in cmds:   # all 100 records (the engine scans every slot), unused ones included
                    of |= c["flags"]
                    of2 |= c["flags2"]
                orf |= of
                orf2 |= of2
                tot_entries += 1
                tot_defined += len(defined)
                tot_slots += 100
                print("%-12s %-16s %5d %7d   0x%02X      0x%02X   %s%s" % (os.path.basename(a.path), nm, count, len(defined), of, of2,
                      " ".join("0x%02X:%d" % kv for kv in sorted(f.items())), "" if ok else "  !! id/count mismatch"))
            elif nm.upper().endswith(".CT"):
                print("%-12s %-16s size %d (not a command table: only *_C.CT of 4632 B are, CHARSEL.CT is the CSS grid)" % (os.path.basename(a.path), nm, e.size))
            elif e.size == CT_SIZE:
                nonct4632.append((os.path.basename(a.path), nm))
    print()
    print("archives scanned: 4, entries scanned: %d, CT files: %d, command records: %d (defined, id != 0xFF: %d)" % (scanned, tot_entries, tot_slots, tot_defined))
    print("non-.CT entries of 4632 bytes: %s" % (nonct4632 or "none"))
    print("flags value histogram over defined commands: %s" % " ".join("0x%02X:%d" % kv for kv in sorted(flag_hist.items())))
    print("flags2 value histogram over defined commands: %s" % " ".join("0x%02X:%d" % kv for kv in sorted(flag2_hist.items())))
    print("OR of all flags = 0x%02X  (bit 0x02 STANCE_AIR set: %s, bit 0x40 GUARD_CANCEL set: %s)" % (orf, bool(orf & 2), bool(orf & 0x40)))
    print("OR of all flags2 = 0x%02X" % orf2)
    return 1 if bad or not tot_entries else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
