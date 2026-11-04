void SLMFSDeleteFile(u16 dirId, char* name, char* extension, bool writeChanges) {
    PRINTF("SLMFSDeleteFile name:%s, type:%s\n", name, extension);
    PRINTF("SLMFSDeleteFile LOOP\n");

    while (true) {
        func_80707B08();
        if (D_8076CBB0) {
            SLMFSNewDisk();
            D_8076CBB0 = false;
        }
        if (Mfs_DeleteFileInDir(dirId, name, extension, writeChanges) == 0) {
            PRINTF("SLMFSDeleteFile Good Return\n");
            return;
        }

        sSLLeoError = gMfsError;
        PRINTF("SLMFSDeleteFile Return %d\n", sSLLeoError);

        switch (sSLLeoError) {
            case LEO_ERROR_GOOD:
            case N64DD_NOT_FOUND:
                return;
#ifdef PORT
            // Host save refusals cannot be repaired by waiting for physical disk hardware.
            case N64DD_AREA_LACKED:
            case N64DD_DISK_DAMAGED:
            case N64DD_ARGUMENT_ILLEGAL:
            case N64DD_DISKID_ILLEGAL:
            case N64DD_READ_ONLY_MEDIA:
            case N64DD_MANAGER_NOT_CREATED:
                return;
#endif
            case LEO_ERROR_BUSY:
            case LEO_ERROR_QUEUE_FULL:
                func_80704CE0();
                break;
            case LEO_ERROR_MEDIUM_NOT_PRESENT:
                func_8070F8A4(sSLLeoError, 4);
                while (SLCheckDiskInsert() != 0) {}
                break;
            case LEO_ERROR_MEDIUM_MAY_HAVE_CHANGED:
                while (SLCheckDiskInsert() != 0) {}
                break;
            case LEO_ERROR_DIAGNOSTIC_FAILURE:
                func_8070F8A4(sSLLeoError, 2);
                while (func_80707780() != 0) {}
                while (SLCheckDiskInsert() != 0) {}
                break;
            case LEO_ERROR_EJECTED_ILLEGALLY_RESUME:
                func_8070F8A4(sSLLeoError, 1);
                while (SLCheckDiskInsert() != 0) {}
                break;
            case LEO_ERROR_UNRECOVERED_READ_ERROR:
            case 0x10A:
                func_80703948();
                SLMFSRecoverManageArea();
                break;
            case N64DD_MEDIA_NOT_INIT:
                PRINTF("hoge40\n");
                PRINTF("hoge41\n");
                PRINTF("MEDIAINIT_AND_WAIT 9\n");
                PRINTF("MEDIAINIT_AND_WAIT 10\n");
                func_80704068();
                func_807038B0();
                func_80706518(1, 0x20, 0);
                func_807040C8();
                return;
            default:
                func_8070F8A4(sSLLeoError, 0);
                while (true) {}
        }
    }
}
