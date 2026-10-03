#!/usr/bin/env python3
"""FOB (RBO script bytecode) disassembler + attack-record (AT) access scanner.

Format / VM facts are documented in docs/formats/ida/fob_vm.md (reverse-engineered from rbo.exe
Script_RunInterpreter 0x428AA0 and its class handlers).

  fobdis.py --all                      decode every .FOB of every RBO archive, print statistics
  fobdis.py --dis FILE|PAC:NAME [FUNC] disassemble (FILE is a loose .fob; PAC:NAME e.g. DATA02.PAC:ACOLYTE_F.FOB)
  fobdis.py --scan [--json OUT]        abstract-interpret every function of every FOB and report every
                                       memory read made through a pointer that is not script-local
                                       (aggregated by pointer chain and offset)
"""
import struct, sys, os, glob, collections, json, argparse

RBO_DIR = os.environ.get('RBO_DATA', '/mnt/c/games/rbo/DATA')
PAC_KEY = 0xE3DF59AC
PACS = ['DATA01.PAC', 'DATA02.PAC', 'Update01.PAC', 'Ex1Disc.PAC', 'Ex2Disc.PAC', 'Ex3Disc.pac', 'ETC.PAC']

U16 = struct.Struct('<H'); U32 = struct.Struct('<I')


# ---------------------------------------------------------------- PAC walking
def pac_entries(path):
    with open(path, 'rb') as f:
        magic, n = struct.unpack('<II', f.read(8)); n ^= PAC_KEY
        raw = f.read(68 * n)
    out = []
    for i in range(n):
        e = raw[68 * i:68 * i + 68]
        name = bytes(c ^ ((i * j * 3 + 61) & 0xff) for j, c in enumerate(e[:59])).split(b'\0')[0].decode('cp932', 'replace')
        out.append((name, struct.unpack_from('<I', e, 60)[0], struct.unpack_from('<I', e, 64)[0] ^ PAC_KEY))
    return out


def all_fobs():
    """yield (archive, name, bytes) for every .FOB entry of every RBO archive (duplicates included)."""
    for p in PACS:
        path = os.path.join(RBO_DIR, p)
        if not os.path.exists(path): continue
        with open(path, 'rb') as f:
            for name, off, size in pac_entries(path):
                if name.upper().endswith('.FOB'):
                    f.seek(off); yield p, name, f.read(size)


# ---------------------------------------------------------------- file layout
def load_fob(b):
    """u32 nFuncs; {char name[32]; u32 entryPc}[nFuncs]; u32 nIndexTypes; {u32 count; i32 entryPc[count]}[]; u32 codeSize; u8 code[]"""
    p = 0
    nf, = U32.unpack_from(b, p); p += 4
    funcs = []
    for i in range(nf):
        nm = b[p:p + 32].split(b'\0')[0].decode('latin1'); pc, = U32.unpack_from(b, p + 32); funcs.append((nm, pc)); p += 36
    nt, = U32.unpack_from(b, p); p += 4
    types = []
    for i in range(nt):
        c, = U32.unpack_from(b, p); p += 4
        types.append(list(struct.unpack_from('<%di' % c, b, p))); p += 4 * c
    cs, = U32.unpack_from(b, p); p += 4
    return dict(funcs=funcs, types=types, code=b[p:p + cs], trail=len(b) - (p + cs))


# ---------------------------------------------------------------- opcode tables
class Bad(Exception): pass

# top-level opcode word ("class") -> (name, handler address in rbo.exe)
CLASS = {
    0x00: ('DATA', 0x426A20), 0x01: ('STK', 0x427B60), 0x02: ('FLOW', 0x428190), 0x03: ('SYS', 0x428210),
    0x04: ('THREAD', 0x428270), 0x05: ('WAIT', 0x4282D0), 0x06: ('RAND', 0x428350), 0x07: ('FILE', 0x4283A0),
    0x08: ('BANK', 0x428420), 0x09: ('MEM', 0x428480), 0x0A: ('NOP', 0x428510), 0x0B: ('GFX', 0x428530),
    0x0C: ('CAMERA2', 0x4285C0), 0x0D: ('EFFECT_CHARSLOT', 0x428600), 0x0E: ('SOUND_CHAN', 0x428680), 0x0F: ('CAMERA', 0x428780),
    0x10: ('SYSFLOW', 0x4287F0), 0x11: ('ACTOR_OPS', 0x428850), 0x12: ('SCREEN_FADE', 0x4288E0), 0x13: ('ACTOR_FIND', 0x428940),
    0x14: ('PICTURE', 0x428960), 0x15: ('AUDIO_CHAN', 0x428710), 0x16: ('GAUGE_MISC', 0x4289E0), 0x17: ('SOUND_LOAD', 0x428A70),
    0x18: ('END', 0x428AA0), 0x19: ('HALT', 0x428AA0),
}
STK_SUB = {
    0x00: 'PUSH_IMM', 0x01: 'PUSH_CODE_ADDR', 0x02: 'PUSH_SCRATCH_ADDR', 0x03: 'PUSH_ARGS_ADDR',
    0x04: 'PUSH_REG0', 0x05: 'PUSH_REG1', 0x06: 'PUSH_REG2', 0x07: 'PUSH_REG3', 0x08: 'PUSH_REG4',
    0x09: 'POP', 0x0A: 'STORE', 0x0B: 'ADD', 0x0C: 'SUB', 0x0D: 'MUL', 0x0E: 'DIV', 0x0F: 'MOD',
    0x10: 'SHL', 0x11: 'SHR', 0x12: 'AND', 0x13: 'OR', 0x14: 'NOT', 0x15: 'XOR', 0x16: 'LNOT',
    0x17: 'LAND', 0x18: 'LOR', 0x19: 'CMP', 0x1A: 'SWAP', 0x1B: 'LOAD',
}
FLOW_SUB = {0: 'JCC', 1: 'SWITCH', 2: 'JMP', 3: 'CALL', 4: 'LJUMPSUB', 5: 'RET', 6: 'RET_FAR', 7: 'RET_VALUE'}
SYS_SUB = {0: 'LINE', 1: 'RESOLVE_BANK_ADDR', 2: 'DEBUG_PRINT', 3: 'GET_ERRNO', 4: 'SET_ERRNO', 5: 'FORMAT_PRINT'}
CMP_KIND = {0: 'eq', 1: 'ne', 2: 'le', 3: 'ge', 4: 'lt', 5: 'gt'}
JCC_KIND = {0: 'z', 1: 'nz', 2: 'lez', 3: 'gez', 4: 'ltz', 5: 'gtz'}
ASSIGN_KIND = {0: '=', 0x64: '+=', 0x65: '-=', 0x66: '*=', 0x67: '/=', 0x68: '%=', 0x69: '&=', 0x6A: '|=', 0x6B: '^='}

