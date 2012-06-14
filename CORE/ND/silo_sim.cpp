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
#include "callbacks.h"

#include <cutils/properties.h>
#include <sys/system_properties.h>

// for SIM technical problem (XSIM or XSIMSTATE: 8), report as cardstate error
BOOL g_bReportCardStateError = FALSE;

//
//
CSilo_SIM::CSilo_SIM(CChannel *pChannel)
: CSilo(pChannel),
m_IsReadyForAttach(FALSE)
{
    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+STKCTRLIND: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+STKCC: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+SATI: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATI },
        { "+SATN: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATN },
        { "+SATF: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseTermRespConfirm },
        { "+XLOCK: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXLOCK },
        { "+XSIM: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXSIM },
        { "+XLEMA: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXLEMA },
        { "+XSIMSTATE: ", (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXSIMSTATE },
        { ""           , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseNULL }
    };

    m_pATRspTable = pATRspTable;

    memset(m_szECCList, 0, sizeof(m_szECCList));

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
        case ND_REQ_ID_CHANGESIMPIN:
        case ND_REQ_ID_CHANGESIMPIN2:
        case ND_REQ_ID_SETFACILITYLOCK:
            bRetVal = ParsePin(rpCmd, rpRsp);
            break;
        case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION:
            bRetVal = ParseNetworkPersonalisationPin(rpCmd, rpRsp);
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

        case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION:
            if (RIL_E_SUCCESS == rpRsp->GetResultCode())
            {
                g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
                CSystemManager::GetInstance().TriggerSimUnlockedEvent();
            }
            break;
        case ND_REQ_ID_WRITESMSTOSIM:
            if (RIL_E_SUCCESS != rpRsp->GetResultCode() &&
                    CMS_ERROR_MEMORY_FULL == rpRsp->GetErrorCode())
            {
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
            }
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
                g_RadioState.SetSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - SIM PUK2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PUK2);
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
            free(pInt);
            pInt = NULL;
            goto Error;
        }
    }

    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParsePin() - Exit\r\n");
    return bRetVal;
}

// Helper functions
BOOL CSilo_SIM::ParseNetworkPersonalisationPin(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_VERBOSE("CSilo_SIM::ParseNetworkPersonalisationPin() - Enter\r\n");

    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        switch(rpRsp->GetErrorCode())
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CSilo_SIM::ParseNetworkPersonalisationPin() - Incorrect password");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);
                break;
            case CME_ERROR_NETWORK_PUK_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParseNetworkPersonalisationPin() - NETWORK PUK required");
                rpRsp->SetResultCode(RIL_E_NETWORK_PUK_REQUIRED);
                g_RadioState.SetSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT);
                break;
            default:
                RIL_LOG_INFO("CSilo_SIM::ParseNetworkPersonalisationPin() - Unknown error [%d]", rpRsp->GetErrorCode());
                rpRsp->SetResultCode(RIL_E_GENERIC_FAILURE);
                break;
        }

        rpRsp->FreeData();
        int* pInt = (int *) malloc(sizeof(int));

        if (NULL == pInt)
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseNetworkPersonalisationPin(): Unable to allocate memory for NET PIN retries\r\n");
            goto Error;
        }

        //  (Dec 22/09) I tried entering different values for this, but I don't think it does anything in the UI.
        *pInt = -1;

        if (!rpRsp->SetData((void*) pInt, sizeof(int*), FALSE))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseNetworkPersonalisationPin() : Unable to set data with number of NET PIN retries left\r\n");
            free(pInt);
            pInt = NULL;
            goto Error;
        }
    }

    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseNetworkPersonalisationPin() - Exit\r\n");
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
            case RRIL_CME_ERROR_SIM_NOT_INSERTED:
            {
                RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : SIM Card is absent!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);

                RIL_CardStatus_v6* pCardStatus = (RIL_CardStatus_v6 *) malloc(sizeof(RIL_CardStatus_v6));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus_v6\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus_v6));

                pCardStatus->gsm_umts_subscription_app_index = -1;
                pCardStatus->cdma_subscription_app_index = -1;
                pCardStatus->ims_subscription_app_index = -1;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;
                pCardStatus->card_state = RIL_CARDSTATE_ABSENT;
                pCardStatus->num_applications = 0;

                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus_v6), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    free(pCardStatus);
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            break;

            case RRIL_CME_ERROR_SIM_NOT_READY:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : SIM Card is not ready!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);

                RIL_CardStatus_v6* pCardStatus = (RIL_CardStatus_v6*) malloc(sizeof(RIL_CardStatus_v6));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus_v6\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus_v6));

                pCardStatus->gsm_umts_subscription_app_index = 0;
                pCardStatus->cdma_subscription_app_index = -1;
                pCardStatus->ims_subscription_app_index = -1;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;

                // for XSIM 8 (SIM technical problem), report cardstate error
                if (g_bReportCardStateError)
                {
                    pCardStatus->card_state = RIL_CARDSTATE_ERROR;
                }
                else
                {
                    pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
                }

                pCardStatus->num_applications = 1;

                pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
                pCardStatus->applications[0].app_state = RIL_APPSTATE_DETECTED;
                pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
                pCardStatus->applications[0].aid_ptr = NULL;
                pCardStatus->applications[0].app_label_ptr = NULL;
                pCardStatus->applications[0].pin1_replaced = 0;
                pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
                pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
                pCardStatus->applications[0].pin1_num_retries = -1;
                pCardStatus->applications[0].puk1_num_retries = -1;
                pCardStatus->applications[0].pin2_num_retries = -1;
                pCardStatus->applications[0].puk2_num_retries = -1;
