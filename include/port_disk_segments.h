#ifndef PORT_DISK_SEGMENTS_H
#define PORT_DISK_SEGMENTS_H

/* Expansion Kit disk-segment addressing.
 *
 * The runtime disk image is a physical/zoned 64DD dump, so its file offsets do not
 * match the leo library's logical LBA->byte math (LeoLBAToByte sums per-zone block
 * sizes and drifts across zone boundaries). Disk reads therefore bypass leo:
 * SEGMENT_DISK_START(x) yields a tagged handle that DiskDrive_LoadData decodes and
 * serves from gdx_disk_buffer at a physical offset.
 *
 * Handle layout (s32, positive):
 *   [31:24] = GDX_DISK_TAG (0x7D)   [23:16] = table id   [15:0] = record base
 * The game computes SEGMENT_DISK_START(seg) + index, so a record base must leave room
 * for the game's index inside the low 16 bits.
 */

#define GDX_DISK_TAG        0x7D
#define GDX_DISK_HANDLE(table, base) \
    (((s32)GDX_DISK_TAG << 24) | ((s32)(table) << 16) | (s32)(base))

/* Table ids consumed by DiskDrive_LoadData (disk_drive_dd.c). */
#define GDX_DTAB_UNMAPPED   0  /* overlay/texture segments: DiskDrive_LoadOverlay is a PORT no-op */
#define GDX_DTAB_DDCOURSE   1  /* DD course records: 6 edit templates + 12 DD courses */
#define GDX_DTAB_DDGHOST    2  /* DD staff ghosts: not yet located -> zero-filled */

/* Physical layout of the DD course record table in the .ndd, located empirically
 * (JP and translated disks identical): 6 Edit-Cup templates then 12 DD default
 * courses, contiguous, one record per LBA. */
#define GDX_DDCOURSE_BASE   0x00EA6A00u  /* file offset of record 0 (edit slot 0) */
#define GDX_DDCOURSE_STRIDE 0x4510u      /* bytes per record (zone block size) */
#define GDX_DDCOURSE_COUNT  18           /* 6 edit + 12 DD (silence_3 = record 6) */

#define GDX_DISK_START_silence_3             GDX_DISK_HANDLE(GDX_DTAB_DDCOURSE, 0)
#define GDX_DISK_START_silence_3_staff_ghost GDX_DISK_HANDLE(GDX_DTAB_DDGHOST, 0)

/* Only ever passed to DiskDrive_LoadOverlay / DiskDrive_LoadOverlayProgressBar, which
 * are PORT no-ops (the overlays are compiled in statically), so the handle is never
 * dereferenced; UNMAPPED makes any stray read zero-fill. */
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
