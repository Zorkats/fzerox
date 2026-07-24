#include "global.h"
#include "fzx_thread.h"
#ifdef PORT
extern unsigned char* gdx_rdram;
#define GDX_DMA_RDRAM_SIZE ((size_t)0x1000000u)  /* must match GDX_RDRAM_SIZE in n64_rdram.h */
extern void gdx_record_dma_load(unsigned int rdram_phys, unsigned int rom_offset, unsigned int size);
/* R1 (C-R1.3/C-R1.9 #2): route the cartridge read through the single byte-source
 * shim (port/gdx_segment_source.{h,c}) instead of touching gdx_rom_buffer here.
 * Forward-declared rather than #include'd: this decomp TU's include path does not
 * carry port/. The shim returns verbatim ROM bytes (archive-first, raw fallback),
 * so the copied bytes are byte-identical to the old memcpy. */
extern int GdxSegmentSourceRead(unsigned int romBase, unsigned int size, void* dst);
#endif

void func_80076490(void) {
}

extern OSMesgQueue gDmaMesgQueue;
extern OSIoMesg gDmaIOMsg;
extern OSPiHandle* gCartRomHandle;

#ifdef PORT
extern void* gdx_resolve_registered_host_address(unsigned int addr);

static size_t Dma_PortRomOffset(u8* romAddr) {
    unsigned int phys = (unsigned int)(unsigned long long)romAddr & 0x1FFFFFFFu;
    return (phys >= 0x10000000u) ? (size_t)(phys - 0x10000000u) : (size_t)phys;
}

static u8* Dma_PortRamPointer(u8* ramAddr) {
    unsigned long long full = (unsigned long long)ramAddr;
    unsigned int low = (unsigned int)full;

    /* Physical RDRAM offset (< 16MB): direct mapping. */
    if (full < (unsigned long long)GDX_DMA_RDRAM_SIZE) {
        return gdx_rdram + (size_t)full;
    }

    /* KSEG0 / KSEG1 virtual addresses (0x80000000–0xBFFFFFFF): strip the top bits
       to get the physical RDRAM offset.  Game model-load DMA calls pass KSEG0
       pointers; without this the memcpy is skipped and gdx_rdram stays empty. */
    if ((low & 0xE0000000u) == 0x80000000u) {
        unsigned int phys = low & 0x1FFFFFFFu;
        if (phys < (unsigned int)GDX_DMA_RDRAM_SIZE) {
            return gdx_rdram + phys;
        }
    }

    if (full <= 0xFFFFFFFFull) {
        return (u8*)gdx_resolve_registered_host_address(low);
    }

    return ramAddr;
}
#endif

void Dma_RomCopy(u8* romAddr, u8* ramAddr, size_t size) {
    OSMesg msgBuf[7];

#ifdef PORT
    {
        size_t romOffset = Dma_PortRomOffset(romAddr);
        u8* dst = Dma_PortRamPointer(ramAddr);
        if (dst == NULL) {
            return;
        }
        /* Archive-first via the shim; on a total miss (ROM absent / out of range)
         * it leaves dst untouched and returns 0 -- match the old zero-fill. */
        if (!GdxSegmentSourceRead((unsigned int)romOffset, (unsigned int)size, dst)) {
            memset(dst, 0, size);
            return;
        }
        if (dst >= gdx_rdram && dst < gdx_rdram + GDX_DMA_RDRAM_SIZE)
            gdx_record_dma_load((unsigned int)(size_t)(dst - gdx_rdram), (unsigned int)romOffset, (unsigned int)size);
        return;
    }
#endif
    gDmaIOMsg.hdr.pri = OS_MESG_PRI_NORMAL;
    gDmaIOMsg.hdr.retQueue = &gDmaMesgQueue;
    gDmaIOMsg.dramAddr = osPhysicalToVirtual(ramAddr);
    gDmaIOMsg.devAddr = romAddr;
    gDmaIOMsg.size = size;
    gCartRomHandle->transferInfo.cmdType = LEO_CMD_TYPE_2;
#ifndef EXPANSION_KIT
    osEPiStartDma(gCartRomHandle, &gDmaIOMsg, OS_READ);
#else
    func_80768B88(gCartRomHandle, &gDmaIOMsg, OS_READ);
#endif
    osRecvMesg(&gDmaMesgQueue, msgBuf, OS_MESG_BLOCK);
}

