#include "global.h"
#include "leo/leo_internal.h"

#ifdef PORT
/* Untruncated host pointer for the glyph buffer: D_xk1_8003A488 is an s32 and
   truncates the 64-bit Arena pointer. */
static u8* sGdxGlyphBufferHost;

/* Glyph-code normalization for this file's string walkers, which were written for
   two-byte text and pair consecutive bytes into one code.

   ASCII (the transplanted English strings in expansion_kit_text.c) pairs into
   codes LeoGetKAdr rejects, so the dialog renders blank; read one glyph per call
   instead. Untranslated strings are still Japanese, stored by the decomp as
   EUC-JP escapes at 0xA1A1..0xFEFE -- above LeoGetKAdr's [0x8140, 0x9872] window,
   so also rejected. Codes must reach LeoGetKAdr (port/n64_leo.c) as Shift-JIS,
   never JIS X 0208; convert EUC->JIS->Shift-JIS by stripping the 0x8080 EUC bias
   and applying the standard JIS packing.

   Discriminating by lead byte is safe here: 0x81-0x9F is unambiguously a
   Shift-JIS lead (EUC leads start at 0xA1), which is what the translated strings
   embed literally as "\x81\x40" for the inline help-icon slot. The 0xE0-0xFC
   overlap between the two encodings is why this normalization lives at a walker
   whose input is known to be decomp C-source text, rather than inside LeoGetKAdr
   where a genuine Shift-JIS caller in that range would be corrupted. */
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

/* Advance width. Every EK panel sizes its box as `mfsStrLen(str) * 8` -- eight
   pixels per BYTE (mfsStrLen in leo/mfs/mfs_device.c is a plain byte counter) --
   while the walkers here advance a fixed 16 pixels per GLYPH. Two-byte JP text
   makes those identical, which is why hardware never showed a defect; one-byte
   ASCII draws twice the width its box reserved.

   ASCII therefore renders from the drive ROM's halfwidth ANK font (LeoGetAAdr),
   not from a fullwidth kanji lookalike. Its layout, decoded offline against both
   ROM images: index is `ascii - 0x20`; cells are I4 packed at ceil(dx/2) bytes
   per row over dy rows, rounded up to a 2-byte boundary (LeoGetAAdr returns
   `off << 1`); `cy` counts rows ABOVE the baseline, so the cell-relative top is
   `GDX_ANK_BASELINE - cy`.

   Codes therefore stay in two disjoint ranges: below 0x100 is an ASCII character
   served by the ANK font, at or above is a Shift-JIS code for the fullwidth kanji
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

/* Advance the string cursor by exactly the bytes one glyph consumed, so the
   draw-side iterators keep the precomputed offset table (walked with var_s1++)
   in step with the string. */
static s8* GdxGlyphStringStep(s8* p) {
    u8* cursor = (u8*)p;

    GdxReadGlyphCode(&cursor);
    return (s8*)cursor;
}

/* Pixel advance per unique glyph, indexed in parallel with D_xk1_8003A54C. */
static u8* sGdxGlyphAdvance;

/* Scratch cells for func_xk1_800260F0, the draw-time (not precomputed) glyph
   path. Retail runs it out of the first 0xE00 bytes of the precomputed buffer,
   bumping D_xk1_8003A490 by 0x80 per glyph and relying on func_xk1_800260E4 to
   rewind -- which only course_edit calls (191080.c:218). Machine Create draws its
   confirmation prompts through the same path (ABC40.c func_xk1_8002ECE8 ->
   func_xk1_800262F4) and never rewinds, so the cursor walks off the end of the
   Arena block: a silent DMA destination on hardware, a stray memcpy here.

   Hence a private ring. 160 cells is sized against the longest prompt rather than
   the average one, because wrapping inside a frame would reuse a cell the display
   list still points at. */
#define GDX_DYN_GLYPH_CELLS 160
static u8* sGdxDynGlyphs;
static s32 sGdxDynCursor;

/* Blit one variable-size ANK glyph into a fixed 16x16 I4 cell. Keeping the cell
   16x16 leaves every gDPLoadTextureBlock_4b in this file untouched -- only the
   destination rect width and the cursor advance become per-glyph -- and the
   widest ANK glyph (13px over 16 rows, 112 bytes) still fits the 0x80-byte cell
   the arena already reserves. */
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
        /* Only '$' (cy=12) reaches here, by one row: no cell height satisfies both
           the tallest ascender and the deepest descender. Clamp rather than shift
           the baseline, so every other glyph stays aligned. */
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

