#include "global.h"
#include "leo/leo_internal.h"

#ifdef PORT
/* Untruncated host pointer for the glyph buffer (D_xk1_8003A488 is an s32 and
   truncates the 64-bit Arena pointer). Glyph fills copy through this. */
static u8* sGdxGlyphBufferHost;

/* The transplanted English strings (expansion_kit_text.c) are plain ASCII, but
   every walker in this file was written for two-byte text: it pairs consecutive
   bytes into one glyph code, so ASCII pairs like "Ch" become nonsense codes,
   LeoGetKAdr rejects them, and the whole dialog renders blank. Read one glyph per
   call instead: a high-bit lead byte keeps the original two-byte decoding,
   printable ASCII advances one byte and maps to the drive-ROM font's fullwidth
   equivalent.

   The codes MUST be Shift-JIS, not JIS X 0208. LeoGetKAdr (port/n64_leo.c,
   ported from the US rev0 leo overlay) rejects anything outside [0x8140, 0x9872];
   JIS row 1 / row 3 codes (0x21xx/0x23xx) return -1, zero-fill every cell, and
   leave the text invisible even with the IPL image loaded. Verified against both
   the US-prototype and JP-retail IPL dumps: all 81 mapped characters resolve to
   non-blank 16x16 I4 cells.

   Fullwidth bases: '０' = 0x824F, 'Ａ' = 0x8260, 'ａ' = 0x8281, ideographic
   space = 0x8140. Unmapped ASCII falls back to the fullwidth space, which is also
   what the strings already embed literally as "\x81\x40" to reserve the inline
   help-icon slot. */
/* Second encoding hazard, distinct from the ASCII mapping above: the strings that
   were never translated are still Japanese, and the decomp stores them as EUC-JP
   byte escapes (e.g. "\305\300\244\316" = EUC 0xC5C0 0xA4CE). EUC codes live at
   0xA1A1..0xFEFE, entirely above LeoGetKAdr's [0x8140, 0x9872] Shift-JIS window,
   so every one is rejected and renders blank.

   Convert EUC-JP to Shift-JIS through JIS: strip the 0x8080 EUC bias to get the
   JIS row/cell, then apply the standard JIS->Shift-JIS packing. Verified against
   the IPL font: EUC 0xA4CE -> JIS 0x244E -> SJIS 0x82CC decodes to the correct
   hiragana "no" bitmap.

   Discrimination is by lead byte and is safe here: 0x81-0x9F is unambiguously a
   Shift-JIS lead (EUC leads start at 0xA1), which is what the translated strings
   embed literally as "\x81\x40". The 0xE0-0xFC overlap between the two encodings
   is why this normalization lives at a walker whose input is known to be decomp
   C-source text, rather than inside LeoGetKAdr where a genuine Shift-JIS caller
   in that range would be corrupted. */
static u16 GdxEucToSjis(u8 b1, u8 b2) {
    u8 j1 = (u8)(b1 - 0x80);
    u8 j2 = (u8)(b2 - 0x80);
    u8 s1 = (u8)(((j1 - 0x21) >> 1) + ((j1 <= 0x5E) ? 0x81 : 0xC1));
    u8 s2;

    if (j1 & 1) {
        s2 = (u8)(j2 + 0x1F + ((j2 >= 0x60) ? 1 : 0));
    } else {
        s2 = (u8)(j2 + 0x7E);
    }
    return (u16)((s1 << 8) | s2);
}

