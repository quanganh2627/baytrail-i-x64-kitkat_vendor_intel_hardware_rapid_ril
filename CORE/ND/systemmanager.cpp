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
 */


#include "types.h"
#include "rillog.h"
#include "util.h"
#include "sync_ops.h"
#include "thread_ops.h"
#include "rilqueue.h"
#include "thread_manager.h"
#include "cmdcontext.h"
#include "rilchannels.h"
#include "channel_atcmd.h"
#include "channel_data.h"
#include "channel_DLC2.h"
#include "channel_DLC6.h"
#include "channel_DLC8.h"
#include "channel_URC.h"
#include "channel_OEM.h"
#include "response.h"
#include "repository.h"
#include "te.h"
#include "rildmain.h"
#include "mmgr_cli.h"
#include "reset.h"
#include "systemmanager.h"

#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// Tx Queue
CRilQueue<CCommand*>* g_pTxQueue[RIL_CHANNEL_MAX];
CEvent* g_TxQueueEvent[RIL_CHANNEL_MAX];

// Rx Queue
CRilQueue<CResponse*>* g_pRxQueue[RIL_CHANNEL_MAX];
CEvent* g_RxQueueEvent[RIL_CHANNEL_MAX];

//  Array of CChannels
CChannel* g_pRilChannel[RIL_CHANNEL_MAX] = { NULL };

// used for 6360 and 7160 modems
int m_hsiChannelsReservedForClass1 = -1;
int m_hsiDataDirect = -1;
int m_dataProfilePathAssignation[NUMBER_OF_APN_PROFILE] = { NULL };


CSystemManager* CSystemManager::m_pInstance = NULL;


