#include "audio.h"

#ifdef PORT
/* Host builds need headroom: the audio pools are carved by N64-tuned spec
   sizes, but several cached/allocated structs carry 64-bit pointers on the
   host and outgrow their N64 footprints. With the retail 0x2ECA00 heap the
   session-pool carve fails, AudioHeap_InitPool builds a near-NULL pool, and
   the second allocation from it faults (AudioHeap_AllocZeroed). RAM is cheap
   on PC — quadruple it. */
u8 gAudioHeap[0x2ECA00 * 4];
#else
u8 gAudioHeap[0x2ECA00];
#endif
