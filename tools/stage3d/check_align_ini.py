#!/usr/bin/env python3
"""check_align_ini.py <stage3d.ini> key=expected ...: parse a Hantei-chan MBAACC-alignment stage3d.ini with the rules of
PovertyCaster's pc-stage3d applyStageIni (pc-stage3d/src/StageIni.cpp): ';'/'#' comments, 'key = value', known keys only,
strict comma-separated decimals. Then compare keys with expected values (tolerance 0.01). Exit 1 on any problem."""
import sys

KNOWN = set("""replaces package scale pos fit_mode center_x pan_boost pan_depth stage_w host_world_w yaw cam_pitch ground_auto ground_y
fighter_px uni_char_px ref_fov debug_overlay depth_offset cam_height negate_z mirror_x flip_v fov world_scale units_to_px floor_px
horizon_px cam_offset lift gain contrast gray vcol_gain fog fog_start fog_end fog_color torch torch_color torch_pos torch_falloff
front_pass front_z""".split())
VEC = {"pos": 3, "cam_offset": 3, "fog_color": 4, "torch_color": 3, "torch_pos": 3}

def floats(v, n):
    parts = [p.strip() for p in v.replace(",", " ").split()]
    if len(parts) != n: raise ValueError(v)
    return [float(p) for p in parts]

def main():
    path, exp = sys.argv[1], sys.argv[2:]
    kv, bad = {}, []
    for raw in open(path, encoding="utf-8").read().split("\n"):
        line = raw
        for c in ";#":
            i = line.find(c)
            if i >= 0: line = line[:i]
        line = line.strip()
        if not line: continue
        if "=" not in line: bad.append("no '=': " + line); continue
        k, v = (x.strip() for x in line.split("=", 1))
        if k not in KNOWN: bad.append("unknown key " + k); continue
        try:
            if k == "fit_mode":
                if v not in ("crop", "scale", "parallax"): raise ValueError(v)
            elif k != "package": floats(v, VEC.get(k, 1))
        except ValueError: bad.append("bad value %s = %s" % (k, v)); continue
        kv[k] = v
    for e in exp:
        k, want = e.split("=", 1)
        got = kv.get(k)
        if got is None: bad.append("missing " + k); continue
        try: ok = abs(float(got.split(",")[0]) - float(want)) <= 0.01
        except ValueError: ok = got == want
        print("  %-12s %-12s expected %-10s %s" % (k, got, want, "ok" if ok else "MISMATCH"))
        if not ok: bad.append("mismatch " + k)
    for b in bad: print("  FAIL", b)
    print("%s: %s (%d keys)" % (path, "ok" if not bad else "FAILED", len(kv)))
    return 1 if bad else 0

sys.exit(main())
