#include "global.h"
#include ASSET_HEADER_EK(overlays/expansion_kit/dd_fault.h)

u16 D_xk1_8003BBA0;
u16 D_xk1_8003BBA8[4];
s32* D_8003BBB0;

u16 D_xk1_80033500 = 180;

#ifdef PORT
/* State for the PORT minimum-display hold on the transient disk prompts. The full reasoning
   lives at the hold itself, inside func_xk1_8002ED64 below; it sits at file scope only so that
   func_xk1_8002E9D0 -- which every EK screen calls from its own init -- can reset it, and so the
   unk_0C == 4 banner hold in func_xk1_8002F9DC can share the same frame budget. */
static s32 sPortPromptHoldId = 0;
static s32 sPortPromptHoldFrames = 0;
static s32 sPortBannerHoldFrames = 0;
#endif

void func_xk1_8002E9D0(s32 arg0) {
    D_xk1_80033500 = 180 / arg0;
#ifdef PORT
    /* Screen entry: drop any hold left over from the previous screen and discard a raise that
       was recorded but never drawn. This is the abort/exit guard. func_xk1_8002ED64 only runs
       while gInCourseEditor is set (sys/sys_gfx.c's Gfx_FullSync), and both Machine Create and
       the Course Editor clear that flag on their way out, so a player who leaves during the one
       frame between a dispatch and the next draw would otherwise freeze the hold mid-countdown
       and see it resume as a stale "now saving" over whichever screen came next. Both screens
       call this function from their init (machine_create.c, course_edit/19DD60.c), so re-entry
       always starts from a clean slate. */
    {
        extern volatile s32 gGdxDiskPromptRaised;
        extern volatile s32 gGdxDiskBannerRaised;

        gGdxDiskPromptRaised = 0;
        gGdxDiskBannerRaised = 0;
        sPortPromptHoldId = 0;
        sPortPromptHoldFrames = 0;
        sPortBannerHoldFrames = 0;
    }
#endif
}

Gfx* func_xk1_8002EA10(Gfx* gfx, u16 arg1, u16 arg2, u16 arg3, u16 arg4, u16 arg5) {

    gfx = ExpansionKit_DrawRectangleBorder(gfx, arg1 - 2, arg2 - 2, arg3 + 1, arg4 + 1, arg5, 2, 2);

    gSPDisplayList(gfx++, D_xk1_800335F0);
    gDPFillRectangle(gfx++, arg1, arg2, arg3, arg4);
    gSPDisplayList(gfx++, D_xk1_80033630);
    return gfx;
}

extern volatile unk_807C6EA8 D_807C6EA8;

Gfx* func_xk1_8002EAF0(Gfx* gfx, u16 arg1, u8 arg2, u16 arg3) {

    D_xk1_8003BBA0 = (SCREEN_WIDTH - arg1) >> 1;

    if (D_807C6EA8.unk_04 != 0) {
        switch (arg2) {
            case 1:
                D_xk1_8003BBA8[0] = 110;
                break;
            case 2:
                D_xk1_8003BBA8[0] = 107;
                D_xk1_8003BBA8[1] = 127;
                break;
            case 3:
                D_xk1_8003BBA8[0] = 104;
                D_xk1_8003BBA8[1] = 124;
                D_xk1_8003BBA8[2] = 144;
                break;
            case 4:
                D_xk1_8003BBA8[0] = 101;
                D_xk1_8003BBA8[1] = 121;
                D_xk1_8003BBA8[2] = 141;
                D_xk1_8003BBA8[3] = 161;
                break;
        }
    } else {
        switch (arg2) {
            case 1:
                D_xk1_8003BBA8[0] = 110;
                break;
            case 2:
                D_xk1_8003BBA8[0] = 100;
                D_xk1_8003BBA8[1] = 120;
                break;
            case 3:
                D_xk1_8003BBA8[0] = 90;
                D_xk1_8003BBA8[1] = 110;
                D_xk1_8003BBA8[2] = 130;
                break;
            case 4:
                D_xk1_8003BBA8[0] = 80;
                D_xk1_8003BBA8[1] = 100;
                D_xk1_8003BBA8[2] = 120;
                D_xk1_8003BBA8[3] = 140;
                break;
        }
    }
    gfx = func_xk1_8002EA10(gfx, D_xk1_8003BBA0 - 6, D_xk1_8003BBA8[0] - 3, D_xk1_8003BBA0 + arg1 + 6,
                            D_xk1_8003BBA8[arg2 - 1] + 19, arg3);

    return gfx;
}

