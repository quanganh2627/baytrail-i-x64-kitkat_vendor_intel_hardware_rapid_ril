////////////////////////////////////////////////////////////////////////////
// silo_data.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the data-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "silo_data.h"
#include "channel_data.h"
#include "data_util.h"
#include "rildmain.h"
#include "callbacks.h"
#include "te.h"

//
//
CSilo_Data::CSilo_Data(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Data::CSilo_Data() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
//        { "+++3"     , (PFN_ATRSP_PARSE)&CSilo_Data::ParseTriplePlus   },
//        { "CONNECT"  , (PFN_ATRSP_PARSE)&CSilo_Data::ParseConnect      },
        { "NO CARRIER"  , (PFN_ATRSP_PARSE)&CSilo_Data::ParseNoCarrier },
        { "+XCIEV: "      , (PFN_ATRSP_PARSE)&CSilo_Data::ParseUnrecognized },
        { "+XCIEV:"      , (PFN_ATRSP_PARSE)&CSilo_Data::ParseUnrecognized },
        { "RING"         , (PFN_ATRSP_PARSE)&CSilo_Data::ParseUnrecognized },
        { ""         , (PFN_ATRSP_PARSE)&CSilo_Data::ParseNULL         }
    };

    m_pATRspTable = pATRspTable;

    RIL_LOG_VERBOSE("CSilo_Data::CSilo_Data() - Exit\r\n");
}

//
//
CSilo_Data::~CSilo_Data()
{
    RIL_LOG_VERBOSE("CSilo_Data::~CSilo_Data() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Data::~CSilo_Data() - Exit\r\n");
}

//  Called in CChannel::SendRILCmdHandleRsp() after AT command is physically sent and
//  a response has been received (or timed out).
BOOL CSilo_Data::PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_Data::PostSendCommandHook() - Enter\r\n");
    if (ND_REQ_ID_SETUPDEFAULTPDP == rpCmd->GetRequestID() &&
            NULL != rpRsp && rpRsp->IsTimedOutFlag())
    {
        RIL_LOG_INFO("CSilo_Data::PostSendCommandHook() - Setup data call timed out\r\n");
        CTE::GetTE().SetupDataCallOngoing(false);
    }

    RIL_LOG_VERBOSE("CSilo_Data::PostSendCommandHook() - Exit\r\n");
    return TRUE;
}

BOOL CSilo_Data::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    if (ND_REQ_ID_SETUPDEFAULTPDP == rpCmd->GetRequestID())
    {
        CTE::GetTE().SetupDataCallOngoing(false);
    }

    return TRUE;
}

//
//
BOOL CSilo_Data::PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfTimedOut*/)
{
    BOOL fRet = TRUE;

    RIL_LOG_VERBOSE("CSilo_Data::PostParseResponseHook rpRsp->GetResultCode() = %d\r\n", rpRsp->GetResultCode());

    if ((ND_REQ_ID_SETUPDEFAULTPDP == rpCmd->GetRequestID()) && (RRIL_RESULT_OK == rpRsp->GetResultCode()))
    {
        rpRsp->SetUnsolicitedFlag(FALSE);
    }

    return fRet;
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////
BOOL CSilo_Data::ParseConnect(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Data::ParseConnect() - Enter\r\n");

    const char* szDummy;
    BOOL fRet = FALSE;
    UINT32 uiBaud = 0;

    if (NULL == pResponse)
    {
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        //  incomplete notification
        goto Error;
    }

    //  CONNECT 9600000<cr><lf>
    //  Check for possible space
    if (SkipString(rszPointer, " ", rszPointer))
    {
        ExtractUInt32(rszPointer, uiBaud, rszPointer);
    }

    pResponse->SetUnsolicitedFlag(FALSE);
    pResponse->SetResultCode(RRIL_RESULT_OK);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Data::ParseConnect() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_Data::ParseNoCarrier(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() - Enter\r\n");

    const char* szDummy;
    BOOL fRet = FALSE;

    CChannel_Data* pChannelData = NULL;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseNoCarrier() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseNoCarrier() : Could not find response end\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    //pResponse->SetResultCode(RIL_UNSOL_DATA_CALL_LIST_CHANGED);
    //RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() - Called timed callback  START\r\n");
    //RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
    //RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() - Called timed callback  END\r\n");


    // Free this channel's context ID.
    pChannelData = CChannel_Data::GetChnlFromRilChannelNumber(m_pChannel->GetRilChannel());
    if (pChannelData)
    {
        RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() : Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", m_pChannel->GetRilChannel(), pChannelData->GetContextID());

        //  Release network interface
        if (!DataConfigDown(pChannelData->GetContextID()))
        {
            RIL_LOG_CRITICAL("CSilo_Data::ParseNoCarrier() - DataConfigDown FAILED chnl=[%d], cid=[%d]\r\n", m_pChannel->GetRilChannel(), pChannelData->GetContextID());
        }

    }
    fRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() - Exit\r\n");
    return fRet;
}
