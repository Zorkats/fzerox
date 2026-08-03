#include "global.h"
#include "leo/mfs.h"

/* The disk worker below dispatches to six functions that no header declares, so
 * each call fell back to C's implicit-int rule. Every one of them returns s32 or
 * void and the results are discarded, so nothing here miscompiled -- but the same
 * rule truncated a returned Gfx* to 32 bits in course_edit/1A2D70.c and crashed
 * "Test Course". Declaring them keeps /we4013 clean so that class cannot come back.
 *
 * These match the sys/disk definitions specifically. The superseded sys/rom twins
 * differ in parameter types (u32 vs s32 lba, size_t vs s32 sizes), which is why the
 * prototypes live here in the disk-only TU rather than in a shared header. */
s32 SLLeoReadWrite_DATA(LEOCmd* cmdBlock, s32 direction, s32 lba, void* vAddr, u32 nLbas,
                        OSMesgQueue* mq);                            /* sys_leo_dd.c */
s32 SLMFSLoad(u16 dirId, char* name, char* extension, u8* buf, s32 sizeToLoad);
s32 SLMFSLoadHalfway(u16 dirId, char* name, char* extension, u8* buf, s32 offset, s32 sizeToLoad);
void SLMFSFlushManageArea(void);
s32 func_800750B0(s32 startLba, void* vram, s32 diskSize, s32 bssSize); /* disk_drive_dd.c */
void func_xk2_800EB938(u16 dirId, char* name, char* extension, u8* buf, u32 fileSize, s32 attr,
                       s32 copyCount, bool writeChanges);            /* course_edit/19C470.c */

OSMesgQueue D_807C6E90;
volatile unk_807C6EA8 D_807C6EA8;
volatile s32 D_807C6F0C;
unk_807C6F10 D_807C6F10[8];

volatile u8 D_80794E10 = 0;
u8 D_80794E14 = 0;
u8 D_80794E18 = 0;
volatile u8 D_80794E1C = 0;
volatile u8 D_80794E20 = 0;
volatile u8 D_80794E24 = 0;
volatile s32 D_80794E28 = 0;
volatile s32 D_80794E2C = 0;

#ifdef PORT
/* PORT: transient disk prompts never appear ("now saving", "now loading", "now formatting").
 *
 * On real hardware a sender below posts a prompt id into D_807C6EA8.unk_08 (or raises the
 * unk_0C == 4 "now formatting" banner), the priority-30 worker then spends many frames talking
 * to the physical drive, and the EK prompt renderer (func_xk1_8002ED64 / func_xk1_8002F9DC,
 * overlays/expansion_kit/ABC40.c) draws that id on every one of those frames.
 *
 * This port's drive is synchronous and host-fast, and osSendMesg's wake path is a priority
 * hand-off rather than a deferral: the senders run on the priority-10 game thread, this worker
 * is priority 30 (sys/sys_main.c's osCreateThread(&sSys6Thread, ..., 30)), so osSendMesg ->
 * osStartThread takes its "__osRunningThread->priority < __osRunQueue->priority" branch
 * (libultra/os/startthread.c) and yields into this worker immediately. The worker runs the
 * ENTIRE operation and zeroes the prompt again -- func_80767940() on the unk_00 == 1/5 paths,
 * AB150.c's "D_807C6EA8.unk_08 = 0" success branch on the unk_00 == 8..18 paths -- and control
 * returns inside the sender's own osSendMesg call. The prompt is raised and cleared between two
 * draws and never rendered: the save completes correctly, the player just sees nothing. Genuine
 * errors are unaffected because they LATCH, which is why error messages still display.
 *
 * Record what was raised, once, when this worker picks the request up and before any of the
 * operation's own bookkeeping can wipe it. ABC40.c's PORT minimum-display hold consumes the
 * record on the next draw. Pure observation: nothing here waits on the reader, so the save is
 * not delayed, the game thread is not blocked and no busy flag is held any longer than before.
 *
 * Deliberately NOT cleared by func_80767940(): the unk_00 == 1/5 paths call precisely that
 * function as their completion step, so clearing the record there would erase it again before
 * any frame could be drawn. The stale-record guard lives on the consumer side instead, in
 * func_xk1_8002E9D0 (ABC40.c), which every EK screen calls from its own init. */
