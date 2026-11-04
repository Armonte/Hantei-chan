#!/usr/bin/env python3
"""Inventory differences between DFCI arcade grpdat/ and PS3 grpdat_ps3/.

Read-only on game data. Usage:
  inventory.py --arcade grpdat --ps3 grpdat_ps3 [--json out.json] [--samples N]
"""
import argparse, collections, difflib, gzip, json, os, sys

PANIBIN = b"PANIBin\0"
PANIDATA = b"PAniDataFile"


def walk(root):
    out = {}
    for dp, _, fns in os.walk(root):
        for fn in fns:
            p = os.path.join(dp, fn)
            out[os.path.relpath(p, root).replace(os.sep, "/")] = p
    return out


def norm(rel):
    """Logical path: strip .gz wrapper, lowercase for matching."""
    r = rel[:-3] if rel.lower().endswith(".gz") else rel
    return r.lower()


def ext_of(rel):
    r = rel[:-3] if rel.lower().endswith(".gz") else rel
    return os.path.splitext(r)[1].lower().lstrip(".") or "(none)"


def unwrap(data):
    if data[:2] == b"\x1f\x8b":
        return gzip.decompress(data), True
    return data, False


def swapped(a, b, width):
    if len(a) != len(b) or len(a) % width:
        return False
    for i in range(0, len(a), width):
        if a[i:i + width] != b[i:i + width][::-1]:
            return False
    return True


def text_info(data):
    info = {"bom": None, "eol": None, "encoding": None}
    body = data
    if data.startswith(b"\xef\xbb\xbf"):
        info["bom"] = "utf-8"; body = data[3:]
    elif data[:2] in (b"\xff\xfe", b"\xfe\xff"):
        info["bom"] = "utf-16le" if data[:2] == b"\xff\xfe" else "utf-16be"
    crlf = body.count(b"\r\n"); lf = body.count(b"\n") - crlf
    info["eol"] = "CRLF" if crlf and not lf else "LF" if lf and not crlf else "mixed" if crlf else "none"
    if info["bom"] and info["bom"].startswith("utf-16"):
        info["encoding"] = info["bom"]; return info, data.decode(info["bom"])
    try:
        body.decode("ascii"); info["encoding"] = "ascii"; return info, body.decode("ascii")
    except UnicodeDecodeError:
        pass
    for enc in ("utf-8", "cp932"):
        try:
            t = body.decode(enc); info["encoding"] = enc; return info, t
        except UnicodeDecodeError:
            pass
    info["encoding"] = "binary/unknown"
    return info, body.decode("cp932", "replace")


