////////////////////////////////////////////////////////////////////////////
// silo_voice.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the voice-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "rildmain.h"
#include "silo_voice.h"
#include "callbacks.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//
//
CSilo_Voice::CSilo_Voice(CChannel *pChannel)
: CSilo(pChannel),
  m_uiCallId(0)
{
    RIL_LOG_VERBOSE("CSilo_Voice::CSilo_Voice() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+CRING: "      , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseExtRing },
        { "DISCONNECT"  , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseDISCONNECT },
        { "+XCALLSTAT: "  , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseXCALLSTAT },
        { "CONNECT"       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseConnect },
        { "+CCWA: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCallWaitingInfo },
        { "+CSSU: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUnsolicitedSSInfo },
        { "+CSSI: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseIntermediateSSInfo },
        { "+CCCM: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCallMeter },
        { "+CUSD: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUSSDInfo },
        { "+COLP: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseConnLineIdPresentation },
        { "+COLR: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseConnLineIdRestriction },
        { "+XCIEV: "      , (PFN_ATRSP_PARSE)/*&CSilo_Voice::ParseIndicatorEvent*/ &CSilo_Voice::ParseUnrecognized },
        { "+XCIEV:"      , (PFN_ATRSP_PARSE)/*&CSilo_Voice::ParseIndicatorEvent*/ &CSilo_Voice::ParseUnrecognized },
        { "+XCALLINFO: "  , (PFN_ATRSP_PARSE)/*&CSilo_Voice::ParseCallProgressInformation*/ &CSilo_Voice::ParseUnrecognized },
        { "RING CTM"      , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUnrecognized },
        { "RING"          , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUnrecognized },
        { "BUSY"          , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseBusy },
        { "NO ANSWER"     , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseNoAnswer },
        { "CTM CALL"      , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCTMCall },
        { "NO CTM CALL"   , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseNoCTMCall },
#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)
        // Handle Call failed cause unsolicited notification here
        { "+XCEER: " , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCallFailedCause },
#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED
        { ""              , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_Voice::CSilo_Voice() - Exit\r\n");
}

//
//
CSilo_Voice::~CSilo_Voice()
{
    RIL_LOG_VERBOSE("CSilo_Voice::~CSilo_Voice() - Enter / Exit\r\n");

}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

BOOL CSilo_Voice::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    if (ND_REQ_ID_HANGUP == rpCmd->GetRequestID())
    {
        // If this is a hangup command and we got a NO CARRIER response, turn it into OK
        if (RRIL_RESULT_NOCARRIER == rpRsp->GetResultCode())
        {
            rpRsp->FreeData();
            rpRsp->SetResultCode(RRIL_RESULT_OK);
        }
    }

    /*
     * Android framework queries the call status upon completion of each call operation or
     * upon the receival of RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED event.  Due to this, on some
     * use cases, wrong call UI is shown to the user.  Android framework has been modified to
     * query the call status only upon receival of RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED
     * event.  In case of dial rejected due to FDN failure, only NO CARRIER will be sent by modem.
     * So, RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED needs to be triggered upon the dial failure.
     */
    if (ND_REQ_ID_DIAL == rpCmd->GetRequestID()
#if defined(M2_VT_FEATURE_ENABLED)
        || ND_REQ_ID_DIALVT == rpCmd->GetRequestID()
#endif // M2_VT_FEATURE_ENABLED
    )
    {
        notifyChangedCallState(NULL);
    }

    return TRUE;
}

//
//
BOOL CSilo_Voice::ParseExtRing(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseExtRing() - Enter\r\n");

    BOOL fRet = FALSE;
    const char* szDummy = NULL;
    char szType[MAX_BUFFER_SIZE] = {0};

    // Make sure this is a complete notification
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseExtRing() : ERROR : incomplete notification\r\n");
        goto Error;
    }

    //  Extract <type>
    if (!ExtractUnquotedString(rszPointer, g_cTerminator, szType, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseExtRing() : ERROR : cannot extract <type>\r\n");
        goto Error;
    }

    // Skip to end
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseExtRing() : ERROR : Couldn't find end of notification\r\n");
        goto Error;
    }
    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    //  Determine what kind of RING this is.
    if (0 == strcmp(szType, "VOICE") || 0 == strcmp(szType, "CTM"))
    {
        //  Normal case, just send ring notification.
        RIL_LOG_INFO("CSilo_Voice::ParseExtRing() : Incoming voice call\r\n");
        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_CALL_RING);

        /*
         * XCALLSTAT received before CRING.
         * Since the call type is not known via XCALLSTAT URC,
         * CALL_STATE_CHANGED notification is triggered from here.
         */
        notifyChangedCallState(NULL);
    }
    //  Check to see if incoming video telephony call
    else if (0 == strcmp(szType, "SYNC"))
    {
        RIL_LOG_INFO("CSilo_Voice::ParseExtRing() : Incoming video telephony call\r\n");

        //  TODO: Send notification for video telephony incoming call
        //        For now, just do normal ring
#if defined(M2_VT_FEATURE_ENABLED)
        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_CALL_RING);

        /*
         * XCALLSTAT received before CRING.
         * Since the call type is not known via XCALLSTAT URC,
         * CALL_STATE_CHANGED notification is triggered from here.
         */
        notifyChangedCallState(NULL);
#else
        pResponse->SetUnrecognizedFlag(TRUE);

        triggerHangup(m_uiCallId);
#endif // M2_VT_FEATURE_ENABLED
    }

    else
    {
        //  Unknown incoming ring
        RIL_LOG_INFO("CSilo_Voice::ParseExtRing() : Unknown incoming call type=[%s]\r\n", szType);
        pResponse->SetUnrecognizedFlag(TRUE);
        triggerHangup(m_uiCallId);
    }
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseExtRing() - Exit\r\n");
    return fRet;
}

#undef Snprintf


//
//
BOOL CSilo_Voice::ParseConnect(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnect() - Enter\r\n");

    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnect() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnect() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnect() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_Voice::ParseXCALLSTAT(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseXCALLSTAT() - Enter\r\n");

    char szAddress[MAX_BUFFER_SIZE];
    BOOL fRet = FALSE;

    const char * szDummy = NULL;
    UINT32 uiID = 0;
    UINT32 uiStat = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() : ERROR : Incomplete notification\r\n");
        goto Error;
    }

    //  Extract <id>
    if (!ExtractUInt32(rszPointer, uiID, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() : ERROR : Could not extract uiID\r\n");
        goto Error;
    }

    //  Extract ,<stat>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiStat, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() : ERROR : Could not extract uiStat\r\n");
        goto Error;
    }

    switch (uiStat)
    {
        case E_CALL_STATUS_INCOMING:
        case E_CALL_STATUS_WAITING:
            /*
             * Since the call type is not known, CALL_STATE_CHANGED is postponed
             * until the call type is known.
             * For incoming call, +CRING: provides the call type.
             * For waiting call, +CCWA: provides the call type.
             */
            m_uiCallId = uiID;
            pResponse->SetUnrecognizedFlag(TRUE);
            break;
        case E_CALL_STATUS_DISCONNECTED:
            m_uiCallId = 0;
            // set the flag to clear all pending chld requests
            g_clearPendingChlds = true;
            // Fall through
        default:
            pResponse->SetUnsolicitedFlag(TRUE);
            pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
            break;
    }

#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)
    //  If <stat> = 6 (6 is disconnected), store in CSystemManager.
    if (6 == uiStat)
    {
        RIL_LOG_INFO("CSilo_Voice::ParseXCALLSTAT() : Received disconnect, uiID=[%d]\r\n", uiID);
        //  Store last disconnected call ID.
        CSystemManager::GetInstance().SetLastCallFailedCauseID(uiID);

        //  We need to queue AT+XCEER command
        //  Let RIL framework handle the +XCEER response as a notification.  No parse function needed here.
        CCommand *pCmd = NULL;
        pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_LASTCALLFAILCAUSE], NULL, ND_REQ_ID_LASTCALLFAILCAUSE, "AT+XCEER\r");
        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() - ERROR: Unable to queue AT+XCEER command!\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() - ERROR: Unable to allocate memory for new AT+XCEER command!\r\n");
            goto Error;
        }
    }

#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED


    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseXCALLSTAT() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseXCALLSTAT() - Exit\r\n");
    return fRet;
}


//
//
//
BOOL CSilo_Voice::ParseCallWaitingInfo(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallWaitingInfo() - Enter\r\n");

    char szAddress[MAX_BUFFER_SIZE];
    BOOL fRet = FALSE;
    char* pszTempBuffer = NULL;
    int nNumParams = 1;
    const char* szDummy;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    pszTempBuffer = new char[szDummy-rszPointer+1];
    if (!pszTempBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() - cannot allocate pszTempBuffer\r\n");
        goto Error;
    }
    memset(pszTempBuffer, 0, szDummy-rszPointer+1);
    strncpy(pszTempBuffer, rszPointer, szDummy-rszPointer);
    pszTempBuffer[szDummy-rszPointer] = '\0';  // Klokworks fix

    //  Loop and count parameters
    szDummy = pszTempBuffer;
    while(FindAndSkipString(szDummy, ",", szDummy))
    {
        nNumParams++;
    }

    RIL_LOG_INFO("CSilo_Voice::ParseCallWaitingInfo(): Number of parameters in +CCWA=%d\r\n", nNumParams);
    //If number of parameters is more than 2, +CCWA came as part of voice call and is a URC
    //otherwise this is a response to AT+CCWA= command and need not be processed here.
    if (nNumParams > 2)
    {
        const char* pDummy;
        char number[MAX_BUFFER_SIZE];
        UINT32 uiType = 0;
        UINT32 uiClass = 0;

        // Look for a "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pDummy))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : Incomplete notification\r\n");
            goto Error;
        }

        if (!ExtractQuotedString(rszPointer, number, MAX_BUFFER_SIZE, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo : Error Could not extract <number>\r\n");
            goto Error;
        }

        //  Extract <type>
        if (!SkipString(rszPointer, ",", rszPointer) ||
                !ExtractUInt32(rszPointer, uiType, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : Could not extract <type>\r\n");
            goto Error;
        }

        //  Extract ,<class>
        if (!SkipString(rszPointer, ",", rszPointer) ||
                !ExtractUInt32(rszPointer, uiClass, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : Could not extract <class>\r\n");
            goto Error;
        }

        if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : Could not find response end\r\n");
            goto Error;
        }

        switch (uiClass)
        {
            case 1: // Voice
                pResponse->SetUnsolicitedFlag(TRUE);
                pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
                break;
            default:
                pResponse->SetUnrecognizedFlag(TRUE);
                triggerHangup(m_uiCallId);
                break;
        }

        // Walk back over the <CR><LF>
        rszPointer -= strlen(g_szNewLine);


        fRet = TRUE;
    }
    else
    {
        //  This is a solicited response
        pResponse->SetUnsolicitedFlag(FALSE);
    }

Error:
    delete[] pszTempBuffer;
    pszTempBuffer = NULL;
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallWaitingInfo() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_Voice::ParseUnsolicitedSSInfo(CResponse* const pResponse, const char*& szPointer)
{
// Parsing for the +CSSU notification. The format is:
//    "<code2>[,<index>[,<address>,<type>[,<subaddress>,<satype>]]]"

    RIL_LOG_VERBOSE("CSilo_Voice::ParseUnsolicitedSSInfo() - Enter\r\n");

    UINT32 nValue;
    const char* szPostfix;
    const char* szDummy;
    BOOL fRet = FALSE;
    RIL_SuppSvcNotification * pSuppSvcBlob = NULL;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    if (!FindAndSkipRspEnd(szPointer, g_szNewLine, szPostfix))
    {
        // This isn't a complete Supplementary services notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo: Failed to find rsp end!\r\n");
        goto Error;
    }

    // We need to alloc the memory for the string as well to ensure the data gets passed along to the socket
    pSuppSvcBlob = (RIL_SuppSvcNotification*)malloc(sizeof(RIL_SuppSvcNotification) + (sizeof(char) * MAX_BUFFER_SIZE));
    if (NULL == pSuppSvcBlob)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo: Failed to alloc pSuppSvcBlob!\r\n");
        goto Error;
    }

    memset(pSuppSvcBlob, 0, sizeof(RIL_SuppSvcNotification) + (sizeof(char) * MAX_BUFFER_SIZE));

    // This is an MT Notification
    pSuppSvcBlob->notificationType = 1;

    // Extract <code2>
    if (ExtractUInt32(szPointer, nValue, szPointer) && (5 != nValue))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Found code: %d\r\n", nValue);

        pSuppSvcBlob->code = nValue;

        // Extract [,<index>
        if (!SkipString(szPointer, ",", szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Goto continue\r\n");
            goto Continue;
        }
        if (ExtractUInt32(szPointer, nValue, szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Found index: %d\r\n", nValue);
            pSuppSvcBlob->index = nValue;
        }

        // Extract [,<number>
        if (!SkipString(szPointer, ",", szPointer))
        {
            goto Continue;
        }

        // Setup the char* number to use the extra memory at the end of the struct
        pSuppSvcBlob->number = (char*)((UINT32)pSuppSvcBlob + sizeof(RIL_SuppSvcNotification));

        if(!ExtractQuotedString(szPointer, pSuppSvcBlob->number, MAX_BUFFER_SIZE, szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Error during extraction of number\r\n");
            goto Error;
        }

        // Skip "," and look for <type>
        if (SkipString(szPointer, ",", szPointer))
        {
            if(!ExtractUInt32(szPointer, nValue, szPointer))
            {
                RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Error during extraction of type\r\n");
                goto Error;
            }

            pSuppSvcBlob->type = nValue;
        }

        // Android doesn't support subaddr or satype fields so just skip to the end

Continue:
        // We have the parameters, look for the postfix
        if (!FindAndSkipRspEnd(szPointer, g_szNewLine, szDummy))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Didn't find rsp end!\r\n");
            szPointer = szPostfix - strlen(g_szNewLine);
            pResponse->SetUnrecognizedFlag(TRUE);
            free(pSuppSvcBlob);
            pSuppSvcBlob = NULL;
            fRet = TRUE;
            goto Error;
        }

        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_SUPP_SVC_NOTIFICATION);

        if (!pResponse->SetData((void*)pSuppSvcBlob, sizeof(RIL_SuppSvcNotification), FALSE))
        {
            free(pSuppSvcBlob);
            pSuppSvcBlob = NULL;
            goto Error;
        }
        else
        {
            fRet = TRUE;
        }
    }
    else if (5 == nValue)
    {
        RIL_LOG_INFO("CSilo_Voice::ParseUnsolicitedSSInfo : Found nValue 5, reporting call state changed\r\n");
        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
        free(pSuppSvcBlob);
        pSuppSvcBlob = NULL;
        fRet = TRUE;
    }

Error:
    if (!fRet)
    {
        free(pSuppSvcBlob);
        pSuppSvcBlob = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Voice::ParseUnsolicitedSSInfo() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_Voice::ParseIntermediateSSInfo(CResponse* const pResponse, const char*& szPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseIntermediateSSInfo() - Enter\r\n");

    UINT32 nValue;
    const char* szPostfix;
    const char* szDummy;
    BOOL fRet = FALSE;
    RIL_SuppSvcNotification * prssn = NULL;


    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseIntermediateSSInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    prssn = (RIL_SuppSvcNotification*)malloc(sizeof(RIL_SuppSvcNotification));
    if (NULL == prssn)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseIntermediateSSInfo() : ERROR : cannot allocate prssn=[%d] bytes\r\n", sizeof(RIL_SuppSvcNotification));
        goto Error;
    }
    memset(prssn, 0, sizeof(RIL_SuppSvcNotification));

    if (!FindAndSkipRspEnd(szPointer, g_szNewLine, szPostfix))
    {
        // This isn't a complete Supplementary services notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_Voice::ParseIntermediateSSInfo() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // This is an MO Notification
    prssn->notificationType = 0;

    // Extract <code1>
    if (!ExtractUInt32(szPointer, nValue, szPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseIntermediateSSInfo() : ERROR : Can't extract <code1>\r\n");
        goto Error;
    }

    prssn->code = nValue;

    if (4 == nValue)
    {
        // Extract ,<index>
        if (SkipString(szPointer, ",", szPointer))
        {
            if (ExtractUInt32(szPointer, nValue, szPointer))
            {
                prssn->index = nValue;
            }
        }
    }

    // We have the parameters, look for the postfix
    if (!SkipRspEnd(szPointer, g_szNewLine, szDummy))
    {
        szPointer = szPostfix - strlen(g_szNewLine);
        pResponse->SetUnrecognizedFlag(TRUE);
        fRet = TRUE;
        free(prssn);
        prssn = NULL;
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_SUPP_SVC_NOTIFICATION);
    if (!pResponse->SetData((void*)prssn, sizeof(RIL_SuppSvcNotification), FALSE))
    {
        free(prssn);
        prssn = NULL;
        goto Error;
    }
    else
    {
        fRet = TRUE;
    }

Error:
    if (!fRet)
    {
        free(prssn);
        prssn = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_Voice::ParseIntermediateSSInfo() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseCallMeter(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallMeter() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallMeter() - Exit\r\n");

    return FALSE;
}

//
//
BOOL CSilo_Voice::ParseUSSDInfo(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseUSSDInfo() - Enter\r\n");
    P_ND_USSD_STATUS pUssdStatus = NULL;
    UINT32 uiStatus = 0;
    char* szDataString = NULL;
    UINT32 uiDataString = 0;
    UINT32 nDCS = 0;
    UINT32 uiAllocSize = 0;
    const char* szDummy;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete USSD notification -- no need to parse it
        goto Error;
    }

    // Extract "<status>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() : ERROR : Couldn't extract status\r\n");
        goto Error;
    }

    // Skip ","
    if (!SkipString(rszPointer, ",", rszPointer))
    {
        // No data parameter present
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            goto Error;
        }
        memset(pUssdStatus, 0, sizeof(S_ND_USSD_STATUS));
        snprintf(pUssdStatus->szType, 2, "%u", uiStatus);
        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = NULL;
        uiAllocSize = sizeof(char *);
    }
    else
    {
        // Extract "<data>,<dcs>"
        // NOTE: we take ownership of allocated szDataString
        if (!ExtractQuotedStringWithAllocatedMemory(rszPointer, szDataString, uiDataString, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() : ERROR : Couldn't extract ExtractQuotedStringWithAllocatedMemory\r\n");
            goto Error;
        }
        if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUInt32(rszPointer, nDCS, rszPointer))
        {
            // default nDCS.  this parameter is supposed to be mandatory but we've
            // seen it missing from otherwise normal USSD responses.
            nDCS = 0;
        }


        //  New for Medfield R2
        //  There's an extra <lf> in here, skip over it.
        // Look for a "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() : cannot find the end postfix!!\r\n");
            goto Error;
        }
        else
        {
            //  Back up over the "\r\n".
            rszPointer -= strlen(g_szNewLine);
        }


        //  Allocate blob.
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: malloc failed\r\n");
            goto Error;
        }
        memset(pUssdStatus, 0, sizeof(S_ND_USSD_STATUS));
        snprintf(pUssdStatus->szType, 2, "%u", uiStatus);

        // Please see 3GPP 23.038 (section 5) CBS Data Coding Scheme
        if ((nDCS >= 0x40) && (nDCS <= 0x5F))  // binary: 010xxxxx
        {
            unsigned char charset = ( (nDCS >> 2) & 0x03 ); // take bit 2 and bit3
            if (0x00 == charset)
            {
                // GSM 7-bit
                //  TODO: Implement this (if necessary)
                RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: GSM 7-bit not supported!\r\n");
                goto Error;
            }
            else if (0x01 == charset)
            {
                // 8-bit
                //  TODO: Implement this (if necessary)
                RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: 8-bit not supported!\r\n");
                goto Error;
            }
            else if (0x02 == charset)
            {
                // UCS2
                RIL_LOG_INFO("CSilo_Voice::ParseUSSDInfo() - UCS2 encoding\r\n");
                unsigned char* tmpUssdUcs2 = NULL;
                char tmpUssdAscii[MAX_BUFFER_SIZE] = {0};
                int lenUssdAscii = 0;

                if (!CopyStringNullTerminate((char*)tmpUssdAscii, szDataString, MAX_BUFFER_SIZE))
                {
                    RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: Cannot CopyStringNullTerminate szDataString to tmpUssdAscii\r\n");
                    goto Error;
                }

                lenUssdAscii = strlen(tmpUssdAscii);
                if ( (lenUssdAscii % 2) != 0)
                {
                    RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: Illegal string from modem\r\n");
                    goto Error;
                }

                tmpUssdUcs2 = new unsigned char[(lenUssdAscii/2)+2];
                if (NULL == tmpUssdUcs2)
                {
                    RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: Cannot allocate %d bytes for tmpUssdUcs2\r\n", lenUssdAscii/2+1);
                    goto Error;
                }
                memset(tmpUssdUcs2, 0, ((lenUssdAscii/2)+2));

                convertStrToHexBuf(tmpUssdAscii, &tmpUssdUcs2);

                memset(pUssdStatus->szMessage, 0, sizeof(pUssdStatus->szMessage));
                ucs2_to_utf8((unsigned char*)tmpUssdUcs2, lenUssdAscii/2, (unsigned char*)pUssdStatus->szMessage);

                delete []tmpUssdUcs2;
            }
            else
            {
                // reserved value
                RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: reserved value not supported!\r\n");
                goto Error;
            }
        }
        else if (nDCS <= 0x0F)  // binary: 0000xxxx
        {
            //  default encoding, just copy string from response directly.
            RIL_LOG_INFO("CSilo_Voice::ParseUSSDInfo() - default encoding\r\n");
            if (!CopyStringNullTerminate(pUssdStatus->szMessage, szDataString, MAX_BUFFER_SIZE))
            {
                RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: Cannot CopyStringNullTerminate szDataString\r\n");
                goto Error;
            }
        }
        else
        {
            //  dcs not supported
            //  TODO: Implement more dcs encodings
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUSSDInfo() - ERROR: dcs value of [%d, 0x%02X] not supported\r\n", nDCS, nDCS);
            goto Error;
        }

        RIL_LOG_INFO("CSilo_Voice::ParseUSSDInfo() - %s\r\n", pUssdStatus->szMessage);

        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;
        uiAllocSize = 2 * sizeof(char *);
    }


    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_ON_USSD);

    if (!pResponse->SetData((void*)pUssdStatus, uiAllocSize, FALSE))
    {
        goto Error;
    }
    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pUssdStatus);
        pUssdStatus = NULL;
    }

    delete[] szDataString;
    szDataString = NULL;

    RIL_LOG_VERBOSE("CSilo_Voice::ParseUSSDInfo() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseConnLineIdPresentation(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdPresentation - Enter\r\n");
    BOOL fRet = FALSE;
    const char* szDummy;
    UINT32 uiStatusPresentation = 0;
    UINT32 uiStatusService = 0;
    P_ND_USSD_STATUS pUssdStatus = NULL;
    char* szDataString = NULL;
    UINT32 uiTypeCode = 0; // USSD Notify

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdPresentation() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to make sure we got the whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete USSD notification -- no need to parse it
        RIL_LOG_INFO("CSilo_Voice::ParseConnLineIdPresentation() : ERROR : Couldn't find response end\r\n");
        goto Error;
    }

    // Extract "<n>"
    if (!ExtractUInt32(rszPointer, uiStatusPresentation, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdPresentation() : ERROR : Couldn't extract presentation status\r\n");
        goto Error;
    }

    // Extract "<m>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt32(rszPointer, uiStatusService, rszPointer)))
    {
        RIL_LOG_INFO("CSilo_Voice::ParseConnLineIdPresentation() - ERROR: Couldn't extract service status\r\n");
        goto Error;
    }

    if (!uiStatusService)
    {
        asprintf(&szDataString, "%s", "Connected Line ID Presentation Service is Disabled");
    }
    else if (uiStatusService == 1)
    {
        asprintf(&szDataString, "%s", "Connected Line ID Presentation Service is Enabled");
    }
    else if (uiStatusService == 2)
    {
        asprintf(&szDataString, "%s", "Connected Line ID Presentation Service Status is Unknown");
    }

    // allocate memory for message string
    pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
    if (!pUssdStatus)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdPresentation() - ERROR: malloc failed\r\n");
        goto Error;
    }
    memset(pUssdStatus, 0, sizeof(S_ND_USSD_STATUS));
    snprintf(pUssdStatus->szType, 2, "%u", uiTypeCode);
    if (!CopyStringNullTerminate(pUssdStatus->szMessage, szDataString , MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdPresentation() - ERROR: Cannot CopyStringNullTerminate szDataString\r\n");
        goto Error;
    }

    // update status pointers
    pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
    pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;

    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdPresentation() - %s\r\n", pUssdStatus->szMessage);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_ON_USSD);

    if (!pResponse->SetData((void*)pUssdStatus, 2 * sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pUssdStatus);
        pUssdStatus = NULL;
    }

    free(szDataString);

    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdPresentation() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseConnLineIdRestriction(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdRestriction() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* szDummy;
    UINT32 uiStatus = 0;
    P_ND_USSD_STATUS pUssdStatus = NULL;
    char* szDataString = NULL;
    UINT32 uiTypeCode = 0; // USSD Notify

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdRestriction() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to make sure we got the whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete USSD notification -- no need to parse it
        RIL_LOG_INFO("CSilo_Voice::ParseConnLineIdRestriction() : ERROR : Couldn't find response end\r\n");
        goto Error;
    }

    // Extract "<status>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdRestriction() : ERROR : Couldn't extract status\r\n");
        goto Error;
    }

    if (!uiStatus)
    {
        asprintf(&szDataString, "%s", "Connected Line ID Restriction Service is Disabled");
    }
    else if (uiStatus == 1)
    {
         asprintf(&szDataString, "%s", "Connected Line ID Restriction Service is Enabled");
    }
    else if (uiStatus == 2)
    {
        asprintf(&szDataString, "%s", "Connected Line ID Restriction Service Status is Unknown");
    }

    // allocate memory for message string
    pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
    if (!pUssdStatus)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdRestriction() - ERROR: malloc failed\r\n");
        goto Error;
    }
    memset(pUssdStatus, 0, sizeof(S_ND_USSD_STATUS));
    snprintf(pUssdStatus->szType, 2, "%u", uiTypeCode);
    if (!CopyStringNullTerminate(pUssdStatus->szMessage, szDataString, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseConnLineIdRestriction() - ERROR: Cannot CopyStringNullTerminate szDataString\r\n");
        goto Error;
    }

    // update status pointers
    pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
    pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;

    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdRestriction() - %s\r\n", pUssdStatus->szMessage);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_ON_USSD);

    if (!pResponse->SetData((void*)pUssdStatus, 2 * sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pUssdStatus);
        pUssdStatus = NULL;
    }

    free(szDataString);

    RIL_LOG_VERBOSE("CSilo_Voice::ParseConnLineIdRestriction() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseCallProgressInformation(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallProgressInformation() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallProgressInformation() - Exit\r\n");

    return FALSE;
}