/* Third hazard: ADVANCE WIDTH. Every EK panel sizes its box with
   `mfsStrLen(str) * 8` -- eight pixels per BYTE (course_edit/19FA50.c:129,245,
   271,318, 1A4210.c:474, A8140.c:435,457, machine_create_draw.c:344,350,358;
   mfsStrLen in leo/mfs/mfs_device.c is a plain byte counter). The walkers in this
   file advance a fixed 16 pixels per GLYPH. For two-byte JP text those agree
   exactly -- two bytes of box, one 16px fullwidth glyph -- which is why hardware
   never showed a defect. One-byte ASCII breaks the identity: eight pixels of box,
   sixteen drawn, a clean 2x overrun (the 24-byte tooltip "Use the  icon to
   display" reserves 192px and the port drew 384px).

   So ASCII must render from the drive ROM's halfwidth ANK font (LeoGetAAdr), not
   from a fullwidth kanji lookalike. Decoded offline against both ROM images:
     - ANK index is exactly `ascii - 0x20`. The metric signature is conclusive:
       'I' and ':' are 1px wide, 'i'/'l'/'.' 2px, '1' 3px, 'W'/'m'/'@' 13px,
       '-' is 8x2, '.' is 2x2, and g/j/p/q/y all carry descenders.
     - Cells are I4 packed at ceil(dx/2) bytes per row, dy rows, the whole glyph
       rounded up to a 2-byte boundary (LeoGetAAdr returns `off << 1`).
     - `cy` counts rows ABOVE the baseline, so the cell-relative top is
       `GDX_ANK_BASELINE - cy`.
   Summing dx over that same tooltip gives 175px, inside the 192px box the layout
   reserves: one ASCII byte costs ~8px of both budget and ink, one fullwidth pair
   costs 16 of each.

   Codes therefore stay in two disjoint ranges. Below 0x100 is an ASCII character
   served by the ANK font; at or above is a Shift-JIS code for the fullwidth kanji
   font. LeoGetKAdr's window starts at 0x8140, so the split cannot collide. */
#define GDX_GLYPH_IS_ANK(code) ((code) < 0x100)
#define GDX_ANK_BASELINE 11

static u16 GdxReadGlyphCode(u8** cursor) {
    u8* p = *cursor;
    u8 c = p[0];

    if (c & 0x80) {
        *cursor = p + 2;
        if (c >= 0xA1 && p[1] >= 0xA1) {
            return GdxEucToSjis(c, p[1]);
        }
        return (u16)((c << 8) + p[1]);
    }
    *cursor = p + 1;
    if (c >= 0x20 && c < 0x7F) {
        return (u16)c;
    }
    /* Control bytes never reach here as text (0x0A is handled by the callers);
       anything else degrades to a blank of one column's width. */
    return (u16)' ';
}

/* Parallel-walk step for the draw-side iterators: advance the string cursor by
   exactly the bytes one glyph consumed, keeping the precomputed offset table
   (walked with var_s1++) in sync with the string. */
static s8* GdxGlyphStringStep(s8* p) {
    u8* cursor = (u8*)p;

    GdxReadGlyphCode(&cursor);
    return (s8*)cursor;
}

/* Pixel advance per unique glyph, parallel to D_xk1_8003A54C. Fullwidth glyphs
   keep the original 16; ANK glyphs carry the font's own dx. */
static u8* sGdxGlyphAdvance;

/* Scratch cells for func_xk1_800260F0, the draw-time (not precomputed) glyph
   path. Retail runs that path out of the first 0xE00 bytes of the same buffer the
   precomputed glyphs live in, bumping D_xk1_8003A490 by 0x80 per glyph and relying
   on func_xk1_800260E4() to rewind it -- which ONLY course_edit calls
   (191080.c:218). Machine Create draws its confirmation prompts through the same
   path (ABC40.c func_xk1_8002ECE8 -> func_xk1_800262F4) and never rewinds, so the
   cursor walks forward every frame, past the 28 scratch cells, through the
   precomputed glyphs, and off the end of the Arena block. On hardware that is a
   DMA destination address and the overrun is silent; here it is a memcpy into
   whatever the Arena handed out next.

   Give that path its own ring instead. 160 cells is 20 KB, comfortably more than
   the longest prompt (the 33-character "Machine data must be saved first."):
   wrapping inside one frame would reuse a cell the display list still points at,
   which is why the ring is sized against the worst string rather than the average
   one. The precomputed glyphs at 0xE00+ and the D_xk1_8003A490 bookkeeping are
   unchanged. */
#define GDX_DYN_GLYPH_CELLS 160
static u8* sGdxDynGlyphs;
static s32 sGdxDynCursor;

/* Blit one variable-size ANK glyph into a fixed 16x16 I4 cell.

   Keeping the cell 16x16 is deliberate: it leaves every gDPLoadTextureBlock_4b
   in this file untouched, so only the destination rect width and the cursor
   advance become per-glyph. The widest ANK glyph is 13px over at most 16 rows
   (7 bytes/row * 16 = 112), so it always fits the 0x80-byte cell the arena
   already reserves. */
