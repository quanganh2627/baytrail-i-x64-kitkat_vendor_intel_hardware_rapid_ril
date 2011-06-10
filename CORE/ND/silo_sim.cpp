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
#include "callbacks.h"

#include <cutils/properties.h>
#include <sys/system_properties.h>


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
        { "+SATI: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATI },
        { "+SATN: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATN },
        { "+SATF: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseTermRespConfirm },
        { "+XLOCK: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+XSIM: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXSIM },
        { "+XLEMA: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXLEMA },
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

        case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION:
            if (RIL_E_SUCCESS == rpRsp->GetResultCode())
            {
                g_RadioState.SetRadioSIMUnlocked();
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

        // Calculate PDU length  - including NULL byte, not including quotes
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(g_szNewLine) - 1;
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
                        pInts[0] = SIM_RESET;
                        pInts[1] = NULL;  // see ril.h
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
                                    uiFileTagLength = SemiByteCharsToByte(szFileTagLength[0], szFileTagLength[1]);
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
                                        unsigned int i1 = 0, i2 = 0;
                                        i1 = (unsigned int)SemiByteCharsToByte(szFileID[0], szFileID[1]);
                                        i2 = (unsigned int)SemiByteCharsToByte(szFileID[2], szFileID[3]);
                                        pInts[1] = (i1 << 8) + i2;
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
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - ERROR: cannot allocate pInts\r\n");
                }

            }
        }
    }


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
    //  May 17/11 - nSIMState of 0 means booted without SIM
    //              nSIMState of 9 means SIM removed
    if ( (0 != nSIMState && 9 != nSIMState) && (9 == m_nXSIMStatePrev || 0 == m_nXSIMStatePrev) )
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM was inserted!\r\n");

        RIL_requestTimedCallback(triggerSIMInserted, NULL, 0, 250000);
    }
    else if ( (9 == nSIMState || 0 == nSIMState) && (0 != m_nXSIMStatePrev && 9 != m_nXSIMStatePrev) )
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM was removed or missing!\r\n");

        RIL_requestTimedCallback(triggerSIMRemoved, NULL, 0, 250000);
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

BOOL CSilo_SIM::ParseXLEMA(CResponse* const pResponse, const BYTE*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    unsigned int uiIndex = 0;
    unsigned int uiTotalCnt = 0;
    char szECCItem[MAX_BUFFER_SIZE] = {0};


    if (pResponse == NULL)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() : ERROR : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() : ERROR : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<index>"
    if (!ExtractUInt(rszPointer, uiIndex, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - ERROR: Could not parse uiIndex.\r\n");
        goto Error;
    }

    // Extract ",<total cnt>"
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt(rszPointer, uiTotalCnt, rszPointer) )
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - ERROR: Could not parse uiTotalCnt.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractQuotedString(rszPointer, szECCItem, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - ERROR: Could not parse szECCItem.\r\n");
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
            RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - ERROR: Could not create m_szCCList\r\n");
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
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - uiIndex == uiTotalCnt == %d\r\n", uiTotalCnt);
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - setting ril.ecclist = [%s]\r\n", m_szECCList);
        property_set("ril.ecclist", m_szECCList);
    }

    //  Flag as unrecognized.
    //pResponse->SetUnrecognizedFlag(TRUE);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Exit\r\n");
    return fRet;
}

