#include "global.h"
#include "leo/mfs.h"
#ifdef PORT
#include "libc/stddef.h" /* offsetof, for the management-area geometry below */
#endif

extern LEODiskID D_800CD2B0;

#define LEO_DISK_TIME(yearhi, yearlo, month, day, hour, minute, second) \
    { 0, 0x##yearhi, 0x##yearlo, 0x##month, 0x##day, 0x##hour, 0x##minute, 0x##second }

#ifdef PORT
/* PORT: 64DD MFS management-area byte order.
 *
 * The MFS management block is big-endian on disk, and port/n64_leo.c's LeoReadWrite is a
 * verbatim bcopy -- right for opaque file payloads, but it leaves gMfsRamArea holding
 * big-endian bytes the decomp then reads little-endian. Every defined MfsRamDirectoryEntry.attr
 * bit lives in the high byte, so unswapped, Mfs_GetDirectoryIndex never matches
 * MFS_FILE_ATTR_DIRECTORY: the EK file picker builds an empty list and Mfs_SaveFile bails with
 * N64DD_NOT_FOUND before writing a byte. Both fail SILENTLY, because func_xk1_8002E368 (AB150.c)
 * folds N64DD_NOT_FOUND into its success branch.
 *
 * The fix cannot live at the transfer boundary (Mfs_ReadLBA/Mfs_WriteLBA or LeoReadWrite): the
 * volume checksum spans the whole three-LBA area, and Mfs_ReadLBA splits the transfer into
 * single-LBA staged copies whenever &gMfsRamArea is not 16-byte aligned, so a per-call hook
 * cannot always see enough of the area to recompute it. Normalize the image once, here, at the
 * single mount point. Notes on the shape this forces:
 *
 *   - The pass must be IDEMPOTENT: its own writes are journalled into the .gdd sidecar and
 *     replayed next boot, so it decides from the root entry's attr byte pattern whether the
 *     window is disk-order or already host-order and rewrites only in the first case.
 *   - It rewrites the volume checksum, because a field-selective swap necessarily invalidates a
 *     whole-area XOR. This also repairs a defect in the shipped image (checksum stored for
 *     renewalCounter 0x47 against a stored 0x48), which drove the EK into its 0x10A "recover
 *     manage area" path on every boot.
 *   - Primary and backup windows are normalized independently: Mfs_CopyRamAreaFromBackup reads
 *     the backup directly and must see the same convention.
 *   - MfsRamId.formatDate and MfsRamDirectoryEntry.creationDate are deliberately NOT swapped.
 *     Mfs_LEODiskTimeToMfsTime writes the MfsTimeFormat union through both its u16 and its u8
 *     members, so no byte order makes both access widths agree. Nothing in the EK reads either.
 *
 * The user's pristine .ndd is never touched: writes land in the in-memory image and the .gdd
 * dirty range (port/disk_savefile.cpp), same as any game save.
 */

/* port/disk_buffer.cpp -- the loaded disk image. Raw extern because this decomp TU cannot include
   the host headers; same boundary idiom as mfs_ram.c's gdx_disk_allow_format() and 75000.c's
   gdx_cki(). Only used to skip the pass when no disk is mounted. */
extern unsigned char* gdx_disk_buffer;

/* Mfs_ReadRamArea/Mfs_BackupRamArea move exactly this many LBAs, and the backup copy sits exactly
   this far behind the primary. Naming the constant keeps the two uses provably in step. */
#define GDX_MFS_MGMT_LBAS 3

static void GdxMfsSwapU16(u16* p) {
    u16 v = *p;

    *p = (u16) (((v >> 8) & 0xFF) | ((v << 8) & 0xFF00));
}

static void GdxMfsSwapU32(u32* p) {
    u32 v = *p;

    *p = ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}

/* Normalize one three-LBA management window in the mounted image, in place. `startLBA` is a
   writable-region LBA (gRamAreaCapacity.startLBA or +GDX_MFS_MGMT_LBAS); gMfsRamArea is used as
   the staging buffer, which is free game here -- DiskMount_Init runs before the first real
   Mfs_ReadRamArea, and Mfs_CreateLeoManager wipes id.diskId[0] right after us anyway. */
static void GdxMfsNormalizeWindow(s32 startLBA) {
    LEOCmd cmd;
    MfsRamDirectoryEntry* entry;
    s32 areaSize = 0;
    s32 entryCount;
    s32 checksum;
    s32* word;
    u16 rootAttr;
    s32 i;

    if (LeoLBAToByte(startLBA, GDX_MFS_MGMT_LBAS, &areaSize) != LEO_ERROR_GOOD) {
        gdx_cki("[mfs] byte-order pass: LBA out of range, skipped window", startLBA);
        return;
    }
    /* A management area smaller than the header + FAT + one entry is not a management area, and
       one larger than the struct would overrun gMfsRamArea. Either means the disk geometry is not
       what MFS expects (e.g. a type-6 image, which has no writable region at all). */
    if ((areaSize <= (s32) (offsetof(MfsRamArea, directoryEntry) + sizeof(MfsRamDirectoryEntry))) ||
        (areaSize > (s32) sizeof(MfsRamArea))) {
        gdx_cki("[mfs] byte-order pass: implausible management-area size, skipped", areaSize);
        return;
    }

    if (LeoReadWrite(&cmd, OS_READ, (u32) startLBA, &gMfsRamArea, GDX_MFS_MGMT_LBAS, NULL) !=
        LEO_ERROR_GOOD) {
        gdx_cki("[mfs] byte-order pass: management-area read failed at LBA", startLBA);
        return;
    }

    /* Only touch a formatted MFS volume. This is retail's own test (Mfs_ValidateRamVolume compares
       id.diskId against D_i1_80428648, "64dd-Multi") and it is a char compare, so it is valid
       regardless of which byte order the rest of the block is in. */
    if (mfsStrnCmp((u8*) gMfsRamArea.id.diskId, (u8*) D_i1_80428648, 10) != 0) {
        gdx_cki("[mfs] byte-order pass: not an MFS volume, skipped window at LBA", startLBA);
        return;
    }

    entryCount = (areaSize - (s32) offsetof(MfsRamArea, directoryEntry)) / (s32) sizeof(MfsRamDirectoryEntry);
    rootAttr = gMfsRamArea.directoryEntry[0].attr;
    gdx_cki("[mfs] byte-order pass: root entry attr as read", rootAttr);

    if (rootAttr == 0) {
        /* Formatted volume with no root entry: nothing to normalize and nothing to infer the
           current byte order from. Leave it exactly as found. */
        gdx_ck("[mfs] byte-order pass: empty root entry, skipped window");
        return;
    }
    if ((rootAttr & 0xFF00) != 0) {
        /* High byte populated => already host order. This is the steady state after the first
           normalized boot, when the .gdd replay has already put host-order bytes back. */
        gdx_cki("[mfs] byte-order pass: already host order, skipped window at LBA", startLBA);
        return;
    }

    GdxMfsSwapU16(&gMfsRamArea.id.renewalCounter);
    for (i = 0; i < (s32) ARRAY_COUNT(gMfsRamArea.fileAllocationTable); i++) {
        GdxMfsSwapU16(&gMfsRamArea.fileAllocationTable[i]);
    }
    for (i = 0; i < entryCount; i++) {
        entry = &gMfsRamArea.directoryEntry[i];
        GdxMfsSwapU16(&entry->attr);
        GdxMfsSwapU16(&entry->parentDirId);
        /* dirId and fileAllocationTableId are the two arms of the same u16 union, so one swap
           serves directories and files alike. fileSize only exists on the file arm -- on the
           directory arm those four bytes are reserve1[4], which must be left alone. attr is
           already host order at this point, so the test below reads the real value. */
        GdxMfsSwapU16(&entry->dirId);
        if (entry->attr & MFS_FILE_ATTR_FILE) {
            GdxMfsSwapU32((u32*) &entry->fileSize);
        }
    }

    /* Rewrite the volume checksum the way Mfs_CalculateVolumeChecksum does: zero the field, XOR
       every host-order 32-bit word of the area, store the result. Mfs_CheckChecksum then XORs the
       same span (including the stored value) and gets zero. Every LBA byte-size in LEOBYTE_TBL2 is
       a multiple of 16, so areaSize divides evenly into words and no field straddles the end. */
    gMfsRamArea.id.checksum = 0;
    checksum = 0;
    word = (s32*) &gMfsRamArea;
    for (i = 0; i < areaSize / (s32) sizeof(s32); i++) {
        checksum ^= *word++;
    }
    gMfsRamArea.id.checksum = checksum;

    if (LeoReadWrite(&cmd, OS_WRITE, (u32) startLBA, &gMfsRamArea, GDX_MFS_MGMT_LBAS, NULL) !=
        LEO_ERROR_GOOD) {
        gdx_cki("[mfs] byte-order pass: management-area WRITE FAILED at LBA", startLBA);
        return;
    }
    gdx_cki("[mfs] byte-order pass: normalized window at LBA", startLBA);
    gdx_cki("[mfs] byte-order pass: directory entries in window", entryCount);
    gdx_cki("[mfs] byte-order pass: root entry attr now", gMfsRamArea.directoryEntry[0].attr);
    gdx_cki("[mfs] byte-order pass: volume attr", gMfsRamArea.id.attr);
}

static void GdxDiskNormalizeMfsManagementArea(void) {
    s32 startLBA;

    if (gdx_disk_buffer == NULL) {
        return;
    }
    /* gRamAreaCapacity is not populated yet (func_i1_80404204 runs from the first MFS mount, which
       is exactly what we are getting ahead of), so derive the writable-region start the same way
       LeoReadCapacity does -- including the -0x18 bias, which is what MFS matches against
       LEO_LBA_RAM_TOPn to recover the disk type. LEOdisk_type and __leoActive were both set by
       gdx_leo_on_disk_loaded() when LeoDriveExist() loaded the image, well before this point. */
    if (!__leoActive || (LEOdisk_type >= 7)) {
        return;
    }
    startLBA = (s32) LEORAM_START_LBA[LEOdisk_type] - 0x18;

    GdxMfsNormalizeWindow(startLBA);
    GdxMfsNormalizeWindow(startLBA + GDX_MFS_MGMT_LBAS);
}

/* Retail can hardcode "EFZJ" because a JP Expansion Kit only ever mounts the JP EK disk. This
 * port mounts whatever .ndd the user supplies, and the fan-translated disk re-IDs itself as
 * "EFZE". The mismatch is not cosmetic: that volume sets MFS_VOLUME_ATTR_VPROTECT_WRITE, so
 * Mfs_ValidateFileSystemOperation compares the disk ID against gGameCode and, stuck at "EFZJ",
 * fails every write with 0x106 -- no custom machine or course can be saved.
 *
 * Take the code from the mounted disk's own boot ID instead (filled by gdx_leo_on_disk_loaded).
 * Accept it only when all four bytes are present: Mfs_SetGameCode copies via mfsStrnCpy, which
 * stops at a NUL, so a short copy fails the compare just as badly. Otherwise fall back to
 * retail's literal.
 */
static void GdxDiskSetGameCodeFromDisk(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (leoBootID.gameName[i] == '\0') {
            gdx_ck("[mfs] disk boot ID has no usable game code; using retail EFZJ");
            Mfs_SetGameCode("01", "EFZJ");
            return;
        }
    }

    Mfs_SetGameCode("01", (char*) leoBootID.gameName);
    /* Logged packed big-endian so the hex reads as the four ASCII characters. */
    gdx_cki("[mfs] game code taken from disk boot ID",
            (leoBootID.gameName[0] << 24) | (leoBootID.gameName[1] << 16) | (leoBootID.gameName[2] << 8) |
                leoBootID.gameName[3]);
}
#endif

void DiskMount_Init(void) {
    // 1999-12-31 23:59:59
    LEODiskTime diskTime = LEO_DISK_TIME(19, 99, 12, 31, 23, 59, 59);

#ifdef PORT
    /* Must precede SLMFSCreateManager and therefore every MFS read of the management area. */
    GdxDiskNormalizeMfsManagementArea();
#endif
    func_80762330(&diskTime);
    GDX_CK(DM1_time_set);
#ifdef PORT
    GdxDiskSetGameCodeFromDisk();
#else
    Mfs_SetGameCode("01", "EFZJ");
#endif
    GDX_CK(DM2_gamecode_set);
    SLMFSCreateManager(LEO_MANAGER_REGION_NONE);
    GDX_CK(DM3_mfs_manager);
    func_8070481C();
    GDX_CK(DM4_8070481C);
    SLLeoReadDiskID(&D_800CD2B0);
    GDX_CK(DM5_diskid_read);
    SLLeoModeSelectAsync(0, 0);
    GDX_CK(DM6_modeselect);
}
