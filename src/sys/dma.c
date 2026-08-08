#include "global.h"
#include "fzx_thread.h"
#ifdef PORT
extern unsigned char* gdx_rdram;
#define GDX_DMA_RDRAM_SIZE ((size_t)0x1000000u)  /* must match GDX_RDRAM_SIZE in n64_rdram.h */
extern void gdx_record_dma_load(unsigned int rdram_phys, unsigned int rom_offset, unsigned int size);
/* port/gdx_segment_source.{h,c}. Forward-declared rather than #include'd: this decomp TU's
 * include path does not carry port/. Returns verbatim ROM bytes, archive-first. */
extern int GdxSegmentSourceRead(unsigned int romBase, unsigned int size, void* dst);
#endif

void func_80076490(void) {
}

extern OSMesgQueue gDmaMesgQueue;
extern OSIoMesg gDmaIOMsg;
extern OSPiHandle* gCartRomHandle;

#ifdef PORT
extern void* gdx_resolve_registered_host_address(unsigned int addr);
/* port/n64_gfx_bridge.cpp; forward-declared for the same reason as the shim above. */
extern size_t gdx_registered_host_capacity(const void* host);
extern void gdx_dma_report_short_dest(const void* dst, unsigned int size, size_t capacity,
                                      unsigned int romOffset);

static size_t Dma_PortRomOffset(u8* romAddr) {
    unsigned int phys = (unsigned int)(unsigned long long)romAddr & 0x1FFFFFFFu;
    return (phys >= 0x10000000u) ? (size_t)(phys - 0x10000000u) : (size_t)phys;
}

/* Resolve an N64 RAM pointer to a host address, reporting how many bytes may be written there.
   Every branch below used to check only that the START address was in range, never start + size.
   `*capacity` is 0 when the extent is unknown; Dma_PortDestTooSmall lets those through. */
static u8* Dma_PortRamPointer(u8* ramAddr, size_t* capacity) {
    unsigned long long full = (unsigned long long)ramAddr;
    unsigned int low = (unsigned int)full;

    *capacity = 0;

    if (full < (unsigned long long)GDX_DMA_RDRAM_SIZE) {
        *capacity = GDX_DMA_RDRAM_SIZE - (size_t)full;
        return gdx_rdram + (size_t)full;
    }

    /* KSEG0/KSEG1 -> physical RDRAM offset; game model-load DMAs pass KSEG0 pointers.
       The `full <= 0xFFFFFFFF` width gate is load-bearing, not redundant: a 64-bit host
       pointer can have a low32 that merely LOOKS like KSEG0 when ASLR bases the EXE at
       0x????????8???????. Ungated it silently rerouted DMAs aimed at EXE globals into RDRAM --
       seen with the module at 0x7FF7805F0000, where &gCourseCtx's low32 is 0x80D31680, so
       course data landed at gdx_rdram+0xD31680, controlPointCount stayed 0, and cup select
       crashed in Course_SegmentLengthsInit. It comes and goes with the per-file ASLR draw. */
    if (full <= 0xFFFFFFFFull && (low & 0xE0000000u) == 0x80000000u) {
        unsigned int phys = low & 0x1FFFFFFFu;
        if (phys < (unsigned int)GDX_DMA_RDRAM_SIZE) {
            *capacity = GDX_DMA_RDRAM_SIZE - (size_t)phys;
            return gdx_rdram + phys;
        }
    }

    if (full <= 0xFFFFFFFFull) {
        u8* host = (u8*)gdx_resolve_registered_host_address(low);
        if (host != NULL) {
            *capacity = gdx_registered_host_capacity(host);
        }
        return host;
    }

    /* Full 64-bit host pointer: not from the registry, so no recorded extent. */
    return ramAddr;
}

/* Refuses a copy ONLY when the extent is known and too small; an unknown extent passes through
   unchanged, so this guard cannot regress a path that works today. */
static int Dma_PortDestTooSmall(const u8* dst, size_t capacity, size_t size, size_t romOffset) {
    if ((capacity != 0) && (capacity < size)) {
        gdx_dma_report_short_dest(dst, (unsigned int)size, capacity, (unsigned int)romOffset);
        return 1;
    }
    return 0;
}
#endif

void Dma_RomCopy(u8* romAddr, u8* ramAddr, size_t size) {
    OSMesg msgBuf[7];

#ifdef PORT
    {
        size_t romOffset = Dma_PortRomOffset(romAddr);
        size_t capacity;
        u8* dst = Dma_PortRamPointer(ramAddr, &capacity);
        if (dst == NULL) {
            return;
        }
        if (Dma_PortDestTooSmall(dst, capacity, size, romOffset)) {
            return;
        }
        /* A total miss (ROM absent / out of range) leaves dst untouched; zero-fill as the
         * pre-shim path did. */
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
        /* Unreachable under PORT: the only caller, Dma_LoadOverlay, early-returns first.
           Kept for structure; a future PORT caller must go through GdxSegmentSourceRead,
           not a reintroduced gdx_rom_buffer. */
        size_t romOffset = Dma_PortRomOffset(romAddr);
        size_t capacity;
        u8* dst = Dma_PortRamPointer(ramAddr, &capacity);
        if (dst == NULL) {
            bzero(bssAddr, bssSize);
            return;
        }
        if (Dma_PortDestTooSmall(dst, capacity, size, romOffset)) {
            bzero(bssAddr, bssSize);
            return;
        }
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
        /* Hardware DMA'd this asynchronously over the PI bus, leaving the CPU free. Here
           Dma_RomCopy is a synchronous memcpy on the game thread, so a hundreds-of-KB asset
           load monopolizes the cooperative scheduler and starves the audio fiber (AI buffer
           underruns during course loads). Yield every 32KB. */
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
    // Overlays are statically compiled into the binary: code and data are already at their
    // host addresses and BSS is zeroed by the host loader, so there is nothing to load.
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
