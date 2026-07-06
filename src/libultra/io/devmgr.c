#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "PR/piint.h"
#ifdef PORT
#include <stddef.h>
#include <string.h>
#endif

void __osDevMgrMain(void* args) {
    OSIoMesg* mb;
    OSMesg em;
    OSMesg dummy;
    s32 ret;
    OSDevMgr* dm;
    s32 messageSend = 0;

    dm = (OSDevMgr*) args;
    mb = NULL;
    ret = 0;

    while (TRUE) {
        osRecvMesg(dm->cmdQueue, (OSMesg) &mb, OS_MESG_BLOCK);

#ifndef PORT
        if (mb->piHandle != NULL && mb->piHandle->type == DEVICE_TYPE_64DD &&
            (mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0 ||
             mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_1)) {
            __OSBlockInfo* blockInfo;
            __OSTranxInfo* info;
            info = &mb->piHandle->transferInfo;
            blockInfo = &info->block[info->blockNum];
            info->sectorNum = -1;

            if (info->transferMode != LEO_SECTOR_MODE) {
                blockInfo->dramAddr = (void*) ((u32) blockInfo->dramAddr - blockInfo->sectorSize);
            }

            if (info->transferMode == LEO_TRACK_MODE && mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0) {
                messageSend = 1;
            } else {
                messageSend = 0;
            }

            osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
            __osResetGlobalIntMask(OS_IM_PI);
            __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, (info->bmCtlShadow | 0x80000000));

        readblock1:
            osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
            info = &mb->piHandle->transferInfo;
            blockInfo = &info->block[info->blockNum];

            if (blockInfo->errStatus == LEO_ERROR_29) {
                u32 stat;
                __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow | LEO_BM_CTL_RESET);
                __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow);
                __osEPiRawReadIo(mb->piHandle, LEO_STATUS, &stat);

                if (stat & LEO_STATUS_MECHANIC_INTERRUPT) {
                    __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow | LEO_BM_CTL_CLR_MECHANIC_INTR);
                }

                blockInfo->errStatus = LEO_ERROR_4;
                IO_WRITE(PI_STATUS_REG, PI_CLR_INTR);
                __osSetGlobalIntMask(OS_IM_PI | SR_IBIT4);
            }

            osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);

            if (messageSend == 1 && mb->piHandle->transferInfo.block[0].errStatus == LEO_ERROR_GOOD) {
                messageSend = 0;
                goto readblock1;
            }

            osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
            if (mb->piHandle->transferInfo.blockNum == 1) {
                osYieldThread();
            }
        } else
#endif
        {
#ifdef PORT
            /* PORT: bypass PI hardware for all DMA reads — copy from gdx_rom_buffer instead.
             * Cart ROM lives at N64 physical 0x10000000; devAddr encodes the cart physical address
             * (possibly with KSEG1 bits, which & 0x1FFFFFFF strips).  It can also be a raw ROM
             * offset from the decomp's segment symbols, so accept both cart-domain addresses and
             * raw offsets.  Zero-fill when the address is out of ROM range.  For writes, send
             * completion immediately as a no-op — we can't write to ROM on the host.
             * In all cases set ret = -1 so the if(ret==0) evtQueue wait below is skipped. */
#define GDX_PI_ROM_READ(mb_, dm_)                                                    \
    do {                                                                              \
        extern unsigned char* gdx_rom_buffer;                                        \
        extern size_t gdx_rom_size;                                                  \
        unsigned int _phys = (unsigned int)(mb_)->devAddr & 0x1FFFFFFFu;            \
        unsigned int _off  = (_phys >= 0x10000000u) ? _phys - 0x10000000u : _phys;  \
        if (gdx_rom_buffer != NULL &&                                                \
                (unsigned long long)_off + (mb_)->size <= (unsigned long long)gdx_rom_size) { \
            memcpy((mb_)->dramAddr, gdx_rom_buffer + _off, (mb_)->size);            \
        } else {                                                                     \
            memset((mb_)->dramAddr, 0, (mb_)->size);                                \
        }                                                                            \
        osSendMesg((mb_)->hdr.retQueue, (mb_), OS_MESG_NOBLOCK);                   \
        osSendMesg((dm_)->acsQueue, NULL, OS_MESG_NOBLOCK);                         \
    } while (0)
#endif
            switch (mb->hdr.type) {
                case OS_MESG_TYPE_DMAREAD:
                    osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
#ifdef PORT
                    GDX_PI_ROM_READ(mb, dm);
                    ret = -1;
#else
                    ret = dm->dma(OS_READ, mb->devAddr, mb->dramAddr, mb->size);
#endif
                    break;
                case OS_MESG_TYPE_DMAWRITE:
                    osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
#ifdef PORT
                    osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
                    osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
                    ret = -1;
#else
                    ret = dm->dma(OS_WRITE, mb->devAddr, mb->dramAddr, mb->size);
#endif
                    break;
                case OS_MESG_TYPE_EDMAREAD:
                    osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
#ifdef PORT
                    GDX_PI_ROM_READ(mb, dm);
                    ret = -1;
#else
                    ret = dm->edma(mb->piHandle, OS_READ, mb->devAddr, mb->dramAddr, mb->size);
#endif
                    break;
                case OS_MESG_TYPE_EDMAWRITE:
                    osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
#ifdef PORT
                    osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
                    osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
                    ret = -1;
#else
                    ret = dm->edma(mb->piHandle, OS_WRITE, mb->devAddr, mb->dramAddr, mb->size);
#endif
                    break;
                case OS_MESG_TYPE_LOOPBACK:
                    osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
                    ret = -1;
                    break;
                default:
                    ret = -1;
                    break;
            }

            if (ret == 0) {
                osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
                osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
                osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
            }
#ifdef PORT
#undef GDX_PI_ROM_READ
#endif
        }
    }
}
