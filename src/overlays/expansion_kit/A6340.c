#include "global.h"
#include "leo/mfs.h"
#include "fzx_game.h"
#include ASSET_HEADER(course_track_gfx.h)
#include ASSET_HEADER_EK(expansion_kit_textures.h)

// TODO: Unsure on sizes
u8 gExpansionKitNameEntryStr[16];
u8 D_xk1_8003A570[40];
unk_8003A5D8 D_xk1_8003A598;
s32 sNameEntryCursorXPos;
s32 sNameEntryCursorYPos;
void (*sNameEntryCallbackFunc)(void);

s32 gExpansionKitNameEntryStrLength = 0;
s32 sInputIndicatorFlashRate = 1;
bool D_xk1_80032AC8 = false;
u32 D_xk1_80032ACC = -1;
s32 D_xk1_80032AD0 = 0;

#ifdef PORT
/* Row 4 (space / dash / END) is deliberately absent from this literal on N64.
 * The keyboard is indexed `row * 10 + col` and row 4 is clamped to cols 6-8, so
 * it reads indices 46-48 -- past this 41-byte string, into the adjacent
 * `D_xk1_80032B00 = 0x202D`. Big-endian those bytes are 00 00 20 2D, which puts
 * ' ' at 46, '-' at 47 and the END terminator at 48. On a little-endian host the
 * same object lays out as 2D 20 00 00, shifting the row two cells: END activates
 * one column left of where it draws, and the cell that renders END yields 0x04,
 * so a finished 8-character name can never be saved.
 *
 * Spell row 4 out rather than depend on object layout and byte order. Cells match
 * ExpansionKit_GetCharacterKeyboardPosition below: ' '->(6,4), '-'->(7,4),
 * '\0' (END) at cols 8-9. Cols 0-5 stay NUL, as they are on hardware. */
char sNameEntryKeyboardStr[50] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ,'& 0123456789\0\0\0\0\0\0 -";
#else
char sNameEntryKeyboardStr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ,'& 0123456789";
#endif
UNUSED s32 D_xk1_80032B00 = 0x202D;

char* func_xk1_800290D0(char* buffer, const char* fmt, size_t size) {
    return (char*) memcpy(buffer, fmt, size) + size;
}

Gfx* func_xk1_800290F4(Gfx* gfx, s32 arg1, s32 arg2, u32 arg3) {

    D_xk1_80032ACC = arg3 / 96;
    switch (arg3 / 96) {
        case 0:
            gSPDisplayList(gfx++, aExpansionKitSetupFontCharacterSheet1DL);
            break;
        case 1:
            arg3 -= 0x60;
            gSPDisplayList(gfx++, aExpansionKitSetupFontCharacterSheet2DL);
            break;
        case 2:
            arg3 -= 0xC0;
            gSPDisplayList(gfx++, aExpansionKitSetupFontCharacterSheet3DL);
            break;
    }
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + 8) << 2, (arg2 + 8) << 2, 0, ((arg3 % 16) * 8) << 5,
                        ((arg3 / 16) * 8) << 5, 1 << 10, 1 << 10);

    return gfx;
}

s32 func_xk1_80029218(s32 arg0) {
    if (arg0 < 0x80) {
        return arg0 - 0x20;
    }
    if (D_xk1_80032AD0 == 0) {
        return arg0 - 0x20;
    }
    return arg0 + 0x40;
}

Gfx* func_xk1_8002924C(Gfx* gfx, s32 xPos, s32 yPos, const char* fmt, ...) {
    s32 charRemaining;
    u8* charPtr;
    char buffer[0x100];
    s32 charIndex;
    va_list args;
    va_start(args, fmt);

    D_xk1_80032ACC = -1;
    charRemaining = _Printf(func_xk1_800290D0, buffer, fmt, args);

#ifdef PORT
    /* Probe (GDX_DIAG_NODEINFO): whether the blank Course-Edit info panels reach
       this draw at all. */
    {
        extern int gdx_dev_gate_by_name_nodeinfo(void);
        extern void gdx_dbg_logf(const char* fmt, ...);
        static s32 sLogged = 0;

        if (gdx_dev_gate_by_name_nodeinfo() && sLogged < 24) {
            sLogged++;
            /* %.*s, not %s: _Printf reports a length and does NOT terminate the
               buffer, so a plain %s runs past the emitted bytes and prints
               convincing garbage. */
            gdx_dbg_logf("[ekprintf] at (%d,%d) len=%d text=\"%.*s\" firstCell=%d\n", xPos, yPos, charRemaining,
                         (charRemaining > 0) ? charRemaining : 0, buffer,
                         (charRemaining > 0) ? func_xk1_80029218(buffer[0]) : -1);
        }
    }
#endif

    if (charRemaining > 0) {
        charPtr = (s8*) buffer;
        while (charRemaining > 0) {
            if (*charPtr == 1) {
                D_xk1_80032AD0 = 0;
            } else if (*charPtr == 2) {
                D_xk1_80032AD0 = 1;
            } else {
                gfx = func_xk1_800290F4(gfx, xPos, yPos, func_xk1_80029218(*charPtr));
                xPos += 8;
            }
            charRemaining--;
            charPtr++;
        }
    }
    va_end(args);

    return gfx;
}

