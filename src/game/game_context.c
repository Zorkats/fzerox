#include "global.h"
#include "fzx_racer.h"

CourseInfo gCourseInfos[56]; // 0x3480
#ifndef EXPANSION_KIT
SegmentChunk gSegmentChunks[1025]; // 0x18060
#else
SegmentChunk gSegmentChunks[769]; // 0x12060
#endif
CourseSegment D_802C2020[64];     // 0x2900
Racer gRacers[TOTAL_RACER_COUNT]; // 0x6DB0

#ifdef PORT
/* G-Diffuser in-session save-state: capture of gRacers, the core racer logical/physics array,
   which lives in native BSS (NOT the emulated gdx_rdram buffer). Additive; port build only.
   Each Racer's embedded pointers (racerAhead/racerBehind/unk_28C -> gRacers; segmentPositionInfo
   .courseSegment -> course-segment data) reference stable BSS or same-course RDRAM, so raw-copying
   is safe under the documented same-course/same-race constraint. See port/gdx_savestate.c. */
static void gdx_ss_gamectx_bcopy(unsigned char* d, const unsigned char* s, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

unsigned int Gdx_SaveState_GameContext_Size(void) {
    return (unsigned int)sizeof(gRacers);
}

void Gdx_SaveState_GameContext_Capture(void* dst) {
    gdx_ss_gamectx_bcopy((unsigned char*)dst, (const unsigned char*)gRacers,
                         (unsigned int)sizeof(gRacers));
}

void Gdx_SaveState_GameContext_Restore(const void* src) {
    gdx_ss_gamectx_bcopy((unsigned char*)gRacers, (const unsigned char*)src,
                         (unsigned int)sizeof(gRacers));
}
#endif /* PORT */
