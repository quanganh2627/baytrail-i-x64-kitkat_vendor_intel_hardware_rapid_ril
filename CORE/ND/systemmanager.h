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


#ifndef RRIL_SYSTEMMANAGER_H
#define RRIL_SYSTEMMANAGER_H

#include "types.h"
#include "request_id.h"
#include "request_info_table.h"
#include "rilqueue.h"
#include "thread_manager.h"
#include "rilchannels.h"
#include "com_init_index.h"
#include <cutils/properties.h>
#include "mmgr_cli.h"

class CCommand;
class CChannel;
class CResponse;

// Queue Containers and Associated Events
extern CRilQueue<CCommand*>* g_pTxQueue[RIL_CHANNEL_MAX];
extern CRilQueue<CResponse*>* g_pRxQueue[RIL_CHANNEL_MAX];
extern CEvent* g_TxQueueEvent[RIL_CHANNEL_MAX];
extern CEvent* g_RxQueueEvent[RIL_CHANNEL_MAX];
extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];

///////////////////////////////////////////////////////////////////////////////
class CSystemManager
{
public:
    static CSystemManager& GetInstance();
    static BOOL Destroy();

private:
    CSystemManager();
    ~CSystemManager();

    //  Prevent assignment: Declared but not implemented.
    CSystemManager(const CSystemManager& rhs);  // Copy Constructor
    CSystemManager& operator=(const CSystemManager& rhs);  //  Assignment operator


public:
    // Start system initialization process
    BOOL InitializeSystem();

    BOOL IsExitRequestSignalled() const;

    //  Get/Set functions.
    static CEvent* GetCancelEvent() { return GetInstance().m_pExitRilEvent; };
    static CEvent* GetInitCompleteEvent() { return GetInstance().m_pSysInitCompleteEvent; };
    static CEvent* GetModemBasicInitCompleteEvent()
    {
        return GetInstance().m_pModemBasicInitCompleteEvent;
    }

    //  Get/Set functions.
    CEvent* GetModemPoweredOffEvent() { return m_pModemPoweredOffEvent; }

    static CMutex* GetDataChannelAccessorMutex()
    {
        return GetInstance().m_pDataChannelAccessorMutex;
    }
    static CMutex* GetTEAccessMutex() { return GetInstance().m_pTEAccessMutex; }
    static CMutex* GetSpoofCommandsStatusAccessMutex()
    {
        return GetInstance().m_pSpoofCommandsStatusAccessMutex;
    }

    void TriggerSimUnlockedEvent() const { CEvent::Signal(m_pSimUnlockedEvent); };
    void TriggerRadioPoweredOnEvent() const { CEvent::Signal(m_pRadioPoweredOnEvent); };
    void TriggerModemPoweredOffEvent() const { CEvent::Signal(m_pModemPoweredOffEvent); };
    void TriggerInitStringCompleteEvent(UINT32 eChannel, eComInitIndex eInitIndex);

    BOOL IsInitializationSuccessful() const { return m_bIsSystemInitialized; };
    void SetInitializationUnsuccessful() { m_bIsSystemInitialized = FALSE; };

    void GetRequestInfo(REQ_ID reqID, REQ_INFO& rReqInfo);

    //  For resetting modem

    /*
     * Signals the command, response thread to exit and also stops the thread before
     * closing the ports.
     */
    void CloseChannelPorts();

    BOOL OpenChannelPortsOnly();

    BOOL SendRequestModemRecovery();
    BOOL SendRequestModemShutdown();
    BOOL SendAckModemShutdown();
    BOOL SendAckModemColdReset();

    BOOL GetModem();
    BOOL ReleaseModem();

    //  This function continues the init in the function InitializeSystem() left
    //  off from InitChannelPorts().  Called when MODEM_UP status is received.
    BOOL ContinueInit();

    void ResetChannelInfo();

#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)
    void SetLastCallFailedCauseID(UINT32 nID) { m_uiLastCallFailedCauseID = nID; };
    UINT32 GetLastCallFailedCauseID() const { return m_uiLastCallFailedCauseID; };
#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED

    // Calls FindIdenticalRequestsAndSendResponses on the given channel
    static void CompleteIdenticalRequests(UINT32 uiChannelId, UINT32 uiReqID, UINT32 uiResultCode,
            void* pResponse, size_t responseLen, int callId = -1);
    char m_szDualSim[PROPERTY_VALUE_MAX];

    char* GetModemResourceName();
    int GetIpcDataChannelMin();

    int GetCancelWaitPipeFd() { return m_cancelWaitPipeFds[0]; }

private:
    // Framework Init Functions
    BOOL CreateQueues();
    void DeleteQueues();
    void DeleteChannels();
    CChannel* CreateChannel(UINT32 uiIndex);

    //  Create and initialize the channels, but don't actually open the ports.
    BOOL InitChannelPorts();

    BOOL MMgrConnectionInit();

    // Internal Init helper functions
    void SetChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex);
    BOOL IsChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex);
    BOOL VerifyAllChannelsCompletedInit(eComInitIndex eInitIndex);
    void ResetChannelCompletedInit();
    void ResetSystemState();

    // RIL Component Initialization functions (called by system init function)
    BOOL InitializeModem();
    BOOL InitializeSim();

    // Modem initialization helper functions (called by component init functions)
    BOOL SendModemInitCommands(eComInitIndex eInitIndex);
    static void* StartModemInitializationThreadWrapper(void* pArg);
    void StartModemInitializationThread();

    BOOL IsChannelUndefined(int channel);

private:
    static CSystemManager* m_pInstance;

    CEvent* m_pExitRilEvent;
    CEvent* m_pModemBasicInitCompleteEvent;
    CEvent* m_pSimUnlockedEvent;
    CEvent* m_pRadioPoweredOnEvent;
    CEvent* m_pModemPoweredOffEvent;
    CEvent* m_pInitStringCompleteEvent;
    CEvent* m_pSysInitCompleteEvent;

    CMutex* m_pSystemManagerMutex;

    // This mutex manages the access to ports as a whole
    // for operations such as opening and closing, thus
    // preventing access by multiple threads.
    CMutex* m_pPortsManagerMutex;

    CMutex* m_pDataChannelAccessorMutex;

    mmgr_cli_handle_t* m_pMMgrLibHandle;

    CRequestInfoTable m_RequestInfoTable;

    BOOL m_bIsSystemInitialized;

    BOOL m_rgfChannelCompletedInit[RIL_CHANNEL_MAX][COM_MAX_INDEX];

    char m_szModemResourceName[MAX_MDM_RESOURCE_NAME_SIZE];
    int m_ipcDataChannelMin;

#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)
    UINT32 m_uiLastCallFailedCauseID;
#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED

    CMutex* m_pSpoofCommandsStatusAccessMutex;
    CMutex* m_pTEAccessMutex;

    /*
     * This holds the read and write pipe descriptors. Upon data in read pipe
     * descriptor, channel response thread will cancel the response polling and
     * exit the response thread.
     */
    int m_cancelWaitPipeFds[2];
};

#endif // SYSTEMMANAGER

