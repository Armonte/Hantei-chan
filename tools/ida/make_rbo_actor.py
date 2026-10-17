#!/usr/bin/env python3
"""Generates the IDA-parsable RboActor struct (size 0x948, Actor_Init 0x441A90) from the traced-field tables:
table F below (frame/pattern/box fields found first) plus docs/formats/ida/actor_part{1..4}_fields.py (full actor RE, one file per
offset range, with actor_part*_types.h holding their nested structs/enums). Any byte left uncovered is a hard error: the actor
has ZERO unmapped bytes. Run:
  python3 tools/ida/make_rbo_actor.py > docs/formats/ida/rbo_actor_types.h   (parse actor_part*_types.h first, then this file)"""
SIZE = 0x948
# (decimal offset, size, c type, name, comment)
F = [
 (12, 4, 'RboActor *', 'nextSibling', 'Actor_DrawTree 0x4483E0 walks v2+12'),
 (20, 4, 'RboActor *', 'firstChild', 'Actor_DrawTree recurses into v2+20'),
 (60, 4, 'int', 'playerId', 'Actor_Init arg a3; compared in the hit resolver sub_4433E0 (v3[15])'),
 (92, 1, 'unsigned char', 'pattern', 'Actor_CacheCurrentFrame 0x440920'),
 (93, 1, 'unsigned char', 'frame', 'frame number inside the pattern'),
 (96, 4, 'struct RboFrameRecord *', 'nextFrame', 'Actor_ResolveNextFrameRecord 0x440970'),
 (100, 2, 'unsigned __int16', 'nextSpriteId', 'Actor_GetFrameSpriteId result'),
 (104, 4, 'int', 'frameTicks', 'compared with frame.duration in Actor_TickFrame 0x43F830'),
 (108, 1, 'unsigned char', 'loopCounter', 'loaded from frame.loopCount in Actor_EnterFrame 0x43EF30'),
 (160, 4, 'int', 'posX', 'sub_440D50 box placement'),
 (164, 4, 'int', 'posY', ''),
 (168, 4, 'int', 'posZ', ''),
 (692, 4, 'int', 'drawDepthOffset', 'sub_440DF0 (draw priority codes 12..61)'),
 (696, 4, 'int', 'drawLayer', 'sub_440DF0 (codes 1..11); Actor_DrawTree selects actors by layer'),
 (744, 4, 'int', 'blendMode', 'Actor_EnterFrame copies frame.blendMode here'),
 (1316, 4, 'int', 'jumpCounter', 'Actor_TickFrame increments on pattern change'),
 (1320, 2, 'unsigned __int16', 'pendingPattern', 'Actor_ApplyLandJump/hit routines; consumed by Actor_TickFrame, 0xFFFF = none'),
 (1322, 2, 'unsigned __int16', 'pendingFrame', '0xFFFF = none'),
 (1324, 1, 'unsigned char', 'pendingFlip', 'Actor_TickFrame, 0xFF = none'),
 (1328, 4, 'int', 'pendingFlipNegatesVelocity', 'Actor_TickFrame'),
 (1332, 1, 'unsigned char', 'facingLeft', 'Actor_FrameMoveAddSpeed 0x44B3D0'),
 (1336, 4, 'unsigned char *', 'facingLeftPtr', 'Actor_Init: points at +1332'),
 (1376, 4, 'int', 'altDrawMode', 'Actor_CacheCurrentFrame: pattern flag 0x40'),
 (1380, 4, 'struct RboFrameRecord *', 'curFrame', 'Actor_CacheCurrentFrame'),
 (1868, 4, 'int', 'attackBoxCount', 'a1[467] Actor_CollectAttackBoxes2 0x440F40'),
 (1872, 4, 'struct RboAtRecord *', 'curAttack', 'a1[468]: 120-byte AT record'),
 (1876, 8, 'void *', 'attackBoxRect[2]', 'a1[469..470] pointers into the box section'),
 (1884, 8, 'int', 'attackBoxSlot[2]', 'a1[471..472] index of the slot in frame.attackBoxIdx'),
 (1904, 4, 'int', 'hurtBoxCount', 'a1[476] Actor_CollectHurtBoxes3 0x441120'),
 (1908, 12, 'void *', 'hurtBoxRect[3]', 'a1[477..479]'),
 (1920, 12, 'int', 'hurtBoxSlot[3]', 'a1[480..482]'),
 (2008, 4, 'int', 'kasanariBoxCount', 'a1[502] Actor_CollectBoxes2_272 0x4411E0'),
 (2012, 8, 'void *', 'kasanariBoxRect[2]', 'a1[503..504]'),
 (2020, 8, 'int', 'kasanariBoxSlot[2]', 'a1[505..506]'),
 (2028, 4, 'struct RboCharContext *', 'charContext', 'Actor_Init'),
 (2032, 4, 'struct RboPatternAreaView *', 'patternArea', 'Actor_Init: charContext+28'),
 (2072, 4, 'int', 'stanceClass', 'Actor_ApplyFrameStatusFlags 0x441330 copies frame.stanceClass'),
 (2076, 4, 'unsigned int', 'hitClassMask', 'Actor_ApplyFrameStatusFlags: 895 minus frame.hurtMaskFlags classes'),
 (2352, 4, 'int', 'ticksUntilWrap', 'Actor_TickFrame: += 256 when <= 0'),
 (2372, 4, 'unsigned int', 'enterFlags', 'Actor_EnterFrame sets 7'),
]

import importlib.util, os, sys
here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'formats', 'ida')
DROP = {'spareMover', 'patternHistoryTail[8]'}   # duplicates of motionSlot460 / the tail of repeatTracker (parts overlap by design)
rows = list(F)
for n in (1, 2, 3, 4):
    spec = importlib.util.spec_from_file_location('part%d' % n, os.path.join(here, 'actor_part%d_fields.py' % n))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    for f in m.FIELDS:
        if f[3] in DROP: continue
        rows.append(tuple(f))
out = ['struct RboAtRecord;', 'struct RboCharContext;', 'struct RboActor;', 'struct RboActor {']
pos = 0
for off, sz, ty, nm, cm in sorted(rows, key=lambda r: r[0]):
    if off < pos: sys.exit('overlap at %d (%s) pos %d' % (off, nm, pos))
    if off > pos: sys.exit('UNMAPPED bytes 0x%X..0x%X before %s' % (pos, off, nm))
    cm = cm.replace('\n', ' ')
    if '[' in nm:
        base, cnt = nm.split('['); cnt = cnt.rstrip(']')
        out.append(' %s %s[%s]; // +0x%03X %s' % (ty, base, cnt, off, cm))
    else:
        out.append(' %s %s; // +0x%03X %s' % (ty, nm, off, cm))
    pos = off + sz
if pos != SIZE: sys.exit('actor ends at 0x%X, expected 0x%X' % (pos, SIZE))
out.append('};')
print('\n'.join(out))