void ExpansionKit_SetInputIndicatorFlashRate(s32 rate) {
    sInputIndicatorFlashRate = rate;
}

void ExpansionKit_GetCharacterKeyboardPosition(char letter, s32* xPosPtr, s32* yPosPtr) {
    s32 xPos;
    s32 yPos;
    s32 alphabetIndex;

#ifdef PORT
    /* Any character outside the switch and the two range tests below leaves both
     * outputs unset, and callers feed them straight into gDPLoadTextureTile as
     * tile coordinates. Default to the END cell. */
    xPos = 9;
    yPos = 4;
#endif

    switch (letter) {
        case ' ':
            xPos = 6;
            yPos = 4;
            break;
        case ',':
            xPos = 6;
            yPos = 2;
            break;
        case '.':
            xPos = 7;
            yPos = 2;
            break;
        case '&':
            xPos = 8;
            yPos = 2;
            break;
        case '\'':
            xPos = 7;
            yPos = 2;
            break;
        case '-':
            xPos = 7;
            yPos = 4;
            break;
        case '\0':
            xPos = 9;
            yPos = 4;
            break;
    }

    if ((letter >= '0') && (letter <= '9')) {
        xPos = letter - '0';
        yPos = 3;
    }
    if ((letter >= 'A') && (letter <= 'Z')) {
        alphabetIndex = letter - 'A';
        xPos = alphabetIndex % 10;
        yPos = alphabetIndex / 10;
    }
    *xPosPtr = xPos;
    *yPosPtr = yPos;
}

void func_xk1_800294AC(void) {
    s32 i;

    gExpansionKitNameEntryStrLength = 0;

    for (i = 0; i < 9; i++) {
        gExpansionKitNameEntryStr[i] = 0;
    }
}

extern s32 D_xk1_8003A550;
extern s32 D_xk1_8003A554;

void ExpansionKit_NameEntryInit(void (*callback)(void)) {
    sNameEntryCursorXPos = 0;
    sNameEntryCursorYPos = 0;
    D_xk1_80032AC8 = true;
    sNameEntryCallbackFunc = callback;
    gExpansionKitNameEntryStrLength = 0;
    D_xk1_8003A550 = 0x58;
    D_xk1_8003A554 = 0x58;
    while (true) {
        if (gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] == '\0') {
            break;
        }
        gExpansionKitNameEntryStrLength++;
    }
}

extern unk_8003A5D8 D_xk1_8003A5D8[];
extern s32 D_80119880;

s32 func_xk1_80029560(void) {
    s32 var_s1;

    var_s1 = 0;
    if ((D_80119880 == -1) || ((D_80119880 != 3) && (D_80119880 == 9))) {
        var_s1 = 1;
    }
    while (var_s1 < func_xk1_8002BFA4()) {
        if (mfsStrCmp(D_xk1_8003A5D8[var_s1].name, gExpansionKitNameEntryStr) == 0) {
            return 1;
        }
        var_s1++;
    }

    return 0;
}

void func_xk1_8002961C(void) {
    if ((gExpansionKitNameEntryStrLength != 0) && (sNameEntryCallbackFunc != NULL)) {
        sNameEntryCallbackFunc();
    }
}

