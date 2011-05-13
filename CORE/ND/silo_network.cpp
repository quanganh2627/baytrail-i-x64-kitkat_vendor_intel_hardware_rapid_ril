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
#include "channel_data.h"
#include "te_inf_6260.h"


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
        { "+XREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXREG         },
        { "+CGEV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGEV         },
        { "+CTZV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCTZV         },
        { "+CTZDST: "   , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+XNITZINFO"  , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+PACSP1"     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+XCGEDPAGE:" , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXCGEDPAGE    },
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
        RIL_LOG_INFO("CSilo_Network::PostParseResponseHook() - INFO: Override GET CURRENT CALLS error with OK as SIM is locked or absent\r\n");
        rpRsp->SetResultCode(RRIL_RESULT_OK);
    }
    else if ((ND_REQ_ID_PDPCONTEXTLIST_UNSOL == rpCmd->GetRequestID()) &&
             (RRIL_RESULT_ERROR == rpRsp->GetResultCode()) &&
             (10 == rpRsp->GetErrorCode()))  // CME ERROR: 10 = SIM absent
    {
        RIL_LOG_INFO("CSilo_Network::PostParseResponseHook() - INFO: Override REQUEST PDP CONTEXT LIST error with OK as SIM is locked or absent\r\n");
        rpRsp->SetResultCode(RRIL_RESULT_OK);

        //  Bring down data context internally.
        CChannel_Data* pChannelData = NULL;

        for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
        {
            if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
                continue;

            CChannel_Data* pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[i]);
            if (pChannelData)
            {
                RIL_LOG_INFO("CSilo_Network::PostParseResponseHook() - Setting chnl=[%d] contextID to 0\r\n", i);
                pChannelData->SetContextID(0);
            }
        }

        DataConfigDown();

        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);


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
    const BYTE* szTemp;
    BOOL fRet = FALSE;
    //UINT32 uiDST;
    UINT32 uiHour, uiMins, uiSecs;
    UINT32 uiMonth, uiDay, uiYear;
    time_t ctime_secs;
    struct tm* pGMT;
    BOOL bTwoParts = FALSE;
    const int TIME_ZONE_SIZE = 5;
    BYTE szTimeZone[TIME_ZONE_SIZE] = {0};
    int nTimeZone = 0;
    int nTimeDiff = 0;
    struct tm lt;


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

    //  First parse the <tz>,
    if (!ExtractUnquotedString(rszPointer, ",", szTimeZone, TIME_ZONE_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to find time zone!\r\n");
        goto Error;
    }

    // Extract "<date, time>"
    if (!ExtractQuotedString(rszPointer, pszTimeData, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to find date/time string!\r\n");
        goto Error;
    }

    // Extract time/date values from quoted string
    szTemp = pszTimeData;

    // Extract "yy/mm/dd"
    if ( (!ExtractUInt(szTemp, uiYear, szTemp)) ||
         (!FindAndSkipString(szTemp, "/", szTemp)) ||
         (!ExtractUInt(szTemp, uiMonth, szTemp)) ||
         (!FindAndSkipString(szTemp, "/", szTemp)) ||
         (!ExtractUInt(szTemp, uiDay, szTemp)) )
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to extract yy/mm/dd!\r\n");
        goto Error;
    }

    // Extract "hh:mm:ss"
    if ( (!FindAndSkipString(szTemp, ",", szTemp)) ||
         (!ExtractUInt(szTemp, uiHour, szTemp)) ||
         (!FindAndSkipString(szTemp, ":", szTemp)) ||
         (!ExtractUInt(szTemp, uiMins, szTemp)) ||
         (!FindAndSkipString(szTemp, ":", szTemp)) ||
         (!ExtractUInt(szTemp, uiSecs, szTemp)) )
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to extract hh:mm:ss!\r\n");
        goto Error;
    }

    // Fill in broken-down time struct with local time values
    memset(&lt, 0, sizeof(struct tm));
    lt.tm_sec = uiSecs;
    lt.tm_min = uiMins;
    lt.tm_hour = uiHour;
    lt.tm_mday = uiDay;
    lt.tm_mon = uiMonth - 1;    // num months since Jan, range 0 to 11
    lt.tm_year = uiYear + 100;  // num years since 1900
    //lt.tm_isdst = uiDST;      // DO NOT SET THIS to 1, it will cause error in mktime().

    // Convert broken-down time -> calendar time (secs)
    ctime_secs = mktime(&lt);
    if (-1 == ctime_secs)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCTZV() - ERROR: Unable to convert local to calendar time!\r\n");
        //  Just skip over notification
        free(pszTimeData);
        fRet = TRUE;
        goto Error;
    }

    //RIL_LOG_INFO("ctime_secs = [%u]\r\n", (UINT32)ctime_secs);

    // Convert local time (in calendar, secs) to GMT
    //pGMT = gmtime(&ctime_secs);

    //  The problem here is that Android expects GMT, but modem gives us local time.
    //  We need to calculate how many seconds to add/subtract from the mktime() call.
    //  Also, the szTimeZone from the modem includes DST offset (from my observations).
    //  BUG fix: On very first boot after flash, Android does not know timezone so
    //           gmtime() call doesn't convert to from local to GMT.  Fix is add
    //           offset number of seconds to ctime_secs.
    nTimeZone = atoi(szTimeZone);
    //RIL_LOG_INFO("nTimeZone=[%d]\r\n", nTimeZone);
    nTimeDiff = (60 * 15) * nTimeZone * -1;
    //RIL_LOG_INFO("nTimeDiff=[%d]\r\n", nTimeDiff);
    ctime_secs += (time_t)nTimeDiff;
    //RIL_LOG_INFO("ctime_secs = [%u]\r\n", (UINT32)ctime_secs);
    pGMT = localtime(&ctime_secs);

    // Format date time as "yy/mm/dd,hh:mm:ss"
    memset(pszTimeData, 0, sizeof(BYTE) * MAX_BUFFER_SIZE);
    strftime(pszTimeData, sizeof(BYTE) * MAX_BUFFER_SIZE, "%y/%m/%d,%T", pGMT);

    // Add timezone and dst: "(+/-)tz,dt"
    strncat(pszTimeData, szTimeZone, strlen(szTimeZone));

    //  FIX: The time from the modem includes DST so DON'T set it here!!
    //strcat(pszTimeData, ",");
    //strncat(pszTimeData, szDummy, rszPointer - szDummy);
    strcat(pszTimeData, ",0");

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
    BOOL   fRet = FALSE, fUnSolicited = TRUE;
    int nNumParams = 1;
    BYTE* pszCommaBuffer = NULL;  //  Store notification in here.
                                  //  Note that cannot loop on rszPointer as it may contain
                                  //  other notifications as well.


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

    pszCommaBuffer = new BYTE[szDummy-rszPointer+1];
    if (!pszCommaBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - cannot allocate pszCommaBuffer\r\n");
        goto Error;
    }

    memset(pszCommaBuffer, 0, szDummy-rszPointer+1);
    strncpy(pszCommaBuffer, rszPointer, szDummy-rszPointer);
    //RIL_LOG_INFO("pszCommaBuffer=[%s]\r\n", CRLFExpandedString(pszCommaBuffer,szDummy-rszPointer).GetString() );


    // Valid CREG notifications can have from one to five parameters, as follows:
    //       <status>                      for an unsolicited notification without location data
    //  <n>, <status>                      for a command response without location data
    //       <status>, <lac>, <ci>, <AcT>  for an unsolicited notification with location data
    //  <n>, <status>, <lac>, <ci>, <AcT>  for a command response with location data

    // Valid CGREG notifications can have from one to six parameters, as follows:
    //       <status>                             for an unsolicited notification without location data
    //  <n>, <status>                             for a command response without location data
    //       <status>, <lac>, <ci>, <AcT>, <rac>  for an unsolicited notification with location data
    //  <n>, <status>, <lac>, <ci>, <AcT>, <rac>  for a command response with location data

    //  3GPP TS 27.007 version 5.4.0 release 5 section 7.2
    //  <n>      valid values: 0, 1, 2
    //  <status> valid values: 0, 1, 2, 3, 4, 5
    //  <lac>    string type, in hex
    //  <ci>     string type, in hex
    //  <AcT>    valid values: 0=GSM, 1=GSM Compact, 2=UTRAN
    //  <rac>    (routing area code) string type, in hex (one byte)


    //  Loop and count parameters (separated by comma)
    szDummy = pszCommaBuffer;
    while(FindAndSkipString(szDummy, ",", szDummy))
    {
        nNumParams++;
    }
    //RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - nNumParams=[%d]\r\n", nNumParams);


    if (bGPRS)
    {
        //  The +CGREG case
        //  Unsol is 1 and 5
        if ((1 == nNumParams) || (5 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((2 == nNumParams) || (6 == nNumParams))
        {
            //  Sol is 2 and 6
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - GPRS Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }
        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - CGREG paramcount=%d  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }
    else
    {
        //  The +CREG case
        //  Unsol is 1 and 4
        if ((1 == nNumParams) || (4 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((2 == nNumParams) || (5 == nNumParams))
        {
            // sol is 2 and 5
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }
        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - CREG paramcount=%d  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }

    if (fUnSolicited)
    {
        // Look for the postfix
        if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - Cannot find postfix  bGPRS=[%d]\r\n", bGPRS);
            goto Error;
        }
        rszPointer -= strlen(g_szNewLine);

        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED);
        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
        goto Error;
    }


    fRet = TRUE;
Error:
    delete[] pszCommaBuffer;
    pszCommaBuffer = NULL;
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

//
//
BOOL CSilo_Network::ParseXREG(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXREG() - Enter\r\n");

    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

    BOOL bRet = FALSE;
    const BYTE* szDummy;
    UINT32 n = 0, state = 0;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXREG() - Error: pResponse is NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }

    //  Parse <n>,<state>
    if (!ExtractUInt32(rszPointer, state, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXREG() - Error: Parsing error\r\n");
        goto Error;
    }

    //  Check state and set global variable for network technology
    switch(state)
    {
        case 1:
        // registered, GPRS
        g_uiAccessTechnology = ACT_GPRS;
        break;

        case 2:
        // registered, EDGE
        g_uiAccessTechnology = ACT_EDGE;
        break;

        case 3:
        // registered, WCDMA
        g_uiAccessTechnology = ACT_UMTS;
        break;

        case 4:
        // registered, HSDPA
        g_uiAccessTechnology = ACT_HSDPA;
        break;

        case 5:
        // registered, HSUPA
        g_uiAccessTechnology = ACT_HSUPA;
        break;

        case 6:
        // registered, HSUPA and HSDPA
        g_uiAccessTechnology = ACT_HSPA;
        break;

        default:
        g_uiAccessTechnology = ACT_UNKNOWN;
        break;
    }

    //  Skip to end (Medfield fix for XREG)

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        // This isn't a complete registration notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo::ParseXREG() chnl=[%d] - ERROR: Failed to find postfix in the response.\r\n", m_pChannel->GetRilChannel());
        goto Error;
    }

    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);


    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED);

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_Network::ParseXREG() - Exit\r\n");

    return bRet;
}

