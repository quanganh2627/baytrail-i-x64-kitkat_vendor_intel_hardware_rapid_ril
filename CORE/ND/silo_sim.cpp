////////////////////////////////////////////////////////////////////////////
// silo_SIM.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SIM-related
//    RIL components.
//
// Author:  Dennis Peter
// Created: 2007-08-01
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
#include "rril.h"
#include "ril_result.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "callbacks.h"
#include "rildmain.h"
#include "silo_sim.h"
#include "atchannel.h"
#include "stk.h"
#include "callbacks.h"

extern "C" char *stk_at_to_hex(ATResponse *p_response);


//
//
CSilo_SIM::CSilo_SIM(CChannel *pChannel)
: CSilo(pChannel),
m_nXSIMStatePrev(-1)
{
    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+STKCTRLIND: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+STKCC: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
#ifndef USE_STK_RAW_MODE
        { "+STKPRO: "  , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseSTKProCmd },
        { "+STKCNF: "  , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseProSessionStatus },
#else
        { "+SATI: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATI },
        { "+SATN: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATN },
        { "+SATF: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseTermRespConfirm },
#endif
        { "+XLOCK: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+XSIM: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXSIM },
        { ""           , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseNULL }
    };

    m_pATRspTable = pATRspTable;

    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Exit\r\n");
}

//
//
CSilo_SIM::~CSilo_SIM()
{
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

BOOL CSilo_SIM::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::PreParseResponseHook() - Enter\r\n");
    BOOL bRetVal = TRUE;

    switch(rpCmd->GetRequestID())
    {
        case ND_REQ_ID_ENTERSIMPIN:
        case ND_REQ_ID_ENTERSIMPUK:
        case ND_REQ_ID_ENTERSIMPIN2:
        case ND_REQ_ID_ENTERSIMPUK2:
        case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION:
        case ND_REQ_ID_CHANGESIMPIN:
        case ND_REQ_ID_CHANGESIMPIN2:
        case ND_REQ_ID_SETFACILITYLOCK:
            bRetVal = ParsePin(rpCmd, rpRsp);
            break;

        case ND_REQ_ID_SIMIO:
            bRetVal = ParseSimIO(rpCmd, rpRsp);
            break;

        //case ND_REQ_ID_GETSIMSTATUS:
        //    bRetVal = ParseSimStatus(rpCmd, rpRsp);
        //    break;

        default:
            break;
    }

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::PreParseResponseHook() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::PostParseResponseHook() - Enter\r\n");
    BOOL bRetVal = TRUE;

    switch(rpCmd->GetRequestID())
    {
        case ND_REQ_ID_GETSIMSTATUS:
            bRetVal = ParseSimStatus(rpCmd, rpRsp);
            break;

        default:
            break;
    }

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::PostParseResponseHook() - Exit\r\n");
    return bRetVal;
}

// Helper functions
BOOL CSilo_SIM::ParsePin(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_VERBOSE("CSilo_SIM::ParsePin() - Enter\r\n");

    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {

        switch(rpRsp->GetErrorCode())
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - Incorrect password");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);
                break;

            case CME_ERROR_SIM_PUK_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - SIM PUK required");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);

                //  Set radio state to sim locked or absent.
                //  Note that when the radio state *changes*, the upper layers will query
                //  for more information.  This is why we cannot change the radio state for
                //  incorrect PIN (not PUKd yet) because state is
                //  "sim locked or absent" -> "sim locked or absent".
                //  But PIN -> PUK we have to do this otherwise phone will think we are still PIN'd.
                RIL_requestTimedCallback(notifySIMLocked, NULL, 0, 500000);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - SIM PUK2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PUK2);

                //  Set radio state to sim locked or absent.
                //  Note that when the radio state *changes*, the upper layers will query
                //  for more information.  This is why we cannot change the radio state for
                //  incorrect PIN2 (not PUK2d yet) because state is
                //  "sim locked or absent" -> "sim locked or absent".
                //  But PIN2 -> PUK2 we have to do this otherwise phone will think we are still PIN2'd.
                RIL_requestTimedCallback(notifySIMLocked, NULL, 0, 500000);
                break;

            default:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - Unknown error [%d]", rpRsp->GetErrorCode());
                rpRsp->SetResultCode(RIL_E_GENERIC_FAILURE);
                break;
        }

        rpRsp->FreeData();
        int* pInt = (int *) malloc(sizeof(int));

        if (NULL == pInt)
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePin() : Unable to allocate memory for SIM unlock retries\r\n");
            goto Error;
        }

        //  (Dec 22/09) I tried entering different values for this, but I don't think it does anything in the UI.
        *pInt = -1;

        if (!rpRsp->SetData((void*) pInt, sizeof(int*), FALSE))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePin() : Unable to set data with number of SIM unlock retries left\r\n");
            delete pInt;
            pInt = NULL;
            goto Error;
        }

    }


    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParsePin() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::ParseSimIO(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimIO() - Enter\r\n");

    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        switch(rpRsp->GetErrorCode())
        {
            case CME_ERROR_SIM_PIN2_REQUIRED:
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - SIM PIN2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PIN2);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - SIM PUK2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PUK2);
                break;

            default:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - Unknown error [%d]", rpRsp->GetErrorCode());
                rpRsp->SetResultCode(RIL_E_GENERIC_FAILURE);
                break;
        }
    }

    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimIO() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::ParseSimStatus(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimStatus() - Enter\r\n");

    BOOL bRetVal = FALSE;
    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : Found CME Error on AT+CPIN? request.\r\n");

        switch (rpRsp->GetErrorCode())
        {
            case RRIL_CME_ERROR_SIM_ABSENT:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : SIM Card is absent!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);

                RIL_CardStatus* pCardStatus = (RIL_CardStatus *) malloc(sizeof(RIL_CardStatus));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus));

                // Initialize as per reference ril as insufficient documentation currently is available
                pCardStatus->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;
                pCardStatus->card_state = RIL_CARDSTATE_ABSENT;
                pCardStatus->num_applications = 0;

                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus*), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    delete pCardStatus;
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            break;

            case RRIL_CME_ERROR_NOT_READY:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : SIM Card is not ready!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);

                RIL_CardStatus* pCardStatus = (RIL_CardStatus*) malloc(sizeof(RIL_CardStatus));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus));

                // Initialize as per reference ril as insufficient documentation currently is available
                pCardStatus->gsm_umts_subscription_app_index = 0;
                pCardStatus->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;
                pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
                pCardStatus->num_applications = 1;

                pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
                pCardStatus->applications[0].app_state = RIL_APPSTATE_DETECTED;
                pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
                pCardStatus->applications[0].aid_ptr = NULL;
                pCardStatus->applications[0].app_label_ptr = NULL;
                pCardStatus->applications[0].pin1_replaced = 0;
                pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
                pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;

                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus*), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    delete pCardStatus;
                    pCardStatus = NULL;
                    goto Error;
                }
            }

            default:
                break;
        }
    }

    bRetVal = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimStatus() - Exit\r\n");

    return bRetVal;
}

