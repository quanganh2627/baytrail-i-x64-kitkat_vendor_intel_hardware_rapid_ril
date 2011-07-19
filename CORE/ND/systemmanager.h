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


#ifndef RRIL_SYSTEMMANAGER_H
#define RRIL_SYSTEMMANAGER_H

#include "types.h"
#include "request_id.h"
#include "request_info_table.h"
#include "globals.h"
#include "rilqueue.h"
#include "thread_manager.h"
#include "rilchannels.h"
#include "com_init_index.h"

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
    BOOL            InitializeSystem();

    BOOL            IsExitRequestSignalled() const;

    //  Get/Set functions.
    static CEvent*  GetSystemInitCompleteEvent()        { return GetInstance().m_pSystemInitCompleteEvent; }
    static CEvent*  GetCancelEvent()                    { return GetInstance().m_pExitRilEvent;    };

    void            TriggerSimUnlockedEvent() const         { CEvent::Signal(m_pSimUnlockedEvent);      };
    void            TriggerModemPowerOnEvent() const        { CEvent::Signal(m_pModemPowerOnEvent);     };
    void            TriggerInitStringCompleteEvent(UINT32 eChannel, eComInitIndex eInitIndex);

    BOOL            IsInitializationSuccessful() const      { return !m_bFailedToInitialize; };
    void            SetInitializationUnsuccessful()         { m_bFailedToInitialize = TRUE; };

    void            GetRequestInfo(REQ_ID reqID, REQ_INFO &rReqInfo);

    //  For resetting modem
    void            CloseChannelPorts();
    BOOL            OpenChannelPortsOnly();

private:
    // Framework Init Functions
    BOOL            CreateQueues();
    void            DeleteQueues();
    BOOL            OpenChannelPorts();
    //void            CloseChannelPorts();
    void            DeleteChannels();
    CChannel*       CreateChannel(UINT32 uiIndex);

    // Internal Init helper functions
    void            SetChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex);
    BOOL            IsChannelCompletedInit(UINT32 uiChannel, eComInitIndex eInitIndex);
    BOOL            VerifyAllChannelsCompletedInit(eComInitIndex eInitIndex);
    void            ResetChannelCompletedInit();
    void            ResetSystemState();

    // RIL Component Initialization functions (called by system init function)
    BOOL            InitializeModem();
    BOOL            InitializeSim();

    // Modem initialization helper functions (called by component init functions)
    BOOL            SendModemInitCommands(eComInitIndex eInitIndex);
    static void*    StartModemInitializationThreadWrapper(void *pArg);
    void            StartModemInitializationThread();

private:
    static CSystemManager* m_pInstance;
    CEvent *            m_pSystemInitCompleteEvent;

    CEvent *            m_pExitRilEvent;
    CEvent *            m_pSimUnlockedEvent;
    CEvent *            m_pModemPowerOnEvent;
    CEvent *            m_pInitStringCompleteEvent;

    CMutex *            m_pAccessorMutex;
    CMutex *            m_pSystemManagerMutex;

    CRequestInfoTable   m_RequestInfoTable;

    BOOL                m_bFailedToInitialize;

    BOOL                m_rgfChannelCompletedInit[RIL_CHANNEL_MAX][COM_MAX_INDEX];

};

#endif // SYSTEMMANAGER

