////////////////////////////////////////////////////////////////////////////
// silo_network.cpp                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the network-related
//    RIL components.
//
// Author:  Dennis Peter
// Created: 2007-07-30
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

#include <stdio.h>  // for sscanf

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "callbacks.h"
#include "rildmain.h"
#include "silo_network.h"

//
//
CSilo_Network::CSilo_Network(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Network::CSilo_Network() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+CREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCREG         },
        { "+CGREG: "    , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGREG        },
        { "+CTZV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCTZV         },
        { "+CTZDST: "   , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+XNITZINFO"  , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+PACSP1"     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { ""            , (PFN_ATRSP_PARSE)&CSilo_Network::ParseNULL         }
    };

    m_pATRspTable = pATRspTable;

    RIL_LOG_VERBOSE("CSilo_Network::CSilo_Network() - Exit\r\n");
}

//
//
CSilo_Network::~CSilo_Network()
{
    RIL_LOG_VERBOSE("CSilo_Network::~CSilo_Network() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Network::~CSilo_Network() - Exit\r\n");
}

//  Called in CChannel::SendRILCmdHandleRsp() after AT command is physically sent and
//  a response has been received (or timed out).
BOOL CSilo_Network::PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfTimedOut*/)
{
    if ((ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC == rpCmd->GetRequestID() ||
         ND_REQ_ID_SETNETWORKSELECTIONMANUAL == rpCmd->GetRequestID()) &&
        (RRIL_RESULT_OK == rpRsp->GetResultCode()))
    {
        RIL_LOG_INFO("PostSendCommandHook() - INFO: Manually trigger signal strength update following successful registration\r\n");
        RIL_requestTimedCallback(triggerSignalStrength, NULL, 0);
    }

    return TRUE;
}

//  Called in CChannel::ParseResponse() after CResponse::ParseResponse() is called, and before
//  CCommand::SendResponse() is called.
BOOL CSilo_Network::PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    if ((ND_REQ_ID_GETCURRENTCALLS == rpCmd->GetRequestID()) &&
        (RRIL_RESULT_ERROR == rpRsp->GetResultCode()) &&
        (RADIO_STATE_SIM_LOCKED_OR_ABSENT == g_RadioState.GetRadioState()))
    {
        RIL_LOG_INFO("PostParseResponseHook() - INFO: Override generic error with OK as modem is pin locked\r\n");
        rpRsp->SetResultCode(RRIL_RESULT_OK);
    }
    
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