static void GdxFillAnkCell(u8* dst, s32 ankIndex) {
    extern unsigned char* gdx_ddipl_buffer;
    extern unsigned int gdx_ddipl_size;
    s32 dx = 0;
    s32 dy = 0;
    s32 cy = 0;
    s32 fontAddr;
    s32 srcStride;
    s32 top;
    s32 y;
    s32 x;
    const u8* src;

    bzero(dst, 0x80);

    fontAddr = LeoGetAAdr(ankIndex, &dx, &dy, &cy) + DDROM_FONT_START;
    if (gdx_ddipl_buffer == NULL || dx <= 0 || dy <= 0) {
        return;
    }

    srcStride = (dx + 1) >> 1;
    if (fontAddr < DDROM_FONT_START || (u32)(fontAddr + srcStride * dy) > gdx_ddipl_size) {
        return;
    }

    src = gdx_ddipl_buffer + fontAddr;
    top = GDX_ANK_BASELINE - cy;
    if (top < 0) {
        /* Only '$' (cy=12) reaches here, and only by a single row of its upper
           tip: no cell height can satisfy both the tallest ascender and the
           deepest descender at once. Clamp rather than shift the baseline, so
           every other glyph stays aligned. */
        top = 0;
    }

    for (y = 0; y < dy; y++) {
        s32 row = top + y;

        if (row < 0 || row >= 16) {
            continue;
        }
        for (x = 0; x < dx && x < 16; x++) {
            u8 packed = src[y * srcStride + (x >> 1)];
            u8 nib = (x & 1) ? (u8)(packed & 0xF) : (u8)(packed >> 4);

            if (x & 1) {
                dst[row * 8 + (x >> 1)] |= nib;
            } else {
                dst[row * 8 + (x >> 1)] |= (u8)(nib << 4);
            }
        }
    }
}

/* Defined below with the rest of the file's globals; needed here to bound the
   advance table lookup. */
extern s32 D_xk1_8003A494;

/* Advance for a glyph identified by its buffer offset — the form the
   precomputed-offset walkers carry. Offsets are laid out by func_xk1_80025ED4
   as (index << 7) + 0xE00. */
static s32 GdxGlyphAdvanceForOffset(s32 offset) {
    s32 index = (offset - 0xE00) >> 7;

    if (sGdxGlyphAdvance == NULL || index < 0 || index >= D_xk1_8003A494) {
        return 16;
    }
    return sGdxGlyphAdvance[index];
}

/* Advance for a glyph identified by its code — the form func_xk1_800262F4
   carries, which draws straight from the code without the offset table. */
static s32 GdxGlyphAdvanceForCode(u16 code) {
    s32 dx = 0;

    if (!GDX_GLYPH_IS_ANK(code)) {
        return 16;
    }
    if (LeoGetAAdr((s32)code - 0x20, &dx, NULL, NULL) < 0 || dx <= 0) {
        return 8;
    }
    return dx;
}

/* Drawn width of a one-line EK string, in pixels, using the same per-glyph
   advances func_xk1_800262F4 uses to place it. Callers that centre a string need
   this because ASCII is proportional: the retail box widths are sized for
   two-byte text at a flat 16 px per glyph and are far too wide for the
   fan-translated English, which would otherwise sit hard against the box's left
   edge. Stops at a newline; ABC40.c's prompts are single-line. */
s32 GdxGlyphStringWidth(u8* str) {
    u8* cursor = str;
    s32 x = 0;

    if (str == NULL) {
        return 0;
    }
    while (cursor[0] != '\0' && cursor[0] != 0xA) {
        x += GdxGlyphAdvanceForCode(GdxReadGlyphCode(&cursor));
    }
    return x;
}

/* Locate the run of fullwidth spaces a string reserves for an inline icon.

   The help tooltip embeds "\x81\x40\x81\x40" to leave a hole for the question
   icon, and the caller (course_edit/19FA50.c) used to hardcode the icon at
   x=128 — correct only while every glyph was a fixed 16px, which is what
   two-byte JP text guaranteed. Proportional ASCII moves the hole, so derive
   the position from the same advance logic that placed the text instead of
   restating a constant that can no longer be right.

   Returns the pixel offset of the run from the start of the first line, with
   its width in *gapWidth, or -1 when the string reserves no such hole. */