//
//
BOOL CSilo_Voice::ParseIndicatorEvent(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseIndicatorEvent() - Enter\r\n");

    BOOL fRet = FALSE;

    RIL_LOG_VERBOSE("CSilo_Voice::ParseIndicatorEvent() - Exit\r\n");

    return fRet;
}

//
//
BOOL CSilo_Voice::ParseDISCONNECT(CResponse *const pResponse, const char* &rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseDISCONNECT() - Enter\r\n");

    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseDISCONNECT() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseDISCONNECT() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseDISCONNECT() - Exit\r\n");
    return fRet;

}

BOOL CSilo_Voice::ParseBusy(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseBusy() - Enter\r\n");

    BOOL fRet = FALSE;

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseBusy() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseBusy() - Exit\r\n");
    return fRet;
}

BOOL CSilo_Voice::ParseNoAnswer(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoAnswer() - Enter\r\n");

    BOOL fRet = FALSE;

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseNoAnswer() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoAnswer() - Exit\r\n");
    return fRet;
}


BOOL CSilo_Voice::ParseCTMCall(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCTMCall() - Enter\r\n");

    BOOL fRet = FALSE;

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCTMCall() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCTMCall() - Exit\r\n");
    return fRet;
}

BOOL CSilo_Voice::ParseNoCTMCall(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoCTMCall() - Enter\r\n");

    BOOL fRet = FALSE;

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseNoCTMCall() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoCTMCall() - Exit\r\n");
    return fRet;
}