#endif // M2_PIN_RETRIES_FEATURE_ENABLED

                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus_v6), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    free(pCardStatus);
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            break;

            case RRIL_CME_ERROR_SIM_WRONG:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : WRONG SIM Card!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);

                RIL_CardStatus_v6* pCardStatus = (RIL_CardStatus_v6*) malloc(sizeof(RIL_CardStatus_v6));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus_v6\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus_v6));

                pCardStatus->gsm_umts_subscription_app_index = 0;
                pCardStatus->cdma_subscription_app_index = -1;
                pCardStatus->ims_subscription_app_index = -1;
                pCardStatus->universal_pin_state = RIL_PINSTATE_ENABLED_PERM_BLOCKED;
                pCardStatus->card_state = RIL_CARDSTATE_ERROR;  //RIL_CARDSTATE_ABSENT
                pCardStatus->num_applications = 0;

                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus_v6), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    free(pCardStatus);
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            break;

            default:
                break;
        }
    }

    bRetVal = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimStatus() - Exit\r\n");

    return bRetVal;
}


BOOL CSilo_SIM::ParseIndicationSATI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() : Could not find response end\r\n");
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
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - Could not alloc mem for command.\r\n");
        goto Error;
    }
    memset(pszProactiveCmd, 0, sizeof(char) * uiLength);

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength-1] = '\0';

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

