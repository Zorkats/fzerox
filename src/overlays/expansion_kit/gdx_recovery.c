#ifdef PORT
#include "global.h"
#include "fzx_course.h"
#include "leo/mfs.h"
#include "../../../../port/gdx_edit_cup.h"

extern char gEditCupTrackNames[GDX_EDIT_CUP_ROWS][GDX_EDIT_CUP_NAME_SIZE];
extern OSThread* __osRunningThread;
extern float gdx_course_edit_max_y(void);
static u8 sRecoveryBlock[0xC830 + BLK_SIZE_ZONE0];
static MfsRamArea sRecoveryRamBefore;
static s32 sRecoveryHealthy = true;

static int recovery_track_valid(const u8* payload) {
    CourseData course;
    u32 checksum;
    s32 i;
    memcpy(&course, payload, sizeof(course));
    if (course.controlPointCount < 0 || course.controlPointCount > 64 || course.creatorId != CREATOR_NINTENDO ||
        course.bgm < 0 || course.bgm > BGM_NEW_04) {
        return 0;
    }
    checksum = course.controlPointCount;
    for (i = 0; i < course.controlPointCount; ++i) {
        const ControlPoint* p = &course.controlPoint[i];
        u32 bits[3];
        memcpy(bits, &p->pos, sizeof(bits));
        if ((bits[0] & 0x7F800000) == 0x7F800000 || (bits[1] & 0x7F800000) == 0x7F800000 ||
            (bits[2] & 0x7F800000) == 0x7F800000 || p->pos.x < -15000.0f || p->pos.x > 15000.0f ||
            p->pos.y < -250.0f || p->pos.y > gdx_course_edit_max_y() ||
            p->pos.z < -15000.0f || p->pos.z > 15000.0f) {
            return 0;
        }
        checksum += (u32) (s32) ((p->pos.x + ((1.1f + 0.7f * i) * p->pos.y)) +
            ((2.2f + 1.2f * i) * p->pos.z * (4.4f + 0.9f * i)) + p->radiusLeft +
            ((5.5f + 0.8f * i) * p->radiusRight * 4.8f));
        checksum += ((u32) p->trackSegmentInfo & ~(0x600u | 0x38000u | 0x40000000u)) * (0xFEu - i);
        checksum += (u32) (course.bankAngle[i] * (0x93DE - i * 2));
        checksum += (u32) (course.pit[i] * i) + (u32) (course.dash[i] * (i + 0x10)) +
            (u32) (course.dirt[i] * (i + 0x80)) + (u32) (course.ice[i] * (i + 0x100)) +
            (u32) (course.jump[i] * (i + 0x800)) + (u32) (course.landmine[i] * (i + 0x1000)) +
            (u32) (course.gate[i] * (i + 0x8000)) + (u32) (course.building[i] * (i + 0x10000)) +
            (u32) (course.sign[i] * (i + 0x80000));
    }
    return checksum == course.checksum;
}

void* gdx_recovery_native_thread(void) {
    return __osRunningThread;
}

int gdx_recovery_native_prepare(void) {
    if (!sRecoveryHealthy || ((gDirectoryEntryCount <= 0) && (func_i1_80404204() < 0))) {
        return 0;
    }
    if (Mfs_GetFilesPreparation(MFS_ENTRY_WORKING_DIR) == 0) {
        return 1;
    }
    if (gMfsError != LEO_ERROR_MEDIUM_MAY_HAVE_CHANGED || func_i1_8040428C() < 0) {
        return 0;
    }
    // Recovery cannot repair or format a missing directory before validating its identity.
    return Mfs_GetFilesPreparation(MFS_ENTRY_WORKING_DIR) == 0;
}

static int recovery_rollback(void) {
    memcpy(&gMfsRamArea, &sRecoveryRamBefore, sizeof(gMfsRamArea));
    // Failed native writes must not leak a partial allocation table into a later GDD flush.
    if (Mfs_BackupRamArea() < 0) {
        sRecoveryHealthy = false;
    }
    return 0;
}

static s32 recovery_read_raw(const char* name, const char* ext, u8* out, size_t expected) {
    u16 index = Mfs_GetFileIndex(gWorkingDirectory, (char*) name, (char*) ext);
    if (index == MFS_ENTRY_DOES_NOT_EXIST) {
        return 0;
    }
    if (gMfsRamArea.directoryEntry[index].fileSize != expected ||
        Mfs_LoadFileInDir(gWorkingDirectory, (char*) name, (char*) ext, out, (s32) expected) != 0) {
        return -1;
    }
    return 1;
}

