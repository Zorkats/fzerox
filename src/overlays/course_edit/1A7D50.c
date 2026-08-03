#include "global.h"
#include "fzx_font.h"
#include "src/overlays/ovl_i3/menus.h"
#include ASSET_HEADER(common_assets_compressed.h)

s32 D_xk2_8013A7E0;
s32 D_xk2_8013A7E4;

void func_xk2_800F6290(void) {
    func_80078104(aMenuTextTLUT, 0x200, 0, 0, 0);
    func_80078104(aMenuQuitTex, 0x200, 0, 1, 0);
    func_80078104(aMenuContinueTex, 0x400, 0, 1, 0);
    Font_LoadString("\243\315", 5);
    Font_LoadString("ABCDEFGHIJKLMNOPQRSTUVWXYZ\243\301\243\30223", 6);
    Font_LoadString("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 3);
}

extern s16 sMenuIsBusy;

void func_xk2_800F632C(void) {
    D_xk2_8013A7E4 = 60;
    D_xk2_8013A7E0 = 0;
    sMenuIsBusy = 0;
}

extern s8 gGamePaused;
/* D_4011D78 (segment 0x04 offset 0x11D78) is the base game's own hud_gfx
   asset aMenuTextTlutSetupDL (include/assets/us/rev0/hud_gfx.h) -- the same
   TLUT-mode setup display list ovl_i3/menus.c uses ahead of its own menu
   text draws. The editor pause menu reuses it here too; referencing the
   named symbol picks up the real asset instead of a zero-filled placeholder
   with no gSPEndDisplayList (which walked off the end of a 0x2000-byte
   zero buffer -- undefined behavior). */
extern Gfx aMenuTextTlutSetupDL[];

#ifdef PORT
/* [pausereg] PORT diagnostic: Course Edit pause-menu corruption.
 *
 * The symptom is a PAIRED swap: while the Test Course runs the track carries
 * scattered coloured specks; on the frame PAUSE is pressed the specks vanish and
 * this menu draws corrupted instead. Both halves are consistent with ONE address
 * claimed by two consumers, but the two candidate mechanisms need different
 * fixes, so they have to be separated by measurement:
 *
 *   (a) STALE REGISTRY POINTER. D_800E33E0 stores raw arena pointers and is only
 *       cleared on a gGameMode change (func_80077D44 via func_80079EC8,
 *       game.c:735). A Course Edit test run re-inits the race IN PLACE
 *       (19DD60.c:338-345) with no mode change, so a registered glyph pointer can
 *       outlive the arena contents it named. func_800783AC then returns a
 *       non-NULL pointer to whatever now lives there and nothing logs -- the
 *       [reg-miss] probe only fires on NULL.
 *   (b) REGISTRY OVERFLOW. The count is unbounded on N64 (guarded in the port,
 *       see GDX_TexRegistryReserve in object.c) and past 200 entries the writes
 *       smash D_800E3A20 and gObjects.
 *
 * One line per second while paused, naming the registry occupancy and the pointer
 * this menu's own TLUT symbol resolves to. Count at or near 200, or a [texreg]
 * OVERFLOW line, means (b). Count sane but the tlut pointer changing between test
 * runs means (a).
 *
 * The arena [start,end) windows would decide (a) outright, but they are not
 * observable here: gArenaStartPtrs/gArenaEndPtrs are defined in sys/segment.c,
 * which port/CMakeLists.txt excludes from the port build, and the only surviving
 * symbol is a zero-filled stub (port/gen/LinkStubs.c). Deciding (a) properly needs
 * a port-side accessor for the live arena bounds -- see gdx_rdram_mode_reset in
 * port/decomp_port.c for where the port tracks them.
 * Gated on GDX_DIAG_TEXREG so a normal run stays silent. */
extern s32 D_800E3A20;
extern u32 gGameFrameCount;
extern int gdx_dev_gate_diag_texreg(void);
extern void gdx_dbg_logf(const char* fmt, ...);

static void GdxPauseMenuRegistryProbe(void) {
    static u32 sLastFrame = 0;

    if (!gdx_dev_gate_diag_texreg()) {
        return;
    }
    if ((gGameFrameCount - sLastFrame) < 60) {
        return;
    }
    sLastFrame = gGameFrameCount;

    gdx_dbg_logf("[pausereg] registryCount=%d (0x%x)\n", (int) D_800E3A20, (unsigned) D_800E3A20);
    gdx_dbg_logf("[pausereg]  tlut resolves to=%p\n", (void*) func_800783AC(aMenuTextTLUT));
}
#endif

Gfx* func_xk2_800F634C(Gfx* gfx) {
    s32 pad[2];

#ifdef PORT
    GdxPauseMenuRegistryProbe();
#endif

    if (D_xk2_8013A7E4 > 0) {
        D_xk2_8013A7E4 -= 8;
    } else {
        D_xk2_8013A7E4 = 0;
    }

    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, D_xk2_8013A7E4 + 100, D_xk2_8013A7E4 + 41, 0xE6 - D_xk2_8013A7E4,
                  0x85 - D_xk2_8013A7E4);

    gfx = Menus_DrawBeveledBox(gfx, 0x78, 0x3D, 0xD2, 0x71, 0, 0, 0, 0xDC);
    gSPDisplayList(gfx++, aMenuTextTlutSetupDL);

    gDPLoadTLUT_pal256(gfx++, func_800783AC(aMenuTextTLUT));

    gfx = Menus_SetOptionColor(gfx, D_xk2_8013A7E0);
    gfx = Menus_DrawRaceMenuTexture(gfx, 0xF, 0x8C, 0x50);
    gfx = Menus_SetOptionColor(gfx, D_xk2_8013A7E0 - 1);
    gfx = Menus_DrawRaceMenuTexture(gfx, 2, 0x8C, 0x5F);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gfx = func_8007DB28(gfx, 0);
    gfx = Font_DrawScaledString(gfx, 0x7D, (D_xk2_8013A7E0 * 0xF) + 0x61, "\243\315", 1, 5, 0, 0.8f, 0.8f);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 128, 128, 128, 255);
    gfx = Font_DrawString(gfx, 0xA6 - (Font_GetStringWidth("PAUSE", 6, 1) / 2), 0x4E, "PAUSE", 1, 6, 0);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 250, 250, 0, 255);

    gfx = Font_DrawString(gfx, 0xA5 - (Font_GetStringWidth("PAUSE", 6, 1) / 2), 0x4D, "PAUSE", 1, 6, 0);
    if ((D_xk2_8013A7E4 == 0) && (sMenuIsBusy == 0)) {
        D_xk2_8013A7E0 = Menus_UpdateHighlightedOptionVertical(0, D_xk2_8013A7E0, 1);
        if (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A) {
            switch (D_xk2_8013A7E0) {
                case 0:
                    func_xk2_800EC8AC();
                    break;
                case 1:
                    func_xk2_800EC91C();
                    break;
            }
        }
        if ((gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_START) &&
            !(gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A)) {
            gGamePaused = false;
            Audio_TriggerSystemSE(NA_SE_12);
            Audio_PauseSet(AUDIO_PAUSE_UNPAUSED);
        }
    }
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 12, 16, 308, 224);

    return gfx;
}
