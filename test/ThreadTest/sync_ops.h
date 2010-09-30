#ifndef __sync_ops_h__
#define __sync_ops_h__

#ifndef __linux__
    typedef CRITICAL_SECTION    pthread_mutex_t;
#else
    #include "defines.h"
    #include <pthread.h>
    #include "List.h"
    using namespace android;

    #define WAIT_EVENT_0_SIGNALED       0
    #define WAIT_TIMEDOUT               0xFFFFFFFF
#endif

class CMutex
{
    public:
        CMutex();
        ~CMutex();

        void EnterMutex();
        void LeaveMutex();

    protected:
        pthread_mutex_t m_mutex;
};

#ifdef __linux__
class CEvent;

// One MultipleEvent for each call to WaitForAnyEvent. It contains pointers to
// all associated events. It creates observers for each event when they are added
// and deletes them when removed. Will get signaled when any of the events watched
// are signaled.
class CMutipleEvent : public CMutex
{
    public:
        CMutipleEvent(UINT32 nEvents);
        ~CMutipleEvent();

        // Sets last signaled event and the multievent condition
        BOOL Update(int iEventIndex);

        // Attach event with associated new Observer to the ME
        void AddEvent(int iEventIndex , CEvent* pEvent);

        // Detach event at iEventIndex and delete Observer
        void RemoveEvent(int iEventIndex);

        // dwTimeout in ms
        int Wait(UINT32 dwTimeout = 0);

    protected:
        pthread_cond_t m_MultipleEventCond;
        CEvent**       m_pEvents;
        UINT32           m_nEvents;

    private:
        volatile int   m_nLastSignaledEvent;
        BOOL           m_fSignaled;
};

// Used by MultipleEvent objects to link together multiple events.
// These are owned by the associated event. An observer is only associated
// to one MultipleEvent.
class CObserver
{
    public:
        CObserver(CMutipleEvent* pMultipleEvent, int iEventIndex)
        {
            m_iEventIndex = iEventIndex;
            m_pMultipleEvent = pMultipleEvent;
            m_pNext = m_pPrev = NULL;
        }

    public:
        CObserver*     m_pNext;
        CObserver*     m_pPrev;

        CMutipleEvent* m_pMultipleEvent;
        int            m_iEventIndex;
};

typedef List<CObserver*> mapObserver;
typedef mapObserver::iterator mapObserverIterator;

#endif // ifdef __linux__

// Each event can have many observers, one for each time someone includes it
// in a WaitForAnyEvent call parameter list. The event is also registered with
// a MultipleEvent object if used in a WaitForAnyEvent call. Otherwise it operates
// on it's own with no observer or associated MultipleEvent object.
class CEvent
#ifdef __linux__
 : public CMutex
#endif
{
    public:

        CEvent(const char * szName = NULL, BOOL bManual = FALSE);
        ~CEvent();

        int Reset();
        int Signal();

        // dwTimeout in ms
        int Wait(UINT32 dwTimeout = 0);

#ifdef __linux__
        BOOL FManual()      { return m_fManual; }
        BOOL FSignaled()    { return m_fSignaled; }

        void CreateObserver(CMutipleEvent* pMultiEvent, int iValue);
        void DeleteObserver(CMutipleEvent*);

    private:
        char *          m_szName;
        BOOL            m_fManual;
        mapObserver     m_mObservers;

        BOOL SignalMultipleEventObject();

    protected:
        pthread_cond_t m_EventCond;
        BOOL           m_fSignaled;
#endif

};

/////////////////////////////////////// WaitFor Interfaces ///////////////////////////////////////////

UINT32 WaitForAnyEvent(UINT32 nEvents, CEvent ** rgpEvents, UINT32 dwTimeout);
UINT32 WaitForAllEvents(UINT32 nEvents, CEvent ** rgpEvents, UINT32 dwTimeout);

#endif // __sync_ops_h__
