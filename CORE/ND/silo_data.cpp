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
#include "te_inf_6260.h"
#include "rildmain.h"
#include "callbacks.h"

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
    else if ((ND_REQ_ID_SETUPDEFAULTPDP == rpCmd->GetRequestID()) && (RRIL_RESULT_OK != rpRsp->GetResultCode()))
    {
        //  Some error with PDP activation.  We got a CME ERROR to the PDP activate command.
        UINT32 uiChannel = rpCmd->GetChannel();
        CChannel_Data* pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[uiChannel]);
        if (pChannelData)
        {
            UINT32 uiCid = pChannelData->GetContextID();

            //  Reset the CID on this data channel to 0.  Free up channel for future use.
            //  This is done by DataConfigDown() function.
            //  Release network interface
            RIL_LOG_INFO("CSilo_Data::PostParseResponseHook - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", uiChannel, uiCid);
            if (!DataConfigDown(uiCid))
            {
                RIL_LOG_CRITICAL("CSilo_Data::PostParseResponseHook - DataConfigDown FAILED chnl=[%d], cid=[%d]\r\n", uiChannel, uiCid);
            }

            // get last data call fail cause
            fRet = ParseDataCallFailCause(rpRsp, uiCid);
        }
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

// helper functions

BOOL CSilo_Data::ParseDataCallFailCause(CResponse*& rpRsp, UINT32 uiCid)
{
    RIL_LOG_VERBOSE("CSilo_Data::ParseDataCallFailCause() - Enter\r\n");

    BOOL bRetVal = FALSE;
    RIL_Data_Call_Response_v6* pDataCallResp = NULL;

    rpRsp->FreeData();
    rpRsp->SetResultCode(RIL_E_SUCCESS);

    pDataCallResp = (RIL_Data_Call_Response_v6 *) malloc(sizeof(RIL_Data_Call_Response_v6));
    if (NULL == pDataCallResp)
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Unable to allocate memory for RIL_Data_Call_Response_v6\r\n");
        goto Error;
    }
    memset(pDataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));

    switch (rpRsp->GetErrorCode())
    {
        case RRIL_CEER_CAUSE_OPTION_NOT_SUPPORTED:
            pDataCallResp->status = PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
            RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Service option not supported!\r\n");
            break;

        case RRIL_CEER_CAUSE_OPTION_NOT_SUBSCRIBED:
            pDataCallResp->status = PDP_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
            RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Requested service option not subscribed!\r\n");
            break;

        case RRIL_CEER_CAUSE_OPTION_TEMP_OUT_OF_ORDER:
            pDataCallResp->status = PDP_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
            RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Service option temorarily out of order!\r\n");
            break;

        case RRIL_CEER_CAUSE_PDP_AUTHENTICATION_FAILURE:
            pDataCallResp->status = PDP_FAIL_USER_AUTHENTICATION;
            RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : PDP authentication failure!\r\n");
            break;

        default:
            RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Unknown data call fail cause [%d]\r\n", rpRsp->GetErrorCode());
            free(pDataCallResp);
            pDataCallResp = NULL;
            // return GENERIC FAILURE without any returned data
            rpRsp->SetResultCode(RRIL_RESULT_ERROR);
            bRetVal = TRUE;
            goto Error;
    }

    pDataCallResp->suggestedRetryTime = -1;
    pDataCallResp->cid = (int)uiCid;
    pDataCallResp->active = 0;
    pDataCallResp->type = NULL;
    pDataCallResp->ifname = NULL;
    pDataCallResp->addresses = NULL;
    pDataCallResp->dnses = NULL;
    pDataCallResp->gateways = NULL;

    // Don't copy the memory, just pass along the pointer as is.
    if (!rpRsp->SetData((void*) pDataCallResp, sizeof(RIL_Data_Call_Response_v6), FALSE))
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseDataCallFailCause : Unable to set data with data call fail cause\r\n");
        goto Error;
    }

    bRetVal = TRUE;
Error:
    if (!bRetVal)
    {
        free(pDataCallResp);
        pDataCallResp = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Data::ParseDataCallFailCause() - Exit\r\n");

    return bRetVal;
}
