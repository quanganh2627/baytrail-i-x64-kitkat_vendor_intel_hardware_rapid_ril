////////////////////////////////////////////////////////////////////////////
// silo_SMS.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SMS-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "rildmain.h"
#include "callbacks.h"
#include "silo_sms.h"

//
//
CSilo_SMS::CSilo_SMS(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_SMS::CSilo_SMS() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+CMT: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCMT  },
        { "+CBM: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCBM  },
        { "+CDS: "   , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCDS  },
        { "+CMTI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCMTI },
        { "+CBMI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCBMI },
        { "+CDSI: "  , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseCDSI },
        { ""         , (PFN_ATRSP_PARSE)&CSilo_SMS::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_SMS::CSilo_SMS() - Exit\r\n");
}

//
//
CSilo_SMS::~CSilo_SMS()
{
    RIL_LOG_VERBOSE("CSilo_SMS::~CSilo_SMS() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::~CSilo_SMS() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////


BOOL CSilo_SMS::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SMS::PreParseResponseHook() - Enter\r\n");
    BOOL bRetVal = TRUE;

    if ( (ND_REQ_ID_SENDSMS == rpCmd->GetRequestID() ||
          ND_REQ_ID_SENDSMSEXPECTMORE == rpCmd->GetRequestID()) &&
         (RIL_E_SUCCESS != rpRsp->GetResultCode()))
    {
        if (RRIL_CMS_ERROR_OPERATION_NOT_ALLOWED == rpRsp->GetErrorCode() ||
            RRIL_CMS_ERROR_SIM_BUSY == rpRsp->GetErrorCode() ||
            RRIL_CMS_ERROR_NETWORK_TIMEOUT == rpRsp->GetErrorCode())
        {
            RIL_LOG_INFO("CSilo_SMS::PreParseResponseHook() - Send SMS failed, retry case\r\n");
            rpRsp->SetResultCode(RIL_E_SMS_SEND_FAIL_RETRY);
        }
        else if (RRIL_CMS_ERROR_FDN_CHECK_FAILED == rpRsp->GetErrorCode() ||
                 RRIL_CMS_ERROR_SCA_FDN_FAILED == rpRsp->GetErrorCode() ||
                 RRIL_CMS_ERROR_DA_FDN_FAILED == rpRsp->GetErrorCode())
        {
            //  tried sending SMS, not in FDN list
            RIL_LOG_INFO("CSilo_SMS::PreParseResponseHook() - Send SMS failed, not in FDN list\r\n");
            rpRsp->SetResultCode(RIL_E_FDN_CHECK_FAILURE);
        }
    }


Error:
    RIL_LOG_VERBOSE("CSilo_SMS::PreParseResponseHook() - Exit\r\n");
    return bRetVal;
}



//
//
BOOL CSilo_SMS::ParseMessage(CResponse* const pResponse, const char*& rszPointer, SILO_SMS_MSG_TYPES msgType)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessage() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessage() - Exit\r\n");
    return FALSE;
}

//
//
//
BOOL CSilo_SMS::ParseMessageInSim(CResponse* const pResponse, const char*& rszPointer, SILO_SMS_MSG_TYPES msgType)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessageInSim() - Enter\r\n");

    BOOL   fRet = FALSE;
    const char* szDummy;
    UINT32 Location;
    UINT32 Index;
    int * pIndex = NULL;

    if (SILO_SMS_MSG_IN_SIM == msgType)
    {
        triggerQuerySimSmsStoreStatus(NULL);
    }

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseMessageInSim() : pResponse was NULL\r\n");
        goto Error;
    }

    pIndex = (int*)malloc(sizeof(int));
    if (NULL == pIndex)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseMessageInSim() - Could not alloc mem for int.\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, szDummy))
    {
        goto Error;
    }

    // Look for a "<anything>,<Index>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt32(rszPointer, Index, rszPointer)))
    {
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM);

    *pIndex = Index;

    if (!pResponse->SetData((void*)pIndex, sizeof(int*), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pIndex);
        pIndex = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseMessageInSim() - Exit\r\n");

    return fRet;
}

