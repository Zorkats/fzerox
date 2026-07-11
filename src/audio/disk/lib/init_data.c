#include "macros.h"
#include "audio.h"

TempoData gTempoData = {
    0xA00,
    SEQTICKS_PER_BEAT,
};

#ifdef PORT
/* The init/permanent pool sizes were tuned for N64 struct sizes. Host
   structs are larger (8-byte pointers) and the port's staging buffer and
   font conversions live longer, so a too-small init pool made the permanent
   carve fail — AudioLoad_Init then silently zeroes permanentPoolSize and
   every font load (CACHEPOLICY_0 -> AudioHeap_AllocPermanent) returns NULL
   forever: no fonts, no notes, silence. gAudioHeap is ~11.7 MB on PORT;
   spend a little of it. */
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