s32 GdxGlyphIconOffset(s8* str, s32* gapWidth) {
    u8* cursor = (u8*)str;
    s32 x = 0;

    while (cursor[0] != '\0' && cursor[0] != 0xA) {
        u8* runStart = cursor;
        u16 code = GdxReadGlyphCode(&cursor);

        if (code == 0x8140) {
            s32 width = 16;

            /* Consume the rest of the run so a two-cell hole reports 32, not 16. */
            while (cursor[0] != '\0' && cursor[0] != 0xA) {
                u8* next = cursor;

                if (GdxReadGlyphCode(&next) != 0x8140) {
                    break;
                }
                cursor = next;
                width += 16;
            }
            if (gapWidth != NULL) {
                *gapWidth = width;
            }
            return x;
        }
        (void)runStart;
        x += GdxGlyphAdvanceForCode(code);
    }
    return -1;
}
#endif

OSIoMesg D_xk1_8003A470;
s32 D_xk1_8003A488;
UNUSED s32 D_xk1_8003A48C;
s32 D_xk1_8003A490;
s32 D_xk1_8003A494;
s32* D_xk1_8003A498[31];
s32* D_xk1_8003A518[12];
s32* D_xk1_8003A548;
u16* D_xk1_8003A54C;

u8 D_80030060[9] = { 0 };
u8 D_8003006C[9] = { 0 };

UNUSED s32 D_80030078 = 0;
UNUSED s32 D_8003007C = 0;

s32 D_xk1_80030080 = 0;
s32 D_xk1_80030084 = 0;

s32 func_xk1_80025C20(s8* arg0) {
    s32 var_v1;

    var_v1 = 0;
    while (true) {

        if (*arg0 == 0) {
            break;
        }

        if (*arg0 == 0xA) {
            arg0++;
        } else {
#ifdef PORT
            u8* cursor = (u8*)arg0;
            GdxReadGlyphCode(&cursor);
            arg0 = (s8*)cursor;
#else
            arg0 += 2;
#endif
            var_v1++;
        }
    }
    return var_v1;
}

extern u8* D_xk1_800331F0[];
extern u8* D_xk1_8003339C[];

s32 func_xk1_80025C58(void) {
    s32 var_s1;
    s32 i;

    var_s1 = 0;
    for (i = 0; i < 31; i++) {
        var_s1 += func_xk1_80025C20(D_xk1_800331F0[i]);
    }
    for (i = 0; i < 12; i++) {
        var_s1 += func_xk1_80025C20(D_xk1_8003339C[i]);
    }

    return var_s1;
}

s32 func_xk1_80025CD8(u16 arg0) {
    s32 i;

    for (i = 0; i < D_xk1_8003A494; i++) {
        if (arg0 == D_xk1_8003A54C[i]) {
            return 0;
        }
    }
    return 1;
}

void func_xk1_80025D2C(char* arg0) {
    u16 temp_s1;

    while (true) {
        if (arg0[0] == '\0') {
            break;
        }

        if (arg0[0] == 0xA) {
            arg0++;
        } else {
#ifdef PORT
            u8* cursor = (u8*)arg0;
            temp_s1 = GdxReadGlyphCode(&cursor);
            arg0 = (char*)cursor;
#else
            temp_s1 = (arg0[0] << 8) + arg0[1];
#endif
            if (func_xk1_80025CD8(temp_s1) != 0) {
                D_xk1_8003A54C[D_xk1_8003A494] = temp_s1;
                D_xk1_8003A494++;
            }
#ifndef PORT
            arg0 += 2;
#endif
        }
    }
}

s32 func_xk1_80025DE4(void) {
    s32 i;

    D_xk1_80030084 = 0;
    for (i = 0; i < D_xk1_80030080; i++) {}
    D_xk1_8003A494 = 0;

    for (i = 0; i < 31; i++) {
        func_xk1_80025D2C(D_xk1_800331F0[i]);
    }

    for (i = 0; i < 12; i++) {
        func_xk1_80025D2C(D_xk1_8003339C[i]);
    }

    return D_xk1_8003A494;
}

s32 func_xk1_80025E8C(u16 arg0) {
    s32 i;

    for (i = 0; i < D_xk1_8003A494; i++) {
        if (arg0 == D_xk1_8003A54C[i]) {
            break;
        }
    }
    return i;
}