unsigned gdx_recovery_native_directory(void) {
    return gWorkingDirectory;
}

int gdx_recovery_native_restore_ready(const char* name, const char* ext, const u8* bytes, size_t size,
                                     int cupMask) {
    s32 required = 0;
    s32 entries = 0;
    s32 freeEntries = 0;
    s32 i;
    if (!gdx_recovery_native_prepare() || size != 16 + 0xC830 || !recovery_track_valid(bytes + 16) ||
        (cupMask & ~3)) {
        return 0;
    }
    if (Mfs_GetFileIndex(gWorkingDirectory, (char*) name, (char*) ext) == MFS_ENTRY_DOES_NOT_EXIST) {
        required += ((0xC830 + BLK_SIZE_ZONE0 - 1) / BLK_SIZE_ZONE0) * BLK_SIZE_ZONE0;
        entries++;
    }
    // Reserve worst-case block rounding and the temporary entry used by native cup replacement.
    for (i = 0; i < 2; ++i) {
        if (cupMask & (1 << i)) {
            required += BLK_SIZE_ZONE0;
            entries += Mfs_GetFileIndex(gWorkingDirectory, "CRS_ENTRY", i ? "CENL" : "CENT") ==
                       MFS_ENTRY_DOES_NOT_EXIST;
        }
    }
    if (cupMask) {
        entries++;
    }
    for (i = 0; i < gDirectoryEntryCount; ++i) {
        freeEntries += gMfsRamArea.directoryEntry[i].attr == 0;
    }
    return freeEntries >= entries && Mfs_RamGetFreeSize() >= required;
}

int gdx_recovery_native_read(const char* name, const char* ext, u8* out, size_t cap, size_t* size) {
    u16 index;
    MfsRamDirectoryEntry* entry;
    if (cap < 16 + 0xC830 || !gdx_recovery_native_prepare()) {
        return 0;
    }
    *size = 0;
    index = Mfs_GetFileIndex(gWorkingDirectory, (char*) name, (char*) ext);
    if (index == MFS_ENTRY_DOES_NOT_EXIST) {
        return 1;
    }
    entry = &gMfsRamArea.directoryEntry[index];
    if (entry->fileSize != 0xC830 ||
        Mfs_LoadFileInDir(gWorkingDirectory, (char*) name, (char*) ext, out + 16, 0xC830) != 0 ||
        !recovery_track_valid(out + 16)) {
        return 0;
    }
    memset(out, 0, 16);
    out[0] = entry->attr & 0xFF;
    out[1] = entry->attr >> 8;
    memcpy(out + 2, entry->companyCode, 2);
    memcpy(out + 4, entry->gameCode, 4);
    out[8] = entry->copyCount;
    out[9] = entry->renewalCounter;
    out[10] = entry->reserve3;
    memcpy(out + 11, &entry->creationDate, 4);
    *size = 16 + 0xC830;
    return 1;
}

int gdx_recovery_native_write(const char* name, const char* ext, const u8* bytes, size_t size) {
    u16 index;
    MfsRamDirectoryEntry* entry;
    if (!sRecoveryHealthy) {
        return 0;
    }
    memcpy(&sRecoveryRamBefore, &gMfsRamArea, sizeof(gMfsRamArea));
    if (!size) {
        if (Mfs_DeleteFileInDir(gWorkingDirectory, (char*) name, (char*) ext, true) != 0) {
            return recovery_rollback();
        }
        return 1;
    }
    if (size != 16 + 0xC830 || !recovery_track_valid(bytes + 16) ||
        Mfs_GetFileIndex(gWorkingDirectory, (char*) name, (char*) ext) !=
        MFS_ENTRY_DOES_NOT_EXIST) {
        return 0;
    }
    memset(sRecoveryBlock, 0, sizeof(sRecoveryBlock));
    memcpy(sRecoveryBlock, bytes + 16, 0xC830);
    if (Mfs_SaveFile(gWorkingDirectory, (char*) name, (char*) ext, sRecoveryBlock, 0xC830,
                     bytes[0] | ((u16) bytes[1] << 8), bytes[8], false) != 0) {
        return recovery_rollback();
    }
    index = Mfs_GetFileIndex(gWorkingDirectory, (char*) name, (char*) ext);
    if (index == MFS_ENTRY_DOES_NOT_EXIST) {
        return recovery_rollback();
    }
    entry = &gMfsRamArea.directoryEntry[index];
    entry->attr = bytes[0] | ((u16) bytes[1] << 8);
    memcpy(entry->companyCode, bytes + 2, 2);
    memcpy(entry->gameCode, bytes + 4, 4);
    entry->copyCount = bytes[8];
    entry->renewalCounter = bytes[9];
    entry->reserve3 = bytes[10];
    memcpy(&entry->creationDate, bytes + 11, 4);
    if (recovery_read_raw(name, ext, sRecoveryBlock, 0xC830) != 1 ||
        memcmp(sRecoveryBlock, bytes + 16, 0xC830) != 0 || Mfs_BackupRamArea() < 0) {
        return recovery_rollback();
    }
    return 1;
}

