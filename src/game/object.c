#include "global.h"
#include "fzx_game.h"
#include "fzx_object.h"
#include "segment_symbols.h"

/* ROM data is big-endian N64. On little-endian x86 hosts, *(s32*) reads bytes reversed.
 * GDX_IS_MIO0 checks bytes individually — works on both big- and little-endian. */
#ifdef PORT
#define GDX_IS_MIO0(p) \
    (((const u8*)(p))[0] == 0x4Du && ((const u8*)(p))[1] == 0x49u && \
     ((const u8*)(p))[2] == 0x4Fu && ((const u8*)(p))[3] == 0x30u)
/* Read a big-endian u32 from any byte address (safe on little-endian hosts). */
#define GDX_READ_BE_U32(p) \
    (((u32)((const u8*)(p))[0] << 24) | ((u32)((const u8*)(p))[1] << 16) | \
     ((u32)((const u8*)(p))[2] << 8)  |  (u32)((const u8*)(p))[3])

/* Return the decoded byte count stored in a MIO0 header.  The original N64
 * routine is just `lw v0, 4(a0)` in the return delay slot; spell the load out
 * so the raw big-endian header has the same meaning on little-endian hosts. */
s32 func_800AA6BC(u8* header) {
    return (s32) GDX_READ_BE_U32(header + 4);
}
#else
#define GDX_IS_MIO0(p) (*(s32*)(p) == (s32)'MIO0')
#endif

unk_800E33E0 D_800E33E0[200];
s32 D_800E3A20;
Object gObjects[32];
unk_800E3F28 D_800E3F28[16];
unk_800E4068 D_800E4068[16];

#ifdef PORT
/* PORT: func_80077CF0 reads MIO0-compressed common assets from gdx_rom_buffer.
 * On N64, segAddr is a 32-bit segmented address (0x0F??????) and SEGMENT_OFFSET()
 * extracts the byte offset. On a 64-bit host, unk_04 is a full host pointer to a
 * 1-byte stub in AssetBindings.c — it must NOT be truncated to s32. We use void*
 * as the parameter type (64-bit on x64) and look up the correct ROM offset via the
 * generated table in AssetBindings.c. */
extern unsigned char* gdx_rom_buffer;
extern size_t         gdx_rom_size;
extern unsigned int gdx_lookup_common_asset_rom_offset(unsigned long long sym_addr);
extern const char* gdx_lookup_common_asset_o2r_key(unsigned long long sym_addr);
extern int GDiffuser_LoadAssetBytes(const char* key, void* out, size_t outSize, size_t* copiedSize);
extern void GDiffuser_RegisterLoadedAssetBuffer(const void* buffer, size_t size, const char* key);

extern void gdx_record_dma_load(unsigned int rdram_phys, unsigned int rom_offset, unsigned int size);
extern unsigned char* gdx_rdram;

/* Route the raw cartridge read through the single byte-source shim
 * (port/gdx_segment_source.{h,c}) instead of touching gdx_rom_buffer directly.
 * Forward-declared rather than #include'd -- this decomp TU's include path does
 * not carry port/ (same pattern as dma.c). Archive-first, byte-identical raw
 * fallback: the copied bytes equal the old memcpy whether served from the
 * common_assets_compressed blob (verbatim ROM slice) or the raw ROM. */
extern int GdxSegmentSourceRead(unsigned int romBase, unsigned int size, void* dst);

/* Renderer staleness tracking only sees recorded writes (HostRangeChanged,
   n64_gfx_bridge.cpp). The per-mode arena rewind reuses destination addresses
   across mode transitions, so every CPU asset copy into RDRAM must be
   recorded or the renderer keeps serving the previous mode's texture bytes. */
static void GDX_RecordAssetWrite(u8* startAddr, size_t size) {
    if (gdx_rdram != NULL && startAddr >= gdx_rdram &&
        startAddr < gdx_rdram + 0x1000000u /* GDX_RDRAM_SIZE */) {
        gdx_record_dma_load((unsigned int)(size_t)(startAddr - gdx_rdram), 0u, (unsigned int)size);
    }
}

static int GDX_TryLoadCommonAssetO2R(void* segAddr, size_t size, u8* startAddr) {
    size_t copiedSize = 0;
    const char* o2rKey = gdx_lookup_common_asset_o2r_key((unsigned long long)segAddr);
    if (o2rKey == NULL) {
        return 0;
    }
    if (GDiffuser_LoadAssetBytes(o2rKey, startAddr, size, &copiedSize)) {
        GDiffuser_RegisterLoadedAssetBuffer(startAddr, size, o2rKey);
        GDX_RecordAssetWrite(startAddr, size);
        static int sO2RLoadPrints = 0;
        if (sO2RLoadPrints < 32) {
            gdx_ck("[o2r] common asset fast path");
            gdx_cki("[o2r]  copied", (int)copiedSize);
            gdx_cki("[o2r]  capacity", (int)size);
            sO2RLoadPrints++;
        }
        return 1;
    }
    return 0;
}

/* Raw-ROM variant for STAGED reads: callers that stage compressed bytes for a
   later mio0Decode (func_80077D50_impl's 8-byte size probe + compressed
   payload) must NEVER be served by the o2r fast path — o2r stores the DECODED
   asset, so serving it into a staging buffer makes the size probe parse pixel
   data and fails the GDX_IS_MIO0 check, whose fallback bzero()s the final
   texture (the zero-fingerprint CI/IA sources in settimg-trace: heal strips,
   boost plates, position gadget). This variant skips o2r and always delivers
   raw cart bytes. */
static void GDX_LoadRawRomAsset(void* segAddr, size_t size, u8* startAddr);

void func_80077CF0(void* segAddr, size_t size, u8* startAddr) {
    if (GDX_TryLoadCommonAssetO2R(segAddr, size, startAddr)) {
        return;
    }

    GDX_LoadRawRomAsset(segAddr, size, startAddr);
}

static void GDX_LoadRawRomAsset(void* segAddr, size_t size, u8* startAddr) {
    unsigned int romOffset = gdx_lookup_common_asset_rom_offset((unsigned long long)segAddr);
    if (romOffset != 0) {
        /* Archive-first, same as Dma_RomCopy (dma.c): try the shim before ever
         * looking at gdx_rom_buffer. GdxSegmentSourceRead can serve this offset
         * out of the mounted archive with gdx_rom_buffer == NULL -- archive-only
         * boot is a legitimate configuration, not an error, so it must not be
         * short-circuited into a zero-fill before the archive is even consulted.
         * The OOB clamp below only makes sense relative to gdx_rom_size, so it
         * is only applied when a ROM is actually loaded; the shim enforces its
         * own bounds against the archive independently for the archive-only case. */
        if (gdx_rom_buffer != NULL && romOffset + size > gdx_rom_size) {
            gdx_ck("[rom] WARN: read past end of ROM — clamping");
            gdx_cki("[rom]  romOffset", (int)romOffset);
            gdx_cki("[rom]  size", (int)size);
            gdx_cki("[rom]  rom_size", (int)gdx_rom_size);
            size = gdx_rom_size - romOffset;
        }
        if (!GdxSegmentSourceRead(romOffset, (unsigned int)size, startAddr)) {
            /* Total miss: the symbol resolved to a ROM offset, but it is neither
             * in the mounted archive nor available from a loaded ROM (or the ROM
             * is loaded and the offset is out of range even after clamping).
             * This is the only path that should zero-fill and warn -- an
             * archive-only boot that successfully resolves the asset never
             * reaches here, and its miss telemetry lives here instead of on
             * every archive-only boot. */
            static int sRomMissLogs = 0;
            if (sRomMissLogs < 32) {
                sRomMissLogs++;
                gdx_ck("[rom] MISS: asset not found in archive and no ROM loaded (or offset out of range) — texture zero-filled. Set FZEROX_ROM env var or check archive contents.");
                gdx_cki("[rom]  romOffset", (int)romOffset);
                gdx_cki("[rom]  size", (int)size);
            }
            memset(startAddr, 0, size);
        }
        GDX_RecordAssetWrite(startAddr, size);
    } else {
        /* Symbol not in the generated common-asset binding table. This zero
           fill is what the renderer later samples (settimg-trace fp=0 sources:
           heal strips, boost plates, position gadget) — it must never be
           silent. Every line here names a binding the generator must cover. */
        {
            extern void gdx_ck(const char*);
            extern void gdx_ckp(const char*, void*);
            extern void gdx_cki(const char*, int);
            static int sAssetMissLogs = 0;
            if (sAssetMissLogs < 32) {
                sAssetMissLogs++;
                gdx_ck("[asset] MISS: common-asset lookup failed; texture zero-filled");
                gdx_ckp("[asset]  symbol", segAddr);
                gdx_cki("[asset]  size", (int)size);
            }
        }
        memset(startAddr, 0, size);
        GDX_RecordAssetWrite(startAddr, size);
    }
}
#else
void func_80077CF0(s32 segAddr, size_t size, u8* startAddr) {
    CLEAR_DATA_CACHE(startAddr, size);
#ifndef EXPANSION_KIT
    Dma_LoadAssets(SEGMENT_ROM_START(common_assets_compressed) + SEGMENT_OFFSET(segAddr), startAddr, size);
#else
    Dma_LoadAssets(gRomSegmentPairs[4][0] + SEGMENT_OFFSET(segAddr), startAddr, size);
#endif
}
#endif