void ExpansionKit_NameEntryHandleAPress(void) {
    s32 xPos;
    s32 yPos;
    char* namePtr;
    char letter;

    // TODO: move to appropriate section
    PRINTF("MOJI %d,%d\n");
    PRINTF("NAMEINPUTc\n");
    PRINTF("GET_FILE_NAMES_START\n");

    if (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A) {
        if (gExpansionKitNameEntryStrLength >= 9) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }
        xPos = sNameEntryCursorXPos;
        yPos = sNameEntryCursorYPos;
        letter = sNameEntryKeyboardStr[yPos * 10 + xPos];
        if ((letter != '\0') && (gExpansionKitNameEntryStrLength >= 8)) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }

        if (letter == '\0') {
            if (gExpansionKitNameEntryStrLength == 0) {
                Audio_TriggerSystemSE(NA_SE_32);
            } else {
                Audio_TriggerSystemSE(NA_SE_33);
                gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = letter;
                namePtr = &gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength];
                namePtr--;
                while (*namePtr == ' ') {
                    *namePtr-- = '\0';
                    gExpansionKitNameEntryStrLength--;
                }
                func_xk1_8002961C();
                gExpansionKitNameEntryStrLength++;
            }
        } else if ((gExpansionKitNameEntryStrLength == 0) && (letter == ' ')) {
            Audio_TriggerSystemSE(NA_SE_32);
        } else {
            Audio_TriggerSystemSE(NA_SE_39);
            gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = letter;
            gExpansionKitNameEntryStrLength++;
        }
        // Mpve Cursor To End
        if (gExpansionKitNameEntryStrLength >= 8) {
            sNameEntryCursorXPos = 9;
            sNameEntryCursorYPos = 4;
        }
    }
}

void ExpansionKit_NameEntryHandleStartPress(void) {
    if (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_START) {
        sNameEntryCursorXPos = 9;
        sNameEntryCursorYPos = 4;
    }
}

extern s32 gCourseEditFileOption;

void ExpansionKit_NameEntryHandleBPress(void) {

    if (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_B) {
        Audio_TriggerSystemSE(NA_SE_37);
        if (gExpansionKitNameEntryStrLength != 0) {
            gExpansionKitNameEntryStrLength--;
            gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = 0;
            return;
        }
        if (sNameEntryCallbackFunc != NULL) {
            sNameEntryCallbackFunc();
        }
        gCourseEditFileOption = -1;
    }
}

void func_xk1_80029924(void) {
    if (sNameEntryCursorYPos != 4) {
        Audio_TriggerSystemSE(NA_SE_35);
        return;
    }
    if ((sNameEntryCursorXPos >= 6) && (sNameEntryCursorXPos < 9)) {
        Audio_TriggerSystemSE(NA_SE_35);
    }
}

void ExpansionKit_NameEntryHandleStickInput(void) {
    s32 oldPos;
    Controller* controller;
    s32 stickXMag;
    s32 stickYMag;

    controller = &gControllers[gPlayerControlPorts[0]];

    stickXMag = ABS(controller->stickX);
    stickYMag = ABS(controller->stickY);

    if (stickYMag < stickXMag) {
        oldPos = sNameEntryCursorXPos;
        func_xk1_8002DAE0(&sNameEntryCursorXPos, 9, 1);
        if (oldPos != sNameEntryCursorXPos) {
            func_xk1_80029924();
        }
    } else {
        oldPos = sNameEntryCursorYPos;
        func_xk1_8002DBD4(&sNameEntryCursorYPos, 4, 0);
        if (oldPos != sNameEntryCursorYPos) {
            Audio_TriggerSystemSE(NA_SE_35);
        }
    }
    if (sNameEntryCursorYPos == 4) {
        if (sNameEntryCursorXPos < 6) {
            sNameEntryCursorXPos = 6;
        }
        if (sNameEntryCursorXPos > 8) {
            sNameEntryCursorXPos = 8;
        }
    }
}

#ifdef PORT
#define GDX_NAME_KEY_ESC -1
#define GDX_NAME_KEY_ENTER -2
#define GDX_NAME_KEY_BACKSPACE -3

#define GDX_NAME_QUEUE_SIZE 16
static s32 sNameKeyQueue[GDX_NAME_QUEUE_SIZE];
static s32 sNameKeyQueueHead = 0;
static s32 sNameKeyQueueTail = 0;

extern unk_800D6CA0 D_800D6CA0;
extern s32 gCourseEditCursorXPos;
extern s32 gCourseEditCursorYPos;
extern s32 gGameMode;

s32 gdx_name_entry_is_active(void) {
    if (gGameMode == GAMEMODE_COURSE_EDIT && D_800D6CA0.unk_08 == 2) {
        return 1;
    }
    return 0;
}

void gdx_name_entry_queue_push(s32 key) {
    s32 next = (sNameKeyQueueHead + 1) % GDX_NAME_QUEUE_SIZE;
    if (next != sNameKeyQueueTail) {
        sNameKeyQueue[sNameKeyQueueHead] = key;
        sNameKeyQueueHead = next;
    }
}