//
//
BOOL CSilo_Network::ParseCTZV(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCTZV() - Enter\r\n");
    const BYTE* szDummy;
    BOOL fRet = FALSE;
    UINT32 uiDST;
    BOOL bTwoParts = FALSE;
    const int TIME_ZONE_SIZE = 5;
    BYTE szTimeZone[TIME_ZONE_SIZE] = {0};
    BYTE * pszTimeData = (BYTE*)malloc(sizeof(BYTE) * MAX_BUFFER_SIZE);
    if (NULL == pszTimeData)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Could not allocate memory for pszTimeData.\r\n");
        goto Error;
    }

    memset(pszTimeData, 0, sizeof(BYTE) * MAX_BUFFER_SIZE);

    //  The notification could come in as:
    //  +CTZV: <tz>,"<year>/<month>/<day>,<hr>:<min>:<sec>"<postfix>+CTZDST: <dst><postfix>
    //  The CTZDST part is optional.

    //  Check to see if we have a complete CTZV notification.
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }
    else
    {
        //  Check to see if we also have a complete CTZDST notification
        if (!FindAndSkipString(szDummy, "+CTZDST: ", szDummy) ||
            !FindAndSkipRspEnd(szDummy, g_szNewLine, szDummy))
        {
            //  We don't have a complete CTZDST.
            RIL_LOG_INFO("CSilo_Network::ParseCTZV() - INFO: Did not get CTZDST notification\r\n");
            goto Error;
        }
    }

    //  First parse the <tz>,
    if (!ExtractUnquotedString(rszPointer, ",", szTimeZone, TIME_ZONE_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to find time zone!\r\n");
        goto Error;
    }

    if (!ExtractQuotedString(rszPointer, pszTimeData, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to find date/time string!\r\n");
        goto Error;
    }
    
    strncat(pszTimeData, szTimeZone, TIME_ZONE_SIZE);

    if (!SkipRspEnd(rszPointer, g_szNewLine, rszPointer) ||
        !FindAndSkipString(rszPointer, "+CTZDST: ", rszPointer))
    {
        goto Error;
    }

    //  Parse <dst>.
    szDummy = rszPointer;
    if(!ExtractUInt(rszPointer, uiDST, rszPointer))
    {
        goto Error;
    }

    strcat(pszTimeData, ",");
    strncat(pszTimeData, szDummy, rszPointer - szDummy);

    RIL_LOG_INFO("CSilo_Network::ParseCTZV() - INFO: pszTimeData: %s\r\n", pszTimeData);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_NITZ_TIME_RECEIVED);
    
    if (!pResponse->SetData((void*)pszTimeData, sizeof(BYTE*), FALSE))
    {
        fRet = FALSE;
        goto Error;
    }
    

    fRet = TRUE;
Error:
    if (!fRet)
    {
        free(pszTimeData);
        pszTimeData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseCTZV() - Exit\r\n");
    return fRet;
}

// Note that this function will get called for Solicited and Unsolicited
// responses that start with "+CGREG: ". The function will determine whether
// or not it should parse the response (using the number of arguments in the
//  response) and will proceed appropriately.
//
BOOL CSilo_Network::ParseRegistrationStatus(CResponse* const pResponse, const BYTE*& rszPointer, BOOL const bGPRS)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Enter\r\n");

    const BYTE* szDummy;
    BOOL   fRet = FALSE;
    UINT32   uiParamCount = 0;
    const UINT32   uiParamCountMax = 4;
    UINT32   uiParams[uiParamCountMax] = {0, 0, 0, 0};

    if (NULL == pResponse)
    {
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }

    // Valid CREG/CGREG notifications can have from one to four parameters, as follows:
    //       <status>               for an unsolicited notification without location data
    //  <n>, <status>               for a command response without location data
    //       <status>, <lac>, <ci>  for an unsolicited notification with location data
    //  <n>, <status>, <lac>, <ci>  for a command response with location data

    //  3GPP TS 27.007 version 5.4.0 release 5 section 7.2
    //  <n>      valid values: 0, 1, 2
    //  <status> valid values: 0, 1, 2, 3, 4, 5
    //  <lac>    string type, in hex
    //  <ci>     string type, in hex

    // Parse and store parameters
    while (uiParamCount < uiParamCountMax)
    {
        SkipSpaces(rszPointer, rszPointer);

        BOOL fQuote = SkipString(rszPointer, "\"", rszPointer);

        // ok to use ExtractHexUInt() here as
        // valid range of <n> is 0-2 and
        // valid range of <status> is 0-5

        if (!ExtractHexUInt(rszPointer, uiParams[uiParamCount], rszPointer))
        {
            goto Error;
        }

        if (fQuote)
        {
            // skip end quote
            if (!SkipString(rszPointer, "\"", rszPointer))
            {
                goto Error;
            }
        }

        uiParamCount++;

        SkipSpaces(rszPointer, rszPointer);

        if (SkipString(rszPointer, "\r", szDummy))
        {
            // done
            break;
        }
        else if (!SkipString(rszPointer, ",", rszPointer))
        {
             // unexpected token
            goto Error;           
        }          
    }
    

    // We have the parameters, look for the postfix
    if (!SkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - Cannot find postfix  bGPRS=[%d]\r\n", bGPRS);
        goto Error;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - uiParamCount=[%d]\r\n", uiParamCount);
    
    // If there are 2 or 4 parameters, this is not a notification - let the
    // response handler take care of it.
    if ((uiParamCount == 2) || (uiParamCount == uiParamCountMax))
    {
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    if (!bGPRS)
    {
        pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED);
    }

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Network::ParseCREG(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, FALSE);
}

//
//
BOOL CSilo_Network::ParseCGREG(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCGREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, TRUE);
}
