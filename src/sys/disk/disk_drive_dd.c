#include "global.h"

extern Controller gSharedController;
extern OSMesgQueue gSerialEventQueue;

s32 D_8076CB40 = -1;
s32 D_8076CB44 = 0;

void func_807038B0(void) {
    s32 sp24;

    PRINTF("WAIT MEDIA INIT\n");
    PRINTF("WAIT MEDIA START\n");
    PRINTF("WAIT RECOVER MANAGE AREA\n");

    func_8070F8A4(-1, 6);
    sp24 = osRecvMesg(&gSerialEventQueue, NULL, OS_MESG_NOBLOCK);
    /* 
    do {
        osContStartReadData(&gSerialEventQueue);
        Controller_UpdateInputs();
    } while (!(gSharedController.buttonPressed & BTN_A));
    */
    if (sp24 != -1) {
        osContStartReadData(&gSerialEventQueue);
    }
    func_8070F8A4(-1, 0xA);
}

void func_80703948(void) {
    s32 sp24;

    func_8070F8A4(-1, 7);
    sp24 = osRecvMesg(&gSerialEventQueue, NULL, OS_MESG_NOBLOCK);
    /*
    do {
        osContStartReadData(&gSerialEventQueue);
        Controller_UpdateInputs();
    } while (!(gSharedController.buttonPressed & BTN_A));
    */
    if (sp24 != -1) {
        osContStartReadData(&gSerialEventQueue);
    }
}

extern u8 D_i1_80415190[];
extern OSMesgQueue gDmaMesgQueue;

