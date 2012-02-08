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
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>  // for sscanf
#include <signal.h>
#include <unistd.h>

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
#include "cutils/tztime.h"
#include "te.h"

//
//
CSilo_Network::CSilo_Network(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Network::CSilo_Network() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+XCSQ:"      , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXCSQ         },
        { "+CREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCREG         },
        { "+CGREG: "    , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGREG        },
        { "+XREG: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXREG         },
        { "+CGEV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseCGEV         },
        { "+CTZV: "     , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+CTZDST: "   , (PFN_ATRSP_PARSE)&CSilo_Network::ParseUnrecognized },
        { "+XNITZINFO"  , (PFN_ATRSP_PARSE)&CSilo_Network::ParseXNITZINFO    },
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

        //  Bring down all data contexts internally.
        CChannel_Data* pChannelData = NULL;

        for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
        {
            if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
                continue;

            CChannel_Data* pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[i]);
            //  We are taking down all data connections here, so we are looping over each data channel.
            //  Don't call DataConfigDown with invalid CID.
            if (pChannelData && pChannelData->GetContextID() > 0)
            {
                RIL_LOG_INFO("CSilo_Network::PostParseResponseHook() - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", i, pChannelData->GetContextID());
                DataConfigDown(pChannelData->GetContextID());
            }
        }

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
BOOL CSilo_Network::ParseXNITZINFO(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXNITZINFO() - Enter\r\n");
    const char* szDummy;
    BOOL fRet = FALSE;
    const int NAME_SIZE = 50;
    const int TIME_ZONE_SIZE = 5;
    const int DATE_TIME_SIZE = 20;
    char szFullName[NAME_SIZE] = {0};
    char szShortName[NAME_SIZE] = {0};
    char szTimeZone[TIME_ZONE_SIZE] = {0};
    char szDateTime[DATE_TIME_SIZE] = {0};

    UINT32 uiDst = 0;
    UINT32 uiHour, uiMins, uiSecs;
    UINT32 uiMonth, uiDay, uiYear;
    char *pszTimeData = NULL;
    BOOL isTimeZoneAvailable = FALSE;

    //  The notification could come in as:
    //  "<fullname>","<shortname>","<tz>","<year>/<month>/<day>,<hr>:<min>:<sec>",<dst><postfix>
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - string rszPointer: %s", rszPointer);

    //  Check to see if we have a complete XNITZINFO notification.
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - This isn't a complete registration notification -- no need to parse it!\r\n");
        goto Error;
    }

    //  Skip  ":",
    if (!SkipString(rszPointer,":", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Skip :\r\n");
        goto Error;
    }

    //  Parse the "<fullname>",
    if (!ExtractQuotedString(rszPointer, szFullName, NAME_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not extract the Full Format Operator Name.\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - Long oper: \"%s\"\r\n", szFullName);

    //  Parse the "<Shortname>"
    if (!ExtractQuotedString(rszPointer, szShortName, NAME_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not extract the Short Format Operator Name.\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - Short oper: \"%s\"\r\n", szShortName);

    //  Parse the <tz>,
    if (!ExtractQuotedString(rszPointer, szTimeZone, TIME_ZONE_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to find time zone!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - szTimeZone: \"%s\"\r\n", szTimeZone);

    // Extract "<date,time>"
    if (!ExtractQuotedString(rszPointer, szDateTime, DATE_TIME_SIZE, rszPointer) ||
        !SkipString(rszPointer, ",", rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to find date/time string!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - szDateTime: \"%s\"\r\n", szDateTime);

    //  Extract <dst>
    if (!ExtractUInt32(rszPointer, uiDst, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Cannot parse <dst>\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - uiDst: \"%u\"\r\n", uiDst);

    if (strlen(szTimeZone) > 0)
    {
        isTimeZoneAvailable = TRUE;
    }

    // If the "<date,time>" (szDateTime) is not empty.
    if (strlen(szDateTime) != 0)
    {
        // Scan the Date and the Time in the szDateTime buffer to check the date and time.
        int num = sscanf(szDateTime, "%2d/%2d/%2d,%2d:%2d:%2d", &uiYear, &uiMonth, &uiDay, &uiHour, &uiMins, &uiSecs);

        // Check the elements number of the date and time.
        if (num != 6)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad element number in the scan szDateTime\r\n");
            goto Error;
        }

        // Check the coherence of the date and time.
        if ((uiMonth > 12) || (uiDay > 31) || (uiHour > 24) || (uiMins > 59) || (uiSecs > 59))
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad date time\r\n");
            goto Error;
        }
    }
    // If the "<date,time>" (szDateTime) is empty.
    else
    {
        /*
         * If timezone is available, then framework expects the RIL to provide
         * the time expressed in UTC(or GMT timzone).
         */
        if (isTimeZoneAvailable)
        {
            time_t ctime_secs;
            struct tm* pGMT;

            // Read the current time.
            ctime_secs = time(NULL);

            // Ckeck ctime_secs.
            if (-1 == ctime_secs)
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Unable to convert local to calendar time!\r\n");
                //  Just skip over notification
                goto Error;
            }

            pGMT = gmtime(&ctime_secs);
            // Ckeck pGMT.
            if (NULL == pGMT)
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - pGMT is NULL!\r\n");
                //  Just skip over notification
                goto Error;
            }

            // Insert the date/time as "yy/mm/dd,hh:mm:ss"
            strftime(szDateTime, DATE_TIME_SIZE, "%y/%m/%d,%T", pGMT);
        }
    }

    // Check the dst.
    if (uiDst > 2)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - bad dst\r\n");
        goto Error;
    }

    /*
     * As per the 3GPP 24.008 specification, if the local time zone has been
     * adjusted for Daylight Saving Time, the network shall indicate this by
     * including the IE Network Daylight Saving Time. This means the DST is
     * always received along with the TimeZone. So, for the case where only
     * time is received but not time zone, no need to update the NITZ to framework.
     */
    if  (isTimeZoneAvailable)
    {
        pszTimeData = (char*)malloc(sizeof(char) * MAX_BUFFER_SIZE);
        if (NULL == pszTimeData)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - Could not allocate memory for pszTimeData.\r\n");
            goto Error;
        }
        memset(pszTimeData, 0, sizeof(char) * MAX_BUFFER_SIZE);

        // Insert the date/time as "yy/mm/dd,hh:mm:ss".
        // Add tz: "+-xx", dst: ",x"
        if (snprintf(pszTimeData, MAX_BUFFER_SIZE-1, "%s%s,%u", szDateTime, szTimeZone, uiDst) == 0)
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseXNITZINFO() - snprintf pszTimeData buffer\r\n");
            goto Error;
        }
        pszTimeData[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix
        RIL_LOG_INFO("CSilo_Network::ParseXNITZINFO() - INFO: pszTimeData: %s\r\n", pszTimeData);

        pResponse->SetResultCode(RIL_UNSOL_NITZ_TIME_RECEIVED);
        if (!pResponse->SetData((void*)pszTimeData, sizeof(char*), FALSE))
        {
            goto Error;
        }

        pResponse->SetUnsolicitedFlag(TRUE);
    }
    else
    {
        pResponse->SetUnrecognizedFlag(TRUE);
    }

    /*
     * Completing RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED will result in
     * framework triggering the RIL_REQUEST_OPERATOR request.
     */
    if (strlen(szFullName) || strlen(szShortName))
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
    }

    fRet = TRUE;
Error:
    if (!fRet)
    {
        free(pszTimeData);
        pszTimeData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseXNITZINFO() - Exit\r\n");
    return fRet;
}

// Note that this function will get called for Solicited and Unsolicited
// responses that start with "+CGREG: ". The function will determine whether
// or not it should parse the response (using the number of arguments in the
//  response) and will proceed appropriately.
//
BOOL CSilo_Network::ParseRegistrationStatus(CResponse* const pResponse, const char*& rszPointer,
                                            SILO_NETWORK_REGISTRATION_TYPES regType)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseRegistrationStatus() - Enter\r\n");

    const char* szDummy;
    BOOL   fRet = FALSE, fUnSolicited = TRUE;
    int nNumParams = 1;
    char* pszCommaBuffer = NULL;  //  Store notification in here.
                                  //  Note that cannot loop on rszPointer as it may contain
                                  //  other notifications as well.

    S_ND_GPRS_REG_STATUS psRegStatus;
    S_ND_REG_STATUS csRegStatus;
    void* pRegStatusInfo = NULL;

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

    pszCommaBuffer = new char[szDummy-rszPointer+1];
    if (!pszCommaBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - cannot allocate pszCommaBuffer\r\n");
        goto Error;
    }

    memset(pszCommaBuffer, 0, szDummy-rszPointer+1);
    strncpy(pszCommaBuffer, rszPointer, szDummy-rszPointer);
    pszCommaBuffer[szDummy-rszPointer] = '\0'; // Klokworks fix

    //RIL_LOG_INFO("pszCommaBuffer=[%s]\r\n", CRLFExpandedString(pszCommaBuffer,szDummy-rszPointer).GetString() );


    // Valid XREG notifications can have from one to five parameters, as follows:
    //       <status>, <AcT>, <Band>                for an unsolicited notification without location data
    //  <n>, <status>, <AcT>, <Band>                for a command response without location data
    //       <status>, <AcT>, <Band>, <lac>, <ci>   for an unsolicited notification with location data
    //  <n>, <status>, <AcT>, <Band>, <lac>, <ci>   for a command response with location data

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


    if (SILO_NETWORK_CGREG == regType)
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
    else if (SILO_NETWORK_CREG == regType)
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
    else if (SILO_NETWORK_XREG == regType)
    {
        //  The +XREG case
        //  Unsol is 3 and 5
        if ((3 == nNumParams) || (5 == nNumParams))
        {
            fUnSolicited = TRUE;
        }
        else if ((4 == nNumParams) || (6 == nNumParams))
        {
            //  Sol is 4 and 6
            fUnSolicited = FALSE;
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Network::ParseRegistrationStatus() - Unknown param count=%d\r\n", nNumParams);
            fUnSolicited = TRUE;
        }

        RIL_LOG_INFO("CSilo_Network::ParseRegistrationStatus() - XREG paramcount=%d  fUnsolicited=%d\r\n", nNumParams, fUnSolicited);
    }

    if (fUnSolicited)
    {
        if (SILO_NETWORK_CGREG == regType)
        {
            fRet = CTE::ParseCGREG(rszPointer, fUnSolicited, psRegStatus);
            pRegStatusInfo = (void*) &psRegStatus;
        }
        else if (SILO_NETWORK_CREG == regType)
        {
            fRet = CTE::ParseCREG(rszPointer, fUnSolicited, csRegStatus);
            pRegStatusInfo = (void*) &csRegStatus;
        }
        else if (SILO_NETWORK_XREG == regType)
        {
            fRet = CTE::ParseXREG(rszPointer, fUnSolicited, psRegStatus);
            pRegStatusInfo = (void*) &psRegStatus;
        }

        if (!fRet)
            goto Error;
        else
            CTE::GetTE().StoreRegistrationInfo(pRegStatusInfo, (SILO_NETWORK_CREG == regType) ? false : true);

        rszPointer -= strlen(g_szNewLine);

        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
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
BOOL CSilo_Network::ParseCREG(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, SILO_NETWORK_CREG);
}

//
//
BOOL CSilo_Network::ParseCGREG(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseCGREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, SILO_NETWORK_CGREG);
}

//
//
BOOL CSilo_Network::ParseXREG(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXREG() - Enter / Exit\r\n");
    return ParseRegistrationStatus(pResponse, rszPointer, SILO_NETWORK_XREG);
}

//
//
BOOL CSilo_Network::ParseCGEV(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Enter\r\n");

    BOOL bRet = FALSE;
    const char* szStrExtract = NULL;
    const char* szResponse = NULL;
    UINT32 nCID = 0;
    UINT32 nREASON=0;
    CChannel_Data* pChannelData = NULL;

    szResponse = rszPointer;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - pResponse is NULL\r\n");
        goto Error;
    }
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szResponse))
    {
        // This isn't a complete registration notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - This isn't a complete registration notification -- no need to parse it\r\n");
        goto Error;
    }

    //  Back up over the "\r\n".
    szResponse -= strlen(g_szNewLine);

    //  Format is "ME PDN ACT, <cid>[, <reason>]"
    if (FindAndSkipString(rszPointer, "ME PDN ACT", szStrExtract))
    {
        if (!ExtractUInt32(szStrExtract, nCID, szStrExtract))
        {
            goto Error;
        }
        else
        {
            RIL_LOG_INFO(
                    "Silo_Network::ParseCGEV() - ME PDN ACT , extracted cid=[%d]\r\n",
                    nCID);
        }

        pChannelData = CChannel_Data::GetChnlFromContextID(nCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_CRITICAL("Silo_Network::ParseCGEV() - Invalid CID=[%d], no data channel found!\r\n", nCID);
            goto Error;
        }
        if (!FindAndSkipString(szStrExtract, ",", szStrExtract))
        {
            pChannelData->m_iStatus = PDP_FAIL_NONE;
        }
        else
        {
            if (!ExtractUInt32(szStrExtract, nREASON, szStrExtract))
            {
                RIL_LOG_CRITICAL(
                        "CSilo_Network::ParseCGEV() - Couldn't extract reason\r\n");
                goto Error;
            }
            else
            {
                RIL_LOG_INFO(
                        "Silo_Network::ParseCGEV() - ME PDN ACT , extracted reason=[%d]\r\n",
                        nREASON);
                pResponse->SetUnsolicitedFlag(TRUE);

                // IPV4 only allowed
                if (nREASON == 0)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    pChannelData->m_iStatus = PDP_FAIL_ONLY_IPV4_ALLOWED;
                }
                // IPV6 only allowed
                else if (nREASON == 1)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    pChannelData->m_iStatus = PDP_FAIL_ONLY_IPV6_ALLOWED;
                }
                // Single bearrer allowed
                else if (nREASON == 2)
                {
                    pResponse->SetResultCode(RRIL_RESULT_OK);
                    pChannelData->m_iStatus = PDP_FAIL_ONLY_SINGLE_BEARER_ALLOWED;
                }
                else
                {
                    RIL_LOG_CRITICAL(
                        "CSilo_Network::ParseCGEV() - reason unknown\r\n");
                    pChannelData->m_iStatus = PDP_FAIL_ERROR_UNSPECIFIED;
                    goto Error;
                }
            }
        }
    }
    else
    {

        szStrExtract = strstr(rszPointer, "NW DEACT");

        if (szStrExtract != NULL)
        {
            //  Now we need to get the CID from the notification
            UINT32 nCID = 0;

            //  Format is "NW DEACT "IP", "xx.xx.xx.xx",<cid>"
            if (!FindAndSkipString(szStrExtract, ",", szStrExtract) ||
                !FindAndSkipString(szStrExtract, ",", szStrExtract))
            {
                RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Couldn't find 2 commas to find cid\r\n");
            }
            else
            {
                //  Should be at <cid> now.
                if (!ExtractUInt32(szStrExtract, nCID, szStrExtract))
                {
                    RIL_LOG_CRITICAL("CSilo_Network::ParseCGEV() - Couldn't extract cid\r\n");
                    //  Just trigger normal DataCallListChanged - Let Android process
                    RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
                    bRet = TRUE;
                    goto Error;
                }
                else
                {
                    RIL_LOG_INFO("Silo_Network::ParseCGEV() - NE DEACT , extracted cid=[%d]\r\n", nCID);
                }
            }
            if (nCID > 0)
            {
                //  Explicitly deactivate context ID = nCID
                RIL_requestTimedCallback(triggerDeactivateDataCall, (void*)nCID, 0, 0);
            }
            else
            {
                //  Just trigger normal DataCallListChanged - Let Android process
                RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
            }
        }
        else
        {
            //  Trigger data call list changed - Let Android process
            RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
        }
    }

    bRet = TRUE;
