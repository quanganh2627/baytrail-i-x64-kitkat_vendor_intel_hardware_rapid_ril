/*
 *
 *
 * Copyright (C) 2009 Intrinsyc Software International,
 * Inc.  All Rights Reserved
 *
 * Use of this code is subject to the terms of the
 * written agreement between you and Intrinsyc.
 *
 * UNLESS OTHERWISE AGREED IN WRITING, THIS WORK IS
 * DELIVERED ON AN AS IS BASIS WITHOUT WARRANTY,
 * REPRESENTATION OR CONDITION OF ANY KIND, ORAL OR
 * WRITTEN, EXPRESS OR IMPLIED, IN FACT OR IN LAW
 * INCLUDING WITHOUT LIMITATION, THE IMPLIED WARRANTIES
 * OR CONDITIONS OF MERCHANTABLE QUALITY
 * AND FITNESS FOR A PARTICULAR PURPOSE
 *
 * This work may be subject to patent protection in the
 *  United States and other jurisdictions
 *
 * Description:
 *    General utilities and system start-up and 
 *    shutdown management
 *
 *  Author: Francesc J. Vilarino Guell
 *  Created: 2009-11-06
 *
 */


#include "types.h"
#include "rillog.h"
#include "../util.h"
#include "sync_ops.h"
#include "thread_ops.h"
#include "rilqueue.h"
#include "globals.h"
#include "thread_manager.h"
#include "cmdcontext.h"
#include "rilchannels.h"
#include "channel_atcmd.h"
#include "channel_sim.h"
#include "channel_data.h"
#include "response.h"
#include "repository.h"
#include "te.h"
#include "rildmain.h"
#include "systemmanager.h"

// Tx Queue
CRilQueue<CCommand*>* g_pTxQueue[RIL_CHANNEL_MAX];
CEvent* g_TxQueueEvent[RIL_CHANNEL_MAX];

// Rx Queue
CRilQueue<CResponse*>* g_pRxQueue[RIL_CHANNEL_MAX];
CEvent* g_RxQueueEvent[RIL_CHANNEL_MAX];

//  Array of CChannels
CChannel* g_pRilChannel[RIL_CHANNEL_MAX] = { NULL };


CSystemManager* CSystemManager::m_pInstance = NULL;


CSystemManager& CSystemManager::GetInstance()
{
    //RIL_LOG_VERBOSE("CSystemManager::GetInstance() - Enter\r\n");
    if (!m_pInstance)
    {
        m_pInstance = new CSystemManager;
        if (!m_pInstance)
        {
            RIL_LOG_CRITICAL("CSystemManager::GetInstance() - ERROR - Cannot create instance\r\n");
            // TODO: blow-up
        }
    }
    //RIL_LOG_VERBOSE("CSystemManager::GetInstance() - Exit\r\n");
    return *m_pInstance;
}

