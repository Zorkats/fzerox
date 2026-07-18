#include "global.h"
#include "audio.h"
#include "fzx_game.h"
#include "fzx_thread.h"

#ifdef PORT
extern void gdx_ck(const char* s);
#define GDX_CK(n) do { if (gdx_diag_verbose()) gdx_ck("[game] ck" #n); } while (0)
#else
#define GDX_CK(n)
#endif

GfxPool* gGfxPool;
OSTask* sGfxTask;
Gfx* gMasterDisp;
s32 D_800DCCFC;
s32 D_800DCD00;
s32 D_800DCD04;
s32 D_800DCD08;
s32 D_800DCD0C;
OSMesg D_800DCD10;
uintptr_t gMainVramStart;
uintptr_t gMainVramEnd;
uintptr_t gOvl2VramStart;
uintptr_t gOvl2VramEnd;
uintptr_t gOvl3VramStart;
uintptr_t gOvl3VramEnd;
uintptr_t gOvl4VramStart;
uintptr_t gOvl4VramEnd;
uintptr_t gOvlCourseSelectVramStart;
uintptr_t gOvlCourseSelectVramEnd;
uintptr_t gOvl6VramStart;
uintptr_t gOvl6VramEnd;
uintptr_t gOvl7VramStart;
uintptr_t gOvl7VramEnd;
uintptr_t gOvl8VramStart;
uintptr_t gOvl8VramEnd;
uintptr_t gOvl9VramStart;
uintptr_t gOvl9VramEnd;
uintptr_t gOvl10VramStart;
uintptr_t gOvl10VramEnd;
uintptr_t gLeoVramStart;
uintptr_t gLeoVramEnd;
UNUSED uintptr_t D_800DCD6C;
UNUSED uintptr_t D_800DCD70;
#ifdef EXPANSION_KIT
uintptr_t gOvlCourseEditVramStart;
uintptr_t gOvlCourseEditVramEnd;
uintptr_t gOvlMachineCreateVramStart;
uintptr_t gOvlMachineCreateVramEnd;
#endif
uintptr_t gFramebuffer1VramStart;
uintptr_t gFramebuffer1VramEnd;
uintptr_t gFramebuffer2VramStart;
uintptr_t gFramebuffer2VramEnd;
uintptr_t gFramebuffer3VramStart;
uintptr_t gFramebuffer3VramEnd;
uintptr_t gFramebufferUnusedVramStart;
uintptr_t gFramebufferUnusedVramEnd;
uintptr_t gGfxPoolVramStart;
uintptr_t gGfxPoolVramEnd;
uintptr_t gUnkBssVramStart;
uintptr_t gUnkBssVramEnd;
uintptr_t gSegment16C8A0VramStart;
uintptr_t gSegment16C8A0VramEnd;
uintptr_t gSegment17B1E0VramStart;
uintptr_t gSegment17B1E0VramEnd;
uintptr_t gSegment17B960VramStart;
uintptr_t gSegment17B960VramEnd;
uintptr_t gExpansionKitTexturesVramStart;
uintptr_t gExpansionKitTexturesVramEnd;
uintptr_t gSegment1B8550VramStart;
uintptr_t gSegment1B8550VramEnd;
uintptr_t gSegment1E23F0VramStart;
uintptr_t gSegment1E23F0VramEnd;
uintptr_t gSegment22B0A0VramStart;
uintptr_t gSegment22B0A0VramEnd;
#ifdef PORT
/* Segment 9 has two mutually-exclusive owners in the Expansion Kit. Keep
 * separate persistent carves so host command-range metadata from one image
 * can never describe bytes after the other image replaces it. */
uintptr_t gGdxMachineModelsVramStart;
uintptr_t gGdxMachineModelsVramEnd;
uintptr_t gGdxCourseEditTexturesVramStart;
uintptr_t gGdxCourseEditTexturesVramEnd;
#endif
uintptr_t gSegment235130VramStart;
uintptr_t gSegment235130VramEnd;
uintptr_t gSegment2738A0VramStart;
uintptr_t gSegment2738A0VramEnd;
uintptr_t gCourseEditTexturesVramStart;
uintptr_t gCourseEditTexturesVramEnd;
uintptr_t gCreateMachineTexturesVramStart;
uintptr_t gCreateMachineTexturesVramEnd;
uintptr_t D_800DCDFC;
uintptr_t D_800DCE00;
uintptr_t gBuffersVramStart;
uintptr_t gBuffersVramEnd;
uintptr_t gUnkGfxVramStart;
uintptr_t gUnkGfxVramEnd;
uintptr_t gUnkContextVramStart;
uintptr_t gUnkContextVramEnd;
uintptr_t gAudioContextVramStart;
uintptr_t gAudioContextVramEnd;

void func_80067AE0(void) {
    s32 temp_t7;

    temp_t7 = D_800DCD08;
    D_800DCD08 = D_800DCD04;
    D_800DCD04 = D_800DCD0C;
    D_800DCD0C = temp_t7;
}

extern GfxPool D_8024DCE0[2];
extern OSTask D_802A6AC0[];