void func_80077D44(void) {
    D_800E3A20 = 0;
}

#ifdef PORT
/* PORT: texture-registry overflow guard + occupancy probe.
 *
 * D_800E33E0 (object.c:27) is a FIXED 200-entry array, and the three registration
 * sites in this file -- func_80077D50_impl, func_i2_800AE578 and func_80078104 --
 * each wrote D_800E33E0[D_800E3A20] and incremented with NO bound of any kind.
 *
 * Registration is CUMULATIVE and the count is only reset by func_80077D44(),
 * reached only from the mode-change tick (game.c:735) and machine_create.c:23 --
 * effectively only on a gGameMode CHANGE. Two paths grow it without one:
 *   * ovl_i2/font.c:1795 -- Font_DrawString re-enters func_80077D50_impl for
 *     EVERY glyph EVERY frame and lazily registers any glyph not present. That
 *     path never calls func_800783AC, so lazy registration is INVISIBLE to the
 *     [reg-miss] probe in that function.
 *   * course_edit/19DD60.c:338-345 -- a Course Edit "Test Course" run re-inits
 *     the race IN PLACE with no mode change, so a whole editing run accumulates
 *     into one registry generation.
 *
 * Past slot 199 the writes land on whatever the linker placed after the array: in
 * this TU that is D_800E3A20 itself (the count, object.c:28) and gObjects[32]
 * (object.c:29). A corrupted count then makes func_800783AC scan far past the
 * array and return arbitrary bytes as a TexturePtr for SOME symbols while others
 * still resolve -- and it never trips [reg-miss], because a non-NULL garbage
 * pointer is indistinguishable from a hit there.
 *
 * Refusing the entry is strictly better than the OOB write: func_800783AC then
 * returns NULL and func_80078EA0_impl (object.c:737) skips that draw, so the
 * worst case is a MISSING glyph instead of corrupted adjacent state. The refusal
 * is loud and always-on (bounded to 16 lines) because it means an upstream reset
 * is missing, not because it is an expected condition.
 *
 * Non-PORT keeps the original unguarded three lines verbatim at all three sites. */
extern int gdx_dev_gate_diag_texreg(void);
extern void gdx_dbg_logf(const char* fmt, ...);

#define GDX_TEXREG_CAPACITY ((s32) (sizeof(D_800E33E0) / sizeof(D_800E33E0[0])))

static bool GDX_TexRegistryReserve(void* symbol) {
    static s32 sHighWater = 0;
    static s32 sOverflowLogs = 0;

    if ((D_800E3A20 < 0) || (D_800E3A20 >= GDX_TEXREG_CAPACITY)) {
        if (sOverflowLogs < 16) {
            sOverflowLogs++;
            gdx_dbg_logf(
                "[texreg] OVERFLOW: texture registry full; refusing to register (this texture will not draw)\n");
            gdx_dbg_logf("[texreg]  symbol=%p\n", symbol);
            gdx_dbg_logf("[texreg]  count=%d (0x%x)\n", (int) D_800E3A20, (unsigned) D_800E3A20);
            gdx_dbg_logf("[texreg]  capacity=%d (0x%x)\n", (int) GDX_TEXREG_CAPACITY,
                         (unsigned) GDX_TEXREG_CAPACITY);
        }
        return false;
    }

    if (D_800E3A20 >= sHighWater) {
        sHighWater = D_800E3A20 + 1;
        if (gdx_dev_gate_diag_texreg()) {
            gdx_dbg_logf("[texreg] high-water=%d (0x%x)\n", (int) sHighWater, (unsigned) sHighWater);
        }
    }
    return true;
}
#endif

extern uintptr_t gArenaStartPtrs[];

