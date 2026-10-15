#!/usr/bin/env python3
"""ActCadenza moon-3 character voices -> MBAACC SeList slots (post-pass for make_actcadenza_moon3).

usage: actcadenza_selist.py DATA_DIR STAGING_DIR [--selftest]
         [--vanilla DIR] [--ogg-dir DIR ...] [--battle-se DIR] [--chars a,b,...]

How a voice is referenced
  EF type 9 ("play audio"), No 1 (character bank, random range) and No 3 (character bank, list):
    No 1: ids p0 .. p0+p2-1 (p2 <= 1 -> just p0); p1 = volume, p3.. = flags
    No 3: each of p0..p4 is one id (<= 0 unused)
  No 0/2 are the common bank (ids <= 52, same in both games) and are left alone.
  ActCadenza plays id N as  <CHAR>%02d.OGG  (effect\\%s%02d.ogg).
  MBAACC plays id N as the SeList slot N:  Character_LoadSlotSeList (0x41C3C0 in MBAA.exe) reads
  data\\<file>_SeList.txt [SeList] "NNN = name" lines and loads .\\se\\battle_se\\<name> (.ogg is
  tried first, then .wav) at SE-table id NNN + 250*playerSlot + 400, so a slot is 0..249.

What this does, per character stem <c> (the 22 moon-3 stems):
  1. collects the voice ids that <c>_3.HA6 uses (EF9 No 1/3, ranges expanded);
  2. groups overlapping/adjacent random ranges into clusters, and gives each cluster a run of
     consecutive free slots (slots that the vanilla SeList does not define), so random ranges
     keep working;
  3. copies the vanilla <c>_SeList.txt byte-for-byte and appends one line per voice
     ("NNN = AC_<stem>_<id> // ActCadenza <CHAR><id>.OGG");
  4. converts each OGG to WAV in the vanilla battle_se voice format (PCM u8, 22050 Hz, mono,
     plain 44-byte RIFF header) at STAGING_DIR/se/battle_se/AC_<stem>_<id>.wav;
  5. rewrites the id ints in DATA_DIR/<c>_3.HA6 in place (file size unchanged).
  Ids with no OGG ("unresolved") still move into a reserved slot that has no SeList entry, so they
  stay silent as in ActCadenza (where the missing file plays nothing) instead of playing an
  unrelated MBAACC voice.

Outputs: STAGING_DIR/data/<c>_SeList.txt, STAGING_DIR/se/battle_se/*.wav,
         STAGING_DIR/<c>_selist_report.txt, STAGING_DIR/<c>_selist_map.json (state).
Idempotent: if <c>_3.HA6 already has the post-rewrite sha256 recorded in the map json, the HA6 is
not rewritten again.
"""
import argparse, collections, glob, hashlib, json, os, re, shutil, struct, subprocess, sys

REPO = os.environ.get("HANTEI_REPO", "/mnt/c/dev/hantei-chan")
sys.path.insert(0, os.path.join(REPO, "docs/tag_research/tools"))
import ha6lib  # noqa: E402

STEMS = ("sion arc ciel akiha hisui kohaku shiki miyako warakia nero v_sion warc akaakiha "
         "m_hisui nanaya satsuki len neco aoko wlen kishima nechaos").split()
DEF_VANILLA = os.path.join(REPO, "gamedata/mbaacc_ref/vanilla_0002")
DEF_OGG = [os.path.join(REPO, "gamedata/mbac_v11_english/06"),   # 1.03A overrides first
           os.path.join(REPO, "gamedata/mbac/04")]
DEF_BATTLE_SE = "/mnt/c/games/mbaacc/se/battle_se"
NSLOTS = 250
WAV_ARGS = ["-ac", "1", "-ar", "22050", "-c:a", "pcm_u8", "-map_metadata", "-1",
            "-fflags", "+bitexact", "-flags:a", "+bitexact"]

# ---------------------------------------------------------------- HA6 voice refs (with offsets)
_refs = []  # (ef_no, [(param_index, file_offset, value)])
_orig_ef = ha6lib._ef

def _ef_rec(d, p):
    e, end = _orig_ef(d, p)
    ty, no, prm = e
    if ty == 9 and no in (1, 3):
        q = p
        while q < end:
            t = d[q:q+4]; q += 4
            if t in (b"EFTP", b"EFNO"): q += 4
            elif t == b"EFPR":
                c = struct.unpack_from("<I", d, q)[0]
                _refs.append((no, [(k, q + 4 + 4*k, prm[k]) for k in range(c)]))
                q += 4 + 4*c
            else: break
    return e, end