void Gfx_InitBuffer(void) {

    D_800DCD00 ^= 1;
    D_800DCCFC ^= 1;
    sGfxTask = &D_802A6AC0[D_800DCCFC];
    gGfxPool = &D_8024DCE0[D_800DCCFC];
    Segment_SetPhysicalAddress(1, gGfxPool);
    gMasterDisp = gGfxPool->gfxBuffer;
}

extern unk_80128C94* D_80128C90;

void Gfx_LoadSegments(void) {
#ifdef EXPANSION_KIT
    Segment_SetPhysicalAddress(6, &D_80128C90[D_800DCCFC]);
#endif
    gMasterDisp = Segment_SetTableAddresses(gMasterDisp);
}

extern bool gInCourseEditor;

void Gfx_FullSync(void) {

#ifdef EXPANSION_KIT
    if (gInCourseEditor) {
        gMasterDisp = func_xk1_8002ED64(gMasterDisp);
    }

    gMasterDisp = func_xk1_8002F9DC(gMasterDisp);
#endif

    gDPFullSync(gMasterDisp++);
    gSPEndDisplayList(gMasterDisp++);
}

extern OSMesgQueue gMainThreadMesgQueue;
extern s32 gGameMode;
extern OSTask* gCurGfxTask;

void Gfx_SetTask(OSTask* task) {

    task->t.type = M_GFXTASK;
    task->t.flags = OS_TASK_LOADABLE;
    task->t.ucode_boot = (u64*) rspbootTextStart;
    task->t.ucode_boot_size = (uintptr_t) rspbootTextEnd - (uintptr_t) rspbootTextStart;

    switch (gGameMode & GAMEMODE_F3D_MASK) {
        case GFXMODE_F3DEX:
            task->t.ucode = (u64*) gspF3DEX2_fifoTextStart;
            task->t.ucode_data = (u64*) gspF3DEX2_fifoDataStart;
            break;
        case GFXMODE_F3DLX:
            task->t.ucode = (u64*) gspF3DLX2_Rej_fifoTextStart;
            task->t.ucode_data = (u64*) gspF3DLX2_Rej_fifoDataStart;
            break;
        case GFXMODE_F3DFLX:
            task->t.ucode = (u64*) gspF3DFLX2_Rej_fifoTextStart;
            task->t.ucode_data = (u64*) gspF3DFLX2_Rej_fifoDataStart;
            break;
    }

    task->t.ucode_size = SP_UCODE_SIZE;
    task->t.ucode_data_size = SP_UCODE_DATA_SIZE;
    task->t.dram_stack = (u64*) gDramStack;
    task->t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    task->t.output_buff = (u64*) gTaskOutputBuffer;
    task->t.output_buff_size = (u64*) (gTaskOutputBuffer + ARRAY_COUNT(gTaskOutputBuffer));
    task->t.data_ptr = (u64*) gGfxPool->gfxBuffer;
    task->t.data_size = (size_t) (gMasterDisp - gGfxPool->gfxBuffer) * sizeof(Gfx);
    task->t.yield_data_ptr = (u64*) gOSYieldData;
    task->t.yield_data_size = OS_YIELD_DATA_SIZE;
    gCurGfxTask = task;
    osSendMesg(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_GFX_TASK_SET, OS_MESG_BLOCK);
}

extern OSMesgQueue D_800DCAB0;
extern OSMesgQueue D_800DCAC8;
extern FrameBuffer* gFrameBuffers[];

void func_80067D64(void) {
    GDX_CK(H1_67D64_wait_vi);
    osRecvMesg(&D_800DCAB0, &D_800DCD10, OS_MESG_BLOCK);
    GDX_CK(H2_67D64_got_vi);
    Audio_Update();
    Gfx_InitBuffer();
    GDX_CK(H3_67D64_pre_gamemode);
    func_800690FC();
    GDX_CK(H4_67D64_post_gamemode);
    Gfx_LoadSegments();
    GDX_CK(H5_67D64_pre_draw);
    gMasterDisp = func_80069698(gMasterDisp);
    GDX_CK(H6_67D64_post_draw);
    Gfx_FullSync();
    GDX_CK(H7_67D64_wait_dp);
    osRecvMesg(&D_800DCAC8, &D_800DCD10, OS_MESG_BLOCK);
    GDX_CK(H8_67D64_got_dp);

    while (osDpGetStatus() &
           (DPC_STATUS_DMA_BUSY | DPC_STATUS_CMD_BUSY | DPC_STATUS_PIPE_BUSY | DPC_STATUS_TMEM_BUSY)) {}

    Segment_LoadAssets();
    Transition_SetBackgroundBuffer();
    osViSwapBuffer(gFrameBuffers[D_800DCD00]);
    GDX_CK(H9_67D64_wait_fb);

    while (osViGetCurrentFramebuffer() != gFrameBuffers[D_800DCD00]) {}

    GDX_CK(HA_67D64_set_task);
    Gfx_SetTask(sGfxTask);
    GDX_CK(HB_67D64_done);
}

