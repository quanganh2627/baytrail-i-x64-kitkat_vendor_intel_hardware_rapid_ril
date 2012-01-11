////////////////////////////////////////////////////////////////////////////
// te_base.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CTEBase class which handles all requests and
//    basic behavior for responses
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>

#include "types.h"
#include "rillog.h"
#include "../util.h"
#include "extract.h"
#include "nd_structs.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "command.h"
#include "cmdcontext.h"
#include "repository.h"
#include "rildmain.h"
#include "te.h"
#include "te_base.h"
#include "reset.h"
#include "channel_data.h"
#include <cutils/properties.h>


CTEBase::CTEBase() :
m_nNetworkRegistrationType(0),
mShutdown(false),
m_nSimAppType(RIL_APPTYPE_UNKNOWN)
{
    memset(m_szManualMCCMNC, 0, MAX_BUFFER_SIZE);
    memset(m_szPIN, 0, MAX_PIN_SIZE);
}

CTEBase::~CTEBase()
{
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTEBase::CoreGetSimStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetSimStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreGetSimStatus() - Exit\r\n");
    return res;
}


//  bSilentPINEntry = out variable (true if PIN needs to be silently sent)
RIL_RESULT_CODE CTEBase::ParseSimPin(const char*& pszRsp, RIL_CardStatus_v6*& pCardStatus, bool* pbSilentPINEntry)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimPin() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szSimState[MAX_BUFFER_SIZE];

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    pCardStatus = (RIL_CardStatus_v6*)malloc(sizeof(RIL_CardStatus_v6));
    if (NULL == pCardStatus)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Could not allocate memory for RIL_CardStatus_v6.\r\n");
        goto Error;
    }

    memset(pCardStatus, 0, sizeof(RIL_CardStatus_v6));

    // Parse "<prefix>+CPIN: <state><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CPIN: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Could not skip \"+CPIN: \".\r\n");
        goto Error;
    }

    if (!ExtractUnquotedString(pszRsp, g_cTerminator, szSimState, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Could not extract SIM State.\r\n");
        goto Error;
    }

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimPin() - ERROR: Could not skip response postfix.\r\n");
        goto Error;
    }

    pCardStatus->gsm_umts_subscription_app_index = -1;
    pCardStatus->cdma_subscription_app_index = -1;
    pCardStatus->ims_subscription_app_index = -1;
    pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;

    // Number of apps is 1 (gsm) if SIM present. Set to 0 if absent.
    pCardStatus->num_applications = 0;

    if (0 == strcmp(szSimState, "READY"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_READY\r\n");
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_READY;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_READY;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
    }
    else if (0 == strcmp(szSimState, "SIM PIN"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_PIN\r\n");

        //  Were we previously informed about modem cold boot?
#if defined(M2_PIN_CACHING_FEATURE_ENABLED)
        if (pbSilentPINEntry && PCache_GetUseCachedPIN())
        {
            RIL_LOG_INFO("CTEBase::ParseSimPin() - Use cached PIN\r\n");
            *pbSilentPINEntry = true;

            //  Fake SIM READY for now.
            //g_RadioState.SetSIMState(RADIO_STATE_SIM_READY);
            pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
            pCardStatus->num_applications = 1;
            pCardStatus->gsm_umts_subscription_app_index = 0;

            pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
            pCardStatus->applications[0].app_state = RIL_APPSTATE_READY;
            pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_READY;
            pCardStatus->applications[0].aid_ptr = NULL;
            pCardStatus->applications[0].app_label_ptr = NULL;
            pCardStatus->applications[0].pin1_replaced = 0;
            pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
            pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
        }
        else
#endif
        {
            //g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
            pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
            pCardStatus->num_applications = 1;
            pCardStatus->gsm_umts_subscription_app_index = 0;

            pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
            pCardStatus->applications[0].app_state = RIL_APPSTATE_PIN;
            pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
            pCardStatus->applications[0].aid_ptr = NULL;
            pCardStatus->applications[0].app_label_ptr = NULL;
            pCardStatus->applications[0].pin1_replaced = 0;
            pCardStatus->applications[0].pin1 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
            pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
        }
    }
    else if (0 == strcmp(szSimState, "SIM PUK"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_PUK\r\n");
        //g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_PUK;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_ENABLED_BLOCKED;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
    }
    else if (0 == strcmp(szSimState, "PH-NET PIN"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_NETWORK_PERSONALIZATION\r\n");
        //g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
    }
    else if (0 == strcmp(szSimState, "PH-NET PUK"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_NETWORK_PERSONALIZATION PUK\r\n");
        //g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK_PUK;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
    }
    else if (0 == strcmp(szSimState, "SIM PIN2"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_PIN2\r\n");
        //g_RadioState.SetRadioSIMLocked();
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_READY;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
    }
    else if (0 == strcmp(szSimState, "SIM PUK2"))
    {
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_PUK2\r\n");
        //g_RadioState.SetRadioSIMLocked();
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_READY;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_ENABLED_BLOCKED;
    }
    else
    {
        // Anything not covered above gets treated as NO SIM
        RIL_LOG_INFO("CTEBase::ParseSimPin() - SIM Status: RIL_SIM_ABSENT\r\n");
        //g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
        pCardStatus->card_state = RIL_CARDSTATE_ABSENT;
        pCardStatus->num_applications = 0;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::ParseSimPin() - Exit\r\n");

    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetSimStatus() - Enter\r\n");

    const char * pszRsp = rRspData.szResponse;
    RIL_CardStatus_v6* pCardStatus = NULL;
    RIL_RESULT_CODE res = ParseSimPin(pszRsp, pCardStatus);
    if (res != RRIL_RESULT_OK)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetSimStatus() - ERROR: Could not parse Sim Pin.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pCardStatus;
    rRspData.uiDataSize  = sizeof(RIL_CardStatus_v6);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCardStatus);
        pCardStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetSimStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPin(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPin() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char **pszCmdData = NULL;
    char *pszPassword = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin() - ERROR: Invalid input\r\n");
        goto Error;
    }

    if (sizeof(char*) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin() - ERROR: Request data was of unexpected size!\r\n");
        goto Error;
    }

    pszCmdData = (char**)pData;
    pszPassword = pszCmdData[0];

    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin() - ERROR: SIM PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN=\"%s\";+CCID\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin() - ERROR: Failed to write command to buffer!\r\n");
        goto Error;
    }

    //  Store PIN
    strncpy(m_szPIN, pszPassword, MAX_PIN_SIZE);

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPin() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    const char * pszRsp = rRspData.szResponse;
    char szUICCID[MAX_PROP_VALUE] = {0};

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseEnterSimPin() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    // Cache the PIN code only if we are not silently entering it
    if (!PCache_GetUseCachedPIN() )
    {
        // Parse "<prefix>+CCID: <ICCID><postfix>"
        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+CCID: ", pszRsp))
        {
            if (!ExtractUnquotedString(pszRsp, g_cTerminator, szUICCID, MAX_PROP_VALUE, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - Cannot parse UICC ID\r\n");
                szUICCID[0] = '\0';
            }

            SkipRspEnd(pszRsp, g_szNewLine, pszRsp);
        }

        //  Cache PIN1 value
        PCache_Store_PIN(szUICCID, m_szPIN);

        //  Clear it locally.
        memset(m_szPIN, 0, MAX_PIN_SIZE);
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPuk(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPuk() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszPUK;
    char* pszNewPIN;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPUK = ((char **) pData)[0];
    pszNewPIN = ((char **) pData)[1];

    if ((NULL == pszPUK) || (NULL == pszNewPIN))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk() - ERROR: PUK or new PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "ATD**05*%s*%s*%s#\r", pszPUK, pszNewPIN, pszNewPIN))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPuk() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPuk(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPuk() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;

    // If the response to PUK unlock is OK, then we are unlocked.
    // rril will get XSIM: <status> which will indicate whether the
    // SIM is ready or not.

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseEnterSimPuk() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPuk() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPin2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPin2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char **pszCmdData = NULL;
    char *pszPassword = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin2() - ERROR: Invalid input\r\n");
        goto Error;
    }

    if (sizeof(char*) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin2() - ERROR: Request data was of unexpected size!\r\n");
        goto Error;
    }

    pszCmdData = (char**)pData;
    pszPassword = pszCmdData[0];

    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin2() - ERROR: SIM PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN2=\"%s\"\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPin2() - ERROR: Failed to write command to buffer!\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPin2() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseEnterSimPin2() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPuk2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPuk2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszPUK2;
    char* pszNewPIN2;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk2() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk2() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPUK2 = ((char **) pData)[0];
    pszNewPIN2 = ((char **) pData)[1];

    if ((NULL == pszPUK2) || (NULL == pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk2() - ERROR: PUK2 or new PIN2 was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "ATD**052*%s*%s*%s#\r", pszPUK2, pszNewPIN2, pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterSimPuk2() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreEnterSimPuk2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPuk2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPuk2() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseEnterSimPuk2() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseEnterSimPuk2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
RIL_RESULT_CODE CTEBase::CoreChangeSimPin(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreChangeSimPin() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszOldPIN;
    char* pszNewPIN;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszOldPIN = ((char **) pData)[0];
    pszNewPIN = ((char **) pData)[1];

    if ((NULL == pszOldPIN) || (NULL == pszNewPIN))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin() - ERROR: old or new PIN was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPWD=\"SC\",\"%s\",\"%s\";+CCID\r", pszOldPIN, pszNewPIN))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    //  Store PIN
    strncpy(m_szPIN, pszNewPIN, MAX_PIN_SIZE);

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreChangeSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseChangeSimPin() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    char szUICCID[MAX_PROP_VALUE] = {0};
    int *pnRetries = NULL;

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseChangeSimPin() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    // Parse "<prefix>+CCID: <ICCID><postfix>"
    SkipRspStart(pszRsp, g_szNewLine, pszRsp);

    if (SkipString(pszRsp, "+CCID: ", pszRsp))
    {
        if (!ExtractUnquotedString(pszRsp, g_cTerminator, szUICCID, MAX_PROP_VALUE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseChangeSimPin() - Cannot parse UICC ID\r\n");
            szUICCID[0] = '\0';
        }

        SkipRspEnd(pszRsp, g_szNewLine, pszRsp);
    }

    //  Cache PIN1 value
    PCache_Store_PIN(szUICCID, m_szPIN);

    //  Clear it locally.
    memset(m_szPIN, 0, MAX_PIN_SIZE);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseChangeSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
RIL_RESULT_CODE CTEBase::CoreChangeSimPin2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreChangeSimPin2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszOldPIN2;
    char* pszNewPIN2;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin2() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin2() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszOldPIN2 = ((char **) pData)[0];
    pszNewPIN2 = ((char **) pData)[1];

    if ((NULL == pszOldPIN2) || (NULL == pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin2() - ERROR: old or new PIN2 was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPWD=\"P2\",\"%s\",\"%s\"\r", pszOldPIN2, pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeSimPin2() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreChangeSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseChangeSimPin2() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseChangeSimPin2() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseChangeSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
RIL_RESULT_CODE CTEBase::CoreEnterNetworkDepersonalization(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreEnterNetworkDepersonalization() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszPassword;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterNetworkDepersonalization() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterNetworkDepersonalization() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPassword = ((char **) pData)[0];

    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterNetworkDepersonalization() - ERROR: Depersonalization code was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLCK=\"PN\",0,\"%s\"\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreEnterNetworkDepersonalization() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseEnterNetworkDepersonalization() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseEnterNetworkDepersonalization() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
RIL_RESULT_CODE CTEBase::CoreGetCurrentCalls(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetCurrentCalls() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLCC\r"))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGetCurrentCalls() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGetCurrentCalls() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetCurrentCalls(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetCurrentCalls() - Enter\r\n");

    UINT32 nValue;
    UINT32 nUsed = 0;
    UINT32 nAllocated = 0;

    const char* szRsp = rRspData.szResponse;
    const char* szDummy = rRspData.szResponse;
    char szAddress[MAX_BUFFER_SIZE];

    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;

    BOOL bSuccess;

    P_ND_CALL_LIST_DATA pCallListData = NULL;
    int  nCalls = 0;

    // Parse "<prefix>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetCurrentCalls() - ERROR: Couldn't find rsp start\r\n");
        goto Error;
    }

    // Count up how many calls are in progress
    while (FindAndSkipString(szDummy, "+CLCC: ", szDummy))
    {
        nCalls++;
    }

    if (nCalls >= RRIL_MAX_CALL_ID_COUNT)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetCurrentCalls() - ERROR: Can't process %d calls due to insufficient space.\r\n", nCalls);
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTEBase::ParseGetCurrentCalls() - Found %d calls\r\n", nCalls);
    }

    //  If we have zero calls, don't allocate data.
    if (nCalls >= 1)
    {
        pCallListData = (P_ND_CALL_LIST_DATA)malloc(sizeof(S_ND_CALL_LIST_DATA));
        if (NULL == pCallListData)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseGetCurrentCalls() - ERROR: Could not allocate memory for a S_ND_CALL_LIST_DATA struct.\r\n");
            goto Error;
        }
        memset(pCallListData, 0, sizeof(S_ND_CALL_LIST_DATA));

        // Parse "+CLCC: "
        while (FindAndSkipString(szRsp, "+CLCC: ", szRsp))
        {
            // Parse "<id>"
            if (!ExtractUInt32(szRsp, nValue, szRsp))
            {
                goto Continue;
            }

            pCallListData->pCallData[nUsed].index = nValue;

            // Parse ",<direction>"
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractUpperBoundedUInt32(szRsp, 2, nValue, szRsp))
            {
                goto Continue;
            }

            pCallListData->pCallData[nUsed].isMT = nValue;

            // Parse ",<status>"
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractUInt32(szRsp, nValue, szRsp))
            {
                goto Continue;
            }

            pCallListData->pCallData[nUsed].state = (RIL_CallState)nValue;

            // Parse ",<type>"
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractUInt32(szRsp, nValue, szRsp))
            {
                goto Continue;
            }

            // If nValue is non-zero, then we are not in a voice call
            pCallListData->pCallData[nUsed].isVoice = nValue ? FALSE : TRUE;
            pCallListData->pCallData[nUsed].isVoicePrivacy = 0; // not used in GSM


            // Parse ",<multiparty>"
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractUpperBoundedUInt32(szRsp, 2, nValue, szRsp))
            {
                goto Continue;
            }

            pCallListData->pCallData[nUsed].isMpty = nValue;

            // Parse ","
            if (SkipString(szRsp, ",", szRsp))
            {
                // Parse "<address>,<type>"
                if (ExtractQuotedString(szRsp, szAddress, MAX_BUFFER_SIZE, szRsp))
                {
                    pCallListData->pCallData[nUsed].number = pCallListData->pCallNumberBuffers[nUsed];
                    strncpy(pCallListData->pCallNumberBuffers[nUsed], szAddress, MAX_BUFFER_SIZE);

                    if (!SkipString(szRsp, ",", szRsp) ||
                        !ExtractUpperBoundedUInt32(szRsp, 0x100, nValue, szRsp))
                    {
                        goto Continue;
                    }

                    pCallListData->pCallData[nUsed].toa = nValue;
                }
                else
                {
                    // If we couldn't parse an address, then it might be empty,
                    // meaning the ID is blocked. Since the address parameter
                    // is present, make sure the type also exists before continuing.
                    if (!SkipString(szRsp, ",", szRsp)                 ||
                        !ExtractUpperBoundedUInt32(szRsp, 0x100, nValue, szRsp))
                    {
                        goto Continue;
                    }
                }

                // Parse ","
                if (SkipString(szRsp, ",", szRsp))
                {
                    // Parse "<description>"
                    //WCHAR description[MAX_BUFFER_SIZE];
                    //if (!ExtractQuotedUnicodeHexStringToUnicode(szRsp, description, MAX_BUFFER_SIZE, szRsp))
                    char description[MAX_BUFFER_SIZE];
                    if (!ExtractQuotedString(szRsp, description, MAX_BUFFER_SIZE, szRsp))
                    {
                        RIL_LOG_WARNING("CTEBase::ParseGetCurrentCalls() - WARNING: Failed to extract call name\r\n");
                        goto Continue;
                    }
                    else
                    {
                        int i;
                        for (i = 0;
                             i < MAX_BUFFER_SIZE && description[i];
                             pCallListData->pCallNameBuffers[nUsed][i] = (char) description[i], ++i);

                        if (i < MAX_BUFFER_SIZE)
                        {
                            pCallListData->pCallNameBuffers[nUsed][i] = '\0';
                        }
                        else
                        {
                            pCallListData->pCallNameBuffers[nUsed][MAX_BUFFER_SIZE - 1] = '\0';
                            RIL_LOG_WARNING("CTEBase::ParseGetCurrentCalls() - WARNING: Buffer overflow in name buffer\r\n");
                        }

                         pCallListData->pCallData[nUsed].name = pCallListData->pCallNameBuffers[nUsed];
                         pCallListData->pCallData[nUsed].namePresentation = 0;
                    }
                }
            }

            pCallListData->pCallData[nUsed].als = 0;
            pCallListData->pCallData[nUsed].numberPresentation = 0;

            pCallListData->pCallPointers[nUsed] = &(pCallListData->pCallData[nUsed]);

            // Increment the array index
            nUsed++;

Continue:
            // Find "<postfix>"
            bSuccess = SkipRspEnd(szRsp, g_szNewLine, szRsp);

            // Note: WaveCom euro radios forget to include the <cr><lf> between +CLCC lines,
            // so for wavecom don't worry if we don't find these characters
            if (!bSuccess)
            {
                goto Error;
            }
        }

        rRspData.pData  = (void*)pCallListData;
        rRspData.uiDataSize = nUsed * sizeof(RIL_Call*);
    }
    else
    {
        //  No calls
        rRspData.pData = NULL;
        rRspData.uiDataSize = 0;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCallListData);
        pCallListData = NULL;
    }


    RIL_LOG_VERBOSE("CTEBase::ParseGetCurrentCalls() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DIAL 10
//
RIL_RESULT_CODE CTEBase::CoreDial(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDial() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_Dial *pRilDial   = NULL;
    const char *clir;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDial() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_Dial) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDial() - ERROR: Invalid data size=%d.\r\n, uiDataSize");
        goto Error;
    }

    pRilDial = (RIL_Dial*)pData;

    switch (pRilDial->clir)
    {
        case 1:  // invocation
            clir = "I";
            break;

        case 2:  // suppression
            clir = "i";
            break;

        default:  // subscription default
            clir = "";
            break;
    }

    if (PrintStringNullTerminate(rReqData.szCmd1,  sizeof(rReqData.szCmd1), "ATD%s%s;\r",
                pRilDial->address, clir))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDial() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDial(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDial() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDial() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMSI 11
//
RIL_RESULT_CODE CTEBase::CoreGetImsi(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetImsi() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CIMI\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreGetImsi() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImsi(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetImsi() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    char * szSerialNumber = NULL;

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImsi() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    szSerialNumber = (char*)malloc(MAX_PROP_VALUE);
    if (NULL == szSerialNumber)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImsi() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }
    memset(szSerialNumber, 0x00, MAX_PROP_VALUE);

    // Parse "<prefix><serial_number><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !ExtractUnquotedString(pszRsp, g_cTerminator, szSerialNumber, MAX_PROP_VALUE, pszRsp) ||
        !SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImsi() - ERROR: Could not extract the IMSI string.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)szSerialNumber;
    rRspData.uiDataSize  = sizeof(char*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szSerialNumber);
        szSerialNumber = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetImsi() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP 12
//
RIL_RESULT_CODE CTEBase::CoreHangup(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHangup() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnLine = NULL;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangup() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangup() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pnLine = (int*)pData;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CHLD=1%u\r", pnLine[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:

    RIL_LOG_VERBOSE("CTEBase::CoreHangup() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangup(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHangup() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseHangup() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
RIL_RESULT_CODE CTEBase::CoreHangupWaitingOrBackground(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHangupWaitingOrBackground() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=0\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHangupWaitingOrBackground() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
RIL_RESULT_CODE CTEBase::CoreHangupForegroundResumeBackground(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHangupForegroundResumeBackground() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=1\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHangupForegroundResumeBackground() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
// RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
//
RIL_RESULT_CODE CTEBase::CoreSwitchHoldingAndActive(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSwitchHoldingAndActive() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=2\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSwitchHoldingAndActive() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CONFERENCE 16
//
RIL_RESULT_CODE CTEBase::CoreConference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreConference() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=3\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreConference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseConference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseConference() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseConference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_UDUB 17
//
RIL_RESULT_CODE CTEBase::CoreUdub(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreUdub() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=0\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreUdub() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseUdub(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseUdub() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseUdub() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
RIL_RESULT_CODE CTEBase::CoreLastCallFailCause(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreLastCallFailCause() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CEER\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreLastCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseLastCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseLastCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32      uiCause  = 0;
    int*      pCause   = NULL;

    if (!ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseLastCallFailCause() - ERROR: Parsing of CEER failed\r\n");
        goto Error;
    }

    pCause= (int*) malloc(sizeof(int));
    if (NULL == pCause)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseLastCallFailCause() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    //  Some error cases here are different.
    if (279 == uiCause)
        uiCause = CALL_FAIL_FDN_BLOCKED;
    else if ( (280 == uiCause) || (8 == uiCause) )
        uiCause = CALL_FAIL_CALL_BARRED;

    //@TODO: cause code mapping needs to be revisited
    switch(uiCause)
    {
        case CALL_FAIL_NORMAL:
        case CALL_FAIL_BUSY:
        case CALL_FAIL_CONGESTION:
        case CALL_FAIL_ACM_LIMIT_EXCEEDED:
        case CALL_FAIL_CALL_BARRED:
        case CALL_FAIL_FDN_BLOCKED:
        case CALL_FAIL_IMSI_UNKNOWN_IN_VLR:
        case CALL_FAIL_IMEI_NOT_ACCEPTED:
            *pCause = (int) uiCause;
            break;

        default:
            *pCause = (int) CALL_FAIL_ERROR_UNSPECIFIED;
            break;
    }

    rRspData.pData    = (void*) pCause;
    rRspData.uiDataSize   = sizeof(int*);
    RIL_LOG_INFO("CTEBase::ParseLastCallFailCause() - Last call fail cause [%d]\r\n", uiCause);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCause);
        pCause = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseLastCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTEBase::CoreSignalStrength(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSignalStrength() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

#if defined(SIMULATE_UNSOL_MODEM_RESET)
    //  NOTE: Uncomment this block to simulate unsolicited modem reset
    static int nCount = 0;
    nCount++;
    RIL_LOG_INFO("COUNT = %d\r\n", nCount);
    if (nCount == 4)
    {
        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CFUN=15\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else

#endif // SIMULATE_UNSOL_MODEM_RESET

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CSQ\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    RIL_SignalStrength_v6* pSigStrData = ParseQuerySignalStrength(rRspData);
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }


    rRspData.pData   = (void*)pSigStrData;
    rRspData.uiDataSize  = sizeof(RIL_SignalStrength_v6);

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::ParseSignalStrength() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_VOICE_REGISTRATION_STATE 20
//
RIL_RESULT_CODE CTEBase::CoreRegistrationState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=2;+CREG?;+CREG=0\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    S_ND_REG_STATUS regStatus;
    P_ND_REG_STATUS pRegStatus = NULL;

    pRegStatus = (P_ND_REG_STATUS)malloc(sizeof(S_ND_REG_STATUS));
    if (NULL == pRegStatus)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseRegistrationState() - ERROR: Could not allocate memory for a S_ND_REG_STATUS struct.\r\n");
        goto Error;
    }
    memset(pRegStatus, 0, sizeof(S_ND_REG_STATUS));

    if (!CTE::ParseCREG(pszRsp, FALSE, regStatus))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseRegistrationState() - ERROR in parsing response.\r\n");
        goto Error;
    }

    CTE::GetTE().StoreRegistrationInfo(&regStatus, FALSE);
    CTE::GetTE().CopyCachedRegistrationInfo(pRegStatus, FALSE);

    // We cheat with the size here.
    // Although we have allocated a S_ND_REG_STATUS struct, we tell
    // Android that we have only allocated a S_ND_REG_STATUS_POINTERS
    // struct since Android is expecting to receive an array of string pointers.
    // Note that we only tell Android about the 14 pointers it supports for CREG notifications.

    rRspData.pData  = (void*)pRegStatus;
    rRspData.uiDataSize = sizeof(S_ND_REG_STATUS_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRegStatus);
        pRegStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTEBase::CoreGPRSRegistrationState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGPRSRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XREG=2;+XREG?;+XREG=0\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGPRSRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGPRSRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    S_ND_GPRS_REG_STATUS psRegStatus;
    P_ND_GPRS_REG_STATUS pGPRSRegStatus = NULL;

    pGPRSRegStatus = (P_ND_GPRS_REG_STATUS)malloc(sizeof(S_ND_GPRS_REG_STATUS));
    if (NULL == pGPRSRegStatus)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGPRSRegistrationState() - ERROR: Could not allocate memory for a S_ND_GPRS_REG_STATUS struct.\r\n");
        goto Error;
    }
    memset(pGPRSRegStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));

    if (!CTE::ParseXREG(pszRsp, FALSE, psRegStatus))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGPRSRegistrationState() - ERROR in parsing response.\r\n");
        goto Error;
    }

    CTE::GetTE().StoreRegistrationInfo(&psRegStatus, TRUE);
    CTE::GetTE().CopyCachedRegistrationInfo(pGPRSRegStatus, TRUE);

    // We cheat with the size here.
    // Although we have allocated a S_ND_GPRS_REG_STATUS struct, we tell
    // Android that we have only allocated a S_ND_GPRS_REG_STATUS_POINTERS
    // struct since Android is expecting to receive an array of string pointers.

    rRspData.pData  = (void*)pGPRSRegStatus;
    rRspData.uiDataSize = sizeof(S_ND_GPRS_REG_STATUS_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pGPRSRegStatus);
        pGPRSRegStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGPRSRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTEBase::CoreOperator(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreOperator() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XCOPS=12;+XCOPS=11;+XCOPS=13;+XCOPS=6;+XCOPS=5\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreOperator() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseOperator(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 uiType = 0;
    const int NAME_SIZE = 50;
    char szFullName[NAME_SIZE] = {0};
    char szShortName[NAME_SIZE] = {0};
    BOOL isNitzNameAvailable = FALSE;
    char szPlmnName[NAME_SIZE] = {0};
    P_ND_OP_NAMES pOpNames = NULL;

    pOpNames = (P_ND_OP_NAMES)malloc(sizeof(S_ND_OP_NAMES));
    if (NULL == pOpNames)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not allocate memory for S_ND_OP_NAMES struct.\r\n");
        goto Error;
    }
    memset(pOpNames, 0, sizeof(S_ND_OP_NAMES));

    /*
     * XCOPS follows a fall back mechanism if a requested type is not available.
     * For requested type 6, fallback types are 4 or 2 or 0
     * For requested type 5, fallback types are 3 or 1 or 0
     * When registered, requested type 11,12 and 13 will always return the
     * currently registered PLMN information
     * Other details can be found in the C-AT specifications.
     *
     * Fallback's are ignored as framework already has this information.
     */

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());

    // Skip "+XCOPS: "
    while (SkipString(pszRsp, "+XCOPS: ", pszRsp))
    {
        // Extract "<Type>"
        if (!ExtractUInt32(pszRsp, uiType, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract <mode>.\r\n");
            goto Error;
        }

        // Extract ","
        if (SkipString(pszRsp, ",", pszRsp))
        {
            // Based on type get the long/short/numeric network name
            switch (uiType)
            {
                // Long name based on NITZ
                case 6:
                {
                    if (!ExtractQuotedString(pszRsp, szFullName, NAME_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract the Long Format Operator Name.\r\n");
                        goto Error;
                    }
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - NITZ Long name: %s\r\n", szFullName);
                    isNitzNameAvailable = TRUE;
                }
                break;

                // Short name based on NITZ
                case 5:
                {
                    if (!ExtractQuotedString(pszRsp, szShortName, NAME_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract the Short Format Operator Name.\r\n");
                        goto Error;
                    }
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - NITZ Short name: %s\r\n", szShortName);
                    isNitzNameAvailable = TRUE;
                }
                break;

                // Long PLMN name(When PS or CS is registered)
                case 12:
                {
                    if (!ExtractQuotedString(pszRsp, szPlmnName, sizeof(szPlmnName), pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract the Long Format Operator Name.\r\n");
                        goto Error;
                    }
                    strncpy(pOpNames->szOpNameLong, szPlmnName, strlen(szPlmnName));
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - PLMN Long Name: %s\r\n", pOpNames->szOpNameLong);
                }
                break;

                // Short PLMN name(When PS or CS is registered)
                case 11:
                {
                    if (!ExtractQuotedString(pszRsp, szPlmnName, sizeof(szPlmnName), pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract the Short Format Operator Name.\r\n");
                        goto Error;
                    }
                    strncpy(pOpNames->szOpNameShort, szPlmnName, strlen(szPlmnName));
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - PLMN Short Name: %s\r\n", pOpNames->szOpNameShort);
                }
                break;

                // numeric format of network MCC/MNC even in limited service
                case 13:
                {
                    if (!ExtractQuotedString(pszRsp, szPlmnName, sizeof(szPlmnName), pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract the Long Format Operator Name.\r\n");
                        goto Error;
                    }
                    strncpy(pOpNames->szOpNameNumeric, szPlmnName, strlen(szPlmnName));
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - PLMN Numeric code: %s\r\n", pOpNames->szOpNameNumeric);
                }
                break;

                default:
                {
                    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Format not handled.\r\n");
                    break;
                }
            }
        }
        else
        {
            RIL_LOG_VERBOSE("CTEBase::ParseOperator() - <network name> not present.\r\n");
            pOpNames->sOpNamePtrs.pszOpNameLong    = NULL;
            pOpNames->sOpNamePtrs.pszOpNameShort   = NULL;
            pOpNames->sOpNamePtrs.pszOpNameNumeric = NULL;
        }

        // Extract "<CR><LF>"
        if (!FindAndSkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseOperator() - ERROR: Could not extract response postfix.\r\n");
            goto Error;
        }

        // If we have another line to parse, get rid of its prefix now.
        // Note that this will do nothing if we don't have another line to parse.
        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());
    }

    if (isNitzNameAvailable)
    {
        strncpy(pOpNames->szOpNameLong, szFullName, strlen(szFullName));
        strncpy(pOpNames->szOpNameShort, szShortName, strlen(szShortName));
    }

    pOpNames->sOpNamePtrs.pszOpNameLong    = pOpNames->szOpNameLong;
    pOpNames->sOpNamePtrs.pszOpNameShort   = pOpNames->szOpNameShort;
    pOpNames->sOpNamePtrs.pszOpNameNumeric = pOpNames->szOpNameNumeric;

    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Long Name: \"%s\", Short Name: \"%s\", Numeric Name: \"%s\"\r\n",
                    pOpNames->sOpNamePtrs.pszOpNameLong, pOpNames->sOpNamePtrs.pszOpNameShort,
                    pOpNames->sOpNamePtrs.pszOpNameNumeric);


    // We cheat with the size here.
    // Although we have allocated a S_ND_OP_NAMES struct, we tell
    // Android that we have only allocated a S_ND_OP_NAME_POINTERS
    // struct since Android is expecting to receive an array of string pointers.

    rRspData.pData  = pOpNames;
    rRspData.uiDataSize = sizeof(S_ND_OP_NAME_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pOpNames);
        pOpNames = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseOperator() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RADIO_POWER 23
//
RIL_RESULT_CODE CTEBase::CoreRadioPower(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreRadioPower() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    bool bTurnRadioOn = (0 == *(int *)pData) ? false : true;

    //  Store setting in context.
    rReqData.pContextData = (void*)bTurnRadioOn;

    // Retrieve the shutdown property
    char szShutdownActionProperty[PROPERTY_VALUE_MAX] = {'\0'};
    if (property_get("sys.shutdown.requested", szShutdownActionProperty, NULL) &&
                ('0' == szShutdownActionProperty[0]) )
    {
        RIL_LOG_INFO("CTEBase::CoreRadioPower - Shutdown requested\r\n");
        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CFUN=0\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
            mShutdown = true;
        }
    }
    else if (CopyStringNullTerminate(rReqData.szCmd1, (true == bTurnRadioOn) ?
                                        "AT+CFUN=1\r" : "AT+CFUN=4\r",
                                        sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreRadioPower() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseRadioPower(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseRadioPower() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    //  Extract power setting from context
    int nPower = (int)rRspData.pContextData;

    if (0 == nPower)
    {
        //  Turning off phone
        g_RadioState.SetRadioState(FALSE);
        if (mShutdown)
        {
            do_request_clean_up(eRadioError_ForceShutdown, __LINE__, __FILE__);
        }
    }
    else if (1 == nPower)
    {
        //  Turning on phone
        g_RadioState.SetRadioState(TRUE);
        CSystemManager::GetInstance().TriggerModemPowerOnEvent();
    }

    RIL_LOG_VERBOSE("CTEBase::ParseRadioPower() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF 24
//
RIL_RESULT_CODE CTEBase::CoreDtmf(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDtmf() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char tone;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDtmf() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDtmf() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    tone = ((char *)pData)[0];

    //  Need to stop any outstanding tone first.
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+VTS=%c\r", tone))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDtmf() - ERROR: Unable to write VTS=tone string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDtmf() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDtmf() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS 25
//
RIL_RESULT_CODE CTEBase::CoreSendSms(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSendSms() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char **       pszCmdData   = NULL;
    char *        szSMSAddress = NULL;
    char *        szPDU        = NULL;

    int nPDULength = 0;
    char szNoAddress[] = "00";

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSms() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != 2 * sizeof(char *))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSms() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    pszCmdData   = (char**)pData;

    szSMSAddress = pszCmdData[0];

    szPDU        = pszCmdData[1];

    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSms() - ERROR: Invalid input(s).\r\n");
        goto Error;
    }

    // 2 chars per byte.
    nPDULength = (strlen(szPDU) / 2);

    if (NULL == szSMSAddress)
    {
        szSMSAddress = szNoAddress;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGS=%u\r", nPDULength))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSms() - ERROR: Cannot create CMGS command\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s%s\x1a", szSMSAddress, szPDU))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSms() - ERROR: Cannot create CMGS PDU\r\n");
        goto Error;
    }

    RIL_LOG_INFO("Payload: %s\r\n", CRLFExpandedString(rReqData.szCmd2, strlen(rReqData.szCmd2)).GetString());

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSendSms() - Exit\r\n");
    return res;

}

RIL_RESULT_CODE CTEBase::ParseSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSendSms() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    P_ND_SEND_MSG pSendMsg = NULL;
    UINT32          uiMsgRef;

    pSendMsg = (P_ND_SEND_MSG)malloc(sizeof(S_ND_SEND_MSG));
    if (NULL == pSendMsg)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not allocate memory for a S_ND_SEND_MSG struct.\r\n");
        goto Error;
    }
    memset(pSendMsg, 0, sizeof(S_ND_SEND_MSG));

    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not parse response prefix.\r\n");
        goto Error;
    }

    //  Sometimes modems add another rspStart here due to sending of a PDU.
    SkipRspStart(pszRsp, g_szNewLine, pszRsp);

    if (!SkipString(pszRsp, "+CMGS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not parse \"+CMGS: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiMsgRef, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not parse <msgRef>.\r\n");
        goto Error;
    }
    else
    {
        pSendMsg->smsRsp.messageRef = (int)uiMsgRef;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUnquotedString(pszRsp, g_cTerminator, pSendMsg->szAckPDU, 160, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not parse <ackPdu>.\r\n");
            goto Error;
        }

        pSendMsg->smsRsp.ackPDU = pSendMsg->szAckPDU;
    }
    else
    {
        pSendMsg->smsRsp.ackPDU = NULL;
    }

    //  Error code is n/a.
    pSendMsg->smsRsp.errorCode = -1;

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSms() - ERROR: Could not parse response postfix.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pSendMsg;
    rRspData.uiDataSize  = sizeof(RIL_SMS_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSendMsg);
        pSendMsg = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSendSms() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
RIL_RESULT_CODE CTEBase::CoreSendSmsExpectMore(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSendSmsExpectMore() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char **       pszCmdData   = NULL;
    char *        szSMSAddress = NULL;
    char *        szPDU        = NULL;

    int nPDULength = 0;
    char szNoAddress[] = "00";

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSmsExpectMore() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != 2 * sizeof(char *))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSmsExpectMore() - ERROR: Invalid data size.  uiDataSize=[%d]\r\n", uiDataSize);
        goto Error;
    }

    pszCmdData   = (char**)pData;

    szSMSAddress = pszCmdData[0];

    szPDU        = pszCmdData[1];

    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSmsExpectMore() - ERROR: Invalid input(s).\r\n");
        goto Error;
    }

    // 2 chars per byte.
    nPDULength = (strlen(szPDU) / 2);

    if (NULL == szSMSAddress)
    {
        szSMSAddress = szNoAddress;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMMS=1;+CMGS=%u\r", nPDULength))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSmsExpectMore() - ERROR: Cannot create CMGS command\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s%s\x1a", szSMSAddress, szPDU))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendSmsExpectMore() - ERROR: Cannot create CMGS PDU\r\n");
        goto Error;
    }

    RIL_LOG_INFO("Payload: %s\r\n", CRLFExpandedString(rReqData.szCmd2, strlen(rReqData.szCmd2)).GetString());

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSendSmsExpectMore() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSendSmsExpectMore(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSendSmsExpectMore() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    P_ND_SEND_MSG pSendMsg = NULL;
    UINT32          uiMsgRef;

    pSendMsg = (P_ND_SEND_MSG)malloc(sizeof(S_ND_SEND_MSG));
    if (NULL == pSendMsg)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not allocate memory for a S_ND_SEND_MSG struct.\r\n");
        goto Error;
    }
    memset(pSendMsg, 0, sizeof(S_ND_SEND_MSG));

    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not parse response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CMGS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not parse \"+CMGS: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiMsgRef, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not parse <msgRef>.\r\n");
        goto Error;
    }
    else
    {
        pSendMsg->smsRsp.messageRef = (int)uiMsgRef;
    }

    if (!SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUnquotedString(pszRsp, g_cTerminator, pSendMsg->szAckPDU, 160, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not parse <ackPdu>.\r\n");
            goto Error;
        }

        pSendMsg->smsRsp.ackPDU = pSendMsg->szAckPDU;
    }
    else
    {
        pSendMsg->smsRsp.ackPDU = NULL;
    }

    //  Error code is n/a.
    pSendMsg->smsRsp.errorCode = -1;

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSendSmsExpectMore() - ERROR: Could not parse response postfix.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pSendMsg;
    rRspData.uiDataSize  = sizeof(RIL_SMS_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSendMsg);
        pSendMsg = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSendSmsExpectMore() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
//
RIL_RESULT_CODE CTEBase::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    PdpData stPdpData;
    memset(&stPdpData, 0, sizeof(PdpData));

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - uiDataSize=[%d]\r\n", uiDataSize);


    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];  // not used
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];  // not used
    stPdpData.szPassword        = ((char **)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char **)pData)[5];  // not used

    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n", stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n", stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n", stPdpData.szUserName);
    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n", stPdpData.szPassword);
    RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n", stPdpData.szPAPCHAP);


    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType         = ((char **)pData)[6];  // new in Android 2.3.4.
        RIL_LOG_INFO("CTEBase::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n", stPdpData.szPDPType);
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1,
          sizeof(rReqData.szCmd1),
          "AT+CGDCONT=%d,\"IP\",\"%s\",,0,0;+CGQREQ=%d;+CGQMIN=%d;+CGACT=0,%d\r", uiCID,
          stPdpData.szApn, uiCID, uiCID, uiCID))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetupDataCall() - ERROR: Cannot create CGDCONT command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "ATD*99***1#\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetupDataCall() - ERROR: Cannot CopyStringNullTerminate ATD\r\n");
        goto Error;
    }

    //  Store the potential uiCID in the pContext
    rReqData.pContextData = (void*)uiCID;


    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSetupDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTEBase::CoreSimIo(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO_v6 *   pSimIOArgs = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO_v6) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO_v6 *)pData;

