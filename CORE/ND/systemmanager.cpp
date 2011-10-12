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
#include "channel_data.h"
#include "channel_DLC2.h"
#include "channel_DLC6.h"
#include "channel_DLC8.h"
#include "channel_URC.h"
#include "response.h"
#include "repository.h"
#include "te.h"
#include "rildmain.h"
#include "reset.h"
#include "systemmanager.h"

#if defined(RESET_MGMT)
#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif // RESET_MGMT

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

            //  Can't call TriggerRadioError here as SystemManager isn't even up yet.
            //  Just call exit and let rild clean everything up.
            exit(0);
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
    m_pExitRilEvent(NULL),
    m_pSimUnlockedEvent(NULL),
    m_pModemPowerOnEvent(NULL),
    m_pInitStringCompleteEvent(NULL),
    m_pSysInitCompleteEvent(NULL),
#if !defined(RESET_MGMT)
    m_pTriggerRadioErrorMutex(NULL),
#endif // !RESET_MGMT
    m_pDataChannelAccessorMutex(NULL),
#if defined(RESET_MGMT)
    m_fdCleanupSocket(-1),
#endif // RESET_MGMT
    m_RequestInfoTable(),
    m_bFailedToInitialize(FALSE)
{
    RIL_LOG_INFO("CSystemManager::CSystemManager() - Enter\r\n");

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
            RIL_LOG_INFO("CSystemManager::~CSystemManager() - DEBUG: Mutex Locked!\r\n");
            fLocked = TRUE;
            break;
        }
    }

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] after Lock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));


    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before signal m_pExitRilEvent\r\n");
    // signal the cancel event to kill the thread
    CEvent::Signal(m_pExitRilEvent);

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before CloseChannelPorts\r\n");
    //  Close the COM ports
    CloseChannelPorts();

    Sleep(300);

    //  Delete channels
    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before DeleteChannels\r\n");
    // free queues
    DeleteChannels();

    Sleep(300);

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before CThreadManager::Stop\r\n");
    CThreadManager::Stop();

    Sleep(300);

    // destroy events
    if (m_pExitRilEvent)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pExitRilEvent\r\n");
        delete m_pExitRilEvent;
        m_pExitRilEvent = NULL;
    }

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before DeleteQueues\r\n");
    // free queues
    DeleteQueues();

    Sleep(300);

    if (m_pSimUnlockedEvent)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pSimUnlockedEvent\r\n");
        delete m_pSimUnlockedEvent;
        m_pSimUnlockedEvent = NULL;
    }

    if (m_pModemPowerOnEvent)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pModemPowerOnEvent\r\n");
        delete m_pModemPowerOnEvent;
        m_pModemPowerOnEvent = NULL;
    }

    if (m_pInitStringCompleteEvent)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pInitStringCompleteEvent\r\n");
        delete m_pInitStringCompleteEvent;
        m_pInitStringCompleteEvent = NULL;
    }

    if (m_pSysInitCompleteEvent)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pSysInitCompleteEvent\r\n");
        delete m_pSysInitCompleteEvent;
        m_pSysInitCompleteEvent = NULL;
    }

#if !defined(RESET_MGMT)
    if (m_pTriggerRadioErrorMutex)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pTriggerRadioErrorMutex\r\n");
        delete m_pTriggerRadioErrorMutex;
        m_pTriggerRadioErrorMutex = NULL;
    }
#endif // !RESET_MGMT

    if (m_pDataChannelAccessorMutex)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pDataChannelAccessorMutex\r\n");
        delete m_pDataChannelAccessorMutex;
        m_pDataChannelAccessorMutex = NULL;
    }

#if defined(RESET_MGMT)
    if (m_fdCleanupSocket >= 0)
    {
        shutdown(m_fdCleanupSocket, SHUT_RDWR);
        close(m_fdCleanupSocket);
        m_fdCleanupSocket = -1;
    }