# native (class,sub) -> (name, handler/helper address).  Arity (stack slots popped, 2 per pushed argument)
# was measured on the corpus (stack height before each call at statement level; always one value per native).
ARITY = {(3, 1): 6, (6, 0): 4, (9, 10): 4, (11, 0): 0, (11, 1): 2, (11, 2): 4, (11, 3): 14, (11, 4): 2, (11, 5): 12, (11, 6): 4, (11, 7): 14, (11, 8): 2, (11, 9): 6, (11, 10): 0, (11, 11): 6, (12, 0): 6, (12, 1): 6, (12, 2): 4, (13, 0): 4, (13, 1): 4, (13, 2): 4, (13, 4): 0, (13, 7): 4, (13, 8): 4, (14, 0): 6, (14, 1): 6, (14, 2): 4, (14, 3): 8, (14, 4): 2, (14, 5): 2, (14, 6): 2, (14, 9): 2, (14, 10): 4, (15, 0): 2, (15, 1): 2, (15, 2): 2, (15, 3): 2, (15, 4): 10, (15, 5): 0, (15, 6): 6, (16, 0): 4, (16, 1): 6, (16, 2): 4, (16, 3): 4, (16, 4): 2, (16, 5): 4, (16, 6): 8, (17, 0): 10, (17, 1): 10, (17, 2): 12, (17, 3): 12, (17, 4): 18, (17, 5): 12, (17, 6): 4, (17, 7): 4, (17, 8): 8, (17, 9): 6, (17, 10): 6, (17, 11): 10, (17, 13): 8, (18, 0): 0, (18, 1): 2, (18, 2): 6, (18, 3): 6, (18, 4): 2, (18, 5): 2, (19, 0): 10, (20, 0): 2, (20, 1): 4, (20, 2): 2, (20, 3): 12, (20, 4): 6, (20, 5): 0, (20, 7): 0, (20, 8): 2, (21, 1): 6, (21, 2): 10, (21, 3): 4, (22, 0): 4, (22, 1): 4, (22, 2): 10, (22, 3): 10, (22, 4): 4, (22, 5): 0, (23, 0): 4}   # stack slots popped per native (2 per argument), measured on the corpus
NATIVES = {
    (0x04, 0x00): (0x429100, 'ScriptNative_Thread_Yield', 'yield one frame (ends the thread slice, pc advances)'),
    (0x04, 0x01): (0x4253C0, 'ScriptNative_Thread_ExitSelf', 'deactivate the current script thread and yield'),
    (0x04, 0x02): (0x4253E0, 'ScriptNative_Thread_Spawn', '(file,label) allocate thread, LJumpSub label, run once; thread id -> args[0]'),
    (0x04, 0x03): (0x4246F0, 'ScriptNative_Thread_KillByNameOrId', '(threadId|-1, name) find thread by name and deactivate it'),
    (0x05, 0x00): (0x4282D0, 'ScriptNative_Wait_Frames', '(frames) set thread wait counter (60 Hz) and yield'),
    (0x05, 0x01): (0x4282D0, 'ScriptNative_Wait_SpinNoAdvance', 'yield without advancing pc (re-executes next frame)'),
    (0x05, 0x02): (0x429100, 'ScriptNative_Thread_Yield', 'same as 4.0'),
    (0x05, 0x03): (0x4282D0, 'RgbFx_NoOpUpdate', 'no-op'),
    (0x05, 0x04): (0x4282D0, 'RgbFx_NoOpUpdate', 'no-op'),
    (0x05, 0x05): (0x425800, 'ScriptNative_Wait_SaveScriptState', 'write the whole VM state (threads, banks) to a file'),
    (0x05, 0x06): (0x425CD0, 'ScriptNative_Wait_LoadScriptState', 'read the VM state back from a file'),
    (0x06, 0x00): (0x428350, 'ScriptNative_Rand_Range', '(*out,max) *out = Rng_Range(max)'),
    (0x06, 0x01): (0x428350, 'ScriptNative_Rand_Range', '(*out,max) *out = Rng_Range(max) (identical to 6.0)'),
    (0x06, 0x02): (0x45F5C0, 'Rng_SetStreamState', '(value) set RNG stream state'),
    (0x06, 0x03): (0x45F5B0, 'Rng_GetStreamState', '(*out) read RNG stream state'),
    (0x07, 0x00): (0x429A20, 'ScriptNative_File_WriteBufferToPath', '(path,buf,count) create file and write'),
    (0x07, 0x01): (0x429B40, 'ScriptNative_File_ReadVirtualToBuffer', '(path,buf,seek,count) read from pack/virtual file'),
    (0x07, 0x02): (0x4283A0, 'ScriptNative_File_GetVirtualSize', '(path,*outSize)'),
    (0x07, 0x03): (0x4283A0, 'ScriptNative_File_Open', '(path,mode,*outHandle)'),
    (0x07, 0x04): (0x4283A0, 'ScriptNative_File_Read', '(*handle,buf,count)'),
    (0x07, 0x05): (0x423550, 'File_Write', '(*handle,buf,count)'),
    (0x07, 0x06): (0x42A050, 'ScriptNative_File_Seek', '(*handle,whence,offset)'),
    (0x07, 0x07): (0x4283A0, 'ScriptNative_File_Close', '(*handle)'),
    (0x07, 0x08): (0x4283A0, 'ScriptNative_File_Exists', '(path) errno=0/1'),
    (0x07, 0x09): (0x4261E0, 'ScriptNative_File_QueryVirtual', '(path,mode,*outA,*outNotFound)'),
    (0x08, 0x00): (0x424F10, 'ScriptNative_Mem_InitSlots', '(slotCount)'),
    (0x08, 0x01): (0x424F70, 'ScriptNative_Mem_AllocSlot', '(slot,size,*outPtr)'),
    (0x08, 0x02): (0x424EC0, 'ScriptNative_Mem_FreeSlot', '(slot|-1)'),
    (0x08, 0x03): (0x424FF0, 'ScriptNative_Mem_GetSlotPtr', '(slot,*out)'),
    (0x08, 0x04): (0x425000, 'ScriptNative_Mem_GetSlotSize', '(slot,*out)'),
    (0x09, 0x00): (0x428480, 'ScriptNative_Mem_Store16', '(*dst16,val)'),
    (0x09, 0x01): (0x428480, 'ScriptNative_Mem_Copy16', '(*dst16,*src16)'),
    (0x09, 0x02): (0x428480, 'ScriptNative_Str_Copy', '(dst,src) strcpy'),
    (0x09, 0x03): (0x428480, 'ScriptNative_Str_CopyN', '(dst,src,n) strncpy'),
    (0x09, 0x04): (0x428480, 'ScriptNative_Str_Length', '(*out,str)'),
    (0x09, 0x05): (0x4296E0, 'ScriptNative_String_Compare', '(*out,s1,s2,n) strncmp'),
    (0x09, 0x06): (0x428480, 'ScriptNative_Str_Find', '(*out,hay,needle) strstr offset or -1'),
    (0x09, 0x07): (0x428480, 'ScriptNative_Mem_CopyDwords', '(dst,src,count)'),
    (0x09, 0x08): (0x428480, 'ScriptNative_Mem_FillDwords', '(dst,value,count)'),
    (0x09, 0x09): (0x42A220, 'ScriptNative_String_FormatNumber', '(flags,value,digits,dest) decimal digit string'),
    (0x09, 0x0A): (0x426430, 'ScriptNative_Math_SineEase', '(*out,{start,end,t,duration})'),
    (0x0A, 0x00): (0x428510, 'RgbFx_NoOpUpdate', 'no-op'),
    (0x0B, 0x00): (0x401730, 'ScriptNative_Gfx_FreeAll', '(mode) free sequence/sprite/image tables'),
    (0x0B, 0x01): (0x401090, 'ScriptNative_Image_AllocTable', '(count)'),
    (0x0B, 0x02): (0x4010F0, 'ScriptNative_Image_SetFileName', '(imageIdx,name)'),
    (0x0B, 0x03): (0x42A5C0, 'ScriptNative_Image_SetRegion', '(imageIdx,a,b,c,d,texW,texH)'),
    (0x0B, 0x04): (0x4011E0, 'ScriptNative_Sprite_AllocTable', '(count)'),
    (0x0B, 0x05): (0x401310, 'ScriptNative_Sprite_Define', '(spriteIdx,imageIdx,x,y,w,h)'),
    (0x0B, 0x06): (0x401440, 'ScriptNative_Seq_AllocFrames', '(seqIdx,frameCount)'),
    (0x0B, 0x07): (0x42AAE0, 'ScriptNative_Seq_SetFrame', '(seqIdx,frameIdx,v0..v4)'),
    (0x0B, 0x08): (0x401560, 'ScriptNative_Seq_AllocTable', '(count)'),
    (0x0B, 0x09): (0x4015C0, 'ScriptNative_Seq_SetProps', '(seqIdx,mask,intArrayRef)'),
    (0x0B, 0x0A): (0x401750, 'ScriptNative_Image_LoadAllTextures', '()'),
    (0x0B, 0x0B): (0x4012E0, 'ScriptNative_Sprite_SetColor', '(spriteIdx,mask,intArrayRef)'),
    (0x0C, 0x00): (0x433000, 'ScriptNative_Camera_Init', '(x,y,zoom)'),
    (0x0C, 0x01): (0x433050, 'ScriptNative_Camera_SetLimits', '(flags,limitA,limitB)'),
    (0x0C, 0x02): (0x433070, 'ScriptNative_Camera_Command', '(mode,intArrayRef) sub-command switch'),
    (0x0D, 0x00): (0x4222B0, 'ScriptNative_Effect_SetSlotScript', '(slot,scriptPtr)'),
    (0x0D, 0x01): (0x4222E0, 'ScriptNative_Effect_SetSlotActive', '(slot,value)'),
    (0x0D, 0x02): (0x422300, 'ScriptNative_Effect_GetSlotActive', '(slot|-1,*out)'),
    (0x0D, 0x03): (0x422930, 'ScriptNative_Effect_AnyPoolInUse', '(a1,*out)'),
    (0x0D, 0x04): (0x422900, 'ScriptNative_Effect_TickSlots', '()'),
    (0x0D, 0x05): (0x421E00, 'ScriptNative_CharSlot_ClearMap', '()'),
    (0x0D, 0x06): (0x421E20, 'ScriptNative_CharSlot_GetId', '(slot,*out)'),
    (0x0D, 0x07): (0x43D6C0, 'ScriptNative_Char_LoadById', '(charId,*out)'),
    (0x0D, 0x08): (0x43D720, 'ScriptNative_Char_Unload', '(charId|-1,*out)'),
    (0x0E, 0x00): (0x45BF00, 'ScriptNative_Sound_LoadSlot', '(channel,slot,filename,0,0)'),
    (0x0E, 0x01): (0x45BE70, 'ScriptNative_Sound_SetSlotPlayPointer', '(channel,slot,index)'),
    (0x0E, 0x02): (0x45BEA0, 'ScriptNative_Sound_InitChannelPlayback', '(channel,startSlot)'),
    (0x0E, 0x03): (0x42B8E0, 'ScriptNative_Sound_ChainSlots', '(channel,from,to,loop)'),
    (0x0E, 0x04): (0x45BFB0, 'ScriptNative_Sound_PrepareChannel', '(channel)'),
    (0x0E, 0x05): (0x45BFD0, 'ScriptNative_Sound_PlayLooping', '(channel)'),
    (0x0E, 0x06): (0x45C0B0, 'ScriptNative_Sound_StopStreamThread', '(channel|<0 all)'),
    (0x0E, 0x07): (0x45C070, 'ScriptNative_Sound_Pause', '(channel)'),
    (0x0E, 0x08): (0x45C090, 'ScriptNative_Sound_Resume', '(channel)'),
    (0x0E, 0x09): (0x45BE20, 'ScriptNative_Sound_ReleaseChannel', '(channel|-1 all)'),
    (0x0E, 0x0A): (0x45C040, 'ScriptNative_Sound_IsPlaying', '(channel,*out)'),
    (0x0F, 0x00): (0x42FD70, 'ScriptNative_Camera_SetTarget', '(structRef)'),
    (0x0F, 0x01): (0x42FE00, 'ScriptNative_Camera_SetEnabled', '(flag)'),
    (0x0F, 0x02): (0x42FE20, 'ScriptNative_Camera_GetEnabled', '(*out)'),
    (0x0F, 0x03): (0x42FE30, 'ScriptNative_Camera_SetMode', '(mode)'),
    (0x0F, 0x04): (0x42C2A0, 'ScriptNative_Camera_SetOffsetEntry', '(index,submode,a3,a4,a5)'),
    (0x0F, 0x05): (0x42FF20, 'ScriptNative_Camera_Reset', '()'),
    (0x0F, 0x06): (0x42FD40, 'ScriptNative_Camera_SetParams', '(mask,a2,a3)'),
    (0x10, 0x00): (0x432710, 'ScriptNative_System_Command', '(subcmd,argptr) misc game-flow switch'),
    (0x10, 0x01): (0x432870, 'ScriptNative_System_SetMode', '(mode,value,addr)'),
    (0x10, 0x02): (0x432920, 'ScriptNative_System_Command2', '(subcmd,addr) stage/result flow'),
    (0x10, 0x03): (0x432A30, 'ScriptNative_System_Command3', '(subcmd,addr)'),
    (0x10, 0x04): (0x4302B0, 'ScriptNative_System_GetCurrentStageIndex', '(*out)'),
    (0x10, 0x05): (0x432AA0, 'ScriptNative_System_Command4', '(subcmd,addr)'),
    (0x11, 0x00): (0x42C8C0, 'ScriptNative_Actors_ExistsMatching', '(poolId,sel44,sel48,excludeByte92,*outBool)'),
    (0x11, 0x01): (0x42CA40, 'ScriptNative_Actors_CountMatching', '(poolId,sel44,sel48,sel92,*outCount)'),
    (0x11, 0x02): (0x42CBC0, 'ScriptNative_Actors_SetCommandAndTick', '(poolId,sel44,sel48,sel92,cmdWord,cmdByte)'),
    (0x11, 0x03): (0x42CDA0, 'ScriptNative_Actors_SetFieldByIndex', '(poolId,sel44,sel48,sel92,fieldIdx,value)'),
    (0x11, 0x04): (0x42CF80, 'ScriptNative_Actors_SetAutoMoveAndTurn', '(poolId,sel44,sel48,targetX,a5..a8,flags)'),
    (0x11, 0x05): (0x42D270, 'ScriptNative_Actors_ApplyOp', '(poolId,sel44,sel48,sel92,mode,ptr)'),
    (0x11, 0x06): (0x43CDC0, 'ScriptNative_Pool_ResetObjectsByFilter', '(poolId,flagsWord)'),
    (0x11, 0x07): (0x44A6D0, 'ScriptNative_Script_SetTargetFilter', '(op,&args)'),
    (0x11, 0x08): (0x44A750, 'ScriptCmd_ActorsKillOrSetAi', '(poolId,sel44,sel48,&args)'),
    (0x11, 0x09): (0x44A010, 'ScriptNative_Actor_Query', '(actor,queryId,&inOut)  query 6/arg 63 passes an AT pointer'),
    (0x11, 0x0A): (0x44A870, 'ScriptCmd_OverlayControl', '(actor,op,&args)'),
    (0x11, 0x0B): (0x42D860, 'ScriptNative_Actors_FindFirstMatching', '(poolId,sel44,sel48,sel92,*outActor)'),
    (0x12, 0x00): (0x4672E0, 'ScriptNative_ScreenFade_Reset', '()'),
    (0x12, 0x01): (0x467300, 'ScriptNative_ScreenFade_SetEnabled', '(enable)'),
    (0x12, 0x02): (0x467320, 'ScriptNative_ScreenFade_SetColor', '(r,g,b)'),
    (0x12, 0x03): (0x467340, 'ScriptNative_ScreenFade_Start', '(startAlpha,endAlpha,frames)'),
    (0x12, 0x04): (0x467370, 'ScriptNative_ScreenFade_SetLayer', '(layer)'),
    (0x12, 0x05): (0x467380, 'ScriptNative_ScreenFade_IsDone', '(*out)'),
    (0x13, 0x00): (0x42DCD0, 'ScriptNative_Actor_FindByDistance', '(mode,ownerList,mask,refActor,*outArr) nearest/farthest actor'),
    (0x14, 0x00): (0x4675B0, 'ScriptNative_Picture_AllocImageTable', '(count)'),
    (0x14, 0x01): (0x467610, 'ScriptNative_Picture_SetImagePath', '(idx,path)'),
    (0x14, 0x02): (0x467690, 'ScriptNative_Picture_AllocPictureTable', '(count)'),
    (0x14, 0x03): (0x4676F0, 'ScriptNative_Picture_DefinePicture', '(picIdx,imageIdx,x,y,w,h)'),
    (0x14, 0x04): (0x4677E0, 'ScriptNative_Picture_SetMode', '(picIdx,mode)'),
    (0x14, 0x05): (0x467930, 'ScriptNative_Picture_LoadAllImages', '()'),
    (0x14, 0x06): (0x467910, 'ScriptNative_Picture_Free', '(flag)'),
    (0x14, 0x07): (0x467A40, 'ScriptNative_Picture_StopBanner', '()'),
    (0x14, 0x08): (0x467B30, 'ScriptNative_Picture_StartBanner', '(picIdx)'),
    (0x14, 0x09): (0x467BA0, 'ScriptNative_Picture_IsBannerActive', '(*out)'),
    (0x15, 0x00): (0x42BD00, 'ScriptNative_Audio_AllocChannelTable', '(count)'),
    (0x15, 0x01): (0x42BD60, 'ScriptNative_Audio_DefineChannel', '(idx,type,targetId)'),
    (0x15, 0x02): (0x42BE50, 'ScriptNative_Audio_SetChannelLevels', '(idx,a2,a3,a4,a5)'),
    (0x15, 0x03): (0x42BFE0, 'ScriptNative_Audio_SetChannelState', '(idx,state)'),
    (0x15, 0x04): (0x42C080, 'ScriptNative_Audio_GetChannelState', '(idx,*out)'),
    (0x16, 0x00): (0x42E370, 'ScriptNative_Meter_TrySpend', '(amount,*okOut)'),
    (0x16, 0x01): (0x42E410, 'ScriptNative_Meter_HasAtLeast', '(amount,*out)'),
    (0x16, 0x02): (0x42E4B0, 'ScriptNative_Actor_SpawnEffectFacingTarget', '(a1,a2,byte92,mode,ptr)'),
    (0x16, 0x03): (0x42E630, 'ScriptNative_Ui_InitBarElement', '(slot,kind,valuePtr,a4,a5)'),
    (0x16, 0x04): (0x42E7C0, 'ScriptNative_Game_QueryState', '(mode,&inOut)'),
    (0x16, 0x05): (0x42E850, 'ScriptNative_Media_StopIfActive', '()'),
    (0x16, 0x06): (0x42E870, 'ScriptNative_Stub_Pop4', 'pops 4 slots, no effect'),
    (0x17, 0x00): (0x42E8D0, 'ScriptNative_Sound_LoadFile', '(slot,filename)'),
}