void func_80067E98(void) {
    s32 retries = 100000;

    osRecvMesg(&D_800DCAB0, &D_800DCD10, OS_MESG_BLOCK);
    Gfx_InitBuffer();
    func_800690FC();
    func_80067AE0();
    Gfx_LoadSegments();
    gMasterDisp = func_80069698(gMasterDisp);
    Gfx_FullSync();
    Audio_Update();
    osRecvMesg(&D_800DCAC8, &D_800DCD10, OS_MESG_BLOCK);
    Transition_SetBackgroundBuffer();
    osViSwapBuffer(gFrameBuffers[D_800DCD08]);
    Segment_LoadAssets();

    while ((osViGetCurrentFramebuffer() == gFrameBuffers[D_800DCD04] ||
            osViGetNextFramebuffer() == gFrameBuffers[D_800DCD04]) &&
           retries != 0) {
        retries--;
    }

    Gfx_SetTask(sGfxTask);
}

u32 gGameFrameCount = 0;
s16 D_800CCFE4 = 2;
s16 D_800CCFE8 = 2;

extern bool gRamDDCompatible;
extern s16 gSettingSoundMode;
extern s32 D_8076CB40;
extern s32 D_800CCFB0;
extern s32 gLeoDriveConnectionState;

extern unk_80225800 D_80225800;

void Game_ThreadEntry(void* entry) {
    s32 startTime;
    OSMesg msgBuf[1];

    startTime = osGetTime();
    GDX_CK(G1_game_entry);
    /* Na_Guitor_Start kicks off the DD-exclusive guitar SE / DDBGM bank load
     * state machine (func_807427C0, ticked every frame from
     * Audio_SetupCreateTask). That loader issues MEDIUM_LBA disk reads
     * unconditionally; with no 64DD disk connected the port's leo stub still
     * signals load completion (so callers don't deadlock) but leaves the
     * destination buffers zeroed/garbage, which crashes shortly after boot
     * once the game tries to use the "loaded" font/seq data. Real hardware
     * gates all DD-disk-dependent work behind gLeoDriveConnectionState != 0
     * (see sys_main.c, sys_gfx.c G7/title.c/save.c) — gRamDDCompatible alone
     * is not a useful gate here since EK builds always set it true regardless
     * of whether a disk is actually connected. Apply the same convention so
     * the base game still boots with EK compiled in but no disk present. */
    if (gLeoDriveConnectionState != 0) {
        Audio_GuitarSeqStart();
    }
    GDX_CK(G2_guitar_seq_start);
    osRecvMesg(&D_800DCAB0, msgBuf, OS_MESG_BLOCK);
    GDX_CK(G3_first_vi_handshake);

    // Segment Start and End Pairs
    gMainVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(main));
    gMainVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(main));
    gOvl2VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i2));
    gOvl2VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i2));
    gOvl3VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i3));
    gOvl3VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i3));
    gOvl4VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i4));
    gOvl4VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i4));
    gOvlCourseSelectVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(course_select));
    gOvlCourseSelectVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(course_select));
    gOvl6VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i6));
    gOvl6VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i6));
    gOvl7VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ending));
    gOvl7VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ending));
    gOvl8VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(records));
    gOvl8VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(records));
    gOvl9VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i9));
    gOvl9VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i9));
    gOvl10VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(ovl_i10));
    gOvl10VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(ovl_i10));
    gFramebuffer1VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(framebuffer1));
    gFramebuffer1VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(framebuffer1));
    gFramebuffer2VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(framebuffer2));
    gFramebuffer2VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(framebuffer2));
    gFramebuffer3VramStart = osVirtualToPhysical(SEGMENT_VRAM_START(framebuffer3));
    gFramebuffer3VramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(framebuffer3));
    gFramebufferUnusedVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(framebuffer_unused));
    gFramebufferUnusedVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(framebuffer_unused));
    gBuffersVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(buffers));
    gBuffersVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(buffers));
    gUnkGfxVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(unk_gfx_segment));
    gUnkGfxVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(unk_gfx_segment));
    gUnkContextVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(game_context));
    gUnkContextVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(game_context));
    gAudioContextVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(audio_context));
    gAudioContextVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(audio_context) + 0x10);
    gGfxPoolVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(gfxpool));
    gGfxPoolVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(gfxpool));
    gUnkBssVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(unk_bss_segment));
    gUnkBssVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(unk_bss_segment));

    gSegment16C8A0VramStart = gBuffersVramEnd;
    gSegment16C8A0VramEnd = gSegment16C8A0VramStart + (size_t) SEGMENT_DATA_SIZE_CONST(course_track_gfx);

    gSegment17B1E0VramStart = gSegment16C8A0VramEnd;
    gSegment17B1E0VramEnd = gSegment17B1E0VramStart + (size_t) SEGMENT_VRAM_SIZE(setup_gfx);

    gSegment17B960VramStart = gSegment17B1E0VramEnd;
    gSegment17B960VramEnd = gSegment17B960VramStart + (size_t) SEGMENT_VRAM_SIZE(machine_custom_gfx);

