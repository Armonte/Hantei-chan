#!/usr/bin/env python3
"""List string literals in the given source files that look like UI text but have no row in src/i18n_ja_*.inc
(for code that reaches ImGui through helpers / std::string, which tools/i18n_check.py cannot see).
  python3 tools/i18n_literals.py src/cmdfile/cmd_editor_ui.cpp ..."""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n_check as C
rows, _ = C.load_table()
for f in sys.argv[1:]:
    for n, line in enumerate(open(f, encoding='utf-8'), 1):
        if re.match(r'\s*(//|#include)', line): continue
        for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', line):
            s = C.unescape(m.group(1)).split('##')[0]
            if len(s) < 3 or not re.search(r'[A-Za-z]{3}', s): continue
            if s in rows or s.startswith(('%', '#', '\\')) or re.fullmatch(r'[\w./\\:%-]+', s) and ' ' not in s and not s[0].isupper(): continue
            print("%s:%d  %r" % (f, n, s))