BOOL CSilo_SIM::ParseIndicationSATN(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;
    char* pRefresh = NULL;
    char* pFileTag = NULL;
    char szRefreshCmd[] = "8103";
    char szFileTag[] = "92";
    char szFileTagLength[3] = {0};
    char szFileID[5] = {0};
    UINT32 uiFileTagLength = 0;
    char szRefreshType[3] = {0};

    int *pInts = NULL;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got the whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by g_szNewline, so look for g_szNewline and use its
        // position to determine length of PDU string.

        // Calculate PDU length  - including NULL byte, not including quotes
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(g_szNewLine) - 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Calculated PDU String length: %u chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - Could not alloc mem for command.\r\n");
        goto Error;
    }
    memset(pszProactiveCmd, 0, sizeof(char) * uiLength);

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    pResponse->SetUnsolicitedFlag(TRUE);

    //  Need to see if this is a SIM_REFRESH command.
    //  Check for "8103", jump next byte and verify if followed by "01".
    pRefresh = strstr(pszProactiveCmd, szRefreshCmd);
    if (pRefresh)
    {
        UINT32 uiPos = pRefresh - pszProactiveCmd;
        //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

        uiPos += strlen(szRefreshCmd);
        //RIL_LOG_INFO("uiPos = %d  uiLength=%d\r\n", uiPos, uiLength);
        if ( (uiPos + 6) < uiLength)  // 6 is next byte plus "01" plus type of SIM refresh
        {
            //  Skip next byte
            uiPos += 2;
            //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

            if (0 == strncmp(&pszProactiveCmd[uiPos], "01", 2))
            {
                //  Skip to type of SIM refresh
                uiPos += 2;
                //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

                //  See what our SIM Refresh command is
                strncpy(szRefreshType, &pszProactiveCmd[uiPos], 2);
                uiPos += 2;

                RIL_LOG_INFO("*** We found %s SIM_REFRESH   type=[%s]***\r\n", szRefreshCmd, szRefreshType);

                //  If refresh type = "01"  -> SIM_FILE_UPDATE
                //  If refresh type = "00","02","03" -> SIM_INIT
                //  If refresh type = "04","05","06" -> SIM_RESET

                pInts = (int*)malloc(2 * sizeof(int));
                if (NULL != pInts)
                {
                    //  default to SIM_INIT
                    pInts[0] = SIM_INIT;
                    pInts[1] = NULL;

                    //  Check for type of refresh
                    if ( (0 == strncmp(szRefreshType, "00", 2)) ||
                         (0 == strncmp(szRefreshType, "02", 2)) ||
                         (0 == strncmp(szRefreshType, "03", 2)) )
                    {
                        //  SIM_INIT
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_INIT\r\n");
                        pInts[0] = SIM_INIT;
                        pInts[1] = NULL;  // see ril.h
                    }
                    else if ( (0 == strncmp(szRefreshType, "04", 2)) ||
                              (0 == strncmp(szRefreshType, "05", 2)) ||
                              (0 == strncmp(szRefreshType, "06", 2)) )
                    {
                        //  SIM_RESET
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_RESET\r\n");
                        /*
                         * Incase of IMC SUNRISE platform, SIM_RESET refresh
                         * is handled on the modem side. If the Android telephony
                         * framework is informed of this refresh, then it will
                         * initiate a RADIO_POWER off which will interfere with
                         * the SIM RESET procedure on the modem side. So, don't send
                         * the RIL_UNSOL_SIM_REFRESH for SIM_RESET refresh type.
                         */
                        goto event_notify;
                    }
                    else if ( (0 == strncmp(szRefreshType, "01", 2)) )
                    {
                        //  SIM_FILE_UPDATE
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_FILE_UPDATE\r\n");
                        pInts[0] = SIM_FILE_UPDATE;

                        //  Tough part here - need to read file ID(s)
                        //  Android looks for EF_MBDN 0x6FC7 or EF_MAILBOX_CPHS 0x6F17
                        //  File tag is "92".  Just look for "92" from uiPos.
                        if ('\0' != pszProactiveCmd[uiPos])
                        {
                            //  Look for "92"
                            pFileTag = strstr(&pszProactiveCmd[uiPos], szFileTag);
                            if (pFileTag)
                            {
                                //  Found "92" somewhere in rest of string
                                uiPos = pFileTag - pszProactiveCmd;
                                uiPos += strlen(szFileTag);
                                RIL_LOG_INFO("FOUND FileTag uiPos now = %d\r\n", uiPos);
                                if (uiPos < uiLength)
                                {
                                    //  Read length of tag
                                    strncpy(szFileTagLength, &pszProactiveCmd[uiPos], 2);
                                    //RIL_LOG_INFO("file tag length = %s\r\n", szFileTagLength);
                                    uiFileTagLength = (UINT8)SemiByteCharsToByte(szFileTagLength[0], szFileTagLength[1]);
                                    RIL_LOG_INFO("file tag length = %d\r\n", uiFileTagLength);
                                    uiPos += 2;  //  we read the tag length
                                    uiPos += (uiFileTagLength * 2);
                                    //RIL_LOG_INFO("uiPos is now end hopefully = %d  uilength=%d\r\n", uiPos, uiLength);

                                    if (uiPos <= uiLength)
                                    {
                                        //  Read last 2 bytes (last 4 characters) of tag
                                        uiPos -= 4;
                                        //RIL_LOG_INFO("uiPos before reading fileID=%d\r\n", uiPos);
                                        strncpy(szFileID, &pszProactiveCmd[uiPos], 4);
                                        RIL_LOG_INFO("szFileID[%s]\r\n", szFileID);

                                        //  Convert hex string to int
                                        UINT32 ui1 = 0, ui2 = 0;
                                        ui1 = (UINT8)SemiByteCharsToByte(szFileID[0], szFileID[1]);
                                        ui2 = (UINT8)SemiByteCharsToByte(szFileID[2], szFileID[3]);
                                        pInts[1] = (ui1 << 8) + ui2;
                                        RIL_LOG_INFO("pInts[1]=[%d],%04X\r\n", pInts[1], pInts[1]);


                                    }
                                }
                            }

                        }

                    }

                    //  Send out SIM_REFRESH notification
                    RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH, (void*)pInts, 2 * sizeof(int));

                }
                else
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - cannot allocate pInts\r\n");
                }

            }
        }
    }


