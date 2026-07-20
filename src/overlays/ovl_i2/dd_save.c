/*****************************
 *    Expansion Kit Only     *
 *****************************/

#include "global.h"
#include "leo/mfs.h"
#include "fzx_course.h"
#include "fzx_save.h"
#include "segment_symbols.h"

char sDDSaveGhostFileName[] = "GHOST00";

extern OSMesgQueue gMFSMesgQ;
extern char gEditCupTrackNames[][9];
extern s32 gLeoDriveConnectionState;

#ifdef PORT
/* Disk-fetched ghost/course save records are big-endian; swap them on the
 * little-endian port immediately after each MFS/64DD fetch and BEFORE any checksum
 * validation or field use. Helpers live in ovl_i2/save.c next to the checksum
 * routines they pair with; declared extern here per the fzx_save.h consumer pattern. */
extern void SaveCourseRecords_FromRom(SaveCourseRecords*);
extern void GhostRecord_FromRom(GhostRecord*);
extern void GhostData_FromRom(GhostData*);

/* Byte-swap the three big-endian disk ghost saves (record + data) in place. */
static void Gdx_SwapGhostSaves_FromRom(GhostSave* ghostSave) {
    s32 i;

    for (i = 0; i < 3; i++) {
        GhostRecord_FromRom(&ghostSave[i].record);
        GhostData_FromRom(&ghostSave[i].data);
    }
}
#endif

void DDSave_LoadCourseGhostRecords(s32 courseIndex, GhostRecord* ghostRecord) {
    s32 i;
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;
    SaveCourseRecords* courseRecords = &COURSE_CONTEXT()->saveCourseRecord;

    PRINTF("Load Ghost Info\n");
    PRINTF("Load Ghost Info2\n");
    PRINTF("idCourse info %d\n");
    PRINTF("machine No %d\n");

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';
    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6) &&
        gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
        for (i = 0; i < 3; i++, ghostRecord++, ghostSave++) {
            *ghostRecord = ghostSave->record;
        }
        return;
    }
    DDSave_ClearCachedGhostSaves();
    /* Blocking MFS ghost-record fetch: reachable from Time Attack race init
     * (Race_Init -> Save_LoadGhost, race.c) and the Records menu
     * (Records_LoadGhostRaceTimes / Records_HasGhostFor, records.c) for ANY
     * course, not just Course Edit. func_807684AC posts a request to
     * sSys6Thread's command queue and this blocks on gMFSMesgQ, which never
     * gets a producer without a real 64DD disk -- same
     * gLeoDriveConnectionState gate as the Course_Load fixes (course_gadgets.c).
     * Skipping the fetch leaves the just-cleared (empty) ghost records in
     * place, matching the existing "no saved ghost" fallback. */
    if (gLeoDriveConnectionState != 0) {
        func_807684AC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT()->ghostSave,
                      offsetof(CourseContext, ghostSave), 3 * sizeof(GhostSave) + sizeof(SaveCourseRecords));
        osRecvMesg(&gMFSMesgQ, NULL, OS_MESG_BLOCK);
#ifdef PORT
        /* This fetch fills ghostSave[3] + saveCourseRecord (contiguous in CourseContext);
           swap both from big-endian before the record checksum check below. */
        Gdx_SwapGhostSaves_FromRom(COURSE_CONTEXT()->ghostSave);
        SaveCourseRecords_FromRom(&COURSE_CONTEXT()->saveCourseRecord);
#endif
    }
    for (i = 0; i < 3; i++, ghostRecord++, ghostSave++) {
        PRINTF("Ghost Name %s\n");
        if (ghostSave->record.checksum != Save_CalculateGhostRecordChecksum(&ghostSave->record)) {
            PRINTF("GHOST_INFO_DATA_BROKEN\n");
            Save_ClearGhostRecord(&ghostSave->record);
            Save_ClearCourseRecord(courseRecords);
            PRINTF("ghost time %d:%d:%d\n");
            COURSE_CONTEXT()->courseData.fileName[0] = '\0';
        }
        *ghostRecord = ghostSave->record;
    }
}

void DDSave_LoadCachedCourseGhostRecords(s32 courseIndex, GhostRecord* ghostRecord) {
    s32 i;
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;
    SaveCourseRecords* courseRecords = &COURSE_CONTEXT()->saveCourseRecord;

    for (i = 0; i < 3; i++, ghostRecord++, ghostSave++) {
        if (ghostSave->record.checksum != Save_CalculateGhostRecordChecksum(&ghostSave->record)) {
            PRINTF("GHOST_INFO_DATA_BROKEN\n");
            Save_ClearGhostRecord(&ghostSave->record);
            Save_ClearCourseRecord(courseRecords);
        }
        *ghostRecord = ghostSave->record;
        PRINTF("ghost time %d:%d:%d\n");
    }
}