void gdx_name_entry_queue_clear(void) {
    sNameKeyQueueHead = 0;
    sNameKeyQueueTail = 0;
}

static void gdx_name_entry_handle_event(s32 event) {
    char letter;

    if (event == GDX_NAME_KEY_ESC) {
        gExpansionKitNameEntryStrLength = 0;
        gExpansionKitNameEntryStr[0] = '\0';
        Audio_TriggerSystemSE(NA_SE_37);
        if (sNameEntryCallbackFunc != NULL) {
            sNameEntryCallbackFunc();
        }
        gCourseEditFileOption = -1;
        return;
    }

    if (event == GDX_NAME_KEY_ENTER) {
        if (gExpansionKitNameEntryStrLength == 0) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }
        while (gExpansionKitNameEntryStrLength > 0 &&
               gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength - 1] == ' ') {
            gExpansionKitNameEntryStrLength--;
            gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = '\0';
        }
        if (gExpansionKitNameEntryStrLength == 0) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }
        sNameEntryCursorXPos = 9;
        sNameEntryCursorYPos = 4;
        Audio_TriggerSystemSE(NA_SE_33);
        func_xk1_8002961C();
        return;
    }

    if (event == GDX_NAME_KEY_BACKSPACE) {
        if (gExpansionKitNameEntryStrLength > 0) {
            gExpansionKitNameEntryStrLength--;
            gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = '\0';
            Audio_TriggerSystemSE(NA_SE_37);
            if (gExpansionKitNameEntryStrLength > 0) {
                letter = gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength - 1];
                ExpansionKit_GetCharacterKeyboardPosition(letter, &sNameEntryCursorXPos, &sNameEntryCursorYPos);
            } else {
                sNameEntryCursorXPos = 0;
                sNameEntryCursorYPos = 0;
            }
        } else {
            Audio_TriggerSystemSE(NA_SE_32);
        }
        return;
    }

    if (event > 0 && event < 128) {
        letter = (char) event;
        if ((gExpansionKitNameEntryStrLength == 0) && (letter == ' ')) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }
        if (gExpansionKitNameEntryStrLength >= 8) {
            Audio_TriggerSystemSE(NA_SE_32);
            return;
        }
        gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = letter;
        gExpansionKitNameEntryStrLength++;
        gExpansionKitNameEntryStr[gExpansionKitNameEntryStrLength] = '\0';
        Audio_TriggerSystemSE(NA_SE_39);
        ExpansionKit_GetCharacterKeyboardPosition(letter, &sNameEntryCursorXPos, &sNameEntryCursorYPos);
        if (gExpansionKitNameEntryStrLength >= 8) {
            sNameEntryCursorXPos = 9;
            sNameEntryCursorYPos = 4;
        }
    }
}

void gdx_name_entry_process_queued_keys(void) {
    while (sNameKeyQueueTail != sNameKeyQueueHead) {
        s32 event = sNameKeyQueue[sNameKeyQueueTail];
        sNameKeyQueueTail = (sNameKeyQueueTail + 1) % GDX_NAME_QUEUE_SIZE;
        gdx_name_entry_handle_event(event);
    }
}
#endif

void ExpansionKit_NameEntryUpdate(s32* arg0, s32* arg1) {
    if (D_xk1_80032AC8) {
        D_xk1_80032AC8 = false;
        return;
    }
#ifdef PORT
    gdx_name_entry_process_queued_keys();
    if (D_800D6CA0.unk_08 != 2) {
        return;
    }
    if (gCourseEditCursorXPos >= 80 && gCourseEditCursorXPos < 240 &&
        gCourseEditCursorYPos >= 80 && gCourseEditCursorYPos < 160) {
        s32 mx = (gCourseEditCursorXPos - 80) / 16;
        s32 my = (gCourseEditCursorYPos - 80) / 16;
        if (my < 4 || mx >= 6) {
            sNameEntryCursorXPos = mx;
            sNameEntryCursorYPos = my;
        }
    }
#endif
    ExpansionKit_NameEntryHandleStartPress();
    ExpansionKit_NameEntryHandleStickInput();
    if (gControllers[gPlayerControlPorts[0]].buttonPressed & BTN_A) {
        ExpansionKit_NameEntryHandleAPress();
    } else {
        ExpansionKit_NameEntryHandleBPress();
    }
}

extern unk_80128C94* D_80128C94;