BOOL CSystemManager::Destroy()
{
    RIL_LOG_INFO("CSystemManager::Destroy() - Enter\r\n");
    if (m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = NULL;
    }
    else
    {
        RIL_LOG_VERBOSE("CSystemManager::Destroy() - WARNING - Called with no instance\r\n");
    }
    RIL_LOG_INFO("CSystemManager::Destroy() - Exit\r\n");
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
CSystemManager::CSystemManager()
  :
    m_pSystemInitCompleteEvent(NULL),
    m_pExitRilEvent(NULL),
    m_pSimUnlockedEvent(NULL),
    m_pModemPowerOnEvent(NULL),
    m_pInitStringCompleteEvent(NULL),
    m_bExitSimInitializationThread(FALSE),
    m_RequestInfoTable(),
    m_bFailedToInitialize(FALSE),
    m_fModemUnlockInitComplete(FALSE),
    m_fSimGeneralInitComplete(FALSE),
    m_fIsModemThreadRunning(FALSE),
    m_fIsSimThreadRunning(FALSE)
{
    RIL_LOG_INFO("CSystemManager::CSystemManager() - Enter\r\n");
    
    // create mutexes
    m_pAccessorMutex = new CMutex();
    
    // TODO / FIXME : Someone is locking this mutex outside of the destructor or system init functions
    //                Need to track down when time is available. Workaround for now is to call TryLock
    //                so we don't block during suspend.
    m_pSystemManagerMutex = new CMutex();
    
    RIL_LOG_INFO("CSystemManager::CSystemManager() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CSystemManager::~CSystemManager()
{
    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Enter\r\n");
    BOOL fLocked = TRUE;

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] before Lock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));

    for (int x = 0; x < 3; x++)
    {
        Sleep(300);

        if (!CMutex::TryLock(m_pSystemManagerMutex))
        {
            RIL_LOG_CRITICAL("CSystemManager::~CSystemManager() - ERROR: Failed to lock mutex!\r\n");
            fLocked = FALSE;
        }
        else
        {
            RIL_LOG_VERBOSE("CSystemManager::~CSystemManager() - DEBUG: Mutex Locked!\r\n");
            fLocked = TRUE;
            break;
        }
    }
    
    RIL_LOG_VERBOSE("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] after Lock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));
 

    // signal the cancel event to kill the thread
    CEvent::Signal(m_pExitRilEvent);

    //  Cancel SIM initialization if running
    StopSimInitialization();

    CThreadManager::Stop();

    // destroy events
    if (m_pExitRilEvent)
    {
        delete m_pExitRilEvent;
        m_pExitRilEvent = NULL;
    }

    // free queues
    DeleteQueues();

    //  Close the COM ports and delete CChannel array.
    CloseChannelPorts();

    if (m_pSystemInitCompleteEvent)
    {
        delete m_pSystemInitCompleteEvent;
        m_pSystemInitCompleteEvent = NULL;
    }

    if (m_pSimUnlockedEvent)
    {
        delete m_pSimUnlockedEvent;
        m_pSimUnlockedEvent = NULL;
    }

    if (m_pModemPowerOnEvent)
    {
        delete m_pModemPowerOnEvent;
        m_pModemPowerOnEvent = NULL;
    }
    
    if (m_pInitStringCompleteEvent)
    {
        delete m_pInitStringCompleteEvent;
        m_pInitStringCompleteEvent = NULL;
    }
    
    if (m_pAccessorMutex)
    {
        delete m_pAccessorMutex;
        m_pAccessorMutex = NULL;
    }

    if (fLocked)
    {
        RIL_LOG_VERBOSE("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] before Unlock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));
        
        CMutex::Unlock(m_pSystemManagerMutex);

        RIL_LOG_VERBOSE("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] after Unlock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));
    }

    if (m_pSystemManagerMutex)
    {
        delete m_pSystemManagerMutex;
        m_pSystemManagerMutex = NULL;
    }

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Exit\r\n");
}


///////////////////////////////////////////////////////////////////////////////
// Start initialization
//
BOOL CSystemManager::InitializeSystem()
{
    RIL_LOG_INFO("CSystemManager::InitializeSystem() - Enter\r\n");
    
    CMutex::Lock(m_pSystemManagerMutex);

    CRepository repository;
    int iTemp;
    BOOL bRetVal = FALSE;

    if (NULL != m_pSystemInitCompleteEvent)
    {
        if (WAIT_EVENT_0_SIGNALED == CEvent::Wait(m_pSystemInitCompleteEvent, 0))
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: System is already initialized\r\n");
            bRetVal = TRUE;
            goto Done;
        }
    }

    if (repository.Read(g_szGroupLastValues, g_szLastCLIP, iTemp))
    {
        g_dwLastCLIP = (UINT32)iTemp;
        RIL_LOG_INFO("CSystemManager::InitializeSystem() - Retrieved Last CLIP Value: 0x%X\r\n", g_dwLastCLIP);
    }

    if (repository.Read(g_szGroupLastValues, g_szLastCLIR, iTemp))
    {
        g_dwLastCLIR = (UINT32)iTemp;
        RIL_LOG_INFO("CSystemManager::InitializeSystem() - Retrieved Last CLIR Value: 0x%X\r\n", g_dwLastCLIR);
    }

    if (repository.Read(g_szGroupLastValues, g_szLastCOLP, iTemp))
    {
        g_dwLastCOLP = (UINT32)iTemp;
        RIL_LOG_INFO("CSystemManager::InitializeSystem() - Retrieved Last COLP Value: 0x%X\r\n", g_dwLastCOLP);
    }

    if (repository.Read(g_szGroupLastValues, g_szLastCOLR, iTemp))
    {
        g_dwLastCOLR = (UINT32)iTemp;
        RIL_LOG_INFO("CSystemManager::InitializeSystem() - Retrieved Last COLR Value: 0x%X\r\n", g_dwLastCOLR);
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutCmdInit, iTemp))
    {
        g_TimeoutCmdInit = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutCmdNoOp, iTemp))
    {
        g_TimeoutCmdNoOp = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutCmdOnline, iTemp))
    {
        g_TimeoutCmdOnline = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutAPIDefault, iTemp))
    {
        g_TimeoutAPIDefault = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutDTRDrop, iTemp))
    {
        g_TimeoutDTRDrop = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutWaitForInit, iTemp))
    {
        g_TimeoutWaitForInit = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupRILSettings, g_szDefaultCmdRetries, iTemp))
    {
        g_DefaultCmdRetries = (UINT32)iTemp;
    }

    if (m_pSystemInitCompleteEvent)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pSystemInitCompleteEvent was already created!\r\n");
    }
    else
    {
        m_pSystemInitCompleteEvent = new CEvent(NULL, TRUE);
        if (!m_pSystemInitCompleteEvent)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create System Init Complete Event.\r\n");
            goto Done;
        }
    }

    if (m_pSimUnlockedEvent)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pSimUnlockedEvent was already created!\r\n");
    }
    else
    {
        m_pSimUnlockedEvent = new CEvent(NULL, TRUE);
        if (!m_pSimUnlockedEvent)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create Sim Unlocked Event.\r\n");
            goto Done;
        }
    }

    if (m_pModemPowerOnEvent)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pModemPowerOnEvent was already created!\r\n");
    }
    else
    {
        m_pModemPowerOnEvent = new CEvent(NULL, TRUE);
        if (!m_pModemPowerOnEvent)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create Modem Power On Event.\r\n");
            goto Done;
        }
    }

    if (m_pInitStringCompleteEvent)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pInitStringCompleteEvent was already created!\r\n");
    }
    else
    {
        m_pInitStringCompleteEvent = new CEvent(NULL, TRUE);
        if (!m_pInitStringCompleteEvent)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create Init commands complete Event.\r\n");
            goto Done;
        }
    }
    
    ResetSystemState();

    // Open the serial ports (and populate g_pRilChannel)
    if (!OpenChannelPorts())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't open VSPs.\r\n");
        goto Done;
    }
    RIL_LOG_INFO("CSystemManager::InitializeSystem() - VSPs were opened successfully.\r\n");

    m_pExitRilEvent = new CEvent(NULL, TRUE);
    if (NULL == m_pExitRilEvent)
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create exit event.\r\n");
        goto Done;
    }

    // Create the Queues
    if (!CreateQueues())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Unable to create queues\r\n");
        goto Done;
    }

    if (!CThreadManager::Start(RIL_CHANNEL_MAX * 2, m_pExitRilEvent))
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Thread manager failed to start.\r\n");
    }

    if (!InitializeModem())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't start Modem initialization!\r\n");
        goto Done;
    }

    if (!InitializeSim())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't start SIM initialization!\r\n");
        goto Done;        
    }
    
    bRetVal = TRUE;

