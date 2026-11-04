void func_xk2_800EB938(u16 dirId, char* name, char* extension, u8* buf, u32 fileSize, s32 attr, s32 copyCount,
                       bool writeChanges) {
    if (extension[3] == 'D') {
        SLMFSDeleteFile(dirId, name, "CRSE", writeChanges);
    } else {
        SLMFSDeleteFile(dirId, name, "CRSD", writeChanges);
    }
#ifdef PORT
    if ((gMfsError != LEO_ERROR_GOOD) && (gMfsError != N64DD_NOT_FOUND)) {
        return;
    }
#endif
    SLMFSSave(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
}
