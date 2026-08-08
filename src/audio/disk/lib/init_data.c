#include "macros.h"
#include "audio.h"

TempoData gTempoData = {
    0xA00,
    SEQTICKS_PER_BEAT,
};

#ifdef PORT
/* Retail pool sizes were tuned for N64 struct sizes; host structs are larger and
   the staging buffer lives longer. Too small an init pool makes the permanent carve
   fail, AudioLoad_Init silently zeroes permanentPoolSize, and every font load then
   returns NULL forever -- total silence. */
AudioHeapInitSizes gAudioHeapInitSizes = {
    ALIGN16(sizeof(gAudioHeap) - 0x100),
    0x40000,
    0x20000,
};
#else
AudioHeapInitSizes gAudioHeapInitSizes = {
    ALIGN16(sizeof(gAudioHeap) - 0x100),
    0x10800,
    0x9800,
};
#endif
