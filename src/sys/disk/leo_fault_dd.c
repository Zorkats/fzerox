#include "global.h"
#include "leo/leo_internal.h"

extern s32 gLeoDriveConnectionState;
extern OSPiHandle* gDriveRomHandle;

u16* sLeoPrintFrameBuffer;
u16* sLeoPrintCurPixel;
OSMesgQueue sLeoFontMsgQueue;
OSMesg sLeoFontMsgBuf[1];
OSIoMesg sLeoFontIoMsg;
s32 sLeoFontLoadedCharacters[110];

u8 D_8076D010[] = "\243\260";

u8 D_8076D014[] = "\243\261";

u8 D_8076D018[] = "\243\262";

u8 D_8076D01C[] = "\243\263";

u8 D_8076D020[] = "\243\264";

u8 D_8076D024[] = "\243\265";

u8 D_8076D028[] = "\243\266";

u8 D_8076D02C[] = "\243\267";

u8 D_8076D030[] = "\243\270";

u8 D_8076D034[] = "\243\271";

u8 D_8076D038[] = "\245\250\245\351\241\274\310\326\271\346";

u8 D_8076D044[] = "\274\350\260\267\300\342\314\300\275\361\244\362\244\252\306\311\244\337\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D064[] = "\241\332\303\355\260\325\241\333\245\242\245\257\245\273\245\271\245\351\245\363\245\327\305\300\314\307\303\346\244\313";

u8 D_8076D084[] = "\245\307\245\243\245\271\245\257\244\362\310\264\244\253\244\312\244\244\244\307\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D0A4[] = "\276\334\244\267\244\257\244\317\241\242\274\350\260\267\300\342\314\300\275\361\244\362";

u8 D_8076D0BC[] = "\244\252\306\311\244\337\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D0D0[] = "\245\307\245\243\245\271\245\257\244\362\272\271\244\267\271\376\244\363\244\307\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D0F0[] = "\245\307\245\243\245\271\245\257\244\362\272\271\244\267\271\376\244\337\244\312\244\252\244\267\244\306";

u8 D_8076D10C[] = "\264\326\260\343\244\303\244\277\245\307\245\243\245\271\245\257\244\254\272\271\244\267\271\376\244\336\244\354\244\306\244\244\244\353";

u8 D_8076D130[] = "\262\304\307\275\\\300\255\244\254\244\242\244\352\244\336\244\271\241\243";

u8 D_8076D144[] = "\300\265\244\267\244\244\245\307\245\243\245\271\245\257\244\313\270\362\264\271\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D168[] = "\245\307\245\243\245\271\245\257\244\254\272\271\244\267\271\376\244\336\244\354\244\306\244\244\244\336\244\271\244\253\241\243";

u8 D_8076D18C[] = "\265\257\306\260\273\376\244\316\245\307\245\243\245\271\245\257\244\362\272\271\244\267\271\376\244\363\244\307";

u8 D_8076D1AC[] = "\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D1B8[] = "\245\307\245\243\245\271\245\257\244\362\274\350\244\352\275\320\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D1D8[] = "\301\260\262\363\245\307\241\274\245\277\244\254\272\307\270\345\244\336\244\307\244\255\244\301\244\363\244\310";

u8 D_8076D1F8[] = "\245\273\241\274\245\326\244\307\244\255\244\336\244\273\244\363\244\307\244\267\244\277\241\243";

u8 D_8076D214[] = "\301\264\244\306\244\316\245\307\241\274\245\277\244\362\276\303\244\267\244\336\244\271\241\243";

u8 D_8076D230[] = "\243\301\245\334\245\277\245\363\244\362\262\241\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D24C[] = "\245\262\241\274\245\340\303\346\244\316\245\307\245\243\245\271\245\257\244\316\270\362\264\271\244\317";

u8 D_8076D268[] = "\244\307\244\255\244\336\244\273\244\363\241\243";

u8 D_8076D278[] = "\245\263\241\274\245\271\244\316\245\307\241\274\245\277\244\254\244\255\244\301\244\363\244\310";

u8 D_8076D294[] = "\306\311\244\341\244\336\244\273\244\363\244\307\244\267\244\277\241\243";