ha6lib._ef = _ef_rec

def voice_refs(path):
    """-> list of (no, base_offset_or_None, ids, [(offset, id)])"""
    del _refs[:]
    ha6lib.parse_ha6(path)
    out = []
    for no, prms in _refs:
        if no == 1:
            if not prms: continue
            k0, off0, v0 = prms[0]
            cnt = prms[2][2] if len(prms) > 2 else 1
            cnt = max(cnt, 1)
            out.append((1, [v0 + i for i in range(cnt)], [(off0, v0)]))
        else:
            sl = [(off, v) for (k, off, v) in prms[:5] if v > 0]
            out.append((3, [v for _, v in sl], sl))
    return out

# ---------------------------------------------------------------- SeList
SE_RE = re.compile(rb"^\s*(\d+)\s*=\s*([^\s/]+)")

def selist_entries(raw):
    ent = {}
    for ln in raw.split(b"\n"):
        m = SE_RE.match(ln)
        if m: ent[int(m.group(1))] = m.group(2).decode("cp932", "replace")
    return ent

def ogg_index(dirs):
    idx = {}
    for d in reversed(dirs):  # later dirs first, so earlier (override) dirs win
        for f in glob.glob(os.path.join(d, "*")):
            if f.lower().endswith(".ogg"): idx[os.path.basename(f).upper()] = f
    return idx

def ogg_for(idx, stem, vid):
    return idx.get("%s%02d.OGG" % (stem.upper(), vid))

def to_wav(src, dst):
    if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src) and wav_ok(dst):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", src] + WAV_ARGS + [dst], check=True)

def wav_ok(path):
    try:
        h = open(path, "rb").read(44)
    except OSError:
        return False
    return (h[:4] == b"RIFF" and h[8:16] == b"WAVEfmt " and
            struct.unpack_from("<HHIIHH", h, 20) == (1, 1, 22050, 22050, 1, 8) and h[36:40] == b"data")

def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()

# ---------------------------------------------------------------- allocation
def runs_needed(refs):
    """distinct voice-id runs the HA6 needs: No 1 ranges (a..b) and single ids."""
    rs = set()
    for no, ids, _ in refs:
        if no == 1: rs.add((ids[0], ids[-1]))
        else: rs.update((i, i) for i in ids)
    return rs

def find_run(sv, a, b):
    """start slot of a placed run holding voices a..b in consecutive slots, else None"""
    for s, v in sv.items():
        if v == a and all(sv.get(s + i) == a + i for i in range(b - a + 1)): return s
    return None

def place(sv, taken, a, b):
    n = b - a + 1
    for s in range(NSLOTS - n + 1):
        if all(x not in taken for x in range(s, s + n)):
            for i in range(n): sv[s + i] = a + i; taken.add(s + i)
            return s
    return None

def allocate(rs, used):
    """-> slot_voice {slot: actcadenza_id}.  Overlapping ranges are first tried as one merged run;
    if the free slots are too fragmented, each range gets its own run (a voice may then sit in
    two slots, which is fine: SeList lines may name the same wav)."""
    taken = set(used); sv = {}
    ranges = sorted(r for r in rs if r[1] > r[0])
    clus = []
    for a, b in ranges:
        if clus and a <= clus[-1][1]: clus[-1][1] = max(clus[-1][1], b); clus[-1][2].append((a, b))
        else: clus.append([a, b, [(a, b)]])
    for a, b, members in sorted(clus, key=lambda c: c[0] - c[1]):  # longest first
        if find_run(sv, a, b) is None and place(sv, taken, a, b) is None:
            for ma, mb in sorted(members, key=lambda r: r[0] - r[1]):
                if find_run(sv, ma, mb) is None and place(sv, taken, ma, mb) is None:
                    raise SystemExit("no run of %d free slots for ids %d..%d" % (mb - ma + 1, ma, mb))
    for a, b in sorted(r for r in rs if r[1] == r[0]):
        if a not in sv.values() and place(sv, taken, a, a) is None:
            raise SystemExit("SeList full (id %d)" % a)
    return sv

