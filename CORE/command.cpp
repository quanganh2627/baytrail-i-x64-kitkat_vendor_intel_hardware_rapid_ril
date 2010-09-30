////////////////////////////////////////////////////////////////////////////
// Command.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CCommand class which stores details required to execute
//    and return the result of a specific RIL API
//
// Author:  Mike Worth
// Created: 2009-11-19
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 19/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "util.h"
#include "sync_ops.h"
#include "cmdcontext.h"
#include "command.h"

CCommand::CCommand( EnumRilChannel eChannel,
                    RIL_Token token,
                    UINT32 uiReqId,
                    const BYTE* pszATCmd,
                    PFN_TE_PARSE pParseFcn) :
    m_eChannel(RIL_CHANNEL_ATCMD),
    m_token(token),
    m_uiReqId(uiReqId),
    m_pszATCmd1(NULL),
    m_pszATCmd2(NULL),
    m_pParseFcn(pParseFcn),
    m_uiTimeout(0),
    m_fAlwaysParse(FALSE),
    m_fHighPriority(FALSE),
    m_pContext(NULL)

{
    if ((RIL_CHANNEL_ATCMD <= eChannel) && (eChannel < RIL_CHANNEL_MAX))
    {
        m_eChannel = eChannel;
    }
    else
    {
        RIL_LOG_CRITICAL("CCommand::CCommand() - ERROR: Using default channel as given argument is invalid [%d]\r\n", eChannel);
    }
    
    if ((NULL == pszATCmd) || ('\0' == pszATCmd[0]))
    {
        m_pszATCmd1 = NULL;
    }
    else
    {
        UINT32 uiCmdLen = strlen(pszATCmd) + 1;
        m_pszATCmd1 = new BYTE[uiCmdLen];
        memset(m_pszATCmd1, 0, uiCmdLen);
        CopyStringNullTerminate(m_pszATCmd1, pszATCmd, uiCmdLen);
    }
}

CCommand::CCommand( EnumRilChannel eChannel,
                    RIL_Token token,
                    UINT32 uiReqId,
                    const BYTE* pszATCmd1,
                    const BYTE* pszATCmd2,
                    PFN_TE_PARSE pParseFcn) : 
    m_eChannel(RIL_CHANNEL_ATCMD),
    m_token(token),
    m_uiReqId(uiReqId),
    m_pszATCmd1(NULL),
    m_pszATCmd2(NULL),
    m_pParseFcn(pParseFcn),
    m_uiTimeout(0),
    m_fAlwaysParse(FALSE),
    m_fHighPriority(FALSE),
    m_pContext(NULL)
{
    if ((RIL_CHANNEL_ATCMD <= eChannel) && (eChannel < RIL_CHANNEL_MAX))
    {
        m_eChannel = eChannel;
    }
    else
    {
        RIL_LOG_CRITICAL("CCommand::CCommand() - ERROR: Using default channel as given argument is invalid [%d]\r\n", eChannel);
    }
    
    if ((NULL == pszATCmd1) || ('\0' == pszATCmd1[0]))
    {
        m_pszATCmd1 = NULL;
    }
    else
    {
        UINT32 uiCmdLen = strlen(pszATCmd1) + 1;
        m_pszATCmd1 = new BYTE[uiCmdLen];
        memset(m_pszATCmd1, 0, uiCmdLen);
        CopyStringNullTerminate(m_pszATCmd1, pszATCmd1, uiCmdLen);
    }

    if ((NULL == pszATCmd2) || ('\0' == pszATCmd2[0]))
    {
        m_pszATCmd2 = NULL;
    }
    else
    {
        UINT32 uiCmdLen = strlen(pszATCmd2) + 1;
        m_pszATCmd2 = new BYTE[uiCmdLen];
        memset(m_pszATCmd2, 0, uiCmdLen);
        CopyStringNullTerminate(m_pszATCmd2, pszATCmd2, uiCmdLen);
    }
}