def native_name(c, s):
    n = NATIVES.get((c, s))
    return n[1].replace('ScriptNative_', '') if n else 'N%02X_%02X' % (c, s)


def decode(code, pc):
    """-> (cls, sub, length, operands).  Raises Bad on an undecodable word."""
    if pc < 0 or pc + 2 > len(code): raise Bad('pc out of range')
    cls, = U16.unpack_from(code, pc)
    if cls in (0x18, 0x19): return cls, 0, 2, {}
    if cls > 0x17: raise Bad('class %x' % cls)
    if pc + 4 > len(code): raise Bad('truncated')
    sub, = U16.unpack_from(code, pc + 2)
    u16 = lambda o: U16.unpack_from(code, pc + o)[0]
    u32 = lambda o: U32.unpack_from(code, pc + o)[0]
    if cls == 0:                                      # inline data: u32 n, n dwords
        if sub > 1: raise Bad('data sub')
        n = u32(4); return cls, sub, 8 + 4 * n, {'n': n}
    if cls == 1:
        if sub in (0x00, 0x01): return cls, sub, 8, {'imm': u32(4)}
        if 2 <= sub <= 9 or sub == 0x1A: return cls, sub, 4, {}
        if sub in (0x0A, 0x19): return cls, sub, 8, {'flags': u16(4), 'kind': u16(6)}
        if 0x0B <= sub <= 0x1B: return cls, sub, 6, {'flags': u16(4)}
        raise Bad('stk sub %x' % sub)
    if cls == 2:
        if sub == 0: return cls, sub, 12, {'flags': u16(4), 'kind': u16(6), 'target': u32(8)}
        if sub == 1: return cls, sub, 10, {'flags': u16(4), 'table': u32(6)}
        if sub in (2, 3): return cls, sub, 8, {'target': u32(4)}
        if 4 <= sub <= 7: return cls, sub, 4, {}
        raise Bad('flow sub')
    if cls == 3:
        if sub in (0, 2, 5): return cls, sub, 8, {'n': u32(4)}
        if sub in (1, 3, 4): return cls, sub, 4, {}
        raise Bad('sys sub')
    return cls, sub, 4, {}