/* arg3 forwards straight into func_xk1_800262F4(Gfx*, s32, s32, u8*), which
   dereferences it as a string pointer. It was mistyped s32: on 64-bit hosts
   the D_xk3_801372B8[] pointers passed in (cases 17/18/20/22/23/24 below)
   truncated to 32 bits and crashed on ordinary Save/Overwrite/Entry prompts. */
Gfx* func_xk1_8002ECE8(Gfx* gfx, u16 arg1, u16 arg2, u8* arg3, u16 arg4) {
    u16 sp28 = (0x140 - arg2) >> 1;

    gfx = func_xk1_8002EA10(gfx, sp28, arg1, (sp28 + arg2), arg1 + 16, arg4);
    gfx = func_xk1_800262F4(gfx, sp28, arg1, arg3);
    return gfx;
}

u16 D_xk1_80033504 = 0;
u16 D_xk1_80033508 = 0;

#ifdef PORT
/* The disk's own progress prompts ("Loading...", "Saving...", "No file." and the
 * eleven others in D_xk1_800337D0) drew an EMPTY BOX on the fan-translated disk.
 * The frame was right, the text was simply absent.
 *
 * The binding was never the problem: port/gen/EkTranslatedStrings.c carries all
 * fourteen entries and gdx_ek_strings_apply() repoints the array at them. The
 * renderer is. LeoFault_DrawErrorMessage (sys/disk/leo_fault_dd.c:259-269) walks
 * its argument TWO BYTES AT A TIME -- `(codes[i] << 8) + codes[i+1]`, i += 2 --
 * and looks each pair up in sLeoFontLoadedCharacters, a fixed table of 110
 * Shift-JIS codes for the drive's own Japanese error strings. Handed ASCII it
 * forms 'L'<<8 | 'o' = 0x4C6F, finds nothing, gets NULL from func_8070F634 and
 * draws nothing for every character -- exactly an empty box. That function is
 * shared with the cart-side fault handler, so the ASCII case is diverted here
 * rather than changed there.
 *
 * ASCII goes to func_xk1_800262F4 (A2E90.c), the EK's own walker: one byte per
 * glyph, served from the drive ROM's halfwidth ANK font at the font's own
 * proportional advance. Japanese -- the retail-JP disk's prompts, and every
 * sLeoErrorMessages string on any disk -- has the high bit set on its lead byte
 * and keeps the retail path, so the JP experience is unchanged.
 *
 * Centring: the caller sized its box for two-byte text at a flat 16 px per glyph
 * (the literal widths passed to func_xk1_8002EAF0), roughly twice what the
 * English needs, so left-aligning at D_xk1_8003BBA0 would park the text at the
 * far left of a wide frame. func_xk1_8002EAF0 centres the box on the screen
 * (D_xk1_8003BBA0 = (SCREEN_WIDTH - width) / 2), so re-centring the text on
 * SCREEN_WIDTH / 2 centres it in the box whatever that width was. Only done when
 * posX is that same box origin -- call sites that pass a literal x (the
 * func_xk1_8002EA10 boxes below) mean it. */
extern s32 GdxGlyphStringWidth(u8* str);

