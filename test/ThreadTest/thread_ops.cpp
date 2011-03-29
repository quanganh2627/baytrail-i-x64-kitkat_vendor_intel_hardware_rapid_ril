#include "thread_ops.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>

void * ThreadProcStartStop(void * pVoid)
{
    void * pvRet = NULL;

    if (NULL == pVoid)
    {
        printf("ThreadProcStartStop() : ERROR : pVoid was NULL!\r\n");
        goto Error;
    }

    {
        THREAD_DATA * pThreadData = (THREAD_DATA*)pVoid;

        THREAD_PROC_PTR pvThreadProc = pThreadData->pvThreadProc;
        void * pvDataObj = pThreadData->pvDataObj;
        BOOL * pfRunning = pThreadData->pfRunning;

        delete pThreadData;

        *pfRunning = TRUE;
        pvRet = pvThreadProc(pvDataObj);
        *pfRunning = FALSE;
    }

Error:
    pthread_exit(pvRet);
    return NULL;
}

void * ThreadWaitProc(void * pVoid)
{
    void ** ppvRetVal = NULL;

    if (NULL == pVoid)
    {
        printf("ThreadWaitProc() : ERROR : pVoid was NULL!\r\n");
        return NULL;
    }

    THREAD_WAIT_DATA * pThreadWaitData = (THREAD_WAIT_DATA*)pVoid;

    pthread_t thread = pThreadWaitData->thread;
    CEvent * pEvent = pThreadWaitData->pEvent;

    delete pThreadWaitData;

    if (THREAD_UNINITIALIZED == thread)
    {
        printf("ThreadWaitProc() : ERROR : Given thread is not initialized!\r\n");
        return NULL;
    }

    int nRet = pthread_join(thread, ppvRetVal);

    if ((NULL != pEvent) && (0 == nRet))
    {
        pEvent->Signal();
    }

    return NULL;
}

CThread::CThread(THREAD_PROC_PTR pvThreadProc, void * pvDataObj, UINT32 dwFlags, UINT32 dwStackSize) :
    m_pvDataObj(pvDataObj),
    m_uiPriority(THREAD_PRIORITY_LEVEL_UNKNOWN),
    m_thread(THREAD_UNINITIALIZED),
    m_fRunning(FALSE)
{
    if (dwFlags & THREAD_FLAGS_START_SUSPENDED)
    {
        printf("CThread::CThread() : WARN : We don't support start from suspended at this time\r\n");
    }

    if (0 != dwStackSize)
    {
        printf("CThread::CThread() : WARN : We don't support stack size parameter at this time\r\n");
    }

    int iResult = 0;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    THREAD_DATA* pThreadData = new THREAD_DATA;
    pThreadData->pvDataObj = pvDataObj;
    pThreadData->pvThreadProc = pvThreadProc;
    pThreadData->pfRunning = &m_fRunning;

    if (THREAD_FLAGS_JOINABLE & dwFlags)
    {
        m_fJoinable = TRUE;
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    }
    else
    {
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }

    iResult = pthread_create(&m_thread, &attr, ThreadProcStartStop, pThreadData);

    pthread_attr_destroy(&attr);

    if (iResult != 0)
    {
        errno = iResult;
        perror("pthread_create");

        printf("CThread::CThread() : ERROR : Failed to create thread!\r\n");
        m_thread = THREAD_UNINITIALIZED;
    }

    if (THREAD_UNINITIALIZED != m_thread)
    {
        SetPriority(THREAD_PRIORITY_LEVEL_NORMAL);
    }
}

CThread::~CThread()
{
    if (m_fRunning)
    {
        printf("CThread::~CThread() : WARN : Attempting to delete thread while still running\r\n");
        pthread_cancel(m_thread);
    }
}