extern Gfx D_3000510[];
extern unk_80128C94 D_6000000;
#ifdef PORT
/* D_6000000 is the N64 segment-6 token, not independent storage. */
#define D_6000000 (*D_80128C94)
#endif

extern u32 gGameFrameCount;
extern s32 gGameMode;

Gfx* ExpansionKit_NameEntryDraw(Gfx* gfx, s32* arg1, s32* arg2) {
    Gfx* var_v0;
    s32 xPos;
    s32 yPos;
    char letter;
    s32 i;

    xPos = sNameEntryCursorXPos;
    if (sNameEntryCursorYPos < 5) {
        letter = sNameEntryKeyboardStr[sNameEntryCursorYPos * 10 + xPos];
        yPos = sNameEntryCursorYPos;
    } else {
        yPos = sNameEntryCursorYPos;
        letter = '\0';
    }

    gSPDisplayList(gfx++, D_8014940);
    gSPDisplayList(gfx++, D_3000510);
    gDPSetPrimColor(gfx++, 0, 0, 128, 128, 255, 255);

    gSPTextureRectangle(gfx++, (80 - 2) << 2, (80 - 2) << 2, (240 + 2) << 2, 80 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (80 - 2) << 2, 200 << 2, (240 + 2) << 2, (200 + 2) << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (80 - 2) << 2, 80 << 2, 80 << 2, 200 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, 240 << 2, 80 << 2, (240 + 2) << 2, 200 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 200);
    gSPTextureRectangle(gfx++, 80 << 2, 80 << 2, 240 << 2, 200 << 2, 0, 0, 0, 1 << 10, 1 << 10);

    // Highlight current letter
    if (letter != '\0') {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        gSPTextureRectangle(gfx++, ((xPos * 16) + 80) << 2, ((yPos * 16) + 80) << 2, (((xPos + 1) * 16) + 80) << 2,
                            (((yPos + 1) * 16) + 80) << 2, 0, 0, 0, 1 << 10, 1 << 10);
    }

    // Draw whole name entry
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetTextureConvert(gfx++, G_TC_FILT);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, aExpansionKitNameEntryPalette);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    if (gGameMode == GAMEMODE_COURSE_EDIT) {
        var_v0 = D_80128C94->unk_110C8;
        for (i = 0; i < 120; i++) {
            gDPLoadTextureBlock(var_v0++, aExpansionKitNameEntryTex + i * 160, G_IM_FMT_CI, G_IM_SIZ_8b, 160, 1, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);

            gSPTextureRectangle(var_v0++, 80 << 2, (i + 80) << 2, 240 << 2, (i + 81) << 2, 0, 0, 0, 1 << 10, 1 << 10);
        }
        gSPEndDisplayList(var_v0++);
        gSPDisplayList(gfx++, D_6000000.unk_110C8);
    } else {
        for (i = 0; i < 120; i++) {
            gDPLoadTextureBlock(gfx++, aExpansionKitNameEntryTex + i * 160, G_IM_FMT_CI, G_IM_SIZ_8b, 160, 1, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);

            gSPTextureRectangle(gfx++, 80 << 2, (i + 80) << 2, 240 << 2, (i + 81) << 2, 0, 0, 0, 1 << 10, 1 << 10);
        }
    }

    // Draw name at bottom
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    for (i = 0; i < 8; i++) {
        letter = gExpansionKitNameEntryStr[i];
        if (letter == '\0') {
            continue;
        }

        ExpansionKit_GetCharacterKeyboardPosition(letter, &xPos, &yPos);
        gDPPipeSync(gfx++);

        gDPLoadTextureTile(gfx++, aExpansionKitNameEntryTex, G_IM_FMT_CI, G_IM_SIZ_8b, 160, 120, xPos * 16, yPos * 16,
                           (xPos * 16) + 15, (yPos * 16) + 15, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

        gSPTextureRectangle(gfx++, ((i * 16) + 111) << 2, 176 << 2, ((i * 16) + 111 + 15) << 2, (176 + 15) << 2, 0,
                            (xPos * 16) << 5, (yPos * 16) << 5, 1 << 10, 1 << 10);
    }

    // Flash over current underscore input indicator
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);

    if ((gGameFrameCount % (6 / sInputIndicatorFlashRate)) < (3 / sInputIndicatorFlashRate)) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
    }
    if (gExpansionKitNameEntryStrLength < 8) {
        gDPLoadTextureTile(gfx++, aExpansionKitNameEntryTex, G_IM_FMT_CI, G_IM_SIZ_8b, 160, 120,
                           (gExpansionKitNameEntryStrLength * 16) + 31, 96,
                           ((gExpansionKitNameEntryStrLength + 1) * 16) + 31, 116, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

        gSPTextureRectangle(gfx++, ((gExpansionKitNameEntryStrLength * 16) + 111) << 2, 176 << 2,
                            (((gExpansionKitNameEntryStrLength + 1) * 16) + 111) << 2, 196 << 2, 0,
                            ((gExpansionKitNameEntryStrLength * 16) + 31) << 5, 96 << 5, 1 << 10, 1 << 10);
    }

    // Flash over "END"
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);

    if (((sNameEntryCursorXPos == 8) || (sNameEntryCursorXPos == 9)) && (sNameEntryCursorYPos == 4)) {
        if ((gGameFrameCount % (6 / sInputIndicatorFlashRate)) < (3 / sInputIndicatorFlashRate)) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        } else {
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
        }
    } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }

    gDPLoadTextureTile(gfx++, aExpansionKitNameEntryTex, G_IM_FMT_CI, G_IM_SIZ_8b, 160, 1, 128, 64, 160, 84, 0,
                       G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                       G_TX_NOLOD);

    gSPTextureRectangle(gfx++, 208 << 2, 144 << 2, 240 << 2, 164 << 2, 0, 128 << 5, 64 << 5, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    return gfx;
}

