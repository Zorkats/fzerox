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
/* Segment 0x04 offset 0x11D78 is the hud_gfx asset aMenuTextTlutSetupDL
   (include/assets/us/rev0/hud_gfx.h), the same TLUT setup list ovl_i3/menus.c uses.
   Referencing it by name matters: the raw D_4011D78 extern resolved to a zero-filled
   placeholder with no gSPEndDisplayList, so the RSP walked off its 0x2000 bytes. */
extern Gfx aMenuTextTlutSetupDL[];

#ifdef PORT
/* Pause-menu corruption: texture-registry overflow (count at or near 200) versus a stale
   registered pointer surviving the test run's in-place race re-init (count sane, but the
   tlut pointer moves between runs). Gated on GDX_DIAG_TEXREG. */
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