void func_xk1_80025ED4(char* arg0) {
    u16 temp_s1;

    while (true) {
        if (arg0[0] == '\0') {
            break;
        }

        if (arg0[0] == 0xA) {
            arg0++;
        } else {
#ifdef PORT
            /* The table is s32-typed, so it cannot hold a 64-bit host address: store the
               OFFSET into the glyph buffer and let the draw side (func_xk1_800263B0) add
               sGdxGlyphBufferHost. Storing base+offset here truncated the pointer and the
               precomputed-string glyph loads sampled garbage (invisible text — e.g. the
               Course Edit invalid-node warning message). */
            u8* cursor = (u8*)arg0;
            temp_s1 = GdxReadGlyphCode(&cursor);
            arg0 = (char*)cursor;
            D_xk1_8003A548[D_xk1_80030080] = (func_xk1_80025E8C(temp_s1) << 7) + 0xE00;
            D_xk1_80030080++;
#else
            temp_s1 = (arg0[0] << 8) + arg0[1];
            D_xk1_8003A548[D_xk1_80030080] = (func_xk1_80025E8C(temp_s1) << 7) + D_xk1_8003A488 + 0xE00;
            D_xk1_80030080++;
            arg0 += 2;
#endif
        }
    }
}

void func_xk1_80025F98(void) {
    s32 i;

    D_xk1_80030080 = func_xk1_80025C58();
    D_xk1_8003A54C = Arena_Allocate(ALLOC_FRONT, D_xk1_80030080 * sizeof(u16));
    D_xk1_8003A548 = Arena_Allocate(ALLOC_FRONT, D_xk1_80030080 * sizeof(s32));
    D_xk1_8003A494 = func_xk1_80025DE4();
#ifdef PORT
    {
        /* Keep the full pointer for glyph fills (assignment below truncates to
           s32) and clear the storage — Arena memory is not zeroed on the port
           and the hardware DMA that would overwrite it is replaced by direct
           copies from the IPL ROM image. */
        void* buf = (void*)Arena_Allocate(ALLOC_FRONT, (D_xk1_8003A494 << 7) + 0xE00);
        extern unsigned char* gdx_ddipl_buffer;
        extern unsigned int gdx_ddipl_size;
        extern void gdx_dbg_logf(const char* fmt, ...);
        sGdxGlyphBufferHost = (u8*)buf;
        bzero(buf, (D_xk1_8003A494 << 7) + 0xE00);
        /* Per-glyph pixel advance, filled alongside each cell by
           func_xk1_8002671C. Sized and cleared here so a glyph that never gets
           filled still reads back as the fullwidth default. */
        sGdxGlyphAdvance = (u8*)Arena_Allocate(ALLOC_FRONT, D_xk1_8003A494);
        if (sGdxGlyphAdvance != NULL) {
            s32 g;

            for (g = 0; g < D_xk1_8003A494; g++) {
                sGdxGlyphAdvance[g] = 16;
            }
        }
        /* Ring for the draw-time glyph path; see GDX_DYN_GLYPH_CELLS above. */
        sGdxDynGlyphs = (u8*)Arena_Allocate(ALLOC_FRONT, GDX_DYN_GLYPH_CELLS * 0x80);
        sGdxDynCursor = 0;
        if (sGdxDynGlyphs != NULL) {
            bzero(sGdxDynGlyphs, GDX_DYN_GLYPH_CELLS * 0x80);
        }
        D_xk1_8003A488 = (s32)(uintptr_t)buf;
        /* The glyph pre-fill below runs exactly once; if the IPL image is not
           loaded yet at this moment every glyph is bzero'd blank and stays
           blank for the session. This line decides that timing question. */
        gdx_dbg_logf("[ek-glyph] prefill: uniqueGlyphs=%d ipl=%p iplSize=%u\n",
                     D_xk1_8003A494, (void*)gdx_ddipl_buffer, gdx_ddipl_size);
    }
#else
    D_xk1_8003A488 = Arena_Allocate(ALLOC_FRONT, (D_xk1_8003A494 << 7) + 0xE00);
#endif
    D_xk1_8003A490 += 0xE00;
    func_xk1_800267C4(D_xk1_8003A54C);
    D_xk1_80030080 = 0;

    for (i = 0; i < 31; i++) {
        D_xk1_8003A498[i] = &D_xk1_8003A548[D_xk1_80030080];
        func_xk1_80025ED4(D_xk1_800331F0[i]);
    }

    for (i = 0; i < 12; i++) {
        D_xk1_8003A518[i] = &D_xk1_8003A548[D_xk1_80030080];
        func_xk1_80025ED4(D_xk1_8003339C[i]);
    };
}

void func_xk1_800260E4(void) {
    D_xk1_8003A490 = 0;
}

extern OSMesgQueue gDmaMesgQueue;
extern OSPiHandle* gDriveRomHandle;