u8* func_80077D50_impl(unk_80077D50* arg0, s32 arg1, bool arg2) {
    bool var_a0;
    s32 var_s0;
    s32 alignedWidth;
    s32 var_s2;
    size_t textureSize;
    u8* header;
    u8* sp44;
    u8* var_s4;
    bool var_s7;
    unk_800E33E0* var_s8 = D_800E33E0;

    sp44 = gArenaStartPtrs[0];
    var_s7 = false;

    while (arg0->unk_04 != 0) {
        var_a0 = false;
        if (arg1 == 0) {
            for (var_s0 = 0; var_s0 < D_800E3A20; var_s0++) {
                // FAKE
                if (D_800E33E0[var_s0].unk_00 == (0, arg0->unk_04)) {
                    var_a0 = true;
                    break;
                }
            }
        }
        if (!var_a0) {
            switch (arg0->unk_00) {
                case 4:
                case 5:
                    if (arg0->width % 16) {
                        alignedWidth = ((arg0->width + 16) / 16) * 16;
                    } else {
                        alignedWidth = arg0->width;
                    }
                    textureSize = arg0->height * alignedWidth;
                    var_s4 = Arena_Allocate(ALLOC_FRONT, textureSize);
#ifdef PORT
                    if (GDX_TryLoadCommonAssetO2R(arg0->unk_04, textureSize, var_s4)) {
                        break;
                    }
#endif
                    func_80077CF0(arg0->unk_04, textureSize, var_s4);
                    break;
                case 20:
                case 21:

                    if (arg0->width % 16) {
                        alignedWidth = ((arg0->width + 16) / 16) * 16;
                    } else {
                        alignedWidth = arg0->width;
                    }
                    textureSize = arg0->height * alignedWidth;
                    if (arg0->compressedSize != 0) {
                        var_s2 = ALIGN_2(arg0->compressedSize) + 2;
                    } else {
                        var_s2 = 0x400;
                    }
                    var_s4 = Arena_Allocate(ALLOC_FRONT, textureSize);
#ifdef PORT
                    if (GDX_TryLoadCommonAssetO2R(arg0->unk_04, textureSize, var_s4)) {
                        break;
                    }
#endif
                    header = Arena_Allocate(ALLOC_PEEK, var_s2);
                    CLEAR_DATA_CACHE(header, var_s2);
                    GDX_LoadRawRomAsset(arg0->unk_04, var_s2, header); /* staging: raw MIO0 bytes required */
                    if (GDX_IS_MIO0(header)) {
#ifdef PORT
                        {
                            u32 mioDestSize = GDX_READ_BE_U32((const u8*)header + 4);
                            u32 allocSize   = (u32)textureSize;
                            if (mioDestSize > allocSize) {
                                gdx_ck("[mio0] OVERFLOW PREVENTED in case20/21");
                                gdx_cki("[mio0]  dest_size", (int)mioDestSize);
                                gdx_cki("[mio0]  alloc_size", (int)allocSize);
                                bzero(var_s4, allocSize);
                            } else {
                                mio0Decode(header, var_s4);
                            }
                        }
#else
                        mio0Decode(header, var_s4);
#endif
                    } else {
                        bzero(var_s4, (arg0->height * alignedWidth) / 2);
                    }
                    break;
                case 17:
                case 18:
                    if (arg0->compressedSize != 0) {
                        var_s0 = ALIGN_2(arg0->compressedSize) + 2;
                    } else {
                        var_s0 = 0x400;
                    }

                    var_s4 = Arena_Allocate(ALLOC_FRONT, arg0->height * arg0->width * 2);
#ifdef PORT
                    if (GDX_TryLoadCommonAssetO2R(arg0->unk_04, arg0->height * arg0->width * 2, var_s4)) {
                        break;
                    }
#endif
                    header = Arena_Allocate(ALLOC_PEEK, var_s0);
                    CLEAR_DATA_CACHE(header, var_s0);
                    GDX_LoadRawRomAsset(arg0->unk_04, var_s0, header); /* staging: raw MIO0 bytes required */
                    if (GDX_IS_MIO0(header)) {
#ifdef PORT
                        {
                            /* MIO0 header bytes 4-7 (big-endian) = decompressed size.
                             * Overflow into the heap if dest_size > alloc prevents crash. */
                            u32 mioDestSize = GDX_READ_BE_U32((const u8*)header + 4);
                            u32 allocSize   = (u32)(arg0->height * arg0->width * 2);
                            if (mioDestSize > allocSize) {
                                gdx_ck("[mio0] OVERFLOW PREVENTED in case17/18");
                                gdx_cki("[mio0]  dest_size", (int)mioDestSize);
                                gdx_cki("[mio0]  alloc_size", (int)allocSize);
                                gdx_cki("[mio0]  w", arg0->width);
                                gdx_cki("[mio0]  h", arg0->height);
                                bzero(var_s4, allocSize);
                            } else {
                                mio0Decode(header, var_s4);
                            }
                        }
#else
                        mio0Decode(header, var_s4);
#endif
                    } else {
                        bzero(var_s4, arg0->height * arg0->width * 2);
                    }
                    break;
                default:
                    var_s0 = arg0->height * arg0->width * 2;
                    var_s4 = Arena_Allocate(ALLOC_FRONT, var_s0);
                    func_80077CF0(arg0->unk_04, var_s0, var_s4);
                    break;
            }
            if (!var_s7) {
                sp44 = var_s4;
                var_s7 = true;
            }
#ifdef PORT
            /* Overflow guard — see GDX_TexRegistryReserve above. */
            if (GDX_TexRegistryReserve(arg0->unk_04)) {
                var_s8[D_800E3A20].unk_00 = arg0->unk_04;
                var_s8[D_800E3A20].unk_04 = var_s4;
                D_800E3A20++;
            }
#else
            var_s8[D_800E3A20].unk_00 = arg0->unk_04;
            var_s8[D_800E3A20].unk_04 = var_s4;
            D_800E3A20++;
#endif
        }

#ifdef EXPANSION_KIT
        if (arg2) {
            break;
        }
#endif

        arg0++;
    }

    return sp44;
}

#ifdef EXPANSION_KIT
u8* func_i2_800AE578(unk_80077D50* arg0, bool arg1) {
    s32 temp_s0;
    bool var_a0;
    s32 var_s0;
    bool var_s6;
    u8* header;
    u8* var_fp;
    u8* var_s3;
    unk_800E33E0* var_s7 = D_800E33E0;

    var_s6 = false;
    var_fp = gArenaStartPtrs[0];

    while (arg0->unk_04 != NULL) {
        var_a0 = false;
        if (!arg1) {
            for (var_s0 = 0; var_s0 < D_800E3A20; var_s0++) {
                if (D_800E33E0[var_s0].unk_00 == (0, arg0->unk_04)) {
                    var_a0 = true;
                    break;
                }
            }
        }
        if (!var_a0) {
            switch (arg0->unk_00) {
                case 17:
                case 18:
                    if (arg0->compressedSize != 0) {
                        var_s0 = ((arg0->compressedSize >> 1) * 2) + 2;
                    } else {
                        var_s0 = 0x400;
                    }
                    var_s3 = Arena_Allocate(ALLOC_FRONT, arg0->height * arg0->width * 2);
                    header = Arena_Allocate(ALLOC_PEEK, var_s0);
                    CLEAR_DATA_CACHE(header, var_s0);
#ifdef PORT
                    /* The five callers of this function -- title Copyright
                       (aCopyrightDDTex), the 64DD logo (aN64DDLogoTex), the
                       credits copyright, the machine-select trophies
                       (aNovice/Standard/Expert/MasterDDTrophyTex) and the
                       course-select DD cup logos (aCupSelectDD1Tex/DD2Tex) --
                       pass arg0->unk_04 pointers that are REAL, full-sized host
                       arrays populated directly FROM THE DISK by
                       gdx_ek_assets_fill (port/gen/EkAssetBindings.c). Their fill
                       rows carry n64Address=0, so they are never registered as
                       segmented cart addresses AND they are absent from the
                       common-asset (o2r) table.

                       Do NOT route these through a common-asset lookup: it can
                       never hit for a disk-filled caller (fill mechanism, not the
                       o2r store), and the miss zero-filled the texture, rendering
                       the copyright, logo, trophies and cup logos invisible.
                       These pointers are exactly what the non-PORT #else path
                       bcopy's from, so read the raw MIO0 blob DIRECTLY from the
                       host array. var_s0 tracks the MIO0 blob size
                       (compressedSize), which is <= the fill array size
                       (EkAssetBindings.c diskLen), so the copy stays in bounds --
                       the same bound the #else path relies on. The GDX_IS_MIO0
                       check below fails safe to bzero (with an [asset] trace) if
                       the disk fill never ran and the array is still all-zero. */
#endif
                    bcopy(arg0->unk_04, header, var_s0);
                    if (GDX_IS_MIO0(header)) {
#ifdef PORT
                        {
                            u32 mioDestSize = GDX_READ_BE_U32((const u8*)header + 4);
                            u32 allocSize   = (u32)(arg0->height * arg0->width * 2);
                            if (mioDestSize > allocSize) {
                                gdx_ck("[mio0] OVERFLOW PREVENTED in i2 case17/18");
                                gdx_cki("[mio0]  dest_size", (int)mioDestSize);
                                gdx_cki("[mio0]  alloc_size", (int)allocSize);
                                bzero(var_s3, allocSize);
                            } else {
                                mio0Decode(header, var_s3);
                            }
                        }
#else
                        mio0Decode(header, var_s3);
#endif
                    } else {
#ifdef PORT
                        /* No MIO0 magic in the host array -> gdx_ek_assets_fill
                           never populated it (disk-less boot). Fail safe to the
                           zero-fill below instead of decoding garbage, and trace
                           it once so a missing disk image is diagnosable rather
                           than a silently invisible texture. */
                        {
                            extern void gdx_ck(const char*);
                            extern void gdx_ckp(const char*, void*);
                            static int sEkFillMissLogs = 0;
                            if (sEkFillMissLogs < 8) {
                                sEkFillMissLogs++;
                                gdx_ck("[asset] EK disk-fill texture missing MIO0 magic; zero-filled (no disk image?)");
                                gdx_ckp("[asset]  host array", arg0->unk_04);
                            }
                        }
#endif
                        bzero(var_s3, arg0->height * arg0->width * 2);
                    }
                    break;
                default:
                    var_s0 = arg0->height * arg0->width * 2;
                    var_s3 = Arena_Allocate(ALLOC_FRONT, var_s0);
                    func_80077CF0(arg0->unk_04, var_s0, var_s3);
                    break;
            }
            if (!var_s6) {
                var_fp = var_s3;
                var_s6 = true;
            }
#ifdef PORT
            /* Overflow guard — see GDX_TexRegistryReserve above. */
            if (GDX_TexRegistryReserve(arg0->unk_04)) {
                var_s7[D_800E3A20].unk_00 = arg0->unk_04;
                var_s7[D_800E3A20].unk_04 = var_s3;
                D_800E3A20++;
            }
#else
            var_s7[D_800E3A20].unk_00 = arg0->unk_04;
            var_s7[D_800E3A20].unk_04 = var_s3;
            D_800E3A20++;
#endif
        }
        arg0++;
    }
    return var_fp;
}
#endif