#if defined(DEBUG)
    RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - command=%d fileid=%04X path=\"%s\" p1=%d p2=%d p3=%d data=\"%s\" pin2=\"%s\" aidPtr=\"%s\"\r\n",
        pSimIOArgs->command, pSimIOArgs->fileid, pSimIOArgs->path,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data, pSimIOArgs->pin2, pSimIOArgs->aidPtr);
#else
    RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - command=%d fileid=%04X path=\"%s\" p1=%d p2=%d p3=%d data=\"%s\" aidPtr=\"%s\"\r\n",
        pSimIOArgs->command, pSimIOArgs->fileid, pSimIOArgs->path,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data, pSimIOArgs->aidPtr);
#endif

    //  If PIN2 is required, send out AT+CPIN2 request
    if (pSimIOArgs->pin2)
    {
        RIL_LOG_INFO("CTEBase::CoreSimIo() - PIN2 required\r\n");

        if (!PrintStringNullTerminate(rReqData.szCmd1,
                     sizeof(rReqData.szCmd1),
                     "AT+CPIN2=\"%s\"\r",
                     pSimIOArgs->pin2))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CPIN2 command\r\n");
            goto Error;
        }


        if (NULL == pSimIOArgs->data)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd2,
                         sizeof(rReqData.szCmd2),
                         "AT+CRSM=%d,%d,%d,%d,%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 1\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd2,
                         sizeof(rReqData.szCmd2),
                         "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->data))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 2\r\n");
                goto Error;
            }
        }



    }
    else
    {
        //  No PIN2


        if (NULL == pSimIOArgs->data)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 3\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->data))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 4\r\n");
                goto Error;
            }
        }
    }

    //  Set the context of this command to the SIM_IO command-type.
    rReqData.pContextData = (void*)pSimIOArgs->command;


    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimIo() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32  uiSW1 = 0;
    UINT32  uiSW2 = 0;
    char* szResponseString = NULL;
    UINT32  cbResponseString = 0;

    RIL_SIM_IO_Response* pResponse = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CRSM: <sw1>,<sw2>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CRSM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not skip over \"+CRSM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiSW2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not extract SW2 value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTEBase::ParseSimIo() - Extracted SW1 = %u and SW2 = %u\r\n", uiSW1, uiSW2);

    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CTEBase::ParseSimIo() - Extracted data string: \"%s\" (%u chars)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimIo() : ERROR : String was not a multiple of 2.\r\n");
            goto Error;
        }
    }

    // Allocate memory for the response struct PLUS a buffer for the response string
    // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
    // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
    pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
        goto Error;
    }
    memset(pResponse, 0, sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);

    pResponse->sw1 = uiSW1;
    pResponse->sw2 = uiSW2;

    if (NULL == szResponseString)
    {
        pResponse->simResponse = NULL;
    }
    else
    {
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
        if (!CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimIo() - ERROR: Could not CopyStringNullTerminate szResponseString\r\n");
            goto Error;
        }

        // Ensure NULL termination!
        pResponse->simResponse[cbResponseString] = '\0';
    }

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        goto Error;
    }

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(RIL_SIM_IO_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    delete[] szResponseString;
    szResponseString = NULL;

    RIL_LOG_VERBOSE("CTEBase::ParseSimIo() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTEBase::CoreSendUssd(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSendUssd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char *szUssdString = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendUssd() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendUssd() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    // extract data
    szUssdString = (char *)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CUSD=1,\"%s\"\r", szUssdString))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendUssd() - ERROR: cannot create CUSD command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSendUssd() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSendUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSendUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSendUssd() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        res = (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSendUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
RIL_RESULT_CODE CTEBase::CoreCancelUssd(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCancelUssd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CUSD=2\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreCancelUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCancelUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCancelUssd() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseCancelUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CLIR 31
//
RIL_RESULT_CODE CTEBase::CoreGetClir(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetClir() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+CLIR?\r", sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGetClir() - ERROR: Unable to write command to buffer\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGetClir() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetClir() - Enter\r\n");

    int * pCLIRBlob = NULL;

    UINT32 nValue;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    pCLIRBlob = (int *)malloc(sizeof(int) * 2);

    if (NULL == pCLIRBlob)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetClir() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }
    memset(pCLIRBlob, 0, sizeof(int) * 2);

    // Parse "<prefix>+CLIR: <status>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp)          ||
        !SkipString(szRsp, "+CLIR: ", szRsp) ||
        !ExtractUInt32(szRsp, nValue, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetClir() - ERROR: Could not find status value\r\n");
        goto Error;
    }

    pCLIRBlob[0] = nValue;

    // Parse ",<provisioning><postfix>"
    if (!SkipString(szRsp, ",", szRsp)     ||
        !ExtractUInt32(szRsp, nValue, szRsp) ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        goto Error;
    }

    pCLIRBlob[1] = nValue;

    rRspData.pData  = (void*)pCLIRBlob;
    rRspData.uiDataSize = 2 * sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCLIRBlob);
        pCLIRBlob = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CLIR 32
