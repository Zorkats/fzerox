void SLMFSSave(u16 dirId, char* name, char* extension, u8* buf, u32 fileSize, s32 attr, s32 copyCount,
               bool writeChanges) {
    PRINTF("hoge48\n");
    PRINTF("SLMFSSave\n");
    PRINTF("SLMFSSave LOOP\n");

    osRecvMesg(&gMFSMesgQ, NULL, OS_MESG_NOBLOCK);
    while (true) {
        func_80704068();
        func_80707B08();
        if (D_8076CBB0) {
            SLMFSNewDisk();
            D_8076CBB0 = false;
        }
        func_807040C8();
        if (Mfs_SaveFile(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges) == 0) {
            PRINTF("MFSMesgQ Send 24\n");
            osSendMesg(&gMFSMesgQ, NULL, OS_MESG_BLOCK);
            return;
        }
        sSLLeoError = gMfsError;
        PRINTF("SLMFSSave Error %d\n", sSLLeoError);

        switch (sSLLeoError) {
            case LEO_ERROR_GOOD:
                PRINTF("MFSMesgQ Send 25\n");
                osSendMesg(&gMFSMesgQ, NULL, OS_MESG_BLOCK);
                if (0) {
                    PRINTF("SLMFSSave 0\n");
                    PRINTF("SLMFSSave 1\n");
                    PRINTF("SLMFSSave 2\n");
                    PRINTF("hoge49\n");
                    PRINTF("SLMFSSave 3\n");
                    PRINTF("hoge50\n");
                }
                return;
#ifdef PORT
            // Host save refusals cannot be repaired by waiting for physical disk hardware.
            case N64DD_AREA_LACKED:
            case N64DD_DISK_DAMAGED:
            case N64DD_ARGUMENT_ILLEGAL:
            case N64DD_DISKID_ILLEGAL:
            case N64DD_READ_ONLY_MEDIA:
            case N64DD_MANAGER_NOT_CREATED:
            case N64DD_NOT_FOUND:
                osSendMesg(&gMFSMesgQ, NULL, OS_MESG_BLOCK);
                return;
#endif
            case LEO_ERROR_BUSY:
            case LEO_ERROR_QUEUE_FULL:
                func_80704CE0();
                break;
            case LEO_ERROR_MEDIUM_NOT_PRESENT:
                func_80704068();
                func_8070F8A4(sSLLeoError, 4);
                while (SLCheckDiskInsert() != 0) {}
                func_807040C8();
                break;
            case LEO_ERROR_MEDIUM_MAY_HAVE_CHANGED:
                func_80704068();
                while (SLCheckDiskInsert() != 0) {}
                func_807040C8();
                break;
            case LEO_ERROR_DIAGNOSTIC_FAILURE:
                func_80704068();
                func_8070F8A4(sSLLeoError, 2);
                while (func_80707780() != 0) {}
                while (SLCheckDiskInsert() != 0) {}
                func_807040C8();
                break;
            case LEO_ERROR_EJECTED_ILLEGALLY_RESUME:
                func_80704068();
                func_8070F8A4(sSLLeoError, 1);
                while (SLCheckDiskInsert() != 0) {}
                func_807040C8();
                break;
            case LEO_ERROR_UNRECOVERED_READ_ERROR:
                func_80703948();
                SLMFSRecoverManageArea();
                break;
            case 0x10A:
                func_80703948();
                SLMFSRecoverManageArea();
                break;
            case N64DD_MEDIA_NOT_INIT:
                PRINTF("MEDIAINIT_AND_WAIT 17\n");
                PRINTF("MEDIAINIT_AND_WAIT 18\n");
                PRINTF("MEDIAINIT_AND_WAIT 19\n");
                func_80704068();
                func_807038B0();
                func_80706518(1, 0x20, 0);
                func_807040C8();
                break;
            default:
                func_8070F8A4(sSLLeoError, 0);
                while (true) {}
        }
    }
}