Error:
    rszPointer = szResponse;
    RIL_LOG_INFO("CSilo_Network::ParseCGEV() - Exit\r\n");
    return bRet;
}


//
//
BOOL CSilo_Network::ParseXCGEDPAGE(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCGEDPAGE() - Enter\r\n");

    BOOL bRet = FALSE;

    const int nBufLen = 20;
    char szTemp[nBufLen] = {0};


    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCGEDPAGE() - pResponse is NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>OK<postfix>"
    snprintf(szTemp, nBufLen-1, "%sOK%s", g_szNewLine, g_szNewLine);
    szTemp[nBufLen-1] = '\0';  //  KW fix
    if (!FindAndSkipRspEnd(rszPointer, szTemp, rszPointer))
    {
        // This isn't a complete registration notification -- no need to parse it
        goto Error;
    }


    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);

    //  Flag as unrecognized.
    //pResponse->SetUnrecognizedFlag(TRUE);


    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCGEDPAGE() - Exit\r\n");
    return bRet;
}

//
//
BOOL CSilo_Network::ParseXCSQ(CResponse *const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Network::ParseXCSQ() - Enter\r\n");

    BOOL bRet = FALSE;
    const char* szDummy;
    UINT32 uiRSSI = 0, uiBER = 0;
    RIL_SignalStrength_v6* pSigStrData = NULL;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - pResponse is NULL\r\n");
        goto Error;
    }

    pSigStrData = (RIL_SignalStrength_v6*)malloc(sizeof(RIL_SignalStrength_v6));
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }
    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength_v6));

    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ: Failed to find rsp end!\r\n");
        goto Error;
    }

    if (!ExtractUInt32(rszPointer, uiRSSI, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - Could not extract uiRSSI.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiBER, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Network::ParseXCSQ() - Could not extract uiBER.\r\n");
        goto Error;
    }

    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    pSigStrData->CDMA_SignalStrength.dbm=-1;
    pSigStrData->CDMA_SignalStrength.ecio=-1;
    pSigStrData->EVDO_SignalStrength.dbm=-1;
    pSigStrData->EVDO_SignalStrength.ecio=-1;
    pSigStrData->EVDO_SignalStrength.signalNoiseRatio=-1;
    pSigStrData->LTE_SignalStrength.signalStrength=-1;
    pSigStrData->LTE_SignalStrength.rsrp=-1;
    pSigStrData->LTE_SignalStrength.rsrq=-1;
    pSigStrData->LTE_SignalStrength.rssnr=-1;
    pSigStrData->LTE_SignalStrength.cqi=-1;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_SIGNAL_STRENGTH);

    if (!pResponse->SetData((void*)pSigStrData, sizeof(RIL_SignalStrength_v6), FALSE))
    {
        goto Error;
    }

    bRet = TRUE;

Error:
    if (!bRet)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Network::ParseXCSQ() - Exit\r\n");
    return bRet;
}