static void GdxDrawPromptMessage(Gfx** gfxP, s32 posX, s32 posY, u8* codes) {
    if ((codes != NULL) && (codes[0] != '\0') && ((codes[0] & 0x80) == 0)) {
        s32 x = posX;

        if (posX == D_xk1_8003BBA0) {
            x = (SCREEN_WIDTH / 2) - (GdxGlyphStringWidth(codes) / 2);
        }
        *gfxP = func_xk1_800262F4(*gfxP, x, posY, codes);
        return;
    }
    LeoFault_DrawErrorMessage(gfxP, posX, posY, codes);
}
#define GDX_PROMPT_MSG GdxDrawPromptMessage
#else
#define GDX_PROMPT_MSG LeoFault_DrawErrorMessage
#endif

extern u8* D_xk3_801372B8[];
extern u8* sLeoErrorMessages[];
extern volatile u8 D_80794E1C;
extern volatile u8 D_80794E24;
extern OSMesgQueue D_807C6E90;
extern u8* D_xk1_800337D0[];

Gfx* func_xk1_8002ED64(Gfx* gfx) {
    static u16 D_xk1_8003350C = 0;
    static u8 D_xk1_80033510 = 0;
    char sp34[4];
#ifdef PORT
    s32 portForcedId = 0;
    s32 portRaisedThisFrame = 0;
#endif

    switch (D_807C6EA8.unk_10) {
        case 2:
        case 3:
        case 4:
            osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
            break;
        default:
            break;
    }

    if (D_807C6EA8.unk_08 == 6) {
        D_xk1_8003350C++;
        if ((D_xk1_80033510 == 1) && (D_xk1_8003350C >= (D_xk1_80033500 / 6))) {
            D_xk1_8003350C = 0;
            D_xk1_80033510 = 0;
            D_807C6EA8.unk_08 = 0;
        }
    } else if (D_xk1_80033508 == 6) {
        if (D_807C6EA8.unk_08 == 2) {
            D_xk1_8003350C = 0;
            D_xk1_80033510 = 0;
        } else if (D_xk1_8003350C < (D_xk1_80033500 / 6)) {
            D_xk1_80033510 = 1;
            D_807C6EA8.unk_08 = 6;
        } else {
            D_xk1_8003350C = 0;
            D_xk1_80033510 = 0;
        }
    }
    D_xk1_80033508 = D_807C6EA8.unk_08;
#ifdef PORT
    {
        /* PORT fix: make the transient progress prompts ("now saving" and its siblings) visible.
         *
         * WHAT THE RETAIL LATCH DIRECTLY ABOVE DOES. D_xk1_8003350C counts frames on which
         * prompt id 6 was displayed, D_xk1_80033510 means "the hold has been armed", and
         * D_xk1_80033508 holds unk_08 as of the end of the PREVIOUS call (assigned on the line
         * above, before the error-promotion switch, so it is the pre-promotion value). While
         * the drive genuinely has id 6 raised the first branch just runs the counter. The frame
         * after the drive clears it, unk_08 is no longer 6 but D_xk1_80033508 still is, and the
         * second branch fires: if hard error id 2 has taken the slot the hold is abandoned,
         * otherwise it arms itself and puts id 6 back until D_xk1_80033500 / 6 frames have been
         * shown. So retail already implements a minimum-display floor -- but it is keyed on
         * "the previous frame was 6", so it can only EXTEND a prompt displayed at least once.
         *
         * WHY IT NEVER ARMS ON THIS PORT. sys/disk/75000.c's senders raise the id and then
         * osSendMesg, which here hands straight over to the priority-30 disk worker (spelled
         * out at gGdxDiskPromptRaised in that file). The worker runs the whole synchronous
         * operation and zeroes unk_08 again before returning inside the sender's own
         * osSendMesg call. Set and cleared inside a single frame, unk_08 reads 0 on both this
         * frame and the last, so neither branch above ever observes a 6. Errors are immune
         * precisely because they outlive the operation.
         *
         * THE HOLD. gGdxDiskPromptRaised carries what the worker saw, so the raise survives
         * the operation that erased it. From there this is presentation only: it re-asserts
         * the id for a floor of D_xk1_80033500 / 6 frames and does nothing else -- the save has
         * already happened, nothing is delayed or blocked. That frame count is the retail
         * latch's own threshold: D_xk1_80033500 is "three seconds in this screen's tick units"
         * (180 for Machine Create's 1:1 cadence, 60 for the Course Editor's 1:3, both set
         * through func_xk1_8002E9D0), so / 6 is half a second on either screen.
         *
         * PRECEDENCE, inherited from the retail rules:
         *   - the hold only writes unk_08 while it is 0, so anything the game or the failed
         *     operation put there -- id 0x10 write-refused, id 3 read-only media, id 9 busy, a
         *     menu's own confirm prompt -- wins outright AND cancels the hold, so a failed save
         *     can never be papered over by a lingering "now saving";
         *   - a drive-level error signalled through unk_04 / unk_0C cancels the hold as well,
         *     using the condition the retail promotion switch below uses to demote an
         *     in-progress id to the silent id 10;
         *   - a newer raise replaces an older one outright;
         *   - the injected value is undone before this function returns, so unk_08 read from
         *     anywhere else is exactly what it would be without the hold. func_80767E30's busy
         *     gate and the menus' "unk_08 == 0" idle tests cannot observe it, and the retail
         *     latch above -- which runs first, on purpose -- sees an unmodified stream. */
        extern volatile s32 gGdxDiskPromptRaised;

        if (gGdxDiskPromptRaised != 0) {
            portRaisedThisFrame = gGdxDiskPromptRaised;
            gGdxDiskPromptRaised = 0;
            sPortPromptHoldId = portRaisedThisFrame;
            sPortPromptHoldFrames = D_xk1_80033500 / 6;
        }

        if (sPortPromptHoldFrames != 0) {
            if ((D_807C6EA8.unk_04 != 0) || (D_807C6EA8.unk_0C != 0) ||
                ((D_807C6EA8.unk_08 != 0) && (D_807C6EA8.unk_08 != sPortPromptHoldId))) {
                sPortPromptHoldId = 0;
                sPortPromptHoldFrames = 0;
            } else {
                if (D_807C6EA8.unk_08 == 0) {
                    D_807C6EA8.unk_08 = sPortPromptHoldId;
                    portForcedId = sPortPromptHoldId;
                }
                sPortPromptHoldFrames--;
                if (sPortPromptHoldFrames == 0) {
                    sPortPromptHoldId = 0;
                }
            }
        }
    }
#endif
    switch (D_807C6EA8.unk_08) {
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 25:
        case 26:
            if ((D_807C6EA8.unk_04 != 0) || (D_807C6EA8.unk_0C != 0)) {
                D_807C6EA8.unk_08 = 10;
            }
            break;
    }
    if (D_807C6EA8.unk_04 != 0) {
        sprintf(sp34, "%02d", D_807C6EA8.unk_14);
        gfx = func_xk1_8002EA10(gfx, 102, 67, func_xk1_8002FB30(sp34) + 200, 89, GPACK_RGBA5551(255, 0, 0, 1));
        LeoFault_DrawErrorMessage(&gfx, 108, 70, sLeoErrorMessages[10]);
        LeoFault_DrawErrorMessageNumber(&gfx, 194, 70, sp34);
    }
    switch (D_807C6EA8.unk_08) {
        case 0:
            break;
        case 1:
            gfx = func_xk1_8002EAF0(gfx, 216, 1, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], sLeoErrorMessages[11]);
            break;
        case 2:
            gfx = func_xk1_8002EAF0(gfx, 240, 4, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], sLeoErrorMessages[12]);
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[1], sLeoErrorMessages[13]);
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[2], sLeoErrorMessages[14]);
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[3], sLeoErrorMessages[15]);
            break;
        case 3:
            gfx = func_xk1_8002EAF0(gfx, 272, 3, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], sLeoErrorMessages[18]);
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[1], sLeoErrorMessages[19]);
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[2], sLeoErrorMessages[20]);
            break;
        case 4:
            gfx = func_xk1_8002EAF0(gfx, 248, 1, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], sLeoErrorMessages[21]);
            break;
        case 6:
            gfx = func_xk1_8002EAF0(gfx, 104, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[0]);
            break;
        case 5:
            gfx = func_xk1_8002EAF0(gfx, 104, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[1]);
            break;
        case 7:
            gfx = func_xk1_8002EAF0(gfx, 104, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[9]);
            break;
        case 8:
            gfx = func_xk1_8002EAF0(gfx, 136, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[10]);
            break;
        case 9:
            gfx = func_xk1_8002EAF0(gfx, 216, 1, GPACK_RGBA5551(255, 0, 0, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[2]);
            break;
        case 11:
            gfx = func_xk1_8002EAF0(gfx, 136, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[3]);
            break;
        case 12:
            gfx = func_xk1_8002EAF0(gfx, 168, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[4]);
            break;
        case 13:
            if (D_80794E24 != 0) {
                D_xk1_80033504 = 0;
                D_80794E24 = 0;
            }
            if (D_xk1_80033504 < D_xk1_80033500) {
                if (((D_xk1_80033500 / 4) < D_xk1_80033504) &&
                    ((gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A) ||
                     (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_B))) {
                    D_807C6EA8.unk_08 = 0;
                } else {
                    gfx = func_xk1_8002EAF0(gfx, 168, 1, GPACK_RGBA5551(130, 130, 255, 1));
                    GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[5]);
                    D_xk1_80033504++;
                    break;
                }
            } else {
                D_807C6EA8.unk_08 = 0;
            }
            break;
        case 14:
            gfx = func_xk1_8002EAF0(gfx, 200, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[6]);
            break;
        case 15:
            gfx = func_xk1_8002EAF0(gfx, 216, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[7]);
            break;
        case 16:
        case 27:
            if (D_80794E1C != 0) {
                D_xk1_80033504 = 0;
                D_80794E1C = 0;
            }

            if (D_xk1_80033504 < D_xk1_80033500) {
                if (((D_xk1_80033500 / 4) < D_xk1_80033504) &&
                    ((gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A) ||
                     (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_B))) {
                    D_807C6EA8.unk_08 = 0;
                } else {
                    switch (D_807C6EA8.unk_08) {
                        case 16:
                            gfx = func_xk1_8002EAF0(gfx, 216, 1, GPACK_RGBA5551(255, 0, 0, 1));
                            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[8]);
                            break;
                        case 27:
                            gfx = func_xk1_8002EAF0(gfx, 184, 1, GPACK_RGBA5551(130, 130, 255, 1));
                            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[13]);
                            break;
                    }
                    D_xk1_80033504++;
                }
            } else {
                D_807C6EA8.unk_08 = 0;
            }
            break;
        case 17:
            gfx = func_xk1_8002ECE8(gfx, 54, 192, D_xk3_801372B8[0], GPACK_RGBA5551(130, 130, 255, 1));
            break;
        case 18:
            gfx = func_xk1_8002ECE8(gfx, 54, 256, D_xk3_801372B8[2], GPACK_RGBA5551(130, 130, 255, 1));
            func_xk1_8002D340(&gfx);
            break;
        case 19:
            gfx = func_xk1_8002EA10(gfx, 40, 40, 280, 72, GPACK_RGBA5551(255, 0, 0, 1));
            gfx = func_xk1_800262F4(gfx, 40, 40, D_xk3_801372B8[3]);
            gfx = func_xk1_800262F4(gfx, 40, 56, D_xk3_801372B8[4]);
            break;
        case 20:
            gfx = func_xk1_8002ECE8(gfx, 54, 272, D_xk3_801372B8[5], GPACK_RGBA5551(130, 130, 255, 1));
            func_xk1_8002D340(&gfx);
            break;
        case 21:
            gfx = func_xk1_8002EA10(gfx, 48, 40, 272, 72, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, 48, 40, sLeoErrorMessages[25]);
            LeoFault_DrawErrorMessage(&gfx, 48, 56, sLeoErrorMessages[26]);
            break;
        case 22:
            gfx = func_xk1_8002ECE8(gfx, 54, 112, D_xk3_801372B8[6], GPACK_RGBA5551(255, 0, 0, 1));
            break;
        case 23:
            gfx = func_xk1_8002ECE8(gfx, 54, 128, D_xk3_801372B8[7], GPACK_RGBA5551(255, 0, 0, 1));
            break;
        case 24:
            gfx = func_xk1_8002ECE8(gfx, 54, 272, D_xk3_801372B8[9], GPACK_RGBA5551(130, 130, 255, 1));
            break;
        case 25:
            gfx = func_xk1_8002EAF0(gfx, 136, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[11]);
            break;
        case 26:
            gfx = func_xk1_8002EAF0(gfx, 184, 1, GPACK_RGBA5551(130, 130, 255, 1));
            GDX_PROMPT_MSG(&gfx, D_xk1_8003BBA0, D_xk1_8003BBA8[0], D_xk1_800337D0[12]);
            break;
    }

    switch (D_807C6EA8.unk_0C) {
        case 1:
            gfx = func_xk1_8002EA10(gfx, 38, 187, 282, 209, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, 44, 190, sLeoErrorMessages[16]);
            break;
        case 2:
            gfx = func_xk1_8002EA10(gfx, 38, 187, 282, 209, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, 44, 190, sLeoErrorMessages[24]);
            break;
        case 3:
            gfx = func_xk1_8002EA10(gfx, 50, 167, 270, 209, GPACK_RGBA5551(255, 0, 0, 1));
            LeoFault_DrawErrorMessage(&gfx, 56, 170, sLeoErrorMessages[17]);
            LeoFault_DrawErrorMessage(&gfx, 56, 190, sLeoErrorMessages[23]);
            break;
    }
