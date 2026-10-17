#!/usr/bin/env python3
"""Minimal HA6 tag walker that mirrors the UNI2/MBTL loader (han6files.cpp in the
game exes; see docs/HANTEI_UNI_MBTL.md). Yields (path, tag, payload_words) events.
Payload sizes are the ones the game reads; variable-length tags are handled."""
import struct, sys

FIXED = {
 # pattern level
 'PSTS':1,'PLVL':1,'PFLG':1,'PUPS':1,'PDST':1,
 # frame level
 'FSNH':1,'FSNA':1,'FSNE':1,'FSNI':1,'HRNM':5,'HRAT':5,'HRNS':2,'HRAS':2,'HRFF':2,'ASSM':1,
 'EFST':1,'IFST':1,
 # AF
 'AFGP':2,'AFGX':3,'AFOF':2,'AFDL':1,'AFFL':1,'AFFE':1,'AFAL':2,'AFRG':3,'AFAX':1,'AFAY':1,'AFAZ':1,
 'AFAN':1,'AFZM':2,'AFJP':1,'AFHK':1,'AFPR':1,'AFCT':1,'AFLP':1,'AFJC':1,'AFTN':2,'AFPL':1,'AFRT':1,
 'AFID':1,'AFPA':1,'AFJH':1,
 # AS
 'ASV0':5,'ASV1':5,'ASVA':2,'ASVC':2,'ASMV':1,'ASMX':1,'ASCN':1,'ASCS':1,'ASCT':1,'ASCF':1,'ASAA':1,
 'ASYS':1,'ASF0':1,'ASF1':1,'ASF2':1,'ASF3':1,'AST0':7,'ASAT':1,'ASKV':1,'ASSS':1,'ASDF':1,'ASCL':2,
 'ASSE':2,'ASDE':1,
 # AT
 'ATGD':1,'ATHS':1,'ATVV':2,'ATHT':1,'ATF1':1,'ATF2':1,'ATHE':2,'ATKK':1,'ATNG':1,'ATUH':1,'ATBT':1,
 'ATSN':1,'ATSU':1,'ATSP':1,'ATGN':1,'ATAT':1,'ATHH':1,'ATAM':1,'ATCA':1,'ATC0':3,'ATSA':1,'ATSH':1,
 'ATRF':1,'ATBC':1,'ATVD':1,'ATAB':1,'ATBG':1,'ATGE':2,'ATKZ':1,'ATGS':1,
 # EF/IF
 'EFTP':1,'EFNO':1,'IFTP':1,
}
ZERO = {'PEND','FSTR','FEND','AFST','AFED','ASST','ASED','ATST','ATED','EFED','IFED','ASVX','ASS1','ASS2',
        'AFYX','AFF1','AFF2','ATS1','ATS2','ATS3','ATS4','ATS5','ATS6','ASMV0'} | {'AFD%d'%i for i in range(10)} | {'AFY%d'%i for i in (1,2,3,7,8,9)}

def walk(buf):
    """Yield (pattern_id, tag, words, offset). Raises on unknown tags."""
    assert buf[:15] == b'Hantei6DataFile', 'not HA6'
    p = 32
    def u32(o): return struct.unpack_from('<I', buf, o)[0]
    assert buf[p:p+4] == b'_STR'; nseq = u32(p+4); p += 8
    pid = -1
    while p < len(buf):
        tag = buf[p:p+4].decode('latin1'); o = p; p += 4
        if tag == '_END': yield (pid, tag, (), o); return
        if tag == 'PSTR': pid = u32(p); p += 4; yield (pid,tag,(pid,),o); continue
        if tag in ('PTT2','PTCN'):
            n = u32(p); raw = buf[p+4:p+4+n]; p += 4 + n; yield (pid, tag, (n, raw), o); continue
        if tag == 'PTIT': raw = buf[p:p+32]; p += 32; yield (pid,tag,(32,raw),o); continue
        if tag == 'PDS2':
            n = u32(p); w = struct.unpack_from('<%dI'%(n//4), buf, p+4); p += 4+n; yield (pid,tag,w,o); continue
        if tag in ('ATV2',):
            a,b = struct.unpack_from('<II', buf, p); w = struct.unpack_from('<%di'%(2+a*b*2), buf, p); p += 4*(2+a*b*2); yield (pid,tag,w,o); continue
        if tag in ('ATHV','ATGV','EFPR','IFPR'):
            n = u32(p); w = struct.unpack_from('<%di'%(n+1), buf, p); p += 4*(n+1); yield (pid,tag,w,o); continue
        if tag in FIXED:
            n = FIXED[tag]; w = struct.unpack_from('<%di'%n, buf, p); p += 4*n; yield (pid,tag,w,o); continue
        if tag in ZERO: yield (pid,tag,(),o); continue
        raise ValueError('unknown tag %r at 0x%x' % (tag, o))

if __name__ == '__main__':
    import collections
    c = collections.Counter()
    for f in sys.argv[1:]:
        for pid,tag,w,o in walk(open(f,'rb').read()): c[tag]+=1
    for k,v in sorted(c.items()): print(k,v)
