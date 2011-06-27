////////////////////////////////////////////////////////////////////////////
// te.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CTE class which handles all overrides to requests and
//    basic behavior for responses for a specific modem
//
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  MW      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "../util.h"
#include "types.h"
#include "rillog.h"
#include "te.h"
#include "command.h"
#include "cmdcontext.h"
#include "rril_OEM.h"
#include "repository.h"
#include "oemhookids.h"
#include "repository.h"
#include "channel_data.h"

CTE * CTE::m_pTEInstance = NULL;

CTE::CTE() :
    m_pTEBaseInstance(NULL),
    m_pTEOemInstance(NULL)
{

    m_pTEBaseInstance = CreateModemTE();

    if (NULL == m_pTEBaseInstance)
    {
        RIL_LOG_CRITICAL("CTE::CTE() - ERROR: Unable to construct base terminal equipment!!!!!! EXIT!\r\n");
        exit(0);
    }

    m_pTEOemInstance = new CTEOem;
    if (NULL == m_pTEOemInstance)
    {
        RIL_LOG_CRITICAL("CTE::CTE() - ERROR: Unable to construct OEM terminal equipment!!!!!! EXIT!\r\n");
        exit(0);
    }
}

CTE::~CTE()
{
    delete m_pTEBaseInstance;
    m_pTEBaseInstance = NULL;

    delete m_pTEOemInstance;
    m_pTEOemInstance = NULL;
}

CTEBase* CTE::CreateModemTE()
{
    const BYTE* szInfineon6260       = "Infineon6260";

    CRepository repository;
    BYTE szModem[m_uiMaxModemNameLen];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, m_uiMaxModemNameLen))
    {
        if (0 == strcmp(szModem, szInfineon6260))
        {
            RIL_LOG_INFO("CTE::CreateModemTE() - Using Infineon 6260\r\n");

            //  Set g_cTerminator and g_szNewLine
            g_cTerminator = '\r';
            (void)CopyStringNullTerminate(g_szNewLine, "\r\n", sizeof(g_szNewLine));

            return new CTE_INF_6260();
        }
    }

    // return default modem
    RIL_LOG_INFO("CTE::CreateModemTE() - No modem specified, returning default\r\n");
    return new CTEBase();
}

CTE& CTE::GetTE()
{
    if (NULL == m_pTEInstance)
    {
        m_pTEInstance = new CTE;
        if (NULL == m_pTEInstance)
        {
            RIL_LOG_CRITICAL("CTE::GetTE() - ERROR: Unable to construct terminal equipment!!!!!! EXIT!\r\n");
            exit(0);
        }
    }

    return *m_pTEInstance;
}

