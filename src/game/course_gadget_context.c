#include "global.h"

#ifndef EXPANSION_KIT
#define MAX_FEATURE_COUNT 105
#define MAX_JUMP_COUNT 4
#else
#define MAX_FEATURE_COUNT 120
#define MAX_JUMP_COUNT 8
#endif

unk_807B3C20 D_802CB6D0;
#ifdef EXPANSION_KIT
unk_807B3C20 D_807B6528;
#endif
unk_807B3C20 D_802CDFD8;
/* Staged from D_802CB6D0.unk_0000 on entry (func_xk2_800EACB0) and copied back on
   exit (func_xk2_800EC3AC), so it must match the 64-entry CourseSegment capacity of
   unk_807B3C20.unk_0000 -- the old 0x1000-byte stub wrote past segment 38. */
#ifdef EXPANSION_KIT
CourseSegment D_802D0620[64];
#endif
CourseDecoration gCourseDecorations[32];
CourseFeature gCourseFeatures[MAX_FEATURE_COUNT];
CourseFeaturesInfo gCourseFeaturesInfo;
CourseEffect gCourseEffects[192];
CourseEffectsInfo gCourseEffectsInfo;
#ifndef EXPANSION_KIT
EffectDrawData gEffectsDrawData[192];
#else
EffectDrawData gEffectsDrawData[2][192];
#endif
Landmine gLandmines[48];
Jump gJumps[MAX_JUMP_COUNT];
Effect gEffects[192];