#ifdef PORT
    {
        /* Allocate proper RDRAM backing for race GFX segments.
         * On PORT, SEGMENT_DATA_SIZE_CONST and SEGMENT_VRAM_END return BSS stub
         * values (garbage physical addresses), so we must carve real RDRAM here
         * BEFORE Segment_SetAddress() uses these variables. */
        extern unsigned char* gdx_rdram;
        extern void* gdx_rdram_alloc_raw(size_t size, size_t align);
        void* seg8  = gdx_rdram_alloc_raw(PORT_course_track_gfx_DECODED_SIZE, 16u);
        void* seg3  = gdx_rdram_alloc_raw(
            (size_t)(PORT_setup_gfx_ROM_END - PORT_setup_gfx_ROM_START), 16u);
        void* seg3b = gdx_rdram_alloc_raw(
            (size_t)(PORT_machine_custom_gfx_ROM_END - PORT_machine_custom_gfx_ROM_START), 16u);
        gSegment16C8A0VramStart = (uintptr_t)((unsigned char*)seg8  - gdx_rdram);
        gSegment16C8A0VramEnd   = gSegment16C8A0VramStart + PORT_course_track_gfx_DECODED_SIZE;
        gSegment17B1E0VramStart = (uintptr_t)((unsigned char*)seg3  - gdx_rdram);
        gSegment17B1E0VramEnd   = gSegment17B1E0VramStart +
            (size_t)(PORT_setup_gfx_ROM_END - PORT_setup_gfx_ROM_START);
        gSegment17B960VramStart = (uintptr_t)((unsigned char*)seg3b - gdx_rdram);
        gSegment17B960VramEnd   = gSegment17B960VramStart +
            (size_t)(PORT_machine_custom_gfx_ROM_END - PORT_machine_custom_gfx_ROM_START);
    }
#endif /* PORT */

    gSegment1B8550VramStart = gSegment17B960VramEnd;
    gSegment1B8550VramEnd = gSegment1B8550VramStart + (size_t) SEGMENT_VRAM_SIZE(hud_gfx);

    gSegment1E23F0VramStart = gOvl3VramEnd;
    gSegment1E23F0VramEnd = gSegment1E23F0VramStart + (size_t) SEGMENT_VRAM_SIZE(machine_global_gfx);

    gSegment22B0A0VramStart = gSegment17B960VramEnd;
    gSegment22B0A0VramEnd = gSegment22B0A0VramStart + (size_t) SEGMENT_DATA_SIZE_CONST(machine_models);

#ifdef PORT
    {
        /* Same problem the carve above solves for track/setup/machine_custom:
         * the assignments just made compute hud_gfx (segment 4), machine_global
         * (segment 7), and machine_models staging from BSS stub symbols —
         * garbage sizes over overlapping regions. Segments 4/7 never had valid
         * backing on the port (countdown faces / start arc / machine-part
         * graphics missing or corrupt). Carve real RDRAM. The segment-4 buffer
         * serves hud_gfx in races AND create_machine_textures in Create
         * Machine — size it for the larger of the two. */
        extern unsigned char* gdx_rdram;
        extern void* gdx_rdram_alloc_raw(size_t size, size_t align);
        size_t hudSize = (size_t)(PORT_hud_gfx_ROM_END - PORT_hud_gfx_ROM_START);
        size_t createMachineSize =
            (size_t)(PORT_create_machine_textures_ROM_END - PORT_create_machine_textures_ROM_START);
        size_t seg4Size = (hudSize > createMachineSize) ? hudSize : createMachineSize;
        size_t seg7Size = (size_t)(PORT_machine_global_gfx_ROM_END - PORT_machine_global_gfx_ROM_START);
        size_t modelsSize = 0x186C8u;
        size_t courseEditSize = 0;
        void* seg4buf = gdx_rdram_alloc_raw(seg4Size, 16u);
        void* seg7buf = gdx_rdram_alloc_raw(seg7Size, 16u);
        void* modelsBuf;
        void* courseEditBuf = NULL;

        /* machine_models is MIO0-compressed in the ROM. The old carve used
         * the compressed span (0xA090) as segment capacity even though the
         * console exposes the 0x186C8-byte decoded image at segment 9. Read
         * the authoritative decoded size from the MIO0 header, retaining the
         * known retail size as a guarded fallback. */
        {
            extern unsigned char* gdx_rom_buffer;
            extern size_t gdx_rom_size;
            const size_t start = (size_t)PORT_machine_models_ROM_START;
            if (gdx_rom_buffer != NULL && start + 8u <= gdx_rom_size &&
                gdx_rom_buffer[start + 0] == 'M' && gdx_rom_buffer[start + 1] == 'I' &&
                gdx_rom_buffer[start + 2] == 'O' && gdx_rom_buffer[start + 3] == '0') {
                size_t decoded = ((size_t)gdx_rom_buffer[start + 4] << 24) |
                                 ((size_t)gdx_rom_buffer[start + 5] << 16) |
                                 ((size_t)gdx_rom_buffer[start + 6] << 8) |
                                 (size_t)gdx_rom_buffer[start + 7];
                if (decoded != 0 && decoded <= 0x1000000u) {
                    modelsSize = decoded;
                }
            }
        }
#ifdef EXPANSION_KIT
        {
            extern unsigned int gdx_ek_segment_image_size(unsigned char segment);
            courseEditSize = (size_t)gdx_ek_segment_image_size(9u);
        }
#endif
        modelsBuf = gdx_rdram_alloc_raw(modelsSize, 16u);
        if (courseEditSize != 0) {
            courseEditBuf = gdx_rdram_alloc_raw(courseEditSize, 16u);
        }

        gSegment1B8550VramStart = (uintptr_t)((unsigned char*)seg4buf - gdx_rdram);
        gSegment1B8550VramEnd = gSegment1B8550VramStart + seg4Size;
        gSegment1E23F0VramStart = (uintptr_t)((unsigned char*)seg7buf - gdx_rdram);
        gSegment1E23F0VramEnd = gSegment1E23F0VramStart + seg7Size;
        gGdxMachineModelsVramStart = (uintptr_t)((unsigned char*)modelsBuf - gdx_rdram);
        gGdxMachineModelsVramEnd = gGdxMachineModelsVramStart + modelsSize;
        if (courseEditBuf != NULL) {
            gGdxCourseEditTexturesVramStart =
                (uintptr_t)((unsigned char*)courseEditBuf - gdx_rdram);
            gGdxCourseEditTexturesVramEnd = gGdxCourseEditTexturesVramStart + courseEditSize;
        } else {
            gGdxCourseEditTexturesVramStart = 0;
            gGdxCourseEditTexturesVramEnd = 0;
        }
        gSegment22B0A0VramStart = gGdxMachineModelsVramStart;
        gSegment22B0A0VramEnd = gGdxMachineModelsVramEnd;
    }