void* func_80078104(void* arg0, s32 textureSize, s32 arg2, s32 arg3, bool arg4) {
    s32 var_a3;
    bool var_t0;
    u8* var_s0;
    u8* var_a2;
    s32 sp24;
    unk_800E33E0* var_v1 = D_800E33E0;

    var_t0 = false;
    if ((arg2 == 0) && !arg4) {
        for (var_a3 = 0; var_a3 < D_800E3A20; var_a3++) {
            if (D_800E33E0[var_a3].unk_00 == arg0) {
                var_t0 = true;
                break;
            }
        }
    }
    if (!var_t0 || arg4) {
        if (arg3 == 0) {
            if (!arg4) {
                var_s0 = Arena_Allocate(ALLOC_FRONT, textureSize);
            } else {
                var_s0 = Arena_Allocate(ALLOC_PEEK, textureSize);
            }
            func_80077CF0(arg0, textureSize, var_s0);
        } else {
            var_s0 = Arena_Allocate(ALLOC_PEEK, 8);
            GDX_LoadRawRomAsset(arg0, 8, var_s0); /* staging: raw MIO0 size probe */
            var_a3 = func_800AA6BC(var_s0);

            if (!arg4) {
                var_s0 = Arena_Allocate(ALLOC_FRONT, var_a3);
                var_a2 = Arena_Allocate(ALLOC_PEEK, textureSize);
            } else {
                if (var_a3 % 16) {
                    var_a3 = ((var_a3 / 16) * 16) + 16;
                }
                if (textureSize % 16) {
                    textureSize = ((textureSize / 16) * 16) + 16;
                }

                sp24 = textureSize + var_a3;
                var_s0 = Arena_Allocate(ALLOC_PEEK, sp24);
                CLEAR_DATA_CACHE(var_s0, sp24);
                var_a2 = var_s0 + var_a3;
            }

            CLEAR_DATA_CACHE(var_a2, textureSize);
            GDX_LoadRawRomAsset(arg0, textureSize, var_a2); /* staging: raw MIO0 bytes required */
            if (GDX_IS_MIO0(var_a2)) {
                mio0Decode(var_a2, var_s0);
            } else {
                bzero(var_s0, var_a3);
            }
        }
        if (!arg4) {
#ifdef PORT
            /* Overflow guard — see GDX_TexRegistryReserve above. */
            if (GDX_TexRegistryReserve(arg0)) {
                var_v1[D_800E3A20].unk_00 = arg0;
                var_v1[D_800E3A20].unk_04 = var_s0;
                D_800E3A20++;
            }
#else
            var_v1[D_800E3A20].unk_00 = arg0;
            var_v1[D_800E3A20].unk_04 = var_s0;
            D_800E3A20++;
#endif
        }
    } else {
        // FAKE
        return (var_v1 + var_a3)->unk_04;
    }
    return var_s0;
}

TexturePtr func_800783AC(void* arg0) {
    s32 i;
#ifdef PORT
    /* Read-side companion to the write guard (GDX_TexRegistryReserve above): scan
       at most the array's real extent. Normally inert, since the write guard keeps
       D_800E3A20 inside [0, 200]. It exists so that a count smashed from outside
       this file makes the loop read garbage *inside* the array rather than walk
       off it: the symbol then misses and trips [reg-miss] instead of returning a
       garbage non-NULL TexturePtr, which renders as correct-looking-but-wrong
       glyph pixels while leaving every probe silent. */
    s32 count = D_800E3A20;

    if (count > GDX_TEXREG_CAPACITY) {
        static s32 sClampLogs = 0;
        if (sClampLogs < 8) {
            sClampLogs++;
            gdx_dbg_logf("[texreg] registry count is out of range; clamping the lookup scan\n");
            gdx_dbg_logf("[texreg]  count=%d (0x%x)\n", (int) count, (unsigned) count);
            gdx_dbg_logf("[texreg]  capacity=%d (0x%x)\n", (int) GDX_TEXREG_CAPACITY, (unsigned) GDX_TEXREG_CAPACITY);
        }
        count = GDX_TEXREG_CAPACITY;
    }
    if (count < 0) {
        count = 0;
    }

    for (i = 0; i < count; i++) {
#else
    for (i = 0; i < D_800E3A20; i++) {
#endif
        if (arg0 == D_800E33E0[i].unk_00) {
            return D_800E33E0[i].unk_04;
        }
    }
#ifdef PORT
    /* [reg-miss]: a NULL return here becomes a
       NULL palette in func_8007E410, which SKIPS the TLUT upload and draws
       CI text against whatever palette was last loaded -- the pause-menu
       stripe mechanism. Every miss names the symbol and the registry size
       (a small size after menus registered ~30 assets = the registry was
       cleared by func_80077D44 between registration and draw). */
    {
        extern void gdx_ckp(const char* s, void* v);
        extern void gdx_cki(const char* s, int v);
        static s32 sRegMissLogs = 0;
        if (sRegMissLogs < 24) {
            sRegMissLogs++;
            gdx_ckp("[reg-miss] symbol", arg0);
            gdx_cki("[reg-miss]  registryCount", (int) D_800E3A20);
        }
    }
#endif
    return NULL;
}

Gfx* func_800783F4(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 0, 0,
                                 0);
        case 5:
        case 21:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 1, 0,
                                 0);
        case 2:
        case 18:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 1,
                                 0, 0);
        case 1:
        default:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0,
                                 0, 0);
    }
}

Gfx* func_8007857C(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 0, 0,
                                 0);
        case 5:
        case 21:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 1, 0,
                                 0);
        case 2:
        case 18:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 1,
                                 0, 0);
        case 1:
        default:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0,
                                 0, 0);
    }
}

Gfx* func_8007870C(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture, s32 arg5, s32 arg6) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 0, 0,
                                 0);
        case 5:
        case 21:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_I, G_IM_SIZ_4b, 3, 1, 0,
                                 0);
        case 2:
        case 18:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 1,
                                 arg5, arg6);
        case 1:
        default:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0,
                                 arg5, arg6);
    }
}

Gfx* func_800788A4(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture, f32 arg5, f32 arg6) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 5:
        case 21:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 2:
        case 18:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 1,
                                 0, 0);
        case 1:
        default:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_RGBA,
                                 G_IM_SIZ_16b, 0, 0, 0);
    }
}

Gfx* func_80078A4C(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture, f32 arg5, f32 arg6) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 5:
        case 21:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 2:
        case 18:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 1,
                                 0, 0);
        case 1:
        default:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_RGBA,
                                 G_IM_SIZ_16b, 0, 0, 1);
    }
}

Gfx* func_80078BF8(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture, f32 arg5, f32 arg6) {

    switch (arg1->unk_00) {
        case 3:
            return func_8007B14C(gfx, texture, left, top, arg1->width, arg1->height, G_IM_FMT_IA, G_IM_SIZ_8b, 2, 0, 0,
                                 0);
        case 4:
        case 20:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 5:
        case 21:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_I,
                                 G_IM_SIZ_4b, 3, 0, 0);
        case 2:
        case 18:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_RGBA,
                                 G_IM_SIZ_16b, 1, 1, 0);
        case 1:
        default:
            return func_8007CDB0(gfx, texture, left, top, arg1->width, arg1->height, arg5, arg6, G_IM_FMT_RGBA,
                                 G_IM_SIZ_16b, 1, 0, 0);
    }
}