void CTE::DeleteTEObject()
{
    delete m_pTEInstance;
    m_pTEInstance = NULL;
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTE::RequestGetSimStatus(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSimStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetSimStatus(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetSimStatus(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETSIMSTATUS], rilToken, ND_REQ_ID_GETSIMSTATUS, reqData, &CTE::ParseGetSimStatus);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSimStatus() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetSimStatus(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetSimStatus(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetSimStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
RIL_RESULT_CODE CTE::RequestEnterSimPin(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMEnterSimPin(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreEnterSimPin(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() : ERROR : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPIN], rilToken, ND_REQ_ID_ENTERSIMPIN, reqData, &CTE::ParseEnterSimPin);

        if (pCmd)
        {
            CContext* pContext = new CContextUnlock();
            if (pContext)
            {
                pCmd->SetContext(pContext);
                //  Call when radio is off.
                pCmd->SetHighPriority();

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - ERROR: Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - ERROR: Could not create context.\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseEnterSimPin(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseEnterSimPin(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMEnterSimPuk(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreEnterSimPuk(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPUK], rilToken, ND_REQ_ID_ENTERSIMPUK, reqData, &CTE::ParseEnterSimPuk);

        if (pCmd)
        {
            CContext* pContext = new CContextUnlock();
            if (pContext)
            {
                pCmd->SetContext(pContext);
                //  Call when radio is off.
                pCmd->SetHighPriority();

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - ERROR: Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - ERROR: Could not create context.\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseEnterSimPuk(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseEnterSimPuk(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
RIL_RESULT_CODE CTE::RequestEnterSimPin2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMEnterSimPin2(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreEnterSimPin2(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() : ERROR : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPIN2], rilToken, ND_REQ_ID_ENTERSIMPIN2, reqData, &CTE::ParseEnterSimPin2);

        if (pCmd)
        {
            CContext* pContext = new CContextUnlock();
            if (pContext)
            {
                pCmd->SetContext(pContext);
                //  Call when radio is off.
                pCmd->SetHighPriority();

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() - ERROR: Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() - ERROR: Could not create context.\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin2() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseEnterSimPin2(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseEnterSimPin2(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMEnterSimPuk2(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreEnterSimPuk2(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() : ERROR : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPUK2], rilToken, ND_REQ_ID_ENTERSIMPUK2, reqData, &CTE::ParseEnterSimPuk2);

        if (pCmd)
        {
            CContext* pContext = new CContextUnlock();
            if (pContext)
            {
                pCmd->SetContext(pContext);
                //  Call when radio is off.
                pCmd->SetHighPriority();

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() - ERROR: Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() - ERROR: Could not create context.\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk2() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseEnterSimPuk2(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseEnterSimPuk2(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
RIL_RESULT_CODE CTE::RequestChangeSimPin(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMChangeSimPin(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreChangeSimPin(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CHANGESIMPIN], rilToken, ND_REQ_ID_CHANGESIMPIN, reqData, &CTE::ParseChangeSimPin);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseChangeSimPin(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseChangeSimPin(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
RIL_RESULT_CODE CTE::RequestChangeSimPin2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMChangeSimPin2(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreChangeSimPin2(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CHANGESIMPIN2], rilToken, ND_REQ_ID_CHANGESIMPIN2, reqData, &CTE::ParseChangeSimPin2);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin2() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseChangeSimPin2(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseChangeSimPin2(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
RIL_RESULT_CODE CTE::RequestEnterNetworkDepersonalization(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMEnterNetworkDepersonalization(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreEnterNetworkDepersonalization(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION], rilToken, ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION, reqData, &CTE::ParseEnterNetworkDepersonalization);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterNetworkDepersonalization() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseEnterNetworkDepersonalization(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseEnterNetworkDepersonalization(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
RIL_RESULT_CODE CTE::RequestGetCurrentCalls(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetCurrentCalls(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetCurrentCalls(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETCURRENTCALLS], rilToken, ND_REQ_ID_GETCURRENTCALLS, reqData, &CTE::ParseGetCurrentCalls);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetCurrentCalls(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetCurrentCalls() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetCurrentCalls(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetCurrentCalls(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetCurrentCalls() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DIAL 10
//
RIL_RESULT_CODE CTE::RequestDial(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDial() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDial(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
    res = m_pTEBaseInstance->CoreDial(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDial() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
    CCommand * pCmd = new CCommand(RIL_CHANNEL_ATCMD, rilToken, ND_REQ_ID_DIAL, reqData, &CTE::ParseDial);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDial() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDial() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDial() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDial(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDial() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDial(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDial(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDial() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMSI 11
//
RIL_RESULT_CODE CTE::RequestGetImsi(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetImsi(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetImsi(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImsi() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMSI], rilToken, ND_REQ_ID_GETIMSI, reqData, &CTE::ParseGetImsi);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImsi() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImsi() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImsi(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImsi() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetImsi(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetImsi(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetImsi() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP 12
//
RIL_RESULT_CODE CTE::RequestHangup(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangup() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMHangup(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreHangup(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangup() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUP], rilToken, ND_REQ_ID_HANGUP, reqData, &CTE::ParseHangup);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangup() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangup() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangup() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangup(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangup() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseHangup(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseHangup(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseHangup() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
RIL_RESULT_CODE CTE::RequestHangupWaitingOrBackground(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMHangupWaitingOrBackground(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreHangupWaitingOrBackground(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUPWAITINGORBACKGROUND], rilToken, ND_REQ_ID_HANGUPWAITINGORBACKGROUND, reqData, &CTE::ParseHangupWaitingOrBackground);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupWaitingOrBackground() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseHangupWaitingOrBackground(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseHangupWaitingOrBackground(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
RIL_RESULT_CODE CTE::RequestHangupForegroundResumeBackground(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMHangupForegroundResumeBackground(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreHangupForegroundResumeBackground(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND], rilToken, ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND, reqData, &CTE::ParseHangupForegroundResumeBackground);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupForegroundResumeBackground() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseHangupForegroundResumeBackground(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseHangupForegroundResumeBackground(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
// RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
//
RIL_RESULT_CODE CTE::RequestSwitchHoldingAndActive(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSwitchHoldingAndActive() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSwitchHoldingAndActive(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSwitchHoldingAndActive(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SWITCHHOLDINGANDACTIVE], rilToken, ND_REQ_ID_SWITCHHOLDINGANDACTIVE, reqData, &CTE::ParseSwitchHoldingAndActive);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSwitchHoldingAndActive() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSwitchHoldingAndActive(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSwitchHoldingAndActive(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CONFERENCE 16
//
RIL_RESULT_CODE CTE::RequestConference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestConference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMConference(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreConference(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestConference() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CONFERENCE], rilToken, ND_REQ_ID_CONFERENCE, reqData, &CTE::ParseConference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestConference() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestConference() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestConference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseConference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseConference() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseConference(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseConference(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseConference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_UDUB 17
//
RIL_RESULT_CODE CTE::RequestUdub(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestUdub() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMUdub(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreUdub(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestUdub() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_UDUB], rilToken, ND_REQ_ID_UDUB, reqData, &CTE::ParseUdub);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestUdub() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestUdub() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestUdub() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseUdub(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUdub() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseUdub(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseUdub(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseUdub() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
RIL_RESULT_CODE CTE::RequestLastCallFailCause(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMLastCallFailCause(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreLastCallFailCause(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_LASTCALLFAILCAUSE], rilToken, ND_REQ_ID_LASTCALLFAILCAUSE, reqData, &CTE::ParseLastCallFailCause);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseLastCallFailCause(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseLastCallFailCause(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseLastCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTE::RequestSignalStrength(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSignalStrength(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSignalStrength(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIGNALSTRENGTH], rilToken, ND_REQ_ID_SIGNALSTRENGTH, reqData, &CTE::ParseSignalStrength);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSignalStrength(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSignalStrength(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSignalStrength() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REGISTRATION_STATE 20
//
RIL_RESULT_CODE CTE::RequestRegistrationState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMRegistrationState(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreRegistrationState(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REGISTRATIONSTATE], rilToken, ND_REQ_ID_REGISTRATIONSTATE, reqData, &CTE::ParseRegistrationState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseRegistrationState(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseRegistrationState(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GPRS_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTE::RequestGPRSRegistrationState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGPRSRegistrationState(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGPRSRegistrationState(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GPRSREGISTRATIONSTATE], rilToken, ND_REQ_ID_GPRSREGISTRATIONSTATE, reqData, &CTE::ParseGPRSRegistrationState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGPRSRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGPRSRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGPRSRegistrationState(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGPRSRegistrationState(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGPRSRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTE::RequestOperator(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestOperator() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMOperator(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreOperator(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestOperator() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_OPERATOR], rilToken, ND_REQ_ID_OPERATOR, reqData, &CTE::ParseOperator);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestOperator() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestOperator() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestOperator() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseOperator(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseOperator() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseOperator(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseOperator(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseOperator() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RADIO_POWER 23
//
RIL_RESULT_CODE CTE::RequestRadioPower(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestRadioPower() - Enter\r\n");

    bool bTurnRadioOn = false;
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(int) != datalen)
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    if (0 == *(int *)pData)
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio OFF\r\n");
        bTurnRadioOn = false;
    }
    else
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio ON\r\n");
        bTurnRadioOn = true;
    }

    if (FALSE == g_RadioState.IsPowerStateChangeable())
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - Preventing command\r\n");
        res = RIL_E_RADIO_NOT_AVAILABLE;
        goto Error;
    }

    // check if the required state is the same as the current one
    // if so, ignore command
    if ((bTurnRadioOn && !g_RadioState.IsRadioOff()) || (!bTurnRadioOn && g_RadioState.IsRadioOff()))
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - No change in state, spoofing command.\r\n");
        res = RIL_E_SUCCESS;
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }
    else
    {
        res = m_pTEOemInstance->OEMRadioPower(reqData, pData, datalen);

        if (RRIL_RESULT_NOTSUPPORTED == res)
        {
            res = m_pTEBaseInstance->CoreRadioPower(reqData, pData, datalen);
        }

        if (RRIL_RESULT_OK != res)
        {
            RIL_LOG_CRITICAL("CTE::RequestRadioPower() : ERROR : Unable to create AT command data\r\n");
        }
        else
        {
            CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_RADIOPOWER], rilToken, ND_REQ_ID_RADIOPOWER, reqData, &CTE::ParseRadioPower);

            if (pCmd)
            {
                CContextContainer* pContext = new CContextContainer();
                if (pContext)
                {
                    CContext* pPowerContext = NULL;

                    if (bTurnRadioOn)
                    {
                        pPowerContext = new CContextPower(true, *pCmd);
                    }
                    else
                    {
                        pPowerContext = new CContextPower(false, *pCmd);
                    }

                    if (pPowerContext)
                    {
                        pContext->Add(pPowerContext);
                        pCmd->SetContext((CContext*&) pContext);
                        pCmd->SetHighPriority();

                        if (!CCommand::AddCmdToQueue(pCmd))
                        {
                            RIL_LOG_CRITICAL("CTE::RequestRadioPower() - ERROR: Unable to add command to queue\r\n");
                            res = RIL_E_GENERIC_FAILURE;
                            delete pCmd;  //  When pCmd is deleted, the pContext is deleted as well.
                            pCmd = NULL;
                        }
                    }
                    else
                    {
                        RIL_LOG_CRITICAL("CTE::RequestRadioPower() : ERROR : Unable to create contexts!\r\n");
                        res = RIL_E_GENERIC_FAILURE;
                        delete pCmd;  //  When pCmd is deleted, the pContext is deleted as well.
                        pCmd = NULL;
                    }
                }
                else
                {
                    RIL_LOG_CRITICAL("CTE::RequestRadioPower() : ERROR : Unable to create context container!\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;  //  When pCmd is deleted, the pContext is deleted as well.
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestRadioPower() - ERROR: Unable to allocate memory for command\r\n");
                res = RIL_E_GENERIC_FAILURE;
            }
        }
    }

Error:

    RIL_LOG_VERBOSE("CTE::RequestRadioPower() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseRadioPower(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseRadioPower() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseRadioPower(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseRadioPower(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseRadioPower() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF 24
//
RIL_RESULT_CODE CTE::RequestDtmf(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDtmf(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDtmf(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmf() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DTMF], rilToken, ND_REQ_ID_DTMF, reqData, &CTE::ParseDtmf);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmf() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmf() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmf() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDtmf(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDtmf(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDtmf() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS 25
//
RIL_RESULT_CODE CTE::RequestSendSms(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSendSms(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSendSms(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSms() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDSMS], rilToken, ND_REQ_ID_SENDSMS, reqData, &CTE::ParseSendSms);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSms() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSms() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSms() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSendSms(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSendSms(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSendSms() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
RIL_RESULT_CODE CTE::RequestSendSmsExpectMore(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSendSmsExpectMore(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSendSmsExpectMore(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDSMSEXPECTMORE], rilToken, ND_REQ_ID_SENDSMSEXPECTMORE, reqData, &CTE::ParseSendSmsExpectMore);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSmsExpectMore(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSmsExpectMore() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSendSmsExpectMore(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSendSmsExpectMore(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSendSmsExpectMore() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
//
RIL_RESULT_CODE CTE::RequestSetupDataCall(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetupDataCall() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res;
    UINT32 uiRilChannel = RIL_CHANNEL_DATA1;
    UINT32 uiCID = CChannel_Data::GetNextContextID();

    CChannel_Data* pDataChannel = CChannel_Data::GetFreeChnl();
    if (pDataChannel)
    {
        uiRilChannel = pDataChannel->GetRilChannel();
    }
    else
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - ERROR: No free data channels available\r\n");
        res = RIL_E_GENERIC_FAILURE;
        goto Error;
    }

    res = m_pTEOemInstance->OEMSetupDataCall(reqData, pData, datalen, uiCID);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetupDataCall(reqData, pData, datalen, uiCID);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - ERROR: Unable to create AT command data\r\n");
        goto Error;
    }

    else
    {
        CCommand * pCmd = new CCommand(uiRilChannel, rilToken, ND_REQ_ID_SETUPDEFAULTPDP, reqData, &CTE::ParseSetupDataCall);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }


Error:
    RIL_LOG_VERBOSE("CTE::RequestSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetupDataCall(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetupDataCall(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetupDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTE::RequestSimIo(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSimIo(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSimIo(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimIo() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMIO], rilToken, ND_REQ_ID_SIMIO, reqData, &CTE::ParseSimIo);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimIo() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimIo() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimIo() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSimIo(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSimIo(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSimIo() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTE::RequestSendUssd(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSendUssd(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSendUssd(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendUssd() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDUSSD], rilToken, ND_REQ_ID_SENDUSSD, reqData, &CTE::ParseSendUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendUssd() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendUssd() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendUssd() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSendUssd(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSendUssd(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSendUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
RIL_RESULT_CODE CTE::RequestCancelUssd(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMCancelUssd(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreCancelUssd(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CANCELUSSD], rilToken, ND_REQ_ID_CANCELUSSD, reqData, &CTE::ParseCancelUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCancelUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCancelUssd() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCancelUssd(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCancelUssd(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCancelUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CLIR 31
//
RIL_RESULT_CODE CTE::RequestGetClir(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetClir(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetClir(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetClir() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETCLIR], rilToken, ND_REQ_ID_GETCLIR, reqData, &CTE::ParseGetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetClir() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetClir() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetClir() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetClir(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetClir(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CLIR 32
//
RIL_RESULT_CODE CTE::RequestSetClir(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Enter\r\n");

    CRepository repository;
    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetClir(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetClir(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetClir() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCLIR], rilToken, ND_REQ_ID_SETCLIR, reqData, &CTE::ParseSetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetClir() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetClir() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetClir() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetClir(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetClir(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
RIL_RESULT_CODE CTE::RequestQueryCallForwardStatus(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryCallForwardStatus(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryCallForwardStatus(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCALLFORWARDSTATUS], rilToken, ND_REQ_ID_QUERYCALLFORWARDSTATUS, reqData, &CTE::ParseQueryCallForwardStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallForwardStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallForwardStatus() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryCallForwardStatus(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryCallForwardStatus(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryCallForwardStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
RIL_RESULT_CODE CTE::RequestSetCallForward(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetCallForward(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetCallForward(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCALLFORWARD], rilToken, ND_REQ_ID_SETCALLFORWARD, reqData, &CTE::ParseSetCallForward);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallForward(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallForward() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetCallForward(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetCallForward(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetCallForward() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
RIL_RESULT_CODE CTE::RequestQueryCallWaiting(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryCallWaiting(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryCallWaiting(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCALLWAITING], rilToken, ND_REQ_ID_QUERYCALLWAITING, reqData, &CTE::ParseQueryCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryCallWaiting(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryCallWaiting(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
RIL_RESULT_CODE CTE::RequestSetCallWaiting(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetCallWaiting(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetCallWaiting(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCALLWAITING], rilToken, ND_REQ_ID_SETCALLWAITING, reqData, &CTE::ParseSetCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetCallWaiting(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetCallWaiting(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE 37
//
RIL_RESULT_CODE CTE::RequestSmsAcknowledge(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSmsAcknowledge(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSmsAcknowledge(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSACKNOWLEDGE], rilToken, ND_REQ_ID_SMSACKNOWLEDGE, reqData, &CTE::ParseSmsAcknowledge);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSmsAcknowledge() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSmsAcknowledge(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSmsAcknowledge(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSmsAcknowledge() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEI 38
//
RIL_RESULT_CODE CTE::RequestGetImei(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetImei(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetImei(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImei() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMEI], rilToken, ND_REQ_ID_GETIMEI, reqData, &CTE::ParseGetImei);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImei() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImei() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImei(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImei() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetImei(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetImei(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetImei() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEISV 39
//
RIL_RESULT_CODE CTE::RequestGetImeisv(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetImeisv(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetImeisv(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMEISV], rilToken, ND_REQ_ID_GETIMEISV, reqData, &CTE::ParseGetImeisv);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImeisv(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImeisv() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetImeisv(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetImeisv(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetImeisv() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ANSWER 40
//
RIL_RESULT_CODE CTE::RequestAnswer(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMAnswer(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
    res = m_pTEBaseInstance->CoreAnswer(reqData, pData, datalen);
     }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestAnswer() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ANSWER], rilToken, ND_REQ_ID_ANSWER, reqData, &CTE::ParseAnswer);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestAnswer() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestAnswer() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseAnswer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseAnswer() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseAnswer(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseAnswer(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseAnswer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE::RequestDeactivateDataCall(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDeactivateDataCall(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDeactivateDataCall(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DEACTIVATEDATACALL], rilToken, ND_REQ_ID_DEACTIVATEDATACALL, reqData, &CTE::ParseDeactivateDataCall);

        if (pCmd)
        {
            //  Call when radio is off.
            //  This happens when in data mode, and we go to flight mode.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDeactivateDataCall(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDeactivateDataCall(rRspData);
    }

Error:
    RIL_LOG_VERBOSE("CTE::ParseDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
RIL_RESULT_CODE CTE::RequestQueryFacilityLock(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryFacilityLock(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryFacilityLock(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYFACILITYLOCK], rilToken, ND_REQ_ID_QUERYFACILITYLOCK, reqData, &CTE::ParseQueryFacilityLock);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryFacilityLock() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryFacilityLock(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryFacilityLock(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
RIL_RESULT_CODE CTE::RequestSetFacilityLock(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetFacilityLock(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetFacilityLock(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETFACILITYLOCK], rilToken, ND_REQ_ID_SETFACILITYLOCK, reqData, &CTE::ParseSetFacilityLock);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetFacilityLock() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetFacilityLock(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetFacilityLock(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
RIL_RESULT_CODE CTE::RequestChangeBarringPassword(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMChangeBarringPassword(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreChangeBarringPassword(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CHANGEBARRINGPASSWORD], rilToken, ND_REQ_ID_CHANGEBARRINGPASSWORD, reqData, &CTE::ParseChangeBarringPassword);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeBarringPassword(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeBarringPassword() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseChangeBarringPassword(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseChangeBarringPassword(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseChangeBarringPassword() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTE::RequestQueryNetworkSelectionMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryNetworkSelectionMode(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryNetworkSelectionMode(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYNETWORKSELECTIONMODE], rilToken, ND_REQ_ID_QUERYNETWORKSELECTIONMODE, reqData, &CTE::ParseQueryNetworkSelectionMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryNetworkSelectionMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryNetworkSelectionMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryNetworkSelectionMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionAutomatic(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetNetworkSelectionAutomatic(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetNetworkSelectionAutomatic(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC], rilToken, ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC, reqData, &CTE::ParseSetNetworkSelectionAutomatic);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionAutomatic() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetNetworkSelectionAutomatic(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetNetworkSelectionAutomatic(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionManual(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetNetworkSelectionManual(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetNetworkSelectionManual(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETNETWORKSELECTIONMANUAL], rilToken, ND_REQ_ID_SETNETWORKSELECTIONMANUAL, reqData, &CTE::ParseSetNetworkSelectionManual);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionManual() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetNetworkSelectionManual(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetNetworkSelectionManual(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTE::RequestQueryAvailableNetworks(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryAvailableNetworks(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryAvailableNetworks(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYAVAILABLENETWORKS], rilToken, ND_REQ_ID_QUERYAVAILABLENETWORKS, reqData, &CTE::ParseQueryAvailableNetworks);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableNetworks(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableNetworks() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryAvailableNetworks(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryAvailableNetworks(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableNetworks() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_START 49
//
RIL_RESULT_CODE CTE::RequestDtmfStart(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDtmfStart(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDtmfStart(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REQUESTDTMFSTART], rilToken, ND_REQ_ID_REQUESTDTMFSTART, reqData, &CTE::ParseDtmfStart);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStart(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStart() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDtmfStart(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDtmfStart(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDtmfStart() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_STOP 50
//
RIL_RESULT_CODE CTE::RequestDtmfStop(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDtmfStop(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDtmfStop(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REQUESTDTMFSTOP], rilToken, ND_REQ_ID_REQUESTDTMFSTOP, reqData, &CTE::ParseDtmfStop);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStop(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStop() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDtmfStop(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDtmfStop(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDtmfStop() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTE::RequestBasebandVersion(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMBasebandVersion(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreBasebandVersion(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_BASEBANDVERSION], rilToken, ND_REQ_ID_BASEBANDVERSION, reqData, &CTE::ParseBasebandVersion);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseBasebandVersion(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseBasebandVersion(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseBasebandVersion(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseBasebandVersion() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
RIL_RESULT_CODE CTE::RequestSeparateConnection(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSeparateConnection(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSeparateConnection(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SEPERATECONNECTION], rilToken, ND_REQ_ID_SEPERATECONNECTION, reqData, &CTE::ParseSeparateConnection);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSeparateConnection(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSeparateConnection() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSeparateConnection(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSeparateConnection(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSeparateConnection() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTE::RequestSetMute(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetMute(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetMute(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetMute() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETMUTE], rilToken, ND_REQ_ID_SETMUTE, reqData, &CTE::ParseSetMute);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetMute() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetMute() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetMute() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetMute(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetMute(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_MUTE 54
//
RIL_RESULT_CODE CTE::RequestGetMute(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetMute(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetMute(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetMute() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETMUTE], rilToken, ND_REQ_ID_GETMUTE, reqData, &CTE::ParseGetMute);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetMute() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetMute() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetMute() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetMute(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetMute(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
RIL_RESULT_CODE CTE::RequestQueryClip(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryClip(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryClip(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryClip() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCLIP], rilToken, ND_REQ_ID_QUERYCLIP, reqData, &CTE::ParseQueryClip);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryClip() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryClip() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryClip(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryClip() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryClip(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryClip(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryClip() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
RIL_RESULT_CODE CTE::RequestLastDataCallFailCause(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMLastDataCallFailCause(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreLastDataCallFailCause(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_LASTPDPFAILCAUSE], rilToken, ND_REQ_ID_LASTPDPFAILCAUSE, reqData, &CTE::ParseLastDataCallFailCause);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastDataCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastDataCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseLastDataCallFailCause(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseLastDataCallFailCause(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseLastDataCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
RIL_RESULT_CODE CTE::RequestDataCallList(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDataCallList(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDataCallList(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDataCallList() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_PDPCONTEXTLIST], rilToken, ND_REQ_ID_PDPCONTEXTLIST, reqData, &CTE::ParseDataCallList);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDataCallList() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDataCallList() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDataCallList(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDataCallList() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDataCallList(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDataCallList(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDataCallList() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RESET_RADIO 58
//
RIL_RESULT_CODE CTE::RequestResetRadio(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMResetRadio(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreResetRadio(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestResetRadio() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_RESETRADIO], rilToken, ND_REQ_ID_RESETRADIO, reqData, &CTE::ParseResetRadio);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestResetRadio() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestResetRadio() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseResetRadio(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseResetRadio() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseResetRadio(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseResetRadio(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseResetRadio() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTE::RequestHookRaw(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMHookRaw(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreHookRaw(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookRaw() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_OEMHOOKRAW], rilToken, ND_REQ_ID_OEMHOOKRAW, reqData, &CTE::ParseHookRaw);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookRaw() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookRaw() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookRaw(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookRaw() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseHookRaw(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseHookRaw(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseHookRaw() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
RIL_RESULT_CODE CTE::RequestHookStrings(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMHookStrings(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreHookStrings(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookStrings() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_OEMHOOKSTRINGS], rilToken, ND_REQ_ID_OEMHOOKSTRINGS, reqData, &CTE::ParseHookStrings);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookStrings() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookStrings() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookStrings(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookStrings() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseHookStrings(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseHookStrings(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseHookStrings() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTE::RequestScreenState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMScreenState(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreScreenState(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestScreenState() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SCREENSTATE], rilToken, ND_REQ_ID_SCREENSTATE, reqData, &CTE::ParseScreenState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestScreenState() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestScreenState() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseScreenState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseScreenState() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseScreenState(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseScreenState(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseScreenState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
RIL_RESULT_CODE CTE::RequestSetSuppSvcNotification(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetSuppSvcNotification(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetSuppSvcNotification(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETSUPPSVCNOTIFICATION], rilToken, ND_REQ_ID_SETSUPPSVCNOTIFICATION, reqData, &CTE::ParseSetSuppSvcNotification);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetSuppSvcNotification(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSuppSvcNotification() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetSuppSvcNotification(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetSuppSvcNotification(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetSuppSvcNotification() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
RIL_RESULT_CODE CTE::RequestWriteSmsToSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMWriteSmsToSim(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreWriteSmsToSim(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_WRITESMSTOSIM], rilToken, ND_REQ_ID_WRITESMSTOSIM, reqData, &CTE::ParseWriteSmsToSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseWriteSmsToSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseWriteSmsToSim() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseWriteSmsToSim(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseWriteSmsToSim(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseWriteSmsToSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
RIL_RESULT_CODE CTE::RequestDeleteSmsOnSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMDeleteSmsOnSim(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreDeleteSmsOnSim(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DELETESMSONSIM], rilToken, ND_REQ_ID_DELETESMSONSIM, reqData, &CTE::ParseDeleteSmsOnSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeleteSmsOnSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeleteSmsOnSim() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDeleteSmsOnSim(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDeleteSmsOnSim(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDeleteSmsOnSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE::RequestSetBandMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetBandMode(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetBandMode(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETBANDMODE], rilToken, ND_REQ_ID_SETBANDMODE, reqData, &CTE::ParseSetBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetBandMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetBandMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE::RequestQueryAvailableBandMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryAvailableBandMode(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryAvailableBandMode(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYAVAILABLEBANDMODE], rilToken, ND_REQ_ID_QUERYAVAILABLEBANDMODE, reqData, &CTE::ParseQueryAvailableBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryAvailableBandMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryAvailableBandMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTE::RequestStkGetProfile(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMStkGetProfile(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreStkGetProfile(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKGETPROFILE], rilToken, ND_REQ_ID_STKGETPROFILE, reqData, &CTE::ParseStkGetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkGetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseStkGetProfile(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseStkGetProfile(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseStkGetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTE::RequestStkSetProfile(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMStkSetProfile(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreStkSetProfile(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSETPROFILE], rilToken, ND_REQ_ID_STKSETPROFILE, reqData, &CTE::ParseStkSetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseStkSetProfile(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseStkSetProfile(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseStkSetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE::RequestStkSendEnvelopeCommand(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMStkSendEnvelopeCommand(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreStkSendEnvelopeCommand(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSENDENVELOPECOMMAND], rilToken, ND_REQ_ID_STKSENDENVELOPECOMMAND, reqData, &CTE::ParseStkSendEnvelopeCommand);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendEnvelopeCommand() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseStkSendEnvelopeCommand(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseStkSendEnvelopeCommand(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE::RequestStkSendTerminalResponse(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMStkSendTerminalResponse(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreStkSendTerminalResponse(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSENDTERMINALRESPONSE], rilToken, ND_REQ_ID_STKSENDTERMINALRESPONSE, reqData, &CTE::ParseStkSendTerminalResponse);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendTerminalResponse() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseStkSendTerminalResponse(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseStkSendTerminalResponse(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseStkSendTerminalResponse() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTE::RequestStkHandleCallSetupRequestedFromSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMStkHandleCallSetupRequestedFromSim(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreStkHandleCallSetupRequestedFromSim(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM], rilToken, ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM, reqData, &CTE::ParseStkHandleCallSetupRequestedFromSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseStkHandleCallSetupRequestedFromSim(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseStkHandleCallSetupRequestedFromSim(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
RIL_RESULT_CODE CTE::RequestExplicitCallTransfer(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMExplicitCallTransfer(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreExplicitCallTransfer(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_EXPLICITCALLTRANSFER], rilToken, ND_REQ_ID_EXPLICITCALLTRANSFER, reqData, &CTE::ParseExplicitCallTransfer);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseExplicitCallTransfer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExplicitCallTransfer() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseExplicitCallTransfer(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseExplicitCallTransfer(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseExplicitCallTransfer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE::RequestSetPreferredNetworkType(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetPreferredNetworkType(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetPreferredNetworkType(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() : ERROR : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETPREFERREDNETWORKTYPE], rilToken, ND_REQ_ID_SETPREFERREDNETWORKTYPE, reqData, &CTE::ParseSetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }


/*
        if (pCmd)
        {
            CContext* pContext = new CContextNetworkType();
            if (pContext)
            {
                pCmd->SetContext(pContext);

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - ERROR: Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
                else
                {
                    // Ensure that we don't mess with the power state while these commands are still processing
                    g_RadioState.DisablePowerStateChange();
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - ERROR: Could not create context.\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
*/
    }

    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetPreferredNetworkType(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetPreferredNetworkType(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE::RequestGetPreferredNetworkType(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetPreferredNetworkType(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetPreferredNetworkType(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETPREFERREDNETWORKTYPE], rilToken, ND_REQ_ID_GETPREFERREDNETWORKTYPE, reqData, &CTE::ParseGetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetPreferredNetworkType(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetPreferredNetworkType(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE::RequestGetNeighboringCellIDs(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetNeighboringCellIDs(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetNeighboringCellIDs(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETNEIGHBORINGCELLIDS], rilToken, ND_REQ_ID_GETNEIGHBORINGCELLIDS, reqData, &CTE::ParseGetNeighboringCellIDs);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetNeighboringCellIDs(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetNeighboringCellIDs(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//
RIL_RESULT_CODE CTE::RequestSetLocationUpdates(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetLocationUpdates() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetLocationUpdates(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetLocationUpdates(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETLOCATIONUPDATES], rilToken, ND_REQ_ID_SETLOCATIONUPDATES, reqData, &CTE::ParseSetLocationUpdates);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetLocationUpdates() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetLocationUpdates(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetLocationUpdates() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetLocationUpdates(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetLocationUpdates(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetLocationUpdates() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION 77
//
RIL_RESULT_CODE CTE::RequestCdmaSetSubscription(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMCdmaSetSubscription(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreCdmaSetSubscription(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMASETSUBSCRIPTION], rilToken, ND_REQ_ID_CDMASETSUBSCRIPTION, reqData, &CTE::ParseCdmaSetSubscription);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetSubscription() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSetSubscription(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSetSubscription(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSetSubscription() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
RIL_RESULT_CODE CTE::RequestCdmaSetRoamingPreference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMCdmaSetRoamingPreference(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreCdmaSetRoamingPreference(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMASETROAMINGPREFERENCE], rilToken, ND_REQ_ID_CDMASETROAMINGPREFERENCE, reqData, &CTE::ParseCdmaSetRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSetRoamingPreference(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSetRoamingPreference(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
RIL_RESULT_CODE CTE::RequestCdmaQueryRoamingPreference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMCdmaQueryRoamingPreference(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreCdmaQueryRoamingPreference(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE], rilToken, ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE, reqData, &CTE::ParseCdmaQueryRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaQueryRoamingPreference(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaQueryRoamingPreference(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTE::RequestSetTtyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetTtyMode(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetTtyMode(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETTTYMODE], rilToken, ND_REQ_ID_SETTTYMODE, reqData, &CTE::ParseSetTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetTtyMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetTtyMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetTtyMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetTtyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTE::RequestQueryTtyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMQueryTtyMode(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreQueryTtyMode(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYTTYMODE], rilToken, ND_REQ_ID_QUERYTTYMODE, reqData, &CTE::ParseQueryTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryTtyMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryTtyMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryTtyMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryTtyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
RIL_RESULT_CODE CTE::RequestCdmaSetPreferredVoicePrivacyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetPreferredVoicePrivacyMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetPreferredVoicePrivacyMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSetPreferredVoicePrivacyMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSetPreferredVoicePrivacyMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSetPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
RIL_RESULT_CODE CTE::RequestCdmaQueryPreferredVoicePrivacyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryPreferredVoicePrivacyMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryPreferredVoicePrivacyMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaQueryPreferredVoicePrivacyMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaQueryPreferredVoicePrivacyMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
RIL_RESULT_CODE CTE::RequestCdmaFlash(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaFlash() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaFlash() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaFlash(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaFlash() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaFlash(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaFlash(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaFlash() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
RIL_RESULT_CODE CTE::RequestCdmaBurstDtmf(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaBurstDtmf() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaBurstDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaBurstDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaBurstDtmf() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaBurstDtmf(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaBurstDtmf(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaBurstDtmf() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
RIL_RESULT_CODE CTE::RequestCdmaValidateAndWriteAkey(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaValidateAndWriteAkey() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaValidateAndWriteAkey() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaValidateAndWriteAkey() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaValidateAndWriteAkey(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaValidateAndWriteAkey(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaValidateAndWriteAkey() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
RIL_RESULT_CODE CTE::RequestCdmaSendSms(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSendSms() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSendSms() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSendSms() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSendSms(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSendSms(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSendSms() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
RIL_RESULT_CODE CTE::RequestCdmaSmsAcknowledge(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsAcknowledge() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsAcknowledge() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSmsAcknowledge(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSmsAcknowledge(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsAcknowledge() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
RIL_RESULT_CODE CTE::RequestGsmGetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGsmGetBroadcastSmsConfig(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGsmGetBroadcastSmsConfig(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETBROADCASTSMSCONFIG], rilToken, ND_REQ_ID_GETBROADCASTSMSCONFIG, reqData, &CTE::ParseGsmGetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmGetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGsmGetBroadcastSmsConfig(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGsmGetBroadcastSmsConfig(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
RIL_RESULT_CODE CTE::RequestGsmSetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGsmSetBroadcastSmsConfig(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGsmSetBroadcastSmsConfig(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETBROADCASTSMSCONFIG], rilToken, ND_REQ_ID_SETBROADCASTSMSCONFIG, reqData, &CTE::ParseGsmSetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGsmSetBroadcastSmsConfig(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGsmSetBroadcastSmsConfig(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//
RIL_RESULT_CODE CTE::RequestGsmSmsBroadcastActivation(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGsmSmsBroadcastActivation(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGsmSmsBroadcastActivation(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSBROADCASTACTIVATION], rilToken, ND_REQ_ID_SMSBROADCASTACTIVATION, reqData, &CTE::ParseGsmSmsBroadcastActivation);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSmsBroadcastActivation() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGsmSmsBroadcastActivation(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGsmSmsBroadcastActivation(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
RIL_RESULT_CODE CTE::RequestCdmaGetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaGetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaGetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaGetBroadcastSmsConfig(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaGetBroadcastSmsConfig(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
RIL_RESULT_CODE CTE::RequestCdmaSetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSetBroadcastSmsConfig(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSetBroadcastSmsConfig(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
RIL_RESULT_CODE CTE::RequestCdmaSmsBroadcastActivation(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsBroadcastActivation() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsBroadcastActivation() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSmsBroadcastActivation(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSmsBroadcastActivation(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsBroadcastActivation() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
RIL_RESULT_CODE CTE::RequestCdmaSubscription(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSubscription() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSubscription() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaSubscription(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaSubscription(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaSubscription() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
RIL_RESULT_CODE CTE::RequestCdmaWriteSmsToRuim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaWriteSmsToRuim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaWriteSmsToRuim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaWriteSmsToRuim() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaWriteSmsToRuim(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaWriteSmsToRuim(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaWriteSmsToRuim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
RIL_RESULT_CODE CTE::RequestCdmaDeleteSmsOnRuim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaDeleteSmsOnRuim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestCdmaDeleteSmsOnRuim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaDeleteSmsOnRuim() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseCdmaDeleteSmsOnRuim(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseCdmaDeleteSmsOnRuim(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseCdmaDeleteSmsOnRuim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
RIL_RESULT_CODE CTE::RequestDeviceIdentity(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeviceIdentity() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestDeviceIdentity() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeviceIdentity(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeviceIdentity() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDeviceIdentity(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDeviceIdentity(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDeviceIdentity() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
RIL_RESULT_CODE CTE::RequestExitEmergencyCallbackMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExitEmergencyCallbackMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE::RequestExitEmergencyCallbackMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExitEmergencyCallbackMode() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseExitEmergencyCallbackMode(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseExitEmergencyCallbackMode(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseExitEmergencyCallbackMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
RIL_RESULT_CODE CTE::RequestGetSmscAddress(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMGetSmscAddress(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreGetSmscAddress(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETSMSCADDRESS], rilToken, ND_REQ_ID_GETSMSCADDRESS, reqData, &CTE::ParseGetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseGetSmscAddress(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseGetSmscAddress(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseGetSmscAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
RIL_RESULT_CODE CTE::RequestSetSmscAddress(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSetSmscAddress(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSetSmscAddress(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETSMSCADDRESS], rilToken, ND_REQ_ID_SETSMSCADDRESS, reqData, &CTE::ParseSetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE::ParseSetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSetSmscAddress(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSetSmscAddress(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSetSmscAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTE::RequestReportSmsMemoryStatus(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMReportSmsMemoryStatus(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreReportSmsMemoryStatus(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REPORTSMSMEMORYSTATUS], rilToken, ND_REQ_ID_REPORTSMSMEMORYSTATUS, reqData, &CTE::ParseReportSmsMemoryStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportSmsMemoryStatus() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseReportSmsMemoryStatus(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseReportSmsMemoryStatus(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTE::RequestReportStkServiceRunning(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMReportStkServiceRunning(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreReportStkServiceRunning(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REPORTSTKSERVICEISRUNNING], rilToken, ND_REQ_ID_REPORTSTKSERVICEISRUNNING, reqData, &CTE::ParseReportStkServiceRunning);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportStkServiceRunning() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseReportStkServiceRunning(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseReportStkServiceRunning(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseReportStkServiceRunning() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 104
//
RIL_RESULT_CODE CTE::RequestSimTransmitBasic(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSimTransmitBasic(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSimTransmitBasic(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMTRANSMITBASIC], rilToken, ND_REQ_ID_SIMTRANSMITBASIC, reqData, &CTE::ParseSimTransmitBasic);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitBasic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitBasic() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSimTransmitBasic(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSimTransmitBasic(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSimTransmitBasic() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SIM_OPEN_CHANNEL 105
//
RIL_RESULT_CODE CTE::RequestSimOpenChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSimOpenChannel(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSimOpenChannel(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMOPENCHANNEL], rilToken, ND_REQ_ID_SIMOPENCHANNEL, reqData, &CTE::ParseSimOpenChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimOpenChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimOpenChannel() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSimOpenChannel(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSimOpenChannel(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSimOpenChannel() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 106
//
RIL_RESULT_CODE CTE::RequestSimCloseChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSimCloseChannel(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSimCloseChannel(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMCLOSECHANNEL], rilToken, ND_REQ_ID_SIMCLOSECHANNEL, reqData, &CTE::ParseSimCloseChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimCloseChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimCloseChannel() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSimCloseChannel(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSimCloseChannel(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSimCloseChannel() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 107
//
RIL_RESULT_CODE CTE::RequestSimTransmitChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMSimTransmitChannel(reqData, pData, datalen);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->CoreSimTransmitChannel(reqData, pData, datalen);
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - ERROR: Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMTRANSMITCHANNEL], rilToken, ND_REQ_ID_SIMTRANSMITCHANNEL, reqData, &CTE::ParseSimTransmitChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - ERROR: Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - ERROR: Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitChannel() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseSimTransmitChannel(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseSimTransmitChannel(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseSimTransmitChannel() - Exit\r\n");
    return res;
}


//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTE::ParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUnsolicitedSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseUnsolicitedSignalStrength(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseUnsolicitedSignalStrength(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseUnsolicitedSignalStrength() - Exit\r\n");
    return res;
}

//
// RIL_UNSOL_DATA_CALL_LIST_CHANGED  1010
//
RIL_RESULT_CODE CTE::ParseDataCallListChanged(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDataCallListChanged() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDataCallListChanged(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDataCallListChanged(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDataCallListChanged() - Exit\r\n");
    return res;
}


//
// GET_IP_ADDRESS (sent internally)
//
RIL_RESULT_CODE CTE::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseIpAddress(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseIpAddress(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseIpAddress() - Exit\r\n");
    return res;
}

//
// GET_DNS (sent internally)
//
RIL_RESULT_CODE CTE::ParseDns(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDns() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseDns(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseDns(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseDns() - Exit\r\n");
    return res;
}

//
// QUERYPIN2 (sent internally)
//
RIL_RESULT_CODE CTE::ParseQueryPIN2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryPIN2() - Enter\r\n");

    RIL_RESULT_CODE res = m_pTEOemInstance->OEMParseQueryPIN2(rRspData);

    if (RRIL_RESULT_NOTSUPPORTED == res)
    {
        res = m_pTEBaseInstance->ParseQueryPIN2(rRspData);
    }

    RIL_LOG_VERBOSE("CTE::ParseQueryPIN2() - Exit\r\n");
    return res;
}

