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
    // This branch once only drained gAudioTaskMesgQueue and never called Audio_Init, because the
    // audio data was uninitialized (ROM DMA no-op'd). gRomSegmentPairs[0..2] (audio_seq/audio_bank/
    // audio_table) are now populated synchronously by DiskDrive_InitRomSegmentPairs() in sys_main.c's
    // Idle_ThreadEntry, before the same function's osCreateThread(&sAudioThread, ...) — sequential,
    // no race — and AudioLoad_Init handles heap==NULL via the static gAudioHeap. So run the real
    // init and per-frame task loop; that is what ticks the sequence engine.
    AudioThread_InitMesgQueues();
#ifndef EXPANSION_KIT
    Audio_Init();
#else
    Audio_Init(gRomSegmentPairs[2][0], gRomSegmentPairs[0][0], gRomSegmentPairs[1][0]);
#endif

    while (true) {
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_NOBLOCK);
        osRecvMesg(&gAudioTaskMesgQueue, &sAudioTaskMsg, OS_MESG_BLOCK);
        /* One-shot diagnostic (first 10 wakeups): does the audio thread's own loop keep cycling
           after boot (VI -> gAudioTaskMesgQueue -> this blocking recv)? If it stops logging early
           while the game keeps running, the break is in the wake chain (the main thread's
           EVENT_MESG_VI handling, or the fiber scheduler) — not inside
           Audio_SetupCreateTask/CreateTaskImpl. */
        {
            extern void gdx_cki(const char* s, int v);
            static s32 sAudioThreadWakeLogCount = 0;
            if (sAudioThreadWakeLogCount < 10) {
                gdx_cki("[audio-diag] Audio_ThreadEntry WOKE iter", sAudioThreadWakeLogCount);
                sAudioThreadWakeLogCount++;
            }
        }
        /* Gate so exactly one producer ever touches gAudioCtx's task-creation state: running the
           dedicated audio thread alongside this loop is a real race, not a redundant one (see
           port/gdx_audio_thread.cpp for the cross-thread touchpoints). Declared locally, no header
           -- the same extern-without-include pattern used two lines above for gdx_cki, since port/
           is outside gdiffuser_game's include path (only decomp/ is). GDX_AUDIO_THREAD=0 /
           --no-audio-thread reverts to producing on every VI tick, byte-for-byte the behavior from
           before the dedicated thread existed. */
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