bool DDSave_ValidateCachedGhostRecords(void) {
    s32 i;
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;
    SaveCourseRecords* courseRecord = &COURSE_CONTEXT()->saveCourseRecord;

    for (i = 0; i < 3; i++, ghostSave++) {
        if (ghostSave->record.checksum != Save_CalculateGhostRecordChecksum(&ghostSave->record)) {
            PRINTF("GHOST_INFO_DATA_BROKEN\n");
            return true;
        }
    }

    if (courseRecord->checksum != Save_CalculateSaveCourseRecordChecksum(courseRecord)) {
        PRINTF("RECORD_DATA_BROKEN\n");
        return true;
    }
#ifdef PORT
    /* An all-zero SaveCourseRecords self-validates: the additive byte-sum of zeros (0)
       matches the zeroed checksum field, so it sails through the check above. A freshly
       formatted MFS save area feeds exactly that through the disk-course record fetch,
       and a 0 ms best time then "beats" every staff ghost (Staff Ghost row + beaten
       badge on a save that never played Time Attack).
       Broadened defense-in-depth: a valid *played* record always holds a time in the
       open range (0, MAX_TIMER). Values <= 0 (e.g. a byte-swap that never got corrected,
       or a negative garbage s32) and values >= MAX_TIMER (the "no record" sentinel that
       Save_InitCourseRecord seeds, plus any out-of-range garbage) are not trustworthy
       playable times, so treat them as broken and let the caller re-initialize. Clearing
       re-seeds MAX_TIMER, so flagging the sentinel here is harmless (same empty state). */
    if (courseRecord->timeRecord[0] <= 0 || courseRecord->timeRecord[0] >= MAX_TIMER) {
        PRINTF("RECORD_DATA_ZEROED\n");
        return true;
    }
#endif
    return false;
}

void DDSave_LoadCourseGhostData(s32 courseIndex, s32 ghostIndex, GhostData* ghostData) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;

    PRINTF("Load Ghost Data\n");
    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';
    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6) &&
        gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
        *ghostData = ghostSave[ghostIndex].data;
        return;
    }
    DDSave_ClearCachedGhostSaves();
    /* Same gLeoDriveConnectionState gate as DDSave_LoadCourseGhostRecords
     * above -- blocking MFS fetch, no producer without a real disk. In
     * practice this is only reached for a ghost record that
     * DDSave_LoadCourseGhostRecords already found on disk, so it's dead code
     * without a disk; gated directly too since it has its own blocking call. */
    if (gLeoDriveConnectionState != 0) {
        func_807684AC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT()->ghostSave,
                      offsetof(CourseContext, ghostSave), 3 * sizeof(GhostSave));
        osRecvMesg(&gMFSMesgQ, NULL, OS_MESG_BLOCK);
#ifdef PORT
        /* Fetched 3 GhostSaves (record + data) from disk; swap from big-endian
           before ghostSave[ghostIndex].data is consumed below. */
        Gdx_SwapGhostSaves_FromRom(COURSE_CONTEXT()->ghostSave);
#endif
    }
    *ghostData = ghostSave[ghostIndex].data;
}

void DDSave_LoadCachedCourseGhostData(s32 courseIndex, s32 ghostIndex, GhostData* ghostData) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;

    *ghostData = ghostSave[ghostIndex].data;
}

extern CourseData D_i2_800D0130;

void DDSave_SaveGhost(s32 courseIndex, s32 ghostIndex, Ghost* ghost) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave + ghostIndex;

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';
    COURSE_CONTEXT()->courseData = D_i2_800D0130;
    func_80707E58();
    if (ghost != NULL) {
        Save_SaveGhostRecord(ghost);
        Save_SaveGhostData(ghost);
    }
    *ghostSave = gSaveContext.ghostSave;

    func_807680EC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF,
                  true);
}

void DDSave_SaveGhostWithCustomSupport(s32 courseIndex, s32 ghostIndex, Ghost* ghost) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave + ghostIndex;

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';

    COURSE_CONTEXT()->courseData = D_i2_800D0130;
    if (ghost != NULL) {
        Save_SaveGhostRecord(ghost);
        Save_SaveGhostData(ghost);
    }
    *ghostSave = gSaveContext.ghostSave;
    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6)) {
        if (gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
            func_807680EC(MFS_ENTRY_WORKING_DIR, gEditCupTrackNames[courseIndex - COURSE_EDIT_1], "CRSD",
                          COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF, true);
            return;
        }
    }
    func_807680EC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF,
                  true);
}

void DDSave_EraseDiskGhostSave(s32 courseIndex) {
    GhostSave* ghostSave;

    PRINTF("ERASE DISK GHOST %d\n");

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';

    ghostSave = COURSE_CONTEXT()->ghostSave;
    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6)) {
        if (gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
            COURSE_CONTEXT()->courseData = D_i2_800D0130;
            func_807680EC(MFS_ENTRY_WORKING_DIR, gEditCupTrackNames[courseIndex - COURSE_EDIT_1], "CRSD",
                          COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF, true);
            return;
        }
    }
    func_807680EC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF,
                  true);
}