extern unk_800D6CA0 D_800D6CA0;

void func_xk1_8002AC24(void) {
    if (gExpansionKitNameEntryStrLength == 0) {
        D_800D6CA0.unk_08 = 0;
    } else {
        D_800D6CA0.unk_08 = 0x34;
        func_8076877C(1, "CRSD");
    }
}

extern s32 D_xk1_80032C20;
extern s32 D_xk2_80104378;

void func_xk1_8002AC70(void) {

    // TODO: move to appropriate section
    PRINTF("SAME FILENAME!!\n");
    PRINTF("NEW FILE SAVE FAILD\n");
    PRINTF("SAME_FILE_NAME!! UWAGAKI\n");

    if (func_xk1_80029560() != 0) {
        switch (D_80119880) {
            case -1:
            case 9:
                D_xk2_80104378 = 4;
                D_xk1_80032C20 = 0;
                D_800D6CA0.unk_08 = 0x10;
                gCourseEditFileOption = -1;
                return;
            case 1:
                break;
            default:
                D_xk2_80104378 = 5;
                D_xk1_80032C20 = 0;
                D_800D6CA0.unk_08 = 0x10;
                gCourseEditFileOption = -1;
                return;
        }
    }

    switch (D_80119880) {
        case -1:
            if (func_xk2_800EAA1C(gExpansionKitNameEntryStr) != 0) {
                gCourseEditFileOption = -1;
            } else {
                gCourseEditFileOption = -1;
            }
            return;
        case 1:
            mfsStrCpy(gExpansionKitNameEntryStr, D_xk1_8003A570);
            if ((func_xk1_80029560() == 0) && ((func_xk1_8002BFA4() - 1) >= 100)) {
                D_80119880 = -2;
                D_xk2_80104378 = 6;
                D_xk1_80032C20 = 0;
                D_800D6CA0.unk_08 = 0x10;
            } else if (func_xk2_800EAA1C(D_xk1_8003A570) != 0) {
                gCourseEditFileOption = -1;
            } else {
                gCourseEditFileOption = -1;
            }
            return;
        case 9:
            if ((func_xk1_8002BFA4() - 1) >= 100) {
                D_xk2_80104378 = 6;
                D_xk1_80032C20 = 0;
                D_800D6CA0.unk_08 = 0x10;
            } else {
                func_xk2_800EAC28(gExpansionKitNameEntryStr);
                gCourseEditFileOption = -1;
            }
            return;
        case 3:
            func_xk2_800EBFE8(D_xk1_8003A598.name);
            func_80768844(MFS_ENTRY_WORKING_DIR, D_xk1_8003A598.name, D_xk1_8003A598.extension,
                          gExpansionKitNameEntryStr, D_xk1_8003A598.extension, true);
            gCourseEditFileOption = -1;
            D_800D6CA0.unk_08 = 0x22;
            return;
    }
    D_800D6CA0.unk_08 = 0;
}

void func_xk1_8002AEB4(s32 arg0, s32 arg1) {
    sNameEntryCursorXPos = arg0;
    sNameEntryCursorYPos = arg1;
}
