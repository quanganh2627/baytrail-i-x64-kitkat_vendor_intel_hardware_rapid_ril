////////////////////////////////////////////////////////////////////////////
// thread_manager.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Manages starting and termination of threads for all command queues
//
// Author:  Mike Worth
// Created: 2009-07-16
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  July 16/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_THREAD_MANAGER_H
#define RRIL_THREAD_MANAGER_H

#include "rril.h"
#include "sync_ops.h"

class CThreadManager
{
public:
    static BOOL Start(unsigned int nChannels, CEvent *& pStopThreadsEvent);
    static BOOL Stop();
    static void RegisterThread();

private:
    CThreadManager(unsigned int nChannels, CEvent *& pStopThreadsEvent);
    ~CThreadManager();
    BOOL Initialize();
    BOOL StopThreads();
    void Register();
    void SignalComplete();
    BOOL StartChannelThreads();

    static CThreadManager* m_pInstance;
    
    unsigned int    m_nChannelsTotal;
    unsigned int    m_nChannelsActive;
    CMutex *        m_pTManMutex;
    CEvent *        m_pStartupCompleteEvent;

    // ThreadManager does not own this event so it will not clean it up
    CEvent *&       m_pStopThreadsEvent;
};

#endif // RRIL_THREAD_MANAGER_H