//
//
BOOL CSilo_Network::ParseCGEV(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Enter\r\n");

    BOOL bRet = FALSE;
    char* szDummy = NULL;
    CChannel_Data* pDataChannel = NULL;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Error: pResponse is NULL\r\n");
        goto Error;
    }

    szDummy = strstr(rszPointer, "NW DEACT");
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }


    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);

    if (szDummy != NULL)
    {
        pResponse->SetUnrecognizedFlag(TRUE);
        RIL_requestTimedCallback(triggerDeactivateDataCall, NULL, 0, 0);
    }
    else
    {
        //  Flag as unrecognized.
        pResponse->SetUnrecognizedFlag(TRUE);

        //  Trigger data call list changed
        //RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Called timed callback  START\r\n");
        RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
        //RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Called timed callback  END\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Exit\r\n");
    return bRet;
}


//
//
BOOL CSilo_Network::ParseXCGEDPAGE(CResponse *const pResponse, const BYTE* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCGEDPAGE() - Enter\r\n");

    BOOL bRet = FALSE;

    char szTemp[20] = {0};


    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCGEDPAGE() - Error: pResponse is NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>OK<postfix>"
    sprintf(szTemp, "%sOK%s", g_szNewLine, g_szNewLine);
    if (!FindAndSkipRspEnd(rszPointer, szTemp, rszPointer))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }


    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);

    //  Flag as unrecognized.
    pResponse->SetUnrecognizedFlag(TRUE);


    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCGEDPAGE() - Exit\r\n");
    return bRet;
}