#endif /* PORT */

    D_800DCDFC = gSegment17B1E0VramEnd;
#ifndef EXPANSION_KIT // TODO: USE MACRO FOR SIZE
    D_800DCE00 = D_800DCDFC + 0x1F820;
#else
    D_800DCE00 = D_800DCDFC + 0x35E10;
#endif

    gCourseEditTexturesVramStart = D_800DCE00;
#ifndef EXPANSION_KIT
    gCourseEditTexturesVramEnd = gCourseEditTexturesVramStart + (size_t) SEGMENT_VRAM_SIZE(course_edit_textures_beta);
#else
    gCourseEditTexturesVramEnd = gCourseEditTexturesVramStart + (size_t) SEGMENT_VRAM_SIZE(course_edit_textures);
#endif

    gCreateMachineTexturesVramStart = D_800DCE00;
    gCreateMachineTexturesVramEnd =
        gCreateMachineTexturesVramStart + (size_t) SEGMENT_VRAM_SIZE(create_machine_textures);

#ifndef EXPANSION_KIT
    gLeoVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(leo));
    gLeoVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(leo));
#else
    gOvlCourseEditVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(course_edit));
    gOvlCourseEditVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(course_edit));

    gExpansionKitTexturesVramStart = gOvlCourseEditVramEnd;
    gExpansionKitTexturesVramEnd = gExpansionKitTexturesVramStart + (size_t) SEGMENT_VRAM_SIZE(expansion_kit_textures);

    gOvlMachineCreateVramStart = osVirtualToPhysical(SEGMENT_VRAM_START(machine_create));
    gOvlMachineCreateVramEnd = osVirtualToPhysical(SEGMENT_VRAM_END(machine_create));
#endif

    GDX_CK(G4_segments_computed);
    // Setup memory
    Segment_SetAddress(0, 0);
    Segment_SetAddress(2, gUnkBssVramStart);
    Segment_SetAddress(8, gSegment16C8A0VramStart);
    Segment_SetAddress(3, gSegment17B1E0VramStart);
#ifdef EXPANSION_KIT
    Segment_SetPhysicalAddress(6, &D_80128C90[D_800DCCFC]);

    Controller_Init();
#endif

    GDX_CK(G5_pre_arena_init);
    Arena_DefaultStartInit();
    Arena_EndInit();
    GDX_CK(G6_post_arena_init);

#if defined(EXPANSION_KIT) && !defined(PORT)
    /* Wait for the EK audio system to finish loading the BGM bank from disk.
       PORT: the audio thread is short-circuited (sys_audio.c), so these load
       states never advance past 1 — spinning here deadlocks the cooperative
       scheduler. Skipped until the audio slice lands. */
    while (func_80742790() != 2) {}
    while (func_807424CC() != 0) {}
#endif

#ifndef EXPANSION_KIT
    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i2), SEGMENT_TEXT_SIZE(ovl_i2), SEGMENT_DATA_START(ovl_i2),
                        SEGMENT_DATA_END(ovl_i2) - SEGMENT_DATA_START(ovl_i2));
    Dma_LoadOverlay(SEGMENT_ROM_START(ovl_i2), SEGMENT_VRAM_START(ovl_i2), SEGMENT_ROM_SIZE(ovl_i2),
                    SEGMENT_BSS_START(ovl_i2), SEGMENT_BSS_SIZE(ovl_i2));
#endif

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i10), SEGMENT_TEXT_SIZE(ovl_i10), SEGMENT_DATA_START(ovl_i10),
                        SEGMENT_DATA_SIZE(ovl_i10));
