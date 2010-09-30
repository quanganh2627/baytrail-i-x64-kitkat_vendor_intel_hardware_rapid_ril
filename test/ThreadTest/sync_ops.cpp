#include "sync_ops.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

CMutex::CMutex()
{
    pthread_mutexattr_t _attr;

    pthread_mutexattr_init(&_attr);
    pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE_NP);

    pthread_mutex_init(&m_mutex, &_attr);
    pthread_mutexattr_destroy(&_attr);
}

CMutex::~CMutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void CMutex::EnterMutex()
{
    int rc = pthread_mutex_lock(&m_mutex);

    if (rc)
        perror("CMutex::EnterMutex");
}

void CMutex::LeaveMutex()
{
    int rc = pthread_mutex_unlock(&m_mutex);

    if (rc)
        perror("CMutex::LeaveMutex");
}

// Use to convert our timeout to a timespec in the future
timespec msFromNow(UINT32 msInFuture)
{
    timespec FutureTime;
    timeval Now;
    UINT32 usFromNow;

    gettimeofday(&Now, NULL);

    usFromNow = ((msInFuture % 1000) * 1000) + Now.tv_usec;

    if (usFromNow >= 1000000 )
    {
        Now.tv_sec++;
        FutureTime.tv_sec = Now.tv_sec + (msInFuture / 1000);
        FutureTime.tv_nsec = (usFromNow - 1000000) * 1000;
    }
    else
    {
        FutureTime.tv_nsec = usFromNow * 1000;
        FutureTime.tv_sec = Now.tv_sec + (msInFuture / 1000);
    }

    return FutureTime;
}

CEvent::CEvent(const char * szName, BOOL fManual) : CMutex()
{
    EnterMutex();

    pthread_cond_init(&m_EventCond, NULL);

    m_fSignaled = FALSE;
    m_fManual   = fManual;
    m_szName    = NULL;

    if (szName)
    {
        // TODO Add support for named events if required
        m_szName = strdup(szName);
    }

    LeaveMutex();
}

CEvent::~CEvent()
{
    pthread_cond_destroy(&m_EventCond);
    if (m_szName)
        free((void*)m_szName);
}

void CEvent::CreateObserver(CMutipleEvent* pMultipleEvent, int iValue)
{
    EnterMutex();

    CObserver* pNewObserver = new CObserver(pMultipleEvent, iValue);
    m_mObservers.push_front(pNewObserver);

    LeaveMutex();
}

void CEvent::DeleteObserver(CMutipleEvent* pMultipleEvent)
{
    EnterMutex();

    for (mapObserverIterator it = m_mObservers.begin(); it != m_mObservers.end(); it++)
    {
        CObserver* pObserver = *it;
        if (pObserver && (pObserver->m_pMultipleEvent == pMultipleEvent))
        {
            m_mObservers.erase(it);
            delete pObserver;
            break;
        }
    }

    LeaveMutex();
}

BOOL CEvent::SignalMultipleEventObject()
{
    BOOL fEventHandled = FALSE;

    for (mapObserverIterator it = m_mObservers.begin(); it != m_mObservers.end(); it++)
    {
        CObserver* pObserver = *it;
        if (pObserver)
        {
            int iEventIndex = pObserver->m_iEventIndex;
            CMutipleEvent* pMultipleEvent = pObserver->m_pMultipleEvent;

            fEventHandled = pMultipleEvent->Update(iEventIndex);
        }
    }

    return fEventHandled;
}

int CEvent::Signal(void)
{
    EnterMutex();

    pthread_cond_broadcast(&m_EventCond);

    BOOL fHandled = SignalMultipleEventObject();

    // If no one is waiting for this signal or the event is manual reset, keep it set to TRUE
    if (m_fManual || !fHandled)
    {
        m_fSignaled = TRUE;
    }

    LeaveMutex();

    return TRUE;
}

int CEvent::Reset()
{
    EnterMutex();

    m_fSignaled = FALSE;

    LeaveMutex();

    return TRUE;
};

int CEvent::Wait(UINT32 dwTimeout)
{
    int rc = WAIT_EVENT_0_SIGNALED;
    struct timespec Time;

    do
    {
        EnterMutex();

        if (m_fSignaled)
        {
            // No need to wait
            break;
        }

        if (dwTimeout == WAIT_FOREVER)
        {
            pthread_cond_wait(&m_EventCond, &m_mutex);
        }
        else
        {
            Time = msFromNow(dwTimeout);

            rc = pthread_cond_timedwait(&m_EventCond, &m_mutex, &Time);

            if (rc != 0)
            {
                if (rc != ETIMEDOUT)
                {
                    printf("CEvent::WaitForEvent() : ERROR : pthread_cond_timedwait(): returned %d\r\n", rc);
                }
            }
        }
    }
    while (0);

    if (!m_fManual)
    {
        m_fSignaled = FALSE;
    }

    LeaveMutex();

    if (rc == ETIMEDOUT)
    {
        rc = WAIT_TIMEDOUT;
    }

    if (rc == 0)
    {
        rc = WAIT_EVENT_0_SIGNALED;
    }

    return rc;
}