#ifdef PORT
    {
        /* Undo the injection so unk_08 leaves this function exactly as it arrived. Restored to
           0 rather than compared against what we wrote: the slot was 0 when the hold took it,
           and whatever the body may have turned our value into was derived from our own
           injection and is ours to discard. This restore is what keeps the hold purely
           cosmetic -- no reader of unk_08 outside this draw call can observe it, so
           func_80767E30's busy gate and the menus' "unk_08 == 0" idle tests are unaffected. */
        s32 portDrawnId = D_807C6EA8.unk_08;

        if (portForcedId != 0) {
            D_807C6EA8.unk_08 = 0;
        }

        /* Bounded diagnostic (GDX_TRACE=1; gdx_cki is silent otherwise). "raised" is the id
           the disk worker recorded this frame (0 = none), "framesLeft" is how many hold frames
           remain after this one, and "drawn" is the id the switch above rendered -- a "drawn"
           of 0 or 10 while a hold is active is the signature of a prompt suppressed by the
           error precedence rules rather than one that was never raised. Emitted only on frames
           where the hold did something, and hard-capped. */
        {
            extern void gdx_cki(const char* s, int v);
            static s32 sPortPromptHoldLogs = 0;

            if (((portRaisedThisFrame != 0) || (portForcedId != 0)) && (sPortPromptHoldLogs < 64)) {
                sPortPromptHoldLogs++;
                gdx_cki("[prompt-hold] raised", portRaisedThisFrame);
                gdx_cki("[prompt-hold]   framesLeft", sPortPromptHoldFrames);
                gdx_cki("[prompt-hold]   drawn", portDrawnId);
            }
        }
    }