#ifndef USE_STK_RAW_MODE
BOOL CSilo_SIM::ParseSTKProCmd(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseSTKProCmd() - Enter\r\n");
    BOOL fRet = FALSE;
    char* line = NULL;
    ATResponse* pAtResp = new ATResponse;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseSTKProCmd() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    if (NULL == pAtResp)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseSTKProCmd() : Unable to allocate memory for ATResponse\r\n");
        goto Error;
    }

    memset(pAtResp, 0, sizeof(ATResponse));
    pAtResp->p_intermediates = new ATLine;
    asprintf(&pAtResp->p_intermediates->line, "%s", rszPointer);

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseSTKProCmd() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Create STK proactive hex string
    line = stk_at_to_hex(pAtResp);

    RIL_LOG_INFO(" line= %s\r\n", line);

    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);

    delete pAtResp->p_intermediates;
    delete pAtResp;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_STK_PROACTIVE_COMMAND);

    if (!pResponse->SetData((void*) line, sizeof(char *), FALSE))
    {
        RIL_LOG_INFO(" SetData failed\r\n");
        goto Error;
    }

    fRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_SIM::ParseSTKProCmd() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseProSessionStatus(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiCmd;
    UINT32 uiResult;
    UINT32 uiAddResult;
    UINT32 uiStatus;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<proactive_cmd>"
    if (!ExtractUInt(rszPointer, uiCmd, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - ERROR: Could not parse proactive cmd.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Proactive Cmd: %u.\r\n", uiCmd);

    // Extract "<result>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt(rszPointer, uiResult, rszPointer)))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - ERROR: Could not parse result.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Result: %u.\r\n", uiResult);

    // Extract "<add_result>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt(rszPointer, uiAddResult, rszPointer)))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - ERROR: Could not parse additional result.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Additional result: %u.\r\n", uiAddResult);

    // Extract "<sw1>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt(rszPointer, uiStatus, rszPointer)))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - ERROR: Could not parse status.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status: %u.\r\n", uiStatus);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_STK_SESSION_END);

    fRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_SIM::ParseProSessionStatus() - Exit\r\n");
    return fRet;
}

#else //USE_STK_RAW_MODE
BOOL CSilo_SIM::ParseIndicationSATI(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() : ERROR : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by g_szNewline, so look for g_szNewline and use its
        // position to determine length of PDU string.

        // Calculate PDU length + NULL byte
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(g_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - ERROR: Could not alloc mem for command.\r\n");
        goto Error;
    }

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - ERROR: Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_STK_PROACTIVE_COMMAND);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseIndicationSATN(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got the whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() : ERROR : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by g_szNewline, so look for g_szNewline and use its
        // position to determine length of PDU string.

        // Calculate PDU length + NULL byte
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(g_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - ERROR: Could not alloc mem for command.\r\n");
        goto Error;
    }

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - ERROR: Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_STK_EVENT_NOTIFY);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char *), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseTermRespConfirm(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiStatus1;
    UINT32 uiStatus2;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<sw1>"
    if (!ExtractUInt(rszPointer, uiStatus1, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - ERROR: Could not parse sw1.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status 1: %u.\r\n", uiStatus1);

    // Extract "<sw2>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt(rszPointer, uiStatus2, rszPointer)))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - ERROR: Could not parse sw2.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status 2: %u.\r\n", uiStatus2);

    if (uiStatus1 == 0x90 && uiStatus2 == 0x00)
    {
        pResponse->SetUnsolicitedFlag(TRUE);
        pResponse->SetResultCode(RIL_UNSOL_STK_SESSION_END);
    }

    fRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Exit\r\n");
    return fRet;
}
#endif //USE_STK_RAW_MODE


BOOL CSilo_SIM::ParseXSIM(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 nSIMState = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<SIM state>"
    if (!ExtractUInt(rszPointer, nSIMState, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - ERROR: Could not parse nSIMState.\r\n");
        goto Error;
    }

    //  If we re-inserted a SIM, then do a re-init
    //  April 5/11 - nSIMState of 0 means SIM removed or booted without SIM
    if ( ((9 == m_nXSIMStatePrev) || (0 == m_nXSIMStatePrev)) && (0 != nSIMState) )
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM was inserted!\r\n");

        RIL_requestTimedCallback(triggerSIMInserted, NULL, 0, 500000);
    }

    //  Save SIM state
    m_nXSIMStatePrev = nSIMState;


    //  Flag as unrecognized.
    pResponse->SetUnrecognizedFlag(TRUE);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Exit\r\n");
    return fRet;
}