// BAD RETURN
Gfx* func_80078DB4(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, TexturePtr texture, u32 arg5, s32 arg6, s32 arg7,
                   f32 arg8, f32 arg9) {
    switch (arg5) {
        case 0:
            return func_800783F4(gfx, arg1, left, top, texture);
        case 1:
            return func_8007857C(gfx, arg1, left, top, texture);
        case 2:
            return func_8007870C(gfx, arg1, left, top, texture, arg6, arg7);
        case 3:
            return func_800788A4(gfx, arg1, left, top, texture, arg8, arg9);
        case 5:
            return func_80078A4C(gfx, arg1, left, top, texture, arg8, arg9);
        case 4:
            return func_80078BF8(gfx, arg1, left, top, texture, arg8, arg9);
    }
    /* AVOID_UB: unknown format fell off and returned a garbage Gfx* — the
       caller keeps building the display list from that cursor (memory
       corruption on host). No-op instead. */
    return gfx;
}

Gfx* func_80078EA0_impl(Gfx* gfx, unk_80077D50* arg1, s32 left, s32 top, u32 arg4, s32 arg5, s32 arg6, f32 arg7,
                        f32 arg8, bool arg9) {
    TexturePtr texture;

    while (arg1->unk_04 != 0) {
        // FAKE
        if (gfx) {}

        texture = func_800783AC(arg1->unk_04);
        if (texture != NULL) {
            gfx = func_80078DB4(gfx, arg1, left, top, texture, arg4, arg5, arg6, arg7, arg8);
        }
#ifdef EXPANSION_KIT
        if (arg9) {
            break;
        }
#endif
        arg1++;
    }
    return gfx;
}

Gfx* func_80078F80_impl(Gfx* gfx, unk_800E3F28* arg1, s32 left, s32 top, u32 arg4, s32 arg5, s32 arg6, f32 arg7,
                        f32 arg8, bool arg9) {
    TexturePtr texture;
    unk_80077D50* var_s0;
    s32 var;

    var_s0 = arg1->unk_00[arg1->unk_04].unk_00;

    while (var_s0->unk_04 != 0) {
#ifdef EXPANSION_KIT
        if (gfx) {}
#endif
        var = arg1->unk_0A;
        switch (var) {
            case 0:
                texture = arg1->unk_0C;
                break;
            default:
                texture = arg1->unk_10;
                break;
        }

        if (texture != NULL) {
            gfx = func_80078DB4(gfx, var_s0, left, top, texture, arg4, arg5, arg6, arg7, arg8);
        }
#ifdef EXPANSION_KIT
        if (arg9) {
            break;
        }
#endif
        var_s0++;
    }
    return gfx;
}

void func_80079080(void) {
    s32 i;

    D_800E4068[0].unk_00 = NULL;

    for (i = 1; i < 17; i++) {}
}

void func_800790A4(unk_80077D50* arg0, TexturePtr arg1) {
    unk_800E4068* var_v0;

    var_v0 = D_800E4068;
    while (var_v0->unk_00 != NULL) {
        var_v0++;
    }
    var_v0->unk_00 = arg0;
    var_v0->unk_04 = arg1;
    (var_v0 + 1)->unk_00 = NULL;
}

void func_800790D4(void) {
    s32 var_v1;
    u8* header;
    size_t size;
    unk_80077D50* temp_s1;
    unk_800E4068* var_s3;

    var_s3 = D_800E4068;

    while (true) {
        temp_s1 = var_s3->unk_00;
        if (temp_s1 != NULL) {
            switch (temp_s1->unk_00) {
                case 4:
                case 5:
                    if (temp_s1->width % 16) {
                        var_v1 = ((temp_s1->width + 16) / 16) * 16;
                    } else {
                        var_v1 = temp_s1->width;
                    }
                    func_80077CF0(temp_s1->unk_04, temp_s1->height * var_v1, var_s3->unk_04);
                    break;
                case 17:
                case 18:
                    if (temp_s1->compressedSize != 0) {
                        size = ALIGN_2(temp_s1->compressedSize) + 2;
                    } else {
                        size = 0x400;
                    }
                    header = Arena_Allocate(ALLOC_PEEK, size);
                    CLEAR_DATA_CACHE(header, size);
                    GDX_LoadRawRomAsset(temp_s1->unk_04, size, header); /* staging: raw MIO0 bytes required */
                    if (GDX_IS_MIO0(header)) {
#ifdef PORT
                        /* Mirrors the overflow guard added to func_80077D50_impl's
                         * case 17/18 -- this deferred-decode path (used by the
                         * portrait/name-card ping-pong cache via func_800793E8)
                         * was missing the same protection + diagnostics, so a
                         * bad ROM-offset lookup here silently produced garbage
                         * or a blank buffer with no trace in the log. */
                        {
                            u32 mioDestSize = GDX_READ_BE_U32((const u8*)header + 4);
                            u32 allocSize   = (u32)(temp_s1->height * temp_s1->width * 2);
                            if (mioDestSize > allocSize) {
                                gdx_ck("[mio0] OVERFLOW PREVENTED in func_800790D4 case17/18");
                                gdx_cki("[mio0]  sym_low32", (int)(uintptr_t) temp_s1->unk_04);
                                gdx_cki("[mio0]  dest_size", (int) mioDestSize);
                                gdx_cki("[mio0]  alloc_size", (int) allocSize);
                                bzero(var_s3->unk_04, allocSize);
                            } else {
                                mio0Decode(header, var_s3->unk_04);
                            }
                        }
#else
                        mio0Decode(header, var_s3->unk_04);
#endif
                    } else {
#ifdef PORT
                        gdx_ck("[mio0] MAGIC MISMATCH in func_800790D4 case17/18 -- blanking texture");
                        gdx_cki("[mio0]  sym_low32", (int)(uintptr_t) temp_s1->unk_04);
                        gdx_cki("[mio0]  header[0..3]", *(int*) header);
#endif
                        bzero(var_s3->unk_04, temp_s1->height * temp_s1->width * 2);
                    }
                    break;
                default:
                    size = temp_s1->height * temp_s1->width;
                    func_80077CF0(temp_s1->unk_04, size * 2, var_s3->unk_04);
                    break;
            }
            var_s3->unk_00 = NULL;
            var_s3++;
        } else {
            break;
        }
    }
}

void func_800792A8(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        D_800E3F28[i].unk_08 = 0;
    }
}

s32 func_800792D8(unk_800792D8* arg0) {
    s32 i = 0;
    u8* var_v0;

    while (D_800E3F28[i].unk_08 != 0) {
        if (++i >= 16) {
            return -1;
        }
    }

    D_800E3F28[i].unk_00 = arg0;
    D_800E3F28[i].unk_04 = -1;
    D_800E3F28[i].unk_06 = 0;
    D_800E3F28[i].unk_08 = -0x8000;

    if (arg0[0].unk_00 != NULL) {
        D_800E3F28[i].unk_0C = func_80077D50_impl(arg0[0].unk_00, 1, true);
    }

    D_800E3F28[i].unk_10 = (arg0[1].unk_00 != NULL) ? func_80077D50_impl(arg0[1].unk_00, 1, true)
                                                    : func_80077D50_impl(arg0[0].unk_00, 1, true);
    D_800E3F28[i].unk_0A = 0;
    return i;
}

void func_800793E8(s32 arg0, s32 arg1, unk_800792D8* arg2) {
    unk_800E3F28* sp1C;
    unk_80077D50* temp_a3;

    D_800E3F28[arg0].unk_04 = arg1;
    D_800E3F28[arg0].unk_00 = arg2;
    // FAKE
    D_800E3F28[arg0].unk_06 = (arg2 + arg1)->unk_04;
    temp_a3 = arg2[D_800E3F28[arg0].unk_04].unk_00;

    if (D_800E3F28[arg0].unk_0A != 0) {
        func_800790A4(temp_a3, D_800E3F28[arg0].unk_0C);
        D_800E3F28[arg0].unk_0A = 0;
    } else {
        func_800790A4(temp_a3, D_800E3F28[arg0].unk_10);
        D_800E3F28[arg0].unk_0A = 1;
    }
}

void Object_ClearAll(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        gObjects[i].cmdId = OBJECT_FREE;
    }
}