//
RIL_RESULT_CODE CTEBase::CoreSetClir(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetClir() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnClir = NULL;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetClir() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetClir() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pnClir = (int*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLIR=%u\r", pnClir[0]))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetClir() - ERROR: Unable to write command to buffer\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetClir() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetClir() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        res = (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
RIL_RESULT_CODE CTEBase::CoreQueryCallForwardStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryCallForwardStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_CallForwardInfo* pCallFwdInfo = NULL;

    if (sizeof(RIL_CallForwardInfo) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallForwardStatus() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallForwardStatus() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pCallFwdInfo = (RIL_CallForwardInfo*)pData;

    if ((RRIL_INFO_CLASS_NONE == pCallFwdInfo->serviceClass) ||
        ((RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_DATA | RRIL_INFO_CLASS_FAX) == pCallFwdInfo->serviceClass))
    {
        if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                        sizeof(rReqData.szCmd1),
                                        "AT+CCFC=%u,2\r",
                                        pCallFwdInfo->reason))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreQueryCallForwardStatus() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                        sizeof(rReqData.szCmd1),
                                        "AT+CCFC=%u,2,,,%u\r",
                                        pCallFwdInfo->reason,
                                        pCallFwdInfo->serviceClass))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreQueryCallForwardStatus() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallForwardStatus() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    //  Store the reason data for the query, which will be used for preparing response
    rReqData.pContextData = (void*)pCallFwdInfo->reason;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryCallForwardStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryCallForwardStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryCallForwardStatus() - Enter\r\n");

    P_ND_CALLFWD_DATA pCallFwdBlob = NULL;

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 nEntries = 0;
    UINT32 nCur = 0;
    UINT32 nValue;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    while (FindAndSkipString(szRsp, "+CCFC: ", szRsp))
    {
        nEntries++;
    }

    RIL_LOG_INFO("CTEBase::ParseQueryCallForwardStatus() - INFO: Found %d CCFC entries!\r\n", nEntries);

    if (RIL_MAX_CALLFWD_ENTRIES < nEntries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryCallForwardStatus() - ERROR: Too many CCFC entries!\r\n");
        goto Error;
    }

    pCallFwdBlob = (P_ND_CALLFWD_DATA)malloc(sizeof(S_ND_CALLFWD_DATA));

    if (NULL == pCallFwdBlob)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryCallForwardStatus() - ERROR: Could not allocate memory for a S_ND_CALLFWD_DATA struct.\r\n");
        goto Error;
    }
    memset(pCallFwdBlob, 0, sizeof(S_ND_CALLFWD_DATA));

    // Reset our buffer to the beginning of the response
    szRsp = rRspData.szResponse;

    // Parse "+CCFC: "
    while (FindAndSkipString(szRsp, "+CCFC: ", szRsp))
    {
        // Stored reason value is updated when we have some data to send
        pCallFwdBlob->pCallFwdData[nCur].reason = (int)rRspData.pContextData;

        // Parse "<status>"
        if (!ExtractUInt32(szRsp, nValue, szRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseQueryCallForwardStatus() - WARN: Could not find status value, skipping entry\r\n");
            goto Continue;
        }

        pCallFwdBlob->pCallFwdData[nCur].status = nValue;

        // Parse ",<serviceClass>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt32(szRsp, nValue, szRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseQueryCallForwardStatus() - WARN: Could not find service class value, skipping entry\r\n");
            goto Continue;
        }

        pCallFwdBlob->pCallFwdData[nCur].serviceClass = nValue;

        // Parse ","
        if (SkipString(szRsp, ",", szRsp))
        {
            // Parse "<address>,<type>"
            if (!ExtractQuotedString(szRsp, pCallFwdBlob->pCallFwdBuffers[nCur], MAX_BUFFER_SIZE, szRsp) ||
                !SkipString(szRsp, ",", szRsp))
            {
                RIL_LOG_WARNING("CTEBase::ParseQueryCallForwardStatus() - WARN: Could not find address string, skipping entry\r\n");
                goto Continue;
            }

            //  Parse type if available.
            if (ExtractUpperBoundedUInt32(szRsp, 0x100, nValue, szRsp))
            {
                pCallFwdBlob->pCallFwdData[nCur].toa = nValue;
            }
            else
            {
                pCallFwdBlob->pCallFwdData[nCur].toa = 0;
            }

            // Parse ","
            if (SkipString(szRsp, ",", szRsp))
            {
                // No support for subaddress in Android... skipping over
                // Parse "<subaddr>,<subaddr_type>"

                // Parse ","
                if (!FindAndSkipString(szRsp, ",", szRsp))
                {
                    RIL_LOG_WARNING("CTEBase::ParseQueryCallForwardStatus() - WARN: Couldn't find comma after subaddress, skipping entry\r\n");
                    goto Continue;
                }

                // Parse ","
                if (FindAndSkipString(szRsp, ",", szRsp))
                {
                    // Parse "<time>"
                    if (!ExtractUInt32(szRsp, nValue, szRsp))
                    {
                        RIL_LOG_WARNING("CTEBase::ParseQueryCallForwardStatus() - WARN: Couldn't find comma after time, skipping entry\r\n");
                        goto Continue;
                    }

                    pCallFwdBlob->pCallFwdData[nCur].timeSeconds = nValue;
                }
            }
        }

        pCallFwdBlob->pCallFwdData[nCur].number = pCallFwdBlob->pCallFwdBuffers[nCur];
        pCallFwdBlob->pCallFwdPointers[nCur] = &(pCallFwdBlob->pCallFwdData[nCur]);

        // Increment the array index
        nCur++;

Continue:
        if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryCallForwardStatus() - ERROR: Could not find response end\r\n");
            goto Error;
        }
    }

    rRspData.pData  = (void*)pCallFwdBlob;
    rRspData.uiDataSize = nCur * sizeof(RIL_CallForwardInfo*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCallFwdBlob);
        pCallFwdBlob = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryCallForwardStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
RIL_RESULT_CODE CTEBase::CoreSetCallForward(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetCallForward() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 nValue = 0;
    RIL_CallForwardInfo* pCallFwdInfo = NULL;
    char szNumber[255] = {0};

    if (sizeof(RIL_CallForwardInfo) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pCallFwdInfo = (RIL_CallForwardInfo*)pData;

    if (NULL != pCallFwdInfo->number)
    {
        RIL_LOG_INFO("CTEBase::CoreSetCallForward() - status: %d   info class: %d   reason: %d   number: %s   toa: %d   time: %d\r\n",
                        pCallFwdInfo->status,
                        pCallFwdInfo->serviceClass,
                        pCallFwdInfo->reason,
                        pCallFwdInfo->number,
                        pCallFwdInfo->toa,
                        pCallFwdInfo->timeSeconds);
        strncpy(szNumber, pCallFwdInfo->number, 255);
    }
    else
    {
        RIL_LOG_INFO("CTEBase::CoreSetCallForward() - status: %d   info class: %d   reason: %d   number: %s   toa: %d   time: %d\r\n",
                        pCallFwdInfo->status,
                        pCallFwdInfo->serviceClass,
                        pCallFwdInfo->reason,
                        "NULL",
                        pCallFwdInfo->toa,
                        pCallFwdInfo->timeSeconds);
        strncpy(szNumber, "", 255);
    }

    if (pCallFwdInfo->serviceClass)
    {
        if (pCallFwdInfo->timeSeconds)
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CCFC=%u,%u,\"%s\",%u,%u,,,%u\r",
                                            pCallFwdInfo->reason,
                                            pCallFwdInfo->status,
                                            szNumber  /*pCallFwdInfo->number*/,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->serviceClass,
                                            pCallFwdInfo->timeSeconds))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CCFC=%u,%u,\"%s\",%u,%u\r",
                                            pCallFwdInfo->reason,
                                            pCallFwdInfo->status,
                                            szNumber /*pCallFwdInfo->number*/,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->serviceClass))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
    }
    else
    {
        if (pCallFwdInfo->timeSeconds)
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CCFC=%u,%u,\"%s\",%u,,,,%u\r",
                                            pCallFwdInfo->reason,
                                            pCallFwdInfo->status,
                                            szNumber /*pCallFwdInfo->number*/,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->timeSeconds))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CCFC=%u,%u,\"%s\",%u\r",
                                            pCallFwdInfo->reason,
                                            pCallFwdInfo->status,
                                            szNumber /*pCallFwdInfo->number*/ ,
                                            pCallFwdInfo->toa))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallForward() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetCallForward() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetCallForward(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetCallForward() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        res = (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSetCallForward() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
RIL_RESULT_CODE CTEBase::CoreQueryCallWaiting(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryCallWaiting() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnInfoClasses = NULL;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallWaiting() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallWaiting() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pnInfoClasses = (int*)pData;

    if ((RRIL_INFO_CLASS_NONE == pnInfoClasses[0]) ||
        ((RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_DATA | RRIL_INFO_CLASS_FAX) == pnInfoClasses[0]))
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
                        sizeof(rReqData.szCmd1),
                        "AT+CCWA=1,2\r"))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreQueryCallWaiting() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
                        sizeof(rReqData.szCmd1),
                        "AT+CCWA=1,2,%u\r",
                        pnInfoClasses[0]))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreQueryCallWaiting() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryCallWaiting() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryCallWaiting() - Exit\r\n");
    return res;

}

RIL_RESULT_CODE CTEBase::ParseQueryCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * prgnCallWaiting = NULL;
    const char* szRsp = rRspData.szResponse;
    UINT32 nStatus;
    UINT32 nClass;
    UINT32 dwServiceInfo = 0, dwStatus = 0;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    // Parse "+CCWA: "
    while (FindAndSkipString(szRsp, "+CCWA: ", szRsp))
    {
        // Parse "<status>,<class>"
        if (!ExtractUInt32(szRsp, nStatus, szRsp) ||
            !SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt32(szRsp, nClass, szRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseQueryCallWaiting() - WARN: Unable to extract UINTS, skip to next entry\r\n");
            goto Continue;
        }

        RIL_LOG_INFO("CTEBase::ParseQueryCallWaiting() - INFO: Status= %d    Class=%d\r\n", nStatus, nClass);

        if (1 == nStatus)
        {
            dwStatus = 1;

            dwServiceInfo |= nClass;

            RIL_LOG_INFO("CTEBase::ParseQueryCallWaiting() - INFO: Recording service %d. Current mask: %d\r\n", nClass, dwServiceInfo);
        }

Continue:
        // Find "<postfix>"
        if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryCallWaiting() - ERROR: Unable to find response end\r\n");
            goto Error;
        }
    }

    RIL_LOG_INFO("CTEBase::ParseQueryCallWaiting() - INFO: No more +CCWA strings found, building result struct\r\n");

    // If we have an active infoclass, we return 2 ints.
    if (1 == dwStatus)
    {
        RIL_LOG_INFO("CTEBase::ParseQueryCallWaiting() - INFO: Returning 2 ints\r\n");
        rRspData.uiDataSize = 2 * sizeof(int*);

        prgnCallWaiting = (int *)malloc(rRspData.uiDataSize);
        if (!prgnCallWaiting)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryCallWaiting() - ERROR: Could not allocate memory for response.\r\n");
            goto Error;
        }

        prgnCallWaiting[0] = dwStatus;
        prgnCallWaiting[1] = dwServiceInfo;
    }
    else
    {
        RIL_LOG_INFO("CTEBase::ParseQueryCallWaiting() - INFO: Returning 1 int\r\n");
        rRspData.uiDataSize = sizeof(int);

        prgnCallWaiting = (int *)malloc(rRspData.uiDataSize);
        if (!prgnCallWaiting)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryCallWaiting() - ERROR: Could not allocate memory for response.\r\n");
            goto Error;
        }

        prgnCallWaiting[0] = dwStatus;
    }

    rRspData.pData   = (void*)prgnCallWaiting;
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(prgnCallWaiting);
        prgnCallWaiting = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
