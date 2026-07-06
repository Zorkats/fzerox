#ifndef PORT_DISK_SEGMENTS_H
#define PORT_DISK_SEGMENTS_H

/* G-Diffuser — Expansion Kit disk-segment addressing (PORT).
 *
 * The runtime disk image (baserom.translated.ek.ndd) is a PHYSICAL/zoned 64DD
 * dump. Its file offsets do NOT equal the leo library's logical LBA->byte math
 * (LeoLBAToByte sums per-zone block sizes and drifts from the physical layout
 * across zone boundaries), so disk reads cannot go through the leo path. Instead
 * SEGMENT_DISK_START(x) yields a tagged handle that DiskDrive_LoadData decodes
 * and serves directly from gdx_disk_buffer at a PHYSICAL offset located
 * empirically in the disk image.
 *
 * Handle layout (s32, positive):
 *   [31:24] = GDX_DISK_TAG (0x7D)   [23:16] = table id   [15:0] = record base
 * The game computes SEGMENT_DISK_START(seg) + index, so the record base plus the
 * game's index stays inside the low 16 bits.
 */

#define GDX_DISK_TAG        0x7D
#define GDX_DISK_HANDLE(table, base) \
    (((s32)GDX_DISK_TAG << 24) | ((s32)(table) << 16) | (s32)(base))

/* Table ids consumed by DiskDrive_LoadData (disk_drive_dd.c). */
#define GDX_DTAB_UNMAPPED   0  /* overlay/texture segments: DiskDrive_LoadOverlay is a PORT no-op */
#define GDX_DTAB_DDCOURSE   1  /* DD course records: 6 edit templates + 12 DD courses */
#define GDX_DTAB_DDGHOST    2  /* DD staff ghosts: not yet located -> zero-filled (safe) */

/* Physical layout of the DD course record table in the .ndd (located by
 * scanning for CREATOR_NINTENDO CourseData; JP and translated disks identical).
 * 6 Edit-Cup templates then 12 DD default courses, contiguous, one record/LBA. */
#define GDX_DDCOURSE_BASE   0x00EA6A00u  /* file offset of record 0 (edit slot 0) */
#define GDX_DDCOURSE_STRIDE 0x4510u      /* bytes per record (zone block size) */
#define GDX_DDCOURSE_COUNT  18           /* 6 edit + 12 DD (silence_3 = record 6) */

/* Read (courses) — mapped to real disk data. */
#define GDX_DISK_START_silence_3             GDX_DISK_HANDLE(GDX_DTAB_DDCOURSE, 0)
/* Read (staff ghosts) — safe zero-fill until located. */
#define GDX_DISK_START_silence_3_staff_ghost GDX_DISK_HANDLE(GDX_DTAB_DDGHOST, 0)

/* Overlay / texture segments: only ever passed to DiskDrive_LoadOverlay /
 * DiskDrive_LoadOverlayProgressBar, which are PORT no-ops (overlays are compiled
 * in statically). The handle value is never dereferenced; UNMAPPED keeps builds
 * clean and any stray read zero-fills. */
#define GDX_DISK_START_ovl_i2                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ovl_i3                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ovl_i4                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ovl_i6                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ovl_i9                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ovl_i10               GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_course_select         GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ending                GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_records               GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_expansion_kit         GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_expansion_kit_textures GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_course_edit           GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_course_edit_textures  GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_machine_create        GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)
#define GDX_DISK_START_ead_demo              GDX_DISK_HANDLE(GDX_DTAB_UNMAPPED, 0)

#endif /* PORT_DISK_SEGMENTS_H */