CSystemManager& CSystemManager::GetInstance()
{
    //RIL_LOG_VERBOSE("CSystemManager::GetInstance() - Enter\r\n");
    if (!m_pInstance)
    {
        m_pInstance = new CSystemManager;
        if (!m_pInstance)
        {
            RIL_LOG_CRITICAL("CSystemManager::GetInstance() - Cannot create instance\r\n");

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
    m_pDataChannelAccessorMutex(NULL),
    m_pMMgrLibHandle(NULL),
    m_RequestInfoTable(),
    m_bFailedToInitialize(FALSE)
#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)
    ,m_uiLastCallFailedCauseID(0)
#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED
{
    RIL_LOG_INFO("CSystemManager::CSystemManager() - Enter\r\n");

    // TODO / FIXME : Someone is locking this mutex outside of the destructor or system init functions
    //                Need to track down when time is available. Workaround for now is to call TryLock
    //                so we don't block during suspend.
    m_pSystemManagerMutex = new CMutex();

    memset(m_szDualSim, 0, PROPERTY_VALUE_MAX);

    m_pSpoofCommandsStatusAccessMutex = new CMutex();

    m_pTEAccessMutex = new CMutex();

    RIL_LOG_INFO("CSystemManager::CSystemManager() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CSystemManager::~CSystemManager()
{
    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Enter\r\n");
    BOOL fLocked = TRUE;

    for (int x = 0; x < 3; x++)
    {
        Sleep(300);

        if (!CMutex::TryLock(m_pSystemManagerMutex))
        {
            RIL_LOG_CRITICAL("CSystemManager::~CSystemManager() - Failed to lock mutex!\r\n");
            fLocked = FALSE;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::~CSystemManager() - DEBUG: Mutex Locked!\r\n");
            fLocked = TRUE;
            break;
        }
    }

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

    if (m_pDataChannelAccessorMutex)
    {
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pDataChannelAccessorMutex\r\n");
        delete m_pDataChannelAccessorMutex;
        m_pDataChannelAccessorMutex = NULL;
    }

    RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete TE object\r\n");
    CTE::GetTE().DeleteTEObject();

    if (m_pSpoofCommandsStatusAccessMutex)
    {
        CMutex::Unlock(m_pSpoofCommandsStatusAccessMutex);
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - "
                "Before delete m_pSpoofCommandsStatusAccessMutex\r\n");
        delete m_pSpoofCommandsStatusAccessMutex;
        m_pSpoofCommandsStatusAccessMutex = NULL;
    }

    if (m_pTEAccessMutex)
    {
        CMutex::Unlock(m_pTEAccessMutex);
        RIL_LOG_INFO("CSystemManager::~CSystemManager() - Before delete m_pTEAccessMutex\r\n");
        delete m_pTEAccessMutex;
        m_pTEAccessMutex = NULL;
    }

    if (fLocked)
    {
        CMutex::Unlock(m_pSystemManagerMutex);
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

    char szModem[MAX_MODEM_NAME_LEN];
    UINT32 uiModemType = MODEM_TYPE_UNKNOWN;
    char szModemState[PROPERTY_VALUE_MAX] = "0";

    // read the modem type used from repository
    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260))
        {
            RIL_LOG_INFO("CSystemManager::InitializeSystem() - Using Infineon 6260\r\n");
            uiModemType = MODEM_TYPE_XMM6260;
        }
        else if (0 == strcmp(szModem, szXMM6360))
        {
            RIL_LOG_INFO("CSystemManager::InitializeSystem() - Using Infineon 6360\r\n");
            uiModemType = MODEM_TYPE_XMM6360;
        }
        else if (0 == strcmp(szModem, szXMM7160))
        {
            RIL_LOG_INFO("CSystemManager::InitializeSystem() - Using Infineon 7160\r\n");
            uiModemType = MODEM_TYPE_XMM7160;
        }
        else
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Unknown modem type- Calling exit(0)\r\n");
            exit(0);
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Failed to read the modem type!\r\n");
        goto Done;
    }

    if (MODEM_TYPE_XMM6360 == uiModemType
            || MODEM_TYPE_XMM7160 == uiModemType)
    {
        int apnType = 0;
        if (!repository.Read(g_szGroupModem, g_szApnTypeDefault, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type default from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[0] = apnType;
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() - ApnTypeThetered: %d...\r\n", apnType);
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeThetered, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type Tethered from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[1] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeIMS, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type IMS from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[2] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeFOTA, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type MMS from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[3] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeCBS, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type CBS from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[4] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeMMS, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type FOTA from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[5] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeSUPL, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type SUPL from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[6] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szApnTypeHIPRI, apnType))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type HIPRI from repository\r\n");
        }
        else
        {
            m_dataProfilePathAssignation[7] = apnType;
        }

        if (!repository.Read(g_szGroupModem, g_szHsiDataDirect, m_hsiDataDirect))
        {
            RIL_LOG_WARNING("CSystemManager::InitializeSystem() : Could not read network apn type default from repository\r\n");
        }

        m_hsiChannelsReservedForClass1 = 0;
        for (UINT32 i = 0; i < NUMBER_OF_APN_PROFILE; i++)
        {
            if (m_dataProfilePathAssignation[i] == 1)
            {
                m_hsiChannelsReservedForClass1++;
            }
        }

        if (m_hsiChannelsReservedForClass1 > m_hsiDataDirect)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() : Too much class1 APN\r\n");
            goto Done;
        }
        // HSI channel 0 and 1 are not used for data.
        if (m_hsiDataDirect > RIL_HSI_CHANNEL_MAX - 2)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() : Too much hsi channel reserved for data\r\n");
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
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Could not create Sim Unlocked Event.\r\n");
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
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Could not create Modem Power On Event.\r\n");
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
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Could not create Init commands complete Event.\r\n");
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
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Could not create System init complete Event.\r\n");
            goto Done;
        }
    }

    if (m_pDataChannelAccessorMutex)
    {
        RIL_LOG_WARNING("CSystemManager::InitializeSystem() - WARN: m_pDataChannelAccessorMutex was already created!\r\n");
    }
    else
    {
        m_pDataChannelAccessorMutex = new CMutex();
        if (!m_pDataChannelAccessorMutex)
        {
            RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Could not create m_pDataChannelAccessorMutex.\r\n");
            goto Done;
        }
    }
    // The modem specific TE Object is created here. This should be done before the
    // AT channels starts sending the initialization commands.
    CTE::CreateTE(uiModemType);

    ResetSystemState();

    if (repository.Read(g_szGroupModem, g_szEnableModemOffInFlightMode, iTemp))
    {
        CTE::GetTE().SetModemOffInFlightModeState((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutCmdInit, iTemp))
    {
        CTE::GetTE().SetTimeoutCmdInit((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutAPIDefault, iTemp))
    {
        CTE::GetTE().SetTimeoutAPIDefault((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutWaitForInit, iTemp))
    {
        CTE::GetTE().SetTimeoutWaitForInit((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupRILSettings, g_szTimeoutThresholdForRetry, iTemp))
    {
        CTE::GetTE().SetTimeoutThresholdForRetry((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupModem, g_szMTU, iTemp))
    {
        CTE::GetTE().SetMTU((UINT32)iTemp);
    }

    if (repository.Read(g_szGroupModem, g_szDisableUSSD, iTemp))
    {
        CTE::GetTE().SetDisableUSSD(iTemp == 1 ? TRUE : FALSE);
    }

    // store initial value of Fast Dormancy Mode
    if (repository.Read(g_szGroupModem, g_szFDMode, iTemp))
    {
        CTE::GetTE().SetFastDormancyMode((UINT32)iTemp);
    }

    //  Need to open the "clean up request" socket here.
    if (!MMgrConnectionInit())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - Unable to connect to MMgr lib\r\n");
        goto Done;
    }

    //  Create and initialize the channels (don't open ports yet)
    if (!InitChannelPorts())
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeSystem() - InitChannelPorts() error!\r\n");
        goto Done;
    }

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

        if (m_pDataChannelAccessorMutex)
        {
            delete m_pDataChannelAccessorMutex;
            m_pDataChannelAccessorMutex = NULL;
        }

        if (m_pMMgrLibHandle)
        {
            mmgr_cli_disconnect(m_pMMgrLibHandle);
            Sleep(300);
            mmgr_cli_delete_handle(m_pMMgrLibHandle);
            delete m_pMMgrLibHandle;
            m_pMMgrLibHandle = NULL;
        }

        CTE::GetTE().DeleteTEObject();
    }

    CMutex::Unlock(m_pSystemManagerMutex);

    if (bRetVal)
    {
        RIL_LOG_INFO("CSystemManager::InitializeSystem() :"
                        " Now waiting for modem initialization....\r\n");
        if (CTE::GetTE().GetModemOffInFlightModeState())
        {
            property_get("persist.radio.ril_modem_state", szModemState , "1");
            if (strncmp(szModemState, "0", 1) == 0)
            {
                // Flightmode enabled
                CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_OFF);
            }
            else
            {
                if (E_MMGR_EVENT_MODEM_DOWN == CTE::GetTE().GetLastModemEvent())
                {
                    // Modem reset or plateform boot
                    CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_UNAVAILABLE);
                }
                else
                {
                    // This is unlikely
                    CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_OFF);
                }
            }
        }
        else
        {
            GetModem();
            CEvent::Wait(m_pSysInitCompleteEvent, WAIT_FOREVER);
        }

        RIL_LOG_INFO("CSystemManager::InitializeSystem() : Rapid Ril initialization completed\r\n");
    }

    RIL_LOG_INFO("CSystemManager::InitializeSystem() - Exit\r\n");

    return bRetVal;
}


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
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - Couldn't open VSPs.\r\n");
        goto Done;
    }
    RIL_LOG_INFO("CSystemManager::ContinueInit() - VSPs were opened successfully.\r\n");

    m_pExitRilEvent = new CEvent(NULL, TRUE);
    if (NULL == m_pExitRilEvent)
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - Could not create exit event.\r\n");
        goto Done;
    }

    // Create the Queues
    if (!CreateQueues())
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - Unable to create queues\r\n");
        goto Done;
    }

    if (!CThreadManager::Start(g_uiRilChannelCurMax * 2))
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - Thread manager failed to start.\r\n");
    }

    if (!InitializeModem())
    {
        RIL_LOG_CRITICAL("CSystemManager::ContinueInit() - Couldn't start Modem initialization!\r\n");
        goto Done;
    }

    bRetVal = TRUE;

    if (CTE::GetTE().GetModemOffInFlightModeState() &&
            (RADIO_STATE_OFF != CTE::GetTE().GetRadioState()))
    {
        CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_OFF);
    }
    // Signal that we have initialized, so that framework
    // can start using the rild socket.
    CEvent::Signal(m_pSysInitCompleteEvent);
    if (CTE::GetTE().GetModemOffInFlightModeState())
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
    }

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

        CThreadManager::Stop();

        if (m_pMMgrLibHandle)
        {
            mmgr_cli_disconnect(m_pMMgrLibHandle);
            Sleep(300);
            mmgr_cli_delete_handle(m_pMMgrLibHandle);
            delete m_pMMgrLibHandle;
            m_pMMgrLibHandle = NULL;
        }

        CTE::GetTE().DeleteTEObject();

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

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::VerifyAllChannelsCompletedInit(eComInitIndex eInitIndex)
{
    BOOL bRetVal = TRUE;

    for (UINT32 i=0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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
    if ((uiChannel < g_uiRilChannelCurMax) && (eInitIndex < COM_MAX_INDEX))
    {
        m_rgfChannelCompletedInit[uiChannel][eInitIndex] = TRUE;
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SetChannelCompletedInit() - Invalid channel [%d] or init index [%d]\r\n", uiChannel, eInitIndex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::IsChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex)
{
    if ((uiChannel < g_uiRilChannelCurMax) && (eInitIndex < COM_MAX_INDEX))
    {
        return m_rgfChannelCompletedInit[uiChannel][eInitIndex];
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::IsChannelCompletedInit() - Invalid channel [%d] or init index [%d]\r\n", uiChannel, eInitIndex);
        return FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Check for a channel type if port is correctly assigned
BOOL CSystemManager::IsChannelUndefined(int channel)
{
    switch(channel) {
        case RIL_CHANNEL_ATCMD:
            if (!g_szCmdPort)
                return true;
            break;
        case RIL_CHANNEL_DLC2:
            if (!g_szDLC2Port)
                return true;
            break;
        case RIL_CHANNEL_DLC6:
            if (!g_szDLC6Port)
                return true;
            break;
        case RIL_CHANNEL_DLC8:
            if (!g_szDLC8Port)
                return true;
            break;
        case RIL_CHANNEL_URC:
            if (!g_szURCPort)
                return true;
            break;
        case RIL_CHANNEL_OEM:
            if (!g_szOEMPort)
                return true;
            break;
        case RIL_CHANNEL_DATA1:
            if (!g_szDataPort1)
                return true;
            break;
        case RIL_CHANNEL_DATA2:
            if (!g_szDataPort2)
                return true;
            break;
        case RIL_CHANNEL_DATA3:
            if (!g_szDataPort3)
                return true;
            break;
        case RIL_CHANNEL_DATA4:
            if (!g_szDataPort4)
                return true;
            break;
        case RIL_CHANNEL_DATA5:
            if (!g_szDataPort5)
                return true;
            break;
        default: return false;
    }
    return false;
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
    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; ++i)
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

    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; ++i)
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

        case RIL_CHANNEL_OEM:
            pChannel = new CChannel_OEM(eIndex);
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
    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        if (IsChannelUndefined(i))
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

///////////////////////////////////////////////////////////////////////////////
//  Create and initialize the channels, but don't actually open the ports.
BOOL CSystemManager::InitChannelPorts()
{
    RIL_LOG_VERBOSE("CSystemManager::InitChannelPorts() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        if (IsChannelUndefined(i))
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

///////////////////////////////////////////////////////////////////////////////
BOOL CSystemManager::OpenChannelPortsOnly()
{
    RIL_LOG_VERBOSE("CSystemManager::OpenChannelPortsOnly() - Enter\r\n");

    BOOL bRet = FALSE;

    //  Init our array of global CChannel pointers.
    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        if (i == RIL_CHANNEL_RESERVED)
            continue;

        if (IsChannelUndefined(i))
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

    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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

    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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
        RIL_LOG_CRITICAL("CSystemManager::InitializeModem() - Unable to send basic init commands!\r\n");
        goto Done;
    }

    pModemThread = new CThread(StartModemInitializationThreadWrapper, this, THREAD_FLAGS_NONE, 0);
    if (!pModemThread || !CThread::IsRunning(pModemThread))
    {
        RIL_LOG_CRITICAL("CSystemManager::InitializeModem() - Unable to launch modem init thread\r\n");
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
                        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Unable to send unlock init commands!\r\n");
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
                        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Unable to send power on init commands!\r\n");
                        goto Done;
                    }

                    fPowerOn = true;
                    break;
                }
                case WAIT_EVENT_0_SIGNALED + 2:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Failed waiting for events!\r\n");
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
                            RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Unable to send power on init commands!\r\n");
                            goto Done;
                        }

                        fPowerOn = true;
                    }
                    else
                    {
                        RIL_LOG_VERBOSE("CSystemManager::StartModemInitializationThread() - DEBUG: Unlocked signaled\r\n");

                        if (!SendModemInitCommands(COM_UNLOCK_INIT_INDEX))
                        {
                            RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Unable to send unlock init commands!\r\n");
                            goto Done;
                        }

                        fUnlocked = true;
                    }
                    break;

                case WAIT_EVENT_0_SIGNALED + 1:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Exit RIL event signaled!\r\n");
                    goto Done;
                    break;

                default:
                    RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Failed waiting for events!\r\n");
                    goto Done;
                    break;
            }
        }
    }

    if (!SendModemInitCommands(COM_READY_INIT_INDEX))
    {
        RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Unable to send ready init commands!\r\n");
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
                RIL_LOG_CRITICAL("CSystemManager::StartModemInitializationThread() - Exiting ril!\r\n");
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

    for (UINT32 i = 0; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
    {
        extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];

        if (g_pRilChannel[i])
        {
            if (!g_pRilChannel[i]->SendModemConfigurationCommands(eInitIndex))
            {
                RIL_LOG_CRITICAL("CSystemManager::SendModemInitCommands() : Channel=[%d] returned ERROR\r\n", i);
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
//  This function opens clean-up request socket.
//  The fd of this socket is stored in the CSystemManager class.
BOOL CSystemManager::MMgrConnectionInit()
{
    RIL_LOG_INFO("CSystemManager::MMgrConnectionInit() - ENTER\r\n");

    BOOL bRet = FALSE;
    const int NUM_LOOPS = 10;
    const int SLEEP_MS = 1000;  // 1 sec between retries

    char RRIL_NAME[CLIENT_NAME_LEN] = "RRIL";

    if (g_szSIMID)
    {
        RRIL_NAME[4] = g_szSIMID[0];
        RRIL_NAME[5] = '\0';
    }

    if (E_ERR_CLI_SUCCEED != mmgr_cli_create_handle(&m_pMMgrLibHandle, RRIL_NAME, NULL))
    {
        m_pMMgrLibHandle = NULL;
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot create handle\n");
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_EVENT_MODEM_UP))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe event %d\n",
                          E_MMGR_EVENT_MODEM_UP);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_EVENT_MODEM_DOWN))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe event %d\n",
                          E_MMGR_EVENT_MODEM_UP);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_EVENT_MODEM_OUT_OF_SERVICE))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe event %d\n",
                          E_MMGR_EVENT_MODEM_OUT_OF_SERVICE);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_NOTIFY_MODEM_WARM_RESET))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe notification %d\n",
                          E_MMGR_NOTIFY_MODEM_WARM_RESET);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_NOTIFY_MODEM_COLD_RESET))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe notification %d\n",
                          E_MMGR_NOTIFY_MODEM_COLD_RESET);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_NOTIFY_MODEM_SHUTDOWN))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe notification %d\n",
                          E_MMGR_NOTIFY_MODEM_SHUTDOWN);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_NOTIFY_PLATFORM_REBOOT))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe notification %d\n",
                          E_MMGR_NOTIFY_PLATFORM_REBOOT);
        goto out;
    }

    if (E_ERR_CLI_SUCCEED !=
          mmgr_cli_subscribe_event(m_pMMgrLibHandle,
                                     ModemManagerEventHandler,
                                     E_MMGR_NOTIFY_CORE_DUMP))
    {
        RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() -"
                         " Cannot subscribe notification %d\n",
                          E_MMGR_NOTIFY_CORE_DUMP);
        goto out;
    }

    //  TODO: Change looping formula

    for (int i = 0; i < NUM_LOOPS; i++)
    {
        RIL_LOG_INFO("CSystemManager::MMgrConnectionInit() -"
                     " Attempting to connect to MMgr try=[%d] out of %d\r\n",
                       i+1,
                       NUM_LOOPS);

        if (E_ERR_CLI_SUCCEED != mmgr_cli_connect(m_pMMgrLibHandle))
        {
            RIL_LOG_CRITICAL("CSystemManager::MMgrConnectionInit() "
                             "- Cannot connect to MMgr\r\n");
            Sleep(SLEEP_MS);
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::MMgrConnectionInit() -"
                         " *** Connection opened ***\r\n");
            bRet = TRUE;
            break;
        }
    }

