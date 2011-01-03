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
// Author:  Dennis Peter
// Created: 2007-08-03
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date        Who      Ver   Description
//  ----------  -------  ----  -----------------------------------------------
//  June 03/08  DP       1.00  Established v1.00 based on current code base.
//  May  04/09  CW       1.01  Moved common code to base class, identified
//                             platform-specific implementations, implemented
//                             general code clean-up.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "silo_data.h"

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
//        { "NO CARRIER"  , (PFN_ATRSP_PARSE)&CSilo_Data::ParseNoCarrier },
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

    return fRet;
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////
BOOL CSilo_Data::ParseConnect(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Data::ParseConnect() - Enter\r\n");

    const BYTE* szDummy;
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
BOOL CSilo_Data::ParseNoCarrier(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Data::ParseNoCarrier() - Enter\r\n");

    const BYTE* szDummy;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseNoCarrier() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Data::ParseNoCarrier() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // TODO: this should not send the notification here, a GetDataCallList should
    // be queued to get the current list of APNs
    RIL_LOG_INFO("CSilo_Data::ParseNoCarrier() : TODO: Queue a GetDataCallList request\r\n");

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_DATA_CALL_LIST_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Data::ParseNoCarrier() - Exit\r\n");
    return fRet;
}