RIL_RESULT_CODE CTEBase::CoreSetCallWaiting(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetCallWaiting() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nStatus;
    UINT32 nInfoClasses;

    if ((2 * sizeof(int *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallWaiting() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallWaiting() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    nStatus = ((int *)pData)[0];
    nInfoClasses = ((int *)pData)[1];

    if (RRIL_INFO_CLASS_NONE == nInfoClasses)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCWA=1,%u\r", nStatus))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSetCallWaiting() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCWA=1,%u,%u\r", nStatus, nInfoClasses))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSetCallWaiting() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetCallWaiting() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        res = (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSetCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE 37
//
RIL_RESULT_CODE CTEBase::CoreSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSmsAcknowledge() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    BOOL fSuccess = FALSE;
    int * pnAckData = NULL;

    if ((sizeof(int *) != uiDataSize) && ((2 * sizeof(int *)) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSmsAcknowledge() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSmsAcknowledge() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pnAckData = (int *)pData;

    if (1 == pnAckData[0])
    {
        fSuccess = TRUE;
    }

    //  We ack the SMS in silo_sms so just send AT here to get OK response and keep upper layers happy.
    //  Unless the want to send an unsuccessful ACK, then do so.
    if (fSuccess)
    {
        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CNMA=1\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        //if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CNMA=%u\r", fSuccess ? 1 : 2))
        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CNMA=2\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
        }
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSmsAcknowledge() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSmsAcknowledge() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEI 38
//
RIL_RESULT_CODE CTEBase::CoreGetImei(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetImei() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGSN\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreGetImei() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImei(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetImei() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* szRsp = rRspData.szResponse;
    char * szIMEI = (char*)malloc(MAX_PROP_VALUE);
    char szProductImeiProperty[PROPERTY_VALUE_MAX] = {'\0'};

    if (NULL == szIMEI)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImei() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_PROP_VALUE);
        goto Error;
    }
    memset(szIMEI, 0, MAX_PROP_VALUE);

    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImei() - ERROR: Could not find response start\r\n");
        goto Error;
    }

    if (!ExtractUnquotedString(szRsp, g_cTerminator, szIMEI, MAX_PROP_VALUE, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImei() - ERROR: Could not find unquoted string\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTEBase::ParseGetImei() - szIMEI=[%s]\r\n", szIMEI);

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)szIMEI;
    rRspData.uiDataSize  = sizeof(char*);

    // Update IMEI property
    snprintf(szProductImeiProperty, sizeof(szProductImeiProperty), "%s", szIMEI);
    property_set("persist.radio.device.imei", szProductImeiProperty);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szIMEI);
        szIMEI = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetImei() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEISV 39
//
RIL_RESULT_CODE CTEBase::CoreGetImeisv(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetImeisv() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGMR\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreGetImeisv() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImeisv(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetImeisv() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* szRsp = rRspData.szResponse;
    char * szIMEISV = (char*)malloc(MAX_PROP_VALUE);
    char szSV[MAX_BUFFER_SIZE] = {0};
    char szSVDigits[MAX_BUFFER_SIZE] = {0};
    int nIndex = 0;

    if (NULL == szIMEISV)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImeisv() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_PROP_VALUE);
        goto Error;
    }
    memset(szIMEISV, 0, MAX_PROP_VALUE);

    //  Skip over <prefix> if there.
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImeisv() - ERROR: Could not find response start\r\n");
        goto Error;
    }

    //  Skip spaces (if any)
    SkipSpaces(szRsp, szRsp);

    //  Grab SV into szSV
    if (!ExtractUnquotedString(szRsp, g_cTerminator, szSV, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImeisv() - ERROR: Could not find unquoted string szSV\r\n");
        goto Error;
    }

    //  The output of CGMR is "Vx.y,d1d2" where d1d2 are the two digits of SV and x.y integers
    //  Only copy the digits into szSVDigits
    for (UINT32 i=0; i<strlen(szSV) && i<MAX_BUFFER_SIZE; i++)
    {
        if ( ('0' <= szSV[i]) && ('9' >= szSV[i]) )
        {
            szSVDigits[nIndex] = szSV[i];
            nIndex++;
        }
    }

    // Get only the last two digits as SV digits.
    if(nIndex > 2)
    {
        szSVDigits[0] = szSVDigits[nIndex - 2];
        szSVDigits[1] = szSVDigits[nIndex - 1];
    }

    //  Copy 2 digits SV into szIMEISV
    if (!PrintStringNullTerminate(szIMEISV, MAX_PROP_VALUE, "%c%c", szSVDigits[0], szSVDigits[1]))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetImeisv() - ERROR: Could not copy string szIMEISV\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTEBase::ParseGetImeisv() - szIMEISV=[%s]\r\n", szIMEISV);

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)szIMEISV;
    rRspData.uiDataSize  = sizeof(char*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szIMEISV);
        szIMEISV = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetImeisv() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ANSWER 40
//
RIL_RESULT_CODE CTEBase::CoreAnswer(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreAnswer() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "ATA\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreAnswer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseAnswer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseAnswer() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseAnswer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTEBase::CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszCid = NULL;
    int nCid = 0;

    if (uiDataSize < (1 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pszCid = ((char**)pData)[0];
    if (NULL == pszCid || '\0' == pszCid[0])
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDeactivateDataCall() - ERROR: pszCid was NULL\r\n");
        goto Error;
    }

    //  Get CID as int.
    if (sscanf(pszCid, "%d", &nCid) == EOF)
    {
        // Error
        RIL_LOG_CRITICAL("CTEBase::CoreDeactivateDataCall() - ERROR: cannot convert %s to int\r\n", pszCid);
        goto Error;
    }


    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%s;+CGDCONT=%s\r", pszCid, pszCid))
    {
        res = RRIL_RESULT_OK;
    }

    //  Set the context of this command to the CID (for multiple context support).
    rReqData.pContextData = (void*)nCid;  // Store this as an int.


Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
RIL_RESULT_CODE CTEBase::CoreQueryFacilityLock(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryFacilityLock() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszFacility = NULL;
    char * pszPassword = NULL;
    char * pszClass = NULL;

    if ((3 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pszFacility = ((char **)pData)[0];
    pszPassword = ((char **)pData)[1];
    pszClass    = ((char **)pData)[2];

    if (NULL == pszFacility)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Facility string pointer is NULL.\r\n");
        goto Error;
    }

    if (NULL == pszPassword || '\0' == pszPassword[0])
    {
        if (NULL == pszClass || '\0' == pszClass[0])
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CLCK=\"%s\",2\r",
                                            pszFacility))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CLCK=\"%s\",2,,%s\r",
                                            pszFacility,
                                            pszClass))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
    }
    else
    {
        if (NULL == pszClass || '\0' == pszClass[0])
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CLCK=\"%s\",2,\"%s\"\r",
                                            pszFacility,
                                            pszPassword))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                            sizeof(rReqData.szCmd1),
                                            "AT+CLCK=\"%s\",2,\"%s\",%s\r",
                                            pszFacility,
                                            pszPassword,
                                            pszClass))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }
        }
    }

    /*
     * In case of the call barring related lock query failure, query the extended error report.
     * This is to findout whether the query failed due to fdn check or general failure
     */
    if (0 == strcmp(pszFacility, "AO") ||
            0 == strcmp(pszFacility, "OI") ||
            0 == strcmp(pszFacility, "OX") ||
            0 == strcmp(pszFacility, "AI") ||
            0 == strcmp(pszFacility, "IR") ||
            0 == strcmp(pszFacility, "AB") ||
            0 == strcmp(pszFacility, "AG") ||
            0 == strcmp(pszFacility, "AC")) {
        if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreQueryFacilityLock() - ERROR: Cannot create CEER command\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryFacilityLock() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnClass = NULL;
    const char* szRsp = rRspData.szResponse;
    UINT32 dwStatus = 0, dwClass = 0, dwServices = 0;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    pnClass = (int*)malloc(sizeof(int));
    if (NULL == pnClass)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryFacilityLock() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    // Parse "+CLCK: "
    while (FindAndSkipString(szRsp, "+CLCK: ", szRsp))
    {
        // Parse "<status>"
        if (!ExtractUInt32(szRsp, dwStatus, szRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseQueryFacilityLock() - WARN: Unable to extract <status>, skip to next entry\r\n");
            goto Continue;
        }

        //  Optionally parse <class> if there.
        if (SkipString(szRsp, ",", szRsp))
        {
            if(!ExtractUInt32(szRsp, dwClass, szRsp))
            {
                RIL_LOG_WARNING("CTEBase::ParseQueryFacilityLock() - WARN: Unable to extract <class>, skip to next entry\r\n");
                goto Continue;
            }
        }
        else
        {
            //  Assume voice class
            dwClass = 1;
        }

        RIL_LOG_INFO("CTEBase::ParseQueryFacilityLock() - INFO: Status= %d    Class=%d, 0x%02x\r\n", dwStatus, dwClass, dwClass);

        //  If the status was active, add bit to dwServices.
        if (1 == dwStatus)
        {
            dwServices |= dwClass;

            RIL_LOG_INFO("CTEBase::ParseQueryFacilityLock() - INFO: Recording service %d, 0x%02x. Current mask: %d, 0x%02x\r\n",
                dwClass, dwClass, dwServices, dwServices);
        }

Continue:
        // Find "<postfix>"
        if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryFacilityLock() - ERROR: Unable to find response end\r\n");
            goto Error;
        }
    }

    *pnClass = (int)dwServices;
    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pnClass;
    rRspData.uiDataSize  = sizeof(int*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnClass);
        pnClass = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
RIL_RESULT_CODE CTEBase::CoreSetFacilityLock(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetFacilityLock() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char * pszFacility = NULL;
    char * pszMode = NULL;
    char * pszPassword = NULL;
    char * pszClass = NULL;

    if ((4 * sizeof(char *)) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pszFacility = ((char **)pData)[0];
    pszMode     = ((char **)pData)[1];
    pszPassword = ((char **)pData)[2];
    pszClass    = ((char **)pData)[3];

    if ((NULL == pszFacility) || (NULL == pszMode))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Facility or Mode strings were NULL\r\n");
        goto Error;
    }
    // Facility and Mode provided
    else if (NULL == pszPassword || '\0' == pszPassword[0])
    {
        if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                        sizeof(rReqData.szCmd1),
                                        "AT+CLCK=\"%s\",%s;+CCID\r",
                                        pszFacility,
                                        pszMode))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }
    // Password provided
    else if (NULL == pszClass || '\0' == pszClass[0] || '0' == pszClass[0])
    {
        if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                        sizeof(rReqData.szCmd1),
                                        "AT+CLCK=\"%s\",%s,\"%s\";+CCID\r",
                                        pszFacility,
                                        pszMode,
                                        pszPassword))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Unable to write command to buffer\r\n");
            goto Error;
        }
    }
    // Class provided
    else if (!PrintStringNullTerminate( rReqData.szCmd1,
                                        sizeof(rReqData.szCmd1),
                                        "AT+CLCK=\"%s\",%s,\"%s\",%s;+CCID\r",
                                        pszFacility,
                                        pszMode,
                                        pszPassword,
                                        pszClass))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Unable to write command to buffer\r\n");
        goto Error;
    }

    /*
     * In case of the call barring related lock set failure, query the extended error report.
     * This is to findout whether the set failed due to fdn check or general failure.
     */
    if (0 == strcmp(pszFacility, "AO") ||
            0 == strcmp(pszFacility, "OI") ||
            0 == strcmp(pszFacility, "OX") ||
            0 == strcmp(pszFacility, "AI") ||
            0 == strcmp(pszFacility, "IR") ||
            0 == strcmp(pszFacility, "AB") ||
            0 == strcmp(pszFacility, "AG") ||
            0 == strcmp(pszFacility, "AC")) {
        if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSetFacilityLock() - ERROR: Cannot create CEER command\r\n");
            goto Error;
        }
    }

    //  Store PIN
    if (0 == strcmp(pszFacility, "SC"))
    {
        if (0 == strcmp(pszMode, "1"))
        {
            strcpy(m_szPIN, pszPassword);
        }
        else
        {
            strcpy(m_szPIN, "CLR");
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetFacilityLock() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    char szUICCID[MAX_PROP_VALUE] = {0};
    int *pnRetries = NULL;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSetFacilityLock() - ERROR: Could not alloc int\r\n");
        goto Error;
    }

    //  Unknown number of retries remaining
    *pnRetries = (int)-1;

    rRspData.pData    = (void*) pnRetries;
    rRspData.uiDataSize   = sizeof(int*);

    //  Cache PIN1 value
    RIL_LOG_CRITICAL("CTEBase::ParseSetFacilityLock() - PIN code: %s\r\n", m_szPIN);
    if (0 == strcmp(m_szPIN, "CLR"))
    {
        PCache_Clear();
        PCache_SetUseCachedPIN(false);
    }
    else
    {
        // Parse "<prefix>+CCID: <ICCID><postfix>"
        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+CCID: ", pszRsp))
        {
            if (!ExtractUnquotedString(pszRsp, g_cTerminator, szUICCID, MAX_PROP_VALUE, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetFacilityLock() - Cannot parse UICC ID\r\n");
                szUICCID[0] = '\0';
            }

            SkipRspEnd(pszRsp, g_szNewLine, pszRsp);
        }

        PCache_Store_PIN(szUICCID, m_szPIN);
    }
    //  Clear it locally.
    memset(m_szPIN, 0, MAX_PIN_SIZE);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnRetries);
        pnRetries = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSetFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
RIL_RESULT_CODE CTEBase::CoreChangeBarringPassword(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreChangeBarringPassword() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char * pszFacility;
    const char * pszOldPassword;
    const char * pszNewPassword;

    if ((3 * sizeof(char *) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeBarringPassword() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeBarringPassword() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

     pszFacility    = ((const char **)pData)[0];
     pszOldPassword = ((const char **)pData)[1];
     pszNewPassword = ((const char **)pData)[2];

    if (!PrintStringNullTerminate(  rReqData.szCmd1,
                                    sizeof(rReqData.szCmd1),
                                    "AT+CPWD=\"%s\",\"%s\",\"%s\"\r",
                                    pszFacility,
                                    pszOldPassword,
                                    pszNewPassword))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeBarringPassword() - ERROR: Unable to write command to buffer\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreChangeBarringPassword() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreChangeBarringPassword() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeBarringPassword(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseChangeBarringPassword() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        res = (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseChangeBarringPassword() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTEBase::CoreQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryNetworkSelectionMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryNetworkSelectionMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnMode = NULL;
    const char* szRsp = rRspData.szResponse;

    pnMode = (int*)malloc(sizeof(int));
    if (NULL == pnMode)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryNetworkSelectionMode() - ERROR: Could not allocate memory for an int.\r\n");
        goto Error;
    }

    if (!FindAndSkipString(szRsp, "+COPS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryNetworkSelectionMode() - ERROR: Could not find +COPS:\r\n");
        goto Error;
    }

    if (!ExtractUInt32(szRsp, (UINT32&)*pnMode, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryNetworkSelectionMode() - ERROR: Could not extract the mode.\r\n");
        goto Error;
    }

    //  If we have a +COPS: 2, then just default to automatic.
    //if (*pnMode >= 2)
    //{
    //    *pnMode = 0; // automatic
    //}

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pnMode;
    rRspData.uiDataSize  = sizeof(int *);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnMode);
        pnMode = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
RIL_RESULT_CODE CTEBase::CoreSetNetworkSelectionAutomatic(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetNetworkSelectionAutomatic() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=0\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetNetworkSelectionAutomatic() - Enter\r\n");

    m_nNetworkRegistrationType = 0;
    m_szManualMCCMNC[0] = '\0';

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
RIL_RESULT_CODE CTEBase::CoreSetNetworkSelectionManual(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetNetworkSelectionManual() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* pszNumeric = NULL;
    char *pTemp = NULL;

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetNetworkSelectionManual() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetNetworkSelectionManual() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pszNumeric = (char *)pData;

    //  Prepare to copy this to context for parse function
    pTemp = new char[MAX_BUFFER_SIZE];
    if (!pTemp)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetNetworkSelectionManual() - ERROR: Cannot allocate memory\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(pTemp, pszNumeric, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetNetworkSelectionManual() - ERROR: Cannot CopyStringNullTerminate pszNumeric\r\n");
        goto Error;
    }
    rReqData.pContextData = (void*)pTemp;

    //  Send AT command
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=1,2,\"%s\"\r", pszNumeric))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    if (res != RRIL_RESULT_OK)
    {
        delete []pTemp;
        pTemp = NULL;
        rReqData.pContextData = NULL;
    }
    RIL_LOG_VERBOSE("CTEBase::CoreSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetNetworkSelectionManual() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    //  Extract from context
    char *pTemp = (char*)rRspData.pContextData;
    if (!pTemp)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSetNetworkSelectionManual() - ERROR: Cannot extract pContextData\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(m_szManualMCCMNC, pTemp, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSetNetworkSelectionManual() - ERROR: Cannot CopyStringNullTerminate pTemp\r\n");
        goto Error;
    }
    m_nNetworkRegistrationType = 1;

    delete[] pTemp;
    pTemp = NULL;

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTEBase::ParseSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTEBase::CoreQueryAvailableNetworks(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryAvailableNetworks() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryAvailableNetworks() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryAvailableNetworks(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryAvailableNetworks() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nValue;
    UINT32 nEntries = 0;
    UINT32 nCurrent = 0;
    UINT32 numericNameNumber = 0;
    BOOL find = false;
    UINT32 i = 0;
    UINT32 j = 0;
    UINT32 k = 0;

    char tmp[MAX_BUFFER_SIZE];

    P_ND_OPINFO_PTRS pOpInfoPtr = NULL;
    P_ND_OPINFO_DATA pOpInfoData = NULL;

    void * pOpInfoPtrBase = NULL;
    void * pOpInfoDataBase = NULL;

    P_ND_OPINFO_PTRS pOpInfoPtrEnd = NULL;
    P_ND_OPINFO_DATA pOpInfoDataEnd = NULL;

    void * pOpInfoPtrBaseEnd = NULL;
    void * pOpInfoDataBaseEnd = NULL;

    const char* szRsp = rRspData.szResponse;
    const char* szDummy = NULL;

    // Skip "<prefix>+COPS: "
    SkipRspStart(szRsp, g_szNewLine, szRsp);

    if (!FindAndSkipString(szRsp, "+COPS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Unable to find +COPS: in response\r\n");
        goto Error;
    }

    szDummy = szRsp;

    while (FindAndSkipString(szDummy, "(", szDummy))
    {
        nEntries++;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryAvailableNetworks() - DEBUG: Found %d entries. Allocating memory...\r\n", nEntries);

    rRspData.pData = malloc(nEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
    if (NULL == rRspData.pData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Cannot allocate rRspData.pData  size=%d\r\n",
                    nEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)) );
        goto Error;
    }
    memset(rRspData.pData, 0, nEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
    rRspData.uiDataSize = nEntries * sizeof(S_ND_OPINFO_PTRS);

    pOpInfoPtrBase = rRspData.pData;
    pOpInfoDataBase = (void*)((UINT32)rRspData.pData + (nEntries * sizeof(S_ND_OPINFO_PTRS)));

    pOpInfoPtr = (P_ND_OPINFO_PTRS)pOpInfoPtrBase;
    pOpInfoData = (P_ND_OPINFO_DATA)pOpInfoDataBase;

    // Skip "("
    while (SkipString(szRsp, "(", szRsp))
    {
        // Extract "<stat>"
        if (!ExtractUInt32(szRsp, nValue, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Unable to extract status\r\n");
            goto Error;
        }

        switch(nValue)
        {
            case 0:
            {
                const char* szTemp = "unknown";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 1:
            {
                const char* szTemp = "available";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 2:
            {
                const char* szTemp = "current";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 3:
            {
                const char* szTemp = "forbidden";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            default:
            {
                RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Invalid status found: %d\r\n", nValue);
                goto Error;
            }
        }

        // Extract ",<long_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Could not extract the Long Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            int len = strlen(tmp);
            if (0 == len)
            {
                //  if long format operator is empty return string "".
                strcpy(pOpInfoData[nCurrent].szOpInfoLong, "");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoLong[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoLong[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoLong = pOpInfoData[nCurrent].szOpInfoLong;
            RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - Long oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoLong);
        }

        // Extract ",<short_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Could not extract the Short Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            int len = strlen(tmp);
            if (0 == len)
            {
                //  if short format operator is empty return string "".
                strcpy(pOpInfoData[nCurrent].szOpInfoShort, "");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoShort[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoShort[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoShort = pOpInfoData[nCurrent].szOpInfoShort;
            RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - Short oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoShort);
       }

        // Extract ",<num_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Could not extract the Numeric Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            int len = strlen(tmp);
            if (0 == len)
            {
                //  if numeric format operator is empty return string "".
                strcpy(pOpInfoData[nCurrent].szOpInfoNumeric, "");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoNumeric[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoNumeric[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoNumeric = pOpInfoData[nCurrent].szOpInfoNumeric;
            RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - Numeric oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoNumeric);
       }

        // Extract ")"
        if (!FindAndSkipString(szRsp, ")", szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Did not find closing bracket\r\n");
            goto Error;
        }

        // Increment the array index
        nCurrent++;

        // Extract ","
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - INFO: Finished parsing entries\r\n");

            // Suppression of duplication.

            // Detection of the network number
            if (nCurrent > 0)
            {
                for (i=0;i<nCurrent;i++)
                {
                    find = false;
                    for (j=0;j<i;j++)
                    {
                        if (strcmp(pOpInfoData[i].szOpInfoNumeric, pOpInfoData[j].szOpInfoNumeric) == 0)
                        {
                            find = true;
                        }
                    }
                    if (!find)
                    {
                        numericNameNumber++;
                    }
                }
            }

            RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - There is %d network and %d double.\r\n", numericNameNumber, (nCurrent-numericNameNumber));

            if (numericNameNumber < nCurrent)
            {
                RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - a new table is build.\r\n");

                // Declaration of the final table.
                rRspData.pData = malloc(numericNameNumber * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
                if (NULL == rRspData.pData)
                {
                    RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - ERROR: Cannot allocate rRspData.pData  size=%d\r\n",
                                numericNameNumber * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)) );
                    goto Error;
                }
                memset(rRspData.pData, 0, numericNameNumber * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
                rRspData.uiDataSize = numericNameNumber * sizeof(S_ND_OPINFO_PTRS);

                pOpInfoPtrBaseEnd = rRspData.pData;
                pOpInfoDataBaseEnd = (void*)((UINT32)rRspData.pData + (numericNameNumber * sizeof(S_ND_OPINFO_PTRS)));

                pOpInfoPtrEnd = (P_ND_OPINFO_PTRS)pOpInfoPtrBaseEnd;
                pOpInfoDataEnd = (P_ND_OPINFO_DATA)pOpInfoDataBaseEnd;

                // Fill first element of the table.
                strcpy(pOpInfoDataEnd[0].szOpInfoLong, pOpInfoData[0].szOpInfoLong);
                strcpy(pOpInfoDataEnd[0].szOpInfoShort, pOpInfoData[0].szOpInfoShort);
                strcpy(pOpInfoDataEnd[0].szOpInfoNumeric, pOpInfoData[0].szOpInfoNumeric);
                strcpy(pOpInfoDataEnd[0].szOpInfoStatus, pOpInfoData[0].szOpInfoStatus);
                pOpInfoPtrEnd[0].pszOpInfoLong = pOpInfoDataEnd[0].szOpInfoLong;
                pOpInfoPtrEnd[0].pszOpInfoShort = pOpInfoDataEnd[0].szOpInfoShort;
                pOpInfoPtrEnd[0].pszOpInfoNumeric = pOpInfoDataEnd[0].szOpInfoNumeric;
                pOpInfoPtrEnd[0].pszOpInfoStatus = pOpInfoDataEnd[0].szOpInfoStatus;

                // Fill the rest of the table.
                find = false;
                j = 1;
                for (i=1;j<numericNameNumber;i++)
                {
                    for (k=0;k<j;k++)
                    {
                        if (strcmp(pOpInfoDataEnd[k].szOpInfoNumeric, pOpInfoData[i].szOpInfoNumeric) == 0)
                        {
                            find = true;
                        }
                    }
                    if (find == false)
                    {
                        strcpy(pOpInfoDataEnd[j].szOpInfoLong, pOpInfoData[i].szOpInfoLong);
                        strcpy(pOpInfoDataEnd[j].szOpInfoShort, pOpInfoData[i].szOpInfoShort);
                        strcpy(pOpInfoDataEnd[j].szOpInfoNumeric, pOpInfoData[i].szOpInfoNumeric);
                        strcpy(pOpInfoDataEnd[j].szOpInfoStatus, pOpInfoData[i].szOpInfoStatus);
                        pOpInfoPtrEnd[j].pszOpInfoLong = pOpInfoDataEnd[j].szOpInfoLong;
                        pOpInfoPtrEnd[j].pszOpInfoShort = pOpInfoDataEnd[j].szOpInfoShort;
                        pOpInfoPtrEnd[j].pszOpInfoNumeric = pOpInfoDataEnd[j].szOpInfoNumeric;
                        pOpInfoPtrEnd[j].pszOpInfoStatus = pOpInfoDataEnd[j].szOpInfoStatus;
                        j++;
                    }
                    find = false;
                }

                for (i=0;i<numericNameNumber;i++)
                {
                    RIL_LOG_INFO("CTEBase::ParseQueryAvailableNetworks() - INFO: new table : %s\r\n", pOpInfoDataEnd[i].szOpInfoLong);
                }

                // Free the old table.
                free(pOpInfoPtrBase);
            }
            else
            {
                RIL_LOG_CRITICAL("CTEBase::ParseQueryAvailableNetworks() - INFO: a new table is not build.\r\n");
            }

            break;
        }
    }

    // NOTE: there may be more data here, but we don't care about it

    // Find "<postfix>"
    if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(rRspData.pData);
        rRspData.pData   = NULL;
        rRspData.uiDataSize  = 0;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryAvailableNetworks() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_START 49
//
RIL_RESULT_CODE CTEBase::CoreDtmfStart(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDtmfStart() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char cTone;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDtmfStart() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDtmfStart() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    cTone = ((char*)pData)[0];

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XVTS=%c\r", cTone))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDtmfStart() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDtmfStart(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDtmfStart() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDtmfStart() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_STOP 50
//
RIL_RESULT_CODE CTEBase::CoreDtmfStop(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDtmfStop() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XVTS\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDtmfStop() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDtmfStop(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDtmfStop() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDtmfStop() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTEBase::CoreBasebandVersion(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreBasebandVersion() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "at@vers:sw_version()\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseBasebandVersion(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    char szTemp[MAX_BUFFER_SIZE] = {0};
    char* szBasebandVersion = (char*)malloc(MAX_PROP_VALUE);
    if (NULL == szBasebandVersion)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseBasebandVersion() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_PROP_VALUE);
        goto Error;
    }
    memset(szBasebandVersion, 0x00, MAX_PROP_VALUE);

    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseBasebandVersion() - ERROR: Could not find response start\r\n");
        goto Error;
    }

    // There is two NewLine before the sw_version, so do again a SkipRspStart
    SkipRspStart(pszRsp, g_szNewLine, pszRsp);

    if (!ExtractUnquotedString(pszRsp, g_cTerminator, szTemp, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseBasebandVersion() - ERROR: Could not extract the baseband version string.\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(szBasebandVersion, MAX_PROP_VALUE, "%s", szTemp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseBasebandVersion() - ERROR: Could not create szBasebandVersion\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTEBase::ParseBasebandVersion() - szBasebandVersion=[%s]\r\n", szBasebandVersion);

    rRspData.pData   = (void*)szBasebandVersion;
    rRspData.uiDataSize  = sizeof(char*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szBasebandVersion);
        szBasebandVersion = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseBasebandVersion() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
RIL_RESULT_CODE CTEBase::CoreSeparateConnection(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSeparateConnection() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int callId = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSeparateConnection() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    callId = ((int *)pData)[0];

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CHLD=2%u\r", callId))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSeparateConnection() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSeparateConnection(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSeparateConnection() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSeparateConnection() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTEBase::CoreSetMute(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetMute() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nEnable = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetMute() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nEnable = ((int *)pData)[0];

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMUT=%d\r", (nEnable ? 1 : 0) ))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetMute() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_MUTE 54
//
RIL_RESULT_CODE CTEBase::CoreGetMute(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetMute() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CMUT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetMute() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    int * pMuteVal = NULL;
    UINT32 nValue = 0;

    pMuteVal = (int*)malloc(sizeof(int));
    if (!pMuteVal)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetMute() - ERROR : Can't allocate an int.\r\n");
        goto Error;
    }
    memset(pMuteVal, 0, sizeof(int));


    // Parse "<prefix>+CMUT: <enabled><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp)                         ||
        !SkipString(pszRsp, "+CMUT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetMute() - ERROR : Can't parse prefix.\r\n");
        goto Error;
    }

    if (!ExtractUpperBoundedUInt32(pszRsp, 2, nValue, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetMute() - ERROR : Can't parse nValue.\r\n");
        goto Error;
    }
    *pMuteVal = nValue;

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetMute() - ERROR : Can't parse postfix.\r\n");
        goto Error;
    }

    rRspData.pData = (void*)pMuteVal;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pMuteVal);
        pMuteVal = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
RIL_RESULT_CODE CTEBase::CoreQueryClip(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryClip() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+CLIP?\r", sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryClip() - ERROR: Unable to write command to buffer\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreQueryClip() - ERROR: Cannot create CEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreQueryClip() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryClip(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryClip() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    int * pClipVal = NULL;
    UINT32 nValue = 0;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        return (279 == uiCause) ? RRIL_RESULT_FDN_FAILURE : RRIL_RESULT_ERROR;
    }

    pClipVal = (int*)malloc(sizeof(int));
    if (!pClipVal)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryClip() - ERROR : Can't allocate an int.\r\n");
        goto Error;
    }
    memset(pClipVal, 0, sizeof(int));


    // Parse "<prefix>+CLIP: <n>,<value><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp)                         ||
        !SkipString(pszRsp, "+CLIP: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryClip() - ERROR : Can't parse prefix.\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, nValue, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryClip() - ERROR : Can't parse nValue1.\r\n");
        goto Error;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, nValue, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQueryClip() - ERROR : Can't parse nValue2.\r\n");
            goto Error;
        }
    }
    *pClipVal = nValue;

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQueryClip() - ERROR : Can't parse postfix.\r\n");
        goto Error;
    }

    rRspData.pData = (void*)pClipVal;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pClipVal);
        pClipVal = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQueryClip() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
RIL_RESULT_CODE CTEBase::CoreLastDataCallFailCause(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreLastDataCallFailCause() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CEER\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreLastDataCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseLastDataCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseLastDataCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32      uiCause  = 0;
    int*      pCause   = NULL;

    if (!ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseLastDataCallFailCause() - ERROR: Parsing of CEER failed\r\n");
        goto Error;
    }

    pCause= (int*) malloc(sizeof(int));
    if (NULL == pCause)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseLastDataCallFailCause() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    //@TODO: cause code mapping needs to be revisited
    *pCause= (int) uiCause;

    rRspData.pData    = (void*) pCause;
    rRspData.uiDataSize   = sizeof(int*);

    RIL_LOG_INFO("CTEBase::ParseLastDataCallFailCause() - Last call fail cause [%d]\r\n", uiCause);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCause);
        pCause = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseLastDataCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
RIL_RESULT_CODE CTEBase::CoreDataCallList(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDataCallList() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CGACT?;+CGDCONT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDataCallList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDataCallList(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDataCallList() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    P_ND_PDP_CONTEXT_DATA pPDPListData = NULL;
    UINT32 nValue = 0;
    UINT32 nCID = 0;
    int  count = 0;
    int active[MAX_PDP_CONTEXTS] = {0};
    char szPDPType[MAX_BUFFER_SIZE] = {0};
    char szIP[MAX_BUFFER_SIZE] = {0};
    int status[MAX_PDP_CONTEXTS] = {0};
    int suggestedRetryTime[MAX_PDP_CONTEXTS] = {0};
    char szAPN[MAX_BUFFER_SIZE] = {0};
    char szDnses[MAX_BUFFER_SIZE] = {0};
    char szGateways[MAX_BUFFER_SIZE] = {0};

    pPDPListData = (P_ND_PDP_CONTEXT_DATA)malloc(sizeof(S_ND_PDP_CONTEXT_DATA));
    if (NULL == pPDPListData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Could not allocate memory for a P_ND_PDP_CONTEXT_DATA struct.\r\n");
        goto Error;
    }
    memset(pPDPListData, 0, sizeof(S_ND_PDP_CONTEXT_DATA));
    memset(active, 0, sizeof(active));

    // Parse +CGACT response
    while (FindAndSkipString(pszRsp, "+CGACT: ", pszRsp))
    {
        // Parse <cid>
        if (!ExtractUInt32(pszRsp, nCID, pszRsp) ||  ((nCID > MAX_PDP_CONTEXTS) || 0 == nCID ))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Invalid CID.\r\n");
            goto Error;
        }

        // Parse <state>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 2, nValue, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Invalid state.\r\n");
            goto Error;
        }
        active[nCID - 1] = nValue;
        RIL_LOG_INFO("CTEBase::ParseDataCallList() - Context %d %s.\r\n", nCID, (nValue ? "active" : "not active") );
    }

    // Parse +CGDCONT response

    // Parse "+CGDCONT: "
    count = 0;
    while (FindAndSkipString(pszRsp, "+CGDCONT: ", pszRsp))
    {
        CChannel_Data *pChannelData = NULL;

        // Parse <cid>
        if (!ExtractUInt32(pszRsp, nCID, pszRsp) ||  ((nCID > MAX_PDP_CONTEXTS) || 0 == nCID ))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Could not extract CID.\r\n");
            goto Error;
        }
        //  Grab the pChannelData for this CID
        pChannelData = CChannel_Data::GetChnlFromContextID(nCID);

        pPDPListData->pPDPData[count].cid = nCID;

        // set active flag
        pPDPListData->pPDPData[count].active = active[nCID - 1];

        // set status
        pPDPListData->pPDPData[count].status = PDP_FAIL_NONE;

        // set suggestedRetryTime
        pPDPListData->pPDPData[count].suggestedRetryTime = -1;

        // Parse ,<PDP_type>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szPDPType, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Could not extract PDP type.\r\n");
            goto Error;
        }
        strncpy(pPDPListData->pTypeBuffers[count], szPDPType, MAX_BUFFER_SIZE);
        pPDPListData->pPDPData[count].type = pPDPListData->pTypeBuffers[count];

        // Skip over ,<APN>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szAPN, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        //  This is interface name (i.e. rmnet0)
        if (pChannelData && pChannelData->m_szInterfaceName)
        {
            strncpy(pPDPListData->pIfnameBuffers[count], pChannelData->m_szInterfaceName, MAX_BUFFER_SIZE);
            pPDPListData->pPDPData[count].ifname = pPDPListData->pIfnameBuffers[count];
        }
        else
        {
            //  don't fill
            strcpy(pPDPListData->pIfnameBuffers[count], "");
            pPDPListData->pPDPData[count].ifname = pPDPListData->pIfnameBuffers[count];
        }

        // Parse ,<PDP_addr>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szIP, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallList() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        strncpy(pPDPListData->pAddressBuffers[count], szIP, MAX_BUFFER_SIZE);
        pPDPListData->pPDPData[count].addresses = pPDPListData->pAddressBuffers[count];

        //  Populate DNSs and gateways
        if (pChannelData)
        {
            sprintf(pPDPListData->pDnsesBuffers[count], "%s %s %s %s",
                (pChannelData->m_szDNS1 ? pChannelData->m_szDNS1 : ""),
                (pChannelData->m_szDNS2 ? pChannelData->m_szDNS2 : ""),
                (pChannelData->m_szIpV6DNS1 ? pChannelData->m_szIpV6DNS1 : ""),
                (pChannelData->m_szIpV6DNS2 ? pChannelData->m_szIpV6DNS2 : ""));
        }
        else
        {
            strcpy(pPDPListData->pDnsesBuffers[count], "");
        }
        pPDPListData->pPDPData[count].dnses = pPDPListData->pDnsesBuffers[count];

        if (pChannelData)
        {
            strncpy(pPDPListData->pGatewaysBuffers[count],
                (pChannelData->m_szIpGateways ? pChannelData->m_szIpGateways : ""),
                MAX_BUFFER_SIZE);
        }
        else
        {
            strcpy(pPDPListData->pGatewaysBuffers[count], "");
        }
        pPDPListData->pPDPData[count].gateways = pPDPListData->pGatewaysBuffers[count];

        // Parse ,<data_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseDataCallList() - WARNING: Could not extract data comp.\r\n");
            goto Continue;
        }

        // Parse ,<head_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseDataCallList() - WARNING: Could not extract header comp.\r\n");
            goto Continue;
        }

        // following we could have ,<pd1>[,...[,pdN]]
        // but Android does not care about extra parameters
        // so skip them all
Continue:

        // entry complete
        ++count;
    }

    if (count > 0)
    {
        rRspData.pData  = (void*) pPDPListData;
        rRspData.uiDataSize = count * sizeof(RIL_Data_Call_Response_v6);
    }
    else
    {
        free(pPDPListData);
        pPDPListData = NULL;
        rRspData.pData  = NULL;
        rRspData.uiDataSize = 0;
    }
    res = RRIL_RESULT_OK;
    RIL_LOG_INFO("CTEBase::ParseDataCallList() - Parse complete, found [%d] contexts.\r\n", count);

    if (CRilLog::IsFullLogBuild())
    {
        for (int i = 0; i < count; ++i)
        {
            RIL_LOG_INFO("i=%d  status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\" addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
                i, pPDPListData->pPDPData[i].status, pPDPListData->pPDPData[i].suggestedRetryTime,
                pPDPListData->pPDPData[i].cid, pPDPListData->pPDPData[i].active,
                pPDPListData->pPDPData[i].type, pPDPListData->pPDPData[i].ifname,
                pPDPListData->pPDPData[i].addresses, pPDPListData->pPDPData[i].dnses,
                pPDPListData->pPDPData[i].gateways);
        }
    }

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pPDPListData);
        pPDPListData = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseDataCallList() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RESET_RADIO 58
//
RIL_RESULT_CODE CTEBase::CoreResetRadio(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreResetRadio() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreResetRadio() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseResetRadio(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseResetRadio() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseResetRadio() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTEBase::CoreHookRaw(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHookRaw() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreHookRaw() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHookRaw(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHookRaw() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseHookRaw() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
RIL_RESULT_CODE CTEBase::CoreHookStrings(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHookStrings() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHookStrings(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHookStrings() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseHookStrings() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTEBase::CoreScreenState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreScreenState() - Enter\r\n");

    // TODO : Mark flags for unsol updates for LAC, CID, CSQ to not send updates
    //        when screen is off. Resume updates when screen turns back on.
    //        will probably need to trigger immediate updates on resume to have correct
    //        info on screen. For now, just return OK and keep throwing updates.
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nEnable = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreScreenState() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nEnable = ((int *)pData)[0];

    //  Store setting in context.
    rReqData.pContextData = (void*)nEnable;

    if (CopyStringNullTerminate(rReqData.szCmd1, (1 == nEnable) ?
                                "AT+CREG=2;+XREG=2;+XCSQ=1;+CGEREP=1,0\r" :
                                "AT+CREG=0;+XREG=0;+XCSQ=0;+CGEREP=0,0\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreScreenState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseScreenState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseScreenState() - Enter\r\n");

    //  Extract screen state from context
    int nScreenState = (int)rRspData.pContextData;

    if (1 == nScreenState)
    {
        /*
         * This will result in quite a few traffic between AP and BP when the screen
         * state is changed frequently.
         */
        triggerSignalStrength(NULL);
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
        triggerDataCallListChanged(NULL);
    }
    else
    {
        CTE::GetTE().ResetRegistrationCache();
    }

    RIL_LOG_VERBOSE("CTEBase::ParseScreenState() - Exit\r\n");
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
RIL_RESULT_CODE CTEBase::CoreSetSuppSvcNotification(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetSuppSvcNotification() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetSuppSvcNotification() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetSuppSvcNotification() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }


    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSSN=%u,%u\r", ((int *)pData)[0], ((int *)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetSuppSvcNotification() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetSuppSvcNotification(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetSuppSvcNotification() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSetSuppSvcNotification() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
RIL_RESULT_CODE CTEBase::CoreWriteSmsToSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreWriteSmsToSim() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SMS_WriteArgs* pSmsArgs;
    UINT32 nPDULength = 0;
    UINT32 nSMSCLen = 0;

    if (sizeof(RIL_SMS_WriteArgs) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreWriteSmsToSim() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreWriteSmsToSim() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    pSmsArgs = (RIL_SMS_WriteArgs*)pData;

    // 2 chars per byte.
    nPDULength = (strlen(pSmsArgs->pdu) / 2);

    // Calculate SMSC phone number length from the first 2 char of pdu.
    // This is to calculate actual TPDU length in AT+CMGW command.

    //  Get SMSC number length from first byte of PDU
    nSMSCLen = (UINT8)SemiByteCharsToByte(pSmsArgs->pdu[0], pSmsArgs->pdu[1]);

    RIL_LOG_INFO("CTEBase::CoreWriteSmsToSim() - nSMSCLen = %d\r\n", nSMSCLen);

    // Calculate actual/expected length in AT+CMGW command. Have to be the TPDU length.
    if ((NULL == pSmsArgs->smsc) && (nSMSCLen == 0))
    {
        nPDULength--;
    }
    else if ((NULL == pSmsArgs->smsc) && (nSMSCLen != 0))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreWriteSmsToSim() - ERROR: SMSC data pointer was NULL but SMSC length is not\r\n");
        goto Error;
    }
    else if ((NULL != pSmsArgs->smsc) && (nSMSCLen != 0))
    {
        nPDULength -= (nSMSCLen + 1);
    }
    else
    {
        RIL_LOG_CRITICAL("CTEBase::CoreWriteSmsToSim() - ERROR: Error parsing smsc length\r\n");
        goto Error;
    }

    if ((PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGW=%u,%u\r", nPDULength, pSmsArgs->status)) &&
        (PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s\x1a", pSmsArgs->pdu)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreWriteSmsToSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseWriteSmsToSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseWriteSmsToSim() - Enter\r\n");

    int * pIndex = NULL;
    const char* szRsp = rRspData.szResponse;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    pIndex = (int*)malloc(sizeof(int));
    if (NULL == pIndex)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseWriteSmsToSim() - ERROR: Unable to allocate memory for int\r\n");
        goto Error;
    }

    if (!FindAndSkipString(szRsp, "+CMGW: ", szRsp) ||
        !ExtractUInt32(szRsp, (UINT32&)*pIndex, szRsp)  ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseWriteSmsToSim() - ERROR: Could not extract the Message Index.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pIndex;
    rRspData.uiDataSize  = sizeof(int*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pIndex);
        pIndex = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseWriteSmsToSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
RIL_RESULT_CODE CTEBase::CoreDeleteSmsOnSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDeleteSmsOnSim() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDeleteSmsOnSim() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDeleteSmsOnSim() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGD=%u\r", ((int*)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDeleteSmsOnSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDeleteSmsOnSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDeleteSmsOnSim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseDeleteSmsOnSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTEBase::CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetBandMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetBandMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseSetBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTEBase::CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryAvailableBandMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryAvailableBandMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTEBase::CoreStkGetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreStkGetProfile() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseStkGetProfile() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseStkGetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTEBase::CoreStkSetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreStkSetProfile() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseStkSetProfile() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseStkSetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTEBase::CoreStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreStkSendEnvelopeCommand() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseStkSendEnvelopeCommand() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTEBase::CoreStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreStkSendTerminalResponse() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseStkSendTerminalResponse() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseStkSendTerminalResponse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTEBase::CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
RIL_RESULT_CODE CTEBase::CoreExplicitCallTransfer(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreExplicitCallTransfer() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=4\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTEBase::CoreExplicitCallTransfer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseExplicitCallTransfer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseExplicitCallTransfer() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseExplicitCallTransfer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTEBase::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetPreferredNetworkType() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetPreferredNetworkType() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseSetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTEBase::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetPreferredNetworkType() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetPreferredNetworkType() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTEBase::CoreGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetNeighboringCellIDs() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetNeighboringCellIDs() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION 77
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetSubscription(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetSubscription() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetSubscription() - Enter\r\n");

    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetSubscription() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetRoamingPreference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetRoamingPreference() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
RIL_RESULT_CODE CTEBase::CoreCdmaQueryRoamingPreference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaQueryRoamingPreference() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaQueryRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTEBase::CoreSetTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetTtyMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreSetTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetTtyMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseSetTtyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTEBase::CoreQueryTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreQueryTtyMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreQueryTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQueryTtyMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_inf_6260.cpp
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseQueryTtyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetPreferredVoicePrivacyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetPreferredVoicePrivacyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
RIL_RESULT_CODE CTEBase::CoreCdmaQueryPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaQueryPreferredVoicePrivacyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaQueryPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaQueryPreferredVoicePrivacyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaQueryPreferredVoicePrivacyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
RIL_RESULT_CODE CTEBase::CoreCdmaFlash(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaFlash() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaFlash() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaFlash(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaFlash() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaFlash() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
RIL_RESULT_CODE CTEBase::CoreCdmaBurstDtmf(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaBurstDtmf() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaBurstDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaBurstDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaBurstDtmf() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaBurstDtmf() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
RIL_RESULT_CODE CTEBase::CoreCdmaValidateAndWriteAkey(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaValidateAndWriteAkey() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaValidateAndWriteAkey() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaValidateAndWriteAkey() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaValidateAndWriteAkey() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
RIL_RESULT_CODE CTEBase::CoreCdmaSendSms(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSendSms() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSendSms() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSendSms() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSendSms() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
RIL_RESULT_CODE CTEBase::CoreCdmaSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSmsAcknowledge() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSmsAcknowledge() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSmsAcknowledge() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
RIL_RESULT_CODE CTEBase::CoreGsmGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGsmGetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGsmGetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nSelected = 0;
    char szChannels[MAX_BUFFER_SIZE] = {0};
    char szLangs[MAX_BUFFER_SIZE] = {0};
    const char* szRsp = rRspData.szResponse;
    const char* pszChannels = NULL;
    const char* pszLangs = NULL;
    P_ND_BROADCASTSMSCONFIGINFO_DATA pBroadcastSmsConfigInfoBlob = NULL;
    UINT32 nCount = 0;
    UINT32 nStructsChannels = 0, nStructsLangs = 0;

    pBroadcastSmsConfigInfoBlob = (P_ND_BROADCASTSMSCONFIGINFO_DATA)malloc(sizeof(S_ND_BROADCASTSMSCONFIGINFO_DATA));
    if (NULL == pBroadcastSmsConfigInfoBlob)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGsmGetBroadcastSmsConfig() - ERROR: Could not allocate memory for pBroadcastSmsConfigInfoBlob.\r\n");
        goto Error;
    }
    //  Set bytes to 0xFF because 0 values mean something in this structure.
    memset(pBroadcastSmsConfigInfoBlob, 0xFF, sizeof(S_ND_BROADCASTSMSCONFIGINFO_DATA));

    // Parse "<prefix>+CSCB: <mode>,<mids>,<dcss><postfix>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp) ||
        !SkipString(szRsp, "+CSCB: ", szRsp) ||
        !ExtractUInt32(szRsp, nSelected, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedString(szRsp, szChannels, MAX_BUFFER_SIZE, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedString(szRsp, szLangs, MAX_BUFFER_SIZE, szRsp) ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGsmGetBroadcastSmsConfig() - ERROR: Could not extract data.\r\n");
        goto Error;
    }

    //  Determine number of structures needed to hold channels and languages.
    //  This is equal to max(nStructsChannels, nStructsLangs).

    //  Count ',' in szChannels, then add one.
    for (UINT32 i=0; i < strlen(szChannels); i++)
    {
        if (',' == szChannels[i])
        {
            nStructsChannels++;
        }
    }
    nStructsChannels++;

    //  Count ',' in szLangs, then add one.
    for (UINT32 i=0; i < strlen(szLangs); i++)
    {
        if (',' == szLangs[i])
        {
            nStructsLangs++;
        }
    }
    nStructsLangs++;

    nCount = ((nStructsChannels > nStructsLangs) ? nStructsChannels : nStructsLangs);
    if (nCount > RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES)
    {
        //  Too many.  Error out.
        RIL_LOG_CRITICAL("CTEBase::ParseGsmGetBroadcastSmsConfig() - ERROR: Too many structs needed!  nCount=%d.\r\n", nCount);
        goto Error;
    }

    //  Parse through szChannels.
    pszChannels = szChannels;
    for (UINT32 i = 0; (i < nStructsChannels) && (i < RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES); i++)
    {
        UINT32 nValue1, nValue2;
        if (!ExtractUInt32(pszChannels, nValue1, pszChannels))
        {
            //  Use -1 as no channels.
            nValue1 = -1;
            nValue2 = -1;
        }
        else
        {
            if (SkipString(pszChannels, "-", pszChannels))
            {
                //  It is a range.
                if (!ExtractUInt32(pszChannels, nValue2, pszChannels))
                {
                    //  Nothing after the "-" is an error.
                    RIL_LOG_CRITICAL("CTEBase::ParseGsmGetBroadcastSmsConfig() - ERROR: Parsing szChannels range. nStructsChannels=%d i=%d\r\n", nStructsChannels, i);
                    goto Error;
                }
            }
            else
            {
                //  Not a range.
                nValue2 = nValue1;
            }
        }

        pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i].fromServiceId = nValue1;
        pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i].toServiceId = nValue2;
        pBroadcastSmsConfigInfoBlob->pBroadcastSmsConfigInfoPointers[i] = &(pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i]);

        //  Parse the next ",".
        if (!SkipString(pszChannels, ",", pszChannels))
        {
            //  We are done.
            break;
        }
    }

    //  Parse through szLangs.
    pszLangs = szLangs;
    for (UINT32 i = 0; (i < nStructsLangs) && (i < RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES); i++)
    {
        UINT32 nValue1, nValue2;
        if (!ExtractUInt32(pszLangs, nValue1, pszLangs))
        {
            //  Use -1 as error for now.
            nValue1 = -1;
            nValue2 = -1;
        }
        else
        {
            if (SkipString(pszLangs, "-", pszLangs))
            {
                //  It is a range.
                if (!ExtractUInt32(pszLangs, nValue2, pszLangs))
                {
                    //  Nothing after the "-" is an error.
                    RIL_LOG_CRITICAL("CTEBase::ParseGsmGetBroadcastSmsConfig() - ERROR: Parsing szLangs range. nStructsLangs=%d i=%d\r\n", nStructsLangs, i);
                    goto Error;
                }
            }
            else
            {
                //  Not a range.
                nValue2 = nValue1;
            }
        }

        pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i].fromCodeScheme = nValue1;
        pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i].toCodeScheme = nValue2;
        pBroadcastSmsConfigInfoBlob->pBroadcastSmsConfigInfoPointers[i] = &(pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i]);

        //  Parse the next ",".
        if (!SkipString(pszChannels, ",", pszChannels))
        {
            //  We are done.
            break;
        }
    }

    //  Loop through each struct, populate "selected".
    for (UINT32 i=0; (i < nCount) && (i < RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES); i++)
    {
        pBroadcastSmsConfigInfoBlob->BroadcastSmsConfigInfoData[i].selected = (unsigned char)nSelected;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pBroadcastSmsConfigInfoBlob;
    rRspData.uiDataSize  = nCount * sizeof(RIL_GSM_BroadcastSmsConfigInfo*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pBroadcastSmsConfigInfoBlob);
        pBroadcastSmsConfigInfoBlob = NULL;
    }


    RIL_LOG_VERBOSE("CTEBase::ParseGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
RIL_RESULT_CODE CTEBase::CoreGsmSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    int nConfigInfos = 0;

    RIL_LOG_VERBOSE("CTEBase::CoreGsmSetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    RIL_GSM_BroadcastSmsConfigInfo** ppConfigInfo = (RIL_GSM_BroadcastSmsConfigInfo**)pData;
    m_vBroadcastSmsConfigInfo.clear();

    if ( (0 == uiDataSize) || (0 != (uiDataSize % sizeof(RIL_GSM_BroadcastSmsConfigInfo *))) )
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGsmSetBroadcastSmsConfig() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGsmSetBroadcastSmsConfig() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    nConfigInfos = uiDataSize / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);
    RIL_LOG_INFO("CTEBase::CoreGsmSetBroadcastSmsConfig() - nConfigInfos = %d.\r\n", nConfigInfos);

    for (int i=0; i<nConfigInfos; i++)
    {
        m_vBroadcastSmsConfigInfo.push_back(*(ppConfigInfo[i]));
    }

    if (m_vBroadcastSmsConfigInfo.empty())
    {
        RIL_LOG_CRITICAL("CTEBase::CoreGsmSetBroadcastSmsConfig() - m_vBroadcastSmsConfigInfo empty.\r\n");
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGsmSetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//
RIL_RESULT_CODE CTEBase::CoreGsmSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGsmSmsBroadcastActivation() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (m_vBroadcastSmsConfigInfo.empty())
    {
        int fBcActivate = 0;

        if (sizeof(int*) != uiDataSize)
        {
            RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
            goto Error;
        }

        if (NULL == pData)
        {
            RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Passed data pointer was NULL\r\n");
            goto Error;
        }

        fBcActivate = ((int*)pData)[0];
        RIL_LOG_INFO("CTEBase::CoreGsmSmsBroadcastActivation() - fBcActivate=[%u]\r\n", fBcActivate);

        //  According to ril.h, 0 = activate, 1 = disable
        if (0 == fBcActivate)
        {
            //  activate
            // This command activates all channels with all code schemes.
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CSCB=1\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
        }
        else
        {
            //  disable
            // This command deactivates all channels with all code schemes.
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CSCB=0\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
        }
    }
    else
    {
        char szChannels[MAX_BUFFER_SIZE] = {0};
        char szLangs[MAX_BUFFER_SIZE] = {0};
        char szChannelsInt[MAX_BUFFER_SIZE] = {0};
        char szLangsInt[MAX_BUFFER_SIZE] = {0};
        int isSelected = 1;
        int nChannelToAccept = 0;
        int fromServiceIdMem = 0xFFFF;
        int toServiceIdMem = 0xFFFF;
        int fromCodeSchemeMem = 0xFFFF;
        int toCodeSchemeMem = 0xFFFF;
        RIL_GSM_BroadcastSmsConfigInfo _tConfigInfo;
        int i;

        RIL_LOG_INFO("CTEBase::CoreGsmSmsBroadcastActivation() - m_vBroadcastSmsConfigInfo.size() = %d.\r\n", m_vBroadcastSmsConfigInfo.size());

        //  Loop through each RIL_GSM_BroadcastSmsConfigInfo structure.
        //  Make string szChannels our list of channels to add.
        for (i = 0; i < (int)m_vBroadcastSmsConfigInfo.size(); i++)
        {
            _tConfigInfo = m_vBroadcastSmsConfigInfo[i];

            // Default Value. If no channel selected the mode must at the not accepted value (1).
            if (_tConfigInfo.selected == true)
            {
                isSelected = 0;
                nChannelToAccept++;
            }
        }

        RIL_LOG_INFO("CTEBase::CoreGsmSmsBroadcastActivation() - nChannelToAccept = %d.\r\n", nChannelToAccept);


        if (nChannelToAccept > 0)
        {
            //  Loop through each RIL_GSM_BroadcastSmsConfigInfo structure.
            //  Make string szChannels our list of channels to add.
            nChannelToAccept = 0;
            for (i = 0; i < (int)m_vBroadcastSmsConfigInfo.size(); i++)
            {
                _tConfigInfo = m_vBroadcastSmsConfigInfo[i];

                if (_tConfigInfo.selected == true)
                {

                    if (nChannelToAccept == 0)
                    {
                        if (_tConfigInfo.fromServiceId == _tConfigInfo.toServiceId)
                        {
                            if (!PrintStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), "%u", _tConfigInfo.fromServiceId))
                            {
                                RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                goto Error;
                            }
                        }
                        else
                        {
                            if (!PrintStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), "%u-%u", _tConfigInfo.fromServiceId, _tConfigInfo.toServiceId))
                            {
                                RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print to service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                goto Error;
                            }
                        }
                        nChannelToAccept++;
                    }
                    else
                    {
                        if ((fromServiceIdMem != _tConfigInfo.fromServiceId) && (toServiceIdMem != _tConfigInfo.toServiceId))
                        {
                            if (_tConfigInfo.fromServiceId == _tConfigInfo.toServiceId)
                            {
                                if (!PrintStringNullTerminate(szChannelsInt, MAX_BUFFER_SIZE - strlen(szChannelsInt), ",%u", _tConfigInfo.fromServiceId))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                                if (!ConcatenateStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), szChannelsInt))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                            }
                            else
                            {
                                if (!PrintStringNullTerminate(szChannelsInt, MAX_BUFFER_SIZE - strlen(szChannelsInt), ",%u-%u", _tConfigInfo.fromServiceId, _tConfigInfo.toServiceId))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print to service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                                if (!ConcatenateStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), szChannelsInt))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print to service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                            }
                            nChannelToAccept++;
                        }
                        fromServiceIdMem = _tConfigInfo.fromServiceId;
                        toServiceIdMem = _tConfigInfo.toServiceId;
                    }
                }
            }

            //  Loop through each RIL_GSM_BroadcastSmsConfigInfo structure.
            //  Make string szLangs our list of languages to add.
            nChannelToAccept = 0;
            for (i = 0; i < (int)m_vBroadcastSmsConfigInfo.size(); i++)
            {
                _tConfigInfo = m_vBroadcastSmsConfigInfo[i];

                if (_tConfigInfo.selected == true)
                {

                    if (nChannelToAccept == 0)
                    {
                        if (_tConfigInfo.fromCodeScheme == _tConfigInfo.toCodeScheme)
                        {
                            if (!PrintStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szLangs), "%u", _tConfigInfo.fromCodeScheme))
                            {
                                RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                goto Error;
                            }
                        }
                        else
                        {
                            if (!PrintStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szLangs), "%u-%u", _tConfigInfo.fromCodeScheme, _tConfigInfo.toCodeScheme))
                            {
                                RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from from-to code scheme of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                goto Error;
                            }
                        }
                        nChannelToAccept++;
                    }
                    else
                    {
                        if ((fromCodeSchemeMem != _tConfigInfo.fromCodeScheme) && (toCodeSchemeMem != _tConfigInfo.toCodeScheme))
                        {
                            if (_tConfigInfo.fromCodeScheme == _tConfigInfo.toCodeScheme)
                            {
                                if (!PrintStringNullTerminate(szLangsInt, MAX_BUFFER_SIZE - strlen(szLangsInt), ",%u", _tConfigInfo.fromCodeScheme))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                                if (!ConcatenateStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szChannels), szLangsInt))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                            }
                            else
                            {
                                if (!PrintStringNullTerminate(szLangsInt, MAX_BUFFER_SIZE - strlen(szLangsInt), ",%u-%u", _tConfigInfo.fromCodeScheme, _tConfigInfo.toCodeScheme))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from from-to code scheme of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                                if (!ConcatenateStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szChannels), szLangsInt))
                                {
                                    RIL_LOG_CRITICAL("CTEBase::CoreGsmSmsBroadcastActivation() - ERROR: Unable to print from service id of m_vBroadcastSmsConfigInfo[%d]\r\n", i);
                                    goto Error;
                                }
                            }
                            nChannelToAccept++;
                        }
                    }
                    fromCodeSchemeMem = _tConfigInfo.fromCodeScheme;
                    toCodeSchemeMem = _tConfigInfo.toCodeScheme;
                }
            }

            //  Make the final string.
            if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB=%u,\"%s\",\"%s\"\r", isSelected, szChannels, szLangs))
            {
                res = RRIL_RESULT_OK;
            }

        }
        else
        {
            //  Make the final string.
            if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB=0,\"\",\"\"\r"))
            {
                res = RRIL_RESULT_OK;
            }
        }
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGsmSmsBroadcastActivation() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
RIL_RESULT_CODE CTEBase::CoreCdmaGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaGetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaGetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
RIL_RESULT_CODE CTEBase::CoreCdmaSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSmsBroadcastActivation() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSmsBroadcastActivation() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSmsBroadcastActivation() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
RIL_RESULT_CODE CTEBase::CoreCdmaSubscription(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSubscription() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSubscription() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaSubscription() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
RIL_RESULT_CODE CTEBase::CoreCdmaWriteSmsToRuim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaWriteSmsToRuim() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaWriteSmsToRuim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaWriteSmsToRuim() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaWriteSmsToRuim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
RIL_RESULT_CODE CTEBase::CoreCdmaDeleteSmsOnRuim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreCdmaDeleteSmsOnRuim() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreCdmaDeleteSmsOnRuim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseCdmaDeleteSmsOnRuim() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseCdmaDeleteSmsOnRuim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
RIL_RESULT_CODE CTEBase::CoreDeviceIdentity(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDeviceIdentity() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreDeviceIdentity() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDeviceIdentity(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDeviceIdentity() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseDeviceIdentity() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
RIL_RESULT_CODE CTEBase::CoreExitEmergencyCallbackMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreExitEmergencyCallbackMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::CoreExitEmergencyCallbackMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseExitEmergencyCallbackMode() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;

    RIL_LOG_VERBOSE("CTEBase::ParseExitEmergencyCallbackMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
RIL_RESULT_CODE CTEBase::CoreGetSmscAddress(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreGetSmscAddress() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCA?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreGetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseGetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;

    char* szSCAddr = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == szSCAddr)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetSMSCAddress() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }
    memset(szSCAddr, 0, MAX_BUFFER_SIZE);

    // Parse "<prefix><sca>,<tosca><postfix>"
    //  We can ignore the , and <tosca>.
    SkipRspStart(szRsp, g_szNewLine, szRsp);

    if (!FindAndSkipString(szRsp, "+CSCA: ", szRsp) ||
        !ExtractQuotedString(szRsp, szSCAddr, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseGetSMSCAddress() - ERROR: Could not extract the SMS Service Center address string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)szSCAddr;
    rRspData.uiDataSize  = sizeof(char*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szSCAddr);
        szSCAddr = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseGetSMSCAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
RIL_RESULT_CODE CTEBase::CoreSetSmscAddress(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSetSmscAddress() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char *szSMSC = NULL;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetSmscAddress() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSetSmscAddress() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    szSMSC = (char*)pData;

    //  Modem works best with addr type of 145.  Need '+' in front.
    //  I noticed CMS ERROR: 96 when setting SMSC with addr type 129.
    if ('+' == szSMSC[0])
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCA=\"%s\",145\r", szSMSC))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCA=\"+%s\",145\r", szSMSC))
        {
            res = RRIL_RESULT_OK;
        }
    }




Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTEBase::ParseSetSmscAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTEBase::CoreReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreReportSmsMemoryStatus() - Enter / Exit\r\n");
    // this is modem dependent, to be implemented in te_inf_6260.cpp
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

RIL_RESULT_CODE CTEBase::ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseReportSmsMemoryStatus() - Enter / Exit\r\n");
    // this is modem dependent, to be implemented in te_inf_6260.cpp
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTEBase::CoreReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreReportStkServiceRunning() - Enter / Exit\r\n");
    // this is modem dependent, to be implemented in te_inf_6260.cpp
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

RIL_RESULT_CODE CTEBase::ParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseReportStkServiceRunning() - Enter / Exit\r\n");
    // this is modem dependent, to be implemented in te_inf_6260.cpp
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 104
//
RIL_RESULT_CODE CTEBase::CoreSimTransmitBasic(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitBasic() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO_v6 *   pSimIOArgs = NULL;
    // ETSI TS 102 221 Section "Coding of Class Byte"
    int classByte = (RIL_APPTYPE_USIM == m_nSimAppType) ? 0x80 : 0xA0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitBasic() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO_v6) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitBasic() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO_v6 *)pData;

    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitBasic() - cla=%02X command=%d p1=%d p2=%d p3=%d data=\"%s\"\r\n",
        classByte, pSimIOArgs->command,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data);

    if ((NULL == pSimIOArgs->data) || (strlen(pSimIOArgs->data) == 0))
    {
        if (pSimIOArgs->p3 < 0)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CSIM=%d,\"%02x%02x%02x%02x\"\r",
                    8,
                    classByte,
                    pSimIOArgs->command,
                    pSimIOArgs->p1,
                    pSimIOArgs->p2))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitBasic() - ERROR: cannot create CSIM command 1\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CSIM=%d,\"%02x%02x%02x%02x%02x\"\r",
                    10,
                    classByte,
                    pSimIOArgs->command,
                    pSimIOArgs->p1,
                    pSimIOArgs->p2,
                    pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitBasic() - ERROR: cannot create CSIM command 2\r\n");
                goto Error;
            }
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CSIM=%d,\"%02x%02x%02x%02x%02x%s\"\r",
                10 + strlen(pSimIOArgs->data),
                classByte,
                pSimIOArgs->command,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitBasic() - ERROR: cannot create CSIM command 3\r\n");
            goto Error;
        }
    }


    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitBasic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSimTransmitBasic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimTransmitBasic() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32  uiSW1 = 0;
    UINT32  uiSW2 = 0;
    UINT32  uiLen = 0;
    char* szResponseString = NULL;
    UINT32  cbResponseString = 0;

    RIL_SIM_IO_Response* pResponse = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CSIM: <len>,"<response>"<postfix>"
    SkipRspStart(pszRsp, g_szNewLine, pszRsp);


    if (!SkipString(pszRsp, "+CSIM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Could not skip over \"+CSIM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiLen, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Could not extract uiLen value.\r\n");
        goto Error;
    }


    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CTEBase::ParseSimTransmitBasic() - Extracted data string: \"%s\" (%u chars)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() : ERROR : String was not a multiple of 2.\r\n");
            goto Error;
        }
    }

    // Allocate memory for the response struct PLUS a buffer for the response string
    // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
    // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
    pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
        goto Error;
    }
    memset(pResponse, 0, sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);

    //  Response must be 4 chars or longer - cbResponseString includes NULL character
    if (cbResponseString < 5)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: response length must be 4 or greater.\r\n");
        goto Error;
    }

    sscanf(&szResponseString[cbResponseString-5], "%02x%02x", &uiSW1, &uiSW2);
    szResponseString[cbResponseString-5] = '\0';

    pResponse->sw1 = uiSW1;
    pResponse->sw2 = uiSW2;

    if (NULL == szResponseString)
    {
        pResponse->simResponse = NULL;
    }
    else
    {
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
        if (!CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitBasic() - ERROR: Cannot CopyStringNullTerminate szResponseString\r\n");
            goto Error;
        }

        // Ensure NULL termination!
        pResponse->simResponse[cbResponseString] = '\0';
    }

    // Parse "<postfix>"
    SkipRspEnd(pszRsp, g_szNewLine, pszRsp);

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(RIL_SIM_IO_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    delete[] szResponseString;
    szResponseString = NULL;

    RIL_LOG_VERBOSE("CTEBase::ParseSimTransmitBasic() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_OPEN_CHANNEL 105
//
RIL_RESULT_CODE CTEBase::CoreSimOpenChannel(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSimOpenChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char *szAID = NULL;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimOpenChannel() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimOpenChannel() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    szAID = (char*)pData;


    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCHO=\"%s\"\r", szAID))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimOpenChannel() - ERROR: Cannot create CCHO command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimOpenChannel() - ERROR: Cannot create XEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSimOpenChannel() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTEBase::ParseSimOpenChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimOpenChannel() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    unsigned int nChannelId = 0;
    int* pnChannelId = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    //  Could have +XEER response here, if AT command returned CME error.
    if (FindAndSkipString(szRsp, "+XEER: ", szRsp))
    {
        unsigned int nCause = 0, nRes1 = 0, nRes2 = 0;

        //  +XEER: "SIM ACCESS", 203, 68, 81 = RIL_E_MISSING_RESOURCE
        //  +XEER: "SIM ACCESS", 203, 6A, 86 = RIL_E_MISSING_RESOURCE
        //  +XEER: "SIM ACCESS", 203, 6A, 81 = RIL_E_MISSING_RESOURCE
        //  +XEER: "SIM ACCESS", 202         = RIL_E_MISSING_RESOURCE
        //  +XEER: "SIM ACCESS", 203, 6A, 82 = RIL_E_NO_SUCH_ELEMENT
        //  +XEER: "SIM ACCESS", 200         = RIL_E_INVALID_PARAMETER
        //  TODO: Add RIL_E_INVALID_PARAMETER in ril.h and Android java framework
        //  For now, just return RIL_E_GENERIC_FAILURE

        //  Parse the "<category>", <cause>, <res1>, <res2>

        //  Skip over the "<category>"
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: cannot parse XEER, can't skip <category>\r\n");
            goto Error;
        }

        //  Extract <cause>
        if (!ExtractUInt32(szRsp, nCause, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: cannot parse XEER, can't extract <cause>\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTEBase::ParseSimOpenChannel() - XEER, nCause=[%d]\r\n", nCause);

        if (202 == nCause)
        {
            return RIL_E_MISSING_RESOURCE;
        }
        else if (200 == nCause)
        {
            return RIL_E_INVALID_PARAMETER;
        }
        else if (203 == nCause)
        {
            // Extract ,<res1>, <res2>
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes1, szRsp) ||
                !SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes2, szRsp) )
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: cannot parse XEER, can't extract <res1>,<res2>\r\n");
                goto Error;
            }

            RIL_LOG_INFO("CTEBase::ParseSimOpenChannel() - XEER, nRes1=[0x%02X], nRes2=[0x%02X]\r\n", nRes1, nRes2);

            if ( (0x68 == nRes1 && 0x81 == nRes2) ||
                 (0x6A == nRes1 && 0x86 == nRes2) ||
                 (0x6A == nRes1 && 0x81 == nRes2) )
            {
                return RIL_E_MISSING_RESOURCE;
            }
            else if (0x6A == nRes1 && 0x82 == nRes2)
            {
                return RIL_E_NO_SUCH_ELEMENT;
            }
            else
            {
                //  None of the above
                return RIL_E_GENERIC_FAILURE;
            }
        }
        else
        {
            //  nCause none of the above
            return RIL_E_GENERIC_FAILURE;
        }
    }
    else
    {
        pnChannelId = (int*)malloc(sizeof(int));
        if (NULL == pnChannelId)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: Could not allocate memory for an int.\r\n", sizeof(int));
            goto Error;
        }
        memset(pnChannelId, 0, sizeof(int));

        // Parse "<prefix><channelId><postfix>"
        SkipRspStart(szRsp, g_szNewLine, szRsp);

        //if (!FindAndSkipString(szRsp, "+CCHO: ", szRsp) ||
        //    !ExtractUInt32(szRsp, nChannelId, szRsp))
        if (!ExtractUInt32(szRsp, nChannelId, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimOpenChannel() - ERROR: Could not extract the Channel Id.\r\n");
            goto Error;
        }

        *pnChannelId = nChannelId;

        res = RRIL_RESULT_OK;

        rRspData.pData   = (void*)pnChannelId;
        rRspData.uiDataSize  = sizeof(int*);
    }

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnChannelId);
        pnChannelId = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseSimOpenChannel() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 106
