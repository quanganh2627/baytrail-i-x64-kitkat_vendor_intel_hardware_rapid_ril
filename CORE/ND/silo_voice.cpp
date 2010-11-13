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

#include <stdio.h>

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "rildmain.h"
#include "silo_voice.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//
//
CSilo_Voice::CSilo_Voice(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Voice::CSilo_Voice() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+CRING: "      , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseExtRing },
        { "DISCONNECT"  , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseDISCONNECT },
        { "+CCWA: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCallWaitingInfo },
        { "+CSSU: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUnsolicitedSSInfo },
        { "+CSSI: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseIntermediateSSInfo },
        { "+CCCM: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseCallMeter },
        { "+CUSD: "       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseUSSDInfo },
        { "+XCIEV: "      , (PFN_ATRSP_PARSE)/*&CSilo_Voice::ParseIndicatorEvent*/ &CSilo_Voice::ParseUnrecognized },
        { "+XCALLINFO: "  , (PFN_ATRSP_PARSE)/*&CSilo_Voice::ParseCallProgressInformation*/ &CSilo_Voice::ParseUnrecognized },
        { "NO CARRIER"    , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseNoCarrier },
        { "CONNECT"       , (PFN_ATRSP_PARSE)&CSilo_Voice::ParseConnect },
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
//
//
BOOL CSilo_Voice::ParseExtRing(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseExtRing() - Enter\r\n");

    BOOL fRet = FALSE;

    // Skip to the next <postfix>
    if(!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= 2;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseExtRing() - Exit\r\n");
    return fRet;
}

#undef Snprintf

//
//
BOOL CSilo_Voice::ParseNoCarrier(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoCarrier() - Enter\r\n");

    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseNoCarrier() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseNoCarrier() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= 2;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseNoCarrier() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseConnect(CResponse* const pResponse, const BYTE*& rszPointer)
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
    rszPointer -= 2;
 
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
BOOL CSilo_Voice::ParseCallWaitingInfo(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallWaitingInfo() - Enter\r\n");

    BYTE szAddress[MAX_BUFFER_SIZE];
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseCallWaitingInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Parse "<address>
    if (!ExtractQuotedString(rszPointer, szAddress, MAX_BUFFER_SIZE, rszPointer))
    {
        // This is not a unsolicited response CCWA. Return error so the response will be handled properly.
        goto Error;
    }

    // Look for a "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= 2;
    
    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallWaitingInfo() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_Voice::ParseUnsolicitedSSInfo(CResponse* const pResponse, const BYTE*& szPointer)
{
// Parsing for the +CSSU notification. The format is:
//    "<code2>[,<index>[,<address>,<type>[,<subaddress>,<satype>]]]"

    RIL_LOG_VERBOSE("CSilo_Voice::ParseUnsolicitedSSInfo() - Enter\r\n");

    UINT32 nValue;
    const BYTE* szPostfix;
    const BYTE* szDummy;
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
    pSuppSvcBlob = (RIL_SuppSvcNotification*)malloc(sizeof(RIL_SuppSvcNotification) + (sizeof(BYTE) * MAX_BUFFER_SIZE));
    if (NULL == pSuppSvcBlob)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo: Failed to alloc pSuppSvcBlob!\r\n");
        goto Error;
    }

    memset(pSuppSvcBlob, 0, sizeof(RIL_SuppSvcNotification) + (sizeof(BYTE) * MAX_BUFFER_SIZE));

    // This is an MT Notification
    pSuppSvcBlob->notificationType = 1;

    // Extract <code2>
    if (ExtractUInt(szPointer, nValue, szPointer) && (5 != nValue))
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Found code: %d\r\n", nValue);

        pSuppSvcBlob->code = nValue;

        // Extract [,<index>
        if (!SkipString(szPointer, ",", szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Goto continue\r\n");
            goto Continue;
        }
        if (ExtractUInt(szPointer, nValue, szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Found index: %d\r\n", nValue);
            pSuppSvcBlob->index = nValue;
        }

        // Extract [,<number>
        if (!SkipString(szPointer, ",", szPointer))
        {
            goto Continue;
        }

        // Setup the BYTE* number to use the extra memory at the end of the struct
        pSuppSvcBlob->number = (BYTE*)((UINT32)pSuppSvcBlob + sizeof(RIL_SuppSvcNotification));

        if(!ExtractQuotedString(szPointer, pSuppSvcBlob->number, MAX_BUFFER_SIZE, szPointer))
        {
            RIL_LOG_CRITICAL("CSilo_Voice::ParseUnsolicitedSSInfo : Error during extraction of number\r\n");
            goto Error;
        }

        // Skip "," and look for <type>
        if (SkipString(szPointer, ",", szPointer))
        {
            if(!ExtractUInt(szPointer, nValue, szPointer))
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
            fRet = TRUE;
            goto Error;
        }

        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_SUPP_SVC_NOTIFICATION);

        if (!pResponse->SetData((void*)pSuppSvcBlob, sizeof(RIL_SuppSvcNotification), FALSE))
        {
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
BOOL CSilo_Voice::ParseIntermediateSSInfo(CResponse* const pResponse, const BYTE*& szPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseIntermediateSSInfo() - Enter\r\n");

    UINT32 nValue;
    const BYTE* szPostfix;
    const BYTE* szDummy;
    BOOL fRet = FALSE;
    RIL_SuppSvcNotification * prssn = (RIL_SuppSvcNotification*)malloc(sizeof(RIL_SuppSvcNotification));

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_Voice::ParseIntermediateSSInfo() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    if (NULL == prssn)
    {
        goto Error;
    }
    memset(prssn, 0, sizeof(RIL_SuppSvcNotification));

    if (!FindAndSkipRspEnd(szPointer, g_szNewLine, szPostfix))
    {
        // This isn't a complete Supplementary services notification -- no need to parse it
        goto Error;
    }

    // This is an MO Notification
    prssn->notificationType = 0;

    // Extract <code1>
    if (!ExtractUInt(szPointer, nValue, szPointer))
    {
        goto Error;
    }

    prssn->code = nValue;

    if (4 == nValue)
    {
        // Extract ,<index>
        if (SkipString(szPointer, ",", szPointer))
        {
            if (ExtractUInt(szPointer, nValue, szPointer))
            {
                prssn->index = nValue;
            }
        }
    }

    // We have the parameters, look for the postfix
    if (!SkipRspEnd(szPointer, g_szNewLine, szDummy))
    {
        szPointer = szPostfix - strlen("\r");
        pResponse->SetUnrecognizedFlag(TRUE);
        fRet = TRUE;
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_SUPP_SVC_NOTIFICATION);
    if (!pResponse->SetData((void*)prssn, sizeof(RIL_SuppSvcNotification), FALSE))
    {
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
BOOL CSilo_Voice::ParseCallMeter(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallMeter() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallMeter() - Exit\r\n");

    return FALSE;
}

//
//
BOOL CSilo_Voice::ParseUSSDInfo(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseUSSDInfo() - Enter\r\n");
    UINT32 nDCS;
    BYTE* szDataString = NULL;
    UINT32 uiDataString;
    UINT32 uiStatus;
    const BYTE* szDummy;
    WCHAR* szUcs2 = NULL;
    P_ND_USSD_STATUS pUssdStatus = NULL;
    BOOL bSupported = FALSE;
    UINT32 uiAllocSize;
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
    if (!ExtractUInt(rszPointer, uiStatus, rszPointer))
    {
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
        snprintf(pUssdStatus->szType, 2, "%d", (int) uiStatus);
        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;
        uiAllocSize = sizeof(BYTE *);
    }
    else
    {
        // Extract "<data>,<dcs>"
        // NOTE: we take ownership of allocated szDataString
        if (!ExtractQuotedStringWithAllocatedMemory(rszPointer, szDataString, uiDataString, rszPointer))
        {
            goto Error;
        }
        if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUInt(rszPointer, nDCS, rszPointer))
        {
            // default nDCS.  this parameter is supposed to be mandatory but we've
            // seen it missing from otherwise normal USSD responses.
            nDCS = 0;
        }

        // See GSM 03.38 for Cell Broadcast DCS details
        // Currently support only default alphabet in all languages (i.e., the DCS range 0x00..0x0f)
        // check if we support this DCS
        if (nDCS <= 0x0f)
        {
            bSupported = TRUE;
        }
        else if ((0x040 <= nDCS) && (nDCS <= 0x05f))
        {
            /* handle coding groups of 010* (binary)  */
            /* only character set is of interest here */
        
            /* refer to 3GPP TS 23.038 5 and 3GPP TS 27.007 7.15 for detail */
            UINT32 uDCSCharset = ( ( nDCS >> 2 ) & 0x03 );
            if ( 0x00 == uDCSCharset )
            {
                // GSM 7 bit default alphabet
                if ( ENCODING_TECHARSET == ENCODING_GSMDEFAULT_HEX )
                {
                    // not supported
                    bSupported = FALSE;
                }
            }
            else if ( 0x01 == uDCSCharset )
            {
                // 8 bit data
                // not supported
                bSupported = FALSE;
            }
            else if ( 0x02 == uDCSCharset )
            {
                // UCS2 (16 bit)
                // supported
                bSupported = TRUE;
            }
            else if ( 0x03 == uDCSCharset )
            {
                // reserved
                RIL_LOG_CRITICAL("Unsupported value 0x03\r\n");
                // not supported
                 bSupported = FALSE;
            }
            else
            {
                /* should not reach here. just in case */
                RIL_LOG_CRITICAL("Unsupported value [%d]\r\n", uDCSCharset);

                // not supported
                 bSupported = FALSE;
            }
        }

        if (!bSupported)
        {
            goto Error;
        }
        /*--- currently we only handle the default alphabet encoding ---*/

        // Default alphabet encoding (which has been requested in our init string to allow direct copying of bytes)
        // At this point, uiDataString has the size of the szDataString which ExtractQuotedStringWithAllocatedMemory allocated for us and
        // which holds the TE-encoded characters.
        // However, this byte count also includes an extra byte for the NULL character at the end, which we're not going to translate.
        // Therefore, decrement uiDataString by 1
        uiDataString--;

        // Now figure out the number of Unicode characters needed to hold the string after it's converted
        size_t cbUnicode;
        if (!ConvertSizeToUnicodeSize(ENCODING_TECHARSET, uiDataString, cbUnicode))
        {
            goto Error;
        }

        // cbUnicode now has the number of bytes we need to hold the Unicode string generated when we
        // convert from the TE-encoded string.
        // However, it doesn't include space for the terminating NULL, so we need to allow for that also.
        // (Note that ParseEncodedString will append the terminating NULL for us automatically.)
        cbUnicode += sizeof(WCHAR);

        // Allocate buffer for conversion
        szUcs2 = new WCHAR[cbUnicode];
        if (!szUcs2)
        {
            goto Error;
        }

        // Now convert the TE-encoded string into Unicode in the buffer we just allocated
        if (!ExtractUnquotedUnicodeHexStringToUnicode(szDataString,
                                                      uiDataString,
                                                      szUcs2,
                                                      cbUnicode))
        {
            goto Error;
        }

        // allocate blob and convert from UCS2 to UTF8
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            goto Error;
        }
        snprintf(pUssdStatus->szType, 2, "%d", (int) uiStatus);
        if (!ConvertUnicodeToUtf8(szUcs2, pUssdStatus->szMessage, MAX_BUFFER_SIZE))
        {
            goto Error;
        }
        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;
        uiAllocSize = 2 * sizeof(BYTE *);
        }
    

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_ON_USSD);

    if (!pResponse->SetData((void*)pUssdStatus, sizeof(BYTE **), FALSE))
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
    delete[] szUcs2;
    szUcs2 = NULL;

    RIL_LOG_VERBOSE("CSilo_Voice::ParseUSSDInfo() - Exit\r\n");
    return fRet;
}

//
//
BOOL CSilo_Voice::ParseCallProgressInformation(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallProgressInformation() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Voice::ParseCallProgressInformation() - Exit\r\n");

    return FALSE;
}

//
//
BOOL CSilo_Voice::ParseIndicatorEvent(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_Voice::ParseIndicatorEvent() - Enter\r\n");

    BOOL fRet = FALSE;
 
    RIL_LOG_VERBOSE("CSilo_Voice::ParseIndicatorEvent() - Exit\r\n");

    return fRet;
}

//
//
BOOL CSilo_Voice::ParseDISCONNECT(CResponse *const pResponse, const BYTE* &rszPointer)
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
        goto Error;
    }

    // Walk back over the <CR>
    rszPointer -= 2;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_Voice::ParseDISCONNECT() - Exit\r\n");
    return fRet;
    
}

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

    return TRUE;
}