Done:
    if (!bRetVal)
    {
        if (m_pSystemInitCompleteEvent)
        {
            delete m_pSystemInitCompleteEvent;
            m_pSystemInitCompleteEvent = NULL;
        }

        if (m_pSimUnlockedEvent)
        {
            delete m_pSimUnlockedEvent;
            m_pSimUnlockedEvent = NULL;
        }

        if (m_pModemPowerOnEvent)
        {
            delete m_pModemPowerOnEvent;
            m_pModemPowerOnEvent = NULL;
        }
        
        if (m_pInitStringCompleteEvent)
        {
            delete m_pInitStringCompleteEvent;
            m_pInitStringCompleteEvent = NULL;
        }
        
        CThreadManager::Stop();

        if (m_pExitRilEvent)
        {
            if (CEvent::Signal(m_pExitRilEvent))
            {
                RIL_LOG_INFO("CSystemManager::InitializeSystem() : INFO : Signaled m_pExitRilEvent as we are failing out, sleeping for 1 second\r\n");
                Sleep(1000);
                RIL_LOG_INFO("CSystemManager::InitializeSystem() : INFO : Sleep complete\r\n");
            }

            delete m_pExitRilEvent;
            m_pExitRilEvent = NULL;
        }
    }

    CMutex::Unlock(m_pSystemManagerMutex);

    RIL_LOG_INFO("CSystemManager::InitializeSystem() - Exit\r\n");

    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::VerifyAllChannelsCompletedInit(eComInitIndex eInitIndex)
{
    BOOL bRetVal = TRUE;

    for (int i=0; i < RIL_CHANNEL_MAX; i++)
    {
        if (!IsChannelCompletedInit((EnumRilChannel)i, eInitIndex))
        {
            bRetVal = FALSE;
            break;
        }
    }
    
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::SetChannelCompletedInit(EnumRilChannel eChannel, eComInitIndex eInitIndex)
{
    if ((eChannel < RIL_CHANNEL_MAX) && (eInitIndex < COM_MAX_INDEX))
    {
        m_rgfChannelCompletedInit[eChannel][eInitIndex] = TRUE;
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SetChannelCompletedInit() - ERROR: Invalid channel [%d] or init index [%d]\r\n", eChannel, eInitIndex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::IsChannelCompletedInit(EnumRilChannel eChannel, eComInitIndex eInitIndex)
{
    if ((eChannel < RIL_CHANNEL_MAX) && (eInitIndex < COM_MAX_INDEX))
    {
        return m_rgfChannelCompletedInit[eChannel][eInitIndex];
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::IsChannelCompletedInit() - ERROR: Invalid channel [%d] or init index [%d]\r\n", eChannel, eInitIndex);
        return FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::ResetChannelCompletedInit()
{
    memset(m_rgfChannelCompletedInit, 0, sizeof(m_rgfChannelCompletedInit));
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::ResetSystemState()
{
    RIL_LOG_VERBOSE("CSystemManager::ResetSystemState() - Enter\r\n");
    
    m_fModemUnlockInitComplete = FALSE;
    m_fSimGeneralInitComplete = FALSE;
    
    ResetChannelCompletedInit();
    
    RIL_LOG_VERBOSE("CSystemManager::ResetSystemState() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::CreateQueues()
{
    RIL_LOG_VERBOSE("CSystemManager::CreateQueues() - Enter\r\n");
    BOOL bRet = FALSE;

    // Create command and response queues
    for (int i = 0; i < RIL_CHANNEL_MAX; ++i)
    {
        if (NULL == (g_TxQueueEvent[i] = new CEvent(NULL, FALSE))     ||
            NULL == (g_pTxQueue[i] = new CRilQueue<CCommand*>(true)) ||
            NULL == (g_RxQueueEvent[i] = new CEvent(NULL, FALSE))     ||
            NULL == (g_pRxQueue[i] = new CRilQueue<CResponse*>(true)))
        {
            RIL_LOG_VERBOSE("CSystemManager::CreateQueues() - ERROR: Out of memory\r\n");
            goto Done;
        }
    }

    bRet = TRUE;

Done:
    if (!bRet)
    {
        DeleteQueues();
    }

    RIL_LOG_VERBOSE("CSystemManager::CreateQueues() - Exit\r\n");
    return bRet;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::DeleteQueues()
{
    RIL_LOG_VERBOSE("CSystemManager::DeleteQueues() - Enter\r\n");

    for (int i = 0; i < RIL_CHANNEL_MAX; ++i)
    {
        delete g_TxQueueEvent[i];
        g_TxQueueEvent[i] = NULL;
            
        delete g_pTxQueue[i];
        g_pTxQueue[i] = NULL;

        delete g_RxQueueEvent[i];
        g_RxQueueEvent[i] = NULL;

        delete g_pRxQueue[i];
        g_pRxQueue[i] = NULL;
    }

    RIL_LOG_VERBOSE("CSystemManager::DeleteQueues() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CChannel* CSystemManager::CreateChannel(EnumRilChannel eIndex)
{
    CChannel* pChannel = NULL;

    switch(eIndex)
    {
        case RIL_CHANNEL_ATCMD:
            pChannel = new CChannel_ATCmd(RIL_CHANNEL_ATCMD); 
            break;

#ifdef RIL_ENABLE_CHANNEL_SIM
        case RIL_CHANNEL_SIM:
            pChannel = new CChannel_SIM(RIL_CHANNEL_SIM);
            break;
#endif // RIL_ENABLE_CHANNEL_SIM

#ifdef RIL_ENABLE_CHANNEL_DATA1
        case RIL_CHANNEL_DATA1:
            pChannel = new CChannel_Data(RIL_CHANNEL_DATA1);
            break;
#endif // RIL_ENABLE_CHANNEL_DATA1

#ifdef RIL_ENABLE_CHANNEL_DATA2
        case RIL_CHANNEL_DATA2:
            pChannel = new CChannel_Data(RIL_CHANNEL_DATA2);        
            break;
#endif // RIL_ENABLE_CHANNEL_DATA2

#ifdef RIL_ENABLE_CHANNEL_DATA3
        case RIL_CHANNEL_DATA3:
            pChannel = new CChannel_Data(RIL_CHANNEL_DATA3);        
            break;
#endif // RIL_ENABLE_CHANNEL_DATA3

        default:
            break;
    }

    return pChannel;
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::OpenChannelPorts()
{
    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPorts() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        g_pRilChannel[i] = CreateChannel((EnumRilChannel)i);
        if (!g_pRilChannel[i] || !g_pRilChannel[i]->InitChannel())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts : Channel[%d] (0x%X) Init failed\r\n", i, (UINT32)g_pRilChannel[i]);
            goto Done;
        }

        if (!g_pRilChannel[i]->OpenPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts : Channel[%d] OpenPort() failed\r\n", i);
            goto Done;
        }

        if (!g_pRilChannel[i]->InitPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts : Channel[%d] InitPort() failed\r\n", i);
            goto Done;
        }        
    }

    //  We made it this far, return TRUE.
    bRet = TRUE;

Done:
    if (!bRet)
    {
        //  We had an error.
        CloseChannelPorts();
    }

    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPorts() - Exit\r\n");
    return bRet;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::CloseChannelPorts()
{
    RIL_LOG_VERBOSE("CSystemManager::CloseChannelPorts() - Enter\r\n");

    for (int i=0; i<RIL_CHANNEL_MAX; i++)
    {
        if (g_pRilChannel[i])
        {
            g_pRilChannel[i]->ClosePort();
        }
    }

    for (int i=0; i<RIL_CHANNEL_MAX; i++)
    {
        if (g_pRilChannel[i])
        {
            delete g_pRilChannel[i];
            g_pRilChannel[i] = NULL;
        }
    }

    RIL_LOG_VERBOSE("CSystemManager::CloseChannelPorts() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
// Test the exit event
//
BOOL CSystemManager::IsExitRequestSignalled() const
{
    RIL_LOG_VERBOSE("CSystemManager::IsExitRequestSignalled() - Enter\r\n");

    BOOL bRetVal = WAIT_EVENT_0_SIGNALED == CEvent::Wait(m_pExitRilEvent, 0);

    RIL_LOG_VERBOSE("CSystemManager::IsExitRequestSignalled() - Result: %s\r\n", bRetVal ? "Set" : "Not Set");
    RIL_LOG_VERBOSE("CSystemManager::IsExitRequestSignalled() - Exit\r\n");
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::GetRequestInfo(REQ_ID reqID, REQ_INFO &rReqInfo)
{
    m_RequestInfoTable.GetRequestInfo(reqID, rReqInfo);
}

///////////////////////////////////////////////////////////////////////////////
void* CSystemManager::StartModemInitializationThreadWrapper(void *pArg)
{
    static_cast<CSystemManager *> (pArg)->StartModemInitializationThread();
    return NULL;
}

BOOL CSystemManager::InitializeModem()
{
    BOOL bRetVal = TRUE;
    CThread* pModemThread = NULL;
    
    if (!SendModemInitCommands(COM_BASIC_INIT_INDEX))
    {
        RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send basic init commands!\r\n");
        goto Done;
    }

    pModemThread = new CThread(StartModemInitializationThreadWrapper, this, THREAD_FLAGS_NONE, 0);
    if (!pModemThread || !CThread::IsRunning(pModemThread))
    {
        RIL_LOG_CRITICAL("InitializeModem() - ERROR: Unable to launch modem init thread\r\n");
        bRetVal = FALSE;
    }
    else
    {
        m_fIsModemThreadRunning = TRUE;
    }

    delete pModemThread;
    pModemThread = NULL;

Done:
    return bRetVal;
}

void CSystemManager::StartModemInitializationThread()
{
    RIL_LOG_VERBOSE("StartModemInitializationThread() : Start Modem initialization thread\r\n");
    BOOL fUnlocked = FALSE;
    BOOL fPowerOn = FALSE;
    UINT32 uiNumEvents = 0;

    while (!fUnlocked || !fPowerOn)
    {
        UINT32 ret = 0;

        if (!fUnlocked && !fPowerOn)
        {
            RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Waiting for unlock, power on or cancel\r\n");
            CEvent* rgpEvents[] = { m_pSimUnlockedEvent, m_pModemPowerOnEvent, m_pExitRilEvent };
            uiNumEvents = 3;
            ret = CEvent::WaitForAnyEvent(uiNumEvents, rgpEvents, WAIT_FOREVER);
        }
        else if (fUnlocked)
        {
            RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Waiting for power on or cancel\r\n");
            CEvent* rgpEvents[] = { m_pModemPowerOnEvent, m_pExitRilEvent };
            uiNumEvents = 2;
            ret = CEvent::WaitForAnyEvent(uiNumEvents, rgpEvents, WAIT_FOREVER);
        }
        else
        {
            RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Waiting for unlock or cancel\r\n");
            CEvent* rgpEvents[] = { m_pSimUnlockedEvent, m_pExitRilEvent };
            uiNumEvents = 2;
            ret = CEvent::WaitForAnyEvent(uiNumEvents, rgpEvents, WAIT_FOREVER);
        }

        if (3 == uiNumEvents)
        {
            switch (ret)
            {
                case WAIT_EVENT_0_SIGNALED:
                {
                    RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Unlocked signaled\r\n");

                    if (!SendModemInitCommands(COM_UNLOCK_INIT_INDEX))
                    {
                        RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send unlock init commands!\r\n");
                        goto Done;
                    }

                    fUnlocked = true;
                    break;
                }
                
                case WAIT_EVENT_0_SIGNALED + 1:
                {
                    RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Power on signaled\r\n");
                    
                    if (!SendModemInitCommands(COM_POWER_ON_INIT_INDEX))
                    {
                        RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send power on init commands!\r\n");
                        goto Done;
                    }

                    fPowerOn = true;
                    break;
                }
                case WAIT_EVENT_0_SIGNALED + 2:
                    RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Failed waiting for events!\r\n");
                    goto Done;
                    break;
            }
        }
        else
        {
            switch (ret)
            {
                case WAIT_EVENT_0_SIGNALED:
                    if (fUnlocked)
                    {
                        RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Power on signaled\r\n");

                        if (!SendModemInitCommands(COM_POWER_ON_INIT_INDEX))
                        {
                            RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send power on init commands!\r\n");
                            goto Done;
                        }

                        fPowerOn = true;
                    }
                    else
                    {
                        RIL_LOG_VERBOSE("StartModemInitializationThread() - DEBUG: Unlocked signaled\r\n");

                        if (!SendModemInitCommands(COM_UNLOCK_INIT_INDEX))
                        {
                            RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send unlock init commands!\r\n");
                            goto Done;
                        }

                        fUnlocked = true;
                    }
                    break;

                case WAIT_EVENT_0_SIGNALED + 1:
                    RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Failed waiting for events!\r\n");
                    goto Done;
                    break;
            }
        }
    }

    if (!SendModemInitCommands(COM_READY_INIT_INDEX))
    {
        RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Unable to send ready init commands!\r\n");
        goto Done;
    }

    {
        CEvent* rgpEvents[] = { m_pInitStringCompleteEvent, m_pExitRilEvent };
        
        switch(CEvent::WaitForAnyEvent(2, rgpEvents, WAIT_FOREVER))
        {
            case WAIT_EVENT_0_SIGNALED:
            {
                RIL_LOG_INFO("StartModemInitializationThread() - INFO: Initialization strings complete\r\n");
                goto Done;
                break;
            }
            
            case WAIT_EVENT_0_SIGNALED + 1:
            default:
            {
                RIL_LOG_CRITICAL("StartModemInitializationThread() - ERROR: Exiting ril!\r\n");
                goto Done;
                break;
            }
        }
    }

Done:
    m_fIsModemThreadRunning = FALSE;
    RIL_LOG_VERBOSE("StartModemInitializationThread() : Modem initialized, thread exiting\r\n");
}

BOOL CSystemManager::SendModemInitCommands(eComInitIndex eInitIndex)
{
    RIL_LOG_VERBOSE("SendModemInitCommands() - Enter\r\n");

    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
        
        if (g_pRilChannel[i])
        {
            if (!g_pRilChannel[i]->SendModemConfigurationCommands(eInitIndex))
            {
                RIL_LOG_CRITICAL("CSystemManager::PhaseStart() : ERROR : Channel=[%d] returned ERROR\r\n", i);
                return FALSE;
            }
        }
    }

    RIL_LOG_VERBOSE("SendModemInitCommands() - Exit\r\n");
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::StopSimInitialization()
{
    if (m_fIsSimThreadRunning && 
        m_pSimInitializationEvent != NULL)
    {
        // signal thread to stop
        m_fAbortSimInitialization = TRUE;
        CEvent::Signal(m_pSimInitializationEvent);
    }
}

///////////////////////////////////////////////////////////////////////////////
void* CSystemManager::StartSimInitializationThreadWrapper(void *pArg)
{
    static_cast<CSystemManager *> (pArg)->StartSimInitializationThread();
    return NULL;
}

BOOL CSystemManager::InitializeSim()
{
    BOOL bRetVal = TRUE;
    
    if (!m_fIsSimThreadRunning)
    {
        CThread* pSimThread = new CThread(StartSimInitializationThreadWrapper, this, THREAD_FLAGS_NONE, 0);
        if (!pSimThread || !CThread::IsRunning(pSimThread))
        {
            RIL_LOG_CRITICAL("InitializeSim() - ERROR: Unable to start SIM initialization thread\r\n");
            bRetVal = FALSE;
        }
        else
        {
            m_fIsSimThreadRunning = TRUE;
        }
        delete pSimThread;
        pSimThread = NULL;
    }
    else
    {
        RIL_LOG_WARNING("InitializeSim() - WARN: SIM initialization thread is already running\r\n");
        bRetVal = FALSE;
    }

    return bRetVal;
}

void CSystemManager::StartSimInitializationThread()
{
    CRepository repository;
    int         iVal;
    BOOL        bSimLockEnabled = FALSE;

    // Create event to notify when state changes
    m_fAbortSimInitialization = FALSE;
    m_pSimInitializationEvent = new CEvent();
    if (NULL == m_pSimInitializationEvent)
    {
        RIL_LOG_CRITICAL("StartSimInitializationThread() : ERROR - Could not create event.\r\n");
        goto Done;
    }

    // Query SIM PIN
    if (repository.Read(g_szGroupSystemReady, g_szQuerySimLock, iVal) && (0 != iVal))
    {
        RIL_LOG_VERBOSE("StartSimInitializationThread() : Query SIM stage\r\n");
        if (!QuerySimPin(bSimLockEnabled) || m_fAbortSimInitialization)
        {
            goto Done;
        }
    }
    else
    {
        RIL_LOG_INFO("StartSimInitializationThread() : Skipping SIM unlock Init\r\n");

        g_RadioState.SetRadioOn();
        g_RadioState.SetRadioSIMReady();
    }

    if (!bSimLockEnabled)
    {
        RIL_LOG_INFO("StartSimInitializationThread() : Setting unlocked event as SIM is Ready\r\n");
        TriggerSimUnlockedEvent();
    }
    
    // Query SMS status
    if (repository.Read(g_szGroupSystemReady, g_szInitializeSimSms, iVal) && (0 != iVal))
    {
        RIL_LOG_VERBOSE("StartSimInitializationThread() : Query SMS stage\r\n");
        if (!InitializeSimSms() || m_fAbortSimInitialization)
        {
            goto Done;
        }
    }
    else
    {
        RIL_LOG_INFO("StartSimInitializationThread() : Skipping SMS Init\r\n");
        g_RadioState.SetRadioSMSReady();
    }
    
#ifdef RIL_ENABLE_SIMTK
    // Query STK status
    if (repository.Read(g_szGroupSystemReady, g_szInitializeSimSTK, iVal) && (0 != iVal))
    {
        RIL_LOG_VERBOSE("StartSimInitializationThread() : Query STK stage\r\n");
        if (!InitializeSimSTK() || m_fAbortSimInitialization)
        {
            goto Done;
        }
    }
    else
    {
        RIL_LOG_INFO("StartSimInitializationThread() : Skipping STK init\r\n");        
    }
#endif  // RIL_ENABLE_SIMTK

    // Query phonebook status
    if (repository.Read(g_szGroupSystemReady, g_szInitializeSimPhonebook, iVal) && (0 != iVal))
    {
        RIL_LOG_VERBOSE("StartSimInitializationThread() : Query phonebook stage\r\n");
        if (!InitializeSimPhonebook() || m_fAbortSimInitialization)
        {
            goto Done;
        }
    }
    else
    {
        RIL_LOG_INFO("StartSimInitializationThread() : Skipping PB Init\r\n");
        g_RadioState.SetRadioSIMPBReady();
    }

    // the SIM is now ready
    g_RadioState.SetRadioSIMReady();
    
    m_fSimGeneralInitComplete = TRUE;
    
Done:
    delete m_pSimInitializationEvent;
    m_pSimInitializationEvent = NULL;
    m_fIsSimThreadRunning = FALSE;
    RIL_LOG_VERBOSE("StartSimInitializationThread() : SIM initialized, thread exiting\r\n");
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::QuerySimPin(BOOL& bSimLockEnabled)
{
    BOOL              fRet = FALSE;
    CCommand*         pCmd          = NULL;
    //REQUEST_DATA      reqData;
    REQ_ID            reqID;
    REQ_INFO          reqInfo;
    UINT32            uiWaitRes;

    RIL_LOG_VERBOSE("QuerySimPin() : Enter\r\n");

    // Query for the PIN until we don't get back 515 as an error
    bSimLockEnabled = FALSE;
    while (!m_fAbortSimInitialization)
    {
        RIL_LOG_VERBOSE("QuerySimPin() : Sending SIM Query Command\r\n");

        // Set up a CPIN command to be sent
        // Note that we don't care what the return value is, so long as it's not that the radio isn't ready
        reqID = ND_REQ_ID_GETSIMSTATUS;

        // Get the info about this API
        memset(&reqInfo, 0, sizeof(reqInfo));
        GetRequestInfo(reqID, reqInfo);

        CCommand * pCmd = new CCommand(RIL_CHANNEL_SIM, NULL,
                                       reqID,
                                       "AT+CPIN?\r",
                                       &CTE::ParseGetSimStatus);
        if (pCmd)
        {
            pCmd->SetTimeout(reqInfo.uiTimeout);
            pCmd->SetHighPriority();

            CContextContainer* pContainer = new CContextContainer();
            if (pContainer)
            {
                pContainer->Add(new CContextEvent(*m_pSimInitializationEvent));
                pContainer->Add(new CContextPinQuery());
            }
            pCmd->SetContext((CContext*&) pContainer);
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("QuerySimPin() - ERROR: Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                break;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("QuerySimPin() - ERROR: Unable to allocate memory for command\r\n");
            break;
        }

        // block until we get a response back
        CEvent::Wait(m_pSimInitializationEvent, reqInfo.uiTimeout);

        if (m_fAbortSimInitialization)
            break;

        // we bail out when the SIM is ready (no PIN), or when
        // it's PIN locked
        if (g_RadioState.IsRadioSIMReady() ||
            g_RadioState.IsRadioSIMLocked())
        {
            RIL_LOG_INFO("QuerySimPin() : SIM responding\r\n");
            break;
        }
        else
        {
            // timed-out, sleep before the next try
            const UINT32 dwSIMSleep = 2000;

            RIL_LOG_INFO("QuerySimPin() : BEGIN Sleep [%d]\r\n", dwSIMSleep);
            Sleep(dwSIMSleep);
            RIL_LOG_INFO("QuerySimPin() : END Sleep\r\n");
        }
    }

    if (m_fAbortSimInitialization)
    {
        goto Done;
    }

    //  In this phase this SIM may be responding, but locked.  We need to poll until the SIM is
    //  unlocked.
    while (g_RadioState.IsRadioSIMLocked())
    {
        bSimLockEnabled = TRUE;
        if (m_fAbortSimInitialization)
        {
            goto Done;
        }

        const UINT32 uiSimUnlockTimeout = 3000;
        RIL_LOG_INFO("QuerySimPin() : Waiting for uiSimUnlockTimeout [%d]\r\n", uiSimUnlockTimeout);

        uiWaitRes = CEvent::Wait(m_pSimInitializationEvent, uiSimUnlockTimeout);

        if (WAIT_EVENT_0_SIGNALED == uiWaitRes)
        {
            RIL_LOG_INFO("QuerySimPin() : SIM unlocked\r\n");
        }
    }

    if (m_fAbortSimInitialization)
    {
        goto Done;
    }

    fRet = TRUE;

Done:
    RIL_LOG_VERBOSE("QuerySimPin() : Exit\r\n");
    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::InitializeSimSms()
{
    BOOL         fRet = FALSE;
    UINT32       uiWaitRes;
    REQUEST_DATA reqData;
#ifdef RIL_CELL_BROADCAST
    const BYTE* szCmd = "AT+CNMI=1,2,2,1\r";
#else
    const BYTE* szCmd = "AT+CNMI=1,2,0,1\r";
#endif  // RIL_CELL_BROADCAST

    RIL_LOG_VERBOSE("InitializeSimSms() : Enter\r\n");

    if (g_RadioState.IsRadioSIMLocked())
    {
        RIL_LOG_CRITICAL("InitializeSimSms() : ERROR : InitializeSimSms called when SIM locked!\r\n");
        goto Done;
    }

    {
        // queue SMS command
        memset(&reqData, 0, sizeof(REQUEST_DATA));
        CCommand* pCmd = new CCommand(RIL_CHANNEL_SIM,
                                      NULL,
                                      ND_REQ_ID_NONE,
                                      "AT+CSMS=1\r");
        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Done;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to allocate memory for command\r\n");
            goto Done;
        }

        // set the MO SMS to Circuit switched preferred.
        memset(&reqData, 0, sizeof(REQUEST_DATA));
        
        pCmd = new CCommand(RIL_CHANNEL_SIM,
                            NULL,
                            ND_REQ_ID_NONE,
                            "AT+CGSMS=3\r");
        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Done;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to allocate memory for command\r\n");
            goto Done;
        }

        RIL_LOG_INFO("InitializeSimSms() : Sending CNMI request\r\n");
        memset(&reqData, 0, sizeof(REQUEST_DATA));

        pCmd = new CCommand(RIL_CHANNEL_SIM,
                            NULL,
                            ND_REQ_ID_NONE,
                            szCmd);
        if (pCmd)
        {
            pCmd->SetHighPriority();
            CContext* pContext = new CContextEvent(*m_pSimInitializationEvent);
            pCmd->SetContext(pContext);
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Done;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("InitializeSimSms() - ERROR: Unable to allocate memory for command\r\n");
            goto Done;
        }
            
        // block until we get a response back
        uiWaitRes = CEvent::Wait(m_pSimInitializationEvent, g_TimeoutCmdInit);
        if (m_fAbortSimInitialization)
            goto Done;

        if (WAIT_EVENT_0_SIGNALED == uiWaitRes)
        {
            RIL_LOG_INFO("InitializeSimSms() : SMS Unlocked!\r\n");
        }
        else
        {
            // sleep before the next try
            //const UINT32 dwSMSSleep = 5000;
            //RIL_LOG_VERBOSE("InitializeSimSms() : BEGIN Sleep [%d]\r\n", dwSMSSleep);
            //Sleep(dwSMSSleep);
            //RIL_LOG_VERBOSE("InitializeSimSms() : END Sleep\r\n");
            RIL_LOG_CRITICAL("InitializeSimSms() : COMMAND TIMED-OUT!\r\n");
        }
    }

    if (m_fAbortSimInitialization)
    {
        goto Done;
    }

    g_RadioState.SetRadioSMSReady();
    fRet = TRUE;

Done:
    RIL_LOG_VERBOSE("InitializeSimSms() : Exit\r\n");
    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
#ifdef RIL_ENABLE_SIMTK
BOOL CSystemManager::InitializeSimSTK()
{
    BOOL         fRet = FALSE;
    REQUEST_DATA reqData;

    RIL_LOG_VERBOSE("InitializeSimSTK() : Enter\r\n");
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    CCommand* pCmd = new CCommand(RIL_CHANNEL_SIM,
                                  NULL,
                                  ND_REQ_ID_NONE,
                                  "AT+CFUN=6\r");
    if (pCmd)
    {
        pCmd->SetInitPhase(INIT_PHASE_2);
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("InitializeSimSTK() - ERROR: Unable to add command to queue\r\n");
            delete pCmd;
            pCmd = NULL;
            goto Done;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("InitializeSimSTK() - ERROR: Unable to allocate memory for command\r\n");
        goto Done;
    }

    fRet = TRUE;

Done:
    RIL_LOG_VERBOSE("InitializeSimSTK() : Exit\r\n");
    return fRet;
}
#endif // RIL_ENABLE_SIMTK

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::InitializeSimPhonebook()
{
    BOOL         fRet = FALSE;
    REQUEST_DATA reqData;
    UINT32       uiWaitRes;
    CRepository  repository;
    int          iTimeout = 10000;

    RIL_LOG_VERBOSE("InitializeSimPhonebook() : Enter\r\n");

    if (g_RadioState.IsRadioSIMLocked())
    {
        RIL_LOG_CRITICAL("InitializeSimPhonebook() : ERROR : InitializeSimPhonebook called when SIM locked!\r\n");
        goto Done;
    }

    while (!m_fAbortSimInitialization)
    {
        RIL_LOG_VERBOSE("InitializeSimPhonebook() : Sending select SIM Phbk Command\r\n");
        BOOL bResult = FALSE;

        memset(&reqData, 0, sizeof(REQUEST_DATA));
        if (!repository.Read(g_szGroupSystemReady, g_szInitializeSimPhonebookTimeout, iTimeout))
        {
            RIL_LOG_VERBOSE("InitializeSimPhonebook() - Defaulting to 10000ms timeout\r\n");
        }
        reqData.uiTimeout = iTimeout;
        
        if (!CopyStringNullTerminate(reqData.szCmd1, "AT+CPBS=\"SM\";+CPBR=?\r", sizeof(reqData.szCmd1)))
        {
            RIL_LOG_CRITICAL("InitializeSimPhonebook() - ERROR: Unable to write command to buffer\r\n");
            goto Done;
        }

        CCommand* pCmd = new CCommand(RIL_CHANNEL_SIM,
                                      NULL,
                                      ND_REQ_ID_NONE,
                                      "AT+CPBS=\"SM\";+CPBR=?\r");
        if (pCmd)
        {
            pCmd->SetHighPriority();
            CContext* pContext = new CContextSimPhonebookQuery(*m_pSimInitializationEvent, bResult);
            pCmd->SetContext(pContext);
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("InitializeSimPhonebook() - ERROR: Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Done;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("InitializeSimPhonebook() - ERROR: Unable to allocate memory for command\r\n");
            goto Done;
        }

        // block until we get notified
        uiWaitRes = CEvent::Wait(m_pSimInitializationEvent, iTimeout);

        if (m_fAbortSimInitialization)
            break;

        if ((WAIT_EVENT_0_SIGNALED == uiWaitRes) && (bResult))
        {
            // response received, bail out
            RIL_LOG_INFO("InitializeSimPhonebook() : PHONEBOOK Unlocked!\r\n");
            break;
        }
        else
        {
            // sleep before the next try
            const UINT32 uiSIMPBSleep = 1000;
            RIL_LOG_VERBOSE("InitializeSimPhonebook() : BEGIN Sleep [%d]\r\n", uiSIMPBSleep);
            Sleep(uiSIMPBSleep);
            RIL_LOG_VERBOSE("InitializeSimPhonebook() : END Sleep\r\n");
        }
    }

    if (m_fAbortSimInitialization)
    {
        goto Done;
    }

    g_RadioState.SetRadioSIMPBReady();
    fRet = TRUE;

Done:
    RIL_LOG_VERBOSE("InitializeSimPhonebook() : Exit\r\n");
    return fRet;
}

void CSystemManager::TriggerInitStringCompleteEvent(EnumRilChannel eChannel, eComInitIndex eInitIndex)
{
    SetChannelCompletedInit(eChannel, eInitIndex);
    
    if (VerifyAllChannelsCompletedInit(COM_READY_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("TriggerInitStringCompleteEvent() - DEBUG: All channels complete ready init!\r\n");
        CEvent::Signal(m_pInitStringCompleteEvent);
    }
    else if (VerifyAllChannelsCompletedInit(COM_UNLOCK_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("TriggerInitStringCompleteEvent() - DEBUG: All channels complete unlock init!\r\n");
        m_fModemUnlockInitComplete = TRUE;
    }
    else if (VerifyAllChannelsCompletedInit(COM_BASIC_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("TriggerInitStringCompleteEvent() - DEBUG: All channels complete basic init!\r\n");
    }
    else if (VerifyAllChannelsCompletedInit(COM_POWER_ON_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("TriggerInitStringCompleteEvent() - DEBUG: All channels complete power on init!\r\n");
    }
    else
    {
        RIL_LOG_VERBOSE("TriggerInitStringCompleteEvent() - DEBUG: Channel [%d] complete! Still waiting for other channels to complete index [%d]!\r\n", eChannel, eInitIndex);
    }
}

BOOL CSystemManager::BlockNonHighPriorityCmds()
{
    if (m_fModemUnlockInitComplete && m_fSimGeneralInitComplete)
    {
        // Let all commands through
        RIL_LOG_VERBOSE("CSystemManager::BlockNonHighPriorityCmds() - returning FALSE\r\n");
        return false;
    }
    else
    {
        // Block until the minimum initialization is complete
        RIL_LOG_VERBOSE("BlockNonHighPriorityCmds() - DEBUG: ModemUnlock=[%s]   SimInit=[%s]  returning TRUE\r\n", 
            m_fModemUnlockInitComplete ? "TRUE" : "FALSE", 
            m_fSimGeneralInitComplete ? "TRUE" : "FALSE");

        return true;
    }
}

BOOL CSystemManager::ResumeSystemFromFlightMode()
{
    return InitializeSim();
}