//
// SMS-DELIVER notification
//
BOOL CSilo_SMS::ParseCMT(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMT() - Enter\r\n");

    BOOL   fRet     = FALSE;
    UINT32 uiLength = 0;
    char*  szPDU    = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    const char* szDummy;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - pResponse is NULL.\r\n");
        goto Error;
    }

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse ",<length><CR><LF>
    if (!SkipString(rszPointer, ",", rszPointer)       ||
        !ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    //RIL_LOG_INFO("CMT=[%s]\r\n", CRLFExpandedString(rszPointer,strlen(rszPointer)).GetString() );

    // The PDU will be followed by g_szNewline, so look for g_szNewline
    // and use its position to determine the length of the PDU string.

    if (!FindAndSkipString(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not find postfix; assuming this is an incomplete response.\r\n");
        goto Error;
    }
    else
    {
        // Override the given length with the actual length. Don't forget the '\0'.
        uiLength = ((UINT32)(szDummy - rszPointer)) - strlen(g_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SMS::ParseCMT() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    szPDU = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not allocate memory for szPDU.\r\n");
        goto Error;
    }
    memset(szPDU, 0, sizeof(char) * uiLength);

    if (!ExtractUnquotedString(rszPointer, g_cTerminator, szPDU, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCMT() - Could not parse PDU String.\r\n");
        goto Error;
    }

    // Ensure NULL Termination.
    szPDU[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SMS::ParseCMT() - PDU String: \"%s\".\r\n", szPDU);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS);

    if (!pResponse->SetData((void*)szPDU, sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(szPDU);
        szPDU = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMT() - Exit\r\n");
    return fRet;
}

//
//  Incoming cell broadcast.
//
BOOL CSilo_SMS::ParseCBM(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBM() - Enter\r\n");

    BOOL   fRet     = FALSE;
    UINT32   uiLength = 0;
    char*  szPDU    = NULL;
    unsigned char*  PDUHexa    = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    const char* szDummy;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - pResponse is NULL.\r\n");
        goto Error;
    }

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse "<length><CR><LF>
    if (!ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    // The PDU will be followed by g_szNewline, so look for g_szNewline
    // and use its position to determine the length of the PDU string.

    if (!FindAndSkipString(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not find postfix; assuming this is an incomplete response.\r\n");
        goto Error;
    }
    else
    {
        uiLength = (UINT32)(szDummy - rszPointer) - strlen(g_szNewLine);
        RIL_LOG_INFO("CSilo_SMS::ParseCBM() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    // Don't forget the '\0'.
    szPDU = (char*)malloc(sizeof(char) * (uiLength+1));
    PDUHexa = (unsigned char*)malloc(sizeof(unsigned char) * (uiLength/2));

    if ((NULL == szPDU) || (NULL == PDUHexa))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not allocate memory for szPDU.\r\n");
        goto Error;
    }

    memset(szPDU, 0, sizeof(char) * (uiLength+1));
    memset(PDUHexa, 0, sizeof(unsigned char) * (uiLength/2));

    if (!ExtractUnquotedString(rszPointer, g_cTerminator, szPDU, (uiLength+1), rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCBM() - Could not parse PDU String.\r\n");
        goto Error;
    }

    // Translate the Hexa String to Hexa value (2 digits -> 1 digits).
    // Ex : "C04E2D..." to {0xC0, 0x4E, 0x2D}.
    // The (uiLength-1)/2 first byte (char) of the szPDU buffer are updated.
    // The last character of the string is not take account.
    convertStrToHexBuf(szPDU, &PDUHexa);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS);

    // The (uiLength-1)/2 first byte (char) of the szPDU buffer are returned in the response.
    if (!pResponse->SetData(PDUHexa, uiLength/2, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    free(szPDU);
    szPDU = NULL;

    if (!fRet)
    {
        free(PDUHexa);
        PDUHexa = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBM() - Exit\r\n");
    return fRet;
}

//
//
//
BOOL CSilo_SMS::ParseCDS(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDS() - Enter\r\n");

    UINT32  uiLength = 0;
    const char* szDummy;
    char* szPDU = NULL;
    char   szAlpha[MAX_BUFFER_SIZE];
    BOOL  fRet = FALSE;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - pResponse is NULL.\r\n");
        goto Error;
    }

    // Throw out the alpha chars if there are any
    (void)ExtractQuotedString(rszPointer, szAlpha, MAX_BUFFER_SIZE, rszPointer);

    // Parse ",<length><CR><LF>
    if (!ExtractUInt32(rszPointer, uiLength, rszPointer) ||
        !SkipString(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not parse PDU Length.\r\n");
        goto Error;
    }

    //RIL_LOG_INFO("CDS=[%s]\r\n", CRLFExpandedString(rszPointer,strlen(rszPointer)).GetString() );

    // The PDU will be followed by g_szNewline, so look for g_szNewline
    // and use its position to determine the length of the PDU string
    if (!FindAndSkipString(rszPointer, g_szNewLine, szDummy))
    {
        // This isn't a complete message notification -- no need to parse it
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not find postfix; assuming this is an incomplete response.\r\n");
        goto Error;
    }
    else
    {
        // Override the given length with the actual length. Don't forget the '\0'.
        uiLength = ((UINT32)(szDummy - rszPointer)) - strlen(g_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SMS::ParseCDS() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    szPDU = (char*) malloc(sizeof(char) * uiLength);
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not allocate memory for szPDU.\r\n");
        goto Error;
    }
    memset(szPDU, 0, sizeof(char) * uiLength);

    if (!ExtractUnquotedString(rszPointer, g_cTerminator, szPDU, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SMS::ParseCDS() - Could not parse PDU String.\r\n");
        goto Error;
    }

    // Ensure NULL Termination.
    szPDU[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SMS::ParseCDS() - PDU String: \"%s\".\r\n", szPDU);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT);

    if (!pResponse->SetData((void*) szPDU, sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(szPDU);
        szPDU = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDS() - Exit\r\n");
    return fRet;

}

//
//
//
BOOL CSilo_SMS::ParseCMTI(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMTI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCMTI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_MSG_IN_SIM));
}

//
//
//
BOOL CSilo_SMS::ParseCBMI(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBMI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCBMI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_CELLBC_MSG_IN_SIM));
}

//
//
//
BOOL CSilo_SMS::ParseCDSI(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDSI() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SMS::ParseCDSI() - Exit\r\n");
    return (ParseMessageInSim(pResponse, rszPointer, SILO_SMS_STATUS_MSG_IN_SIM));
}

