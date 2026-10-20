#!/usr/bin/env python3
"""Unit tests for diff.py on synthetic PNGs (PLAN.md 4.4, ctest
render_parity_tools). No GPU, no game data: every PNG is generated here.

    python3 tools/render_parity/test_diff.py

Covered: identical, one-pixel diff, re-encoded identical pixels (a FAIL: PASS
is byte-identical only), size mismatch, missing case, an allowlist entry
inside/outside its bounds, region=edges, the allowlist guard that rejects a glob
covering a whole area, a non-zero / timed-out / unknown process exit on either
side, and non-PNG export outputs compared by sha with only the wall-clock
durations masked.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DIFF = os.path.join(HERE, "diff.py")

FAILS = []


def check(cond, what):
    if cond:
        print("ok   " + what)
    else:
        print("FAIL " + what)
        FAILS.append(what)


def make_run(root, name, cases, round_no=1, exits=None):
    """cases: {case: ndarray|bytes|None}. None = manifest row whose PNG is
    missing; bytes = a non-PNG output written verbatim under that case name.
    exits: {case: exit code} for the manifest's exit column (default 0)."""
    exits = exits or {}
    os.makedirs(os.path.join(root, "shots"), exist_ok=True)
    rows = []
    for case, px in cases.items():
        if px is None:
            rows.append((case, "shots/%s.png" % case, "0" * 64, 0, 0, 1))
            continue
        if isinstance(px, bytes):
            import hashlib
            rel = "shots/%s" % case
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "wb").write(px)
            rows.append((case, rel, hashlib.sha256(px).hexdigest(), 0, 0, exits.get(case, 0)))
            continue
        h, w = px.shape[:2]
        rel = "shots/%s.png" % case
        path = os.path.join(root, rel)
        Image.fromarray(px.astype(np.uint8), "RGBA").save(path)
        import hashlib
        sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
        rows.append((case, rel, sha, w, h, exits.get(case, 0)))
    with open(os.path.join(root, "manifest.tsv"), "w") as f:
        f.write("case\tpng\tsha256\tw\th\texit\tround\n")
        for case, rel, sha, w, h, rc in rows:
            f.write("%s\t%s\t%s\t%d\t%d\t%d\t%s\n" % (case, rel, sha, w, h, rc, round_no))


