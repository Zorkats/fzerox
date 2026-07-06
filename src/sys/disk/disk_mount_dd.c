#include "global.h"
#include "leo/mfs.h"

extern LEODiskID D_800CD2B0;

#define LEO_DISK_TIME(yearhi, yearlo, month, day, hour, minute, second) \
    { 0, 0x##yearhi, 0x##yearlo, 0x##month, 0x##day, 0x##hour, 0x##minute, 0x##second }

void DiskMount_Init(void) {
    // 1999-12-31 23:59:59
    LEODiskTime diskTime = LEO_DISK_TIME(19, 99, 12, 31, 23, 59, 59);

    func_80762330(&diskTime);
    GDX_CK(DM1_time_set);
    Mfs_SetGameCode("01", "EFZJ");
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