//
RIL_RESULT_CODE CTEBase::CoreSimCloseChannel(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSimCloseChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nChannelId = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimCloseChannel() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nChannelId = ((int *)pData)[0];

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCHC=%u\r", nChannelId))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimCloseChannel() - ERROR: Cannot create CCHC command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimCloseChannel() - ERROR: Cannot create XEER command\r\n");
        goto Error;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSimCloseChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSimCloseChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimCloseChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimCloseChannel() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    //  Could have +XEER response here, if AT command returned CME error.
    if (FindAndSkipString(szRsp, "+XEER: ", szRsp))
    {
        unsigned int nCause = 0, nRes1 = 0, nRes2 = 0;

        //  +XEER: "SIM ACCESS", 202         = RIL_E_MISSING_RESOURCE
        //  +XEER: "SIM ACCESS", 200         = RIL_E_INVALID_PARAMETER
        //  +XEER: "SIM ACCESS", 203, 68, 81 = RIL_E_INVALID_PARAMETER
        //  TODO: Add RIL_E_INVALID_PARAMETER in ril.h and Android java framework
        //  For now, just return RIL_E_GENERIC_FAILURE

        //  Parse the "<category>", <cause>, <res1>, <res2>

        //  Skip over the "<category>"
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimCloseChannel() - ERROR: cannot parse XEER, can't skip <category>\r\n");
            goto Error;
        }

        //  Extract <cause>
        if (!ExtractUInt32(szRsp, nCause, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimCloseChannel() - ERROR: cannot parse XEER, can't extract <cause>\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTEBase::ParseSimCloseChannel() - XEER, nCause=[%d]\r\n", nCause);

        if (202 == nCause)
        {
            return RIL_E_MISSING_RESOURCE;
        }
        else if (200 == nCause)
        {
            return RIL_E_INVALID_PARAMETER;
        }
        else if (203 == nCause)
        {
            // Extract ,<res1>, <res2>
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes1, szRsp) ||
                !SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes2, szRsp) )
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimCloseChannel() - ERROR: cannot parse XEER, can't extract <res1>,<res2>\r\n");
                goto Error;
            }

            RIL_LOG_INFO("CTEBase::ParseSimCloseChannel() - XEER, nRes1=[0x%02X], nRes2=[0x%02X]\r\n", nRes1, nRes2);

            if (0x68 == nRes1 && 0x81 == nRes2)
            {
                return RIL_E_INVALID_PARAMETER;
            }
            else
            {
                //  None of the above
                return RIL_E_GENERIC_FAILURE;
            }
        }
        else
        {
            // nCause none of the above
            return RIL_E_GENERIC_FAILURE;
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::ParseSimCloseChannel() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 107
//
RIL_RESULT_CODE CTEBase::CoreSimTransmitChannel(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO_v6 *   pSimIOArgs = NULL;
    // ETSI TS 102 221 Section "Coding of Class Byte"
    int classByte = (RIL_APPTYPE_USIM == m_nSimAppType) ? 0x80 : 0xA0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO_v6) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO_v6 *)pData;

    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitChannel() - cla=%02X command=%d fileid=%04X p1=%d p2=%d p3=%d data=\"%s\"\r\n",
        classByte, pSimIOArgs->command, pSimIOArgs->fileid,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data);

    if ((NULL == pSimIOArgs->data) || (strlen(pSimIOArgs->data) == 0))
    {
        if (pSimIOArgs->p3 < 0)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CGLA=%d,%d,\"%02x%02x%02x%02x\"",
                    pSimIOArgs->fileid,
                    8,
                    classByte,
                    pSimIOArgs->command,
                    pSimIOArgs->p1,
                    pSimIOArgs->p2))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: cannot create CGLA command 1\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x\"",
                    pSimIOArgs->fileid,
                    10,
                    classByte,
                    pSimIOArgs->command,
                    pSimIOArgs->p1,
                    pSimIOArgs->p2,
                    pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: cannot create CGLA command 2\r\n");
                goto Error;
            }
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s\"",
                pSimIOArgs->fileid,
                10 + strlen(pSimIOArgs->data),
                classByte,
                pSimIOArgs->command,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data))
        {
            RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: cannot create CGLA command 3\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreSimTransmitChannel() - ERROR: Cannot create XEER command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreSimTransmitChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSimTransmitChannel(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseSimTransmitChannel() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * szRsp = rRspData.szResponse;

    UINT32  uiSW1 = 0;
    UINT32  uiSW2 = 0;
    UINT32  uiLen = 0;
    char* szResponseString = NULL;
    UINT32  cbResponseString = 0;

    RIL_SIM_IO_Response* pResponse = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    //  Could have +XEER response here, if AT command returned CME error.
    if (FindAndSkipString(szRsp, "+XEER: ", szRsp))
    {
        unsigned int nCause = 0, nRes1 = 0, nRes2 = 0;

        //  +XEER: "SIM ACCESS", 200         = RIL_E_INVALID_PARAMETER
        //  +XEER: "SIM ACCESS", 203, 68, 81 = RIL_E_INVALID_PARAMETER
        //  TODO: Add RIL_E_INVALID_PARAMETER in ril.h and Android java framework
        //  For now, just return RIL_E_GENERIC_FAILURE

        //  Parse the "<category>", <cause>, <res1>, <res2>

        //  Skip over the "<category>"
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: cannot parse XEER, can't skip <category>\r\n");
            goto Error;
        }

        //  Extract <cause>
        if (!ExtractUInt32(szRsp, nCause, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: cannot parse XEER, can't extract <cause>\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTEBase::ParseSimTransmitChannel() - XEER, nCause=[%d]\r\n", nCause);

        if (200 == nCause)
        {
            return RIL_E_INVALID_PARAMETER;
        }
        else if (203 == nCause)
        {
            // Extract ,<res1>, <res2>
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes1, szRsp) ||
                !SkipString(szRsp, ",", szRsp) ||
                !ExtractHexUInt32(szRsp, nRes2, szRsp) )
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: cannot parse XEER, can't extract <res1>,<res2>\r\n");
                goto Error;
            }

            RIL_LOG_INFO("CTEBase::ParseSimTransmitChannel() - XEER, nRes1=[0x%02X], nRes2=[0x%02X]\r\n", nRes1, nRes2);

            if (0x68 == nRes1 && 0x81 == nRes2)
            {
                return RIL_E_INVALID_PARAMETER;
            }
            else
            {
                //  None of the above
                return RIL_E_GENERIC_FAILURE;
            }
        }
        else
        {
            // nCause none of the above
            return RIL_E_GENERIC_FAILURE;
        }
    }
    else
    {
        // Parse "<prefix>+CGLA: <len>,<response><postfix>"
        SkipRspStart(szRsp, g_szNewLine, szRsp);


        if (!SkipString(szRsp, "+CGLA: ", szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Could not skip over \"+CSIM: \".\r\n");
            goto Error;
        }

        if (!ExtractUInt32(szRsp, uiLen, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Could not extract uiLen value.\r\n");
            goto Error;
        }


        // Parse ","
        if (SkipString(szRsp, ",", szRsp))
        {
            // Parse <response>
            // NOTE: we take ownership of allocated szResponseString
            if (!ExtractQuotedStringWithAllocatedMemory(szRsp, szResponseString, cbResponseString, szRsp))
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Could not extract data string.\r\n");
                goto Error;
            }
            else
            {
                RIL_LOG_INFO("CTEBase::ParseSimTransmitChannel() - Extracted data string: \"%s\" (%u chars)\r\n", szResponseString, cbResponseString);
            }

            if (0 != (cbResponseString - 1) % 2)
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() : ERROR : String was not a multiple of 2.\r\n");
                goto Error;
            }
        }

        // Allocate memory for the response struct PLUS a buffer for the response string
        // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
        // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
        pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
        if (NULL == pResponse)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
            goto Error;
        }
        memset(pResponse, 0, sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);

        //  Response must be 4 chars or longer - cbResponseString includes NULL character
        if (cbResponseString < 5)
        {
            RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: response length must be 4 or greater.\r\n");
            goto Error;
        }

        sscanf(&szResponseString[cbResponseString-5], "%02x%02x", &uiSW1, &uiSW2);
        szResponseString[cbResponseString-5] = '\0';

        pResponse->sw1 = uiSW1;
        pResponse->sw2 = uiSW2;

        if (NULL == szResponseString)
        {
            pResponse->simResponse = NULL;
        }
        else
        {
            pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
            if (!CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString))
            {
                RIL_LOG_CRITICAL("CTEBase::ParseSimTransmitChannel() - ERROR: Cannot CopyStringNullTerminate szResponseString\r\n");
                goto Error;
            }

            // Ensure NULL termination!
            pResponse->simResponse[cbResponseString] = '\0';
        }

        // Parse "<postfix>"
        SkipRspEnd(szRsp, g_szNewLine, szRsp);

        rRspData.pData   = (void*)pResponse;
        rRspData.uiDataSize  = sizeof(RIL_SIM_IO_Response);

        res = RRIL_RESULT_OK;
    }

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    delete[] szResponseString;
    szResponseString = NULL;

    RIL_LOG_VERBOSE("CTEBase::ParseSimTransmitChannel() - Exit\r\n");
    return res;
}