def is_end(c, s):
    return c in (0x18, 0x19) or (c == 4 and s == 1) or (c == 2 and s in (5, 6, 7)) or (c == 2 and s in (1, 2))


def successors(code, pc, c, s, l, o):
    """control-flow successors (list of pcs); call targets returned separately."""
    nxt = pc + l
    if c in (0x18, 0x19) or (c == 4 and s == 1): return [], None
    if c == 2:
        if s == 0: return [nxt, o['target']], None
        if s == 1:
            t = o['table']; df, n = U32.unpack_from(code, t)[0], U32.unpack_from(code, t + 4)[0]
            return [df] + [U32.unpack_from(code, t + 8 + 8 * i + 4)[0] for i in range(n)], None
        if s == 2: return [o['target']], None
        if s == 3: return [nxt], o['target']
        if s in (5, 6, 7): return [], None
    return [nxt], None


def entry_points(f):
    e = [(n, pc) for n, pc in f['funcs']]
    for k, t in enumerate(f['types']):
        for i, x in enumerate(t):
            if x >= 0: e.append(('idx%d[%d]' % (k, i), x))
    return e


def descend(f):
    """recursive-descent decode of everything reachable from named functions + index tables."""
    code = f['code']; seen = {}; errs = []
    work = [pc for _, pc in entry_points(f)]
    while work:
        pc = work.pop()
        while pc not in seen:
            try: c, s, l, o = decode(code, pc)
            except Bad as e: errs.append((pc, str(e))); break
            seen[pc] = (c, s, l, o)
            succ, call = successors(code, pc, c, s, l, o)
            if call is not None: work.append(call)
            if len(succ) == 1 and succ[0] == pc + l: pc += l; continue
            work.extend(succ); break
    return seen, errs