#endif // RESET_MGMT

    if (fLocked)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] before Unlock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));

        CMutex::Unlock(m_pSystemManagerMutex);

        RIL_LOG_INFO("CSystemManager::~CSystemManager() - INFO: GetLockValue=[%d] after Unlock\r\n", CMutex::GetLockValue(m_pSystemManagerMutex));
    }

    if (m_pSystemManagerMutex)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pSystemManagerMutex\r\n");
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


    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutCmdInit, iTemp))
    {
        g_TimeoutCmdInit = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutAPIDefault, iTemp))
    {
        g_TimeoutAPIDefault = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutWaitForInit, iTemp))
    {
        g_TimeoutWaitForInit = (UINT32)iTemp;
    }

    if (repository.Read(g_szGroupRILSettings, g_szTimeoutThresholdForRetry, iTemp))
    {
        g_TimeoutThresholdForRetry = (UINT32)iTemp;
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

    if (m_pSysInitCompleteEvent)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pSysInitCompleteEvent was already created!\r\n");
    }
    else
    {
        m_pSysInitCompleteEvent = new CEvent(NULL, TRUE);
        if (!m_pSysInitCompleteEvent)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create System Init complete Event.\r\n");
            goto Done;
        }
    }

#if !defined(RESET_MGMT)
    if (m_pTriggerRadioErrorMutex)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pTriggerRadioErrorMutex was already created!\r\n");
    }
    else
    {
        m_pTriggerRadioErrorMutex = new CMutex();
        if (!m_pTriggerRadioErrorMutex)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create m_pTriggerRadioErrorMutex.\r\n");
            goto Done;
        }
    }
#endif // !RESET_MGMT

    if (m_pDataChannelAccessorMutex)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pDataChannelAccessorMutex was already created!\r\n");
    }
    else
    {
        m_pDataChannelAccessorMutex = new CMutex();
        if (!m_pDataChannelAccessorMutex)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Could not create m_pDataChannelAccessorMutex.\r\n");
            goto Done;
        }
    }

    ResetSystemState();

#if defined(RESET_MGMT)
    //  Need to open the "clean up request" socket here.
    if (!OpenCleanupRequestSocket())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Unable to create cleanup request socket\r\n");
        goto Done;
    }

    //  Launch the modem reset watchdog socket thread
    if (!CreateModemWatchdogThread())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't create modem watchdog thread!\r\n");
        goto Done;
    }

    //  Create and initialize the channels (don't open ports yet)
    if (!InitChannelPorts())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: InitChannelPorts() error!\r\n");
        goto Done;
    }
#else  // RESET_MGMT


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

    //  Check repository to see if we support the watchdog thread.
    iTemp = 0;
    if (!repository.Read(g_szGroupModem, g_szDisableWatchdogThread, iTemp))
    {
        iTemp = 0;
    }
    if (0 == iTemp)
    {
        //  support watchdog thread
        if (!CreateWatchdogThread())
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't create watchdog thread!\r\n");
            goto Done;
        }
    }
    else
    {
        //  don't support watchdog thread
        RIL_LOG_CRITICAL("******* CSystemManager::InitializeSystem() - Watchdog thread is DISABLED.\r\n");
        RIL_LOG_CRITICAL("******* Unsoliticited modem reset detection AND core dump will not function.\r\n");
    }

    if (!InitializeModem())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - ERROR: Couldn't start Modem initialization!\r\n");
        goto Done;
    }
#endif // RESET_MGMT

    bRetVal = TRUE;

Done:
    if (!bRetVal)
    {
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

        if (m_pSysInitCompleteEvent)
        {
            delete m_pSysInitCompleteEvent;
            m_pSysInitCompleteEvent = NULL;
        }

#if !defined(RESET_MGMT)
        if (m_pTriggerRadioErrorMutex)
        {
            delete m_pTriggerRadioErrorMutex;
            m_pTriggerRadioErrorMutex = NULL;
        }
#endif // !RESET_MGMT

        if (m_pDataChannelAccessorMutex)
        {
            delete m_pDataChannelAccessorMutex;
            m_pDataChannelAccessorMutex = NULL;
        }

#if defined(RESET_MGMT)
        if (m_fdCleanupSocket >= 0)
        {
            shutdown(m_fdCleanupSocket, SHUT_RDWR);
            close(m_fdCleanupSocket);
            m_fdCleanupSocket = -1;
        }
#else  // RESET_MGMT

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
#endif // RESET_MGMT
    }


    CMutex::Unlock(m_pSystemManagerMutex);

    if(bRetVal)
    {
        CEvent::Wait(m_pSysInitCompleteEvent, WAIT_FOREVER);
        RIL_LOG_INFO("rapid ril initialization completed\r\n");
    }

    RIL_LOG_INFO("CSystemManager::InitializeSystem() - Exit\r\n");

    return bRetVal;
}