#if defined(M2_VT_FEATURE_ENABLED)
//
// RIL_REQUEST_HANGUP_VT 108
//
RIL_RESULT_CODE CTEBase::CoreHangupVT(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreHangupVT() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nCause = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangupVT() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangupVT() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    //  Extract data
    nCause = ((int *)pData)[0];
    RIL_LOG_INFO("CTEBase::CoreHangupVT() - Cause value=[%d]\r\n", nCause);

    //  Form AT command string
    //  Send AT+XSETCAUSE=1,<cause>
    //  Regardless of error result, then send ATH.
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XSETCAUSE=1,%u\r", nCause))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangupVT() - ERROR: Cannot create XSETCAUSE command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "ATH\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreHangupVT() - ERROR: Cannot create ATH command\r\n");
        goto Error;
    }

    // All tests are OK.
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreHangupVT() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTEBase::ParseHangupVT(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseHangupVT() - Enter\r\n");
    RIL_LOG_VERBOSE("CTEBase::ParseHangupVT() - Exit\r\n");
    return RRIL_RESULT_OK;
}


//
// RIL_REQUEST_DIAL_VT 109
//
RIL_RESULT_CODE CTEBase::CoreDialVT(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreDialVT() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_Dial *pRilDial   = NULL;
    const char *clir;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDialVT() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_Dial) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDialVT() - ERROR: Invalid data size=%d.\r\n, uiDataSize");
        goto Error;
    }

    pRilDial = (RIL_Dial*)pData;


    //  First populate rReqData.szCmd1 with FCLASS and CBST values.
    //  Then populate rReqData.szCmd2 with ATD string.

    //  Now fill in szCmd1
    //  TODO: These values may change.
    if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+FCLASS=0;+CBST=134,1,0\r", sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_CRITICAL("CTEBase::CoreDialVT() - ERROR: CopyStringNullTerminate FCLASS,CBST failed\r\n");
        goto Error;
    }

    //  Now fill in szCmd1
    switch (pRilDial->clir)
    {
        case 1:  // invocation
            clir = "I";
            break;

        case 2:  // suppression
            clir = "i";
            break;

        default:  // subscription default
            clir = "";
            break;
    }

    if (PrintStringNullTerminate(rReqData.szCmd2,  sizeof(rReqData.szCmd2), "ATD%s%s\r",
                pRilDial->address, clir))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTEBase::CoreDialVT() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDialVT(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDialVT() - Enter\r\n");
    RIL_LOG_VERBOSE("CTEBase::ParseDialVT() - Exit\r\n");
    return RRIL_RESULT_OK;
}
#endif // M2_VT_FEATURE_ENABLED