# ---------------------------------------------------------------- per character
def do_char(stem, a, idx):
    ha6 = os.path.join(a.data_dir, "%s_3.HA6" % stem)
    van = os.path.join(a.vanilla, "%s_SeList.txt" % stem)
    if not os.path.exists(ha6) or not os.path.exists(van):
        return None, "missing %s" % (ha6 if not os.path.exists(ha6) else van)
    st = a.staging
    mapf = os.path.join(st, "%s_selist_map.json" % stem)
    prev = json.load(open(mapf)) if os.path.exists(mapf) else None
    cur = sha(ha6)
    already = prev is not None and prev.get("ha6_sha_after") == cur
    raw = open(van, "rb").read()
    used = selist_entries(raw)
    refs = voice_refs(ha6)
    if already:
        sv = {int(k): v for k, v in prev["slot_voice"].items()}
    else:
        sv = allocate(runs_needed(refs), used)
    ids = sorted(set(sv.values()))
    lines, resolved, unresolved, wavs = [], collections.defaultdict(list), [], {}
    for slot in sorted(sv):
        vid = sv[slot]
        src = ogg_for(idx, stem, vid)
        if src is None:
            if vid not in unresolved: unresolved.append(vid)
            resolved[vid].append(slot)
            continue
        name = "AC_%s_%03d" % (stem, vid)
        if vid not in wavs:
            to_wav(src, os.path.join(st, "se", "battle_se", name + ".wav")); wavs[vid] = (name, src)
        lines.append("%03d = %s // ActCadenza %s" % (slot, name, os.path.basename(src)))
        resolved[vid].append(slot)
    out = raw
    nl = b"\r\n" if b"\r\n" in raw else b"\n"
    if out and not out.endswith(b"\n"): out += nl
    out += nl.join(l.encode("cp932") for l in
                   ["// ---- ActCadenza moon-3 voices (actcadenza_selist.py) ----"] + lines) + nl
    os.makedirs(os.path.join(st, "data"), exist_ok=True)
    open(os.path.join(st, "data", "%s_SeList.txt" % stem), "wb").write(out)
    nrw = 0
    if not already:
        d = bytearray(open(ha6, "rb").read()); n0 = len(d); old_at = {}
        for no, vids, slots in refs:
            if no == 1:
                (off, v), = slots; old_at[off] = v
                struct.pack_into("<i", d, off, find_run(sv, vids[0], vids[-1])); nrw += 1
            else:
                for off, v in slots:
                    old_at[off] = v
                    struct.pack_into("<i", d, off, find_run(sv, v, v)); nrw += 1
        assert len(d) == n0
        open(ha6, "wb").write(d)
        json.dump({"stem": stem, "ha6_sha_before": cur, "ha6_sha_after": sha(ha6),
                   "slot_voice": {str(k): v for k, v in sorted(sv.items())},
                   "unresolved": unresolved,
                   "refs": {str(k): v for k, v in sorted(old_at.items())}}, open(mapf, "w"), indent=1)
    with open(os.path.join(st, "%s_selist_report.txt" % stem), "w") as w:
        w.write("# %s: ActCadenza voice ids -> MBAACC %s_SeList.txt slots\n" % (stem, stem))
        w.write("# vanilla entries %d, free %d, voice ids used %d, mapped %d, unresolved %d, "
                "slots used %d, refs rewritten %d%s\n" % (len(used), NSLOTS - len(used), len(ids),
                len(ids) - len(unresolved), len(unresolved), len(sv), nrw,
                " (HA6 already rewritten, skipped)" if already else ""))
        w.write("old_id\tslots\twav\togg\n")
        for vid in ids:
            sl = ",".join("%03d" % x for x in resolved[vid])
            if vid in wavs:
                w.write("%d\t%s\t%s.wav\t%s\n" % (vid, sl, wavs[vid][0], os.path.basename(wavs[vid][1])))
        w.write("# unresolved (no %s%%02d.OGG; slot reserved with no entry, so silent as in ActCadenza):\n"
                % stem.upper())
        for vid in unresolved:
            w.write("%d\t%s\t-\t%s%02d.OGG\n" % (vid, ",".join("%03d" % x for x in resolved[vid]),
                    stem.upper(), vid))
    return dict(stem=stem, used=len(ids), mapped=len(ids) - len(unresolved), unresolved=unresolved,
                free=NSLOTS - len(used), slots=len(sv), rewritten=nrw, skipped=already), None