int gdx_recovery_native_cups(u8* cent, u8* cenl) {
    s32 i;
    memset(cent, 0, GDX_EDIT_CUP_CENT_SIZE);
    memset(cenl, 0, GDX_EDIT_CUP_CENL_SIZE);
    memcpy(cenl, GDX_EDIT_CUP_CENL_MAGIC, 4);
    if (recovery_read_raw("CRS_ENTRY", "CENT", cent, GDX_EDIT_CUP_CENT_SIZE) < 0 ||
        recovery_read_raw("CRS_ENTRY", "CENL", cenl, GDX_EDIT_CUP_CENL_SIZE) < 0 ||
        !gdx_edit_cup_cenl_valid(cenl, GDX_EDIT_CUP_CENL_SIZE)) {
        return 0;
    }
    for (i = 0; i < GDX_EDIT_CUP_ROWS; ++i) {
        if (memchr(cent + i * 9, 0, 9) == NULL) {
            return 0;
        }
    }
    return 1;
}

int gdx_recovery_native_cup_slot(int longNames, int slot, const u8* bytes, size_t size) {
    u8 cent[GDX_EDIT_CUP_CENT_SIZE];
    u8 cenl[GDX_EDIT_CUP_CENL_SIZE];
    u8* image;
    size_t width = longNames ? 21 : 9;
    size_t imageSize = longNames ? sizeof(cenl) : sizeof(cent);
    size_t offset = (longNames ? 4 : 0) + slot * width;
    s32 wrote;
    s32 anyLong = false;
    s32 i;
    if (!sRecoveryHealthy || slot < 0 || slot >= 24 || (size && size != width) ||
        (size && (!bytes || !bytes[0] || memchr(bytes, 0, size) == NULL)) ||
        !gdx_recovery_native_cups(cent, cenl)) {
        return 0;
    }
    image = longNames ? cenl : cent;
    memset(image + offset, 0, width);
    if (size) {
        memcpy(image + offset, bytes, size);
    }
    memset(sRecoveryBlock, 0, sizeof(sRecoveryBlock));
    memcpy(sRecoveryBlock, image, imageSize);
    memcpy(&sRecoveryRamBefore, &gMfsRamArea, sizeof(gMfsRamArea));
    if (longNames) {
        for (i = 4; i < sizeof(cenl); ++i) {
            anyLong |= cenl[i] != 0;
        }
    }
    if (longNames && !anyLong) {
        wrote = Mfs_DeleteFileInDir(gWorkingDirectory, "CRS_ENTRY", "CENL", true);
        if (wrote != 0 || recovery_read_raw("CRS_ENTRY", "CENL", sRecoveryBlock, imageSize) != 0) {
            return recovery_rollback();
        }
    } else {
        wrote = Mfs_SaveFile(gWorkingDirectory, "CRS_ENTRY", longNames ? "CENL" : "CENT", sRecoveryBlock,
                            (u32) imageSize, 0, 0xFF, true);
        // Stock overwrite does not propagate every delete/rename error, so verify the entire cup image.
        if (wrote != 0 || recovery_read_raw("CRS_ENTRY", longNames ? "CENL" : "CENT", sRecoveryBlock,
                                          imageSize) != 1 || memcmp(sRecoveryBlock, image, imageSize) != 0) {
            return recovery_rollback();
        }
    }
    gdx_edit_cup_unpack_cent(cent, &gEditCupTrackNames[0][0]);
    gdx_edit_cup_merge_cenl(cenl, &gEditCupTrackNames[0][0]);
    return 1;
}
#endif
