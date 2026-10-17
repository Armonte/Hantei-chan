#!/usr/bin/env python3
"""Wrap bare ImGui literals with LBL()/TXT() (see src/i18n.h).  python3 tools/i18n_autowrap.py [--dry] files...
Label widgets -> LBL, text widgets -> TXT.  Popup ids / DockBuilder / child / table ids are left alone.
BeginPopupModal titles are wrapped and the matching OpenPopup("same") literals in the same file follow."""
import re, sys, os
LIT = r'"(?:[^"\\]|\\.)*"'
SEQ = r'(?:' + LIT + r'(?:\s*' + LIT + r')*)'
LBLF = set("""Begin Button SmallButton MenuItem BeginMenu TreeNode TreeNodeEx CollapsingHeader Selectable BeginCombo Combo Checkbox
CheckboxFlags RadioButton InputText InputTextMultiline InputTextWithHint InputInt InputInt2 InputInt3 InputInt4 InputFloat InputFloat2
InputFloat3 InputScalar DragInt DragInt2 DragInt3 DragInt4 DragFloat DragFloat2 DragFloat3 DragFloat4 DragScalar SliderInt SliderInt2
SliderFloat SliderFloat2 SliderAngle ColorEdit3 ColorEdit4 ColorPicker3 BeginTabItem BeginPopupModal TableSetupColumn ListBox BeginListBox
VSliderInt VSliderFloat TabItemButton""".split())
TXTF = set("Text TextDisabled TextWrapped BulletText SetTooltip SetItemTooltip SeparatorText TextUnformatted".split())
SKIP_LITS = {"Dock Window"}
def lit_text(seq): return ''.join(re.findall(LIT, seq)) 
def run(path, dry):
    src = open(path, encoding='utf-8', newline='').read()
    modal = set()
    def sub(m):
        fn, seq = m.group(2), m.group(3)
        vis = ''.join(x[1:-1] for x in re.findall(LIT, seq)).split('##')[0]
        words = re.sub(r'%[-+# 0]*\d*(?:\.\d+)?(?:ll|l|z|h)*[a-zA-Z%]', '', vis)
        if not re.search(r'[A-Za-z]{3}', words) or vis in SKIP_LITS: return m.group(0)
        if fn in LBLF:
            if '%' in vis and fn != 'Begin': return m.group(0)
            if fn == 'BeginPopupModal': modal.add(seq)
            return m.group(1) + fn + '(LBL(' + seq + ')'
        if fn in TXTF: return m.group(1) + fn + '(TXT(' + seq + ')'
        return m.group(0)
    pat = re.compile(r'(\b(?:ImGui|im)::)(\w+)\(\s*(' + SEQ + r')')
    def sub2(m):
        ls = m.string.rfind('\n', 0, m.start()) + 1
        if m.string[ls:m.start()].lstrip().startswith('//'): return m.group(0)
        return sub(m)
    s = pat.sub(sub2, src)
    # TextColored(color, "text", ...) and ImSearch::SearchBar("hint")
    def sub3(m):
        vis = ''.join(x[1:-1] for x in re.findall(LIT, m.group(3)))
        if not re.search(r'[A-Za-z]{3}', vis) or vis.startswith('%'): return m.group(0)
        return m.group(1) + m.group(2) + 'TXT(' + m.group(3) + ')'
    s = re.sub(r'(\b(?:ImGui|im)::TextColored\()((?:[^"(),]|\((?:[^()]|\([^()]*\))*\))+,\s*)(' + SEQ + r')', sub3, s)
    s = re.sub(r'(\bImSearch::SearchBar\()()(' + SEQ + r')', sub3, s)
    for seq in modal:
        s = re.sub(r'(\b(?:ImGui|im)::OpenPopup\(\s*)(' + re.escape(seq) + r')', r'\1LBL(\2)', s)
    if s != src:
        if '#include "i18n.h"' not in s and '#include "../i18n.h"' not in s and 'i18n.h' not in s:
            # add include after the last leading #include
            lines = s.split('\n'); idx = 0
            for i, l in enumerate(lines):
                if l.startswith('#include'): idx = i
            rel = os.path.relpath('src/i18n.h', os.path.dirname(path)).replace('\\', '/')
            lines.insert(idx + 1, '#include "%s"' % rel); s = '\n'.join(lines)
        if not dry: open(path, 'w', encoding='utf-8', newline='').write(s)
        print('wrapped', path)
if __name__ == '__main__':
    dry = '--dry' in sys.argv
    for f in [a for a in sys.argv[1:] if not a.startswith('--')]: run(f, dry)