RIL_SignalStrength_v6* CTEBase::ParseQuerySignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQuerySignalStrength() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 uiRSSI  = 0;
    UINT32 uiBER   = 0;
    RIL_SignalStrength_v6* pSigStrData;

    pSigStrData = (RIL_SignalStrength_v6*)malloc(sizeof(RIL_SignalStrength_v6));
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }

    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength_v6));

    // Parse "<prefix>+CSQ: <rssi>,<ber><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CSQ: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySignalStrength() - ERROR: Could not find AT response.\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiRSSI, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySignalStrength() - ERROR: Could not extract uiRSSI.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiBER, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySignalStrength() - ERROR: Could not extract uiBER.\r\n");
        goto Error;
    }

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySignalStrength() - ERROR: Could not extract the response end.\r\n");
        goto Error;
    }

    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CTEBase::ParseQuerySignalStrength - Exit()\r\n");
    return pSigStrData;
}

//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTEBase::ParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseUnsolicitedSignalStrength() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    RIL_SignalStrength_v6* pSigStrData = ParseQuerySignalStrength(rRspData);
    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseUnsolicitedSignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength_v6.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, (void*)pSigStrData, sizeof(RIL_SignalStrength_v6));

Error:
    free(pSigStrData);
    pSigStrData = NULL;

    RIL_LOG_VERBOSE("CTEBase::ParseUnsolicitedSignalStrength() - Exit\r\n");
    return res;
}


