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