#ifndef EXPANSION_KIT
    Dma_LoadOverlay(SEGMENT_ROM_START(ovl_i10), SEGMENT_VRAM_START(ovl_i10), SEGMENT_ROM_SIZE(ovl_i10),
                    SEGMENT_BSS_START(ovl_i10), SEGMENT_BSS_SIZE(ovl_i10));
#else
    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ovl_i10), SEGMENT_VRAM_START(ovl_i10),
                                     SEGMENT_DISK_SIZE(ovl_i10), SEGMENT_BSS_SIZE(ovl_i10));
#endif

#ifdef EXPANSION_KIT

    D_8076CB40 = D_800CCFB0;

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(expansion_kit), SEGMENT_TEXT_SIZE(expansion_kit),
                        SEGMENT_DATA_START(expansion_kit),
                        SEGMENT_DATA_END(expansion_kit) - SEGMENT_DATA_START(expansion_kit));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(expansion_kit), SEGMENT_VRAM_START(expansion_kit),
                                     SEGMENT_DISK_SIZE(expansion_kit), SEGMENT_BSS_SIZE(expansion_kit));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i3), SEGMENT_TEXT_SIZE(ovl_i3), SEGMENT_DATA_START(ovl_i3),
                        SEGMENT_DATA_SIZE(ovl_i3));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ovl_i3), SEGMENT_VRAM_START(ovl_i3), SEGMENT_DISK_SIZE(ovl_i3),
                                     SEGMENT_BSS_SIZE(ovl_i3));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i4), SEGMENT_TEXT_SIZE(ovl_i4), SEGMENT_DATA_START(ovl_i4),
                        SEGMENT_DATA_SIZE(ovl_i4));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ovl_i4), SEGMENT_VRAM_START(ovl_i4), SEGMENT_DISK_SIZE(ovl_i4),
                                     SEGMENT_BSS_SIZE(ovl_i4));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(course_select), SEGMENT_TEXT_SIZE(course_select),
                        SEGMENT_DATA_START(course_select), SEGMENT_DATA_SIZE(course_select));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(course_select), SEGMENT_VRAM_START(course_select),
                                     SEGMENT_DISK_SIZE(course_select), SEGMENT_BSS_SIZE(course_select));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i6), SEGMENT_TEXT_SIZE(ovl_i6), SEGMENT_DATA_START(ovl_i6),
                        SEGMENT_DATA_SIZE(ovl_i6));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ovl_i6), SEGMENT_VRAM_START(ovl_i6), SEGMENT_DISK_SIZE(ovl_i6),
                                     SEGMENT_BSS_SIZE(ovl_i6));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ending), SEGMENT_TEXT_SIZE(ending), SEGMENT_DATA_START(ending),
                        SEGMENT_DATA_SIZE(ending));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ending), SEGMENT_VRAM_START(ending), SEGMENT_DISK_SIZE(ending),
                                     SEGMENT_BSS_SIZE(ending));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(records), SEGMENT_TEXT_SIZE(records), SEGMENT_DATA_START(records),
                        SEGMENT_DATA_SIZE(records));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(records), SEGMENT_VRAM_START(records),
                                     SEGMENT_DISK_SIZE(records), SEGMENT_BSS_SIZE(records));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i2), SEGMENT_TEXT_SIZE(ovl_i2), SEGMENT_DATA_START(ovl_i2),
                        SEGMENT_DATA_END(ovl_i2) - SEGMENT_DATA_START(ovl_i2));

    DiskDrive_LoadOverlayProgressBar(SEGMENT_DISK_START(ovl_i2), SEGMENT_VRAM_START(ovl_i2), SEGMENT_DISK_SIZE(ovl_i2),
                                     SEGMENT_BSS_SIZE(ovl_i2));

    CLEAR_OVERLAY_CACHE(SEGMENT_TEXT_START(ovl_i9), SEGMENT_TEXT_SIZE(ovl_i9), SEGMENT_DATA_START(ovl_i9),
                        SEGMENT_DATA_SIZE(ovl_i9));

    DiskDrive_LoadOverlay(SEGMENT_DISK_START(ovl_i9), SEGMENT_VRAM_START(ovl_i9), SEGMENT_DISK_SIZE(ovl_i9),
                          SEGMENT_BSS_SIZE(ovl_i9));

    D_8076CB40 = -1;
    /* func_i10_8012B904 reads the course-edit cup track names / options off the
     * 64DD disk via func_8076852C -> func_80767F14, which blocks on
     * osSendMesg(&D_807C6E90, ..., OS_MESG_BLOCK). That queue is only ever
     * osCreateMesgQueue'd from inside sSys6Thread's own entry function
     * (func_80767958, sys/disk/75000.c), which sys_main.c only starts when a
     * real drive is detected (gLeoDriveConnectionState == 1, promoted to 2).
     * With no disk connected, sSys6Thread is created but never started, so
     * this send blocks forever on an uninitialized queue with no consumer --
     * the game thread parks here and ckG7_overlays_done never prints (boot
     * stays on a black screen). This data only exists on a real EK disk
     * anyway (it's the saved course-edit slot names), so gate it the same
     * way as the other disk-dependent boot work: on gLeoDriveConnectionState,
     * not gRamDDCompatible (see the G2 guitar-seq gate above). */
    if (gLeoDriveConnectionState != 0) {
        func_i10_8012B904();
    }
