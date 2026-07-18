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

s32 osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags) {
    register u32 saveMask;

#ifdef _DEBUG
    if ((flags != OS_MESG_NOBLOCK) && (flags != OS_MESG_BLOCK)) {
        __osError(ERR_OSRECVMESG, 1, flags);
        return -1;
    }
#endif

    saveMask = __osDisableInt();

#ifdef PORT
    gdx_mq_lock();

    while (MQ_IS_EMPTY(mq)) {
        gdx_mq_unlock();
        if (flags == OS_MESG_NOBLOCK) {
            __osRestoreInt(saveMask);
            return -1;
        }
        /* Never hold the lock across a fiber switch (see sendmesg.c). */
        if (gdx_sched_on_host_thread()) {
            __osRunningThread->state = OS_STATE_WAITING;
        }
        __osEnqueueAndYield(&mq->mtqueue);
        gdx_mq_lock();
    }

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    if (!gdx_sched_on_host_thread()) {
        gdx_sched_defer_wake(&mq->fullqueue);
        gdx_mq_unlock();
    } else if (mq->fullqueue->next != NULL) {
        OSThread* waiter = __osPopThread(&mq->fullqueue);
        gdx_mq_unlock();
        osStartThread(waiter);
    } else {
        gdx_mq_unlock();
    }

    __osRestoreInt(saveMask);
    return 0;
#else
    while (MQ_IS_EMPTY(mq)) {
        if (flags == OS_MESG_NOBLOCK) {
            __osRestoreInt(saveMask);
            return -1;
        } else {
            __osRunningThread->state = OS_STATE_WAITING;
            __osEnqueueAndYield(&mq->mtqueue);
        }
    }

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    if (mq->fullqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->fullqueue));
    }

    __osRestoreInt(saveMask);
    return 0;
#endif
}
