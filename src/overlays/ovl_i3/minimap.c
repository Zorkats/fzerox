#include "global.h"
#include "minimap.h"
#include "fzx_game.h"
#include "fzx_racer.h"
#include ASSET_HEADER(course_track_gfx.h)

#ifndef EXPANSION_KIT
u8 sCourseMinimapTex[0x1000] = { 0 };
#else
u8* sCourseMinimapTex;
#endif

#ifdef PORT
/* This TLUT is a compile-time C array, not extracted asset data, so it never
   passes through the asset loader's 16-bit endian fixup (gdx_fixup_asset_segment_image).
   The port's CI texture decoder reads palette bytes big-endian (correct for
   ROM-sourced TLUTs, which end up big-endian in host memory), but a native u16
   literal on a little-endian host is stored little-endian -- so the decoder
   swaps every entry. That is harmless for the byte-palindromes CLEAR (0x0000)
   and WHITE (0xFFFF), but it turns BLACK 0x0001 into 0x0100, which zeroes the
   RGBA5551 alpha bit (LSB) and renders the minimap's black track outline fully
   transparent (the long-missing "black border"). Store each entry pre-swapped so
   its in-memory bytes are big-endian, matching what the decoder reads. */
#define MINIMAP_TLUT_ENTRY(r, g, b, a) \
    ((u16)((GPACK_RGBA5551(r, g, b, a) >> 8) | (GPACK_RGBA5551(r, g, b, a) << 8)))
#else
#define MINIMAP_TLUT_ENTRY(r, g, b, a) GPACK_RGBA5551(r, g, b, a)
#endif

u16 sCourseMinimapPalette[] = {
    MINIMAP_TLUT_ENTRY(0, 0, 0, 0),       // MINIMAP_PALETTE_CLEAR
    MINIMAP_TLUT_ENTRY(0, 0, 0, 1),       // MINIMAP_PALETTE_BLACK
    MINIMAP_TLUT_ENTRY(255, 255, 255, 1), // MINIMAP_PALETTE_WHITE
    MINIMAP_TLUT_ENTRY(100, 100, 100, 1), // MINIMAP_PALETTE_GREY
};

s32 sPlayerMinimapPositions[][4][2] = {
    { { 232, 132 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 246, 40 }, { 246, 152 }, { 0, 0 }, { 0, 0 } },
    { { 106, 58 }, { 106, 170 }, { 248, 58 }, { 210, 152 } },
    { { 106, 58 }, { 106, 170 }, { 248, 58 }, { 248, 170 } },
};

s32 gPlayerMinimapLapCounterToggle[] = { 0, 0, 0, 0 };

extern s32 gNumPlayers;
extern CourseInfo* gCurrentCourseInfo;
extern bool gInCourseEditor;