def classify(a_raw, p_raw, ext):
    p, gz = unwrap(p_raw)
    a, agz = unwrap(a_raw)
    wrap = "gzip" if gz else "none"
    if a == p:
        return wrap, "identical"
    if p.startswith(PANIBIN) and a.startswith(PANIDATA):
        return wrap, "PANIBin-vs-PAniDataFile"
    if p[:8] != a[:8] and p.startswith(PANIBIN):
        return wrap, "PANIBin-vs-other"
    for w in (2, 4):
        if swapped(a, p, w):
            return wrap, "byteswapped%d" % (w * 8)
    if ext in ("csv", "txt"):
        return wrap, "text-differs"
    if len(a) == len(p):
        nd = sum(1 for x, y in zip(a, p) if x != y)
        return wrap, "same-size-differs(%d bytes)" % nd
    return wrap, "different"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arcade", required=True)
    ap.add_argument("--ps3", required=True)
    ap.add_argument("--json")
    ap.add_argument("--samples", type=int, default=5)
    args = ap.parse_args()

    A = walk(args.arcade); P = walk(args.ps3)
    An = {norm(k): k for k in A}; Pn = {norm(k): k for k in P}
    only_a = sorted(An[k] for k in An.keys() - Pn.keys())
    only_p = sorted(Pn[k] for k in Pn.keys() - An.keys())
    common = sorted(An.keys() & Pn.keys())

    rep = {"arcade_files": len(A), "ps3_files": len(P), "common": len(common),
           "only_arcade": only_a, "only_ps3": only_p,
           "ps3_ext_raw": collections.Counter(os.path.splitext(k)[1].lower() for k in P),
           "ps3_wrapped_ext": collections.Counter(ext_of(k) for k in P if k.lower().endswith(".gz")),
           "per_ext": {}, "text": [], "pairs": {}}
    per = collections.defaultdict(collections.Counter)
    for k in common:
        a_path = A[An[k]]; p_path = P[Pn[k]]
        ext = ext_of(Pn[k])
        with open(a_path, "rb") as f: a_raw = f.read()
        with open(p_path, "rb") as f: p_raw = f.read()
        wrap, cls = classify(a_raw, p_raw, ext)
        key = cls if not cls.startswith("same-size") else "same-size-differs"
        per[ext]["%s/%s" % (wrap, key)] += 1
        rep["pairs"][An[k]] = {"ps3": Pn[k], "wrap": wrap, "class": cls}
        if ext in ("csv", "txt"):
            ai, at = text_info(unwrap(a_raw)[0]); pi, pt = text_info(unwrap(p_raw)[0])
            al = at.splitlines(); pl = pt.splitlines()
            diff = [l for l in difflib.unified_diff(al, pl, lineterm="", n=0)
                    if l[:1] in "+-" and not l.startswith(("+++", "---"))]
            rep["text"].append({"path": An[k], "arcade": ai, "ps3": pi,
                                "arcade_lines": len(al), "ps3_lines": len(pl),
                                "content_same": al == pl,
                                "added": sum(1 for l in diff if l[0] == "+"),
                                "removed": sum(1 for l in diff if l[0] == "-"),
                                "sample": diff[:args.samples]})
    rep["per_ext"] = {e: dict(c) for e, c in sorted(per.items())}

    def dirs(lst):
        return collections.Counter(p.split("/")[0] for p in lst).most_common()

    print("arcade files: %d  ps3 files: %d  common logical paths: %d" % (len(A), len(P), len(common)))
    print("only arcade: %d  by top dir: %s" % (len(only_a), dirs(only_a)))
    print("only ps3:    %d  by top dir: %s" % (len(only_p), dirs(only_p)))
    print("only arcade by ext:", collections.Counter(ext_of(x) for x in only_a).most_common())
    print("only ps3 by ext:   ", collections.Counter(ext_of(x) for x in only_p).most_common())
    print("ps3 .gz inner ext: ", dict(rep["ps3_wrapped_ext"]))
    print("\nper-extension classification (wrap/class: count):")
    for e, c in rep["per_ext"].items():
        print("  %-6s %s" % (e, dict(c)))
    print("\ntext files:")
    enc = collections.Counter()
    for t in rep["text"]:
        enc[(t["arcade"]["encoding"], t["arcade"]["bom"], t["arcade"]["eol"],
             t["ps3"]["encoding"], t["ps3"]["bom"], t["ps3"]["eol"])] += 1
    for k, v in enc.most_common():
        print("  arcade(enc=%s bom=%s eol=%s) -> ps3(enc=%s bom=%s eol=%s): %d" % (k + (v,)))
    nd = [t for t in rep["text"] if not t["content_same"]]
    print("  content identical after decode: %d / %d" % (len(rep["text"]) - len(nd), len(rep["text"])))
    for t in nd:
        print("  DIFF %s  lines %d->%d  +%d -%d" % (t["path"], t["arcade_lines"], t["ps3_lines"], t["added"], t["removed"]))
        for s in t["sample"][:3]:
            print("      " + s[:120])
    if args.json:
        rep["ps3_ext_raw"] = dict(rep["ps3_ext_raw"]); rep["ps3_wrapped_ext"] = dict(rep["ps3_wrapped_ext"])
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(rep, f, ensure_ascii=False, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