# ---------------------------------------------------------------- selftest
def selftest(stem, a):
    ha6 = os.path.join(a.data_dir, "%s_3.HA6" % stem)
    sel = os.path.join(a.staging, "data", "%s_SeList.txt" % stem)
    van = os.path.join(a.vanilla, "%s_SeList.txt" % stem)
    mapf = os.path.join(a.staging, "%s_selist_map.json" % stem)
    errs = []
    if not (os.path.exists(ha6) and os.path.exists(sel) and os.path.exists(mapf)):
        return ["missing outputs"]
    st = json.load(open(mapf)); unres = set(st["unresolved"])
    sv = {int(k): v for k, v in st["slot_voice"].items()}
    unres_slots = {k for k, v in sv.items() if v in unres}
    if sha(ha6) != st["ha6_sha_after"]: errs.append("HA6 sha differs from the rewrite")
    vraw = open(van, "rb").read(); sraw = open(sel, "rb").read()
    if not sraw.startswith(vraw): errs.append("vanilla SeList bytes not kept as prefix")
    vent = selist_entries(vraw); ent = selist_entries(sraw)
    new = {k: v for k, v in ent.items() if k not in vent}
    if any(vent[k] != ent[k] for k in vent): errs.append("vanilla entry changed")
    if len(new) + len(vent) != len(ent): errs.append("appended entry collides with vanilla slot")
    for k, n in new.items():
        if sv.get(k) is None or n != "AC_%s_%03d" % (stem, sv[k]):
            errs.append("slot %d: entry %s does not match the slot table" % (k, n))
        if not wav_ok(os.path.join(a.staging, "se", "battle_se", n + ".wav")):
            errs.append("slot %d: wav %s missing/bad format" % (k, n))
    old_at = {int(k): v for k, v in st["refs"].items()}
    nref = 0
    for no, ids, slots in voice_refs(ha6):
        for i in ids:
            if i in unres_slots: continue
            if i not in new: errs.append("EF9 No%d id %d is not an appended ActCadenza entry" % (no, i))
        # the slot run must hold the ActCadenza voices the ref named before the rewrite
        if no == 1:
            (off, v), = slots
            olds = [old_at.get(off, -1) + k for k in range(len(ids))]
            pairs = zip(ids, olds)
        else:
            pairs = [(v, old_at.get(off, -1)) for off, v in slots]
        for slot, old in pairs:
            nref += 1
            if sv.get(slot) != old: errs.append("slot %d holds id %s, ref wants %d" % (slot, sv.get(slot), old))
    if nref == 0 and old_at: errs.append("no voice refs found after rewrite")
    return errs

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("data_dir"); ap.add_argument("staging")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--vanilla", default=DEF_VANILLA)
    ap.add_argument("--ogg-dir", action="append", default=None)
    ap.add_argument("--chars", default=",".join(STEMS))
    a = ap.parse_args()
    if shutil.which("ffmpeg") is None: raise SystemExit("ffmpeg not found")
    a.data_dir = os.path.abspath(a.data_dir); a.staging = os.path.abspath(a.staging)
    os.makedirs(a.staging, exist_ok=True)
    idx = ogg_index(a.ogg_dir or DEF_OGG)
    tot = collections.Counter(); bad = 0
    for stem in a.chars.split(","):
        r, err = do_char(stem, a, idx)
        if err: print("%-9s SKIP %s" % (stem, err)); bad += 1; continue
        tot.update(used=r["used"], mapped=r["mapped"], unresolved=len(r["unresolved"]),
                   rewritten=r["rewritten"])
        print("%-9s ids %3d mapped %3d unresolved %2d free %3d slots %3d rewritten %4d%s %s" % (
            stem, r["used"], r["mapped"], len(r["unresolved"]), r["free"], r["slots"], r["rewritten"],
            " (already)" if r["skipped"] else "", r["unresolved"] or ""))
        if a.selftest:
            e = selftest(stem, a)
            if e: bad += 1; print("  SELFTEST FAIL:", *e[:10], sep="\n    ")
    print("total ids %(used)d mapped %(mapped)d unresolved %(unresolved)d refs rewritten %(rewritten)d"
          % tot)
    if a.selftest: print("selftest:", "FAIL" if bad else "PASS")
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