void Minimap_InitCourseMinimap(void) {
    s32 pad[23];
    s32 i;
    f32 forwardMagnitude;
    f32 minimapDimension;
    f32 t;
    s32 column;
    s32 row;
    CourseSegment* startSegment;
    CourseSegment* segment;
    f32 scale;
    Vec3f tangent;
    Vec3f pos;
    CourseInfo* courseInfo;

#ifdef EXPANSION_KIT
    if (!gInCourseEditor) {
        sCourseMinimapTex = Arena_Allocate(ALLOC_FRONT, MINIMAP_MAX_SIZE);
    }
#endif

    courseInfo = gCurrentCourseInfo;

#ifdef PORT
    { extern void gdx_ck(const char*); extern void gdx_ckp(const char*, void*);
      gdx_ck("[mmap] A_courseInfo");
      gdx_ckp("[mmap] courseInfo", (void*)courseInfo); }
#endif

    if (gNumPlayers == 1) {
        scale = 1;
    } else {
        scale = 0.75f;
    }

    for (i = 0; i < MINIMAP_MAX_SIZE; i++) {
        sCourseMinimapTex[i] = MINIMAP_PALETTE_CLEAR;
    }
#ifdef PORT
    { extern void gdx_ck(const char*); gdx_ck("[mmap] B_pre_segments"); }
#endif
    segment = courseInfo->courseSegments;
#ifdef PORT
    { extern void gdx_ck(const char*); extern void gdx_ckp(const char*, void*);
      gdx_ck("[mmap] C_post_segments");
      gdx_ckp("[mmap] segment", (void*)segment); }
#endif
    t = 0.0f;
    minimapDimension = MINIMAP_MAX_DIMENSION * scale;

    startSegment = segment;

#ifdef PORT
    { extern void gdx_cki(const char*, int); extern void gdx_ckp(const char*, void*);
      gdx_cki("[mmap] segmentCount", courseInfo->segmentCount);
      gdx_ckp("[mmap] seg->next", (void*)segment->next); }
#endif

    while (true) {
#ifdef PORT
        { extern void gdx_ck(const char*); gdx_ck("[mmap] D_pre_tangent"); }
#endif
        forwardMagnitude = Course_SplineGetTangent(segment, t, &tangent);
#ifdef PORT
        { extern void gdx_ck(const char*); gdx_ck("[mmap] E_post_tangent"); }
#endif
        Course_SplineGetPosition(segment, t, &pos);
        column = Math_Round(((pos.x * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension) / 2;
        row = Math_Round(((pos.z * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension);
        if ((column >= 0) && (column < MINIMAP_MAX_DIMENSION)) {
            row /= 2;
            if ((row >= 0) && (row < MINIMAP_MAX_DIMENSION)) {
                if (column > 0) {
                    sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column - 1] = MINIMAP_PALETTE_BLACK;
                }
                if (column < MINIMAP_MAX_DIMENSION - 1) {
                    sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column + 1] = MINIMAP_PALETTE_BLACK;
                }
                if (row > 0) {
                    sCourseMinimapTex[(row - 1) * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_BLACK;
                }
                if (row < MINIMAP_MAX_DIMENSION - 1) {
                    sCourseMinimapTex[(row + 1) * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_BLACK;
                }
            }
        }
        t += 200.0f / forwardMagnitude;
        if (t >= 1.0f) {
            segment = segment->next;
            if (startSegment == segment) {
                break;
            }
            t -= 1.0f;
            t *= (forwardMagnitude / Course_SplineGetTangent(segment, 0.0f, &tangent));
        }
    }

    segment = courseInfo->courseSegments;
    t = 0.0f;
    startSegment = segment;

    while (true) {
        forwardMagnitude = Course_SplineGetTangent(segment, t, &tangent);
        Course_SplineGetPosition(segment, t, &pos);
        column = Math_Round(((pos.x * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension) / 2;
        row = Math_Round(((pos.z * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension);
        if ((column > 0) && (column < MINIMAP_MAX_DIMENSION)) {
            row /= 2;
            if ((row > 0) && (row < MINIMAP_MAX_DIMENSION)) {
                if (startSegment == segment->next) {
                    sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_GREY;
                } else {
                    sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_WHITE;
                }
            }
        }
        t += 200.0f / forwardMagnitude;
        if (t >= 1.0f) {
            segment = segment->next;
            if (startSegment == segment) {
                break;
            }
            t -= 1.0f;
            t *= (forwardMagnitude / Course_SplineGetTangent(segment, 0.0f, &tangent));
        }
    }

    segment = courseInfo->courseSegments;
    Course_SplineGetTangent(segment, 0.0f, &tangent);
    Course_SplineGetPosition(segment, 0.0f, &pos);
    column = Math_Round(((pos.x * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension) / 2;
    row = Math_Round(((pos.z * MINIMAP_MAX_DIMENSION * scale) / MINIMAP_WORLD_DIMENSION) + minimapDimension);
    if ((column > 0) && (column < MINIMAP_MAX_DIMENSION - 1)) {
        row /= 2;
        if ((row > 0) && (row < MINIMAP_MAX_DIMENSION - 1)) {
            sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column - 1] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[row * MINIMAP_MAX_DIMENSION + column + 1] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row - 1) * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row + 1) * MINIMAP_MAX_DIMENSION + column + 0] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row - 1) * MINIMAP_MAX_DIMENSION + column - 1] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row + 1) * MINIMAP_MAX_DIMENSION + column - 1] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row - 1) * MINIMAP_MAX_DIMENSION + column + 1] = MINIMAP_PALETTE_BLACK;
            sCourseMinimapTex[(row + 1) * MINIMAP_MAX_DIMENSION + column + 1] = MINIMAP_PALETTE_BLACK;
        }
    }

#ifdef PORT
    /* Fast3D keys CI8 textures by address with no content hash, and the per-race
       arena rewind re-hands this buffer's address to the next course, so the cache
       would serve this race's outline for the next one. Evict the exact address now
       that the buffer has been re-rasterized. Covers every caller of this function
       (including Course Edit's preview).

       Minimap_DrawCourseMinimap (below) uploads sCourseMinimapTex as TWO independent
       CI8 blocks: half0 at the base address and half1 at base + MINIMAP_MAX_SIZE / 2
       (see the "(i * MINIMAP_MAX_DIMENSION * ...) / 2" load offset in that function's
       loop, i = 0..1). The interpreter's texture cache (TextureCacheDelete) only
       evicts exact-address matches with no range awareness, so invalidating the base
       address alone leaves half1's cache entry untouched -- it can keep serving the
       PREVIOUS race's decoded texture, showing as stale content in the bottom half of
       the minimap across races. Evict both halves explicitly. The half1 offset DEPENDS
       ON THE DRAW SCALE: Minimap_DrawCourseMinimap computes it as
       (MINIMAP_MAX_DIMENSION * (s32)(MINIMAP_MAX_DIMENSION * scale)) / 2, which is
       MINIMAP_MAX_SIZE / 2 at the single-player scale (1.0) but a SMALLER offset at
       the multiplayer scale (0.75) -- evicting only the 1.0-scale address left the
       multiplayer half1 entry stale across course changes. Both scales are fixed by
       the numPlayersIndex switch in that function, so evict both derived offsets. */
    {
        extern void gdx_invalidate_texture_address(const void*);
        gdx_invalidate_texture_address(sCourseMinimapTex);
        gdx_invalidate_texture_address(sCourseMinimapTex + MINIMAP_MAX_SIZE / 2);
        gdx_invalidate_texture_address(
            sCourseMinimapTex + (MINIMAP_MAX_DIMENSION * (s32) (MINIMAP_MAX_DIMENSION * 0.75f)) / 2);
    }
#endif
}