volatile s32 gGdxDiskPromptRaised = 0;
volatile s32 gGdxDiskBannerRaised = 0;
#endif

void func_80767800(unk_807C6F10 arg0) {
    OSIntMask prevMask;
    s32 temp_lo;

    do {
    } while (D_80794E28 == ((D_80794E2C + 1) & 7));
    prevMask = osGetIntMask();
    osSetIntMask(OS_IM_NONE);
    D_80794E2C &= 7;
    D_807C6F10[D_80794E2C] = arg0;
    temp_lo = D_80794E2C;
    D_80794E2C = (D_80794E2C + 1) & 7;
    osSendMesg(&D_807C6E90, &D_807C6F10[temp_lo], OS_MESG_BLOCK);
    osSetIntMask(prevMask);
}

void func_80767900(void) {
    if (D_80794E2C != D_80794E28) {
        D_80794E28++;
        D_80794E28 &= 7;
    }
}

void func_80767940(void) {
    D_807C6EA8.unk_04 = 0;
    D_807C6EA8.unk_08 = 0;
    D_807C6EA8.unk_0C = 0;
}

OSMesg D_807C7030[8];

#ifdef NON_MATCHING
void func_80767958(void* entry) {
    static unk_807C6F10* D_807C7050;
    static OSMesgQueue D_807C7058;
    static OSMesg D_807C7070;
    static unk_807C6F10* D_807C7074;
    static OSMesg D_807C7078;
    static OSMesgQueue* D_807C707C;
    OSIoMesg* temp_v0_2;
    s32 temp_v0;
    s32 temp_v0_3;
    unk_807C6F10* temp_s0;
    unk_807C6F10* temp_v1;

    osCreateMesgQueue(&D_807C6E90, D_807C7030, ARRAY_COUNT(D_807C7030));
    osCreateMesgQueue(&D_807C7058, &D_807C7070, 1);
    func_80767940();

    while (true) {
        osRecvMesg(&D_807C6E90, &D_807C7050, 1);
        temp_v1 = D_807C7050;
        D_80794E14 = 1;
        if (temp_v1 != NULL) {
            temp_s0 = D_807C7050;
            func_80767900();
            switch (temp_s0->unk_00) {
                case 0:
                    D_807C7074 = temp_s0;
                    SLLeoReadWrite(D_807C7074->cmdBlock, D_807C7074->direction, D_807C7074->lba, D_807C7074->vAddr,
                                   D_807C7074->nLBAs, &D_807C7058);
                    osRecvMesg(&D_807C7058, &D_807C7078, OS_MESG_BLOCK);
                    osSendMesg(D_807C7074->mq, D_807C7078, OS_MESG_BLOCK);
                    break;
                case 1:
                    D_807C7074 = temp_s0;
                    SLLeoReadWrite_DATA(D_807C7074->cmdBlock, D_807C7074->direction, D_807C7074->lba, D_807C7074->vAddr,
                                        D_807C7074->nLBAs, &D_807C7058);
                    osRecvMesg(&D_807C7058, &D_807C7078, OS_MESG_BLOCK);
                    osSendMesg(D_807C7074->mq, D_807C7078, OS_MESG_BLOCK);
                    break;
                case 2:
                case 3:
                    D_807C7074 = temp_s0;
                    temp_v0_2 = D_807C7074->ioMesg;
                    D_807C707C = temp_v0_2->hdr.retQueue;
                    temp_v0_2->hdr.retQueue = &D_807C7058;
                    osInvalDCache(osPhysicalToVirtual(D_807C7074->ioMesg->dramAddr), D_807C7074->ioMesg->size);
                    osEPiStartDma(D_807C7074->piHandle, D_807C7074->ioMesg, D_807C7074->direction);
                    osRecvMesg(&D_807C7058, NULL, OS_MESG_BLOCK);
                    osSendMesg(D_807C707C, NULL, OS_MESG_BLOCK);
                    break;
                default:
                    D_807C7074 = temp_s0;
                    break;
            }
            D_80794E14 = 0;
            continue;
        }
        D_80794E18 = 1;

#ifdef PORT
        /* Snapshot point for the transient-prompt hold (see gGdxDiskPromptRaised above). Placed
           here, after the ring-buffer entries have already been handled and `continue`d out, so
           it sees exactly the file-management class of requests -- the ones whose prompt every
           branch below is about to clear -- and it runs before any of that clearing.
           Prompt id 10 is skipped on purpose: ABC40.c's draw switch has no case 10, which makes
           it the "run this operation silently" id, and holding it would occupy the prompt slot
           for half a second with nothing to show. */
        if ((D_807C6EA8.unk_08 != 0) && (D_807C6EA8.unk_08 != 10)) {
            gGdxDiskPromptRaised = D_807C6EA8.unk_08;
        }
        if (D_807C6EA8.unk_0C == 4) {
            gGdxDiskBannerRaised = 4;
        }
#endif

        if (D_807C6EA8.unk_00 == 4) {
            SLMFSSave(D_807C6EA8.dirId, D_807C6EA8.name, D_807C6EA8.extension, D_807C6EA8.readBuf, D_807C6EA8.fileSize,
                      D_807C6EA8.attr, D_807C6EA8.copyCount, D_807C6EA8.writeChanges);
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 5) {
            func_xk2_800EB938(D_807C6EA8.dirId, D_807C6EA8.name, D_807C6EA8.extension, D_807C6EA8.readBuf,
                              D_807C6EA8.fileSize, D_807C6EA8.attr, D_807C6EA8.copyCount, D_807C6EA8.writeChanges);
            func_80767940();
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 6) {
            SLMFSLoad(D_807C6EA8.dirId, D_807C6EA8.name, D_807C6EA8.extension, D_807C6EA8.writeBuf,
                      D_807C6EA8.fileSize);
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 7) {
            SLMFSLoadHalfway(D_807C6EA8.dirId, D_807C6EA8.name, D_807C6EA8.extension, D_807C6EA8.writeBuf,
                             D_807C6EA8.offset, D_807C6EA8.fileSize);
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 0) {
            func_800750B0(D_807C6EA8.startLba, D_807C6EA8.lbaBuf, D_807C6EA8.fileSize, D_807C6EA8.bssSize);
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 1) {
            func_80706518(D_807C6EA8.copyCount, D_807C6EA8.unk_54, D_807C6EA8.extension);
            func_80767940();
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 2) {
            SLMFSFlushManageArea();
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_00 == 3) {
            SLMFSNewDisk();
            D_80794E14 = D_80794E18 = 0;
            continue;
        }
        if (D_807C6EA8.unk_10 == 4) {

        } else {
            D_807C6EA8.unk_10 = 1;
        }

        temp_v0_3 = func_xk1_8002E368();
        switch (temp_v0_3) {
            case LEO_ERROR_GOOD:
                D_807C6EA8.unk_10 = 0;
                D_80794E14 = 0;
                D_80794E18 = 0;
                break;
            case LEO_ERROR_DIAGNOSTIC_FAILURE:
                D_807C6EA8.unk_10 = 2;
                break;
            case LEO_ERROR_COMMAND_PHASE_ERROR:
                D_807C6EA8.unk_10 = 3;
                break;
            case LEO_ERROR_DATA_PHASE_ERROR:
                D_807C6EA8.unk_10 = 4;
                D_807C6EA8.unk_08 = 3;
                break;
            case LEO_ERROR_COMMAND_TERMINATED:
                func_8070F8A4(temp_v0_3, 0);
                while (true) {}
            default:
                break;
        }

#ifdef PORT
        /* PORT fix: stuck disk-busy flag blanking EK kanji text and course-edit MFS ops.
         *
         * func_xk1_8002E368 (overlays/expansion_kit/AB150.c) is a multi-STEP retry state
         * machine for the EK MFS file-MANAGEMENT ops routed here (D_807C6EA8.unk_00 8-18:
         * create/rename/delete/get-or-set-attr/exists-check). Unlike the SL* wrappers used by
         * unk_00 4-7 (sys/disk/sys_leo_dd.c), which retry INTERNALLY until they have a final
         * result, it advances ONE step per call and, on anything other than
         * LEO_ERROR_GOOD/LEO_ERROR_COMMAND_TERMINATED, expects to be invoked again later
         * (see its D_807C6EA8.unk_10 == 4 resume branch). On real hardware that follow-up call
         * is driven by a later 64DD interrupt re-posting to this thread's own queue
         * (D_807C6E90) as the drive's physical state changes. This port's disk backing
         * (port/n64_leo.c) is fully synchronous -- there is no later interrupt -- so without
         * this fix the outer loop falls straight back to a blocking osRecvMesg,
         * D_80794E14/D_80794E18 stay latched at 1 forever, func_80767E30's busy gate silently
         * drops every later MFS send, and every reader gated on D_80794E14 (A2E90.c's kanji
         * glyph renderer, Course Edit's input/update gates in 19DD60.c/188850.c) goes blank or
         * unresponsive permanently. unk_00 == 4 (SLMFSSave) is unaffected: it never reaches
         * this state machine.
         *
         * The underlying Mfs_ and LeoReadWrite calls are synchronous and deterministic here,
         * so re-invoke the state machine immediately instead of waiting for an event that will
         * never arrive. Bounded so a genuinely unrecoverable condition falls through -- forcing
         * the busy flags clear -- rather than spinning; each iteration is a handful of
         * synchronous C calls.
         *
         * ONE ERROR IS EXCLUDED: LEO_ERROR_DATA_PHASE_ERROR (the unk_10 = 4 / unk_08 = 3 case
         * set by the switch above). That is not a transient the drive can resolve by being
         * asked again -- it is a deliberate MULTI-FRAME HAND-OFF TO THE PLAYER, reached only
         * from func_xk1_8002E368's N64DD_READ_ONLY_MEDIA branch. unk_08 = 3 is the "insert
         * writable media" prompt ABC40.c draws, and unk_10 = 4 is the resume token
         * func_xk1_8002E368 consumes to pick the operation back up after the swap. Re-driving
         * it collapses that conversation into 64 same-frame retries against a medium the player
         * has had no opportunity to change, and the fail-safe below then destroys the resume
         * token while the prompt is still on screen.
         *
         * Excluding it restores retail state exactly: unk_10 = 4, unk_08 = 3, D_80794E14 and
         * D_80794E18 left latched at 1 (the operation genuinely IS still in flight, and the
         * busy gate is what stops a second op being queued on top of it). The re-drive then
         * comes from where it comes from on hardware: the unk_10 == 2/3/4 osSendMesg at the top
         * of func_xk1_8002ED64 (ABC40.c), once per drawn frame. That draw path runs while
         * gInCourseEditor is set, which covers Course Edit and Machine Create -- the only two
         * modes any sender that can reach this state machine is called from. The residual hole
         * is a mode exit racing an in-flight unk_10 == 4, which also clears gInCourseEditor;
         * that race needs a disk image whose MFS RAM area has zero capacity (leo/mfs/mfs_ram.c),
         * which a well-formed Expansion Kit image does not have. */
        if (temp_v0_3 != LEO_ERROR_GOOD && temp_v0_3 != LEO_ERROR_COMMAND_TERMINATED &&
            temp_v0_3 != LEO_ERROR_DATA_PHASE_ERROR) {
            s32 portRetries;
            for (portRetries = 0; portRetries < 64; portRetries++) {
                temp_v0_3 = func_xk1_8002E368();
                if (temp_v0_3 == LEO_ERROR_GOOD) {
                    D_807C6EA8.unk_10 = 0;
                    D_80794E14 = 0;
                    D_80794E18 = 0;
                    break;
                }
                if (temp_v0_3 == LEO_ERROR_COMMAND_TERMINATED) {
                    func_8070F8A4(temp_v0_3, 0);
                    while (true) {}
                }
                /* Mirror the state update the console switch above performs so the
                   unk_10 == 4 (read-only-media resume) branch inside
                   func_xk1_8002E368 still sees the state it expects next iteration. */
                switch (temp_v0_3) {
                    case LEO_ERROR_DIAGNOSTIC_FAILURE:
                        D_807C6EA8.unk_10 = 2;
                        break;
                    case LEO_ERROR_COMMAND_PHASE_ERROR:
                        D_807C6EA8.unk_10 = 3;
                        break;
                    case LEO_ERROR_DATA_PHASE_ERROR:
                        D_807C6EA8.unk_10 = 4;
                        D_807C6EA8.unk_08 = 3;
                        break;
                    default:
                        break;
                }
                if (temp_v0_3 == LEO_ERROR_DATA_PHASE_ERROR) {
                    /* Same exclusion as the entry condition above, for the case where an
                       EARLIER error retried its way into the media-swap hand-off: stop here
                       with retail's state standing (unk_10 = 4, unk_08 = 3, busy flags still
                       latched) and let the draw path re-post drive the resume. Breaking out
                       rather than falling to the loop bound also keeps the fail-safe below
                       from force-clearing the resume token. */
                    break;
                }
            }
            /* The DATA_PHASE_ERROR exclusion applies to the fail-safe too: force-clearing here
               is exactly the destructive step the exclusion exists to prevent. Every other
               unresolved error still gets the busy-flag rescue. */
            if (temp_v0_3 != LEO_ERROR_GOOD && temp_v0_3 != LEO_ERROR_DATA_PHASE_ERROR) {
                /* Fail-safe, not fail-stuck: force the busy flags clear so the UI/kanji
                   renderer recovers even from a condition this bounded retry could not
                   resolve. Bounded, rate-limited log so a genuinely stuck case is still
                   diagnosable without spamming every frame. */
                extern void gdx_cki(const char* s, int v);
                static int sPortMfsMgmtStuckLogs = 0;
                if (sPortMfsMgmtStuckLogs < 8) {
                    sPortMfsMgmtStuckLogs++;
                    gdx_cki("[disk] WARNING: EK MFS-mgmt op did not converge after PORT retry; "
                            "forcing busy-flag clear, last error", (int) temp_v0_3);
                }
                D_807C6EA8.unk_10 = 0;
                D_80794E14 = 0;
                D_80794E18 = 0;
            }
        }
#endif
    }
}
#else
unk_807C6F10* D_807C7050;
OSMesgQueue D_807C7058;
OSMesg D_807C7070;
unk_807C6F10* D_807C7074;
OSMesg D_807C7078;
OSMesgQueue* D_807C707C;
#pragma GLOBAL_ASM("asm/jp/ek/nonmatchings/sys/disk/75000/func_80767958.s")
#endif

