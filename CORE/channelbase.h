////////////////////////////////////////////////////////////////////////////
// channelbase.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines channel-related classes, constants, and structures.
//
// Author:  Dennis Peter
// Created: 2007-07-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_CHANNEL_H
#define RRIL_CHANNEL_H

#include "com_init_index.h"
#include "port.h"
#include "command.h"
#include "com_init_index.h"

// forward declarations
class CSilo;
class CThread;

// forward declarations
class CSilo;
class CThread;


const UINT32 MAX_SILOS = 6;

#define MAX_COM_PORT_NAME_LENGTH  64


//  Structure used for specifying init strings
struct INITSTRING_DATA
{
    const BYTE* szCmd;
};

// Structure to hold silos
struct SILO_CONTAINER
{
    CSilo*  rgpSilos[MAX_SILOS];
    UINT32  nSilos;
};

class CChannelBase
{
public:
    CChannelBase(UINT32 uiChannel);
    virtual ~CChannelBase();

    //  Init functions
    BOOL            InitChannel();
    BOOL            StartChannelThreads();
    BOOL            StopChannelThreads();
    virtual UINT32  CommandThread();
    virtual UINT32  ResponseThread();

    BOOL            SendModemConfigurationCommands(eComInitIndex eInitIndex);

    // Public port interface
    virtual BOOL    OpenPort();
    BOOL            InitPort();
    BOOL            ClosePort();
    int             GetFD() { return m_Port.GetFD(); };

    UINT32          GetRilChannel() const { return m_uiRilChannel; };

    //  Public framework functions
    void            ClearCmdThreadBlockedOnRxQueue() { m_bCmdThreadBlockedOnRxQueue = FALSE; };

    BOOL            BlockReadThread()   { return CEvent::Reset(m_pBlockReadThreadEvent); }
    BOOL            UnblockReadThread() { return CEvent::Signal(m_pBlockReadThreadEvent); }

    //  Public interfaces to notify all silos.
    BOOL            ParseUnsolicitedResponse(CResponse* const pResponse, const BYTE*& rszPointer, BOOL &fGotoError);

    //  General public functions
    BOOL            IsCmdThreadBlockedOnRxQueue() const { return m_bCmdThreadBlockedOnRxQueue; };
//    BOOL            FWaitingForRsp() const  { return m_fWaitingForRsp; };

protected:
    //  Init functions
    virtual BOOL    FinishInit() = 0;

    //  Silo-related functions
    virtual BOOL    AddSilos() = 0;
    BOOL            AddSilo(CSilo *pSilo);

    // Protected port interface for inside of channel class
    BOOL            WriteToPort(const BYTE* pData, UINT32 uiBytesToWrite, UINT32& ruiBytesWritten);
    BOOL            ReadFromPort(BYTE* pszReadBuf, UINT32 uiReadBufSize, UINT32& ruiBytesRead);
    BOOL            IsPortOpen();
    BOOL            WaitForAvailableData(UINT32 uiTimeout);

    //  Framework functions
    virtual BOOL            SendCommand(CCommand*& rpCmd) = 0;
    virtual RIL_RESULT_CODE GetResponse(CCommand*& rpCmd, CResponse*& rpRsp) = 0;
    virtual BOOL            ParseResponse(CCommand*& rpCmd, CResponse*& rpRsp) = 0;

    // Called at end of ResponseThread()
    // Give GPRS response thread a chance to handle Rx data in Data mode
    virtual BOOL    ProcessModemData(BYTE *szData, UINT32 uiRead) = 0;

    //  Interfaces to hooks to notify silos
    BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp);
    BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp);
    BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);
    BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);

    //  Framework helper functions
    void            SetCmdThreadBlockedOnRxQueue() { m_bCmdThreadBlockedOnRxQueue = TRUE; }
    BOOL            WaitForCommand();

    void            HandleTimedOutError(BOOL fCmdTimedOut);

protected:
    //  Member variables
    UINT32                          m_uiRilChannel;
    CSystemManager*                 m_pSystemManager;

    UINT32                          m_bCmdThreadBlockedOnRxQueue : 1;
    UINT32                          m_bTimeoutWaitingForResponse : 1;
    UINT32                          m_fWaitingForRsp : 1;
    UINT32                          m_fLastCommandTimedOut : 1;
    UINT32                          m_fFinalInitOK : 1;

    CThread *                       m_pCmdThread;
    CThread *                       m_pReadThread;

    CEvent *                        m_pBlockReadThreadEvent;

    UINT32                          m_uiNumTimeouts;
    UINT32                          m_uiMaxTimeouts;

    UINT32                          m_uiLockCommandQueue;
    UINT32                          m_uiLockCommandQueueTimeout;

    INITSTRING_DATA*                m_prisdModuleInit;

    SILO_CONTAINER                  m_SiloContainer;

    CPort                           m_Port;
};

#endif //RRIL_CHANNEL_H