void Object_Init(s32 cmdId, s32 left, s32 top, s8 priority) {
    s32 i = 0;
    Object* object = gObjects;

    while (true) {
        if (object->cmdId == OBJECT_FREE) {
            break;
        }
        if (++i > ARRAY_COUNT(gObjects)) {
            return;
        }
        object++;
    }

    object->cmdId = cmdId;
    object->state = 0;
    object->state2 = 0;
    object->left = left;
    object->top = top;
    object->priority = priority;
    object->shouldDraw = true;
    object->counter = 0;
    object->counter2 = 0;

    switch (cmdId) {
        case OBJECT_TITLE_BACKGROUND:
            Title_BackgroundInit(object);
            break;
        case OBJECT_14:
            func_i4_8011B0C8();
            break;
        case OBJECT_TITLE_LOGO:
            Title_LogoInit(object);
            break;
        case OBJECT_16:
            func_i4_8011B134();
            break;
        case OBJECT_TITLE_PUSH_START:
            Title_StartInit(object);
            break;
        case OBJECT_18:
            func_i4_8011B1E4();
            break;
        case OBJECT_TITLE_COPYRIGHT:
            Title_CopyrightInit();
            break;
        case OBJECT_TITLE_DISK_DRIVE:
            Title_DiskDriveInit(object);
            break;
#ifdef EXPANSION_KIT
        case OBJECT_21:
            func_i4_800748F4();
            break;
#endif
        case OBJECT_MACHINE_SELECT_HEADER:
            MachineSelect_HeaderInit();
            break;
        case OBJECT_32:
            func_i4_80116E8C(object);
            break;
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_0:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_1:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_2:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_3:
            MachineSettings_PortraitInit(object);
            break;
        case OBJECT_MACHINE_SELECT_PORTRAIT_0:
        case OBJECT_MACHINE_SELECT_PORTRAIT_1:
        case OBJECT_MACHINE_SELECT_PORTRAIT_2:
        case OBJECT_MACHINE_SELECT_PORTRAIT_3:
            MachineSelect_PortraitInit(object);
            break;
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_0:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_1:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_2:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_3:
            MachineSelect_CursorNumInit(object);
            break;
        case OBJECT_MACHINE_SELECT_CURSOR:
            MachineSelect_CursorInit();
            break;
        case OBJECT_MACHINE_SELECT_MACHINE:
            MachineSelect_MachineInit(object);
            break;
        case OBJECT_MACHINE_SETTINGS_MACHINE:
            MachineSettings_MachineInit(object);
            break;
        case OBJECT_MACHINE_SETTINGS_ENGINE_WEIGHT:
            MachineSettings_EngineWeightInit();
            break;
        case OBJECT_MACHINE_SETTINGS_STATS:
            MachineSettings_StatsInit();
            break;
        case OBJECT_MACHINE_SETTINGS_NAME_CARD:
            MachineSettings_NameCardInit(object);
            break;
        case OBJECT_MACHINE_SETTINGS_SLIDER:
            MachineSettings_SliderInit();
            break;
        case OBJECT_MACHINE_SELECT_OK:
            MachineSelect_OkInit(object);
            break;
        case OBJECT_MACHINE_SETTINGS_OK:
            MachineSettings_OkInit(object);
            break;
        case OBJECT_MACHINE_SELECT_DIFFICULTY_CUPS:
            MachineSelect_DifficultyCupsInit(object);
            break;
        case OBJECT_MACHINE_SELECT_STATS_0:
        case OBJECT_MACHINE_SELECT_STATS_1:
        case OBJECT_MACHINE_SELECT_STATS_2:
        case OBJECT_MACHINE_SELECT_STATS_3:
            MachineSelect_StatsInit();
            break;
        case OBJECT_MAIN_MENU_BACKGROUND:
            MainMenu_BackgroundInit(object);
            break;
        case OBJECT_MAIN_MENU_MODE_SIGN_0:
        case OBJECT_MAIN_MENU_MODE_SIGN_1:
        case OBJECT_MAIN_MENU_MODE_SIGN_2:
        case OBJECT_MAIN_MENU_MODE_SIGN_3:
        case OBJECT_MAIN_MENU_MODE_SIGN_4:
        case OBJECT_MAIN_MENU_MODE_SIGN_5:
        case OBJECT_MAIN_MENU_MODE_SIGN_6:
        case OBJECT_MAIN_MENU_MODE_SIGN_7:
            MainMenu_SignInit(object);
            break;
        case OBJECT_MAIN_MENU_HEADER:
            MainMenu_HeaderInit(object);
            break;
        case OBJECT_MAIN_MENU_SELECT_NUM_PLAYERS:
            MainMenu_NumPlayersInit();
            break;
        case OBJECT_MAIN_MENU_SELECT_DIFFICULTY:
            MainMenu_DifficultyInit();
            break;
        case OBJECT_MAIN_MENU_SELECT_TIME_ATTACK_MODE:
            MainMenu_TimeAttackModeInit();
            break;
        case OBJECT_MAIN_MENU_OK:
            MainMenu_OkInit(object);
            break;
        case OBJECT_MAIN_MENU_UNLOCK_EVERYTHING:
            MainMenu_UnlockEverythingInit(object);
            break;
        case OBJECT_COURSE_SELECT_BACKGROUND:
            CourseSelect_BackgroundInit(object);
            break;
        case OBJECT_COURSE_SELECT_MODEL:
            CourseSelect_ModelInit();
            break;
        case OBJECT_COURSE_SELECT_CUP_0:
        case OBJECT_COURSE_SELECT_CUP_1:
        case OBJECT_COURSE_SELECT_CUP_2:
        case OBJECT_COURSE_SELECT_CUP_3:
        case OBJECT_COURSE_SELECT_CUP_4:
        case OBJECT_COURSE_SELECT_CUP_5:
#ifdef EXPANSION_KIT
        case OBJECT_COURSE_SELECT_CUP_6:
        case OBJECT_COURSE_SELECT_CUP_7:
#endif
            CourseSelect_CupInit(object);
            break;
        case OBJECT_COURSE_SELECT_HEADER:
            CourseSelect_HeaderInit(object);
            break;
        case OBJECT_COURSE_SELECT_OK:
            CourseSelect_OkInit(object);
            break;
        case OBJECT_COURSE_SELECT_ARROWS:
            CourseSelect_ArrowsInit(object);
            break;
        case OBJECT_COURSE_SELECT_GHOST_MARKER:
            CourseSelect_GhostMarkerInit(object);
            break;
        case OBJECT_COURSE_SELECT_GHOST_OPTION:
            CourseSelect_GhostOptionInit(object);
            break;
#ifdef EXPANSION_KIT
        case OBJECT_170:
            func_xk3_80133B4C(object);
            break;
        case OBJECT_171:
            func_xk3_80133B84();
            break;
        case OBJECT_172:
            func_xk3_80133BD4(object);
            break;
        case OBJECT_173:
            func_xk3_80133F40();
            break;
        case OBJECT_174:
            func_xk3_8012F5F0(object);
            break;
#endif
        default:
            break;
    }
}