s32 func_80767E30(void) {
    if (D_80794E18 == 0) {
        return 0;
    }
    switch (D_807C6EA8.unk_08) {
        case 0:
            return 0;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 14:
        case 15:
            D_807C6EA8.unk_08 = 9;
            return 1;
        default:
            return 1;
    }
}

void func_80767E98(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {

    D_807C6EA8.dirId = dirId;
    D_807C6EA8.name = name;
    D_807C6EA8.extension = extension;
    D_807C6EA8.readBuf = buf;
    D_807C6EA8.fileSize = fileSize;
    D_807C6EA8.attr = attr;
    D_807C6EA8.copyCount = copyCount;
    D_807C6EA8.writeChanges = writeChanges;
    osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
}

void func_80767F14(u16 dirId, char* name, char* extension, void* buf, s32 fileSize) {
    D_807C6EA8.dirId = dirId;
    D_807C6EA8.name = name;
    D_807C6EA8.extension = extension;
    D_807C6EA8.writeBuf = buf;
    D_807C6EA8.fileSize = fileSize;
    osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
}

void func_80767F78(s32 startLba, s32 lbaBuf, s32 fileSize, s32 bssSize) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 0;
        D_807C6EA8.startLba = startLba;
        D_807C6EA8.lbaBuf = lbaBuf;
        D_807C6EA8.fileSize = fileSize;
        D_807C6EA8.bssSize = bssSize;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_80767FE4(s32 copyCount, u8 arg1, char* extension) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 1;
        D_807C6EA8.unk_04 = 0;
        D_807C6EA8.unk_08 = 0;
        D_807C6EA8.unk_0C = 4;
        D_807C6EA8.copyCount = copyCount;
        D_807C6EA8.unk_54 = arg1;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_8076805C(void) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 2;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_807680A4(void) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 3;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_807680EC(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 4;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_8076814C(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 5;
        D_807C6EA8.unk_08 = 6;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_807681C8(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 5;
        D_807C6EA8.unk_08 = 7;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_80768244(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 8;
        D_807C6EA8.unk_08 = 6;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_807682C0(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 8;
        D_807C6EA8.unk_08 = 10;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_8076833C(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 8;
        D_807C6EA8.unk_08 = 25;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_807683B8(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 8;
        D_807C6EA8.unk_08 = 26;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_80768434(u16 dirId, char* name, char* extension, void* buf, s32 fileSize, s32 attr, s32 copyCount,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 8;
        D_807C6EA8.unk_08 = 8;
        func_80767E98(dirId, name, extension, buf, fileSize, attr, copyCount, writeChanges);
    }
}

void func_807684AC(u16 dirId, char* name, char* extension, void* buf, s32 offset, s32 fileSize) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 7;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        D_807C6EA8.writeBuf = buf;
        D_807C6EA8.offset = offset;
        D_807C6EA8.fileSize = fileSize;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_8076852C(u16 dirId, char* name, char* extension, void* buf, s32 fileSize) {
    if (D_80794E18 == 0) {
        D_807C6EA8.unk_00 = 6;
        func_80767F14(dirId, name, extension, buf, fileSize);
    }
}

void func_80768574(u16 dirId, char* name, char* extension, void* buf, s32 fileSize) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 9;
        D_807C6EA8.unk_08 = 5;
        func_80767F14(dirId, name, extension, buf, fileSize);
    }
}

void func_807685D8(u16 dirId, char* name, char* extension, void* buf, s32 fileSize) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 10;
        D_807C6EA8.unk_08 = 10;
        func_80767F14(dirId, name, extension, buf, fileSize);
    }
}

void func_80768638(u16 dirId, char* name, char* extension, void* buf, s32 fileSize) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 9;
        D_807C6EA8.unk_08 = 10;
        func_80767F14(dirId, name, extension, buf, fileSize);
    }
}

void func_8076869C(u16 dirId, char* name, char* extension) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 13;
        D_807C6EA8.unk_08 = 10;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_8076870C(u16 dirId, char* name, char* extension) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 14;
        D_807C6EA8.unk_08 = 10;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_8076877C(u8 arg0, char* extension) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 11;
        D_807C6EA8.unk_08 = 10;
        D_807C6EA8.unk_54 = arg0;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_807687E0(u8 arg0, char* extension) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 12;
        D_807C6EA8.unk_08 = 10;
        D_807C6EA8.unk_54 = arg0;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_80768844(u16 dirId, char* oldName, char* oldExtension, char* newName, char* newExtension, bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 15;
        D_807C6EA8.unk_08 = 11;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.oldName = oldName;
        D_807C6EA8.oldExtension = oldExtension;
        D_807C6EA8.newName = newName;
        D_807C6EA8.newExtension = newExtension;
        D_807C6EA8.writeChanges = writeChanges;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_807688D0(u16 dirId, char* name, char* extension, bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 16;
        D_807C6EA8.unk_08 = 12;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        D_807C6EA8.writeChanges = writeChanges;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_8076894C(u16 dirId, char* name, char* extension) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 17;
        D_807C6EA8.unk_08 = 10;
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

void func_807689BC(u16 dirId, char* name, char* extension, s32 attributeToAdd, s32 attributeToRemove,
                   bool writeChanges) {
    if (func_80767E30() == 0) {
        D_807C6EA8.unk_00 = 18;
        if (attributeToAdd == MFS_FILE_ATTR_FORBID_W) {
            D_807C6EA8.unk_08 = 14;
        } else {
            D_807C6EA8.unk_08 = 15;
        }
        D_807C6EA8.dirId = dirId;
        D_807C6EA8.name = name;
        D_807C6EA8.extension = extension;
        D_807C6EA8.attributeToAdd = attributeToAdd;
        D_807C6EA8.attributeToRemove = attributeToRemove;
        D_807C6EA8.writeChanges = writeChanges;
        osSendMesg(&D_807C6E90, NULL, OS_MESG_BLOCK);
    }
}

s32 func_80768A5C(LEOCmd* cmdBlock, s32 direction, u32 lba, void* vAddr, u32 nLbas, OSMesgQueue* mq) {
    unk_807C6F10 sp34;

    sp34.unk_00 = 0;
    sp34.cmdBlock = cmdBlock;
    sp34.direction = direction;
    sp34.lba = lba;
    sp34.vAddr = vAddr;
    sp34.nLBAs = nLbas;
    sp34.mq = mq;

    func_80767800(sp34);
    return 0;
}

s32 func_80768AF0(LEOCmd* cmdBlock, s32 direction, u32 lba, void* vAddr, u32 nLbas, OSMesgQueue* mq) {
    unk_807C6F10 sp34;

    sp34.unk_00 = 1;
    sp34.cmdBlock = cmdBlock;
    sp34.direction = direction;
    sp34.lba = lba;
    sp34.vAddr = vAddr;
    sp34.nLBAs = nLbas;
    sp34.mq = mq;

    func_80767800(sp34);
    return 0;
}

s32 func_80768B88(OSPiHandle* piHandle, OSIoMesg* mb, s32 direction) {
    unk_807C6F10 sp34;

    sp34.unk_00 = 2;
    sp34.piHandle = piHandle;
    sp34.ioMesg = mb;
    sp34.direction = direction;

    func_80767800(sp34);
    return 0;
}

s32 func_80768C08(OSPiHandle* piHandle, OSIoMesg* mb, s32 direction) {
    unk_807C6F10 sp34;

    sp34.unk_00 = 3;
    sp34.piHandle = piHandle;
    sp34.ioMesg = mb;
    sp34.direction = direction;

    func_80767800(sp34);
    return 0;
}

extern s32 gMfsError;

bool func_80768C88(u16 dirId, char* name, char* extension) {
    if (Mfs_GetFilesPreparation(dirId) == -1) {
        return false;
    }
    while (Mfs_GetNextFileInPreparedDir() != MFS_ENTRY_DOES_NOT_EXIST) {}

    if (Mfs_GetFileIndex(dirId, name, extension) == MFS_ENTRY_DOES_NOT_EXIST) {
        if (gMfsError == N64DD_NOT_FOUND) {
            gMfsError = LEO_ERROR_GOOD;
        }
        return false;
    }
    return true;
}

extern LEODiskID D_800CD2B0;

void func_80768D30(void) {
    static LEODiskID D_807C7080;

    SLLeoReadDiskID(&D_800CD2B0);
    if (SLLeoDiskCompare(D_807C7080, D_800CD2B0)) {
        SLLeo_mfs_newdisk();
        D_807C7080 = D_800CD2B0;
    }

    PRINTF("[ERROR] RING-BUFFER EMPTY\n");
}