extern s16 gSettingVsCom;
extern s8 gTitleDemoState;
extern s32 gGameMode;
extern s32 gTotalRacers;
extern Racer* gRacersByPosition[];
extern GhostRacer* gFastestGhostRacer;
extern u32 gGameFrameCount;

Gfx* Minimap_DrawCourseMinimap(Gfx* gfx, s32 numPlayersIndex, s32 playerIndex) {
    Controller* controller = &gControllers[gPlayerControlPorts[playerIndex]];
    Racer* racer;
    s32 i;
    s32 numPlayers;
    s32 left;
    s32 top;
    f32 minimapScale;
    s32 playerMarkerX;
    s32 playerMarkerY;

    if ((controller->buttonPressed & BTN_CLEFT) && (numPlayersIndex >= 2)) {
        if (gTitleDemoState == TITLE_DEMO_INACTIVE) {
            gPlayerMinimapLapCounterToggle[playerIndex] = (gPlayerMinimapLapCounterToggle[playerIndex] + 1) % 2;
        }
    }
    if (((numPlayersIndex != 2) || (playerIndex != 3)) && (gPlayerMinimapLapCounterToggle[playerIndex] == 0) &&
        (numPlayersIndex >= 2)) {
        return gfx;
    }

    switch (numPlayersIndex) {
        case 0:
            minimapScale = 1.0f;
            break;
        case 1:
            minimapScale = 0.75f;
            break;
        case 2:
        case 3:
            minimapScale = 0.75f;
            break;
    }

    left = sPlayerMinimapPositions[numPlayersIndex][playerIndex][0];
    top = sPlayerMinimapPositions[numPlayersIndex][playerIndex][1];

    gSPDisplayList(gfx++, D_8014940);
    gDPLoadTLUT_pal256(gfx++, sCourseMinimapPalette);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);

    for (i = 0; i < 2; i++) {
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock(
            gfx++, (sCourseMinimapTex + (i * MINIMAP_MAX_DIMENSION * (s32) (MINIMAP_MAX_DIMENSION * minimapScale)) / 2),
            G_IM_FMT_CI, G_IM_SIZ_8b, MINIMAP_MAX_DIMENSION, (s32) (MINIMAP_MAX_DIMENSION * minimapScale) / 2, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, left << 2, (s32) (top + (((i * MINIMAP_MAX_DIMENSION) / 2) * minimapScale)) << 2,
                            (s32) (left + MINIMAP_MAX_DIMENSION * minimapScale) << 2,
                            (s32) (top + ((i * MINIMAP_MAX_DIMENSION) / 2 + (MINIMAP_MAX_DIMENSION / 2)) * minimapScale)
                                << 2,
                            0, 0, 0, 1 << 10, 1 << 10);
    }

    gSPDisplayList(gfx++, D_80149A0);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);

    if ((gGameMode == GAMEMODE_VS_2P) || (gGameMode == GAMEMODE_VS_3P)) {
        numPlayers = gTotalRacers;
    } else {
        numPlayers = numPlayersIndex + 1;
    }

    for (i = 0; i < numPlayers; i++) {
        if (i == playerIndex) {
            continue;
        }
        playerMarkerX =
            Math_Round(((gRacers[i].segmentPositionInfo.segmentPos.x * MINIMAP_MAX_DIMENSION * minimapScale) /
                        MINIMAP_WORLD_DIMENSION) +
                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
            2;
        playerMarkerY =
            Math_Round(((gRacers[i].segmentPositionInfo.segmentPos.z * MINIMAP_MAX_DIMENSION * minimapScale) /
                        MINIMAP_WORLD_DIMENSION) +
                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
            2;
        playerMarkerX += left;
        playerMarkerY += top;
        gDPPipeSync(gfx++);

        // Player Markers
        switch (i) {
            case 0:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER1_COLOR << 16 | MINIMAP_PLAYER1_COLOR);
                break;
            case 1:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER2_COLOR << 16 | MINIMAP_PLAYER2_COLOR);
                break;
            case 2:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER3_COLOR << 16 | MINIMAP_PLAYER3_COLOR);
                break;
            case 3:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER4_COLOR << 16 | MINIMAP_PLAYER4_COLOR);
                break;
        }

        gDPFillRectangle(gfx++, playerMarkerX - 1, playerMarkerY - 1, playerMarkerX + 1, playerMarkerY + 1);
    }
    if (numPlayersIndex == 0) {
        if (gGameMode == GAMEMODE_GP_RACE) {
            // Lead Non-Player Racer Marker
            if (gRacers[0].position == 1) {
                racer = gRacersByPosition[1];
            } else {
                racer = gRacersByPosition[0];
            }
            playerMarkerX =
                Math_Round(((racer->segmentPositionInfo.segmentPos.x * MINIMAP_MAX_DIMENSION * minimapScale) /
                            MINIMAP_WORLD_DIMENSION) +
                           (MINIMAP_MAX_DIMENSION * minimapScale)) /
                2;
            playerMarkerY =
                Math_Round(((racer->segmentPositionInfo.segmentPos.z * MINIMAP_MAX_DIMENSION * minimapScale) /
                            MINIMAP_WORLD_DIMENSION) +
                           (MINIMAP_MAX_DIMENSION * minimapScale)) /
                2;
            playerMarkerX += left;
            playerMarkerY += top;

            gDPPipeSync(gfx++);
            gDPSetFillColor(gfx++, MINIMAP_LEADER_COLOR << 16 | MINIMAP_LEADER_COLOR);
            gDPFillRectangle(gfx++, playerMarkerX - 1, playerMarkerY - 1, playerMarkerX + 1, playerMarkerY + 1);

        } else if (gFastestGhostRacer != NULL) {
            // Ghost Racer Marker
            playerMarkerX = Math_Round(((gFastestGhostRacer->racer->segmentPositionInfo.segmentPos.x *
                                         MINIMAP_MAX_DIMENSION * minimapScale) /
                                        MINIMAP_WORLD_DIMENSION) +
                                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
                            2;
            playerMarkerY = Math_Round(((gFastestGhostRacer->racer->segmentPositionInfo.segmentPos.z *
                                         MINIMAP_MAX_DIMENSION * minimapScale) /
                                        MINIMAP_WORLD_DIMENSION) +
                                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
                            2;
            playerMarkerX += left;
            playerMarkerY += top;

            gDPPipeSync(gfx++);
            gDPSetFillColor(gfx++, MINIMAP_GHOST_COLOR << 16 | MINIMAP_GHOST_COLOR);
            gDPFillRectangle(gfx++, playerMarkerX - 1, playerMarkerY - 1, playerMarkerX + 1, playerMarkerY + 1);
        }
    }
    if ((gGameFrameCount % 16) < 8) {
        if ((numPlayersIndex < playerIndex) && (gSettingVsCom == 0)) {
            return gfx;
        }

        playerMarkerX =
            Math_Round(((gRacers[playerIndex].segmentPositionInfo.segmentPos.x * MINIMAP_MAX_DIMENSION * minimapScale) /
                        MINIMAP_WORLD_DIMENSION) +
                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
            2;
        playerMarkerY =
            Math_Round(((gRacers[playerIndex].segmentPositionInfo.segmentPos.z * MINIMAP_MAX_DIMENSION * minimapScale) /
                        MINIMAP_WORLD_DIMENSION) +
                       (MINIMAP_MAX_DIMENSION * minimapScale)) /
            2;
        playerMarkerX += left;
        playerMarkerY += top;

        gDPPipeSync(gfx++);

        switch (playerIndex) {
            case 0:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER1_COLOR << 16 | MINIMAP_PLAYER1_COLOR);
                break;
            case 1:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER2_COLOR << 16 | MINIMAP_PLAYER2_COLOR);
                break;
            case 2:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER3_COLOR << 16 | MINIMAP_PLAYER3_COLOR);
                break;
            case 3:
                gDPSetFillColor(gfx++, MINIMAP_PLAYER4_COLOR << 16 | MINIMAP_PLAYER4_COLOR);
                break;
        }

        gDPFillRectangle(gfx++, playerMarkerX - 1, playerMarkerY - 1, playerMarkerX + 1, playerMarkerY + 1);
    }

    return gfx;
}