void DDSave_EraseDiskCourseRecord(s32 courseIndex) {
    SaveCourseRecords* courseRecords = &COURSE_CONTEXT()->saveCourseRecord;

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';

    Save_ClearCourseRecord(courseRecords);
    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6)) {
        if (gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
            COURSE_CONTEXT()->courseData = D_i2_800D0130;
            func_807680EC(MFS_ENTRY_WORKING_DIR, gEditCupTrackNames[courseIndex - COURSE_EDIT_1], "CRSD",
                          COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF, true);
            func_i2_800A8CE4(courseRecords, courseIndex);
            return;
        }
    }
    func_807680EC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF,
                  true);
    func_i2_800A8CE4(courseRecords, courseIndex);
}

void DDSave_ClearCachedGhostSaves(void) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;

    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
}

SaveCourseRecords* DDSave_GetCachedCourseRecord(void) {
    return &COURSE_CONTEXT()->saveCourseRecord;
}

GhostSave* DDSave_GetCachedGhostSaves(void) {
    return COURSE_CONTEXT()->ghostSave;
}

void DDSave_SaveCourseGhost(s32 courseIndex) {

    COURSE_CONTEXT()->courseData = D_i2_800D0130;

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';

    if ((courseIndex >= COURSE_EDIT_1) && (courseIndex <= COURSE_EDIT_6)) {
        if (gEditCupTrackNames[courseIndex - COURSE_EDIT_1][0] != '\0') {
            func_807680EC(MFS_ENTRY_WORKING_DIR, gEditCupTrackNames[courseIndex - COURSE_EDIT_1], "CRSD",
                          COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF, true);
            return;
        }
    }
    func_807680EC(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", COURSE_CONTEXT(), sizeof(CourseContext), 0, 0xFF,
                  true);
}

void DDSave_LoadBaseCourses(void) {
    s32 courseIndex;
    s32 courseIndexToEncode;

    for (courseIndex = 0; courseIndex < COURSE_EDIT_1; courseIndex++) {
        if (courseIndex >= COURSE_EDIT_1) {
            courseIndexToEncode = COURSE_EDIT_1;
        } else {
            courseIndexToEncode = courseIndex;
        }
        Course_Load(courseIndex);

        PRINTF("=========================================\n");
        PRINTF("courseID 0x%X -> right data is 0x7B4113D8\n");
        PRINTF("=========================================\n");
        gCourseInfos[courseIndex].encodedCourseIndex = (Course_CalculateChecksum() << 5) | courseIndexToEncode;
    }
}

void DDSave_LoadCachedGhostRecord(s32 ghostIndex, GhostRecord* ghostRecord) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;

    if (1) {}
    *ghostRecord = ghostSave[ghostIndex].record;
}

void DDSave_LoadCachedGhostData(s32 ghostIndex, GhostData* ghostData) {
    GhostSave* ghostSave = COURSE_CONTEXT()->ghostSave;

    if (1) {}
    *ghostData = ghostSave[ghostIndex].data;
}

void DDSave_LoadDDCourseGhosts(s32 courseIndex) {

    courseIndex -= COURSE_SILENCE_3;
    if (courseIndex < 0) {
        courseIndex = 0;
    }
    DiskDrive_LoadData(SEGMENT_DISK_START(silence_3_staff_ghost) + courseIndex, COURSE_CONTEXT()->ghostSave,
                       3 * sizeof(GhostSave), 0);
#ifdef PORT
    /* Staff-ghost records are read straight from the big-endian disk image; swap them
       so their timeRecord/raceTime and checksum are interpreted correctly on the port.
       Without this, garbage byte-swapped staff-ghost times "beat" every record. */
    Gdx_SwapGhostSaves_FromRom(COURSE_CONTEXT()->ghostSave);
#endif
}

void DDSave_EraseCourseGhostFile(s32 courseIndex) {
    GhostSave* ghostSave;

    sDDSaveGhostFileName[5] = (courseIndex / 10) + '0';
    sDDSaveGhostFileName[6] = (courseIndex % 10) + '0';

    PRINTF("STUFF GHOST IS READING BY REAL DISK\n");

    ghostSave = COURSE_CONTEXT()->ghostSave;
    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
    ghostSave++;
    Save_ClearGhostRecord(&ghostSave->record);
    func_80704050(true);
    PRINTF("ERASE DISK GHOST %d\n");
    SLMFSDeleteFile(MFS_ENTRY_WORKING_DIR, sDDSaveGhostFileName, "GOST", false);
    func_80704050(false);
}
