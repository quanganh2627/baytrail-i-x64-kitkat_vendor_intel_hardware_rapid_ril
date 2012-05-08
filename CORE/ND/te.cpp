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
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>

#include "../util.h"
#include "extract.h"
#include "types.h"
#include "rillog.h"
#include "te.h"
#include "command.h"
#include "cmdcontext.h"
#include "rril_OEM.h"
#include "repository.h"
#include "oemhookids.h"
#include "channel_data.h"
#include "te_inf_6260.h"

CTE * CTE::m_pTEInstance = NULL;

CTE::CTE() :
    m_pTEBaseInstance(NULL),
    m_bCSStatusCached(FALSE),
    m_bPSStatusCached(FALSE)
{
    m_pTEBaseInstance = CreateModemTE();

    if (NULL == m_pTEBaseInstance)
    {
        RIL_LOG_CRITICAL("CTE::CTE() - Unable to construct base terminal equipment!!!!!! EXIT!\r\n");
        exit(0);
    }
}

CTE::~CTE()
{
    delete m_pTEBaseInstance;
    m_pTEBaseInstance = NULL;
}

CTEBase* CTE::CreateModemTE()
{
    const char* szInfineon6260       = "Infineon6260";

    CRepository repository;
    char szModem[m_uiMaxModemNameLen];

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
    RIL_LOG_INFO("CTE::CreateModemTE() - No modem specified, returning NULL\r\n");
    return NULL;
}

