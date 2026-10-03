"""Value statistics for GOF2 pattern-area sections 3..8 (attack records, small tables, script lists, effect-spawn records).
Usage: python3 at_sections_scan.py [sec] [--all]
Default: EFFECTIVE set = every .DT2 name once, the highest archive index wins (the engine searches data05 -> data00, FileIo_OpenLooseThenPacSlots).
--all : every archive copy (data02 + data05 duplicates counted twice)."""
import sys, struct, collections
sys.path.insert(0, '/home/teo/dev/hantei-chan-wt/rbo-support/tools/han2')
from han2lib import han2_files, split
args = [a for a in sys.argv[1:] if not a.startswith('--')]
SEC = int(args[0]) if args else 3
ALL = '--all' in sys.argv
STR = {3: 236, 4: 28, 5: 20, 6: 20, 7: 20, 8: 96}[SEC]

def effective():
    files = {}
    for arc, name, b in han2_files('gof'):
        if not name.upper().endswith('.DT2'): continue
        if ALL: files[(arc, name)] = b
        else: files[name.upper()] = b          # archives are yielded data00..data05, later wins
    return files

if __name__ == '__main__':
    cnt = collections.defaultdict(collections.Counter)
    n = 0; nf = 0
    for key, b in sorted(effective().items()):
        d = split(b); s = d['secs'][SEC]
        if not s: continue
        nf += 1
        assert len(s) % STR == 0, (key, len(s))
        for i in range(0, len(s), STR):
            if SEC in (6, 7, 8) and i == 0: continue   # dummy record 0
            n += 1
            for o in range(0, STR, 4):
                cnt[o][struct.unpack_from('<I', s, i + o)[0]] += 1
    print('files with section', nf, 'records', n, '(all copies)' if ALL else '(effective set)')
    for o in sorted(cnt):
        c = cnt[o]
        top = ', '.join('%s:%d' % (hex(k) if k > 9 else k, v) for k, v in c.most_common(8))
        print('+0x%03X distinct=%d  %s' % (o, len(c), top))
