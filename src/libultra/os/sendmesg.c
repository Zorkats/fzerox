#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/osint.h"

#ifdef PORT
/* port/n64_sched.c: cross-OS-thread message-queue guard. __osDisableInt is a no-op on the port
   and the dedicated audio OS thread calls this, so the queue needs a real lock; the wake path
   mutates the host-thread-affine run queue and must be deferred when off the host thread. */
extern void gdx_mq_lock(void);
extern void gdx_mq_unlock(void);
extern int gdx_sched_on_host_thread(void);
extern void gdx_sched_defer_wake(OSThread** waitList);
#endif

s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
    register u32 saveMask;
    register s32 last;

#ifdef _DEBUG
    if ((flags != OS_MESG_NOBLOCK) && (flags != OS_MESG_BLOCK)) {
        __osError(ERR_OSSENDMESG, 1, flags);
        return -1;
    }
#endif

    saveMask = __osDisableInt();

#ifdef PORT
    gdx_mq_lock();

    while (MQ_IS_FULL(mq)) {
        gdx_mq_unlock();
        if (flags == OS_MESG_BLOCK) {
            /* Never hold the lock across a fiber switch. From a non-host thread
               __osEnqueueAndYield spin-yields without touching scheduler state, so the
               running-thread state write must be skipped there too. */
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

    last = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[last] = msg;
    mq->validCount++;

    if (!gdx_sched_on_host_thread()) {
        /* Unconditional (not gated on a visible waiter): closes the lost-wakeup window where
           the consumer checks empty before this send but parks itself after it. */
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
    while (MQ_IS_FULL(mq)) {
        if (flags == OS_MESG_BLOCK) {
            __osRunningThread->state = OS_STATE_WAITING;
            __osEnqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(saveMask);
            return -1;
        }
    }

    last = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[last] = msg;
    mq->validCount++;

    if (mq->mtqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->mtqueue));
    }

    __osRestoreInt(saveMask);
    return 0;
#endif
}
