#!/usr/bin/env python3
"""Unit tests for diff.py on synthetic PNGs (PLAN.md 4.4, ctest
render_parity_tools). No GPU, no game data: every PNG is generated here.

    python3 tools/render_parity/test_diff.py

Covered: identical, one-pixel diff, re-encoded identical pixels, size mismatch,
missing case, an allowlist entry inside/outside its bounds, region=edges, and
the allowlist guard that rejects a glob covering a whole area.
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


def make_run(root, name, cases, round_no=1):
    """cases: {case: ndarray|None}. None = manifest row whose PNG is missing."""
    os.makedirs(os.path.join(root, "shots"), exist_ok=True)
    rows = []
    for case, px in cases.items():
        if px is None:
            rows.append((case, "shots/%s.png" % case, "0" * 64, 0, 0, 1))
            continue
        h, w = px.shape[:2]
        rel = "shots/%s.png" % case
        path = os.path.join(root, rel)
        Image.fromarray(px.astype(np.uint8), "RGBA").save(path)
        import hashlib
        sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
        rows.append((case, rel, sha, w, h, 0))
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
        rc, _ = run_diff(g9, r9)
        check(rc == 0, "a re-encoded PNG with the same pixels passes")

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
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if FAILS:
        print("%d FAILED" % len(FAILS))
        return 1
    print("all diff.py tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
