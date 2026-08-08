#include "audio.h"

#ifdef PORT
/* The pools are carved by N64-tuned spec sizes, but several cached structs carry
   64-bit pointers on the host and outgrow their N64 footprints. At the retail
   0x2ECA00 the session-pool carve fails, AudioHeap_InitPool builds a near-NULL
   pool, and the second AudioHeap_AllocZeroed from it faults. */
u8 gAudioHeap[0x2ECA00 * 4];
#else
u8 gAudioHeap[0x2ECA00];
#endif
