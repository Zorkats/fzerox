#include "global.h"
#include "libultra/ultra64.h"
#include "audio.h"
#include "fzx_thread.h"

extern OSMesgQueue gAudioTaskMesgQueue;
extern OSMesgQueue gMainThreadMesgQueue;
extern OSTask* gCurAudioOSTask;

OSMesg sAudioTaskMsg;

void Audio_ThreadEntry(void* arg0) {
    static AudioTask* sCurAudioTask = NULL;
    (void) arg0;
#ifdef PORT
    // R7: previously this branch only drained gAudioTaskMesgQueue without ever calling Audio_Init,
    // because audio data used to be uninitialized (ROM DMA no-op'd). That's no longer true:
    // gRomSegmentPairs[0..2] (audio_seq/audio_bank/audio_table) are populated synchronously by
    // DiskDrive_InitRomSegmentPairs() in decomp/src/sys/sys_main.c's Idle_ThreadEntry, well before
    // osCreateThread(&sAudioThread, ...) runs (same function, sequential — no race), and
    // AudioLoad_Init (audio/disk/lib/load.c) safely handles heap==NULL via the static gAudioHeap.
    // Run the real init + per-frame task loop so the sequence engine actually ticks.
    AudioThread_InitMesgQueues();
#ifndef EXPANSION_KIT
    Audio_Init();
#else
    Audio_Init(gRomSegmentPairs[2][0], gRomSegmentPairs[0][0], gRomSegmentPairs[1][0]);
#endif

    while (true) {
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_NOBLOCK);
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_BLOCK);
        /* Diagnostic for engram slice/audio-synthesis follow-up (hop 3): one-shot log (first 10
           wakeups) proving whether the audio thread's own loop keeps cycling after boot (VI ->
           gAudioTaskMesgQueue -> this blocking recv). If this stops logging early while the game
           keeps running, the wake chain (main thread's EVENT_MESG_VI handling or the fiber
           scheduler) is the break, not anything inside Audio_SetupCreateTask/CreateTaskImpl. */
        {
            extern void gdx_cki(const char* s, int v);
            static s32 sAudioThreadWakeLogCount = 0;
            if (sAudioThreadWakeLogCount < 10) {
                gdx_cki("[audio-diag] Audio_ThreadEntry WOKE iter", sAudioThreadWakeLogCount);
                sAudioThreadWakeLogCount++;
            }
        }
        /* Phase 3 (port/gdx_audio_thread.cpp): kill-switch gate so exactly one producer ever
           touches gAudioCtx's task-creation state (see gdx_audio_thread.cpp's cross-thread
           touchpoint enumeration for why running both at once would be a real race, not just a
           redundant one). Declared locally (no header) -- same extern-without-include pattern
           already used two lines above for gdx_cki, since port/ is outside gdiffuser_game's
           include path (only decomp/ is). GDX_AUDIO_THREAD=0 / --no-audio-thread reverts this
           to unconditionally producing every VI tick, byte-for-byte the pre-Phase-3 behavior. */
        {
            extern int gdx_audio_thread_active(void);
            if (gdx_audio_thread_active()) {
                continue; /* dedicated thread owns production this run -- stay alive, do nothing. */
            }
        }
        if (sCurAudioTask != NULL) {
            gCurAudioOSTask = &sCurAudioTask->task;
            osSendMesg(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_AUDIO_TASK_SET, OS_MESG_BLOCK);
        }
        sCurAudioTask = Audio_SetupCreateTask();
    }
#else
#ifndef EXPANSION_KIT
    Audio_Init();
#else
    Audio_Init(gRomSegmentPairs[2][0], gRomSegmentPairs[0][0], gRomSegmentPairs[1][0]);
#endif

    while (true) {
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_NOBLOCK);
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_BLOCK);
        if (sCurAudioTask != NULL) {
            gCurAudioOSTask = &sCurAudioTask->task;
            osSendMesg(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_AUDIO_TASK_SET, OS_MESG_BLOCK);
        }
        sCurAudioTask = Audio_SetupCreateTask();
    }
#endif
}