Gfx* func_xk1_800260F0(Gfx* gfx, s32 arg1, s32 arg2, s32 code) {
    s32 fontAddr = LeoGetKAdr(code) + DDROM_FONT_START;

    // TODO: move to appropriate place
    PRINTF("KANJI NUM COUNT START\n");
    PRINTF("KANJI NUM IS %d\n");
    PRINTF("FONT NUM IS %d\n");
    PRINTF("\nDMA OK\n");
    PRINTF("KANJI READY OK %d\n");
    PRINTF("*");

    D_xk1_8003A470.hdr.pri = 0;
    D_xk1_8003A470.hdr.retQueue = &gDmaMesgQueue;
    D_xk1_8003A470.dramAddr = D_xk1_8003A488 + D_xk1_8003A490;
    D_xk1_8003A470.devAddr = fontAddr;
    D_xk1_8003A470.size = 0x80;
#ifndef PORT
    gDriveRomHandle->transferInfo.cmdType = LEO_CMD_TYPE_2;
    func_80768B88(gDriveRomHandle, &D_xk1_8003A470, OS_READ);
    osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
#else
    /* Glyphs live in the 64DD drive's internal ROM; copy from the
       user-supplied IPL ROM image (port/disk_buffer.cpp). Invalid codes
       (LeoGetKAdr < 0 => fontAddr < DDROM_FONT_START) fall back to a blank
       glyph.

       ASCII MUST take the halfwidth ANK branch here, exactly as the prefill
       does in func_xk1_8002671C. This function is the OTHER glyph source -- the
       draw-time one, used by func_xk1_800262F4 for strings that were never
       collected into the precomputed table -- and it was left behind when the
       ANK font support landed: it resolved every code through LeoGetKAdr, which
       rejects anything below 0x8140, so every ASCII glyph zero-filled its cell.
       The advance was already proportional, so the text was not merely
       mis-spaced, it was INVISIBLE: correctly positioned blank cells. That is
       what every Create Machine confirmation prompt drew (ABC40.c
       func_xk1_8002ECE8 -> here) on the English disk. */
    {
        extern unsigned char* gdx_ddipl_buffer;
        extern unsigned int gdx_ddipl_size;
        u8* dst = (sGdxDynGlyphs != NULL) ? (sGdxDynGlyphs + sGdxDynCursor * 0x80) : NULL;

        if (dst == NULL) {
            /* Ring not allocated yet (func_xk1_80025F98 has not run): nothing can
               be drawn, and returning here keeps the caller from emitting a
               texture load against a null base. */
            return gfx;
        }
        if (GDX_GLYPH_IS_ANK((u16)code)) {
            GdxFillAnkCell(dst, code - 0x20);
        } else if (gdx_ddipl_buffer != NULL && fontAddr >= DDROM_FONT_START &&
                   (u32)fontAddr + 0x80 <= gdx_ddipl_size) {
            bcopy(gdx_ddipl_buffer + fontAddr, dst, 0x80);
        } else {
            bzero(dst, 0x80);
        }
    }
#endif

#ifdef PORT
    /* Load from this path's own ring rather than from D_xk1_8003A488 + 0x490:
       that cursor is only rewound by course_edit, so on every other screen it
       runs off the end of the Arena block (see GDX_DYN_GLYPH_CELLS). */
    gDPLoadTextureBlock_4b(gfx++, sGdxDynGlyphs + sGdxDynCursor * 0x80, G_IM_FMT_I, 16, 16, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    sGdxDynCursor++;
    if (sGdxDynCursor >= GDX_DYN_GLYPH_CELLS) {
        sGdxDynCursor = 0;
    }
#else
    gDPLoadTextureBlock_4b(gfx++, D_xk1_8003A488 + D_xk1_8003A490, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
#endif

#ifdef PORT
    /* Clip the rect to the glyph's own width: ANK cells are left-aligned inside
       the 16x16 tile, so a fullwidth 16 here would drag in the neighbouring
       blank columns and reintroduce the fixed-pitch spacing. */
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + GdxGlyphAdvanceForCode((u16)code)) << 2,
                        (arg2 + 16) << 2, 0, 0, 0, 1 << 10, 1 << 10);
#else
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + 16) << 2, (arg2 + 16) << 2, 0, 0, 0, 1 << 10, 1 << 10);
#endif

    D_xk1_8003A490 += 0x80;
    return gfx;
}

extern volatile u8 D_80794E14;

