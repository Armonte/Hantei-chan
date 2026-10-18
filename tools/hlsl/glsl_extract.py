#!/usr/bin/env python3
"""Extract the inline GLSL programs from the C++ sources.

The single source of truth for the renderer's GLSL today is the code: three
translation units hold the programs as C++ raw string literals.  P1 moves them
to res/shaders/ with identical text, so this module both writes those files
(--write) and serves as the reference for tests/hlsl_glsl_identity_test.py.

Usage:
    python3 tools/hlsl/glsl_extract.py --write     # refresh res/shaders/*.glsl.*
    python3 tools/hlsl/glsl_extract.py --list      # variable -> file table
"""

import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# variable name -> (program name, stage) for every inline GLSL string that is a
# real program stage.  Anything not listed here is an error (see check_covered).
EXTRACTIONS = {
    "src/render.cpp": {
        "simpleSrcVert": ("simple", "vert"),
        "simpleSrcFrag": ("simple", "frag"),
        "texturedSrcVert": ("textured", "vert"),
        "texturedSrcFrag": ("textured", "frag"),
        "partsSrcVert": ("parts", "vert"),
        "partsSrcFrag": ("parts", "frag"),
    },
    "src/background/bg_renderer.cpp": {
        "kVS": ("bg_object", "vert"),
        "kFS": ("bg_object", "frag"),
        "kBlurVS": ("bg_blur", "vert"),
        "kBlurFS": ("bg_blur", "frag"),
        "kHeatVS": ("bg_heat", "vert"),
        "kHeatFS_A": ("bg_heat_a", "frag"),
        "kHeatFS_B": ("bg_heat_b", "frag"),
    },
    "src/stage3d/s3d_view.cpp": {
        "kVs": ("s3d", "vert"),
        "kFs": ("s3d", "frag"),
        "kGradeVs": ("s3d_grade", "vert"),
        "kGradeFs": ("s3d_grade", "frag"),
    },
}

# const char* name = R"DELIM( ... )DELIM";
RAW_STRING_RE = re.compile(
    r'(?:static\s+)?const\s+char\s*\*\s*(?P<var>[A-Za-z_]\w*)\s*=\s*'
    r'R"(?P<delim>[A-Za-z_]*)\((?P<body>.*?)\)(?P=delim)"',
    re.DOTALL,
)


class ExtractError(Exception):
    pass


def shader_file(program, stage):
    return "res/shaders/%s.glsl.%s" % (program, stage)


def parse_source(rel_path, text):
    """Every raw string literal in rel_path, as {var: body}."""
    out = {}
    if 'R"' not in text:
        raise ExtractError("%s: no raw string literals found" % rel_path)
    for m in RAW_STRING_RE.finditer(text):
        var = m.group("var")
        if var in out:
            raise ExtractError("%s: duplicate shader variable %s" % (rel_path, var))
        out[var] = m.group("body")
    return out


def extract(read=None):
    """Return {res/shaders file path: exact bytes as str} for every program."""
    opened = []
    if read is None:
        def read(path):
            with open(path, "r", encoding="utf-8", newline="") as f:
                return f.read()
    try:
        result = {}
        for rel_path, mapping in EXTRACTIONS.items():
            text = read(os.path.join(REPO, rel_path))
            found = parse_source(rel_path, text)
            for var in found:
                if var not in mapping:
                    raise ExtractError(
                        "%s: raw string %s is not in the EXTRACTIONS table" % (rel_path, var)
                    )
            for var, (program, stage) in mapping.items():
                if var not in found:
                    raise ExtractError("%s: %s not found" % (rel_path, var))
                target = shader_file(program, stage)
                if target in result:
                    raise ExtractError("two variables map to %s" % target)
                result[target] = found[var]
        return result
    finally:
        opened.clear()


def write_files(read=None):
    files = extract(read=read)
    for rel, body in sorted(files.items()):
        path = os.path.join(REPO, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(body)
        print("wrote %s (%d bytes)" % (rel, len(body)))
    return files


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true", help="write res/shaders/*.glsl.*")
    ap.add_argument("--list", action="store_true", help="print the variable table")
    args = ap.parse_args(argv)
    if args.list:
        for rel_path, mapping in EXTRACTIONS.items():
            for var, (program, stage) in mapping.items():
                print("%-40s %-16s %s" % (rel_path, var, shader_file(program, stage)))
    if args.write:
        write_files()
    if not (args.list or args.write):
        ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