#if defined (M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)

BOOL CSilo_Voice::ParseCallFailedCause(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallFailedCause() - Enter\r\n");

    BOOL fRet = FALSE;
    int *pFailedCauseData = NULL;
    const char * szDummy = NULL;
    UINT32 uiDummy = 0;
    UINT32 uiCause = 0;
    UINT32 uiID = 0;

    // Do we have a complete notification?
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallFailedCause() : ERROR : Incomplete notification\r\n");
        goto Error;
    }

    pFailedCauseData = (int*)malloc(2 * sizeof(int*));
    if (!pFailedCauseData)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallFailedCause() : ERROR : Could not allocate data\r\n");
        goto Error;
    }
    memset(pFailedCauseData, 0, 2 * sizeof(int*));

    //  Extract <report>
    if (!ExtractUInt32(rszPointer, uiDummy, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallFailedCause() : ERROR : Could not extract <report>\r\n");
        goto Error;
    }

    //  Extract <cause>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiCause, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallFailedCause() : ERROR : Could not extract uiCause\r\n");
        goto Error;
    }

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallFailedCause() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= strlen(g_szNewLine);

    //  Now we have ID and cause
    uiID = CSystemManager::GetInstance().GetLastCallFailedCauseID();
    RIL_LOG_INFO("CSilo_Voice::ParseCallFailedCause() : ***** RECEIVED CALL FAILED CAUSE NOTIFICATION *****\r\n");
    RIL_LOG_INFO("CSilo_Voice::ParseCallFailedCause() : ID=[%u]  cause=[%u]\r\n", uiID, uiCause);
    RIL_LOG_INFO("CSilo_Voice::ParseCallFailedCause() : ***** SENDING NOTIFICATION=[%d] ******\r\n", RIL_UNSOL_CALL_FAILED_CAUSE);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_CALL_FAILED_CAUSE);

    //  TODO: Set call id = pData[0]
    //        Set failed cause = pData[1]

    pFailedCauseData[0] = uiID; // call id
    pFailedCauseData[1] = uiCause; // failed cause

    if (!pResponse->SetData((void*)pFailedCauseData, 2 * sizeof(int *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pFailedCauseData);
        pFailedCauseData = NULL;
    }
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallFailedCause() - Exit\r\n");
    return fRet;
}

#endif // M2_CALL_FAILED_CAUSE_FEATURE_ENABLED