CCommand::CCommand( EnumRilChannel eChannel,
                    RIL_Token token,
                    UINT32 uiReqId,
                    REQUEST_DATA reqData,
                    PFN_TE_PARSE pParseFcn) : 
    m_eChannel(RIL_CHANNEL_ATCMD),
    m_token(token),
    m_uiReqId(uiReqId),
    m_pszATCmd1(NULL),
    m_pszATCmd2(NULL),
    m_pParseFcn(pParseFcn),
    m_uiTimeout(reqData.uiTimeout),
    m_fAlwaysParse(reqData.fForceParse),
    m_fHighPriority(FALSE),
    m_pContext(NULL)
{
    if ((RIL_CHANNEL_ATCMD <= eChannel) && (eChannel < RIL_CHANNEL_MAX))
    {
        m_eChannel = eChannel;
    }
    else
    {
        RIL_LOG_CRITICAL("CCommand::CCommand() - ERROR: Using default channel as given argument is invalid [%d]\r\n", eChannel);
    }
    
    if ('\0' == reqData.szCmd1[0])
    {
        m_pszATCmd1 = NULL;
    }
    else
    {
        UINT32 uiCmdLen = strlen(reqData.szCmd1) + 1;
        m_pszATCmd1 = new BYTE[uiCmdLen];
        memset(m_pszATCmd1, 0, uiCmdLen);
        CopyStringNullTerminate(m_pszATCmd1, reqData.szCmd1, uiCmdLen);
    }

    if ('\0' == reqData.szCmd2[0])
    {
        m_pszATCmd2 = NULL;
    }
    else
    {
        UINT32 uiCmdLen = strlen(reqData.szCmd2) + 1;
        m_pszATCmd2 = new BYTE[uiCmdLen];
        memset(m_pszATCmd2, 0, uiCmdLen);
        CopyStringNullTerminate(m_pszATCmd2, reqData.szCmd2, uiCmdLen);
    }
}

CCommand::~CCommand()
{
    delete[] m_pszATCmd1;
    m_pszATCmd1 = NULL;
    delete[] m_pszATCmd2;
    m_pszATCmd2 = NULL;
    delete m_pContext;
    m_pContext = NULL;
}

BOOL CCommand::AddCmdToQueue(CCommand *& rpCmd)
{
    RIL_LOG_VERBOSE("CCommand::AddCmdToQueue() - Enter\r\n");

    if (NULL != rpCmd)
    {
        //extern CSystemManager* g_pDevice;

        REQ_INFO reqInfo;
        memset(&reqInfo, 0, sizeof(reqInfo));
        
        // Get the info about this API
        CSystemManager::GetInstance().GetRequestInfo((REQ_ID)rpCmd->GetRequestID(), reqInfo);

        //  A value of "0" for uiTimeout will use the retrieved request info from the registry.
        if (0 == rpCmd->GetTimeout())
        {
            rpCmd->SetTimeout(reqInfo.uiTimeout);
        }

        UINT32 nChannel = rpCmd->GetChannel();
        //RIL_LOG_INFO("CCommand::AddCmdToQueue() - TXQueue ENQUEUE BEGIN  ishighpriority=[%d]\r\n", rpCmd->IsHighPriority());
        if (g_pTxQueue[nChannel]->Enqueue(rpCmd, (UINT32)(rpCmd->IsHighPriority()) ))
        {
            //RIL_LOG_INFO("CCommand::AddCmdToQueue() - TXQueue ENQUEUE END\r\n");
            // signal Tx thread
            //RIL_LOG_INFO("CCommand::AddCmdToQueue() - TXQueue SIGNAL BEGIN\r\n");
            (void) CEvent::Signal(g_TxQueueEvent[nChannel]);
            //(void) CEvent::Reset(g_TxQueueEvent[nChannel]);
            //RIL_LOG_INFO("CCommand::AddCmdToQueue() - TXQueue SIGNAL END\r\n");
            
            // The queue owns the command now
            rpCmd = NULL;

            RIL_LOG_VERBOSE("CCommand::AddCmdToQueue() - Exit\r\n");
            return TRUE;
        }
        else
        {
            RIL_LOG_CRITICAL("CCommand::AddCmdToQueue() - ERROR: Unable to queue command on channel [%d]\r\n", rpCmd->m_eChannel);
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CCommand::AddCmdToQueue() - ERROR: Command pointer was NULL\r\n");
    }

    RIL_LOG_VERBOSE("CCommand::AddCmdToQueue() - Exit\r\n");
    return FALSE;
}