Gfx* func_xk1_800262F4(Gfx* gfx, s32 arg1, s32 arg2, u8* arg3) {
    u8* var_s0;
    u8* var_s1;
    s32 i;

    if (D_80794E14) {
        return gfx;
    }

    i = 0;
    var_s0 = arg3;
    while (var_s0[i] != '\0') {
#ifdef PORT
        u8* cursor = &var_s0[i];
        u16 code = GdxReadGlyphCode(&cursor);

        gfx = func_xk1_800260F0(gfx, arg1, arg2, code);
        i = (s32)(cursor - var_s0);
        arg1 += GdxGlyphAdvanceForCode(code);
#else
        gfx = func_xk1_800260F0(gfx, arg1, arg2, (var_s0[i] << 8) + var_s0[i + 1]);
        i += 2;
        arg1 += 16;
#endif
    }

    return gfx;
}

Gfx* func_xk1_800263B0(Gfx* gfx, s32 arg1, s32 arg2, s32 arg3) {

#ifdef PORT
    /* arg3 is a buffer OFFSET on the port (see func_xk1_80025ED4): resolve against the
       untruncated host base at draw time. */
    gDPLoadTextureBlock_4b(gfx++, sGdxGlyphBufferHost + (u32)arg3, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
#else
    gDPLoadTextureBlock_4b(gfx++, arg3, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
#endif

#ifdef PORT
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + GdxGlyphAdvanceForOffset(arg3)) << 2, (arg2 + 16) << 2, 0,
                        0, 0, 1 << 10, 1 << 10);
#else
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + 16) << 2, (arg2 + 16) << 2, 0, 0, 0, 1 << 10, 1 << 10);
#endif

    return gfx;
}

Gfx* func_xk1_800264C0(Gfx* gfx, s32 arg1, s32 arg2, s32 arg3) {
    s32* var_s1;
    s8* var_s0;
    Gfx* tempGfx;

    var_s1 = D_xk1_8003A498[arg3];
    var_s0 = D_xk1_800331F0[arg3];

    while (*var_s0 != 0) {
        if (*var_s0 == 0xA) {
            break;
        }

#ifdef PORT
        {
            s32 glyphOffset = *var_s1++;

            tempGfx = func_xk1_800263B0(gfx, arg1, arg2, glyphOffset);
            arg1 += GdxGlyphAdvanceForOffset(glyphOffset);
        }
#else
        tempGfx = func_xk1_800263B0(gfx, arg1, arg2, *var_s1++);
        arg1 += 16;
#endif
        gfx = tempGfx;
#ifdef PORT
        var_s0 = GdxGlyphStringStep(var_s0);
#else
        var_s0 += 2;
#endif
    }
    return gfx;
}

Gfx* func_xk1_8002656C(Gfx* gfx, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    Gfx* tempGfx;
    s32* var_s1;
    s8* var_s0;
    s32 i;

    var_s1 = D_xk1_8003A498[arg3];
    var_s0 = D_xk1_800331F0[arg3];

    for (i = 0; i < arg4; i++) {
        while (*var_s0 != 0) {
            if (*var_s0 == 0xA) {
                var_s0 += 1;
                break;
            }
            var_s1++;
#ifdef PORT
            var_s0 = GdxGlyphStringStep(var_s0);
#else
            var_s0 += 2;
#endif
        }
    }

    while (*var_s0 != 0) {
        if (*var_s0 == 0xA) {
            break;
        }

#ifdef PORT
        {
            s32 glyphOffset = *var_s1++;

            tempGfx = func_xk1_800263B0(gfx, arg1, arg2, glyphOffset);
            arg1 += GdxGlyphAdvanceForOffset(glyphOffset);
        }
#else
        tempGfx = func_xk1_800263B0(gfx, arg1, arg2, *var_s1++);

        arg1 += 16;
#endif
        gfx = tempGfx;
#ifdef PORT
        var_s0 = GdxGlyphStringStep(var_s0);
#else
        var_s0 += 2;
#endif
    }
    return gfx;
}