#endif
    GDX_CK(G7_overlays_done);

#ifndef PORT
    // These DMA calls load track/race assets into N64 VRAM addresses.
    // On host the destination pointers are invalid (N64 physical addrs, not host heap).
    // Guarded until proper segment memory management is in place (S6+).
    CLEAR_DATA_CACHE(osPhysicalToVirtual(gSegment16C8A0VramStart), SEGMENT_DATA_SIZE_CONST(course_track_gfx));
#ifndef EXPANSION_KIT
    Dma_LoadAssets(SEGMENT_ROM_START(course_track_gfx),
                   (uintptr_t) osPhysicalToVirtual(gSegment16C8A0VramStart) +
                       (size_t) SEGMENT_DATA_SIZE_CONST(course_track_gfx),
                   SEGMENT_ROM_SIZE(course_track_gfx));
#else
    Dma_LoadAssets(gRomSegmentPairs[15][0],
                   (uintptr_t) osPhysicalToVirtual(gSegment16C8A0VramStart) +
                       (size_t) SEGMENT_DATA_SIZE_CONST(course_track_gfx),
                   SEGMENT_VRAM_SIZE(course_track_gfx));
#endif

    mio0Decode((uintptr_t) osPhysicalToVirtual(gSegment16C8A0VramStart) +
                   (size_t) SEGMENT_DATA_SIZE_CONST(course_track_gfx),
               osPhysicalToVirtual(gSegment16C8A0VramStart));

#ifndef EXPANSION_KIT
    Dma_LoadAssets(SEGMENT_ROM_START(setup_gfx), osPhysicalToVirtual(gSegment17B1E0VramStart),
                   SEGMENT_ROM_SIZE(setup_gfx));
    Dma_LoadAssets(SEGMENT_ROM_START(machine_custom_gfx), osPhysicalToVirtual(gSegment17B960VramStart),
                   SEGMENT_ROM_SIZE(machine_custom_gfx));
#else
    Dma_LoadAssets(gRomSegmentPairs[7][0], (uintptr_t) osPhysicalToVirtual(gSegment17B1E0VramStart),
                   SEGMENT_VRAM_SIZE(setup_gfx));

    Dma_LoadAssets(gRomSegmentPairs[10][0], (uintptr_t) osPhysicalToVirtual(gSegment17B960VramStart),
                   SEGMENT_VRAM_SIZE(machine_custom_gfx));
#endif
#else /* PORT */
    {
        /* Load race geometry segments directly from ROM into the RDRAM regions
         * carved above. course_track_gfx is MIO0-compressed; the others are raw. */
        extern unsigned char* gdx_rom_buffer;
        mio0Decode(gdx_rom_buffer + PORT_course_track_gfx_ROM_START,
                   osPhysicalToVirtual(gSegment16C8A0VramStart));
        /* Byte-order pass (2026-07-11, exploded-decorations root cause): the
           carve above is what gSegments[8] serves at draw time, but only the
           bridge's separate heap image ever received the generated fixups.
           Decoration DLs and their Vtx blocks therefore rendered from raw
           big-endian bytes -- the interpreter read every s16 coordinate
           byte-swapped (x256-ish values: boards exploded across the screen,
           start arc invisible). Apply the same generated fixup pass (kind-1
           Gfx word swaps + kind-3 Vtx swaps) to the carve so both copies of
           the image agree. */
        {
            extern void gdx_fixup_asset_segment_image(unsigned char segment, unsigned int rom_base,
                                                      unsigned char* data, unsigned int size);
            gdx_fixup_asset_segment_image(0x08u, PORT_course_track_gfx_ROM_START,
                                          (unsigned char*) osPhysicalToVirtual(gSegment16C8A0VramStart),
                                          (unsigned int) PORT_course_track_gfx_DECODED_SIZE);
        }
        Dma_LoadAssets(SEGMENT_ROM_START(setup_gfx),
                       osPhysicalToVirtual(gSegment17B1E0VramStart),
                       SEGMENT_ROM_SIZE(setup_gfx));
        /* Byte-order pass for the segment-3 carve (2026-07-15, NINTEX/Overhead sign root
           cause): setup_gfx is the only decoration source that segment 8's fixup above does
           not cover. The NINTEX and Overhead sign draw functions pull their texture-load and
           geometry DLs (D_3000590/D_30005D8/D_3000688/D_30006D0) plus embedded Vtx blocks
           (D_3000608/D_30006F8) from this segment, and gSegments[3] serves this carve raw --
           unswapped s16 vertex coordinates exploded the boards and corrupted Gfx words turned
           the sign texture into a screen-covering smear. Same generated fixup table, same
           pattern as the segment-8 call above. */
        {
            extern void gdx_fixup_asset_segment_image(unsigned char segment, unsigned int rom_base,
                                                      unsigned char* data, unsigned int size);
            gdx_fixup_asset_segment_image(0x03u, PORT_setup_gfx_ROM_START,
                                          (unsigned char*) osPhysicalToVirtual(gSegment17B1E0VramStart),
                                          (unsigned int) (PORT_setup_gfx_ROM_END - PORT_setup_gfx_ROM_START));
        }
        Dma_LoadAssets(SEGMENT_ROM_START(machine_custom_gfx),
                       osPhysicalToVirtual(gSegment17B960VramStart),
                       SEGMENT_ROM_SIZE(machine_custom_gfx));
    }