# ---------------------------------------------------------------- disassembly text
def cstr(code, off):
    e = code.find(b'\0', off); return code[off:e if e >= 0 else off + 40].decode('cp932', 'replace')


def fmt(code, pc, c, s, l, o):
    if c == 0:
        d = struct.unpack_from('<%di' % o['n'], code, pc + 8)
        txt = ''
        try:
            raw = code[pc + 8:pc + 8 + 4 * o['n']].split(b'\0')[0]
            if len(raw) >= 2 and all(32 <= ch < 127 or ch >= 0x80 for ch in raw): txt = '  ; "%s"' % raw.decode('cp932', 'replace')
        except Exception: pass
        return 'DATA%d  %s%s' % (s, ' '.join(str(x) for x in d[:12]) + (' ...' if o['n'] > 12 else ''), txt)
    if c == 1:
        nm = STK_SUB.get(s, '?')
        if s == 0: return 'PUSH_IMM %d' % struct.unpack('<i', struct.pack('<I', o['imm']))[0]
        if s == 1: return 'PUSH_CODE_ADDR %d' % o['imm']
        if s in (0x0A,): return 'STORE%s %s' % ('' if not o['flags'] & 1 else '[deref rhs]', ASSIGN_KIND.get(o['kind'], o['kind']))
        if s == 0x19: return 'CMP.%s%s' % (CMP_KIND.get(o['kind'], o['kind']), ' flags=%d' % o['flags'])
        if 'flags' in o: return '%s flags=%d' % (nm, o['flags'])
        return nm
    if c == 2:
        nm = FLOW_SUB[s]
        if s == 0: return 'JCC.%s flags=%d -> %d' % (JCC_KIND.get(o['kind'], o['kind']), o['flags'], o['target'])
        if s in (2, 3): return '%s %d' % (nm, o['target'])
        if s == 1: return 'SWITCH flags=%d table=%d' % (o['flags'], o['table'])
        return nm
    if c == 3:
        if s in (0, 2, 5): return '%s %d' % (SYS_SUB[s], o['n'])
        return SYS_SUB[s]
    if c in (0x18, 0x19): return CLASS[c][0]
    return '%s.%s' % (CLASS[c][0], native_name(c, s))


def disassemble(f, fnname=None, out=sys.stdout):
    code = f['code']; seen, errs = descend(f)
    ents = entry_points(f); lab = {}
    for n, pc in ents: lab.setdefault(pc, n)
    tg = set()
    for pc, (c, s, l, o) in seen.items():
        if c == 2 and s in (0, 2, 3): tg.add(o['target'])
    start = None
    if fnname:
        for n, pc in ents:
            if n == fnname: start = pc
        if start is None: raise SystemExit('no function ' + fnname)
        # walk from start
        pcs = []; wl = [start]; vis = set()
        while wl:
            p = wl.pop()
            while p in seen and p not in vis:
                vis.add(p); pcs.append(p); c, s, l, o = seen[p]
                succ, call = successors(code, p, c, s, l, o)
                if len(succ) == 1 and succ[0] == p + l: p += l; continue
                wl.extend(succ); break
        pcs.sort()
    else: pcs = sorted(seen)
    for pc in pcs:
        c, s, l, o = seen[pc]
        if pc in lab: print('\n%s:' % lab[pc], file=out)
        elif pc in tg: print('L%d:' % pc, file=out)
        print('%7d  %s' % (pc, fmt(code, pc, c, s, l, o)), file=out)


# ---------------------------------------------------------------- abstract interpreter
# Values: ('K',n) const | ('A',region,off) address of script-local storage (args array / code area / globals)
#         ('arg',i) engine-provided argument i | ('P',base,off) base+off with base in {arg,mem,reg}
#         ('mem',addr) value loaded through a non-local pointer | ('reg',name) | ('op',name,a,b) | ('T',)
T = ('T',)
PBASE = ('arg', 'mem', 'reg', 'nret', 'gvar', 'ldat', 'AT')            # values that can be the base of a pointer chain
PADDR = ('P', 'PX') + PBASE                      # address kinds that are not script-local
PBASE_ALL = ('A', 'AX') + PADDR
def K(n): return ('K', n & 0xFFFFFFFF)
def sgn(n): return n - (1 << 32) if n & 0x80000000 else n


def add_val(a, b, sign=1):
    """a + sign*b on abstract values"""
    if a[0] == 'K' and b[0] == 'K': return K(a[1] + sign * b[1])
    if sign == 1 and a[0] == 'K': a, b = b, a
    if b[0] == 'K':
        n = sgn(b[1]) * sign
        if a[0] == 'A': return ('A', a[1], a[2] + n)
        if a[0] == 'P': return ('P', a[1], a[2] + n)
        if a[0] in PBASE: return ('P', a, n)
        if a[0] in ('AX', 'PX'): return a
        return T
    # non-constant addend: pointer + index stays inside the same object (array element access)
    if sign == 1 and b[0] in PBASE_ALL and a[0] not in PBASE_ALL:
        a, b = b, a
    if a[0] in ('A', 'AX'): return ('AX', a[1])
    if a[0] in ('P', 'PX'): return ('PX', a[1])
    if a[0] in PBASE: return ('PX', a)
    return T


def expr(v):
    t = v[0]
    if t == 'K': return '%d' % sgn(v[1]) if abs(sgn(v[1])) < 0x10000 else '0x%X' % v[1]
    if t == 'A': return '&%s%+d' % (v[1], v[2])
    if t == 'AX': return '&%s[?]' % v[1]
    if t == 'PX': return '%s+?' % expr(v[1])
    if t == 'arg': return 'arg%d' % v[1]
    if t == 'reg': return v[1]
    if t == 'nret': return '%s#%d' % (v[1], v[2])
    if t == 'AT': return 'AT'
    if t == 'gvar': return 'gvar@%s' % v[1]
    if t == 'ldat': return 'ldat@%s[?]' % v[1]
    if t == 'mem': return '[%s]' % expr(v[1])
    if t == 'P': return '%s+0x%X' % (expr(v[1]), v[2]) if v[2] >= 0 else '%s-0x%X' % (expr(v[1]), -v[2])
    if t == 'op': return '(%s %s %s)' % (expr(v[2]), v[1], expr(v[3]))
    return '?'


def has_ptr(v):
    t = v[0]
    if t in ('arg', 'P', 'mem', 'nret', 'AT'): return True
    if t == 'op': return has_ptr(v[2]) or has_ptr(v[3])
    return False