CMutipleEvent::CMutipleEvent(UINT32 nEvents)
{
    pthread_cond_init(&m_MultipleEventCond, NULL);
    m_nLastSignaledEvent = -1;
    m_fSignaled = FALSE;
    m_nEvents = nEvents;

    m_pEvents = new CEvent*[nEvents] ;
    if (m_pEvents != NULL)
    {
        for (int ii = 0; ii < nEvents; ii++)
            m_pEvents[ii] = NULL;
    }
};

CMutipleEvent::~CMutipleEvent()
{
    EnterMutex();
    {
        pthread_cond_destroy(&m_MultipleEventCond);
        delete [] m_pEvents;
    }
    LeaveMutex();
};

void CMutipleEvent::AddEvent(int iEventIndex, CEvent* pEvent)
{
    assert((iEventIndex >= 0) && (iEventIndex < m_nEvents));
    
    EnterMutex();
    {
        if (m_pEvents[iEventIndex])
        {
            m_pEvents[iEventIndex]->DeleteObserver(this);
        }

        m_pEvents[iEventIndex] = pEvent;

        if (pEvent)
        {
            pEvent->CreateObserver(this, iEventIndex);
        }
    }

    LeaveMutex();
}

void CMutipleEvent::RemoveEvent(int iEventIndex)
{
    assert((iEventIndex >= 0) && (iEventIndex < m_nEvents));

    if (m_pEvents[iEventIndex])
        m_pEvents[iEventIndex]->DeleteObserver(this);

    m_pEvents[iEventIndex] = NULL;
}

BOOL CMutipleEvent::Update(int iEventIndex)
{
    BOOL fHandled = FALSE;

    if (!m_fSignaled)
    {
        m_nLastSignaledEvent = iEventIndex;
        m_fSignaled = TRUE;
        fHandled = TRUE;

        //printf("CMutipleEvent::Update() : m_nLastSignaledEvent = %d\r\n", m_nLastSignaledEvent);
    
        pthread_cond_signal(&m_MultipleEventCond);
    }

    return fHandled;
}

int CMutipleEvent::Wait(UINT32 dwTimeout)
{
    int rc = WAIT_TIMEDOUT;
    struct timespec Time;

    EnterMutex();

    if (m_fSignaled)
    {
        //printf("CMutipleEvent::Wait() : Returning m_nLastSignaledEvent = %d\r\n", m_nLastSignaledEvent);
        rc = m_nLastSignaledEvent;
    }
    else if (dwTimeout == WAIT_FOREVER)
    {
        pthread_cond_wait(&m_MultipleEventCond, &m_mutex);
        rc = m_nLastSignaledEvent;
    }
    else
    {
        Time = msFromNow(dwTimeout);
        rc = pthread_cond_timedwait(&m_MultipleEventCond, &m_mutex, &Time);

        if (rc == 0)
        {
            rc = m_nLastSignaledEvent;
        }
        else
        {
            if (rc != ETIMEDOUT)
            {
                printf("CEvent::Wait() : ERROR : pthread_cond_timedwait(): returned %d\r\n", rc);

                // TODO Deal with these errors
                rc = ETIMEDOUT;

            }
        }
    }

    if (rc == ETIMEDOUT)
    {
        rc = WAIT_TIMEDOUT;
    }
    LeaveMutex();

    return rc;
}

/////////////////////////////////////// WaitFor Interfaces ///////////////////////////////////////////

UINT32 WaitForAnyEvent(UINT32 nEvents, CEvent ** rgpEvents, UINT32 dwTimeout)
{
    CMutipleEvent* pMultipleEvents = new CMutipleEvent(nEvents);
    BOOL fHaveAtLeastOne = FALSE;
    UINT32 dwRet = WAIT_TIMEDOUT;

    // load the events into the MultipleEvent object
    for (UINT32 index = 0; index < nEvents; index++)
    {
        CEvent* pEvent = (CEvent*)rgpEvents[index];

        if (pEvent != NULL)
        {
            pMultipleEvents->AddEvent(index, pEvent);

            if (rgpEvents[index]->FSignaled() && !fHaveAtLeastOne)
            {
                //printf("WaitForAnyEvent() : Event at index %d is set, handling event\r\n", index);

                // set dwRet and update status as required
                dwRet = index;
                fHaveAtLeastOne = TRUE;

                if (!rgpEvents[index]->FManual())
                {
                    rgpEvents[index]->Reset();
                }
            }
            else if (rgpEvents[index]->FSignaled())
            {
                //printf("WaitForAnyEvent() : Event at index %d is set, will handle on next wait\r\n, index");
            }
            else
            {
                //printf("WaitForAnyEvent() : Event at index %d is not set\r\n", index);
            }
        }
        else
        {
            printf("WaitForAnyEvent() : ERROR : Item %d was NULL\r\n", index);
        }
    }

    if (!fHaveAtLeastOne)
    {
        dwRet = pMultipleEvents->Wait(dwTimeout);
    }

    for (UINT32 index = 0; index < nEvents; index++)
    {
        CEvent* pEvent = (CEvent*)rgpEvents[index];

        if (pEvent != NULL)
        {
            pMultipleEvents->RemoveEvent(index);
        }
    }

    delete pMultipleEvents;

    return dwRet;
}

UINT32 WaitForAllEvents(UINT32 nEvents, CEvent ** rgpEvents, UINT32 dwTimeout)
{
    // FIXME Not currently supported... or needed?
    return WAIT_TIMEDOUT;
}