#endif
    return gfx;
}

Gfx* func_xk1_8002F9DC(Gfx* gfx) {
#ifdef PORT
    s32 portForcedBanner = 0;

    /* The same defect and the same fix, one field over, for the unk_0C == 4 "now formatting"
       banner: func_80767FE4 (sys/disk/75000.c, reached from the Course Editor's format-confirm in
       19DD60.c) raises it and the worker's unk_00 == 1 branch clears it through func_80767940()
       inside the same osSendMesg, so on this port the only feedback for a destructive whole-disk
       format was no feedback at all. Held to the same D_xk1_80033500 / 6 floor as the prompt ids.
       Precedence: the three error banners (unk_0C 1, 2 and 3) latch on their own and outrank this
       one, so a nonzero unk_0C that is not 4 cancels the hold, and the hold only ever writes
       unk_0C while it is 0. Undone before returning, exactly as the prompt hold is, so nothing
       outside this draw call can observe it -- including func_xk1_8002ED64's own hold, which runs
       earlier in Gfx_FullSync and treats any nonzero unk_0C as a reason to stand down. */
    {
        extern volatile s32 gGdxDiskBannerRaised;

        if (gGdxDiskBannerRaised != 0) {
            gGdxDiskBannerRaised = 0;
            sPortBannerHoldFrames = D_xk1_80033500 / 6;
        }

        if (sPortBannerHoldFrames != 0) {
            if ((D_807C6EA8.unk_0C != 0) && (D_807C6EA8.unk_0C != 4)) {
                sPortBannerHoldFrames = 0;
            } else {
                if (D_807C6EA8.unk_0C == 0) {
                    D_807C6EA8.unk_0C = 4;
                    portForcedBanner = 1;
                }
                sPortBannerHoldFrames--;
            }
        }
    }
#endif
    if (D_807C6EA8.unk_0C == 4) {
        gfx = func_xk1_8002EA10(gfx, 62, 187, 258, 209, GPACK_RGBA5551(130, 130, 255, 1));
        LeoFault_DrawErrorMessage(&gfx, 68, 190, sLeoErrorMessages[34]);
    }
#ifdef PORT
    if (portForcedBanner != 0) {
        D_807C6EA8.unk_0C = 0;
    }
#endif
    return gfx;
}