u8 D_8076D2A8[] = "\245\352\245\273\245\303\245\310\245\334\245\277\245\363\244\362\262\241\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D2CC[] = "\244\267\244\320\244\351\244\257\244\252\302\324\244\301\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D2E8[] = "\245\342\241\274\245\311\245\273\245\354\245\257\245\310\262\350\314\314\244\307\272\271\244\267\271\376\244\363\244\307\244\244\244\277";

u8 D_8076D30C[] = "\245\307\245\243\245\271\245\257\244\313\314\341\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8 D_8076D328[] = "\245\307\245\243\245\271\245\257\244\362\314\341\244\267\244\306\244\257\244\300\244\265\244\244\241\243";

u8* sLeoErrorMessages[] = {
    D_8076D010, D_8076D014, D_8076D018, D_8076D01C, D_8076D020, D_8076D024, D_8076D028, D_8076D02C,
    D_8076D030, D_8076D034, D_8076D038, D_8076D044, D_8076D064, D_8076D084, D_8076D0A4, D_8076D0BC,
    D_8076D0D0, D_8076D0F0, D_8076D10C, D_8076D130, D_8076D144, D_8076D168, D_8076D18C, D_8076D1AC,
    D_8076D1B8, D_8076D1D8, D_8076D1F8, D_8076D214, D_8076D230, D_8076D24C, D_8076D268, D_8076D278,
    D_8076D294, D_8076D2A8, D_8076D2CC, D_8076D2E8, D_8076D30C, D_8076D328,
};

u8 sLeoFontCharacters[] =
    "\243\260\243\261\243\262\243\263\243\264\243\265\243\266\243\267\243\270\243\271\245\250\245\351\241\274\310\326\271\346\274\350\260\267\300\342\314\300\275\361\244\362\244\252\306\311\244\337\244\257\244\300\244\265\244\244\241\243\241\332\303\355\260\325\241\333\245\242\245\257\245\273\245\271\245\363\245\327\305\300\314\307\303\346\244\313\245\307\245\243\310\264\244\253\244\312\244\307\276\334\244\267\244\317\241\242\272\271\271\376\244\363\244\306"
    "\264\326\260\343\244\303\244\277\244\254\244\336\244\354\244\353\262\304\307\275\\\300\255\244\242\244\352\244\271\300\265\270\362\264\271\265\257\306\260\273\376\244\316\275\320\301\260\262\363\245\277\272\307\270\345\244\255\244\301\244\310\245\326\244\273\301\264\276\303\243\301\245\334\262\241\245\262\245\340\245\263\244\341\245\352\245\303\245\310\244\320\244\351\302\324\245\342\245\311\245\354\262\350\314\314\314\341";

u16 sLeoFontPallete[] = {
    GPACK_RGBA5551(0, 0, 0, 1),       GPACK_RGBA5551(16, 16, 16, 1),    GPACK_RGBA5551(32, 32, 32, 1),
    GPACK_RGBA5551(48, 48, 48, 1),    GPACK_RGBA5551(64, 64, 64, 1),    GPACK_RGBA5551(80, 80, 80, 1),
    GPACK_RGBA5551(96, 96, 96, 1),    GPACK_RGBA5551(112, 112, 112, 1), GPACK_RGBA5551(136, 136, 136, 1),
    GPACK_RGBA5551(152, 152, 152, 1), GPACK_RGBA5551(168, 168, 168, 1), GPACK_RGBA5551(184, 184, 184, 1),
    GPACK_RGBA5551(200, 200, 200, 1), GPACK_RGBA5551(216, 216, 216, 1), GPACK_RGBA5551(232, 232, 232, 1),
    GPACK_RGBA5551(255, 255, 255, 1),
};