Gfx* func_xk1_80026670(Gfx* gfx, s32 arg1, s32 arg2, s32 arg3) {
    Gfx* tempGfx;
    s32* var_s1;
    s8* var_s0;

    var_s1 = D_xk1_8003A518[arg3];
    var_s0 = D_xk1_8003339C[arg3];

    while (*var_s0 != 0) {
        if (*var_s0 == 0xA) {
            break;
        }

#ifdef PORT
        {
            s32 glyphOffset = *var_s1++;

            tempGfx = func_xk1_800263B0(gfx, arg1, arg2, glyphOffset);
            arg1 += GdxGlyphAdvanceForOffset(glyphOffset);
        }
#else
        tempGfx = func_xk1_800263B0(gfx, arg1, arg2, *var_s1++);
        arg1 += 16;
#endif
        gfx = tempGfx;
#ifdef PORT
        var_s0 = GdxGlyphStringStep(var_s0);
#else
        var_s0 += 2;
#endif
    }
    return gfx;
}

void func_xk1_8002671C(s32 code) {
    s32 fontAddr = LeoGetKAdr(code) + DDROM_FONT_START;

    D_xk1_8003A470.hdr.pri = 0;
    D_xk1_8003A470.hdr.retQueue = &gDmaMesgQueue;
    D_xk1_8003A470.dramAddr = D_xk1_8003A488 + D_xk1_8003A490;
    D_xk1_8003A470.devAddr = fontAddr;
    D_xk1_8003A470.size = 0x80;
#ifndef PORT
    gDriveRomHandle->transferInfo.cmdType = LEO_CMD_TYPE_2;
    func_80768B88(gDriveRomHandle, &D_xk1_8003A470, OS_READ);
    osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
#else
    /* Same as func_xk1_800260F0: serve the glyph from the IPL ROM image.
       ASCII comes from the halfwidth ANK font instead, blitted into the same
       16x16 cell, and both kinds record their advance for the draw side. */
    {
        extern unsigned char* gdx_ddipl_buffer;
        extern unsigned int gdx_ddipl_size;
        u8* dst = sGdxGlyphBufferHost + D_xk1_8003A490;
        s32 index = (D_xk1_8003A490 - 0xE00) >> 7;
        s32 advance = 16;

        if (sGdxGlyphBufferHost == NULL) {
            /* glyph buffer not allocated yet; nothing to fill */
        } else if (GDX_GLYPH_IS_ANK((u16)code)) {
            GdxFillAnkCell(dst, code - 0x20);
            advance = GdxGlyphAdvanceForCode((u16)code);
        } else if (gdx_ddipl_buffer != NULL && fontAddr >= DDROM_FONT_START &&
                   (u32)fontAddr + 0x80 <= gdx_ddipl_size) {
            bcopy(gdx_ddipl_buffer + fontAddr, dst, 0x80);
        } else {
            bzero(dst, 0x80);
        }

        if (sGdxGlyphAdvance != NULL && index >= 0 && index < D_xk1_8003A494) {
            sGdxGlyphAdvance[index] = (u8)advance;
        }
    }
#endif
    D_xk1_8003A490 += 0x80;
}

void func_xk1_800267C4(u16* arg0) {
    s32 i;

    for (i = 0; i < D_xk1_8003A494; i++) {
        func_xk1_8002671C(arg0[i]);
    }

#ifdef PORT
    /* Prefill verdict: how many of the unique glyph codes this session needs
       actually resolve to a font-block offset. A nonzero "rejected" count means
       GdxReadGlyphCode is emitting codes LeoGetKAdr does not accept, which is
       silent on screen (the cell is just zero filled) and was exactly the
       failure mode of the earlier JIS-instead-of-Shift-JIS mapping. */
    {
        extern void gdx_dbg_logf(const char* fmt, ...);
        s32 resolved = 0;
        s32 ank = 0;
        s32 advSum = 0;

        for (i = 0; i < D_xk1_8003A494; i++) {
            if (GDX_GLYPH_IS_ANK(arg0[i])) {
                ank++;
                if (LeoGetAAdr((s32)arg0[i] - 0x20, NULL, NULL, NULL) >= 0) {
                    resolved++;
                }
            } else if (LeoGetKAdr(arg0[i]) >= 0) {
                resolved++;
            }
            if (sGdxGlyphAdvance != NULL) {
                advSum += sGdxGlyphAdvance[i];
            }
        }
        gdx_dbg_logf("[ek-glyph] codes: %d resolved, %d rejected (of %d unique; %d ANK halfwidth, "
                     "%d fullwidth), mean advance %d px\n",
                     resolved, D_xk1_8003A494 - resolved, D_xk1_8003A494, ank, D_xk1_8003A494 - ank,
                     (D_xk1_8003A494 > 0) ? (advSum / D_xk1_8003A494) : 0);
    }
#endif
}
