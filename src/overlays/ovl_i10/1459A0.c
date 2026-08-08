#include "global.h"
#include "fzx_save.h"
#include "fzx_course.h"

extern s32 gLeoDriveConnectionState;
extern OSPiHandle* gSramPiHandlePtr;

s32 func_i10_80115DF0(void) {
    gSramPiHandlePtr = Sram_Init();
    func_i10_80115E30((SaveContext*) Arena_Allocate(ALLOC_PEEK, sizeof(SaveContext)));

#ifdef EXPANSION_KIT
    if (gLeoDriveConnectionState != 0) {
        func_xk1_8002FBC8();
    }
    func_i10_8012B580();
#endif
    return 0;
}

#ifdef EXPANSION_KIT
extern s32 sDDStaffGhostRecordTimes[];

void func_i10_8012B580(void) {
    s32 pad;
    s32 courseIndex;
    GhostInfo ghostInfo;
#ifdef PORT
    s32 loaded = 0;
#endif

    for (courseIndex = COURSE_MUTE_CITY; courseIndex <= COURSE_BIG_HAND; courseIndex++) {
#ifdef PORT
        /* Only publish a pacing seed when the record actually loaded; on failure the
           sDDStaffGhostRecordTimes[] -1 initializer must stand, or CPU pacing gets seeded
           from an uninitialized stack GhostInfo. */
        if (Save_LoadStaffGhostRecord(&ghostInfo, courseIndex) == 0) {
            sDDStaffGhostRecordTimes[courseIndex] = ghostInfo.raceTime;
            loaded++;
        }
#else
        Save_LoadStaffGhostRecord(&ghostInfo, courseIndex);
        sDDStaffGhostRecordTimes[courseIndex] = ghostInfo.raceTime;
#endif
    }
#ifdef PORT
    /* Boot log: how many of the 24 standard-course staff records resolved. */
    {
        extern void gdx_cki(const char*, int);
        gdx_cki("[staffghost] loaded standard-course records (of 24)", loaded);
    }
#endif
}
#endif

void func_i10_80115E30(SaveContext* saveContext) {
    s32 i;
    s32 var_s2;
    s32 sp34;
    ProfileSave* var_s1;

#ifdef PORT
    { extern void gdx_ckp(const char*, void*);
      gdx_ckp("[i10] saveContext", (void*)saveContext); }
#endif

    Sram_ReadWrite(OS_READ, 0, saveContext, sizeof(SaveContext));

    for (var_s2 = 0, i = 0, var_s1 = saveContext->profileSaves; i < 2; var_s2++, i++, var_s1++) {
#ifdef PORT
        { extern void gdx_ckp(const char*, void*);
          gdx_ckp("[i10] var_s1", (void*)var_s1);
          gdx_ckp("[i10] fileName", (void*)var_s1->saveSettings.fileName); }
#endif
        if (!func_i10_80115EE8(var_s1->saveSettings.fileName)) {
            break;
        }
        sp34 = i;
    }

    if (var_s2 == 2) {
        Save_Init(saveContext, 0);
        return;
    }
    if (var_s2 == 1) {
        func_i10_80115F2C(saveContext, sp34);
    }
    Save_Load(saveContext);
}

extern const char D_i2_8010ADE0[];

bool func_i10_80115EE8(u8* arg0) {
    s32 ret = false;
    s32 i;

    for (i = 0; i != 8; i++) {
        if (arg0[i] != D_i2_8010ADE0[i]) {
            ret = true;
            break;
        }
    }

    return ret;
}

void func_i10_80115F2C(SaveContext* arg0, s32 arg1) {
    s32 i;
    u8* temp_a2 = arg0->profileSaves[arg1].saveSettings.fileName;

    for (i = 0; i < 8; i++) {
        temp_a2[i] = D_i2_8010ADE0[i];
    }

    Sram_ReadWrite(OS_WRITE, temp_a2 - (u8*) arg0, temp_a2, 8);
}