event_notify:
    //  Normal STK Event notify
    pResponse->SetResultCode(RIL_UNSOL_STK_EVENT_NOTIFY);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char *), FALSE))
    {
        goto Error;
    }

    free(pInts);
    pInts = NULL;

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
        free(pInts);
        pInts = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseTermRespConfirm(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiStatus1;
    UINT32 uiStatus2;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<sw1>"
    if (!ExtractUInt32(rszPointer, uiStatus1, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() - Could not parse sw1.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status 1: %u.\r\n", uiStatus1);

    // Extract "<sw2>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt32(rszPointer, uiStatus2, rszPointer)))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() - Could not parse sw2.\r\n");
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

BOOL CSilo_SIM::ParseXSIM(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 nSIMState = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<SIM state>"
    if (!ExtractUInt32(rszPointer, nSIMState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() - Could not parse nSIMState.\r\n");
        goto Error;
    }

    // Here we assume we don't have card error.
    // This variable will be changed in case we received XSIM=8.
    g_bReportCardStateError = FALSE;


    /// @TODO: Need to revisit the XSIM and radio state mapping
    switch (nSIMState)
    {
        /*
         * XSIM: 3 will be received upon PIN related operations.
         *
         * For PIN related operation occuring after the SIM initialisation,
         * no XSIM: 7 will be sent by modem. So, trigger the radio state
         * change with SIM ready upon XSIM: 3.
         *
         * For PIN related operation occuring during the boot or before
         * the SIM initialisation, then XSIM: 7 will be sent by modem. In this
         * case, radio state  change with SIM ready will be sent upon the
         * receival of XSIM: 7 event.
         */
        case 3: // PIN verified - Ready
            if (m_IsReadyForAttach) {
                RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
                g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            }
            break;
        /*
         * XSIM: 10 and XSIM: 11 will be received when the SIM driver has
         * lost contact of SIM and re-established the contact respectively.
         * After XSIM: 10, either XSIM: 9 or XSIM: 11 will be received.
         * So, no need to trigger SIM_NOT_READY on XSIM: 10. Incase of
         * XSIM: 11, network registration will be restored by the modem
         * itself.
         */
        case 10: // SIM Reactivating
            break;
        case 11: // SIM Reactivated
            g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            break;
        /*
         * XSIM: 2 means PIN verification not needed but not ready for attach.
         * SIM_READY should be triggered only when the modem is ready
         * to attach.(XSIM: 7 or XSIM: 3(in some specific case))
         */
        case 2:
        case 6: // SIM Error
            // The SIM is initialized, but modem is still in the process of it.
            // we can inform Android that SIM is still not ready.
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM NOT READY\r\n");
            g_RadioState.SetSIMState(RRIL_SIM_STATE_NOT_READY);
            break;
        case 8: // SIM Technical problem
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM TECHNICAL PROBLEM\r\n");
            g_RadioState.SetSIMState(RRIL_SIM_STATE_NOT_READY);
            g_bReportCardStateError = TRUE;
            break;
        case 7: // ready for attach (+COPS)
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
            m_IsReadyForAttach = TRUE;
            g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            CSystemManager::GetInstance().TriggerSimUnlockedEvent();
            break;
        case 12: // SIM SMS caching completed
            RIL_LOG_INFO("[RIL STATE] SIM SMS CACHING COMPLETED\r\n");
            triggerQuerySimSmsStoreStatus(NULL);
            break;
        case 0: // SIM not present
        case 9: // SIM Removed
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM REMOVED/NOT PRESENT\r\n");
            m_IsReadyForAttach = FALSE;
            // Fall through to notify Radio and Sim status
        case 1: // PIN verification needed
        case 4: // PUK verification needed
        case 5: // SIM permanently blocked
        default:
            g_RadioState.SetSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT);
            break;
    }

    /*
     * Instead of triggering the GET_SIM_STATUS and QUERY_FACILITY_LOCK
     * requests based on RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
     * android telephony stack is triggering those requests based on
     * RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED.
     * Might change in the future, so its better to trigger
     * the RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED
     */
    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseXLOCK(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLOCK() - Enter\r\n");

    /*
     * If the MT is waiting for any of the following passwords
     * PH-NET PIN, PH-NETSUB PIN, PH-SP PIN then only XLOCK URC will be
     * received.
     */

    BOOL fRet = FALSE;
    int i = 0;
    const char* pszEnd = NULL;

    //  The number of locks returned by +XLOCK URC.
    const int nMAX_LOCK_INFO = 5;

    typedef struct
    {
        char fac[3];
        UINT32 lock_state;
        UINT32 lock_result;
    } S_LOCK_INFO;

    S_LOCK_INFO lock_info[nMAX_LOCK_INFO];

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() : Could not find response end\r\n");
        goto Error;
    }

    memset(lock_info, 0, sizeof(lock_info));

    // Change the number to the number of facility locks supported via XLOCK URC.
    while (i < nMAX_LOCK_INFO)
    {
        memset(lock_info[i].fac, '\0', sizeof(lock_info[i].fac));

        // Extract "<fac>"
        if (!ExtractQuotedString(rszPointer, lock_info[i].fac, sizeof(lock_info[i].fac), rszPointer))
        {
            RIL_LOG_INFO("CSilo_SIM::ParseXLOCK() - Unable to find <fac>!\r\n");
            goto complete;
        }

        // Extract ",<Lock state>"
        if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUInt32(rszPointer, lock_info[i].lock_state, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not parse <lock state>.\r\n");
            goto Error;
        }

        // Extract ",<Lock result>"
        if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUInt32(rszPointer, lock_info[i].lock_result, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not parse <lock result>.\r\n");
            goto Error;
        }

        SkipString(rszPointer, ",", rszPointer);

        i++;
    }

complete:
    // Look for "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not extract response postfix.\r\n");
        goto Error;
    }

    // Walk back over the <CR><LF>
    rszPointer -= strlen(g_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);

    i = 0;
    // Change the number to the number of facility locks supported via XLOCK URC.
    while (i < nMAX_LOCK_INFO)
    {
        RIL_LOG_INFO("lock:%s state:%d result:%d", lock_info[i].fac, lock_info[i].lock_state,
                        lock_info[i].lock_result);

        /// @TODO: Need to revisit the lock state mapping.
        if ((lock_info[i].lock_state == 1 && lock_info[i].lock_result == 1) ||
           (lock_info[i].lock_state == 3 && lock_info[i].lock_result == 2))
        {
            g_RadioState.SetSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT);
            pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
            break;
        }
        i++;
    }

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLOCK() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseXLEMA(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiIndex = 0;
    UINT32 uiTotalCnt = 0;
    char szECCItem[MAX_BUFFER_SIZE] = {0};
    const char szRIL_ECCLIST[] = "ril.ecclist";


    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<index>"
    if (!ExtractUInt32(rszPointer, uiIndex, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse uiIndex.\r\n");
        goto Error;
    }

    // Extract ",<total cnt>"
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiTotalCnt, rszPointer) )
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse uiTotalCnt.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractQuotedString(rszPointer, szECCItem, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse szECCItem.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - Found ECC item=[%d] out of [%d]  ECC=[%s]\r\n", uiIndex, uiTotalCnt, szECCItem);

    //  Skip the rest of the parameters (if any)
    // Look for a "<postfix>" to be sure we got a whole message
    FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer);

    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);


    //  If the uiIndex is 1, then assume reading first ECC code.
    //  Clear the master list and store the code.
    if (1 == uiIndex)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - First index, clear master ECC list, store code=[%s]\r\n", szECCItem);
        if (!PrintStringNullTerminate(m_szECCList, MAX_BUFFER_SIZE, "%s", szECCItem))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not create m_szCCList\r\n");
            goto Error;
        }
    }
    else
    {
        //  else add code to the end.
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - store code=[%s]\r\n", szECCItem);
        strncat(m_szECCList, ",", MAX_BUFFER_SIZE - strlen(m_szECCList) - 1);
        strncat(m_szECCList, szECCItem, MAX_BUFFER_SIZE - strlen(m_szECCList) - 1);
    }

    RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - m_szECCList=[%s]\r\n", m_szECCList);


    //  If the uiIndex is the total count, assume we have all ECC codes.
    //  In that case, set property!
    if (uiIndex == uiTotalCnt)
    {
        char szEccListProp[MAX_PROP_VALUE] = {0};

        //  If sim id == 0 or if sim id is not provided by RILD, then continue
        //  to use "ril.ecclist" property name.
        if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
        {
            strncpy(szEccListProp, szRIL_ECCLIST, MAX_PROP_VALUE-1);
            szEccListProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
        }
        else
        {
            snprintf(szEccListProp, MAX_PROP_VALUE, "%s%s", szRIL_ECCLIST, g_szSIMID);
        }

        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - uiIndex == uiTotalCnt == %d\r\n", uiTotalCnt);
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - setting %s = [%s]\r\n", szEccListProp, m_szECCList);
        property_set(szEccListProp, m_szECCList);
    }

    //  Flag as unrecognized.
    //pResponse->SetUnrecognizedFlag(TRUE);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseXSIMSTATE(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIMSTATE() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 nMode = 0;
    UINT32 nSIMState = 0;
    UINT32 nPBReady = 0;
    UINT32 nSIMSMSReady = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<nMode>"
    if (!ExtractUInt32(rszPointer, nMode, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() - Could not parse nMode.\r\n");
        goto Error;
    }

    // Extract ",<SIM state>"
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nSIMState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() - Could not parse nSIMState.\r\n");
        goto Error;
    }

    // Extract ",<pbready>"
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nPBReady, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() - Could not parse nPBReady.\r\n");
        goto Error;
    }

    // Extract ",<SMS Ready>"
    if (SkipString(rszPointer, ",", rszPointer))
    {
        if (!ExtractUInt32(rszPointer, nSIMSMSReady, rszPointer))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIMSTATE() - Could not parse <SMS Ready>.\r\n");
            goto Error;
        }
    }

    if (nSIMSMSReady)
    {
        triggerQuerySimSmsStoreStatus(NULL);
    }

    //  Skip the rest of the parameters (if any)
    // Look for a "<postfix>" to be sure we got a whole message
    FindAndSkipRspEnd(rszPointer, g_szNewLine, rszPointer);

    //  Back up over the "\r\n".
    rszPointer -= strlen(g_szNewLine);

    // Here we assume we don't have card error.
    // This variable will be changed in case we received XSIM=8.
    g_bReportCardStateError = FALSE;

    /// @TODO: Need to revisit the XSIM and radio state mapping
    switch (nSIMState)
    {
        /*
         * XSIM: 3 will be received upon PIN related operations.
         *
         * For PIN related operation occuring after the SIM initialisation,
         * no XSIM: 7 will be sent by modem. So, trigger the radio state
         * change with SIM ready upon XSIM: 3.
         *
         * For PIN related operation occuring during the boot or before
         * the SIM initialisation, then XSIM: 7 will be sent by modem. In this
         * case, radio state  change with SIM ready will be sent upon the
         * receival of XSIM: 7 event.
         */
        case 3: // PIN verified - Ready
            if (m_IsReadyForAttach) {
                RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
                g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            }
            break;
        /*
         * XSIM: 10 and XSIM: 11 will be received when the SIM driver has
         * lost contact of SIM and re-established the contact respectively.
         * After XSIM: 10, either XSIM: 9 or XSIM: 11 will be received.
         * So, no need to trigger SIM_NOT_READY on XSIM: 10. Incase of
         * XSIM: 11, network registration will be restored by the modem
         * itself.
         */
        case 10: // SIM Reactivating
            break;
        case 11: // SIM Reactivated
            g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            break;
        /*
         * XSIM: 2 means PIN verification not needed but not ready for attach.
         * SIM_READY should be triggered only when the modem is ready
         * to attach.(XSIM: 7 or XSIM: 3(in some specific case))
         */
        case 2:
        case 6: // SIM Error
            // The SIM is initialized, but modem is still in the process of it.
            // we can inform Android that SIM is still not ready.
            RIL_LOG_INFO("CSilo_SIM::ParseXSIMSTATE() - SIM NOT READY\r\n");
            g_RadioState.SetSIMState(RRIL_SIM_STATE_NOT_READY);
            break;
        case 8: // SIM Technical problem
            RIL_LOG_INFO("CSilo_SIM::ParseXSIMSTATE() - SIM TECHNICAL PROBLEM\r\n");
            g_RadioState.SetSIMState(RRIL_SIM_STATE_NOT_READY);
            g_bReportCardStateError = TRUE;
            break;
        case 7: // ready for attach (+COPS)
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
            m_IsReadyForAttach = TRUE;
            g_RadioState.SetSIMState(RRIL_SIM_STATE_READY);
            CSystemManager::GetInstance().TriggerSimUnlockedEvent();
            break;
        case 0: // SIM not present
        case 9: // SIM Removed
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM REMOVED/NOT PRESENT\r\n");
            m_IsReadyForAttach = FALSE;
            // Fall through to notify Radio and Sim status
        case 1: // PIN verification needed
        case 4: // PUK verification needed
        case 5: // SIM permanently blocked
        case 99: // SIM state unknown
        default:
            g_RadioState.SetSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT);
            break;
    }

    /*
     * Instead of triggering the GET_SIM_STATUS and QUERY_FACILITY_LOCK
     * requests based on RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
     * android telephony stack is triggering those requests based on
     * RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED.
     * Might change in the future, so its better to trigger
     * the RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED
     */
    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIMSTATE() - Exit\r\n");
    return fRet;
}


