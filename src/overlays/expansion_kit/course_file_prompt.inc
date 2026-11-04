#ifdef PORT
extern s32 GdxGlyphStringWidth(u8* str);

void func_xk1_8002C720(Gfx** gfxP, s32 arg1, s32 arg2, char* arg3, s32 arg4) {
    Gfx* gfx = *gfxP;
    char rawName[GDX_EK_FILE_NAME_MAX + 1];
    u8 name[2][GDX_EK_FILE_NAME_MAX * 2 + 1];
    u8* question = (u8*)D_xk1_800331F0[arg4 + 1];
    s32 nameWidth[2];
    s32 questionWidth;
    s32 nameRows = 1;
    s32 rows = 1;
    s32 width;
    s32 top;
    s32 bottom = arg2 + 16;
    s32 i;

    if (D_xk1_80032BD8 == 0) {
        return;
    }
    for (i = 0; i < GDX_EK_FILE_NAME_MAX && arg3[i] != 0; i++) {
        rawName[i] = arg3[i];
    }
    rawName[i] = 0;
    func_xk1_80026830(rawName, name[0]);
    nameWidth[0] = GdxGlyphStringWidth(name[0]);
    questionWidth = GdxGlyphStringWidth(question);
    width = nameWidth[0] + questionWidth;
    if (width > 272) {
        s32 end = 0;
        s32 measured = 0;

        // MFS filenames become two-byte glyphs; never split a glyph at the wrap.
        while (name[0][end] != 0) {
            s32 next = end + ((name[0][end] & 0x80) ? 2 : 1);
            u8 glyph[3] = { name[0][end], 0, 0 };

            if (next - end == 2) {
                glyph[1] = name[0][end + 1];
            }
            measured += GdxGlyphStringWidth(glyph);
            if (measured > 272) {
                for (i = 0; name[0][end + i] != 0; i++) {
                    name[1][i] = name[0][end + i];
                }
                name[1][i] = 0;
                name[0][end] = 0;
                nameRows = 2;
                break;
            }
            end = next;
        }
        width = questionWidth;
        for (i = 0; i < nameRows; i++) {
            nameWidth[i] = GdxGlyphStringWidth(name[i]);
            if (nameWidth[i] > width) {
                width = nameWidth[i];
            }
        }
        rows = nameRows + 1;
    }
    width += 16;
    arg1 = (320 - width) / 2;
    // Preserve the bottom edge so the native Yes/No controls never move.
    top = bottom - rows * 16;
    gSPDisplayList(gfx++, D_3000510);
    gDPSetPrimColor(gfx++, 0, 0, 128, 128, 255, 255);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, (top - 2) << 2, (arg1 + width + 2) << 2, top << 2,
                        0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, bottom << 2, (arg1 + width + 2) << 2, (bottom + 2) << 2,
                        0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, top << 2, arg1 << 2, bottom << 2,
                        0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (arg1 + width) << 2, top << 2, (arg1 + width + 2) << 2, bottom << 2,
                        0, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 160);
    gSPTextureRectangle(gfx++, arg1 << 2, top << 2, (arg1 + width) << 2, bottom << 2,
                        0, 0, 0, 1 << 10, 1 << 10);
    gSPDisplayList(gfx++, D_3000540);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 64, 64, 255);
    for (i = 0; i < nameRows; i++) {
        s32 x = rows == 1 ? arg1 + 8 : (320 - nameWidth[i]) / 2;

        gfx = func_xk1_800262F4(gfx, x, top + i * 16, name[i]);
    }
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    arg1 = rows == 1 ? arg1 + 8 + nameWidth[0] : (320 - questionWidth) / 2;
    gfx = func_xk1_800264C0(gfx, arg1, bottom - 16, arg4 + 1);
    *gfxP = gfx;
}
#else
void func_xk1_8002C720(Gfx** gfxP, s32 arg1, s32 arg2, char* arg3, s32 arg4) {
    Gfx* gfx;
    s32 temp_a0;
    s32 temp_t4;
    u8 sp1B4[0x100];
    u8 spB4[0x100];

    gfx = *gfxP;
    if (D_xk1_80032BD8 == 0) {
        return;
    }
    func_xk1_80026830(arg3, sp1B4);
    mfsStrCpy(spB4, D_xk1_800331F0[arg4 + 1]);
    temp_a0 = mfsStrLen(sp1B4) + mfsStrLen(spB4);
    temp_t4 = temp_a0 * 8;
    arg1 = (((34 - temp_a0) / 2) * 8) + 24;

    gSPDisplayList(gfx++, D_3000510);
    gDPSetPrimColor(gfx++, 0, 0, 128, 128, 255, 255);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, (arg2 - 2) << 2, (arg1 + temp_t4 + 2) << 2, arg2 << 2, 0, 0, 0, 1 << 10,
                        1 << 10);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, (arg2 + 16) << 2, (arg1 + temp_t4 + 2) << 2, (arg2 + 16 + 2) << 2, 0, 0,
                        0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (arg1 - 2) << 2, arg2 << 2, arg1 << 2, (arg2 + 16) << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(gfx++, (arg1 + temp_t4) << 2, arg2 << 2, (arg1 + temp_t4 + 2) << 2, (arg2 + 16) << 2, 0, 0, 0,
                        1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 160);
    gSPTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + temp_t4) << 2, (arg2 + 16) << 2, 0, 0, 0, 1 << 10,
                        1 << 10);
    gSPDisplayList(gfx++, D_3000540);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 64, 64, 255);
    gfx = func_xk1_800262F4(gfx, arg1, arg2, sp1B4);
    arg1 += mfsStrLen(sp1B4) * 8;
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gfx = func_xk1_800264C0(gfx, arg1, arg2, arg4 + 1);

    *gfxP = gfx;
}
#endif