void Dma_RomCopyWithBssInit(u8* romAddr, u8* ramAddr, size_t size, void* bssAddr, size_t bssSize) {
    OSMesg msgBuf[7];

#ifdef PORT
    {
        /* DEAD CODE under PORT (R4 census): this function's only caller, Dma_LoadOverlay(),
           early-returns under PORT before ever reaching its call site, so this body is
           unreachable. Retained for structure and for any future PORT caller -- which now
           routes through the single byte-source shim (GdxSegmentSourceRead), archive-first
           with a byte-identical raw-ROM fallback, exactly like Dma_RomCopy above, NOT by
           reintroducing gdx_rom_buffer here. */
        size_t romOffset = Dma_PortRomOffset(romAddr);
        u8* dst = Dma_PortRamPointer(ramAddr);
        if (dst == NULL) {
            bzero(bssAddr, bssSize);
            return;
        }
        /* Archive-first; on a total miss (ROM absent / out of range) the shim leaves dst
           untouched and returns 0 -- match the old zero-fill. */
        if (!GdxSegmentSourceRead((unsigned int)romOffset, (unsigned int)size, dst)) {
            memset(dst, 0, size);
        } else if (dst >= gdx_rdram && dst < gdx_rdram + GDX_DMA_RDRAM_SIZE) {
            gdx_record_dma_load((unsigned int)(size_t)(dst - gdx_rdram), (unsigned int)romOffset, (unsigned int)size);
        }
        bzero(bssAddr, bssSize);
        return;
    }
#endif
    gDmaIOMsg.hdr.pri = OS_MESG_PRI_NORMAL;
    gDmaIOMsg.hdr.retQueue = &gDmaMesgQueue;
    gDmaIOMsg.dramAddr = osPhysicalToVirtual(ramAddr);
    gDmaIOMsg.devAddr = romAddr;
    gDmaIOMsg.size = size;
    gCartRomHandle->transferInfo.cmdType = LEO_CMD_TYPE_2;
#ifndef EXPANSION_KIT
    osEPiStartDma(gCartRomHandle, &gDmaIOMsg, OS_READ);
#else
    func_80768B88(gCartRomHandle, &gDmaIOMsg, OS_READ);
#endif
    bzero(bssAddr, bssSize);
    osRecvMesg(&gDmaMesgQueue, msgBuf, OS_MESG_BLOCK);
}

void Dma_LoadAssets(u8* romAddr, u8* ramAddr, size_t size) {
    s32 remainder;
    s32 i;
    s32 numBlocks = size / 1024;
#ifdef PORT
    extern void gdx_yield(void);
#endif

    for (i = 0; i < numBlocks; i++) {
        Dma_RomCopy(romAddr, ramAddr, 0x400);

        romAddr += 0x400;
        ramAddr += 0x400;
#ifdef PORT
        /* Real hardware DMA'd this over the PI bus asynchronously, freeing the
           CPU for other threads (notably audio) while it ran. This port's
           Dma_RomCopy is a synchronous memcpy with no yield point, so a large
           asset load (hud_gfx, machine_global_gfx, course textures -- often
           hundreds of KB) run inline on the GAME thread can monopolize the
           cooperative scheduler for its entire duration, starving the AUDIO
           fiber (measured as AI buffer underrun gaps during course loads).
           Yield every 32 blocks (32KB) so other runnable fibers get a turn;
           see port/n64_sched.c's gdx_yield() -- it re-enqueues this thread as
           runnable and returns to the host frame pump, then we resume here. */
        if ((i & 31) == 31) {
            gdx_yield();
        }
#endif
    }
    remainder = size % 1024;
    if (remainder != 0) {
        Dma_RomCopy(romAddr, ramAddr, remainder);
    }
}

void Dma_LoadOverlay(u8* romAddr, u8* ramAddr, size_t size, void* bssAddr, size_t bssSize) {
#ifdef PORT
    // Overlays are statically compiled into the binary. Code+data live at their native host
    // addresses; BSS is zero-initialised by the host loader. Nothing to load or clear.
    (void)romAddr; (void)ramAddr; (void)size; (void)bssAddr; (void)bssSize;
    return;
#endif
    s32 remainder;
    s32 i;
    s32 numBlocks;

    numBlocks = size / 1024;

    for (i = 0; i < numBlocks; i++) {
        Dma_RomCopy(romAddr, ramAddr, 0x400);

        romAddr += 0x400;
        ramAddr += 0x400;
    }
    remainder = size % 1024;
    if (remainder != 0) {
        Dma_RomCopyWithBssInit(romAddr, ramAddr, remainder, bssAddr, bssSize);
    }
}