#endif /* PORT */

#ifdef EXPANSION_KIT
    if ((gLeoDriveConnectionState != 0) && gRamDDCompatible) {
        if (osAppNMIBuffer[13] != 0x20DE1529) {
            osAppNMIBuffer[13] = 0x20DE1529;
            func_xk1_8002FFA0();
        }
        func_xk1_8002FFDC();
    }
#endif

    GDX_CK(G8_assets_done);
    // FrameBuffer Indexes
    D_800DCCFC = 0;
    D_800DCD00 = 1;
    D_800DCD08 = 2;
    D_800DCD04 = 0;
    D_800DCD0C = 1;

    // General Initialisation
    Math_SinTableInit();
    if (gRamDDCompatible) {
        func_800742D0();
    }
    GDX_CK(G8b_pre_742FC);
    func_800742FC(); // calls Course_Load(COURSE_MUTE_CITY) -- see Course_Load's gLeoDriveConnectionState gate
    GDX_CK(G8c_post_742FC);
    Matrix_SetTransRot(&D_80225800.unk_000, 0, 1.0f, 0, 0, 0, 0.0f, 0.0f, 0.0f);

    Math_Rand1Init(osGetTime(), osGetTime() + osGetTime());

    GDX_CK(G9_pre_controller_init);
#ifndef EXPANSION_KIT
    Controller_Init();
#endif
    GDX_CK(GA_post_controller_init);

    GDX_CK(GB_pre_i10_init);
    func_i10_80115DF0();
    GDX_CK(GC_post_i10_init);
    if (gSettingSoundMode == 0) {
        Audio_SetOutMode(SOUNDMODE_SURROUND);
    } else {
        Audio_SetOutMode(SOUNDMODE_MONO);
    }

#ifndef PORT
    while (true) {
        if (OS_CYCLES_TO_NSEC(osGetTime() - startTime) * 6e-8 > 230.0) {
            break;
        }
    }
#else
    /* PORT: this is the console's boot-logo hold — the N64/EK logo blitted by
       func_806F33D0 sits on screen for this entire wait while the guitar riff
       plays over it, and only then does the title (and its BGM, whose sample
       position later swaps player 0 to the SE sequence) begin. Skipping it
       made the logo invisible (~12ms on screen) and started the title BGM so
       early its 250..260 window cut the riff mid-phrase. Same condition as
       console, but with scheduler yields so the audio fiber keeps running. */
    /* startTime is a console-matching s32 truncation; host osGetTime() is
       epoch-based and enormous, so (osGetTime() - startTime) overflows the
       comparison instantly (verified: GC and GD checkpoints logged on the
       same millisecond). Use a full-width baseline captured here instead. */
    /* NOTE: an earlier iteration re-presented the CPU-blitted logo from this
       loop; the direct-to-interpreter draw path renders nothing visible while
       costing enough GPU sync per call to starve the audio fiber (choppy
       sound during the hold). Logo presentation needs a real VI-scanout
       fallback in the frame loop (see the pipeline scoping doc); until then
       this hold keeps the riff/title timing console-faithful. */
    {
        extern void gdx_yield(void);
        OSTime gdxHoldStart = osGetTime();
        while (OS_CYCLES_TO_NSEC(osGetTime() - gdxHoldStart) * 6e-8 <= 230.0) {
            gdx_yield();
        }
    }
#endif
    GDX_CK(GD_post_timer_wait);

    Math_Rand2Init(osGetTime() + osGetTime(), osGetTime());
    osSetTime(0);
    osViSwapBuffer(gFrameBuffers[0]);
    Gfx_InitBuffer();
    func_80067AE0();

    gMoveWd(gMasterDisp++, 6, 0, 0);
    gDPFullSync(gMasterDisp++);
    gSPEndDisplayList(gMasterDisp++);

    GDX_CK(GE_pre_gfx_task);
    Gfx_SetTask(sGfxTask);
    GDX_CK(GF_post_gfx_task);
    Game_Init();
    gGameFrameCount = 0;

    while (true) {
        if (D_800CCFE4 != D_800CCFE8) {
            D_800CCFE4 = D_800CCFE8;
            D_800DCD04 = D_800DCCFC;
            D_800DCD08 = D_800DCD04 - 1;
            if (D_800DCD08 == -1) {
                D_800DCD08 = 2;
            }
            D_800DCD0C = D_800DCD04 + 1;
            if (D_800DCD0C == 3) {
                D_800DCD0C = 0;
            }
        }

        // Game main loops
        switch (D_800CCFE4) {
            case 2:
                func_80067D64();
                break;
            case 3:
                func_80067E98();
                break;
        }
        gGameFrameCount++;
    }
}