#if defined(RESET_MGMT)
///////////////////////////////////////////////////////////////////////////////
//  This function continues the init in the function InitializeSystem() left
//  off from InitChannelPorts().  Called when MODEM_UP status is received.
BOOL CSystemManager::ContinueInit()
{
    RIL_LOG_INFO("CSystemManager::ContinueInit() - ENTER\r\n");

    BOOL bRetVal = FALSE;

    CMutex::Lock(m_pSystemManagerMutex);

    // Open the serial ports only (g_pRilChannel should already be populated)
    if (!OpenChannelPortsOnly())
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - ERROR: Couldn't open VSPs.\r\n");
        goto Done;
    }
    RIL_LOG_INFO("CSystemManager::ContinueInit() - VSPs were opened successfully.\r\n");

    m_pExitRilEvent = new CEvent(NULL, TRUE);
    if (NULL == m_pExitRilEvent)
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - ERROR: Could not create exit event.\r\n");
        goto Done;
    }

    // Create the Queues
    if (!CreateQueues())
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - ERROR: Unable to create queues\r\n");
        goto Done;
    }

    if (!CThreadManager::Start(RIL_CHANNEL_MAX * 2, m_pExitRilEvent))
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - ERROR: Thread manager failed to start.\r\n");
    }

    if (!InitializeModem())
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - ERROR: Couldn't start Modem initialization!\r\n");
        goto Done;
    }

    bRetVal = TRUE;
    //Signal that we have initialized, so that framework
    // can start using the rild socket.
    CEvent::Signal(m_pSysInitCompleteEvent);