/* Defined below with the file's other globals; needed here to bound the lookup. */
extern s32 D_xk1_8003A494;

/* Offsets are laid out by func_xk1_80025ED4 as (index << 7) + 0xE00. */
static s32 GdxGlyphAdvanceForOffset(s32 offset) {
    s32 index = (offset - 0xE00) >> 7;

    if (sGdxGlyphAdvance == NULL || index < 0 || index >= D_xk1_8003A494) {
        return 16;
    }
    return sGdxGlyphAdvance[index];
}

/* Variant for func_xk1_800262F4, which draws straight from the code with no
   offset table. */
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

/* Drawn width of a one-line EK string, using the same per-glyph advances
   func_xk1_800262F4 places it with. Callers that centre a string need this
   because ASCII is proportional and the retail box widths, sized for a flat 16px
   per glyph, leave the fan-translated English hard against the left edge. Stops
   at a newline; ABC40.c's prompts are single-line. */
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

/* Locate the run of fullwidth spaces a string reserves for an inline icon (the
   help tooltip embeds "\x81\x40\x81\x40" for the question icon). The caller,
   course_edit/19FA50.c, used to hardcode x=128 -- correct only while every glyph
   was a fixed 16px; proportional ASCII moves the hole, so derive it from the same
   advance logic that placed the text.

   Returns the pixel offset of the run from the start of the first line, its width
   in *gapWidth, or -1 when the string reserves no such hole. */
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
            /* The table is s32-typed and cannot hold a 64-bit host address: store the
               OFFSET and let the draw side (func_xk1_800263B0) add sGdxGlyphBufferHost.
               Storing base+offset here truncated the pointer, and the precomputed-string
               glyph loads sampled garbage. */
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
        /* Arena memory is not zeroed on the port (Arena_Allocate only bumps a
           pointer) and the hardware DMA that would have overwritten it is now a
           direct copy from the IPL image, so clear it here. buf also keeps the
           full pointer the s32 assignment below truncates. */
        void* buf = (void*)Arena_Allocate(ALLOC_FRONT, (D_xk1_8003A494 << 7) + 0xE00);
        extern unsigned char* gdx_ddipl_buffer;
        extern unsigned int gdx_ddipl_size;
        extern void gdx_dbg_logf(const char* fmt, ...);
        sGdxGlyphBufferHost = (u8*)buf;
        bzero(buf, (D_xk1_8003A494 << 7) + 0xE00);
        /* Preset to the fullwidth default so a glyph func_xk1_8002671C never fills
           still reads back sanely. */
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
        /* Probe: the prefill runs once, so an IPL image not loaded by this point
           leaves every glyph blank for the session. */
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
    /* Glyphs live in the 64DD drive's internal ROM; copy from the user-supplied
       IPL ROM image (port/disk_buffer.cpp). Invalid codes (LeoGetKAdr < 0 =>
       fontAddr < DDROM_FONT_START) fall back to a blank glyph.

       ASCII must take the halfwidth ANK branch here, exactly as the prefill does
       in func_xk1_8002671C: this is the other glyph source, the draw-time one
       used by func_xk1_800262F4 for strings never collected into the precomputed
       table. Resolving ASCII through LeoGetKAdr zero-fills the cell while the
       advance stays proportional, so the text is not mis-spaced but invisible --
       correctly positioned blank cells. */
    {
        extern unsigned char* gdx_ddipl_buffer;
        extern unsigned int gdx_ddipl_size;
        u8* dst = (sGdxDynGlyphs != NULL) ? (sGdxDynGlyphs + sGdxDynCursor * 0x80) : NULL;

        if (dst == NULL) {
            /* Ring not allocated yet (func_xk1_80025F98 has not run): returning
               keeps the caller from emitting a texture load against a null base. */
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
    /* This path's own ring, not D_xk1_8003A488 + D_xk1_8003A490: only course_edit
       rewinds that cursor (see GDX_DYN_GLYPH_CELLS). */
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
    /* ANK cells are left-aligned inside the 16x16 tile, so a fullwidth 16 here
       would drag in the blank columns and reintroduce fixed-pitch spacing. */
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
    /* Same as func_xk1_800260F0: serve the glyph from the IPL ROM image, ASCII
       from the halfwidth ANK font into the same 16x16 cell. Both record their
       advance for the draw side. */
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
    /* Probe: a nonzero "rejected" count means GdxReadGlyphCode is emitting codes
       the font lookup will not accept -- silent on screen, the cell just
       zero-fills. */
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