BOOL CThread::SetPriority(UINT32 dwPriority)
{
    BOOL fRet = FALSE;

    if (THREAD_UNINITIALIZED != m_thread)
    {
        if (m_uiPriority == dwPriority)
        {
            fRet = TRUE;
        }
        else if (THREAD_PRIORITY_LEVEL_HIGHEST >= dwPriority)
        {
            struct sched_param sc;
            int MinPriority = sched_get_priority_min(SCHED_RR);
            int MaxPriority = sched_get_priority_max(SCHED_RR);
            int NormalPriority = (MaxPriority - MinPriority) / 2;
            int Delta = (MaxPriority - MinPriority) / 4;

            //printf("MIN: %d     MAX: %d     NORMAL: %d\r\n", MinPriority, MaxPriority, NormalPriority);

            switch (dwPriority)
            {
                case THREAD_PRIORITY_LEVEL_LOWEST:
                    sc.sched_priority = NormalPriority - (Delta*2);
                    m_uiPriority = THREAD_PRIORITY_LEVEL_LOWEST;
                    break;

                case THREAD_PRIORITY_LEVEL_LOW:
                    sc.sched_priority = NormalPriority - (Delta*1);
                    m_uiPriority = THREAD_PRIORITY_LEVEL_LOW;
                    break;

                case THREAD_PRIORITY_LEVEL_NORMAL:
                    sc.sched_priority = NormalPriority;
                    m_uiPriority = THREAD_PRIORITY_LEVEL_NORMAL;
                    break;

                case THREAD_PRIORITY_LEVEL_HIGH:
                    sc.sched_priority = NormalPriority + (Delta*1);
                    m_uiPriority = THREAD_PRIORITY_LEVEL_HIGH;
                    break;

                case THREAD_PRIORITY_LEVEL_HIGHEST:
                    sc.sched_priority = NormalPriority + (Delta*2);
                    m_uiPriority = THREAD_PRIORITY_LEVEL_HIGHEST;
                    break;

                default:
                    sc.sched_priority = NormalPriority;
                    m_uiPriority = THREAD_PRIORITY_LEVEL_NORMAL;
                    break;
            }

            if (0 == pthread_setschedparam(m_thread, SCHED_RR, &sc))
            {
                fRet = TRUE;
            }
            else
            {
                fRet = FALSE;
                perror("pthread_setschedparam");
                printf("CThread::SetPriority() : ERROR : pthread_setschedparam returned failed response\r\n");
            }
        }
        else
        {
            printf("CThread::SetPriority() : ERROR : Given priority out of range: %d\r\n", dwPriority);
        }
    }
    else
    {
        printf("CThread::SetPriority() : ERROR : Thread is not initialized!\r\n");
    }

    return fRet;
}

UINT32 CThread::GetPriority()
{
    return m_uiPriority;
}

UINT32 CThread::WaitForThread(UINT32 dwTimeout)
{
    THREAD_WAIT_DATA* pThreadWaitData = NULL;
    UINT32 dwRet = THREAD_WAIT_GEN_FAILURE;

    if (!m_fJoinable)
    {
        printf("CThread::WaitForThread() : ERROR : Unable to wait for non-joinable thread!\r\n");
        return THREAD_WAIT_NOT_JOINABLE;
    }

    int iResult = 0;
    pthread_t helperThread = 0;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    CEvent * pEvent = new CEvent();
    if (NULL == pEvent)
    {
        printf("CThread::WaitForThread() : ERROR : Failed to create event!\r\n");
        goto Error;
    }

    pThreadWaitData = new THREAD_WAIT_DATA;
    pThreadWaitData->thread = m_thread;
    pThreadWaitData->pEvent = pEvent;

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    iResult = pthread_create(&helperThread, &attr, ThreadWaitProc, pThreadWaitData);

    pthread_attr_destroy(&attr);

    if (iResult != 0)
    {
        errno = iResult;
        perror("pthread_create");

        printf("CThread::WaitForThread() : ERROR : Failed to create thread!\r\n");
    }

    switch(pEvent->Wait(dwTimeout))
    {
        case WAIT_EVENT_0_SIGNALED:
        {
            dwRet = THREAD_WAIT_0;
            break;
        }

        case WAIT_TIMEDOUT:
        default:
        {
            dwRet = THREAD_WAIT_TIMEOUT;

            // Kill our helper thread
            pthread_cancel(helperThread);
            break;
        }
    }

Error:
    delete pEvent;
    return dwRet;
}