Done:
    if (!bRetVal)
    {
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

        if (m_pSysInitCompleteEvent)
        {
            delete m_pSysInitCompleteEvent;
            m_pSysInitCompleteEvent = NULL;
        }

        if (m_pDataChannelAccessorMutex)
        {
            delete m_pDataChannelAccessorMutex;
            m_pDataChannelAccessorMutex = NULL;
        }

        if (m_fdCleanupSocket >= 0)
        {
            shutdown(m_fdCleanupSocket, SHUT_RDWR);
            close(m_fdCleanupSocket);
            m_fdCleanupSocket = -1;
        }

        CThreadManager::Stop();

        if (m_pExitRilEvent)
        {
            if (CEvent::Signal(m_pExitRilEvent))
            {
                RIL_LOG_INFO("CSystemManager::ContinueInit() : INFO : Signaled m_pExitRilEvent as we are failing out, sleeping for 1 second\r\n");
                Sleep(1000);
                RIL_LOG_INFO("CSystemManager::ContinueInit() : INFO : Sleep complete\r\n");
            }

            delete m_pExitRilEvent;
            m_pExitRilEvent = NULL;
        }
    }


    CMutex::Unlock(m_pSystemManagerMutex);

    return bRetVal;
    RIL_LOG_INFO("CSystemManager::ContinueInit() - EXIT\r\n");
}
#endif // RESET_MGMT

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::VerifyAllChannelsCompletedInit(eComInitIndex eInitIndex)
{
    BOOL bRetVal = TRUE;

    for (int i=0; i < RIL_CHANNEL_MAX; i++)
    {
        if (!IsChannelCompletedInit(i, eInitIndex))
        {
            bRetVal = FALSE;
            break;
        }
    }

    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::SetChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex)
{
    if ((uiChannel < RIL_CHANNEL_MAX) && (eInitIndex < COM_MAX_INDEX))
    {
        m_rgfChannelCompletedInit[uiChannel][eInitIndex] = TRUE;
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SetChannelCompletedInit() - ERROR: Invalid channel [%d] or init index [%d]\r\n", uiChannel, eInitIndex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::IsChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex)
{
    if ((uiChannel < RIL_CHANNEL_MAX) && (eInitIndex < COM_MAX_INDEX))
    {
        return m_rgfChannelCompletedInit[uiChannel][eInitIndex];
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::IsChannelCompletedInit() - ERROR: Invalid channel [%d] or init index [%d]\r\n", uiChannel, eInitIndex);
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
CChannel* CSystemManager::CreateChannel(UINT32 eIndex)
{
    CChannel* pChannel = NULL;

    switch(eIndex)
    {
        case RIL_CHANNEL_ATCMD:
            pChannel = new CChannel_ATCmd(eIndex);
            break;

        case RIL_CHANNEL_DLC2:
            pChannel = new CChannel_DLC2(eIndex);
            break;

        case RIL_CHANNEL_DLC6:
            pChannel = new CChannel_DLC6(eIndex);
            break;

        case RIL_CHANNEL_DLC8:
            pChannel = new CChannel_DLC8(eIndex);
            break;

        case RIL_CHANNEL_URC:
            pChannel = new CChannel_URC(eIndex);
            break;

        default:
            if (eIndex >= RIL_CHANNEL_DATA1) {
                pChannel = new CChannel_Data(eIndex);
            }
            break;
    }

    return pChannel;
}

///////////////////////////////////////////////////////////////////////////////
//  Note that OpenChannelPorts() = InitChannelPorts() + OpenChannelPortsOnly()
BOOL CSystemManager::OpenChannelPorts()
{
    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPorts() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        g_pRilChannel[i] = CreateChannel(i);
        if (!g_pRilChannel[i] || !g_pRilChannel[i]->InitChannel())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts() : Channel[%d] (0x%X) Init failed\r\n", i, (UINT32)g_pRilChannel[i]);
            goto Done;
        }

        if (!g_pRilChannel[i]->OpenPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts() : Channel[%d] OpenPort() failed\r\n", i);
            goto Done;
        }

        if (!g_pRilChannel[i]->InitPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPorts() : Channel[%d] InitPort() failed\r\n", i);
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

#if defined(RESET_MGMT)
///////////////////////////////////////////////////////////////////////////////
//  Create and initialize the channels, but don't actually open the ports.
BOOL CSystemManager::InitChannelPorts()
{
    RIL_LOG_VERBOSE("CSystemManager::InitChannelPorts() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        g_pRilChannel[i] = CreateChannel(i);
        if (!g_pRilChannel[i] || !g_pRilChannel[i]->InitChannel())
        {
            RIL_LOG_CRITICAL("CSystemManager::InitChannelPorts() : Channel[%d] (0x%X) Init failed\r\n", i, (UINT32)g_pRilChannel[i]);
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

    RIL_LOG_VERBOSE("CSystemManager::InitChannelPorts() - Exit\r\n");
    return bRet;
}
#endif // RESET_MGMT

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::OpenChannelPortsOnly()
{
    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPortsOnly() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        if (!g_pRilChannel[i]->OpenPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPortsOnly() : Channel[%d] OpenPort() failed\r\n", i);
            goto Done;
        }

        if (!g_pRilChannel[i]->InitPort())
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenChannelPortsOnly() : Channel[%d] InitPort() failed\r\n", i);
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

    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPortsOnly() - Exit\r\n");
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


    RIL_LOG_VERBOSE("CSystemManager::CloseChannelPorts() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::DeleteChannels()
{
    RIL_LOG_VERBOSE("CSystemManager::DeleteChannels() - Enter\r\n");

    for (int i=0; i<RIL_CHANNEL_MAX; i++)
    {
        if (g_pRilChannel[i])
        {
            delete g_pRilChannel[i];
            g_pRilChannel[i] = NULL;
        }
    }
    RIL_LOG_VERBOSE("CSystemManager::DeleteChannels() - Exit\r\n");
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

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::InitializeModem()
{
    BOOL bRetVal = TRUE;
    CThread* pModemThread = NULL;

    if (!SendModemInitCommands(COM_BASIC_INIT_INDEX))
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeModem() - ERROR: Unable to send basic init commands!\r\n");
        goto Done;
    }

    pModemThread = new CThread(StartModemInitializationThreadWrapper, this, THREAD_FLAGS_NONE, 0);
    if (!pModemThread || !CThread::IsRunning(pModemThread))
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeModem() - ERROR: Unable to launch modem init thread\r\n");
        bRetVal = FALSE;
    }

    delete pModemThread;
    pModemThread = NULL;

Done:
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::StartModemInitializationThread()
{
    RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() : Start Modem initialization thread\r\n");
    BOOL fUnlocked = FALSE;
    BOOL fPowerOn = FALSE;
    UINT32 uiNumEvents = 0;

    while (!fUnlocked || !fPowerOn)
    {
        UINT32 ret = 0;

        if (!fUnlocked && !fPowerOn)
        {
            RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Waiting for unlock, power on or cancel\r\n");
            CEvent* rgpEvents[] = { m_pSimUnlockedEvent, m_pModemPowerOnEvent, m_pExitRilEvent };
            uiNumEvents = 3;
            ret = CEvent::WaitForAnyEvent(uiNumEvents, rgpEvents, WAIT_FOREVER);
        }
        else if (fUnlocked)
        {
            RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Waiting for power on or cancel\r\n");
            CEvent* rgpEvents[] = { m_pModemPowerOnEvent, m_pExitRilEvent };
            uiNumEvents = 2;
            ret = CEvent::WaitForAnyEvent(uiNumEvents, rgpEvents, WAIT_FOREVER);
        }
        else
        {
            RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Waiting for unlock or cancel\r\n");
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
                    RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Unlocked signaled\r\n");

                    if (!SendModemInitCommands(COM_UNLOCK_INIT_INDEX))
                    {
                        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Unable to send unlock init commands!\r\n");
                        goto Done;
                    }

                    fUnlocked = true;
                    break;
                }

                case WAIT_EVENT_0_SIGNALED + 1:
                {
                    RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Power on signaled\r\n");

                    if (!SendModemInitCommands(COM_POWER_ON_INIT_INDEX))
                    {
                        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Unable to send power on init commands!\r\n");
                        goto Done;
                    }

                    fPowerOn = true;
                    break;
                }
                case WAIT_EVENT_0_SIGNALED + 2:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Failed waiting for events!\r\n");
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
                        RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Power on signaled\r\n");

                        if (!SendModemInitCommands(COM_POWER_ON_INIT_INDEX))
                        {
                            RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Unable to send power on init commands!\r\n");
                            goto Done;
                        }

                        fPowerOn = true;
                    }
                    else
                    {
                        RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Unlocked signaled\r\n");

                        if (!SendModemInitCommands(COM_UNLOCK_INIT_INDEX))
                        {
                            RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Unable to send unlock init commands!\r\n");
                            goto Done;
                        }

                        fUnlocked = true;
                    }
                    break;

                case WAIT_EVENT_0_SIGNALED + 1:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Failed waiting for events!\r\n");
                    goto Done;
                    break;
            }
        }
    }

    if (!SendModemInitCommands(COM_READY_INIT_INDEX))
    {
        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Unable to send ready init commands!\r\n");
        goto Done;
    }

    {
        CEvent* rgpEvents[] = { m_pInitStringCompleteEvent, m_pExitRilEvent };

        switch(CEvent::WaitForAnyEvent(2, rgpEvents, WAIT_FOREVER))
        {
            case WAIT_EVENT_0_SIGNALED:
            {
                RIL_LOG_INFO("CSystemManager::StartModemInitializationThread() - INFO: Initialization strings complete\r\n");
                goto Done;
                break;
            }

            case WAIT_EVENT_0_SIGNALED + 1:
            default:
            {
                RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - ERROR: Exiting ril!\r\n");
                goto Done;
                break;
            }
        }
    }

Done:
    RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() : Modem initialized, thread exiting\r\n");
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::SendModemInitCommands(eComInitIndex eInitIndex)
{
    RIL_LOG_VERBOSE("CSystemManager::SendModemInitCommands() - Enter\r\n");

    for (int i = 0; i < RIL_CHANNEL_MAX; i++)
    {
        extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];

        if (g_pRilChannel[i])
        {
            if (!g_pRilChannel[i]->SendModemConfigurationCommands(eInitIndex))
            {
                RIL_LOG_CRITICAL("CSystemManager::SendModemInitCommands() : ERROR : Channel=[%d] returned ERROR\r\n", i);
                return FALSE;
            }
        }
    }

    RIL_LOG_VERBOSE("CSystemManager::SendModemInitCommands() - Exit\r\n");
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
void CSystemManager::TriggerInitStringCompleteEvent(UINT32 uiChannel, eComInitIndex eInitIndex)
{
    SetChannelCompletedInit(uiChannel, eInitIndex);

    if (VerifyAllChannelsCompletedInit(COM_READY_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("CSystemManager::TriggerInitStringCompleteEvent() - DEBUG: All channels complete ready init!\r\n");
        CEvent::Signal(m_pInitStringCompleteEvent);
    }
    else if (VerifyAllChannelsCompletedInit(COM_UNLOCK_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("CSystemManager::TriggerInitStringCompleteEvent() - DEBUG: All channels complete unlock init!\r\n");
    }
    else if (VerifyAllChannelsCompletedInit(COM_BASIC_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("CSystemManager::TriggerInitStringCompleteEvent() - DEBUG: All channels complete basic init!\r\n");
    }
    else if (VerifyAllChannelsCompletedInit(COM_POWER_ON_INIT_INDEX))
    {
        RIL_LOG_VERBOSE("CSystemManager::TriggerInitStringCompleteEvent() - DEBUG: All channels complete power on init!\r\n");
    }
    else
    {
        RIL_LOG_VERBOSE("CSystemManager::TriggerInitStringCompleteEvent() - DEBUG: Channel [%d] complete! Still waiting for other channels to complete index [%d]!\r\n", uiChannel, eInitIndex);
    }
}

///////////////////////////////////////////////////////////////////////////////
#if defined(RESET_MGMT)
//  This function opens clean-up request socket.
//  The fd of this socket is stored in the CSystemManager class.
BOOL CSystemManager::OpenCleanupRequestSocket()
{
    RIL_LOG_INFO("CSystemManager::OpenCleanupRequestSocket() - ENTER\r\n");

    BOOL bRet = FALSE;
    const int NUM_LOOPS = 10;
    const int SLEEP_MS = 1000;  // 1 sec between retries
    //  TODO: Change looping formula

    for (int i = 0; i < NUM_LOOPS; i++)
    {
        RIL_LOG_INFO("CSystemManager::OpenCleanupRequestSocket() - Attempting open cleanup socket try=[%d] out of %d\r\n", i+1, NUM_LOOPS);
        m_fdCleanupSocket = socket_local_client(SOCKET_NAME_CLEAN_UP,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_STREAM);

        if (m_fdCleanupSocket < 0)
        {
            RIL_LOG_CRITICAL("CSystemManager::OpenCleanupRequestSocket() - ERROR: Cannot open m_fdCleanupSocket\r\n");
            Sleep(SLEEP_MS);
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::OpenCleanupRequestSocket() - *** CREATED socket fd=[%d] ***\r\n", m_fdCleanupSocket);
            break;
        }
    }

    if (m_fdCleanupSocket < 0)
        bRet = FALSE;
    else
        bRet = TRUE;

    RIL_LOG_INFO("CSystemManager::OpenCleanupRequestSocket() - EXIT\r\n");
    return bRet;
}


//  Send clean up request on the socket
BOOL CSystemManager::SendRequestCleanup()
{
    RIL_LOG_INFO("CSystemManager::SendRequestCleanup() - ENTER\r\n");
    BOOL bRet = FALSE;

    if (m_fdCleanupSocket >= 0)
    {
        unsigned int data;
        int data_size = 0;

        RIL_LOG_INFO("CSystemManager::SendRequestCleanup() - Send request clean up\r\n");
        data = REQUEST_CLEANUP;
        data_size = send(m_fdCleanupSocket, &data, sizeof(unsigned int), 0);
        if (data_size < 0)
        {
            RIL_LOG_CRITICAL("CSystemManager::SendRequestCleanup() - Failed to send CLEANUP_REQUEST\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendRequestCleanup() - Send request clean up  SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendRequestCleanup() - invalid socket fd=[%d]\r\n", m_fdCleanupSocket);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendRequestCleanup() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::SendRequestShutdown()
{
    RIL_LOG_INFO("CSystemManager::SendRequestShutdown() - ENTER\r\n");
    BOOL bRet = FALSE;

    if (m_fdCleanupSocket >= 0)
    {
        unsigned int data;
        int data_size = 0;

        RIL_LOG_INFO("CSystemManager::SendRequestShutdown() - Send request shutdown\r\n");
        data = REQUEST_SHUTDOWN;
        data_size = send(m_fdCleanupSocket, &data, sizeof(unsigned int), 0);
        if (data_size < 0)
        {
            RIL_LOG_CRITICAL("CSystemManager::SendRequestShutdown() - Failed to send CLEANUP_SHUTDOWN\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendRequestShutdown() - Send request shutdown  SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendRequestShutdown() - invalid socket fd=[%d]\r\n", m_fdCleanupSocket);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendRequestShutdown() - EXIT\r\n");
    return bRet;
}
#endif // RESET_MGMT


