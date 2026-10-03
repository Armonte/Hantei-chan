/* dMp (DMP.EXE 2003-10-20) types ALREADY present in the IDB (created by an earlier PovertyCaster pass).
 * PARTIAL: this agent run created no new IDA structs (stopped early at the load cap). Field names below are the
 * existing ones; the full-typing pass (script VM globals dword_95080C/94C1EC/950700/96AEA0 etc., entity body,
 * opcode enum) is still TODO. */
struct DmpScriptBank {            /* 276 bytes, g_ScriptBanks 0x94C1F8, 64 slots, ends at dword_9506F8 */
    char  name[260];              /* upper-cased bank file name */
    void *codeImage;              /* +0x104 malloc(codeSize+2), 2 zero bytes appended */
    int   codeImageBytes;         /* +0x108 codeSize+2 */
    int   funcCount;              /* +0x10C */
    void *funcTable;              /* +0x110 malloc(36*funcCount): {char name[32]; u32 pc} */
};
struct DmpScriptThread {          /* 2148 bytes, g_ScriptThreads */
    int allocated; char name[64]; int stack[512]; int sp; void *codeImage; int pc; int bank; int line;
    int waitFlag; int waitStartMs; int waitDurMs;
};
/* DmpEntity: 1520 bytes, prev/next/sibling/parent/firstChild links, update fn at +0x18, body[1468] untyped (TODO). */