s32 func_800750B0(s32 startLba, void* vram, s32 diskSize, s32 bssSize) {
    void* bssStart;
    s32 sp58 = 0; /* AVOID_UB: returned without assignment */
    s32 lbaCount;
    s32 nBytes;
    LEOCmd cmdBlock;

    nBytes = 0;
    LeoByteToLBA(startLba, diskSize, &lbaCount);
    osVirtualToPhysical(vram);
    bssStart = (uintptr_t) vram + diskSize;
    osVirtualToPhysical(bssStart);
    osVirtualToPhysical((uintptr_t) bssStart + bssSize);

    PRINTF("========================================================\n");
    PRINTF("LBA %d, dist 0x%x-0x%x-0x%x , %dLBAs\n", startLba, vram, bssStart, (uintptr_t) bssStart + bssSize,
           lbaCount);
    PRINTF("========================================================\n");

    if (lbaCount - 1) {
        LeoLBAToByte(startLba, lbaCount - 1, &nBytes);
        SLLeoReadWrite(&cmdBlock, OS_READ, startLba, osPhysicalToVirtual((uintptr_t) vram), lbaCount - 1,
                       &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    }
    diskSize -= nBytes;
    SLLeoReadWrite(&cmdBlock, OS_READ, (startLba + lbaCount) - 1, osPhysicalToVirtual((uintptr_t) D_i1_80415190), 1,
                   &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    bcopy(D_i1_80415190, osPhysicalToVirtual((uintptr_t) vram + nBytes), diskSize);
    bzero((uintptr_t) vram + nBytes + diskSize, bssSize);
    D_8076CB44 = 0;
    return sp58;
}

s32 DiskDrive_LoadData(s32 startLba, void* vram, s32 diskSize, s32 bssSize) {
    void* bssStart;
    s32 sp58;
    s32 lbaCount;
    s32 nBytes;
    LEOCmd cmdBlock;

#ifdef PORT
    /* The runtime .ndd is a physical/zoned dump, but the leo LBA->byte path below is LOGICAL
       and drifts from the physical file across zone boundaries. On the port startLba is instead
       a tagged handle from SEGMENT_DISK_START (port_disk_segments.h), served straight out of
       the disk buffer; anything not yet mapped zero-fills rather than reading garbage. */
    {
        extern unsigned char* gdx_disk_buffer;
        extern unsigned int gdx_disk_size;
        extern void gdx_cki(const char* s, int v);
        u32 handle = (u32) startLba;
        u32 phys = 0;
        s32 mapped = 0;

        if ((handle >> 24) == (u32) GDX_DISK_TAG) {
            u32 table = (handle >> 16) & 0xFF;
            u32 record = handle & 0xFFFF;
            if (table == GDX_DTAB_DDCOURSE && record < GDX_DDCOURSE_COUNT) {
                phys = GDX_DDCOURSE_BASE + record * GDX_DDCOURSE_STRIDE;
                mapped = 1;
            }
        }

        if (mapped && gdx_disk_buffer != NULL &&
            (unsigned long long) phys + (u32) diskSize <= (unsigned long long) gdx_disk_size) {
            bcopy(gdx_disk_buffer + phys, vram, diskSize);
            gdx_cki("[dd] served phys", (s32) phys);
        } else {
            bzero(vram, diskSize);
            gdx_cki("[dd] zero-filled handle", startLba);
        }
        if (bssSize > 0) {
            bzero((unsigned char*) vram + diskSize, bssSize);
        }
        return 0;
    }
#else
    nBytes = 0;
    LeoByteToLBA(startLba, diskSize, &lbaCount);
    osVirtualToPhysical(vram);
    bssStart = (uintptr_t) vram + diskSize;
    osVirtualToPhysical(bssStart);
    osVirtualToPhysical((uintptr_t) bssStart + bssSize);

    PRINTF("========================================================\n");
    PRINTF("LBA %d, dist 0x%x-0x%x-0x%x , %dLBAs\n", startLba, vram, bssStart, (uintptr_t) bssStart + bssSize,
           lbaCount);
    PRINTF("========================================================\n");

    if (lbaCount - 1) {
        LeoLBAToByte(startLba, lbaCount - 1, &nBytes);
        func_80768AF0(&cmdBlock, OS_READ, startLba, osPhysicalToVirtual((uintptr_t) vram), lbaCount - 1,
                      &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    }
    diskSize -= nBytes;
    func_80768AF0(&cmdBlock, OS_READ, (startLba + lbaCount) - 1, osPhysicalToVirtual((uintptr_t) D_i1_80415190), 1,
                  &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    bcopy(D_i1_80415190, osPhysicalToVirtual((uintptr_t) vram + nBytes), diskSize);
    bzero((uintptr_t) vram + nBytes + diskSize, bssSize);
    return sp58;
#endif
}

s32 DiskDrive_LoadOverlay(s32 startLba, void* vram, s32 diskSize, s32 bssSize) {
#ifdef PORT
    // Overlays are statically compiled into the binary (mirrors Dma_LoadOverlay's
    // PORT path). The vram/bss linker markers are 1-byte host stubs — writing
    // the disk payload through them would corrupt adjacent memory.
    (void)startLba; (void)vram; (void)diskSize; (void)bssSize;
    return 0;
#else
    void* bssStart;
    s32 sp58;
    s32 lbaCount;
    s32 nBytes;
    LEOCmd cmdBlock;

    nBytes = 0;
    LeoByteToLBA(startLba, diskSize, &lbaCount);
    osVirtualToPhysical(vram);
    bssStart = (uintptr_t) vram + diskSize;
    osVirtualToPhysical(bssStart);
    osVirtualToPhysical((uintptr_t) bssStart + bssSize);

    PRINTF("========================================================\n");
    PRINTF("LBA %d, dist 0x%x-0x%x-0x%x , %dLBAs\n", startLba, vram, bssStart, (uintptr_t) bssStart + bssSize,
           lbaCount);
    PRINTF("========================================================\n");

    if (lbaCount - 1) {
        LeoLBAToByte(startLba, lbaCount - 1, &nBytes);
        func_80768A5C(&cmdBlock, OS_READ, startLba, osPhysicalToVirtual((uintptr_t) vram), lbaCount - 1,
                      &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    }
    diskSize -= nBytes;
    func_80768A5C(&cmdBlock, OS_READ, (startLba + lbaCount) - 1, osPhysicalToVirtual((uintptr_t) D_i1_80415190), 1,
                  &gDmaMesgQueue);
    osRecvMesg(&gDmaMesgQueue, NULL, OS_MESG_BLOCK);
    bcopy(&D_i1_80415190, osPhysicalToVirtual((uintptr_t) vram + nBytes), diskSize);
    bzero((uintptr_t) vram + nBytes + diskSize, bssSize);
    return sp58;
#endif /* PORT */
}

extern s32 D_800CCFB0;

s32 DiskDrive_LoadOverlayProgressBar(s32 startLba, void* vram, s32 diskSize, s32 bssSize) {
#ifdef PORT
    // Same as DiskDrive_LoadOverlay: compiled-in overlays, stub markers.
    (void)startLba; (void)vram; (void)diskSize; (void)bssSize;
    return 0;
#else
    void* bssStart;
    s32 sp70;
    s32 lbaCount;
    s32 progress;
    LEOCmd sp4C;
    OSMesg sp48;
    s32 pad[2];

    LeoByteToLBA(startLba, diskSize, &lbaCount);
    osVirtualToPhysical(vram);
    bssStart = (uintptr_t) vram + diskSize;
    osVirtualToPhysical(bssStart);
    osVirtualToPhysical((uintptr_t) bssStart + bssSize);
    func_80768A5C(&sp4C, OS_READ, startLba, vram, lbaCount, &gDmaMesgQueue);

    PRINTF("========================================================\n");
    PRINTF("LBA %d, dist 0x%x-0x%x-0x%x , %dLBAs\n", startLba, vram, bssStart, (uintptr_t) bssStart + bssSize,
           lbaCount);
    PRINTF("========================================================\n");

    if (D_8076CB40 != -1) {
        func_i10_8012B854();
    }

    while (osRecvMesg(&gDmaMesgQueue, &sp48, OS_MESG_NOBLOCK) == -1) {
        if (D_8076CB40 != -1) {
            osWritebackDCacheAll();
            progress = (D_800CCFB0 - D_8076CB40) / 6 + 160;
            if (progress > 192) {
                progress = 192;
            }
            func_i10_8012B894(progress);
            osWritebackDCacheAll();
        }
    }
    bzero(bssStart, bssSize);
    return sp70;
#endif /* PORT */
}

void DiskDrive_InitRomSegmentPairs(void) {
#ifdef PORT
    /* On hardware the 64DD IPL fills osAppNMIBuffer with the cartridge_offsets segment range.
       The port bypasses both the IPL and ovl_i11, so osAppNMIBuffer stays zero, the
       Dma_ClearRomCopy below copies nothing, and every EK asset load would DMA from ROM
       offset 0 and crash. This is the same table the base build compiles into
       cartridge_offsets.c, built from offsets the port DMA already resolves. */
    static const RomOffset kRomSegmentPairs[29][2] = {
        { (RomOffset)SEGMENT_ROM_START(audio_bank),                  (RomOffset)SEGMENT_ROM_END(audio_bank) },
        { (RomOffset)SEGMENT_ROM_START(audio_table),                 (RomOffset)SEGMENT_ROM_END(audio_table) },
        { (RomOffset)SEGMENT_ROM_START(audio_seq),                   (RomOffset)SEGMENT_ROM_END(audio_seq) },
        { (RomOffset)SEGMENT_ROM_START(boot_textures),               (RomOffset)SEGMENT_ROM_END(boot_textures) },
        { (RomOffset)SEGMENT_ROM_START(common_assets_compressed),    (RomOffset)SEGMENT_ROM_END(common_assets_compressed) },
        { (RomOffset)SEGMENT_ROM_START(course_data),                 (RomOffset)SEGMENT_ROM_END(course_data) },
        { (RomOffset)SEGMENT_ROM_START(super_textures),              (RomOffset)SEGMENT_ROM_END(super_textures) },
        { (RomOffset)SEGMENT_ROM_START(setup_gfx),                   (RomOffset)SEGMENT_ROM_END(setup_gfx) },
        { (RomOffset)SEGMENT_ROM_START(hud_gfx),                     (RomOffset)SEGMENT_ROM_END(hud_gfx) },
        { (RomOffset)SEGMENT_ROM_START(machine_global_gfx),          (RomOffset)SEGMENT_ROM_END(machine_global_gfx) },
        { (RomOffset)SEGMENT_ROM_START(machine_custom_gfx),          (RomOffset)SEGMENT_ROM_END(machine_custom_gfx) },
        { (RomOffset)SEGMENT_ROM_START(expansion_kit_textures_beta), (RomOffset)SEGMENT_ROM_END(expansion_kit_textures_beta) },
        { (RomOffset)SEGMENT_ROM_START(course_edit_textures_beta),   (RomOffset)SEGMENT_ROM_END(course_edit_textures_beta) },
        { (RomOffset)SEGMENT_ROM_START(staff_ghost_records),         (RomOffset)SEGMENT_ROM_END(staff_ghost_records) },
        { (RomOffset)SEGMENT_ROM_START(machine_models),              (RomOffset)SEGMENT_ROM_END(machine_models) },
        { (RomOffset)SEGMENT_ROM_START(course_track_gfx),            (RomOffset)SEGMENT_ROM_END(course_track_gfx) },
        { (RomOffset)SEGMENT_ROM_START(mute_city_textures),          (RomOffset)SEGMENT_ROM_END(mute_city_textures) },
        { (RomOffset)SEGMENT_ROM_START(port_town_textures),          (RomOffset)SEGMENT_ROM_END(port_town_textures) },
        { (RomOffset)SEGMENT_ROM_START(big_blue_textures),           (RomOffset)SEGMENT_ROM_END(big_blue_textures) },
        { (RomOffset)SEGMENT_ROM_START(sand_ocean_textures),         (RomOffset)SEGMENT_ROM_END(sand_ocean_textures) },
        { (RomOffset)SEGMENT_ROM_START(devils_forest_textures),      (RomOffset)SEGMENT_ROM_END(devils_forest_textures) },
        { (RomOffset)SEGMENT_ROM_START(white_land_textures),         (RomOffset)SEGMENT_ROM_END(white_land_textures) },
        { (RomOffset)SEGMENT_ROM_START(sector_textures),             (RomOffset)SEGMENT_ROM_END(sector_textures) },
        { (RomOffset)SEGMENT_ROM_START(red_canyon_textures),         (RomOffset)SEGMENT_ROM_END(red_canyon_textures) },
        { (RomOffset)SEGMENT_ROM_START(fire_field_textures),         (RomOffset)SEGMENT_ROM_END(fire_field_textures) },
        { (RomOffset)SEGMENT_ROM_START(silence_textures),            (RomOffset)SEGMENT_ROM_END(silence_textures) },
        { (RomOffset)SEGMENT_ROM_START(ending_venue_textures),       (RomOffset)SEGMENT_ROM_END(ending_venue_textures) },
        { (RomOffset)SEGMENT_ROM_START(podium_gfx),                  (RomOffset)SEGMENT_ROM_END(podium_gfx) },
        { (RomOffset)SEGMENT_ROM_START(create_machine_textures),     (RomOffset)SEGMENT_ROM_END(create_machine_textures) },
    };
    s32 i;
    for (i = 0; i < 29; i++) {
        gRomSegmentPairs[i][0] = kRomSegmentPairs[i][0];
        gRomSegmentPairs[i][1] = kRomSegmentPairs[i][1];
    }
#else
    size_t size = osAppNMIBuffer[1] - osAppNMIBuffer[0];
    Dma_ClearRomCopy(osAppNMIBuffer[0], gRomSegmentPairs, size);
#endif
}

extern FrameBuffer* gFrameBuffers[];

void DiskDrive_DrawErrorBackground(void) {
    u8 i;
    u64* var_v0;
    u64* temp;

    SLForceWritebackDCacheAll();

    for (i = 0; i < 3; i++) {
        // FAKE
        temp = gFrameBuffers[i]->buffer;
        var_v0 = &gFrameBuffers[i]->buffer[19199];
        while (var_v0 >= temp) {
            var_v0--;
            *(var_v0 + 1) = 0x0001000100010001;
        }
    }
    SLForceWritebackDCacheAll();
}
