#ifdef __linux__
#include <pthread.h>
#include "defines.h"
#include "sync_ops.h"
#endif

#define THREAD_UNINITIALIZED        0xFFFFFFFF

#define THREAD_PRIORITY_LEVEL_UNKNOWN     -1
#define THREAD_PRIORITY_LEVEL_LOWEST      0
#define THREAD_PRIORITY_LEVEL_LOW         1
#define THREAD_PRIORITY_LEVEL_NORMAL      2
#define THREAD_PRIORITY_LEVEL_HIGH        3
#define THREAD_PRIORITY_LEVEL_HIGHEST     4

// Select any (bitmask)
#define THREAD_FLAGS_NONE               0x0000
#define THREAD_FLAGS_START_SUSPENDED    0x0001
#define THREAD_FLAGS_JOINABLE           0x0002

#define THREAD_WAIT_0                   0x00000000
#define THREAD_WAIT_TIMEOUT             0xFFFFFFFF
#define THREAD_WAIT_NOT_JOINABLE        0xFFFF0001
#define THREAD_WAIT_GEN_FAILURE         0xFFFF0002

typedef void* (*THREAD_PROC_PTR)(void*);

typedef struct sThreadData
{
    void *          pvDataObj;
    THREAD_PROC_PTR pvThreadProc;
    BOOL *          pfRunning;
} THREAD_DATA;

typedef struct sThreadWaitData
{
    pthread_t   thread;
    CEvent*     pEvent;
} THREAD_WAIT_DATA;

class CThread
{
public:
    CThread(THREAD_PROC_PTR pvThreadProc, void * pvDataObj, UINT32 dwFlags, UINT32 dwStackSize);
    ~CThread();

    BOOL    SetPriority(UINT32 dwPriority);
    UINT32   GetPriority();

    UINT32   WaitForThread(UINT32 dwTimeout);

private:
    void *  m_pvDataObj;
    void *  m_pvThreadProc;
    UINT32   m_uiPriority;
    BOOL    m_fJoinable;
    BOOL    m_fRunning;

#ifdef __linux__
    pthread_t m_thread;
#else
    HANDLE m_thread;
#endif

};