CTE& CTE::GetTE()
{
    if (NULL == m_pTEInstance)
    {
        m_pTEInstance = new CTE;
        if (NULL == m_pTEInstance)
        {
            RIL_LOG_CRITICAL("CTE::GetTE() - Unable to construct terminal equipment!!!!!! EXIT!\r\n");
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

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSimStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETSIMSTATUS], rilToken, ND_REQ_ID_GETSIMSTATUS, reqData, &CTE::ParseGetSimStatus);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            // Setting the request to be placed in front of queue
            // because SIM IO requests will block this during SIM hot swap use cases.
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSimStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSimStatus(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
RIL_RESULT_CODE CTE::RequestEnterSimPin(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPin(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPIN], rilToken, ND_REQ_ID_ENTERSIMPIN, reqData, &CTE::ParseEnterSimPin);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPin(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPuk(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPUK], rilToken, ND_REQ_ID_ENTERSIMPUK, reqData, &CTE::ParseEnterSimPuk);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPuk(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
RIL_RESULT_CODE CTE::RequestEnterSimPin2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPin2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPIN2], rilToken, ND_REQ_ID_ENTERSIMPIN2, reqData, &CTE::ParseEnterSimPin2);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPin2(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPuk2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ENTERSIMPUK2], rilToken, ND_REQ_ID_ENTERSIMPUK2, reqData, &CTE::ParseEnterSimPuk2);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPuk2(rRspData);
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
RIL_RESULT_CODE CTE::RequestChangeSimPin(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeSimPin(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeSimPin(rRspData);
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
RIL_RESULT_CODE CTE::RequestChangeSimPin2(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeSimPin2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeSimPin2(rRspData);
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
RIL_RESULT_CODE CTE::RequestEnterNetworkDepersonalization(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterNetworkDepersonalization(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterNetworkDepersonalization() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterNetworkDepersonalization(rRspData);
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
RIL_RESULT_CODE CTE::RequestGetCurrentCalls(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetCurrentCalls(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETCURRENTCALLS], rilToken, ND_REQ_ID_GETCURRENTCALLS, reqData, &CTE::ParseGetCurrentCalls);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetCurrentCalls(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetCurrentCalls() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetCurrentCalls(rRspData);
}

//
// RIL_REQUEST_DIAL 10
//
RIL_RESULT_CODE CTE::RequestDial(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDial() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDial(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to create AT command data\r\n");
    }
    else
    {
    CCommand * pCmd = new CCommand(RIL_CHANNEL_ATCMD, rilToken, ND_REQ_ID_DIAL, reqData, &CTE::ParseDial);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDial() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDial(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDial() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDial(rRspData);
}

//
// RIL_REQUEST_GET_IMSI 11
//
RIL_RESULT_CODE CTE::RequestGetImsi(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImsi(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMSI], rilToken, ND_REQ_ID_GETIMSI, reqData, &CTE::ParseGetImsi);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImsi(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImsi() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImsi(rRspData);
}

//
// RIL_REQUEST_HANGUP 12
//
RIL_RESULT_CODE CTE::RequestHangup(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangup() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangup(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUP], rilToken, ND_REQ_ID_HANGUP, reqData, &CTE::ParseHangup);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangup() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangup(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangup() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangup(rRspData);
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
RIL_RESULT_CODE CTE::RequestHangupWaitingOrBackground(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupWaitingOrBackground(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUPWAITINGORBACKGROUND], rilToken, ND_REQ_ID_HANGUPWAITINGORBACKGROUND, reqData, &CTE::ParseHangupWaitingOrBackground);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupWaitingOrBackground() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupWaitingOrBackground(rRspData);
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
RIL_RESULT_CODE CTE::RequestHangupForegroundResumeBackground(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupForegroundResumeBackground(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND], rilToken, ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND, reqData, &CTE::ParseHangupForegroundResumeBackground);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupForegroundResumeBackground() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupForegroundResumeBackground(rRspData);
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

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSwitchHoldingAndActive(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SWITCHHOLDINGANDACTIVE], rilToken, ND_REQ_ID_SWITCHHOLDINGANDACTIVE, reqData, &CTE::ParseSwitchHoldingAndActive);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSwitchHoldingAndActive() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSwitchHoldingAndActive(rRspData);
}

//
// RIL_REQUEST_CONFERENCE 16
//
RIL_RESULT_CODE CTE::RequestConference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestConference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreConference(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestConference() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CONFERENCE], rilToken, ND_REQ_ID_CONFERENCE, reqData, &CTE::ParseConference);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestConference() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestConference() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestConference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseConference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseConference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseConference(rRspData);
}

//
// RIL_REQUEST_UDUB 17
//
RIL_RESULT_CODE CTE::RequestUdub(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestUdub() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreUdub(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_UDUB], rilToken, ND_REQ_ID_UDUB, reqData, &CTE::ParseUdub);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestUdub() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseUdub(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUdub() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseUdub(rRspData);
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
RIL_RESULT_CODE CTE::RequestLastCallFailCause(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreLastCallFailCause(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_LASTCALLFAILCAUSE], rilToken, ND_REQ_ID_LASTCALLFAILCAUSE, reqData, &CTE::ParseLastCallFailCause);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastCallFailCause() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseLastCallFailCause(rRspData);
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTE::RequestSignalStrength(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSignalStrength(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIGNALSTRENGTH], rilToken, ND_REQ_ID_SIGNALSTRENGTH, reqData, &CTE::ParseSignalStrength);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSignalStrength() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSignalStrength(rRspData);
}

//
// RIL_REQUEST_VOICE_REGISTRATION_STATE 20
//
RIL_RESULT_CODE CTE::RequestRegistrationState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (m_bCSStatusCached)
    {
        P_ND_REG_STATUS pRegStatus = NULL;

        pRegStatus = (P_ND_REG_STATUS)malloc(sizeof(S_ND_REG_STATUS));
        if (NULL == pRegStatus)
        {
            RIL_LOG_CRITICAL("CTE::ParseRegistrationState() - Could not allocate memory for S_ND_REG_STATUS struct.\r\n");
            RIL_onRequestComplete(rilToken, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
        else
        {
            CopyCachedRegistrationInfo(pRegStatus, FALSE);
            /*
             * cheat with the size here.
             * Although we have allocated a S_ND_REG_STATUS struct, we tell
             * Android that we have only allocated a S_ND_REG_STATUS_POINTERS
             * struct since Android is expecting to receive an array of string pointers.
             */
            RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, (void*)pRegStatus, sizeof(S_ND_REG_STATUS_POINTERS));
        }
        return RRIL_RESULT_OK;
    }

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreRegistrationState(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REGISTRATIONSTATE], rilToken, ND_REQ_ID_REGISTRATIONSTATE, reqData, &CTE::ParseRegistrationState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseRegistrationState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseRegistrationState(rRspData);
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTE::RequestGPRSRegistrationState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Enter\r\n");

    if (m_bPSStatusCached)
    {
        P_ND_GPRS_REG_STATUS pRegStatus = NULL;

        pRegStatus = (P_ND_GPRS_REG_STATUS)malloc(sizeof(S_ND_GPRS_REG_STATUS));
        if (NULL == pRegStatus)
        {
            RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - Could not allocate memory for S_ND_GPRS_REG_STATUS struct.\r\n");
            RIL_onRequestComplete(rilToken, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
        else
        {
            CopyCachedRegistrationInfo(pRegStatus, TRUE);
            /*
             * cheat with the size here.
             * Although we have allocated a S_ND_GPRS_REG_STATUS struct, we tell
             * Android that we have only allocated a S_ND_GPRS_REG_STATUS_POINTERS
             * struct since Android is expecting to receive an array of string pointers.
             */
            RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, (void*)pRegStatus, sizeof(S_ND_GPRS_REG_STATUS_POINTERS));
        }

        return RRIL_RESULT_OK;
    }

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGPRSRegistrationState(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GPRSREGISTRATIONSTATE], rilToken, ND_REQ_ID_GPRSREGISTRATIONSTATE, reqData, &CTE::ParseGPRSRegistrationState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGPRSRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGPRSRegistrationState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGPRSRegistrationState(rRspData);
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTE::RequestOperator(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestOperator() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreOperator(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_OPERATOR], rilToken, ND_REQ_ID_OPERATOR, reqData, &CTE::ParseOperator);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestOperator() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseOperator(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseOperator() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseOperator(rRspData);
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
    RIL_RadioState radio_state = g_RadioState.GetRadioState();
    char szShutdownActionProperty[PROPERTY_VALUE_MAX] = {'\0'};

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(int) != datalen)
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - Invalid data size.\r\n");
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

    // Retrieve the shutdown property to verify if the requested radio state is OFF and a reboot is requested
    // if so ignore the command
    if (property_get("sys.shutdown.requested", szShutdownActionProperty, NULL) &&
                ('1' == szShutdownActionProperty[0]) )
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - Reboot requested, do nothing\r\n");
        res = RIL_E_SUCCESS;
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }
#if !defined(M2_DUALSIM_FEATURE_ENABLED)
    // check if the required state is the same as the current one
    // if so, ignore command
    else if ((bTurnRadioOn && RADIO_STATE_OFF != radio_state) || (!bTurnRadioOn && RADIO_STATE_OFF == radio_state))
    {
        RIL_LOG_INFO("CTE::RequestRadioPower() - No change in state, spoofing command.\r\n");
        res = RIL_E_SUCCESS;
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }
#endif
    else
    {
        RIL_RESULT_CODE res = m_pTEBaseInstance->CoreRadioPower(reqData, pData, datalen);
        if (RRIL_RESULT_OK != res)
        {
            RIL_LOG_CRITICAL("CTE::RequestRadioPower() : Unable to create AT command data\r\n");
        }
        else
        {
            CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_RADIOPOWER], rilToken, ND_REQ_ID_RADIOPOWER, reqData, &CTE::ParseRadioPower);

            if (pCmd)
            {
                pCmd->SetHighPriority();

                if (!CCommand::AddCmdToQueue(pCmd))
                {
                    RIL_LOG_CRITICAL("CTE::RequestRadioPower() : Unable to add command to queue\r\n");
                    res = RIL_E_GENERIC_FAILURE;
                    delete pCmd;
                    pCmd = NULL;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("CTE::RequestRadioPower() - Unable to allocate memory for command\r\n");
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
    RIL_LOG_VERBOSE("CTE::ParseRadioPower() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseRadioPower(rRspData);
}

//
// RIL_REQUEST_DTMF 24
//
RIL_RESULT_CODE CTE::RequestDtmf(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmf(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DTMF], rilToken, ND_REQ_ID_DTMF, reqData, &CTE::ParseDtmf);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmf() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmf(rRspData);
}

//
// RIL_REQUEST_SEND_SMS 25
//
RIL_RESULT_CODE CTE::RequestSendSms(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendSms(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDSMS], rilToken, ND_REQ_ID_SENDSMS, reqData, &CTE::ParseSendSms);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSms() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendSms(rRspData);
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
RIL_RESULT_CODE CTE::RequestSendSmsExpectMore(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendSmsExpectMore(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDSMSEXPECTMORE], rilToken, ND_REQ_ID_SENDSMSEXPECTMORE, reqData, &CTE::ParseSendSmsExpectMore);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSmsExpectMore(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSmsExpectMore() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendSmsExpectMore(rRspData);
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
    UINT32 uiCID = 0;

    //  Find free channel, and get the context ID that was set.
    CChannel_Data* pChannelData = CChannel_Data::GetFreeChnl(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - ****** No free data channels available ******\r\n");
        res = RIL_E_GENERIC_FAILURE;
        goto Error;
    }

    res = m_pTEBaseInstance->CoreSetupDataCall(reqData, pData, datalen, uiCID);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - Unable to create AT command data\r\n");
        goto Error;
    }

    else
    {
        CCommand * pCmd = new CCommand(pChannelData->GetRilChannel(), rilToken, ND_REQ_ID_SETUPDEFAULTPDP, reqData, &CTE::ParseSetupDataCall);

        if (pCmd)
        {
            pCmd->SetAlwaysParse();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }


Error:
    if (RRIL_RESULT_OK != res)
    {
        if (pChannelData)
            pChannelData->FreeContextID();
    }
    RIL_LOG_VERBOSE("CTE::RequestSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetupDataCall() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetupDataCall(rRspData);
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTE::RequestSimIo(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimIo(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimIo() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimIo(rRspData);
}

//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTE::RequestSendUssd(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendUssd(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SENDUSSD], rilToken, ND_REQ_ID_SENDUSSD, reqData, &CTE::ParseSendUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendUssd() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendUssd(rRspData);
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
RIL_RESULT_CODE CTE::RequestCancelUssd(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCancelUssd(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CANCELUSSD], rilToken, ND_REQ_ID_CANCELUSSD, reqData, &CTE::ParseCancelUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCancelUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCancelUssd() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCancelUssd(rRspData);
}

//
// RIL_REQUEST_GET_CLIR 31
//
RIL_RESULT_CODE CTE::RequestGetClir(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetClir(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETCLIR], rilToken, ND_REQ_ID_GETCLIR, reqData, &CTE::ParseGetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetClir() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetClir(rRspData);
}

//
// RIL_REQUEST_SET_CLIR 32
//
RIL_RESULT_CODE CTE::RequestSetClir(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetClir(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCLIR], rilToken, ND_REQ_ID_SETCLIR, reqData, &CTE::ParseSetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetClir() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetClir(rRspData);
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
RIL_RESULT_CODE CTE::RequestQueryCallForwardStatus(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryCallForwardStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCALLFORWARDSTATUS], rilToken, ND_REQ_ID_QUERYCALLFORWARDSTATUS, reqData, &CTE::ParseQueryCallForwardStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallForwardStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallForwardStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryCallForwardStatus(rRspData);
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
RIL_RESULT_CODE CTE::RequestSetCallForward(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetCallForward(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCALLFORWARD], rilToken, ND_REQ_ID_SETCALLFORWARD, reqData, &CTE::ParseSetCallForward);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallForward(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallForward() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetCallForward(rRspData);
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
RIL_RESULT_CODE CTE::RequestQueryCallWaiting(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryCallWaiting(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCALLWAITING], rilToken, ND_REQ_ID_QUERYCALLWAITING, reqData, &CTE::ParseQueryCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallWaiting() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryCallWaiting(rRspData);
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
RIL_RESULT_CODE CTE::RequestSetCallWaiting(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetCallWaiting(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETCALLWAITING], rilToken, ND_REQ_ID_SETCALLWAITING, reqData, &CTE::ParseSetCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallWaiting() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetCallWaiting(rRspData);
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE 37
//
RIL_RESULT_CODE CTE::RequestSmsAcknowledge(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSmsAcknowledge(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSACKNOWLEDGE], rilToken, ND_REQ_ID_SMSACKNOWLEDGE, reqData, &CTE::ParseSmsAcknowledge);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSmsAcknowledge() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSmsAcknowledge(rRspData);
}

//
// RIL_REQUEST_GET_IMEI 38
//
RIL_RESULT_CODE CTE::RequestGetImei(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImei(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMEI], rilToken, ND_REQ_ID_GETIMEI, reqData, &CTE::ParseGetImei);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImei(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImei() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImei(rRspData);
}

//
// RIL_REQUEST_GET_IMEISV 39
//
RIL_RESULT_CODE CTE::RequestGetImeisv(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImeisv(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIMEISV], rilToken, ND_REQ_ID_GETIMEISV, reqData, &CTE::ParseGetImeisv);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImeisv(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImeisv() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImeisv(rRspData);
}

//
// RIL_REQUEST_ANSWER 40
//
RIL_RESULT_CODE CTE::RequestAnswer(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreAnswer(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_ANSWER], rilToken, ND_REQ_ID_ANSWER, reqData, &CTE::ParseAnswer);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseAnswer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseAnswer() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseAnswer(rRspData);
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE::RequestDeactivateDataCall(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDeactivateDataCall(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeactivateDataCall() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeactivateDataCall(rRspData);
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
RIL_RESULT_CODE CTE::RequestQueryFacilityLock(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryFacilityLock(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryFacilityLock() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryFacilityLock(rRspData);
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
RIL_RESULT_CODE CTE::RequestSetFacilityLock(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetFacilityLock(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - Unable to create AT command data\r\n");
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
                RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetFacilityLock() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetFacilityLock(rRspData);
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
RIL_RESULT_CODE CTE::RequestChangeBarringPassword(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeBarringPassword(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CHANGEBARRINGPASSWORD], rilToken, ND_REQ_ID_CHANGEBARRINGPASSWORD, reqData, &CTE::ParseChangeBarringPassword);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeBarringPassword(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeBarringPassword() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeBarringPassword(rRspData);
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTE::RequestQueryNetworkSelectionMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryNetworkSelectionMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYNETWORKSELECTIONMODE], rilToken, ND_REQ_ID_QUERYNETWORKSELECTIONMODE, reqData, &CTE::ParseQueryNetworkSelectionMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryNetworkSelectionMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryNetworkSelectionMode(rRspData);
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionAutomatic(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetNetworkSelectionAutomatic(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC], rilToken, ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC, reqData, &CTE::ParseSetNetworkSelectionAutomatic);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionAutomatic() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetNetworkSelectionAutomatic(rRspData);
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionManual(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetNetworkSelectionManual(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETNETWORKSELECTIONMANUAL], rilToken, ND_REQ_ID_SETNETWORKSELECTIONMANUAL, reqData, &CTE::ParseSetNetworkSelectionManual);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionManual() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetNetworkSelectionManual(rRspData);
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTE::RequestQueryAvailableNetworks(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryAvailableNetworks(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYAVAILABLENETWORKS], rilToken, ND_REQ_ID_QUERYAVAILABLENETWORKS, reqData, &CTE::ParseQueryAvailableNetworks);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableNetworks(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableNetworks() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryAvailableNetworks(rRspData);
}

//
// RIL_REQUEST_DTMF_START 49
//
RIL_RESULT_CODE CTE::RequestDtmfStart(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmfStart(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REQUESTDTMFSTART], rilToken, ND_REQ_ID_REQUESTDTMFSTART, reqData, &CTE::ParseDtmfStart);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStart(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStart() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmfStart(rRspData);
}

//
// RIL_REQUEST_DTMF_STOP 50
//
RIL_RESULT_CODE CTE::RequestDtmfStop(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmfStop(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REQUESTDTMFSTOP], rilToken, ND_REQ_ID_REQUESTDTMFSTOP, reqData, &CTE::ParseDtmfStop);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStop(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStop() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmfStop(rRspData);
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTE::RequestBasebandVersion(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreBasebandVersion(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_BASEBANDVERSION], rilToken, ND_REQ_ID_BASEBANDVERSION, reqData, &CTE::ParseBasebandVersion);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseBasebandVersion(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseBasebandVersion() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseBasebandVersion(rRspData);
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
RIL_RESULT_CODE CTE::RequestSeparateConnection(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSeparateConnection(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SEPERATECONNECTION], rilToken, ND_REQ_ID_SEPERATECONNECTION, reqData, &CTE::ParseSeparateConnection);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSeparateConnection(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSeparateConnection() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSeparateConnection(rRspData);
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTE::RequestSetMute(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetMute(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETMUTE], rilToken, ND_REQ_ID_SETMUTE, reqData, &CTE::ParseSetMute);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetMute() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetMute(rRspData);
}

//
// RIL_REQUEST_GET_MUTE 54
//
RIL_RESULT_CODE CTE::RequestGetMute(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetMute(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETMUTE], rilToken, ND_REQ_ID_GETMUTE, reqData, &CTE::ParseGetMute);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetMute() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetMute(rRspData);
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
RIL_RESULT_CODE CTE::RequestQueryClip(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryClip(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryClip() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYCLIP], rilToken, ND_REQ_ID_QUERYCLIP, reqData, &CTE::ParseQueryClip);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryClip() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryClip() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryClip(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryClip() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryClip(rRspData);
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
RIL_RESULT_CODE CTE::RequestLastDataCallFailCause(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreLastDataCallFailCause(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_LASTPDPFAILCAUSE], rilToken, ND_REQ_ID_LASTPDPFAILCAUSE, reqData, &CTE::ParseLastDataCallFailCause);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastDataCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastDataCallFailCause() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseLastDataCallFailCause(rRspData);
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
RIL_RESULT_CODE CTE::RequestDataCallList(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDataCallList(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDataCallList() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_PDPCONTEXTLIST], rilToken, ND_REQ_ID_PDPCONTEXTLIST, reqData, &CTE::ParseDataCallList);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDataCallList() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDataCallList() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDataCallList(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDataCallList() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDataCallList(rRspData);
}

//
// RIL_REQUEST_RESET_RADIO 58
//
RIL_RESULT_CODE CTE::RequestResetRadio(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreResetRadio(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestResetRadio() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_RESETRADIO], rilToken, ND_REQ_ID_RESETRADIO, reqData, &CTE::ParseResetRadio);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestResetRadio() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestResetRadio() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseResetRadio(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseResetRadio() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseResetRadio(rRspData);
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTE::RequestHookRaw(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    //  CoreHookRaw API chooses what RIL Channel to send command on.
    //  Channel is passed back through uiChannel parameter.
    //  Default is value defined in rilchannels.cpp.
    UINT32 uiRilChannel = g_arChannelMapping[ND_REQ_ID_OEMHOOKRAW];

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHookRaw(reqData, pData, datalen, uiRilChannel);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(uiRilChannel, rilToken, ND_REQ_ID_OEMHOOKRAW, reqData, &CTE::ParseHookRaw);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookRaw(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookRaw() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHookRaw(rRspData);
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
RIL_RESULT_CODE CTE::RequestHookStrings(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    //  CoreHookStrings API chooses what RIL Channel to send command on.
    //  Channel is passed back through uiChannel parameter.
    //  Default is value defined in rilchannels.cpp.
    UINT32 uiRilChannel = g_arChannelMapping[ND_REQ_ID_OEMHOOKSTRINGS];

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHookStrings(reqData, pData, datalen, uiRilChannel);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookStrings() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(uiRilChannel, rilToken, ND_REQ_ID_OEMHOOKSTRINGS, reqData, &CTE::ParseHookStrings);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookStrings() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookStrings() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookStrings(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookStrings() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHookStrings(rRspData);
}


//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTE::RequestScreenState(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreScreenState(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestScreenState() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SCREENSTATE], rilToken, ND_REQ_ID_SCREENSTATE, reqData, &CTE::ParseScreenState);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestScreenState() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestScreenState() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseScreenState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseScreenState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseScreenState(rRspData);
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
RIL_RESULT_CODE CTE::RequestSetSuppSvcNotification(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetSuppSvcNotification(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETSUPPSVCNOTIFICATION], rilToken, ND_REQ_ID_SETSUPPSVCNOTIFICATION, reqData, &CTE::ParseSetSuppSvcNotification);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetSuppSvcNotification(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSuppSvcNotification() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetSuppSvcNotification(rRspData);
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
RIL_RESULT_CODE CTE::RequestWriteSmsToSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreWriteSmsToSim(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_WRITESMSTOSIM], rilToken, ND_REQ_ID_WRITESMSTOSIM, reqData, &CTE::ParseWriteSmsToSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseWriteSmsToSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseWriteSmsToSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseWriteSmsToSim(rRspData);
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
RIL_RESULT_CODE CTE::RequestDeleteSmsOnSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDeleteSmsOnSim(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DELETESMSONSIM], rilToken, ND_REQ_ID_DELETESMSONSIM, reqData, &CTE::ParseDeleteSmsOnSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeleteSmsOnSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeleteSmsOnSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeleteSmsOnSim(rRspData);
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE::RequestSetBandMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetBandMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETBANDMODE], rilToken, ND_REQ_ID_SETBANDMODE, reqData, &CTE::ParseSetBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetBandMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetBandMode(rRspData);
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE::RequestQueryAvailableBandMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryAvailableBandMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYAVAILABLEBANDMODE], rilToken, ND_REQ_ID_QUERYAVAILABLEBANDMODE, reqData, &CTE::ParseQueryAvailableBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableBandMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryAvailableBandMode(rRspData);
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTE::RequestStkGetProfile(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkGetProfile(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKGETPROFILE], rilToken, ND_REQ_ID_STKGETPROFILE, reqData, &CTE::ParseStkGetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkGetProfile() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkGetProfile(rRspData);
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTE::RequestStkSetProfile(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSetProfile(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSETPROFILE], rilToken, ND_REQ_ID_STKSETPROFILE, reqData, &CTE::ParseStkSetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSetProfile() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSetProfile(rRspData);
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE::RequestStkSendEnvelopeCommand(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSendEnvelopeCommand(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSENDENVELOPECOMMAND], rilToken, ND_REQ_ID_STKSENDENVELOPECOMMAND, reqData, &CTE::ParseStkSendEnvelopeCommand);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendEnvelopeCommand() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSendEnvelopeCommand(rRspData);
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE::RequestStkSendTerminalResponse(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSendTerminalResponse(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKSENDTERMINALRESPONSE], rilToken, ND_REQ_ID_STKSENDTERMINALRESPONSE, reqData, &CTE::ParseStkSendTerminalResponse);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendTerminalResponse() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSendTerminalResponse(rRspData);
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTE::RequestStkHandleCallSetupRequestedFromSim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkHandleCallSetupRequestedFromSim(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM], rilToken, ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM, reqData, &CTE::ParseStkHandleCallSetupRequestedFromSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkHandleCallSetupRequestedFromSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkHandleCallSetupRequestedFromSim(rRspData);
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
RIL_RESULT_CODE CTE::RequestExplicitCallTransfer(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreExplicitCallTransfer(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_EXPLICITCALLTRANSFER], rilToken, ND_REQ_ID_EXPLICITCALLTRANSFER, reqData, &CTE::ParseExplicitCallTransfer);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseExplicitCallTransfer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExplicitCallTransfer() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseExplicitCallTransfer(rRspData);
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE::RequestSetPreferredNetworkType(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetPreferredNetworkType(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETPREFERREDNETWORKTYPE], rilToken, ND_REQ_ID_SETPREFERREDNETWORKTYPE, reqData, &CTE::ParseSetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetPreferredNetworkType() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetPreferredNetworkType(rRspData);
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE::RequestGetPreferredNetworkType(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetPreferredNetworkType(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETPREFERREDNETWORKTYPE], rilToken, ND_REQ_ID_GETPREFERREDNETWORKTYPE, reqData, &CTE::ParseGetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetPreferredNetworkType() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetPreferredNetworkType(rRspData);
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE::RequestGetNeighboringCellIDs(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetNeighboringCellIDs(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETNEIGHBORINGCELLIDS], rilToken, ND_REQ_ID_GETNEIGHBORINGCELLIDS, reqData, &CTE::ParseGetNeighboringCellIDs);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetNeighboringCellIDs() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetNeighboringCellIDs(rRspData);
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE 77
//
RIL_RESULT_CODE CTE::RequestCdmaSetSubscription(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaSetSubscription(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMASETSUBSCRIPTION], rilToken, ND_REQ_ID_CDMASETSUBSCRIPTION, reqData, &CTE::ParseCdmaSetSubscription);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetSubscription() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetSubscription(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
RIL_RESULT_CODE CTE::RequestCdmaSetRoamingPreference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaSetRoamingPreference(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMASETROAMINGPREFERENCE], rilToken, ND_REQ_ID_CDMASETROAMINGPREFERENCE, reqData, &CTE::ParseCdmaSetRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetRoamingPreference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetRoamingPreference(rRspData);
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
RIL_RESULT_CODE CTE::RequestCdmaQueryRoamingPreference(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaQueryRoamingPreference(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE], rilToken, ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE, reqData, &CTE::ParseCdmaQueryRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryRoamingPreference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaQueryRoamingPreference(rRspData);
}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTE::RequestSetTtyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetTtyMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETTTYMODE], rilToken, ND_REQ_ID_SETTTYMODE, reqData, &CTE::ParseSetTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetTtyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetTtyMode(rRspData);
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTE::RequestQueryTtyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryTtyMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYTTYMODE], rilToken, ND_REQ_ID_QUERYTTYMODE, reqData, &CTE::ParseQueryTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryTtyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryTtyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
RIL_RESULT_CODE CTE::RequestCdmaSetPreferredVoicePrivacyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetPreferredVoicePrivacyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
RIL_RESULT_CODE CTE::RequestCdmaQueryPreferredVoicePrivacyMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaQueryPreferredVoicePrivacyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
RIL_RESULT_CODE CTE::RequestCdmaFlash(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaFlash() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaFlash(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaFlash() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaFlash(rRspData);
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
RIL_RESULT_CODE CTE::RequestCdmaBurstDtmf(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaBurstDtmf() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaBurstDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaBurstDtmf() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaBurstDtmf(rRspData);
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
RIL_RESULT_CODE CTE::RequestCdmaValidateAndWriteAkey(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaValidateAndWriteAkey() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaValidateAndWriteAkey() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaValidateAndWriteAkey(rRspData);
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
RIL_RESULT_CODE CTE::RequestCdmaSendSms(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSendSms() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSendSms() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSendSms(rRspData);
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
RIL_RESULT_CODE CTE::RequestCdmaSmsAcknowledge(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsAcknowledge() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsAcknowledge() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSmsAcknowledge(rRspData);
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
RIL_RESULT_CODE CTE::RequestGsmGetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmGetBroadcastSmsConfig(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETBROADCASTSMSCONFIG], rilToken, ND_REQ_ID_GETBROADCASTSMSCONFIG, reqData, &CTE::ParseGsmGetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmGetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
RIL_RESULT_CODE CTE::RequestGsmSetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmSetBroadcastSmsConfig(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETBROADCASTSMSCONFIG], rilToken, ND_REQ_ID_SETBROADCASTSMSCONFIG, reqData, &CTE::ParseGsmSetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmSetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//
RIL_RESULT_CODE CTE::RequestGsmSmsBroadcastActivation(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmSmsBroadcastActivation(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSBROADCASTACTIVATION], rilToken, ND_REQ_ID_SMSBROADCASTACTIVATION, reqData, &CTE::ParseGsmSmsBroadcastActivation);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSmsBroadcastActivation() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmSmsBroadcastActivation(rRspData);
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
RIL_RESULT_CODE CTE::RequestCdmaGetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaGetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
RIL_RESULT_CODE CTE::RequestCdmaSetBroadcastSmsConfig(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
RIL_RESULT_CODE CTE::RequestCdmaSmsBroadcastActivation(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsBroadcastActivation() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsBroadcastActivation() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSmsBroadcastActivation(rRspData);
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
RIL_RESULT_CODE CTE::RequestCdmaSubscription(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSubscription() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSubscription() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSubscription(rRspData);
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
RIL_RESULT_CODE CTE::RequestCdmaWriteSmsToRuim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaWriteSmsToRuim() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaWriteSmsToRuim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaWriteSmsToRuim(rRspData);
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
RIL_RESULT_CODE CTE::RequestCdmaDeleteSmsOnRuim(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaDeleteSmsOnRuim() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaDeleteSmsOnRuim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaDeleteSmsOnRuim(rRspData);
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
RIL_RESULT_CODE CTE::RequestDeviceIdentity(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeviceIdentity() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseDeviceIdentity(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeviceIdentity() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeviceIdentity(rRspData);
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
RIL_RESULT_CODE CTE::RequestExitEmergencyCallbackMode(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExitEmergencyCallbackMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExitEmergencyCallbackMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseExitEmergencyCallbackMode(rRspData);
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
RIL_RESULT_CODE CTE::RequestGetSmscAddress(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSmscAddress(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETSMSCADDRESS], rilToken, ND_REQ_ID_GETSMSCADDRESS, reqData, &CTE::ParseGetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSmscAddress() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSmscAddress(rRspData);
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
RIL_RESULT_CODE CTE::RequestSetSmscAddress(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetSmscAddress(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETSMSCADDRESS], rilToken, ND_REQ_ID_SETSMSCADDRESS, reqData, &CTE::ParseSetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE::ParseSetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSmscAddress() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetSmscAddress(rRspData);
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTE::RequestReportSmsMemoryStatus(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreReportSmsMemoryStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REPORTSMSMEMORYSTATUS], rilToken, ND_REQ_ID_REPORTSMSMEMORYSTATUS, reqData, &CTE::ParseReportSmsMemoryStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportSmsMemoryStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReportSmsMemoryStatus(rRspData);
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTE::RequestReportStkServiceRunning(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreReportStkServiceRunning(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_REPORTSTKSERVICEISRUNNING], rilToken, ND_REQ_ID_REPORTSTKSERVICEISRUNNING, reqData, &CTE::ParseReportStkServiceRunning);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportStkServiceRunning() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReportStkServiceRunning(rRspData);
}

//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 108
//
RIL_RESULT_CODE CTE::RequestSimTransmitBasic(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimTransmitBasic(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMTRANSMITBASIC], rilToken, ND_REQ_ID_SIMTRANSMITBASIC, reqData, &CTE::ParseSimTransmitBasic);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitBasic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitBasic() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimTransmitBasic(rRspData);
}

//
// RIL_REQUEST_SIM_OPEN_CHANNEL 109
//
RIL_RESULT_CODE CTE::RequestSimOpenChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimOpenChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMOPENCHANNEL], rilToken, ND_REQ_ID_SIMOPENCHANNEL, reqData, &CTE::ParseSimOpenChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() -Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimOpenChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimOpenChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimOpenChannel(rRspData);
}

//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 110
//
RIL_RESULT_CODE CTE::RequestSimCloseChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimCloseChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMCLOSECHANNEL], rilToken, ND_REQ_ID_SIMCLOSECHANNEL, reqData, &CTE::ParseSimCloseChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimCloseChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimCloseChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimCloseChannel(rRspData);
}

//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 111
//
RIL_RESULT_CODE CTE::RequestSimTransmitChannel(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimTransmitChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIMTRANSMITCHANNEL], rilToken, ND_REQ_ID_SIMTRANSMITCHANNEL, reqData, &CTE::ParseSimTransmitChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimTransmitChannel(rRspData);
}


#if defined(M2_VT_FEATURE_ENABLED)
//
// RIL_REQUEST_HANGUP_VT 112
//
RIL_RESULT_CODE CTE::RequestHangupVT(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupVT() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupVT(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUPVT], rilToken, ND_REQ_ID_HANGUPVT, reqData, &CTE::ParseHangupVT);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupVT() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupVT(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupVT() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupVT(rRspData);
}


//
// RIL_REQUEST_DIAL_VT 113
//
RIL_RESULT_CODE CTE::RequestDialVT(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDialVT() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDialVT(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DIALVT], rilToken, ND_REQ_ID_DIALVT, reqData, &CTE::ParseDialVT);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDialVT() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDialVT(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDialVT() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDialVT(rRspData);
}
#endif // M2_VT_FEATURE_ENABLED


#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
//
// RIL_REQUEST_GET_SIM_SMS_STORAGE 114
//
RIL_RESULT_CODE CTE::RequestGetSimSmsStorage(RIL_Token rilToken, void * pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSimSmsStorage() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSimSmsStorage(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS], rilToken, ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS, reqData, &CTE::ParseGetSimSmsStorage);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSimSmsStorage() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSimSmsStorage(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSimSmsStorage() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSimSmsStorage(rRspData);
}
#endif // M2_GET_SIM_SMS_STORAGE_ENABLED


//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTE::ParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUnsolicitedSignalStrength() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseUnsolicitedSignalStrength(rRspData);
}

//
// RIL_UNSOL_DATA_CALL_LIST_CHANGED  1010
//
RIL_RESULT_CODE CTE::ParseDataCallListChanged(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDataCallListChanged() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDataCallListChanged(rRspData);
}

//
// GET_IP_ADDRESS (sent internally)
//
RIL_RESULT_CODE CTE::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseIpAddress() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseIpAddress(rRspData);
}

//
// GET_DNS (sent internally)
//
RIL_RESULT_CODE CTE::ParseDns(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDns() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDns(rRspData);
}

//
// QUERYPIN2 (sent internally)
//
RIL_RESULT_CODE CTE::ParseQueryPIN2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryPIN2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryPIN2(rRspData);
}

//
// QueryDataCallFailCause (sent internally)
//
RIL_RESULT_CODE CTE::ParseDataCallFailCause(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDataCallFailCause() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDataCallFailCause(rRspData);
}

RIL_RadioTechnology CTE::MapAccessTechnology(UINT32 uiStdAct)
{
    RIL_LOG_VERBOSE("CTE::MapAccessTechnology() - Enter\r\n");

    /*
     * 20111103: There is no 3GPP standard value defined for GPRS and HSPA+
     * access technology. So, values 1 and 8 are used in relation with the
     * IMC proprietary +XREG: <Act> parameter.
     *
     * Note: GSM Compact is not supported by IMC modem.
     */
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;

    //  Check state and set global variable for network technology
    switch(uiStdAct)
    {
        case 0: // GSM
        rtAct = RADIO_TECH_UNKNOWN; // 0 - GSM access technology is not used on android side
        break;

        case 1: // GSM Compact
        rtAct = RADIO_TECH_GPRS; // 1
        break;

        case 2: // UTRAN
        rtAct = RADIO_TECH_UMTS; // 3
        break;

        case 3: // GSM w/EGPRS
        rtAct = RADIO_TECH_EDGE; // 2
        break;

        case 4: // UTRAN w/HSDPA
        rtAct = RADIO_TECH_HSDPA; // 9
        break;

        case 5: // UTRAN w/HSUPA
        rtAct = RADIO_TECH_HSUPA; // 10
        break;

        case 6: // UTRAN w/HSDPA and HSUPA
        rtAct = RADIO_TECH_HSPA; // 11
        break;

        case 7:
        rtAct = RADIO_TECH_LTE; // 14
        break;

        case 8: // Proprietary value introduced for HSPA+
        rtAct = RADIO_TECH_HSPAP; // 15
        break;

        default:
        rtAct = RADIO_TECH_UNKNOWN; // 0
        break;
    }

    RIL_LOG_VERBOSE("CTE::MapAccessTechnology() - Exit\r\n");
    return rtAct;
}

BOOL CTE::ParseCREG(const char*& rszPointer, const BOOL bUnSolicited, S_ND_REG_STATUS& rCSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseCREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;
    BOOL bRet = false;

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+CREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip \"+CREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <lac>.\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);

        // Extract ",<cid>"
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract \",<cid>\".\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
         {
             RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <cid>.\r\n");
             goto Error;
         }
        SkipString(rszPointer, "\"", rszPointer);

        // Extract ",Act"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (!ExtractUInt32(rszPointer, uiAct, rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <act>.\r\n");
                goto Error;
            }

            /*
             * Maps the 3GPP standard access technology values to android specific access
             * technology values.
             */
            rtAct = MapAccessTechnology(uiAct);
        }
    }

    // Skip "<postfix>"
    if (!SkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip response postfix.\r\n");
        goto Error;
    }

    /*
     * Registration status value of 2,3 or 4 means that there is a cell
     * around on which emergency calls are possible. Inorder to show
     * emergency calls only, Android telephony stack expects the registration
     * status value to be one of 10,12,13,14.
     */
    if (2 == uiStatus || 3 == uiStatus || 4 == uiStatus)
    {
        uiStatus += 10;
    }

    snprintf(rCSRegStatusInfo.szStat,        REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rCSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)rtAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rCSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rCSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rCSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rCSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);
    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE::ParseCREG() - Exit\r\n");
    return bRet;
}

BOOL CTE::ParseCGREG(const char*& rszPointer, const BOOL bUnSolicited, S_ND_GPRS_REG_STATUS& rPSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseCGREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;
    UINT32 uiRAC = 0;
    BOOL bRet = false;

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+CGREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip \"+CREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <lac>.\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);

        // Extract ",<cid>"
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract \",<cid>\".\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
         {
             RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <cid>.\r\n");
             goto Error;
         }
        SkipString(rszPointer, "\"", rszPointer);

        // Extract ",Act"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (!ExtractUInt32(rszPointer, uiAct, rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <act>.\r\n");
                goto Error;
            }

            /*
             * Maps the 3GPP standard access technology values to android specific access
             * technology values.
             */
            rtAct = MapAccessTechnology(uiAct);

            // Extract ","
            if (!SkipString(rszPointer, ",", rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract ,<rac>.\r\n");
                goto Error;
            }

            // Extract "<rac>"
            SkipString(rszPointer, "\"", rszPointer);
            if (!ExtractHexUInt32(rszPointer, uiRAC, rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract \",<rac>\".\r\n");
                goto Error;
            }

            SkipString(rszPointer, "\"", rszPointer);
        }
    }

    // Skip "<postfix>"
    if (!SkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip response postfix.\r\n");
        goto Error;
    }

    snprintf(rPSRegStatusInfo.szStat, REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rPSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)rtAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rPSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rPSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE::ParseCGREG() - Exit\r\n");
    return bRet;
}

BOOL CTE::ParseXREG(const char*& rszPointer, const BOOL bUnSolicited, S_ND_GPRS_REG_STATUS& rPSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseXREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    char szBand[MAX_BUFFER_SIZE] = {0};
    BOOL bRet = false;

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+XREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip \"+XREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    //  Parse <AcT>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiAct, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseXREG() - Error: Parsing <AcT>\r\n");
        goto Error;
    }

    /*
     * Maps the 3GPP standard access technology values to android specific access
     * technology values.
     */
    uiAct = CTE::MapAccessTechnology(uiAct);

    //  Extract <Band> and throw away
    if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUnquotedString(rszPointer, ",", szBand, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseXREG() - Error: Parsing <Band>\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <lac>.\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);

        // Extract ",<cid>"
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract \",<cid>\".\r\n");
            goto Error;
        }
        SkipString(rszPointer, "\"", rszPointer);
        if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
         {
             RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <cid>.\r\n");
             goto Error;
         }
        SkipString(rszPointer, "\"", rszPointer);
    }

    // Skip "<postfix>"
    if (!SkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip response postfix.\r\n");
        goto Error;
    }

    snprintf(rPSRegStatusInfo.szStat, REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rPSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)uiAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rPSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rPSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE::ParseXREG() - Exit\r\n");
    return bRet;
}

void CTE::StoreRegistrationInfo(void* pRegStruct, BOOL bPSStatus)
{
    RIL_LOG_VERBOSE("CTE::StoreRegistrationInfo() - Enter\r\n");

    /*
     * LAC and CID reported as part of the CS and PS registration status changed URCs
     * are supposed to be the same. But there is nothing wrong in keeping it separately.
     */
    if (bPSStatus)
    {
        P_ND_GPRS_REG_STATUS psRegStatus = (P_ND_GPRS_REG_STATUS) pRegStruct;

        RIL_LOG_INFO("[RIL STATE] GPRS REG STATUS = %s  RAT = %s\r\n",
            PrintGPRSRegistrationInfo(psRegStatus->szStat),
            PrintRAT(psRegStatus->szNetworkType));

        strncpy(m_sPSStatus.szStat, psRegStatus->szStat, sizeof(psRegStatus->szStat));
        strncpy(m_sPSStatus.szLAC, psRegStatus->szLAC, sizeof(psRegStatus->szLAC));
        strncpy(m_sPSStatus.szCID, psRegStatus->szCID, sizeof(psRegStatus->szCID));
        strncpy(m_sPSStatus.szNetworkType, psRegStatus->szNetworkType, sizeof(psRegStatus->szNetworkType));
        m_bPSStatusCached = TRUE;
    }
    else
    {
        P_ND_REG_STATUS csRegStatus = (P_ND_REG_STATUS) pRegStruct;

        RIL_LOG_INFO("[RIL STATE] REG STATUS = %s\r\n", PrintRegistrationInfo(csRegStatus->szStat));

        strncpy(m_sCSStatus.szStat, csRegStatus->szStat, sizeof(csRegStatus->szStat));
        strncpy(m_sCSStatus.szLAC, csRegStatus->szLAC, sizeof(csRegStatus->szLAC));
        strncpy(m_sCSStatus.szCID, csRegStatus->szCID, sizeof(csRegStatus->szCID));

        /*
         * 20111025: framework doesn't make use of technology information
         * sent as part of RIL_REQUEST_REGISTRATION_STATE response.
         */
        strncpy(m_sCSStatus.szNetworkType, csRegStatus->szNetworkType, sizeof(csRegStatus->szNetworkType));
        m_bCSStatusCached = TRUE;
    }

    RIL_LOG_VERBOSE("CTE::StoreRegistrationInfo() - Exit\r\n");
}

void CTE::CopyCachedRegistrationInfo(void* pRegStruct, BOOL bPSStatus)
{
    RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - Enter\r\n");

    if (bPSStatus)
    {
        P_ND_GPRS_REG_STATUS psRegStatus = (P_ND_GPRS_REG_STATUS) pRegStruct;

        memset(psRegStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));
        strncpy(psRegStatus->szStat, m_sPSStatus.szStat, sizeof(psRegStatus->szStat));
        strncpy(psRegStatus->szLAC, m_sPSStatus.szLAC, sizeof(psRegStatus->szLAC));
        strncpy(psRegStatus->szCID, m_sPSStatus.szCID, sizeof(psRegStatus->szCID));
        strncpy(psRegStatus->szNetworkType, m_sPSStatus.szNetworkType, sizeof(psRegStatus->szNetworkType));

        //  Ice Cream Sandwich has new field ((const char **)response)[5] which is
        //  the maximum number of simultaneous data calls.
        snprintf(psRegStatus->szNumDataCalls, REG_STATUS_LENGTH, "%d", (g_uiRilChannelCurMax - RIL_CHANNEL_DATA1));

        psRegStatus->sStatusPointers.pszStat = psRegStatus->szStat;
        psRegStatus->sStatusPointers.pszLAC = psRegStatus->szLAC;
        psRegStatus->sStatusPointers.pszCID = psRegStatus->szCID;
        psRegStatus->sStatusPointers.pszNetworkType = psRegStatus->szNetworkType;
        psRegStatus->sStatusPointers.pszNumDataCalls = psRegStatus->szNumDataCalls; // ICS new field
    }
    else
    {
        P_ND_REG_STATUS csRegStatus = (P_ND_REG_STATUS) pRegStruct;

        memset(csRegStatus, 0, sizeof(S_ND_REG_STATUS));
        strncpy(csRegStatus->szStat, m_sCSStatus.szStat, sizeof(csRegStatus->szStat));
        strncpy(csRegStatus->szLAC, m_sCSStatus.szLAC, sizeof(csRegStatus->szLAC));
        strncpy(csRegStatus->szCID, m_sCSStatus.szCID, sizeof(csRegStatus->szCID));
        strncpy(csRegStatus->szNetworkType, m_sCSStatus.szNetworkType, sizeof(csRegStatus->szNetworkType));

        csRegStatus->sStatusPointers.pszStat = csRegStatus->szStat;
        csRegStatus->sStatusPointers.pszLAC = csRegStatus->szLAC;
        csRegStatus->sStatusPointers.pszCID = csRegStatus->szCID;
        csRegStatus->sStatusPointers.pszNetworkType = csRegStatus->szNetworkType;
        // Note that the remaining fields in the structure have been previously set to NULL (0)
        // by memset().  They are not used in this RIL.
    }

    RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - Exit\r\n");
}

void CTE::ResetRegistrationCache()
{
    m_bCSStatusCached = FALSE;
    m_bPSStatusCached = FALSE;
}

const char* CTE::PrintRegistrationInfo(char *szRegInfo) const
{
    int nRegInfo = atoi(szRegInfo);

    switch (nRegInfo)
    {
        case 0: return "NOT REGISTERED, NOT SEARCHING";
        case 1: return "REGISTERED, HOME NETWORK";
        case 2: return "NOT REGISTERED, SEARCHING";
        case 3: return "REGISTRATION DENIED";
        case 4: return "UNKNOWN";
        case 5: return "REGISTERED, IN ROAMING";
        default: return "UNKNOWN REG STATUS";
    }
}

const char* CTE::PrintGPRSRegistrationInfo(char *szGPRSInfo) const
{
    int nGPRSInfo = atoi(szGPRSInfo);

    switch (nGPRSInfo)
    {
        case 0: return "NOT REGISTERED, HOME NETWORK";
        case 1: return "REGISTERED, HOME NETWORK";
        case 2: return "NOT REGISTERED, SEARCHING";
        case 3: return "REGISTRATION DENIED";
        case 4: return "UNKNOWN";
        case 5: return "REGISTERED, IN ROAMING";
        default: return "UNKNOWN REG STATUS";
    }
}

const char* CTE::PrintRAT(char* szRAT) const
{
    int nRAT = atoi(szRAT);

    switch(nRAT)
    {
        case RADIO_TECH_UNKNOWN: return "RADIO_TECH_UNKNOWN";
        case RADIO_TECH_GPRS: return "RADIO_TECH_GPRS";
        case RADIO_TECH_EDGE: return "RADIO_TECH_EDGE";
        case RADIO_TECH_UMTS: return "RADIO_TECH_UMTS";
        case RADIO_TECH_IS95A: return "RADIO_TECH_IS95A";
        case RADIO_TECH_IS95B: return "RADIO_TECH_IS95B";
        case RADIO_TECH_1xRTT: return "RADIO_TECH_1xRTT";
        case RADIO_TECH_EVDO_0: return "RADIO_TECH_EVDO_0";
        case RADIO_TECH_EVDO_A: return "RADIO_TECH_EVDO_A";
        case RADIO_TECH_HSDPA: return "RADIO_TECH_HSDPA";
        case RADIO_TECH_HSUPA: return "RADIO_TECH_HSUPA";
        case RADIO_TECH_HSPA: return "RADIO_TECH_HSPA";
        case RADIO_TECH_EVDO_B: return "RADIO_TECH_EVDO_B";
        case RADIO_TECH_EHRPD: return "RADIO_TECH_EHRPD";
        case RADIO_TECH_LTE: return "RADIO_TECH_LTE";
        case RADIO_TECH_HSPAP: return "RADIO_TECH_HSPAP";
        default: return "UNKNOWN RAT";
    }
}


//
// ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS (sent internally)
//
RIL_RESULT_CODE CTE::ParseQuerySimSmsStoreStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQuerySimSmsStoreStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQuerySimSmsStoreStatus(rRspData);
}

void CTE::SetIncomingCallStatus(UINT32 uiCallId, UINT32 uiStatus)
{
    RIL_LOG_VERBOSE("CTE::SetIncomingCallStatus() - Enter / Exit\r\n");

    m_pTEBaseInstance->SetIncomingCallStatus(uiCallId, uiStatus);
}

UINT32 CTE::GetIncomingCallId()
{
    RIL_LOG_VERBOSE("CTE::GetIncomingCallId() - Enter / Exit\r\n");

    return m_pTEBaseInstance->GetIncomingCallId();
}