def run_diff(golden, run, *extra):
    p = subprocess.run([sys.executable, DIFF, golden, run, "--quiet"] + list(extra),
                       capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def base(w=8, h=6, value=(10, 20, 30, 255)):
    a = np.zeros((h, w, 4), np.uint8)
    a[...] = value
    return a


def main():
    tmp = tempfile.mkdtemp(prefix="rp_test_diff_")
    try:
        g = os.path.join(tmp, "golden")
        r = os.path.join(tmp, "run")

        # 1. identical
        same = base()
        make_run(g, "g", {"case_same": same})
        make_run(r, "r", {"case_same": same.copy()})
        rc, _ = run_diff(g, r)
        check(rc == 0, "identical run passes (exit 0)")

        # 2. one-pixel diff -> FAIL without an allowlist
        diff = same.copy()
        diff[2, 3] = (250, 0, 0, 255)
        make_run(r, "r", {"case_same": diff})
        rc, out = run_diff(g, r)
        check(rc == 1, "one-pixel diff fails without an allowlist")

        # 3. the same diff with a generous allowlist entry -> ALLOWED
        allow = os.path.join(tmp, "allow.tsv")
        with open(allow, "w") as f:
            f.write("case_*\t*\tAA\t255\t4\t\tevidence\tnote\n")
        rc, _ = run_diff(g, r, "--allow", allow)
        check(rc == 0, "allowlist within bounds allows the case")

        # 4. allowlist bound exceeded -> FAIL
        with open(allow, "w") as f:
            f.write("case_*\t*\tAA\t255\t0\t\tevidence\tnote\n")
        rc, _ = run_diff(g, r, "--allow", allow)
        check(rc == 1, "allowlist over max_pixels fails")

        # 5. region=edges: on a golden with a real geometry edge (left half
        #    opaque, right half transparent), a differing pixel on that boundary
        #    is allowed and one well inside a flat region is rejected.
        solid = np.zeros((6, 8, 4), np.uint8)
        solid[:, :4] = (200, 100, 50, 255)
        make_run(g, "g", {"case_edge": solid})
        with open(allow, "w") as f:
            f.write("case_*\t*\tAA\t255\t4\tedges\tevidence\tnote\n")
        on_edge = solid.copy()
        on_edge[2, 4] = (250, 0, 0, 255)       # the column where alpha changes
        make_run(r, "r", {"case_edge": on_edge})
        rc, _ = run_diff(g, r, "--allow", allow)
        check(rc == 0, "region=edges allows an edge pixel")
        interior = solid.copy()
        interior[3, 1] = (250, 0, 0, 255)      # flat interior
        make_run(r, "r", {"case_edge": interior})
        rc, _ = run_diff(g, r, "--allow", allow)
        check(rc == 1, "region=edges rejects an interior pixel")

        # 6. size mismatch -> FAIL
        make_run(g, "g", {"case_same": same})
        make_run(r, "r", {"case_same": base(9, 6)})
        rc, _ = run_diff(g, r)
        check(rc == 1, "a size change fails")

        # 7. missing on the run side -> FAIL
        make_run(g, "g", {"case_same": same})
        make_run(r, "r", {})
        rc, _ = run_diff(g, r)
        check(rc == 1, "a case missing in the run fails")

        # 8. missing on the golden side -> FAIL
        make_run(g, "g", {})
        make_run(r, "r", {"case_same": same})
        rc, _ = run_diff(g, r)
        check(rc == 1, "a case missing in the golden fails")

        # 9. a re-encode of the same pixels passes (PNG bytes differ, pixels match)
        g9 = os.path.join(tmp, "g9"); r9 = os.path.join(tmp, "r9")
        make_run(g9, "g", {"case_re": same})
        make_run(r9, "r", {"case_re": same.copy()})
        p = os.path.join(r9, "shots", "case_re.png")
        Image.fromarray(same, "RGBA").save(p, optimize=True, compress_level=1)
        import hashlib
        sha = hashlib.sha256(open(p, "rb").read()).hexdigest()
        txt = open(os.path.join(r9, "manifest.tsv")).read().splitlines()
        txt[1] = "\t".join(txt[1].split("\t")[:2] + [sha] + txt[1].split("\t")[3:])
        open(os.path.join(r9, "manifest.tsv"), "w").write("\n".join(txt) + "\n")
        rc, out = run_diff(g9, r9, "--report", os.path.join(tmp, "r9.md"))
        check(rc == 1, "a re-encoded PNG with the same pixels fails (PASS is byte-identical only)")
        check("re-encoded" in open(os.path.join(tmp, "r9.md")).read(), "the re-encode is named in the report")

        # 10. the allowlist guard rejects a glob covering a whole area
        with open(allow, "w") as f:
            f.write("uni2_*\t*\tEMU\t255\t10\t\tevidence\tnote\n")
        rc, out = run_diff(g, r, "--allow", allow)
        check("whole area" in out, "a whole-area allowlist glob is rejected")

        # 11. a backend-specific entry only applies to its backend
        make_run(g, "g", {"case_same": same})
        make_run(r, "r", {"case_same": diff})
        with open(allow, "w") as f:
            f.write("case_*\td3d9\tFLT\t255\t4\t\tevidence\tnote\n")
        rc, _ = run_diff(g, r, "--allow", allow, "--backend", "gl")
        check(rc == 1, "a d3d9 allowlist entry does not apply to the gl backend")
        rc, _ = run_diff(g, r, "--allow", allow, "--backend", "d3d9")
        check(rc == 0, "a d3d9 allowlist entry applies to the d3d9 backend")

        # 12. diff.py golden golden -> 100% PASS
        rc, _ = run_diff(g9, g9)
        check(rc == 0, "diff.py <golden> <golden> is 100% PASS")

        # 13. a non-zero process exit fails the case even with identical bytes,
        #     on either side; 124 is the runner's timeout, -1 an unclaimed file
        for side, code in (("run", 1), ("run", 124), ("golden", 3), ("run", -1)):
            make_run(g, "g", {"case_same": same}, exits={"case_same": code} if side == "golden" else None)
            make_run(r, "r", {"case_same": same.copy()}, exits={"case_same": code} if side == "run" else None)
            rc, _ = run_diff(g, r)
            check(rc == 1, "exit %d on the %s side fails an identical case" % (code, side))

        # 14. non-PNG export outputs: equal except the wall-clock durations ->
        #     PASS; any other byte differing -> FAIL
        js = b'{\n  "pattern": 10,\n  "timing_ms": {\n    "analysis": %s,\n    "png_encode": 2.5\n  },\n  "tool": "x"\n}\n'
        res = b'OK\nExported 1 PNG (0 skipped) in %s ms.\n'
        make_run(g, "g", {"e/a.json": js % b"5.576", "e/export_result.txt": res % b"40"})
        make_run(r, "r", {"e/a.json": js % b"22.336", "e/export_result.txt": res % b"57"})
        rc, out = run_diff(g, r)
        check(rc == 0, "non-PNG outputs that differ only in wall-clock durations pass")
        make_run(r, "r", {"e/a.json": (js % b"5.576").replace(b'"pattern": 10', b'"pattern": 11'),
                          "e/export_result.txt": res % b"40"})
        rc, _ = run_diff(g, r)
        check(rc == 1, "a non-PNG output with a changed field fails")
        make_run(r, "r", {"e/a.json": js % b"5.576",
                          "e/export_result.txt": (res % b"40").replace(b"1 PNG", b"2 PNG")})
        rc, _ = run_diff(g, r)
        check(rc == 1, "a non-PNG output with a changed count fails")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if FAILS:
        print("%d FAILED" % len(FAILS))
        return 1
    print("all diff.py tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