Gfx* Object_Draw(Gfx* gfx, Object* object) {

    if (!object->shouldDraw) {
        return gfx;
    }

    switch (object->cmdId) {
        case OBJECT_FREE:
            break;
        case OBJECT_FRAMEBUFFER:
            gfx = func_8007AB88(gfx);
            gfx = func_8007AE70(gfx);
            gfx = func_8007ABA4(gfx);
            break;
        case OBJECT_TITLE_BACKGROUND:
            gfx = Title_BackgroundDraw(gfx, object);
            break;
        case OBJECT_14:
            gfx = func_i4_8011B3DC(gfx, object);
            break;
        case OBJECT_12:
            gfx = func_8007AF40(gfx, 118, 164, 203, 217, 255, 255, 255, 48);
            break;
        case OBJECT_13:
            gfx = func_8007AE8C(gfx, 12, 8, 307, 231, 0, 0, 0, 0);
            break;
        case OBJECT_TITLE_LOGO:
            gfx = Title_LogoDraw(gfx, object);
            break;
        case OBJECT_16:
            gfx = func_i4_8011B438(gfx, object);
            break;
        case OBJECT_TITLE_PUSH_START:
            gfx = Title_StartDraw(gfx, object);
            break;
        case OBJECT_18:
            gfx = func_i4_8011B668(gfx, object);
            break;
        case OBJECT_TITLE_COPYRIGHT:
            gfx = Title_CopyrightDraw(gfx, object);
            break;
        case OBJECT_TITLE_DISK_DRIVE:
            gfx = Title_DiskDriveDraw(gfx, object);
            break;
#ifdef EXPANSION_KIT
        case OBJECT_21:
            gfx = func_i4_80074EE0(gfx, object);
            break;
#endif
        case OBJECT_MACHINE_SELECT_BACKGROUND:
            gfx = MachineSelect_BackgroundDraw(gfx);
            break;
        case OBJECT_MACHINE_SETTINGS_BACKGROUND:
            gfx = func_8007AC48(gfx, 24, 24, 24);
            break;
        case OBJECT_MACHINE_SELECT_HEADER:
            gfx = MachineSelect_HeaderDraw(gfx, object);
            break;
        case OBJECT_32:
            gfx = func_i4_80117BE0(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_0:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_1:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_2:
        case OBJECT_MACHINE_SETTINGS_PORTRAIT_3:
            gfx = MachineSettings_PortraitDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_PORTRAIT_0:
        case OBJECT_MACHINE_SELECT_PORTRAIT_1:
        case OBJECT_MACHINE_SELECT_PORTRAIT_2:
        case OBJECT_MACHINE_SELECT_PORTRAIT_3:
            gfx = MachineSelect_PortraitDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_0:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_1:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_2:
        case OBJECT_MACHINE_SELECT_CURSOR_NUM_3:
            gfx = MachineSelect_CursorNumDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_CURSOR:
            gfx = MachineSelect_CursorDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_MACHINE:
            gfx = MachineSelect_MachineDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_OK:
            gfx = MachineSelect_OkDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_STATS_0:
        case OBJECT_MACHINE_SELECT_STATS_1:
        case OBJECT_MACHINE_SELECT_STATS_2:
        case OBJECT_MACHINE_SELECT_STATS_3:
            gfx = MachineSelect_StatsDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_MACHINE:
            gfx = MachineSettings_MachineDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_NAME:
            gfx = MachineSettings_NameDraw(gfx);
            break;
        case OBJECT_MACHINE_SETTINGS_ENGINE_WEIGHT:
            gfx = MachineSettings_EngineWeightDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_STATS:
            gfx = MachineSettings_StatsDraw(gfx, object);
            break;
        case OBJECT_58:
            gfx = func_i4_801193B8(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_NAME_CARD:
            gfx = MachineSettings_NameCardDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_SLIDER:
            gfx = MachineSettings_SliderDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SETTINGS_SPLITSCREEN_BARS:
            gfx = MachineSettings_SplitscreenDraw(gfx);
            break;
        case OBJECT_MACHINE_SETTINGS_OK:
            gfx = MachineSettings_OkDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_DIFFICULTY_CUPS:
            gfx = MachineSelect_DifficultyCupsDraw(gfx, object);
            break;
        case OBJECT_MACHINE_SELECT_NAME:
            gfx = MachineSelect_NameDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_BACKGROUND:
            gfx = MainMenu_BackgroundDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_MODE_SIGN_0:
        case OBJECT_MAIN_MENU_MODE_SIGN_1:
        case OBJECT_MAIN_MENU_MODE_SIGN_2:
        case OBJECT_MAIN_MENU_MODE_SIGN_3:
        case OBJECT_MAIN_MENU_MODE_SIGN_4:
        case OBJECT_MAIN_MENU_MODE_SIGN_5:
        case OBJECT_MAIN_MENU_MODE_SIGN_6:
        case OBJECT_MAIN_MENU_MODE_SIGN_7:
            gfx = MainMenu_SignDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_HEADER:
            gfx = MainMenu_HeaderDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_SELECT_NUM_PLAYERS:
            gfx = MainMenu_NumPlayersDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_SELECT_DIFFICULTY:
            gfx = MainMenu_DifficultyDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_SELECT_TIME_ATTACK_MODE:
            gfx = MainMenu_TimeAttackModeDraw(gfx, object);
            break;
        case OBJECT_MAIN_MENU_OK:
            gfx = MainMenu_OkDraw(gfx, object);
            break;
        case OBJECT_100:
        case OBJECT_140:
            gfx = func_8007AC48(gfx, 0, 0, 0);
            break;
        case OBJECT_COURSE_SELECT_BACKGROUND:
            gfx = CourseSelect_BackgroundDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_MODEL:
            gfx = CourseSelect_ModelDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_CUP_0:
        case OBJECT_COURSE_SELECT_CUP_1:
        case OBJECT_COURSE_SELECT_CUP_2:
        case OBJECT_COURSE_SELECT_CUP_3:
        case OBJECT_COURSE_SELECT_CUP_4:
        case OBJECT_COURSE_SELECT_CUP_5:
#ifdef EXPANSION_KIT
        case OBJECT_COURSE_SELECT_CUP_6:
        case OBJECT_COURSE_SELECT_CUP_7:
#endif
            gfx = CourseSelect_CupDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_HEADER:
            gfx = CourseSelect_HeaderDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_OK:
            gfx = CourseSelect_OkDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_ARROWS:
            gfx = CourseSelect_ArrowsDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_NAME:
            gfx = CourseSelect_NameDraw(gfx);
            break;
        case OBJECT_COURSE_SELECT_GHOST_MARKER:
            gfx = CourseSelect_GhostMarkerDraw(gfx, object);
            break;
        case OBJECT_COURSE_SELECT_GHOST_OPTION:
            gfx = CourseSelect_GhostOptionDraw(gfx, object);
            break;
#ifdef EXPANSION_KIT
        case OBJECT_170:
            gfx = func_xk3_80133F6C(gfx, object);
            break;
        case OBJECT_171:
            gfx = func_xk3_801340DC(gfx, object);
            break;
        case OBJECT_172:
            gfx = func_xk3_80134408(gfx, object);
            break;
        case OBJECT_173:
            gfx = func_xk3_80134854(gfx, object);
            break;
        case OBJECT_174:
            gfx = func_xk3_8012F628(gfx, object);
            break;
#endif
    }
    return gfx;
}

Gfx* Object_UpdateAndDrawAll(Gfx* gfx) {
    s32 i;
    s32 j;

    for (i = 0; i < 32; i++) {
        switch (gObjects[i].cmdId) {
            case OBJECT_FREE:
                break;
            case OBJECT_TITLE_BACKGROUND:
                Title_BackgroundUpdate(&gObjects[i]);
                break;
            case OBJECT_TITLE_DISK_DRIVE:
                Title_DiskDriveUpdate(&gObjects[i]);
                break;
            case OBJECT_MAIN_MENU_OK:
                MainMenu_OkUpdate(&gObjects[i]);
                break;
            case OBJECT_MAIN_MENU_UNLOCK_EVERYTHING:
                MainMenu_UnlockEverythingUpdate(&gObjects[i]);
                break;
            case OBJECT_32:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                func_i4_80119BB8(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SETTINGS_PORTRAIT_0:
            case OBJECT_MACHINE_SETTINGS_PORTRAIT_1:
            case OBJECT_MACHINE_SETTINGS_PORTRAIT_2:
            case OBJECT_MACHINE_SETTINGS_PORTRAIT_3:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                MachineSettings_PortraitUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SELECT_PORTRAIT_0:
            case OBJECT_MACHINE_SELECT_PORTRAIT_1:
            case OBJECT_MACHINE_SELECT_PORTRAIT_2:
            case OBJECT_MACHINE_SELECT_PORTRAIT_3:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                MachineSelect_PortraitUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SELECT_CURSOR_NUM_0:
            case OBJECT_MACHINE_SELECT_CURSOR_NUM_1:
            case OBJECT_MACHINE_SELECT_CURSOR_NUM_2:
            case OBJECT_MACHINE_SELECT_CURSOR_NUM_3:
                MachineSelect_CursorNumUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SELECT_CURSOR:
                MachineSelect_CursorUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SELECT_MACHINE:
                MachineSelect_MachineUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SETTINGS_MACHINE:
                MachineSettings_MachineUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SETTINGS_NAME_CARD:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                MachineSettings_NameCardUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SELECT_OK:
                MachineSelect_OkUpdate(&gObjects[i]);
                break;
            case OBJECT_MACHINE_SETTINGS_OK:
                MachineSettings_OkUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_MODEL:
                CourseSelect_ModelUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_CUP_0:
            case OBJECT_COURSE_SELECT_CUP_1:
            case OBJECT_COURSE_SELECT_CUP_2:
            case OBJECT_COURSE_SELECT_CUP_3:
            case OBJECT_COURSE_SELECT_CUP_4:
            case OBJECT_COURSE_SELECT_CUP_5:
#ifdef EXPANSION_KIT
            case OBJECT_COURSE_SELECT_CUP_6:
            case OBJECT_COURSE_SELECT_CUP_7:
#endif
                CourseSelect_CupUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_OK:
                CourseSelect_OkUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_ARROWS:
                CourseSelect_ArrowsUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_GHOST_MARKER:
                CourseSelect_GhostMarkerUpdate(&gObjects[i]);
                break;
            case OBJECT_COURSE_SELECT_GHOST_OPTION:
                CourseSelect_GhostOptionUpdate(&gObjects[i]);
                break;
#ifdef EXPANSION_KIT
            case OBJECT_170:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                func_xk3_80134A48(&gObjects[i]);
                break;
            case OBJECT_172:
                func_xk3_80134B04(&gObjects[i]);
                break;
            case OBJECT_174:
                D_800E3F28[OBJECT_CACHE_INDEX(&gObjects[i])].unk_04 = 0;
                func_xk3_8012F6A8(&gObjects[i]);
                break;
#endif
        }
    }

    for (j = 0; j < 16; j++) {
        for (i = 0; i < 32; i++) {
            if (j == gObjects[i].priority) {
                gfx = Object_Draw(gfx, &gObjects[i]);
            }
        }
    }

    return gfx;
}

Object* Object_Get(s32 cmdId) {
    Object* object;

    object = gObjects;

    while (true) {
        if (cmdId == object->cmdId) {
            break;
        }
        object++;
        //! @bug this allows for an iteration out of the bounds of the array
        /* AVOID_UB: >= stops before dereferencing one element past the array
           (a lucky cmdId match there returned an out-of-bounds Object* that
           callers then write through). */
        if (object >= &gObjects[ARRAY_COUNT(gObjects)]) {
            return NULL;
        }
    }
    return object;
}

extern s32 gGameMode;

void func_80079EC8(void) {
    func_80077D44();
    Object_ClearAll();
    func_800792A8();
    func_80079080();
    if (gGameMode != GAMEMODE_CREATE_MACHINE) {
        func_8007E2B4();
    }
}

void func_80079F1C(void) {
    Object_ClearAll();
    func_800792A8();
    func_80079080();
    func_8007E2B4();
}

void Object_LerpPosXToTarget(Object* object, s32 target, s32 stepScale) {
    s32 step;

    step = target - OBJECT_LEFT(object);
    if (step != 0) {
        if (step > 0) {
            step /= stepScale;
            if (++step > 8) {
                step = 8;
            }
        } else {
            step /= stepScale;
            if (--step < -8) {
                step = -8;
            }
        }
    }
    OBJECT_LEFT(object) += step;
}

void Object_LerpPosYToTarget(Object* object, s32 target) {
    s32 step;

    step = target - OBJECT_TOP(object);
    if (step != 0) {
        if (step > 0) {
            step /= 4;
            if (++step > 16) {
                step = 16;
            }
        } else {
            step /= 4;
            if (--step < -16) {
                step = -16;
            }
        }
    }
    OBJECT_TOP(object) += step;
}

void Object_LerpToPos(Object* object, s32 xTarget, s32 yTarget) {
    Object_LerpPosXToTarget(object, xTarget, 4);
    Object_LerpPosYToTarget(object, yTarget);
}

void Object_LerpPosXToClampedTargetMaxStep(Object* object, s32 target, s32 maxStep) {
    s32 step;

    step = target - OBJECT_LEFT(object);
    if (step != 0) {
        if (step > 0) {
            step /= 4;
            if (maxStep < ++step) {
                step = maxStep;
            }
            if (step < 8) {
                step = 8;
            }
            OBJECT_LEFT(object) += step;

            if (target < OBJECT_LEFT(object)) {
                OBJECT_LEFT(object) = target;
            }
        } else {
            step /= 4;
            if (--step < -maxStep) {
                step = -maxStep;
            }
            if (step > -8) {
                step = -8;
            }
            OBJECT_LEFT(object) += step;

            if (OBJECT_LEFT(object) < target) {
                OBJECT_LEFT(object) = target;
            }
        }
    }
}

void Object_LerpPosYToClampedTarget(Object* object, s32 target) {
    s32 step;

    step = target - OBJECT_TOP(object);
    if (step != 0) {
        if (step > 0) {
            step /= 4;
            if (++step > 192) {
                step = 192;
            }
            if (step < 8) {
                step = 8;
            }
            OBJECT_TOP(object) += step;

            if (target < OBJECT_TOP(object)) {
                OBJECT_TOP(object) = target;
            }
        } else {
            step /= 4;
            if (--step < -192) {
                step = -192;
            }
            if (step >= -7) {
                step = -8;
            }
            OBJECT_TOP(object) += step;

            if (OBJECT_TOP(object) < target) {
                OBJECT_TOP(object) = target;
            }
        }
    }
}

// Duplicate function
void Object_LerpPosYToTarget2(Object* object, s32 target) {
    s32 step;

    step = target - OBJECT_TOP(object);
    if (step != 0) {
        if (step > 0) {
            step /= 4;
            if (++step > 16) {
                step = 16;
            }
        } else {
            step /= 4;
            if (--step < -16) {
                step = -16;
            }
        }
    }
    OBJECT_TOP(object) += step;
}

void Object_LerpPosXToClampedTarget(Object* object, s32 target) {
    s32 step;

    step = target - OBJECT_LEFT(object);
    if (step != 0) {
        step = 200 / step;
        if (step > 0) {
            if (step > 24) {
                step = 24;
            }
            if (step < 16) {
                step = 16;
            }
            OBJECT_LEFT(object) += step;

            if (target < OBJECT_LEFT(object)) {
                OBJECT_LEFT(object) = target;
            }
        } else {
            if (step < -24) {
                step = -24;
            }
            if (step > -16) {
                step = -16;
            }
            OBJECT_LEFT(object) += step;

            if (OBJECT_LEFT(object) < target) {
                OBJECT_LEFT(object) = target;
            }
        }
    }
}

void Object_LerpAwayFromPosX(Object* object, s32 origin, s32 initialStep) {
    UNUSED s32 temp = OBJECT_LEFT(object);
    s32 step;

    step = origin - OBJECT_LEFT(object);
    if (origin == OBJECT_LEFT(object)) {
        OBJECT_LEFT(object) += initialStep;
        return;
    }
    if (step != 0) {
        if (step > 0) {
            step /= 8;
            if (++step > 16) {
                step = 16;
            }
        } else {
            step /= 8;
            if (--step < -16) {
                step = -16;
            }
        }
    }
    OBJECT_LEFT(object) -= step;
}

void Object_LerpAwayFromPosY(Object* object, s32 origin, s32 initialStep) {
    UNUSED s32 temp = OBJECT_TOP(object);
    s32 step;

    step = origin - OBJECT_TOP(object);
    if (origin == OBJECT_TOP(object)) {
        OBJECT_TOP(object) += initialStep;
        return;
    }
    if (step != 0) {
        if (step > 0) {
            step /= 8;
            if (++step > 16) {
                step = 16;
            }
        } else {
            step /= 8;
            if (--step < -16) {
                step = -16;
            }
        }
    }
    OBJECT_TOP(object) -= step;
}