void LeoFault_CopyFontToRam(s32* code, u8* ramAddr) {
    uintptr_t fontAddr = LeoGetKAdr(code) + DDROM_FONT_START;

    sLeoFontIoMsg.hdr.pri = OS_MESG_PRI_NORMAL;
    sLeoFontIoMsg.hdr.retQueue = &sLeoFontMsgQueue;
    sLeoFontIoMsg.dramAddr = ramAddr;
    sLeoFontIoMsg.devAddr = fontAddr;
    sLeoFontIoMsg.size = 0x80; // leo font size
    gDriveRomHandle->transferInfo.cmdType = LEO_CMD_TYPE_2;
    if (gLeoDriveConnectionState == 2) {
        func_80768B88(gDriveRomHandle, &sLeoFontIoMsg, OS_READ);
    } else {
        osInvalDCache(osPhysicalToVirtual((uintptr_t) ramAddr), 0x80);
        osEPiStartDma(gDriveRomHandle, &sLeoFontIoMsg, OS_READ);
    }
    osRecvMesg(&sLeoFontMsgQueue, NULL, OS_MESG_BLOCK);
}

extern u8 gLeoFontBuffer[];

void LeoFault_LoadFontSet(void) {
#ifdef PORT
    /* The kanji glyphs live in the 64DD drive's internal ROM, normally DMA'd through
       gDriveRomHandle. No such hardware here, but the same font block ships inside the
       user-supplied IPL ROM image, so copy each cell out of that instead.

       sLeoFontLoadedCharacters must be populated unconditionally: func_8070F634 matches every
       error-message code against this table, so an early return on a missing image left every
       lookup failing and the error box empty. Unknown codes get a zeroed cell, which renders
       blank rather than sampling uninitialized memory. */
    u16 i;
    extern unsigned char* gdx_ddipl_buffer;
    extern unsigned int gdx_ddipl_size;

    for (i = 0; i < 110; i++) {
        u8* dst = gLeoFontBuffer + i * 0x80;
        s32 code = (sLeoFontCharacters[i * 2] << 8) + sLeoFontCharacters[i * 2 + 1];
        s32 fontAddr = LeoGetKAdr(code) + DDROM_FONT_START;

        sLeoFontLoadedCharacters[i] = code;
        if (gdx_ddipl_buffer != NULL && fontAddr >= DDROM_FONT_START &&
            (u32) fontAddr + 0x80 <= gdx_ddipl_size) {
            bcopy(gdx_ddipl_buffer + fontAddr, dst, 0x80);
        } else {
            bzero(dst, 0x80);
        }
    }
#else
    u16 i;

    osCreateMesgQueue(&sLeoFontMsgQueue, sLeoFontMsgBuf, ARRAY_COUNT(sLeoFontMsgBuf));
    for (i = 0; i < 110; i++) {
        sLeoFontLoadedCharacters[i] = (sLeoFontCharacters[i * 2] << 8) + sLeoFontCharacters[i * 2 + 1];
        LeoFault_CopyFontToRam(sLeoFontLoadedCharacters[i], gLeoFontBuffer + i * 0x80);
    }
#endif
}

void LeoFault_DrawErrorBox(u16 left, u16 top, u16 right, u16 bottom, u16 color) {
    u16 i;
    u16 j;

    sLeoPrintFrameBuffer = osViGetNextFramebuffer();

    for (i = top; i <= bottom; i++) {
        for (j = left; j <= right; j++) {
            sLeoPrintCurPixel = sLeoPrintFrameBuffer + (i * SCREEN_WIDTH) + j;
            *sLeoPrintCurPixel = color;
        }
    }
}

void LeoFault_DrawErrorBackground(void) {
    DiskDrive_DrawErrorBackground();
}

extern u16 sLeoFontPallete[];
extern FrameBuffer* gFrameBuffers[3];