extern u8* gExpansionKitFontPtr;
extern u8 D_xk1_80033808[];

void func_xk1_8002FA50(void) {
    u16 i;

    gExpansionKitFontPtr = Arena_Allocate(ALLOC_FRONT, 20 * 0x80);
    D_8003BBB0 = Arena_Allocate(ALLOC_FRONT, 20 * sizeof(s32));

#ifdef PORT
    /* Arena memory is not zeroed on the port (Arena_Allocate only bumps a
       pointer) and the hardware DMA that would fill these glyphs
       (LeoFault_CopyFontToRam via gDriveRomHandle) never runs. Clear the whole
       block up front so any slot left unfilled below renders as a blank 16x16
       I4 cell instead of uninitialized-arena noise. */
    bzero(gExpansionKitFontPtr, 20 * 0x80);
#endif

    for (i = 0; i < 20; i++) {
        D_8003BBB0[i] = (D_xk1_80033808[i * 2] << 8) + D_xk1_80033808[i * 2 + 1];
#ifndef PORT
        /* PORT: glyphs come from the 64DD drive's internal ROM via
           gDriveRomHandle — no drive ROM on PC (see LeoFault_LoadFontSet). */
        LeoFault_CopyFontToRam(D_8003BBB0[i], gExpansionKitFontPtr + i * 0x80);
#else
        /* The same font block ships inside the user-supplied 64DD IPL ROM image
           (N64DDIPLROM.n64, loaded by port/disk_buffer.cpp). Resolve the
           Shift-JIS code to a font-block offset with LeoGetKAdr and copy its
           16x16 I4 cell straight out of that image — identical to the EK setup
           font path (A2E90.c func_xk1_800260F0). LeoGetKAdr returns -1 for
           unknown codes (fontAddr < DDROM_FONT_START) and a missing image
           leaves gdx_ddipl_buffer NULL; either way the pre-zeroed blank cell
           from the bzero above stands. */
        {
            extern unsigned char* gdx_ddipl_buffer;
            extern unsigned int gdx_ddipl_size;
            s32 fontAddr = LeoGetKAdr(D_8003BBB0[i]) + DDROM_FONT_START;

            if (gdx_ddipl_buffer != NULL && fontAddr >= DDROM_FONT_START &&
                (u32) fontAddr + 0x80 <= gdx_ddipl_size) {
                bcopy(gdx_ddipl_buffer + fontAddr, gExpansionKitFontPtr + i * 0x80, 0x80);
            }
        }
#endif
    };
}
