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
 * The MFS "RAM area" is the 64DD disk's own filesystem management block: a volume header, a
 * 0xB3A-entry file allocation table, and an array of MfsRamDirectoryEntry. It lives at the
 * first three LBAs of the writable region (gRamAreaCapacity.startLBA), is mirrored by a
 * three-LBA backup copy immediately after it, and is moved wholesale in and out of gMfsRamArea
 * by mfs_ram.c -- always exactly three LBAs, always that one struct.
 *
 * On N64 that block is BIG-ENDIAN, because it was authored by a big-endian CPU writing this
 * very struct. This port's drive replacement (port/n64_leo.c LeoReadWrite) is a verbatim bcopy
 * -- correct for file payloads, which are opaque bytes, but it leaves gMfsRamArea holding
 * big-endian bytes that the decomp then reads with host (little-endian) semantics.
 *
 * The field that decides everything is MfsRamDirectoryEntry.attr. EVERY defined attr bit
 * (COPYLIMIT..DIRECTORY, bits 9..15) lives in the HIGH byte, so a correctly ordered attr always
 * has a zero low byte and a byte-swapped one always has a zero high byte. Unswapped,
 * Mfs_GetDirectoryIndex never matches MFS_FILE_ATTR_DIRECTORY, so Mfs_GetFilesPreparation
 * returns MFS_ENTRY_DOES_NOT_EXIST and the EK's "Choose a file" builder (func_xk1_8002BD64,
 * overlays/expansion_kit/A8140.c) takes its early-return with an empty list, while Mfs_SaveFile
 * bails with N64DD_NOT_FOUND before writing a byte. Both failures are SILENT, because
 * func_xk1_8002E368 (overlays/expansion_kit/AB150.c) folds N64DD_NOT_FOUND into its SUCCESS
 * branch and sets D_807C6EA8.unk_08 = 0, the "no message" case in the prompt renderer.
 *
 * The fix cannot live at the transfer boundary (mfs_device.c Mfs_ReadLBA/Mfs_WriteLBA, or
 * LeoReadWrite): the volume checksum spans the WHOLE three-LBA area, and Mfs_ReadLBA splits the
 * transfer into single-LBA staged copies whenever &gMfsRamArea is not 16-byte aligned, so a
 * per-call hook cannot always see enough of the area to recompute it. Normalize the IMAGE once
 * instead, here, at the single point where the disk is mounted and before anything has read the
 * management area:
 *
 *   - DiskMount_Init is called exactly once (sys_main.c), after LeoDriveExist() has loaded the
 *     image and after the .gdd sidecar has been replayed over it, and before SLMFSCreateManager
 *     and every MFS operation.
 *   - The pass is IDEMPOTENT and self-describing: it reads the window, decides from the root
 *     entry's attr byte pattern whether it is disk-order or already host-order, and only rewrites
 *     when it is disk-order. That is what makes it safe to run every boot even though the write
 *     it performs is itself journalled into the .gdd and replayed next boot.
 *   - It rewrites the volume checksum in host word order, exactly as Mfs_CalculateVolumeChecksum
 *     would, because a field-selective swap necessarily invalidates a whole-area XOR. This also
 *     repairs a pre-existing defect in the shipped image, whose stored checksum is the one for
 *     renewalCounter 0x47 with 0x48 stored -- the mismatch that made Mfs_CheckChecksum fail and
 *     drove the EK into its 0x10A "recover manage area" path on every boot.
 *   - Both the primary window and the backup window are normalized independently, because
 *     Mfs_CopyRamAreaFromBackup reads the backup directly and must see the same convention.
 *
 * Fields deliberately NOT swapped: MfsRamId.formatDate and MfsRamDirectoryEntry.creationDate.
 * MfsTimeFormat is a union that Mfs_LEODiskTimeToMfsTime writes through BOTH u16 members
 * (unks0/unks2) and u8 members (unkb0..unkb3) of the same bytes, so its bit layout is only
 * self-consistent in the original byte order -- a byte swap cannot make both access widths agree.
 * Nothing in the EK reads either field (the picker sorts on name bytes, func_xk1_8002CA98), so
 * leaving them in disk order costs nothing.
 *
 * The user's pristine .ndd is never touched: LeoReadWrite writes into the in-memory image and
 * records the dirty range in the .gdd sidecar (port/disk_savefile.cpp), same as any game save.
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

/* PORT: the game code the MFS volume is written under.
 *
 * Retail hardcodes "EFZJ" because the only disk a JP Expansion Kit can ever mount is the JP EK
 * disk, whose LEODiskID.gameName is "EFZJ". This port mounts whatever .ndd the user supplies, and
 * the fan-translated English EK disk re-IDs itself as "EFZE".
 *
 * That mismatch is not cosmetic. The translated volume's MfsRamId.attr is 0x20 =
 * MFS_VOLUME_ATTR_VPROTECT_WRITE, "prohibits writes unless game and company code match", and
 * Mfs_ValidateFileSystemOperation (mfs_validate.c) enforces it by comparing the DISK ID's company
 * and gameName against gCompanyCode/gGameCode. With gGameCode stuck at "EFZJ" every write
 * validation returns -1, Mfs_SaveFile reports 0x106, and no custom machine or course can be
 * saved -- the second blocker behind broken saving, sitting immediately behind the byte-order
 * bug above.
 *
 * So take the code from the mounted disk's own boot ID, which is what the retail pairing would
 * have produced anyway. leoBootID is filled from the image's disk-ID block by
 * gdx_leo_on_disk_loaded() (port/n64_leo.c). Only accept it when all four bytes are present:
 * Mfs_SetGameCode copies via mfsStrnCpy, which stops at a NUL, and a short copy would leave
 * gGameCode partly zeroed and fail the compare just as badly as the wrong code did. Anything
 * unexpected falls back to retail's literal so behaviour is never worse than before.
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
    /* Must precede SLMFSCreateManager and therefore every MFS read of the management area. See the
       essay above for why the mount point is the right place for this. */
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