void func_8070F3D4(Gfx** gfxP, s32 posX, s32 posY, u8* fontCharData) {
    u8 i;
    u16 j;
    u8 k;
    u8 color1index;
    u8 color2index;

    if (gfxP == NULL) {
        u8* fontPtr = fontCharData;

        for (i = posY; i < posY + 16; i++) {
            for (j = posX; j < posX + 16; j += 2, fontPtr++) {
                color1index = *fontPtr >> 4;
                color2index = *fontPtr & 0xF;
                for (k = 0; k < ARRAY_COUNT(gFrameBuffers); k++) {
                    sLeoPrintCurPixel = gFrameBuffers[k]->data + (i * SCREEN_WIDTH) + j;
                    if (color1index != 0) {
                        *sLeoPrintCurPixel = sLeoFontPallete[color1index];
                    }

                    sLeoPrintCurPixel++;
                    if (color2index != 0) {
                        *sLeoPrintCurPixel = sLeoFontPallete[color2index];
                    }
                }
            }
        }
    } else {
        Gfx* gfx = *gfxP;

        gDPLoadTextureBlock_4b(gfx++, fontCharData, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, posX << 2, posY << 2, (posX + 16) << 2, (posY + 16) << 2, 0, 0, 0, 1 << 10, 1 << 10);

        *gfxP = gfx;
    }
}

extern s32* D_8003BBB0;
extern u8* gExpansionKitFontPtr;

u8* func_8070F634(s32 code) {
    u16 i;

    for (i = 0; i < 110; i++) {
        if (code == sLeoFontLoadedCharacters[i]) {
            return gLeoFontBuffer + i * 0x80;
        }
    }
#ifdef PORT
    /* The EK font table is Arena_Allocate'd later (expansion_kit ABC40.c);
       the error renderer can run before that init — guard the NULL deref. */
    if (D_8003BBB0 == NULL) {
        return NULL;
    }
#endif
    for (i = 0; i < 20; i++) {
        if (code == D_8003BBB0[i]) {
            return gExpansionKitFontPtr + i * 0x80;
        }
    }
    return NULL;
}

void LeoFault_DrawErrorMessage(Gfx** gfxP, s32 posX, s32 posY, u8* codes) {
    u8 i;
    u8* fontData;

    for (i = 0; codes[i] != 0; i += 2, posX += 16) {
        fontData = func_8070F634((codes[i] << 8) + codes[i + 1]);
        if (fontData != NULL) {
            func_8070F3D4(gfxP, posX, posY, fontData);
        }
    }
}

void LeoFault_DrawErrorMessageNumber(Gfx** gfxP, s32 posX, s32 posY, s8* str) {
    u8 i;

    if (*str == '1') {
        posX -= 6;
    } else {
        posX -= 4;
    }

    for (i = 0; str[i] != 0; i++) {
        func_8070F3D4(gfxP, posX, posY, &gLeoFontBuffer[(str[i] - '0') * 0x80]);
        if (str[i] == '1') {
            posX += 7;
        } else {
            posX += 9;
        }
    }
}

void LeoFault_DrawErrorNumber(s32 errNo) {
    char errNoStr[4];

    sprintf(errNoStr, "%02d", errNo);
    LeoFault_DrawErrorMessage(NULL, 108, 70, sLeoErrorMessages[10]);
    LeoFault_DrawErrorMessageNumber(NULL, 194, 70, errNoStr);
}

