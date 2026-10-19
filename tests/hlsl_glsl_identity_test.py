#!/usr/bin/env python3
"""ctest glsl_identity: res/shaders/*.glsl.* must be byte-identical to the
inline GLSL in the C++ sources.  P1 loads these files instead of the strings,
so any drift here would silently change the renderer.
"""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools", "hlsl"))

import glsl_extract  # noqa: E402


def main():
    try:
        expected = glsl_extract.extract()
    except glsl_extract.ExtractError as e:
        print("FAIL: could not extract the inline GLSL: %s" % e)
        return 1

    failures = 0
    on_disk = set()
    shaders_dir = os.path.join(REPO, "res", "shaders")
    if os.path.isdir(shaders_dir):
        for name in os.listdir(shaders_dir):
            if ".glsl." in name:
                on_disk.add("res/shaders/" + name)

    for rel, body in sorted(expected.items()):
        path = os.path.join(REPO, rel)
        try:
            with open(path, "r", encoding="utf-8", newline="") as f:
                actual = f.read()
        except OSError as e:
            print("FAIL: %s missing (%s)" % (rel, e))
            failures += 1
            continue
        if actual != body:
            # Pinpoint the first difference instead of dumping both blobs.
            line = 1
            for i, (a, b) in enumerate(zip(actual, body)):
                if a != b:
                    break
                if a == "\n":
                    line += 1
            print("FAIL: %s differs from the inline string at line %d "
                  "(%d bytes on disk, %d in the source)"
                  % (rel, line, len(actual.encode("utf-8")), len(body.encode("utf-8"))))
            failures += 1

    for extra in sorted(on_disk - set(expected)):
        print("FAIL: %s has no matching inline string (stale copy?)" % extra)
        failures += 1

    if failures:
        print("%d file(s) differ; run tools/hlsl/glsl_extract.py --write" % failures)
        return 1
    print("%d GLSL files are byte-identical to the inline strings." % len(expected))
    return 0


if __name__ == "__main__":
    sys.exit(main())