//
// RIL_UNSOL_DATA_CALL_LIST_CHANGED  1010
//
RIL_RESULT_CODE CTEBase::ParseDataCallListChanged(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseDataCallListChanged() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    P_ND_PDP_CONTEXT_DATA pPDPListData = NULL;
    UINT32 nValue = 0;
    UINT32 nCID = 0;
    int  count = 0;
    int active[MAX_PDP_CONTEXTS] = {0};
    char szPDPType[MAX_BUFFER_SIZE] = {0};
    char szIP[MAX_BUFFER_SIZE] = {0};
    int status[MAX_PDP_CONTEXTS] = {0};
    int suggestedRetryTime[MAX_PDP_CONTEXTS] = {0};
    char szAPN[MAX_BUFFER_SIZE] = {0};
    char szDnses[MAX_BUFFER_SIZE] = {0};
    char szGateways[MAX_BUFFER_SIZE] = {0};

    pPDPListData = (P_ND_PDP_CONTEXT_DATA)malloc(sizeof(S_ND_PDP_CONTEXT_DATA));
    if (NULL == pPDPListData)
    {
        RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Could not allocate memory for a P_ND_PDP_CONTEXT_DATA struct.\r\n");
        goto Error;
    }
    memset(pPDPListData, 0, sizeof(S_ND_PDP_CONTEXT_DATA));
    memset(active, 0, sizeof(active));

    // Parse "+CGACT: "
    while (FindAndSkipString(pszRsp, "+CGACT: ", pszRsp))
    {
        // Parse <cid>
        if (!ExtractUInt32(pszRsp, nCID, pszRsp) ||  ((nCID > MAX_PDP_CONTEXTS) || 0 == nCID ))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Invalid CID.\r\n");
            goto Error;
        }

        // Parse <state>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 2, nValue, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Invalid state.\r\n");
            goto Error;
        }
        active[nCID - 1] = nValue;
        RIL_LOG_INFO("CTEBase::ParseDataCallListChanged() - Context %d %s.\r\n", nCID, (nValue ? "active" : "not active") );
    }

    // Parse +CGDCONT response
    count = 0;
    while (FindAndSkipString(pszRsp, "+CGDCONT: ", pszRsp))
    {
        CChannel_Data *pChannelData = NULL;

        // Parse <cid>
        if (!ExtractUInt32(pszRsp, nCID, pszRsp) ||  ((nCID > MAX_PDP_CONTEXTS) || 0 == nCID ))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Could not extract CID.\r\n");
            goto Error;
        }
        //  Grab the pChannelData for this CID
        pChannelData = CChannel_Data::GetChnlFromContextID(nCID);

        pPDPListData->pPDPData[count].cid = nCID;

        // set active flag
        pPDPListData->pPDPData[count].active = active[nCID - 1];

        // set status
        pPDPListData->pPDPData[count].status = PDP_FAIL_NONE;

        // set suggestedRetryTime
        pPDPListData->pPDPData[count].suggestedRetryTime = -1;

        // Parse ,<PDP_type>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szPDPType, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Could not extract PDP type.\r\n");
            goto Error;
        }
        strncpy(pPDPListData->pTypeBuffers[count], szPDPType, MAX_BUFFER_SIZE);
        pPDPListData->pPDPData[count].type = pPDPListData->pTypeBuffers[count];

        // Skip over ,<APN>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szAPN, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        //  This is interface name (i.e. rmnet0)
        if (pChannelData && pChannelData->m_szInterfaceName)
        {
            strncpy(pPDPListData->pIfnameBuffers[count], pChannelData->m_szInterfaceName, MAX_BUFFER_SIZE);
            pPDPListData->pPDPData[count].ifname = pPDPListData->pIfnameBuffers[count];
        }
        else
        {
            //  don't fill
            strcpy(pPDPListData->pIfnameBuffers[count], "");
            pPDPListData->pPDPData[count].ifname = pPDPListData->pIfnameBuffers[count];
        }

        // Parse ,<PDP_addr>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szIP, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseDataCallListChanged() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        strncpy(pPDPListData->pAddressBuffers[count], szIP, MAX_BUFFER_SIZE);
        pPDPListData->pPDPData[count].addresses = pPDPListData->pAddressBuffers[count];

        //  Populate DNSs and gateways
        if (pChannelData)
        {
            sprintf(pPDPListData->pDnsesBuffers[count], "%s %s %s %s",
                (pChannelData->m_szDNS1 ? pChannelData->m_szDNS1 : ""),
                (pChannelData->m_szDNS2 ? pChannelData->m_szDNS2 : ""),
                (pChannelData->m_szIpV6DNS1 ? pChannelData->m_szIpV6DNS1 : ""),
                (pChannelData->m_szIpV6DNS2 ? pChannelData->m_szIpV6DNS2 : ""));
        }
        else
        {
            strcpy(pPDPListData->pDnsesBuffers[count], "");
        }
        pPDPListData->pPDPData[count].dnses = pPDPListData->pDnsesBuffers[count];

        if (pChannelData)
        {
            strncpy(pPDPListData->pGatewaysBuffers[count],
                (pChannelData->m_szIpGateways ? pChannelData->m_szIpGateways : ""),
                MAX_BUFFER_SIZE);
        }
        else
        {
            strcpy(pPDPListData->pGatewaysBuffers[count], "");
        }
        pPDPListData->pPDPData[count].gateways = pPDPListData->pGatewaysBuffers[count];

        // Parse ,<data_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseDataCallListChanged() - WARNING: Could not extract data comp.\r\n");
            goto Continue;
        }

        // Parse ,<head_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt32(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseDataCallListChanged() - WARNING: Could not extract header comp.\r\n");
            goto Continue;
        }

        // following we could have ,<pd1>[,...[,pdN]]
        // but Android does not care about extra parameters
        // so skip them all
Continue:

        // entry complete
        ++count;
    }

    RIL_LOG_INFO("CTEBase::ParseDataCallListChanged() - Parse complete, found [%d] contexts.\r\n", count);

    if (CRilLog::IsFullLogBuild())
    {
        for (int i = 0; i < count; ++i)
        {
            RIL_LOG_INFO("i=%d  status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\" addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
                i, pPDPListData->pPDPData[i].status, pPDPListData->pPDPData[i].suggestedRetryTime,
                pPDPListData->pPDPData[i].cid, pPDPListData->pPDPData[i].active,
                pPDPListData->pPDPData[i].type, pPDPListData->pPDPData[i].ifname,
                pPDPListData->pPDPData[i].addresses, pPDPListData->pPDPData[i].dnses,
                pPDPListData->pPDPData[i].gateways);
        }
    }

    if (count > 0)
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, (void*)pPDPListData, count * sizeof(RIL_Data_Call_Response_v6));
    }
    else
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
    }

    res = RRIL_RESULT_OK;

Error:
    free(pPDPListData);
    pPDPListData = NULL;


    RIL_LOG_VERBOSE("CTEBase::ParseDataCallListChanged() - Exit\r\n");
    return res;
}


//
// GET IP ADDRESS (sent internally)
//
RIL_RESULT_CODE CTEBase::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED; // only suported at modem level
}

//
// GET DNS (sent internally)
//
RIL_RESULT_CODE CTEBase::ParseDns(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;  // only suported at modem level
}

//
// QUERY PIN2 (sent internally)
//
RIL_RESULT_CODE CTEBase::ParseQueryPIN2(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;  // only suported at modem level
}

//
// Parse Extended Error Report
//
BOOL CTEBase::ParseCEER(RESPONSE_DATA & rRspData, UINT32& rUICause)
{
    BOOL bRet = FALSE;
    char      szDummy[MAX_BUFFER_SIZE];
    const char* szRsp = rRspData.szResponse;

    if (FindAndSkipString(szRsp, "+CEER: ", szRsp))
    {
        bRet = TRUE;
        rUICause = 0;

        // skip first string
        if (!ExtractQuotedString(szRsp, szDummy, MAX_BUFFER_SIZE, szRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseCEER() - ERROR: Could not find first string.\r\n");
            goto Error;
        }

        // Get failure cause (if it exists)
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractUInt32(szRsp, rUICause, szRsp))
            {
                RIL_LOG_CRITICAL("CTEBase::ParseCEER() - ERROR: Could not extract failure cause.\r\n");
                goto Error;
            }
        }

        //  Get verbose description (if it exists)
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedString(szRsp, szDummy, MAX_BUFFER_SIZE, szRsp))
            {
                RIL_LOG_WARNING("CTEBase::ParseCEER() - WARNING: Could not extract verbose cause.\r\n");
            }
            else if (!SkipRspEnd(szRsp, g_szNewLine, szRsp))
            {
                RIL_LOG_WARNING("CTEBase::ParseCEER() - WARNING: Could not extract RspEnd.\r\n");
            }
        }
        else if (!SkipRspEnd(szRsp, g_szNewLine, szRsp))
        {
            RIL_LOG_WARNING("CTEBase::ParseCEER() - WARNING: Could not extract RspEnd.\r\n");
        }
    }

Error:
    return bRet;
}

//
// QUERY SIM SMS Store Status (sent internally)
//
RIL_RESULT_CODE CTEBase::ParseQuerySimSmsStoreStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTEBase::ParseQuerySimSmsStoreStatus() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    char szMemStore[3];
    UINT32 uiUsed = 0, uiTotal = 0;
    int i = 0;
    //  The number of memory store information returned by AT+CPMS?.
    const int MAX_MEM_STORE_INFO = 3;

    // Parse "<prefix>+CPMS: <mem1>,<used1>,<total1>,<mem2>,<used2>,<total2>,<mem3>,<used3>,<total3><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CPMS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySimSmsStoreStatus() - ERROR: Could not find AT response.\r\n");
        goto Error;
    }

    while (i < MAX_MEM_STORE_INFO)
    {
        if (!ExtractQuotedString(pszRsp, szMemStore, sizeof(szMemStore), pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQuerySimSmsStoreStatus() - ERROR: Could not extract <mem>.\r\n");
            goto Error;
        }

        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiUsed, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQuerySimSmsStoreStatus() - ERROR: Could not extract uiUsed.\r\n");
            goto Error;
        }

        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiTotal, pszRsp))
        {
            RIL_LOG_CRITICAL("CTEBase::ParseQuerySimSmsStoreStatus() - ERROR: Could not extract uiTotal.\r\n");
            goto Error;
        }

        if (0 == strcmp(szMemStore, "SM"))
        {
            if (uiUsed == uiTotal)
            {
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
            }
            break;
        }

        SkipString(pszRsp, ",", pszRsp);

        i++;
    }

    if (!FindAndSkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTEBase::ParseQuerySimSmsStoreStatus() - ERROR: Could not extract the response end.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTEBase::ParseQuerySimSmsStoreStatus - Exit()\r\n");
    return res;
}