void func_8070F8A4(s32 error, s32 errorType) {
    FrameBuffer* temp_a0;
    u8 i;
    u64* temp_v1;
    u64* temp;
    u64* var_v0;

#ifdef PORT
    gdx_cki("[leo-fault] error", (int) error);
    gdx_cki("[leo-fault] errorType", (int) errorType);
#endif
    func_80767940();
    if (error == LEO_ERROR_WAITING_NMI) {
        return;
    }

    if (errorType == 10) {
        for (i = 0; i < ARRAY_COUNT(gFrameBuffers); i++) {
            var_v0 = &gFrameBuffers[i]->buffer[0x3B60];
            temp_v1 = temp = &gFrameBuffers[i]->buffer[0x4060];
            while (var_v0 < temp_v1) {
                var_v0++;
                *(var_v0 - 1) = 0x1000100010001;
            }
        }
        LeoFault_DrawErrorMessage(NULL, 68, 190, sLeoErrorMessages[34]);
    } else if (errorType == 11) {
        SLForceWritebackDCacheAll();
        Fault_SetFrameBuffer(osViGetNextFramebuffer(), SCREEN_WIDTH, 16);
        Fault_FillRectangle(62, 187, 196, 22);
        LeoFault_DrawErrorBox(60, 185, 259, 186, GPACK_RGBA5551(130, 130, 255, 1));
        LeoFault_DrawErrorBox(60, 209, 259, 210, GPACK_RGBA5551(130, 130, 255, 1));
        LeoFault_DrawErrorBox(60, 187, 0x3D, 208, GPACK_RGBA5551(130, 130, 255, 1));
        LeoFault_DrawErrorBox(258, 187, 259, 208, GPACK_RGBA5551(130, 130, 255, 1));
        LeoFault_DrawErrorMessage(NULL, 68, 190, sLeoErrorMessages[34]);
    } else {
        LeoFault_DrawErrorBackground();
        switch (errorType) {
            case 0:
                LeoFault_DrawErrorMessage(NULL, 52, 110, sLeoErrorMessages[11]);
                LeoFault_DrawErrorNumber(error);
                break;
            case 1:
                LeoFault_DrawErrorMessage(NULL, 40, 101, sLeoErrorMessages[12]);
                LeoFault_DrawErrorMessage(NULL, 40, 121, sLeoErrorMessages[13]);
                LeoFault_DrawErrorMessage(NULL, 40, 141, sLeoErrorMessages[14]);
                LeoFault_DrawErrorMessage(NULL, 40, 161, sLeoErrorMessages[15]);
                LeoFault_DrawErrorMessage(NULL, 44, 190, sLeoErrorMessages[16]);
                LeoFault_DrawErrorNumber(error);
                break;
            case 2:
                LeoFault_DrawErrorMessage(NULL, 56, 107, sLeoErrorMessages[17]);
                LeoFault_DrawErrorMessage(NULL, 56, 127, sLeoErrorMessages[23]);
                LeoFault_DrawErrorNumber(error);
                break;
            case 3:
                LeoFault_DrawErrorMessage(NULL, 24, 90, sLeoErrorMessages[18]);
                LeoFault_DrawErrorMessage(NULL, 24, 110, sLeoErrorMessages[19]);
                LeoFault_DrawErrorMessage(NULL, 24, 130, sLeoErrorMessages[20]);
                break;
            case 4:
                LeoFault_DrawErrorMessage(NULL, 36, 110, sLeoErrorMessages[21]);
                break;
            case 5:
                LeoFault_DrawErrorMessage(NULL, 48, 100, sLeoErrorMessages[22]);
                LeoFault_DrawErrorMessage(NULL, 48, 120, sLeoErrorMessages[23]);
                break;
            case 6:
                LeoFault_DrawErrorMessage(NULL, 48, 90, sLeoErrorMessages[25]);
                LeoFault_DrawErrorMessage(NULL, 48, 110, sLeoErrorMessages[26]);
                LeoFault_DrawErrorMessage(NULL, 48, 130, sLeoErrorMessages[27]);
                LeoFault_DrawErrorMessage(NULL, 60, 190, sLeoErrorMessages[28]);
                break;
            case 7:
                LeoFault_DrawErrorMessage(NULL, 48, 100, sLeoErrorMessages[25]);
                LeoFault_DrawErrorMessage(NULL, 48, 120, sLeoErrorMessages[26]);
                LeoFault_DrawErrorMessage(NULL, 60, 190, sLeoErrorMessages[28]);
                break;
            case 8:
                LeoFault_DrawErrorMessage(NULL, 24, 80, sLeoErrorMessages[29]);
                LeoFault_DrawErrorMessage(NULL, 24, 100, sLeoErrorMessages[30]);
                LeoFault_DrawErrorMessage(NULL, 24, 130, sLeoErrorMessages[35]);
                LeoFault_DrawErrorMessage(NULL, 24, 150, sLeoErrorMessages[36]);
                break;
            case 9:
                LeoFault_DrawErrorMessage(NULL, 64, 100, sLeoErrorMessages[31]);
                LeoFault_DrawErrorMessage(NULL, 64, 120, sLeoErrorMessages[32]);
                LeoFault_DrawErrorMessage(NULL, 36, 190, sLeoErrorMessages[33]);
                break;
            case 12:
                LeoFault_DrawErrorMessage(NULL, 60, 110, sLeoErrorMessages[37]);
                break;
        }
    }
    SLForceWritebackDCacheAll();
}
