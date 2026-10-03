/* Qoh98.exe character .dat container (PARTIAL: field typing of the 24/52/8-byte records still unproven). */
#pragma pack(push,1)
struct Qoh98DatHeader {            /* 28 bytes, file offset 0 */
    unsigned int version;          /* always 5 in all 13 files; value discarded by loader */
    unsigned int imageCount;       /* c0: TIM entries == image table entries */
    unsigned int tileCount;        /* c1: 260-byte tile blocks */
    unsigned int rectCount;        /* c2: 24-byte records, 256 in every file */
    unsigned int spriteCount;      /* c3: 52-byte records */
    unsigned int animCount;        /* c4: 8-byte records */
    unsigned int hurtCount;        /* c5: 24-byte records */
};
struct Qoh98TimPathEntry {         /* 260 bytes; read and discarded by game */
    char name[8];                  /* "Aka0000" NUL, rest 0xCD */
    char sjisTextAndPath[252];     /* SJIS display text then "\\AKARI\\Aka0000.tim", 0xCD padded (split unproven) */
};
struct Qoh98ImageTileRange {       /* 8 bytes, stored in a1[2]; ranges partition the tile array in order */
    int firstTile;
    int tileCount;
};
struct Qoh98Tile {                 /* 260 bytes, stored in a1[0] */
    unsigned short x;              /* multiple of 16, <=256 (position inside the TIM sheet) */
    unsigned short y;              /* multiple of 16, <=256 */
    unsigned char  pixels[256];    /* 16x16, 8bpp palette indices (hypothesis: row-major) */
};
struct Qoh98CharData {             /* runtime, 12 dwords (a1[]) filled by the loader */
    Qoh98Tile*           tiles;        /* [0]  260*tileCount */
    int                  tileCount;    /* [1]  c1 */
    Qoh98ImageTileRange* images;       /* [2]  8*imageCount */
    int                  imageCount;   /* [3]  c0 */
    void*                rects24;      /* [4]  24*rectCount, role unproven */
    int                  rectCount;    /* [5]  c2 */
    void*                sprites52;    /* [6]  52*spriteCount, layout unproven */
    int                  spriteCount;  /* [7]  c3 */
    void*                anims8;       /* [8]  8*animCount, layout unproven */
    int                  animCount;    /* [9]  c4 */
    void*                hurt24;       /* [10] 24*hurtCount, layout unproven */
    int                  hurtCount;    /* [11] c5 */
};
#pragma pack(pop)