out:
    if (!bRet && (m_pMMgrLibHandle != NULL))
    {
        mmgr_cli_delete_handle(m_pMMgrLibHandle);
        m_pMMgrLibHandle = NULL;
    }

    RIL_LOG_INFO("CSystemManager::MMgrConnectionInit() - EXIT\r\n");
    return bRet;
}


//  Send clean up request on the socket
BOOL CSystemManager::SendRequestModemRecovery()
{
    RIL_LOG_INFO("CSystemManager::SendRequestModemRecovery() - ENTER\r\n");
    BOOL bRet = FALSE;
    mmgr_cli_requests_t request;
    request.id = E_MMGR_REQUEST_MODEM_RECOVERY;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::SendRequestModemRecovery() -"
                     " Send request clean up\r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_send_msg(m_pMMgrLibHandle, &request))
        {
            RIL_LOG_CRITICAL("CSystemManager::SendRequestModemRecovery() -"
                             " Failed to send REQUEST_MODEM_RECOVERY\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendRequestModemRecovery() -"
                         " Send request clean up  SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendRequestModemRecovery() -"
                         " unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendRequestModemRecovery() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::SendRequestModemShutdown()
{
    RIL_LOG_INFO("CSystemManager::SendRequestModemShutdown() - ENTER\r\n");
    BOOL bRet = FALSE;
    mmgr_cli_requests_t request;
    request.id = E_MMGR_REQUEST_FORCE_MODEM_SHUTDOWN;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::SendRequestModemShutdown() -"
                     " Send request modem force shutdown\r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_send_msg(m_pMMgrLibHandle, &request))
        {
            RIL_LOG_CRITICAL("CSystemManager::SendRequestModemShutdown() -"
                             " Failed to send REQUEST_FORCE_MODEM_SHUTDOWN\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendRequestModemShutdown() -"
                         " Send request modem force shutdown SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendRequestModemShutdown() -"
                         " unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendRequestModemShutdown() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::SendAckModemShutdown()
{
    RIL_LOG_INFO("CSystemManager::SendAckModemShutdown() - ENTER\r\n");
    BOOL bRet = FALSE;
    mmgr_cli_requests_t request;
    request.id = E_MMGR_ACK_MODEM_SHUTDOWN;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::SendAckModemShutdown() -"
                     " Acknowledging modem force shutdown\r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_send_msg(m_pMMgrLibHandle, &request))
        {
            RIL_LOG_CRITICAL("CSystemManager::SendAckModemShutdown() -"
                             " Failed to send REQUEST_ACK_MODEM_SHUTDOWN\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendAckModemShutdown() -"
                         " Modem force shutdown acknowledge SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendAckModemShutdown() -"
                         " unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendAckModemShutdown() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::SendAckModemColdReset()
{
    RIL_LOG_INFO("CSystemManager::SendAckModemColdReset() - ENTER\r\n");
    BOOL bRet = FALSE;
    mmgr_cli_requests_t request;
    request.id = E_MMGR_ACK_MODEM_COLD_RESET;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::SendAckModemColdReset() -"
                     " Acknowledging cold reset \r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_send_msg(m_pMMgrLibHandle, &request))
        {
            RIL_LOG_CRITICAL("CSystemManager::SendAckModemColdReset() -"
                             " Failed to send ACK_MODEM_COLD_RESET\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::SendAckModemColdReset() -"
                         " Cold reset acknowledge SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::SendAckModemColdReset() -"
                         " unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::SendAckModemColdReset() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::GetModem()
{
    RIL_LOG_INFO("CSystemManager::GetModem() - ENTER\r\n");
    BOOL bRet = FALSE;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::GetModem() - Getting modem resource\r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_lock(m_pMMgrLibHandle))
        {
            RIL_LOG_CRITICAL("CSystemManager::GetModem() - Failed to get modem resource\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::GetModem() - Modem resource get SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::GetModem() - unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::GetModem() - EXIT\r\n");
    return bRet;
}

//  Send shutdown request on the socket
BOOL CSystemManager::ReleaseModem()
{
    RIL_LOG_INFO("CSystemManager::ReleaseModem() - ENTER\r\n");
    BOOL bRet = FALSE;

    if (m_pMMgrLibHandle)
    {
        RIL_LOG_INFO("CSystemManager::ReleaseModem() - Releasing modem resource\r\n");

        if (E_ERR_CLI_SUCCEED != mmgr_cli_unlock(m_pMMgrLibHandle))
        {
            RIL_LOG_CRITICAL("CSystemManager::ReleaseModem() - Modem resource release failed\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CSystemManager::ReleaseModem() - Modem resource release SUCCESSFUL\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CSystemManager::ReleaseModem() - unable to communicate with MMgr\r\n");
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSystemManager::ReleaseModem() - EXIT\r\n");
    return bRet;
}


void CSystemManager::CompleteIdenticalRequests(UINT32 uiChannelId, UINT32 uiReqID,
                                                UINT32 uiResultCode,
                                                void* pResponse,
                                                size_t responseLen)
{
    RIL_LOG_VERBOSE("CSystemManager::CompleteIdenticalRequests() - Enter\r\n");

    if (uiChannelId < RIL_CHANNEL_MAX)
    {
        CChannel* pChannel = g_pRilChannel[uiChannelId];
        if (NULL != pChannel)
        {
            pChannel->FindIdenticalRequestsAndSendResponses(uiReqID, uiResultCode,
                    pResponse, responseLen);
        }
    }
    RIL_LOG_VERBOSE("CSystemManager::CompleteIdenticalRequests() - Exit\r\n");
}