class Scan:
    """collects events from abstract interpretation"""
    def __init__(self):
        self.reads = collections.Counter()      # (addr expr) -> n
        self.reads_where = collections.defaultdict(set)
        self.ops = collections.Counter()        # (readexpr, op, K) -> n
        self.ops_where = collections.defaultdict(set)
        self.native_ptr = collections.Counter() # (native, argpos, expr, tag) -> n
        self.native_ptr_where = collections.defaultdict(set)
        self.writes = collections.Counter()
        self.writes_where = collections.defaultdict(set)
        self.kaddr_reads = 0; self.kaddr_where = set()
        self.unknown_reads = 0; self.unknown_where = set()
        self.unknown_writes = 0
        self.budget_fail = []; self.ljump = collections.Counter(); self.errors = []
        self.records = []                       # (fileKey, codeoff) record pointers stored into args[1..]


class Interp:
    def __init__(self, fkey, f, scan, budget=200000):
        self.fkey = fkey; self.f = f; self.code = f['code']; self.scan = scan; self.budget = budget; self.steps = 0
        self.where = None
        self.memo = {}
        self.gstores = collections.defaultdict(set)   # code cell -> non-constant values some function stores there
        self.gnonk = set()

    # --- state: (stack tuple, cells tuple-of-items) as python dict copy-on-write via frozen tuples
    def read_mem(self, st, addr):
        t = addr[0]
        if t == 'A':
            cell = st[1].get((addr[1], addr[2]))
            if cell is not None:
                if cell == T and addr[1] in ('code', 'args'): return ('gvar', addr[2] if addr[1] == 'code' else 'args%d' % addr[2])
                return cell
            if addr[1] == 'args': return ('arg', addr[2] // 4) if addr[2] % 4 == 0 and addr[2] >= 0 else T
            if addr[1] == 'code':
                if addr[2] in self.gnonk: return ('gvar', addr[2])
                if 0 <= addr[2] and addr[2] + 4 <= len(self.code): return K(U32.unpack_from(self.code, addr[2])[0])
                return T
            return T
        if t == 'AX': return ('ldat', addr[1])      # element of a script-local table
        if t in PADDR:
            self.scan.reads[expr(addr)] += 1; self.scan.reads_where[expr(addr)].add(self.where)
            return ('mem', addr)
        if t == 'K':          # constant address: a pointer value taken from script-local data (e.g. stage object tables); never an engine pointer
            self.scan.kaddr_reads += 1; self.scan.kaddr_where.add(self.where)
            return ('ldat', 'k')
        self.scan.unknown_reads += 1; self.scan.unknown_where.add(self.where)
        return T

    def write_mem(self, st, addr, val):
        t = addr[0]
        if t == 'A':
            cells = dict(st[1]); cells[(addr[1], addr[2])] = val
            if addr[1] == 'code' and (val[0] in PBASE or val[0] in ('P', 'PX', 'T') or has_ptr(val)): self.gstores[addr[2]].add(expr(val))
            if addr[1] == 'args' and addr[2] >= 4 and addr[2] % 4 == 0 and val[0] == 'A' and val[1] == 'code':
                self.scan.records.append((self.fkey, val[2]))
            return (st[0], cells)
        if t == 'AX':
            cells = {k: (T if k[0] == addr[1] else v) for k, v in st[1].items()}
            return (st[0], cells)
        if t in PADDR:
            self.scan.writes[expr(addr)] += 1; self.scan.writes_where[expr(addr)].add(self.where)
            return st
        self.scan.unknown_writes += 1
        return st

    def deref_if(self, st, v, flag):
        return self.read_mem(st, v) if flag else v

    def binop(self, name, a, b):
        if a[0] == 'K' and b[0] == 'K':
            x, y = sgn(a[1]), sgn(b[1])
            try:
                r = {'add': x + y, 'sub': x - y, 'mul': x * y, 'div': int(x / y) if y else 0, 'mod': (abs(x) % abs(y)) * (1 if x >= 0 else -1) if y else 0,
                     'shl': x << (y & 31), 'shr': x >> (y & 31), 'and': x & y, 'or': x | y, 'xor': x ^ y,
                     'land': int(bool(x) and bool(y)), 'lor': int(bool(x) or bool(y))}[name]
                return K(r)
            except KeyError: pass
        if name == 'add': return add_val(a, b)
        if name == 'sub':
            if a[0] == 'A' and b[0] == 'A' and a[1] == b[1]: return K(a[2] - b[2])
            return add_val(a, b, -1)
        if T in (a, b): return T
        r = ('op', name, a, b)
        # remember operations applied to freshly loaded memory values (flag / mask tests)
        for x, y in ((a, b), (b, a)):
            if x[0] == 'mem' and y[0] == 'K':
                self.scan.ops[(expr(x), name, expr(y))] += 1; self.scan.ops_where[(expr(x), name, expr(y))].add(self.where)
        return r

    def cmp(self, kind, a, b):
        x = None
        if a[0] == 'K' and b[0] == 'K':
            x, y = sgn(a[1]), sgn(b[1])
            return K(int({0: x == y, 1: x != y, 2: x <= y, 3: x >= y, 4: x < y, 5: x > y}[kind]))
        nm = 'cmp.' + CMP_KIND[kind]
        for p, q in ((a, b), (b, a)):
            if p[0] == 'mem' or (p[0] == 'op' and p[2][0] == 'mem'):
                if q[0] == 'K':
                    key = (expr(p), nm, expr(q)); self.scan.ops[key] += 1; self.scan.ops_where[key].add(self.where)
        return ('op', nm, a, b) if T not in (a, b) else T

    def run_function(self, entry, st, depth=0, stack_at=()):
        """abstract-run from entry with state st until RET; returns joined state at the returns (None if none)."""
        code = self.code
        states = {entry: st}; work = [entry]; ret = None
        while work:
            pc = work.pop()
            s0 = states[pc]
            while True:
                self.steps += 1
                if self.steps > self.budget:
                    self.scan.budget_fail.append(self.where); return None
                try: c, s, l, o = decode(code, pc)
                except Bad as e:
                    self.scan.errors.append((self.fkey, pc, str(e))); break
                try:
                    res = self.step(pc, c, s, l, o, s0, depth)
                except IndexError:
                    self.scan.errors.append((self.fkey, pc, 'stack underflow')); break
                kind = res[0]
                if kind == 'ret':
                    ret = self.join(ret, res[1]) if ret else res[1]; break
                if kind == 'end': break
                nexts = res[1]
                for npc, nst in nexts[1:]:
                    self.merge_push(states, work, npc, nst)
                npc, nst = nexts[0]
                # continue straight with first successor, merging with existing state
                if npc in states:
                    j = self.join(states[npc], nst)
                    if j == states[npc]: break
                    states[npc] = j; s0 = j; pc = npc; continue
                states[npc] = nst; s0 = nst; pc = npc
        return ret

    def merge_push(self, states, work, npc, nst):
        if npc in states:
            j = self.join(states[npc], nst)
            if j != states[npc]: states[npc] = j; work.append(npc)
        else: states[npc] = nst; work.append(npc)

    def join(self, a, b):
        if a == b: return a
        sa, sb = a[0], b[0]
        if len(sa) != len(sb):
            self.scan.errors.append((self.fkey, self.where, 'stack height mismatch at join')); return (tuple(T for _ in sa), {})
        ns = tuple(self.join_val(x, y) for x, y in zip(sa, sb))
        cells = {}
        for k in set(a[1]) | set(b[1]):
            x, y = a[1].get(k), b[1].get(k)
            if x is None or y is None:
                # a cell present on one path only: its other value is the initial content (code data / engine arg)
                other = x if y is None else y
                init = ('arg', k[1] // 4) if k[0] == 'args' and k[1] % 4 == 0 and k[1] >= 0 else (
                    K(U32.unpack_from(self.code, k[1])[0]) if k[0] == 'code' and 0 <= k[1] and k[1] + 4 <= len(self.code) else T)
                x, y = (other, init)
            cells[k] = self.join_val(x, y)
        return (ns, cells)

    @staticmethod
    def join_val(x, y):
        if x == y: return x
        if x[0] in ('A', 'AX') and y[0] in ('A', 'AX') and x[1] == y[1]: return ('AX', x[1])
        if x[0] in ('P', 'PX') and y[0] in ('P', 'PX') and x[1] == y[1]: return ('PX', x[1])
        return T

    def step(self, pc, c, s, l, o, st, depth):
        stack, cells = list(st[0]), st[1]
        nxt = pc + l
        def pop(): return stack.pop()
        S = (None,)
        if c == 0: return 'go', [(nxt, st)]
        if c in (0x18, 0x19) or (c == 4 and s == 1): return 'end', None
        if c == 1:
            if s == 0x00: stack.append(K(o['imm']))
            elif s == 0x01: stack.append(('A', 'code', o['imm']))
            elif s == 0x02: stack.append(('A', 'scratch', 0))
            elif s == 0x03: stack.append(('A', 'args', 0))
            elif 4 <= s <= 8: stack.append(('reg', 'reg%d' % (s - 4)))
            elif s == 0x09: pop()
            elif s == 0x0A:
                rhs = pop(); rhs = self.deref_if(st, rhs, o['flags'] & 1)
                lhs = stack[-1]
                k = o['kind']
                if k != 0:   # compound
                    cur = self.read_mem(st, lhs)
                    rhs = self.binop({0x64: 'add', 0x65: 'sub', 0x66: 'mul', 0x67: 'div', 0x68: 'mod', 0x69: 'and', 0x6A: 'or', 0x6B: 'xor'}[k], cur, rhs)
                st = self.write_mem((tuple(stack), cells), lhs, rhs); return 'go', [(nxt, (tuple(stack), st[1]))]
            elif s in (0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x15, 0x17, 0x18):
                rhs = pop(); lhs = pop()
                rhs = self.deref_if(st, rhs, o['flags'] & 1); lhs = self.deref_if(st, lhs, o['flags'] & 2)
                nm = {0x0B: 'add', 0x0C: 'sub', 0x0D: 'mul', 0x0E: 'div', 0x0F: 'mod', 0x10: 'shl', 0x11: 'shr', 0x12: 'and', 0x13: 'or', 0x15: 'xor', 0x17: 'land', 0x18: 'lor'}[s]
                stack.append(self.binop(nm, lhs, rhs))
            elif s in (0x14, 0x16, 0x1B):
                v = pop(); v = self.deref_if(st, v, o['flags'] & 2)
                if s == 0x14: v = K(~v[1]) if v[0] == 'K' else (('op', 'not', v, K(0)) if v != T else T)
                elif s == 0x16: v = K(int(v[1] == 0)) if v[0] == 'K' else (('op', 'lnot', v, K(0)) if v != T else T)
                stack.append(v)
            elif s == 0x19:
                rhs = pop(); lhs = pop()
                rhs = self.deref_if(st, rhs, o['flags'] & 1); lhs = self.deref_if(st, lhs, o['flags'] & 2)
                stack.append(self.cmp(o['kind'], lhs, rhs) if o['kind'] in CMP_KIND else T)
            elif s == 0x1A:
                stack[-1], stack[-2] = stack[-2], stack[-1]
            return 'go', [(nxt, (tuple(stack), cells))]
        if c == 2:
            if s == 0:
                v = pop(); v = self.deref_if(st, v, o['flags'] == 2)
                ns = (tuple(stack), cells)
                k = o['kind']
                if v[0] == 'K':
                    x = sgn(v[1]); t = {0: x == 0, 1: x != 0, 2: x <= 0, 3: x >= 0, 4: x < 0, 5: x > 0}.get(k, True)
                    return 'go', [(o['target'] if t else nxt, ns)]
                return 'go', [(nxt, ns), (o['target'], ns)]
            if s == 1:
                v = pop(); v = self.deref_if(st, v, o['flags'] == 2)
                ns = (tuple(stack), cells); t = o['table']
                df, n = U32.unpack_from(self.code, t)[0], U32.unpack_from(self.code, t + 4)[0]
                tg = [df] + [U32.unpack_from(self.code, t + 8 + 8 * i + 4)[0] for i in range(n)]
                return 'go', [(x, ns) for x in tg]
            if s == 2: return 'go', [(o['target'], (tuple(stack), cells))]
            if s == 3:
                ns = self.do_call(o['target'], (tuple(stack), cells), depth)
                if ns is None: return 'end', None
                return 'go', [(nxt, ns)]
            if s == 4:
                for _ in range(4): pop()
                # unknown callee: it may rewrite any arg cell
                cells = {k: (T if k[0] == 'args' else v) for k, v in cells.items()}
                cells.update({('args', 4 * i): T for i in range(16)})
                return 'go', [(nxt, (tuple(stack), cells))]
            if s in (5, 6): return 'ret', (tuple(stack), cells)
            if s == 7: return 'end', None
        if c == 3:
            if s in (0,): return 'go', [(nxt, st)]
            if s == 1:                           # *dst = bankCodeBase(bank) + offset : an address inside script code/data
                pp = [pop() for _ in range(6)]
                dst = pp[5]; cells = dict(cells)
                if dst[0] == 'A': cells[(dst[1], dst[2])] = ('AX', 'bank')
                elif dst[0] == 'AX': cells = {k: (T if k[0] == dst[1] else v) for k, v in cells.items()}
                return 'go', [(nxt, (tuple(stack), cells))]
            if s in (3, 4):
                pop(); pop(); return 'go', [(nxt, (tuple(stack), cells))]
            if s in (2, 5):
                for _ in range(o['n']): pop()
                return 'go', [(nxt, (tuple(stack), cells))]
        # natives
        ar = ARITY.get((c, s))
        if ar is None: ar = 0; self.scan.errors.append((self.fkey, pc, 'native arity unknown %02X.%02X' % (c, s)))
        popped = [pop() for _ in range(ar)]
        # popped = [tag_n, val_n, tag_{n-1}, val_{n-1}, ...]
        nm = native_name(c, s)
        cells = dict(cells)
        for i in range(0, ar, 2):
            tag, val = popped[i], popped[i + 1]
            pos = ar // 2 - 1 - i // 2
            ptrish = val[0] in ('arg', 'reg', 'P', 'nret') or (val[0] == 'op' and has_ptr(val) and val[2][0] in PBASE + ('P',))
            if ptrish or (tag == K(2) and val[0] in PADDR):
                key = (nm, pos, expr(val), 'ref' if tag == K(2) else 'val')
                self.scan.native_ptr[key] += 1; self.scan.native_ptr_where[key].add(self.where)
            if tag == K(2) and val[0] == 'A':     # reference to a script-local struct: pointer-valued fields are handed to the native
                for k in range(16):
                    fv = cells.get((val[1], val[2] + 4 * k))
                    if fv is not None and (fv[0] in ('arg', 'reg', 'P', 'gvar', 'AT') or (fv[0] == 'op' and has_ptr(fv))):
                        key = (nm, pos, '%s.f%d=%s' % (val[1], k, expr(fv)), 'struct')
                        self.scan.native_ptr[key] += 1; self.scan.native_ptr_where[key].add(self.where)
            if tag == K(2):                      # reference argument: the native may write the cell it points at
                if val[0] == 'A':
                    for k in range(16): cells[(val[1], val[2] + 4 * k)] = ('nret', nm, pos * 100 + k)   # may fill a struct
                elif val[0] == 'AX': cells = {k: (T if k[0] == val[1] else v) for k, v in cells.items()}
        if (c, s) in NATIVE_CLOBBERS_ARGS:
            cells.update({('args', 4 * i): T for i in range(16)})
        return 'go', [(nxt, (tuple(stack), cells))]

    def do_call(self, target, st, depth):
        if depth > 12: self.scan.budget_fail.append(self.where + ' (call depth)'); return None
        key = (target, st[0], tuple(sorted(st[1].items(), key=lambda kv: str(kv[0]))))
        if key in self.memo:
            m = self.memo[key]
            return m
        self.memo[key] = None   # recursion guard
        r = self.run_function(target, st, depth + 1)
        self.memo[key] = r
        return r


# natives that fill the args array (script-spawn, thread start): args are conservatively clobbered
NATIVE_CLOBBERS_ARGS = {(4, 2), (4, 3)}


def scan_fob(fkey, f, scan, only_entries=None, init_cells=None):
    pre = Interp(fkey, f, Scan())          # pass 1: which code cells ever receive non-constant values
    for name, pc in entry_points(f):
        pre.where = name; pre.steps = 0; pre.memo = {}
        pre.run_function(pc, ((), dict(init_cells or {})))
    it = Interp(fkey, f, scan)
    it.gnonk = set(pre.gstores); it.gstores = pre.gstores
    ents = entry_points(f)
    seen_pc = set()
    for name, pc in ents:
        if pc in seen_pc: continue
        seen_pc.add(pc)
        it.where = '%s:%s' % (fkey, name); it.steps = 0; it.memo = {}
        it.run_function(pc, ((), dict(init_cells or {})))
    return it


# ---------------------------------------------------------------- AT-pointer receivers
def overlap_records(f):
    """type-14 (kasanari overlap rule) event records referenced by the script: yield (code offset, [(kind,id,argset)...], [type1 ids])"""
    out = set()
    code = f['code']
    for off in range(0, len(code) - 36, 2):
        pass
    return out


def condition_triples(f, scan_records):
    res = []
    code = f['code']
    for off in sorted(set(scan_records)):
        if off + 36 > len(code) or struct.unpack_from('<i', code, off)[0] != 14: continue
        j = off + 32; trip = []; t1 = []
        while j + 4 <= len(code):
            t, = struct.unpack_from('<i', code, j)
            if t == -1: break
            if t == 0: trip.append(struct.unpack_from('<iii', code, j + 4)); j += 16
            elif t == 1: t1.append(struct.unpack_from('<i', code, j + 4)[0]); j += 12
            else: break
        res.append((off, trip, t1))
    return res


def at_receivers():
    """Find every condition script the engine calls with argset 4 (arg1 = attacker's AT pointer) and abstractly run it
    with arg1 typed 'AT'.  Returns list of (archive/file, kind, id, native pointer-use events, deref reads)."""
    out = []
    for p, n, b in all_fobs():
        f = load_fob(b); sc = Scan(); scan_fob('%s/%s' % (p, n), f, sc)
        for off, trip, t1 in condition_triples(f, [o for _, o in sc.records]):
            for k, i, a in trip:
                if a != 4: continue
                if k >= len(f['types']) or i >= len(f['types'][k]) or f['types'][k][i] < 0: continue
                s2 = Scan(); it = Interp('%s/%s' % (p, n), f, s2)
                it.where = '%s/%s idx%d[%d]' % (p, n, k, i)
                it.run_function(f['types'][k][i], ((), {('args', 4): ('AT',), ('args', 0): ('arg', 0)}))
                ev = [(key, v) for key, v in s2.native_ptr.items() if 'AT' in key[2]]
                out.append((p, n, k, i, ev, sorted(s2.reads)))
    return out


# ---------------------------------------------------------------- CLI
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all', action='store_true'); ap.add_argument('--scan', action='store_true'); ap.add_argument('--at', action='store_true')
    ap.add_argument('--dis'); ap.add_argument('func', nargs='?'); ap.add_argument('--json')
    a = ap.parse_args()
    if a.dis:
        if ':' in a.dis and not os.path.exists(a.dis):
            pac, name = a.dis.split(':', 1)
            for p, n, b in all_fobs():
                if p.lower() == pac.lower() and n.upper() == name.upper(): data = b; break
            else: raise SystemExit('not found')
        else: data = open(a.dis, 'rb').read()
        disassemble(load_fob(data), a.func); return
    if a.at:
        res = at_receivers(); seen = collections.Counter()
        for p, n, k, i, ev, rd in res:
            seen[(n, k, i, tuple(sorted(e[0] for e, _ in ev)), tuple(rd))] += 1
        for (n, k, i, ev, rd), c in sorted(seen.items()):
            print('%-22s kind%d id%-4d x%d AT-valued native args: %s ; deref reads: %s' % (n, k, i, c, ev, rd))
        return
    nf = 0; ninst = 0; nerr = 0; trail = 0
    scan = Scan()
    for p, n, b in all_fobs():
        f = load_fob(b); nf += 1; trail += f['trail']
        seen, errs = descend(f); ninst += len(seen); nerr += len(errs)
        for e in errs: print('DECODE ERROR', p, n, e)
        if a.scan: scan_fob('%s/%s' % (p, n), f, scan)
    print('files=%d instructions=%d decode_errors=%d trailing_bytes=%d' % (nf, ninst, nerr, trail))
    if a.scan:
        print('distinct deref addr exprs:', len(scan.reads), 'unknown reads', scan.unknown_reads, 'const-address reads', scan.kaddr_reads, 'budget fails', len(scan.budget_fail), 'errors', len(scan.errors))
        if a.json:
            json.dump(dict(
                reads={k: [v, sorted(scan.reads_where[k])] for k, v in scan.reads.items()},
                writes={k: [v, sorted(scan.writes_where[k])] for k, v in scan.writes.items()},
                ops={'|'.join(k): [v, sorted(scan.ops_where[k])] for k, v in scan.ops.items()},
                native_ptr={'|'.join(map(str, k)): [v, sorted(scan.native_ptr_where[k])] for k, v in scan.native_ptr.items()},
                unknown_reads=scan.unknown_reads, kaddr_reads=scan.kaddr_reads, kaddr_where=sorted(scan.kaddr_where), unknown_where=sorted(scan.unknown_where), records=scan.records[:0],
                errors=scan.errors[:50]), open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
