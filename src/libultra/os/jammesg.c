#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/osint.h"

#ifdef PORT
/* port/n64_sched.c: cross-OS-thread message-queue guard (see sendmesg.c / n64_sched.c). */
extern void gdx_mq_lock(void);
extern void gdx_mq_unlock(void);
extern int gdx_sched_on_host_thread(void);
extern void gdx_sched_defer_wake(OSThread** waitList);
#endif

s32 osJamMesg(OSMesgQueue* mq, OSMesg msg, s32 flag) {
    register u32 saveMask;

#ifdef _DEBUG
    if ((flag != OS_MESG_NOBLOCK) && (flag != OS_MESG_BLOCK)) {
        __osError(ERR_OSJAMMESG, 1, flag);
        return -1;
    }
#endif

    saveMask = __osDisableInt();

#ifdef PORT
    gdx_mq_lock();

    while (mq->validCount >= mq->msgCount) {
        gdx_mq_unlock();
        if (flag == OS_MESG_BLOCK) {
            /* Never hold the lock across a fiber switch (see sendmesg.c). */
            if (gdx_sched_on_host_thread()) {
                __osRunningThread->state = OS_STATE_WAITING;
            }
            __osEnqueueAndYield(&mq->fullqueue);
            gdx_mq_lock();
        } else {
            __osRestoreInt(saveMask);
            return -1;
        }
    }

    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;

    if (!gdx_sched_on_host_thread()) {
        gdx_sched_defer_wake(&mq->mtqueue);
        gdx_mq_unlock();
    } else if (mq->mtqueue->next != NULL) {
        OSThread* waiter = __osPopThread(&mq->mtqueue);
        gdx_mq_unlock();
        osStartThread(waiter);
    } else {
        gdx_mq_unlock();
    }

    __osRestoreInt(saveMask);
    return 0;
#else
    while (mq->validCount >= mq->msgCount) {
        if (flag == OS_MESG_BLOCK) {
            __osRunningThread->state = OS_STATE_WAITING;
            __osEnqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(saveMask);
            return -1;
        }
    }

    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;

    if (mq->mtqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->mtqueue));
    }

    __osRestoreInt(saveMask);
    return 0;
#endif
}
