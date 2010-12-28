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
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  FV      1.00  Established v1.00 based on current code base.
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

CTEBase::CTEBase()
{
}

CTEBase::~CTEBase()
{
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTEBase::CoreGetSimStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetSimStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetSimStatus() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_CardStatus* pCardStatus = NULL;
    char szSimState[MAX_BUFFER_SIZE];
    const char * pszRsp = rRspData.szResponse;

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    pCardStatus = (RIL_CardStatus*)malloc(sizeof(RIL_CardStatus));
    if (NULL == pCardStatus)
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Could not allocate memory for RIL_CardStatus.\r\n");
        goto Error;
    }
    
    memset(pCardStatus, 0, sizeof(RIL_CardStatus));

    // Parse "<prefix>+CPIN: <state><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CPIN: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Could not skip \"+CPIN: \".\r\n");
        goto Error;
    }

    if (!ExtractUnquotedString(pszRsp, g_cTerminator, szSimState, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Could not extract SIM State.\r\n");
        goto Error;
    }

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetSimStatus() - ERROR: Could not skip response postfix.\r\n");
        goto Error;
    }

    // Initialize as per reference ril as insufficient documentation currently is available
    pCardStatus->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
    pCardStatus->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
    pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;

    // Number of apps is 1 (gsm) if SIM present. Set to 0 if absent.
    pCardStatus->num_applications = 0;

    if (0 == strcmp(szSimState, "READY"))
    {
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_READY\r\n");
        g_RadioState.SetRadioSIMReady();
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
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_PIN\r\n");
        g_RadioState.SetRadioSIMLocked();
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
    else if (0 == strcmp(szSimState, "SIM PUK"))
    {
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_PUK\r\n");
        g_RadioState.SetRadioSIMLocked();
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
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_NETWORK_PERSONALIZATION\r\n");
        g_RadioState.SetRadioSIMLocked();
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
    else if (0 == strcmp(szSimState, "SIM PIN2"))
    {
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_PIN2\r\n");
        //g_RadioState.SetRadioSIMLocked();
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_UNKNOWN;
        pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
        pCardStatus->applications[0].aid_ptr = NULL;
        pCardStatus->applications[0].app_label_ptr = NULL;
        pCardStatus->applications[0].pin1_replaced = 0;
        pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
        pCardStatus->applications[0].pin2 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;        
    }
    else if (0 == strcmp(szSimState, "SIM PUK2"))
    {
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_PUK2\r\n");
        //g_RadioState.SetRadioSIMLocked();
        pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
        pCardStatus->num_applications = 1;
        pCardStatus->gsm_umts_subscription_app_index = 0;

        pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
        pCardStatus->applications[0].app_state = RIL_APPSTATE_UNKNOWN;
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
        RIL_LOG_INFO("ParseGetSimStatus() - SIM Status: RIL_SIM_ABSENT\r\n");
        g_RadioState.SetRadioSIMLocked();
        pCardStatus->card_state = RIL_CARDSTATE_ABSENT;
        pCardStatus->num_applications = 0;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pCardStatus;
    rRspData.uiDataSize  = sizeof(RIL_CardStatus);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCardStatus);
        pCardStatus = NULL;
    }

    RIL_LOG_VERBOSE("ParseGetSimStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPin(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreEnterSimPin() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char **pszCmdData = NULL;
    char *pszPassword = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin() - ERROR: Invalid input\r\n");
        goto Error;
    }

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin() - ERROR: Request data was of unexpected size!\r\n");
        goto Error;
    }
    
    pszCmdData = (char**)pData;
    pszPassword = pszCmdData[0];
    
    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin() - ERROR: SIM PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN=\"%s\"\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin() - ERROR: Failed to write command to buffer!\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreEnterSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseEnterSimPin() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseEnterSimPin() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseEnterSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPuk(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreEnterSimPuk() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char* pszPUK;
    char* pszNewPIN;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPUK = ((char **) pData)[0];
    pszNewPIN = ((char **) pData)[1];

    if ((NULL == pszPUK) || (NULL == pszNewPIN))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk() - ERROR: PUK or new PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN=\"%s\",\"%s\"\r", pszPUK, pszNewPIN))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreEnterSimPuk() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPuk(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseEnterSimPuk() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseEnterSimPuk() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseEnterSimPuk() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPin2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreEnterSimPin2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char **pszCmdData = NULL;
    char *pszPassword = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin2() - ERROR: Invalid input\r\n");
        goto Error;
    }

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin2() - ERROR: Request data was of unexpected size!\r\n");
        goto Error;
    }
    
    pszCmdData = (char**)pData;
    pszPassword = pszCmdData[0];
    
    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin2() - ERROR: SIM PIN string was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN2=\"%s\"\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPin2() - ERROR: Failed to write command to buffer!\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreEnterSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseEnterSimPin2() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseEnterSimPin2() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseEnterSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
RIL_RESULT_CODE CTEBase::CoreEnterSimPuk2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreEnterSimPuk2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char* pszPUK2;
    char* pszNewPIN2;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk2() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk2() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPUK2 = ((char **) pData)[0];
    pszNewPIN2 = ((char **) pData)[1];

    if ((NULL == pszPUK2) || (NULL == pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk2() - ERROR: PUK2 or new PIN2 was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPIN2=\"%s\",\"%s\"\r", pszPUK2, pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CoreEnterSimPuk2() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreEnterSimPuk2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterSimPuk2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseEnterSimPuk2() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseEnterSimPuk2() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseEnterSimPuk2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
RIL_RESULT_CODE CTEBase::CoreChangeSimPin(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreChangeSimPin() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char* pszOldPIN;
    char* pszNewPIN;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszOldPIN = ((char **) pData)[0];
    pszNewPIN = ((char **) pData)[1];

    if ((NULL == pszOldPIN) || (NULL == pszNewPIN))
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin() - ERROR: old or new PIN was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPWD=\"SC\",\"%s\",\"%s\"\r", pszOldPIN, pszNewPIN))
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreChangeSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseChangeSimPin() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseChangeSimPin() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseChangeSimPin() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
RIL_RESULT_CODE CTEBase::CoreChangeSimPin2(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreChangeSimPin2() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char* pszOldPIN2;
    char* pszNewPIN2;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin2() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if ((2 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin2() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszOldPIN2 = ((char **) pData)[0];
    pszNewPIN2 = ((char **) pData)[1];

    if ((NULL == pszOldPIN2) || (NULL == pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin2() - ERROR: old or new PIN2 was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPWD=\"P2\",\"%s\",\"%s\"\r", pszOldPIN2, pszNewPIN2))
    {
        RIL_LOG_CRITICAL("CoreChangeSimPin2() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreChangeSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeSimPin2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseChangeSimPin2() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseChangeSimPin2() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseChangeSimPin2() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
RIL_RESULT_CODE CTEBase::CoreEnterNetworkDepersonalization(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreEnterNetworkDepersonalization() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    char* pszPassword;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreEnterNetworkDepersonalization() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreEnterNetworkDepersonalization() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszPassword = ((char **) pData)[0];

    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CoreEnterNetworkDepersonalization() - ERROR: Depersonalization code was NULL!\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLCK=\"PN\",0,\"%s\"\r", pszPassword))
    {
        RIL_LOG_CRITICAL("CoreEnterNetworkDepersonalization() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseEnterNetworkDepersonalization() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseEnterNetworkDepersonalization() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
RIL_RESULT_CODE CTEBase::CoreGetCurrentCalls(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetCurrentCalls() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLCC\r"))
    {
        RIL_LOG_CRITICAL("CoreGetCurrentCalls() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreGetCurrentCalls() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetCurrentCalls(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetCurrentCalls() - Enter\r\n");

    UINT32 nValue;
    UINT32 nUsed = 0;
    UINT32 nAllocated = 0;

    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* szDummy = rRspData.szResponse;
    char szAddress[MAX_BUFFER_SIZE];

    RIL_Call* calls = NULL;
    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;

    BOOL bSuccess;
    BOOL fHyperPoll = FALSE;

    P_ND_CALL_LIST_DATA pCallListData = NULL;
    int  nCalls = 0;

    // Parse "<prefix>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetCallList() - ERROR: Couldn't find rsp start\r\n");
        goto Error;
    }

    // Count up how many calls are in progress
    while (FindAndSkipString(szDummy, "+CLCC: ", szDummy))
    {
        nCalls++;
    }

    if (nCalls >= RRIL_MAX_CALL_ID_COUNT)
    {
        RIL_LOG_CRITICAL("ParseGetCallList() - ERROR: Can't process %d calls due to insufficient space.\r\n", nCalls);
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("ParseGetCallList() - Found %d calls\r\n", nCalls);
    }

    pCallListData = (P_ND_CALL_LIST_DATA)malloc(sizeof(S_ND_CALL_LIST_DATA));
    if (NULL == pCallListData)
    {
        RIL_LOG_CRITICAL("ParseGetCallList() - ERROR: Could not allocate memory for a S_ND_CALL_LIST_DATA struct.\r\n");
        goto Error;
    }
    memset(pCallListData, 0, sizeof(S_ND_CALL_LIST_DATA));

    // Parse "+CLCC: "
    while (FindAndSkipString(szRsp, "+CLCC: ", szRsp))
    {
        // Parse "<id>"
        if (!ExtractUInt(szRsp, nValue, szRsp))
        {
            goto Continue;
        }

        pCallListData->pCallData[nUsed].index = nValue;

        // Parse ",<direction>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUpperBoundedUInt(szRsp, 2, nValue, szRsp))
        {
            goto Continue;
        }

        pCallListData->pCallData[nUsed].isMT = nValue;

        // Parse ",<status>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt(szRsp, nValue, szRsp))
        {
            goto Continue;
        }

        pCallListData->pCallData[nUsed].state = (RIL_CallState)nValue;
        
        if (nValue >= RIL_CALL_DIALING)
        {
            // If we have a call coming in or going out that hasn't connected, poll!
            fHyperPoll = TRUE;
        }

        // Parse ",<type>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt(szRsp, nValue, szRsp))
        {
            goto Continue;
        }

        // If nValue is non-zero, then we are not in a voice call
        pCallListData->pCallData[nUsed].isVoice = nValue ? FALSE : TRUE;
        pCallListData->pCallData[nUsed].isVoicePrivacy = 0; // not used in GSM


        // Parse ",<multiparty>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUpperBoundedUInt(szRsp, 2, nValue, szRsp))
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
                    !ExtractUpperBoundedUInt(szRsp, 0x100, nValue, szRsp))
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
                    !ExtractUpperBoundedUInt(szRsp, 0x100, nValue, szRsp))
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
                BYTE description[MAX_BUFFER_SIZE];
                if (!ExtractQuotedString(szRsp, description, MAX_BUFFER_SIZE, szRsp))
                {
                    RIL_LOG_WARNING("ParseGetCallList() - WARNING: Failed to extract call name\r\n");
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
                        RIL_LOG_WARNING("ParseGetCallList() - WARNING: Buffer overflow in name buffer\r\n");
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

#if 0
    if (nUsed > 0)
    {
        if (fHyperPoll)
        {
            RIL_requestTimedCallback(notifyChangedCallState, NULL, &CallStateHyperPoll);
        }
        else
        {
            RIL_requestTimedCallback(notifyChangedCallState, NULL, &CallStateSlowPoll);
        }
    }
#endif // 0

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCallListData);
        pCallListData = NULL;
    }


    RIL_LOG_VERBOSE("ParseGetCurrentCalls() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DIAL 10
//
RIL_RESULT_CODE CTEBase::CoreDial(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDial() - Enter\r\n");

    RIL_Errno eRetVal    = RIL_E_GENERIC_FAILURE;
    RIL_Dial *pRilDial   = NULL;
    BYTE*     szCmdWalk  = rReqData.szCmd1;
    BYTE*     szAddrWalk = NULL;
    BYTE*     szDialStringStart = NULL;
    UINT32      cchCmd     = sizeof(rReqData.szCmd1);
    int       clirVal    = 0;
    const int nMaxDialStringLength = 43;

    const char* pcszCmdEnd = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: pData is NULL.\r\n");
        goto Error;
    }
    
    if (sizeof(RIL_Dial) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }
        
    //  Extract data.
    pRilDial = (RIL_Dial *)pData;
    szAddrWalk = (BYTE*)(pRilDial->address);
    clirVal = pRilDial->clir;


    if (!CopyStringNullTerminate(szCmdWalk, "ATD", cchCmd - (szCmdWalk - rReqData.szCmd1)))
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: String truncation or other error when writting ATD!\r\n");
        goto Error;
    }

    szCmdWalk = strchr(szCmdWalk, '\0');  // NO_TYPO: 27
    if (NULL == szCmdWalk)
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: Did not find NULL termination character in string.\r\n");
        goto Error;
    }

    szDialStringStart = szCmdWalk;

    pcszCmdEnd = rReqData.szCmd1 + cchCmd - 1;
    while (szCmdWalk < pcszCmdEnd && *szAddrWalk != '\0')
    {
        // Only allow characters in the set specified by GSM 07.07 section 6.2
        if (strchr("1234567890*#+ABCD,TP!W@", *szAddrWalk))
        {
            *szCmdWalk++ = *szAddrWalk;
        }
        szAddrWalk++;
    }

    //  Check to see if the dial string is too long.
    if ((szCmdWalk - szDialStringStart) > nMaxDialStringLength)
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: Dial string is too long.\r\n");
        goto Error;
    }

    if (szCmdWalk == szDialStringStart)
    {
        RIL_LOG_CRITICAL("CoreDial() - ERROR: Address string does not contain any valid characters.\r\n");
        goto Error;
    }

    // Remember to paste on the terminating '\0'
    *szCmdWalk='\0';

    if (1 == clirVal)  // "CLIR invocation" (restrict CLI presentation)
    {
        (void)CopyStringNullTerminate(szCmdWalk, "I", cchCmd - (szCmdWalk - rReqData.szCmd1));
        szCmdWalk++;
    }
    else if (2 == clirVal)  // "CLIR suppression" (allow CLI presentation)
    {
        (void)CopyStringNullTerminate(szCmdWalk, "i", cchCmd - (szCmdWalk - rReqData.szCmd1));
        szCmdWalk++;
    }

    (void)CopyStringNullTerminate(szCmdWalk, ";\r", cchCmd - (szCmdWalk - rReqData.szCmd1));

    eRetVal = RIL_E_SUCCESS;

Error:
    RIL_LOG_VERBOSE("CoreDial() - Exit\r\n");
    return eRetVal;
}

RIL_RESULT_CODE CTEBase::ParseDial(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDial() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDial() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMSI 11
//
RIL_RESULT_CODE CTEBase::CoreGetImsi(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetImsi() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CIMI\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGetImsi() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImsi(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetImsi() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    char * szSerialNumber = NULL;

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("ParseGetImsi() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    szSerialNumber = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == szSerialNumber)
    {
        RIL_LOG_CRITICAL("ParseGetImsi() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }
    memset(szSerialNumber, 0x00, MAX_BUFFER_SIZE);

    // Parse "<prefix><serial_number><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !ExtractUnquotedString(pszRsp, g_cTerminator, szSerialNumber, MAX_BUFFER_SIZE, pszRsp) ||
        !SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImsi() - ERROR: Could not extract the IMSI string.\r\n");
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

    RIL_LOG_VERBOSE("ParseGetImsi() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP 12
//
RIL_RESULT_CODE CTEBase::CoreHangup(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreHangup() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnLine = NULL;
    
    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreHangup() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreHangup() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pnLine = (int*)pData;
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CHLD=1%u\r", pnLine[0]))
    {
        res = RRIL_RESULT_OK;
    }
    
Error:

    RIL_LOG_VERBOSE("CoreHangup() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangup(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseHangup() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseHangup() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
RIL_RESULT_CODE CTEBase::CoreHangupWaitingOrBackground(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreHangupWaitingOrBackground() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "ATH\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseHangupWaitingOrBackground() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
RIL_RESULT_CODE CTEBase::CoreHangupForegroundResumeBackground(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreHangupForegroundResumeBackground() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=1\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseHangupForegroundResumeBackground() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
// RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
//
RIL_RESULT_CODE CTEBase::CoreSwitchHoldingAndActive(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSwitchHoldingAndActive() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=2\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSwitchHoldingAndActive() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CONFERENCE 16
//
RIL_RESULT_CODE CTEBase::CoreConference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreConference() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=3\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreConference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseConference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseConference() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseConference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_UDUB 17
//
RIL_RESULT_CODE CTEBase::CoreUdub(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreUdub() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "ATH\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreUdub() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseUdub(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseUdub() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseUdub() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
RIL_RESULT_CODE CTEBase::CoreLastCallFailCause(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreLastCallFailCause() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CEER\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreLastCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseLastCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseLastCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint      uiCause  = 0;
    int*      pCause   = NULL;
    char      szDummy[MAX_BUFFER_SIZE];

    pCause= (int*) malloc(sizeof(int));
    if (NULL == pCause)
    {
        RIL_LOG_CRITICAL("ParseGetLastCallFailCause() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CEER: <string><value><string><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CEER: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetLastCallFailCause() - ERROR: Could not find AT response.\r\n");
        goto Error;
    }

    // skip first string
    // TODO: should check if this is indeed for a call?
    if (!ExtractQuotedString(pszRsp, szDummy, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetLastCallFailCause() - ERROR: Could not find first string.\r\n");
        goto Error;
    }

    // Get failure cause
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !SkipSpaces(pszRsp, pszRsp) ||
        !ExtractUInt(pszRsp, uiCause, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetLastCallFailCause() - ERROR: Could not extract failure cause.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !SkipSpaces(pszRsp, pszRsp) ||
        !ExtractQuotedString(pszRsp, szDummy, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_WARNING("ParseGetLastCallFailCause() - WARNING: Could not extract verbose cause.\r\n");
    }
    else if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_WARNING("ParseGetLastCallFailCause() - WARNING: Could not extract RspEnd.\r\n");
    }

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
    RIL_LOG_INFO("ParseGetLastCallFailCause() - Last call fail cause [%d]\r\n", uiCause);
    
    res = RRIL_RESULT_OK;        

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCause);
        pCause = NULL;
    }        

    RIL_LOG_VERBOSE("ParseLastCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTEBase::CoreSignalStrength(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSignalStrength() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CSQ\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSignalStrength() - Enter\r\n");
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint                uiRSSI  = 0;
    uint                uiBER   = 0;
    RIL_SignalStrength* pSigStrData;

    pSigStrData = (RIL_SignalStrength*)malloc(sizeof(RIL_SignalStrength));

    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("ParseSignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength.\r\n");
        goto Error;
    }
    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength));

    // Parse "<prefix>+CSQ: <rssi>,<ber><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CSQ: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSignalStrength() - ERROR: Could not find AT response.\r\n");
        goto Error;            
    }
    
    if (!ExtractUInt(pszRsp, uiRSSI, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSignalStrength() - ERROR: Could not extract uiRSSI.\r\n");
        goto Error;                        
    }
    
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiBER, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSignalStrength() - ERROR: Could not extract uiBER.\r\n");
        goto Error;            
    }
    
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSignalStrength() - ERROR: Could not extract the response end.\r\n");
        goto Error;
    }

    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    rRspData.pData   = (void*)pSigStrData;
    rRspData.uiDataSize  = sizeof(RIL_SignalStrength);
    
    res = RRIL_RESULT_OK;        

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("ParseSignalStrength() - Exit\r\n");
    return res;    
}

//
// RIL_REQUEST_REGISTRATION_STATE 20
//
RIL_RESULT_CODE CTEBase::CoreRegistrationState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint      uiNum    = 0;
    uint      uiStat   = 0;
    uint      uiLAC    = 0;
    uint      uiCID    = 0;
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;    

    P_ND_REG_STATUS pRegStatus = NULL;

    pRegStatus = (P_ND_REG_STATUS)malloc(sizeof(S_ND_REG_STATUS));
    if (NULL == pRegStatus)
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not allocate memory for a S_ND_REG_STATUS struct.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+CREG: "
    if (!SkipString(pszRsp, "+CREG: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not skip \"+CREG: \".\r\n");
        goto Error;
    }

    // Extract <n> and throw away
    if (!ExtractUInt(pszRsp, uiNum, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <n>.\r\n");
        goto Error;
    }

    // Skip ",<stat>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiStat, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Extract "<lac>"
        SkipString(pszRsp, "\"", pszRsp);
        if (!ExtractHexUInt(pszRsp, uiLAC, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <lac>.\r\n");
            goto Error;
        }
        SkipString(pszRsp, "\"", pszRsp);

        // Extract ",<cid>"
        if (!SkipString(pszRsp, ",", pszRsp))
        {
            RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract \",<cid>\".\r\n");
            goto Error;
        }
        SkipString(pszRsp, "\"", pszRsp);
        if (!ExtractHexUInt(pszRsp, uiCID, pszRsp))
         {
             RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <cid>.\r\n");
             goto Error;
         }
        SkipString(pszRsp, "\"", pszRsp);
    }
    
    // Do we have more to parse?
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Extract <n2> and throw away
        if (!ExtractUInt(pszRsp, uiNum, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <n2>.\r\n");
            goto Error;
        }
    }

    // Skip "<postfix>"
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not skip response postfix.\r\n");
        goto Error;
    }

    snprintf(pRegStatus->szStat,        REG_STATUS_LENGTH, "%d", (int)uiStat);
    snprintf(pRegStatus->szLAC,         REG_STATUS_LENGTH, "%x", (int)uiLAC);
    snprintf(pRegStatus->szCID,         REG_STATUS_LENGTH, "%x", (int)uiCID);
    //if (0 == uiStat)
    //{
    //    snprintf(pRegStatus->szNetworkType, REG_STATUS_LENGTH, "%d", 0);
    //}
    //else
    //{
        snprintf(pRegStatus->szNetworkType, REG_STATUS_LENGTH, "%d", (int)(g_uiAccessTechnology));
    //}


    pRegStatus->sStatusPointers.pszStat = pRegStatus->szStat;
    if (0 == uiLAC)
    {
        pRegStatus->sStatusPointers.pszLAC  = NULL;
    }
    else
    {
        pRegStatus->sStatusPointers.pszLAC  = pRegStatus->szLAC;
    }
    if (0 == uiCID)
    {
        pRegStatus->sStatusPointers.pszCID  = NULL;
    }
    else
    {
        pRegStatus->sStatusPointers.pszCID  = pRegStatus->szCID;
    }
    pRegStatus->sStatusPointers.pszNetworkType = pRegStatus->szNetworkType;
    pRegStatus->sStatusPointers.pszBaseStationID = NULL;
    pRegStatus->sStatusPointers.pszBaseStationLat = NULL;
    pRegStatus->sStatusPointers.pszBaseStationLon = NULL;
    pRegStatus->sStatusPointers.pszConcurrentServices = NULL;
    pRegStatus->sStatusPointers.pszSystemID = NULL;
    pRegStatus->sStatusPointers.pszNetworkID = NULL;
    pRegStatus->sStatusPointers.pszTSB58 = NULL;
    pRegStatus->sStatusPointers.pszPRL = NULL;
    pRegStatus->sStatusPointers.pszDefaultRoaming = NULL;
    pRegStatus->sStatusPointers.pszReasonDenied = NULL;

    // We cheat with the size here.
    // Although we have allocated a S_ND_REG_STATUS struct, we tell
    // Android that we have only allocated a S_ND_REG_STATUS_POINTERS
    // struct since Android is expecting to receive an array of string pointers.
    // Note that we only tell Android about the 14 pointers it supports for CREG notifications.

    rRspData.pData  = (void*)pRegStatus;
    rRspData.uiDataSize = sizeof(S_ND_REG_STATUS_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (g_pSIMReadyRegEvent)
    {
        CEvent::Signal(g_pSIMReadyRegEvent);
    }

    if (RRIL_RESULT_OK != res)
    {
        free(pRegStatus);
        pRegStatus = NULL;
    }

    RIL_LOG_VERBOSE("ParseRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GPRS_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTEBase::CoreGPRSRegistrationState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGPRSRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CGREG?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGPRSRegistrationState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGPRSRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    uint uiNum = 0;
    uint uiStat = 0;
    uint uiLAC = 0;
    uint uiCID = 0;

    P_ND_GPRS_REG_STATUS pGPRSRegStatus = NULL;

    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

    pGPRSRegStatus = (P_ND_GPRS_REG_STATUS)malloc(sizeof(S_ND_GPRS_REG_STATUS));
    if (NULL == pGPRSRegStatus)
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not allocate memory for a S_ND_GPRS_REG_STATUS struct.\r\n");
        goto Error;
    }

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    // Parse "+CGREG: "
    if (!SkipString(pszRsp, "+CGREG: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not skip over \"+CGREG: \".\r\n");
        goto Error;
    }

    // Parse <n> and throw away
    if (!ExtractUInt(pszRsp, uiNum, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not extract <n>.\r\n");
        goto Error;
    }

    // Parse ",<stat>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiStat, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse "<lac>"
        SkipString(pszRsp, "\"", pszRsp);
        if (!ExtractHexUInt(pszRsp, uiLAC, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not extract <lac>.\r\n");
            goto Error;
        }
        SkipString(pszRsp, "\"", pszRsp);

        // Parse ",<cid>"
        if (!SkipString(pszRsp, ",", pszRsp))
         {
             RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not extract <cid>.\r\n");
             goto Error;
         }
         SkipString(pszRsp, "\"", pszRsp);
         if (!ExtractHexUInt(pszRsp, uiCID, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not extract <cid>.\r\n");
            goto Error;
        }
        SkipString(pszRsp, "\"", pszRsp);
    }
    
    // Do we have more to parse?
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Extract <n2> and throw away
        if (!ExtractUInt(pszRsp, uiNum, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseRegistrationState() - ERROR: Could not extract <n2>.\r\n");
            goto Error;
        }
    }    

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGPRSRegistrationState() - ERROR: Could not skip over response postfix.\r\n");
        goto Error;
    }

    snprintf(pGPRSRegStatus->szStat,        REG_STATUS_LENGTH, "%d", (int)uiStat);
    snprintf(pGPRSRegStatus->szLAC,         REG_STATUS_LENGTH, "%x", (int)uiLAC);
    snprintf(pGPRSRegStatus->szCID,         REG_STATUS_LENGTH, "%x", (int)uiCID);
    //if (0 == uiStat)
    //{
    //    snprintf(pGPRSRegStatus->szNetworkType, REG_STATUS_LENGTH, "%d", 0);
    //}
    //else
    //{
        snprintf(pGPRSRegStatus->szNetworkType, REG_STATUS_LENGTH, "%d", (int)(g_uiAccessTechnology));
    //}

    pGPRSRegStatus->sStatusPointers.pszStat         = pGPRSRegStatus->szStat;
    if (0 == uiLAC)
    {
        pGPRSRegStatus->sStatusPointers.pszLAC          = NULL;
    }
    else
    {
        pGPRSRegStatus->sStatusPointers.pszLAC          = pGPRSRegStatus->szLAC;
    }
    if (0 == uiCID)
    {
        pGPRSRegStatus->sStatusPointers.pszCID          = NULL;
    }
    else
    {
        pGPRSRegStatus->sStatusPointers.pszCID          = pGPRSRegStatus->szCID;
    }
    pGPRSRegStatus->sStatusPointers.pszNetworkType  = pGPRSRegStatus->szNetworkType;

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

    RIL_LOG_VERBOSE("ParseGPRSRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTEBase::CoreOperator(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreOperator() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreOperator() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseOperator(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseOperator() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint      uiMode    = 0;
    uint      uiFormat  = 0;
    int       i         = 0;
    int       numericID = 0;
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;        

    P_ND_OP_NAMES pOpNames = NULL;

    pOpNames = (P_ND_OP_NAMES)malloc(sizeof(S_ND_OP_NAMES));
    if (NULL == pOpNames)
    {
        RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not allocate memory for S_ND_OP_NAMES struct.\r\n");
        goto Error;
    }
    memset(pOpNames, 0, sizeof(S_ND_OP_NAMES));

    // We ask the modem for the Long Name and the Numeric Name, but it looks
    // like we actually receive the Short name and the Numeric Name. (If we ask
    // for the Short name, we get CME ERROR 21.) We can thus assume that the
    // given Long Name is actually the Short Name, and use the Numeric Name
    // and our global lookup table to get the actual Long Name.

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());

    // no short name in Infineon modem
    pOpNames->szOpNameShort[0] = '\0';
    // Skip "+COPS: "
    while (SkipString(pszRsp, "+COPS: ", pszRsp))
    {
        // Extract "<mode>"
        if (!ExtractUInt(pszRsp, uiMode, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract <mode>.\r\n");
            goto Error;
        }

        // Extract ",<format>,<oper>" - these are optional
        if (SkipString(pszRsp, ",", pszRsp))
        {
            if (!ExtractUInt(pszRsp, uiFormat, pszRsp) ||
                !SkipString(pszRsp, ",", pszRsp))
            {
                RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract <format>.\r\n");
                goto Error;
            }


            switch (uiFormat)
            {
                case 0:
                {
                    //WCHAR tmp[MAX_BUFFER_SIZE];
                    //if (!ExtractQuotedUnicodeHexStringToUnicode(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    BYTE tmp[MAX_BUFFER_SIZE];
                    if (!ExtractQuotedString(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract the Long Format Operator Name.\r\n");
                        goto Error;
                    }
                    int i;
                    for (i = 0; tmp[i]; pOpNames->szOpNameLong[i] = (char) tmp[i], ++i);
                    pOpNames->szOpNameLong[i] = '\0';
                    RIL_LOG_INFO("ParseOperator() - Long oper: %s\r\n", pOpNames->szOpNameLong);
                }
                break;

                case 1:
                {
                    //WCHAR tmp[MAX_BUFFER_SIZE];
                    //if (!ExtractQuotedUnicodeHexStringToUnicode(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    BYTE tmp[MAX_BUFFER_SIZE];
                    if (!ExtractQuotedString(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract the Short Format Operator Name.\r\n");
                        goto Error;
                    }
                    int i;
                    for (i = 0; tmp[i]; pOpNames->szOpNameShort[i] = (char) tmp[i], ++i);
                    pOpNames->szOpNameShort[i] = '\0';
                    RIL_LOG_INFO("ParseOperator() - Short oper: %s\r\n", pOpNames->szOpNameShort);

                }
                break;

                case 2:
                {
                    //WCHAR tmp[MAX_BUFFER_SIZE];
                    //if (!ExtractQuotedUnicodeHexStringToUnicode(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    BYTE tmp[MAX_BUFFER_SIZE];
                    if (!ExtractQuotedString(pszRsp, tmp, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract the Long Format Operator Name.\r\n");
                        goto Error;
                    }
                    int i;
                    for (i = 0; tmp[i]; pOpNames->szOpNameNumeric[i] = (char) tmp[i], ++i);
                    pOpNames->szOpNameNumeric[i] = '\0';
                    RIL_LOG_INFO("ParseOperator() - Numeric: %s\r\n", pOpNames->szOpNameNumeric);
                }
                break;

                default:
                {
                    RIL_LOG_CRITICAL("ParseOperator() - ERROR: Unknown Format.\r\n");
                    goto Error;
                }
                break;
            }

            if (SkipString(pszRsp, ",", pszRsp))
            {
                uint act;
                ExtractUInt(pszRsp, act, pszRsp);
                #if 0
                switch (act)
                {
                    case 0:
                        g_uiAccessTechnology = ACT_GPRS;
                        break;
                    case 1:
                        g_uiAccessTechnology = ACT_EDGE;
                        break;
                    case 2:
                        g_uiAccessTechnology = ACT_UMTS;
                        break;
                    default:
                        g_uiAccessTechnology = ACT_UNKNOWN;
                }
                #endif // 0
            }
            else
            {
                //g_uiAccessTechnology = ACT_UNKNOWN;
            }

            pOpNames->sOpNamePtrs.pszOpNameLong    = pOpNames->szOpNameLong;
            pOpNames->sOpNamePtrs.pszOpNameShort   = pOpNames->szOpNameShort;
            pOpNames->sOpNamePtrs.pszOpNameNumeric = pOpNames->szOpNameNumeric;
            
            RIL_LOG_INFO("ParseOperator() - Long    Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameLong);
            RIL_LOG_INFO("ParseOperator() - Short   Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameShort);
            RIL_LOG_INFO("ParseOperator() - Numeric Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameNumeric);
        }
        else
        {
            RIL_LOG_INFO("ParseOperator() - <format> and <oper> not present.\r\n");
            pOpNames->sOpNamePtrs.pszOpNameLong    = NULL;
            pOpNames->sOpNamePtrs.pszOpNameShort   = NULL;
            pOpNames->sOpNamePtrs.pszOpNameNumeric = NULL;
            //g_uiAccessTechnology = ACT_UNKNOWN;
        }

        // Extract "<CR><LF>"
        if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseOperator() - ERROR: Could not extract response postfix.\r\n");
            goto Error;
        }

        // If we have another line to parse, get rid of its prefix now.
        // Note that this will do nothing if we don't have another line to parse.

        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        RIL_LOG_INFO("ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());
    }

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

    RIL_LOG_VERBOSE("ParseOperator() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RADIO_POWER 23
//
RIL_RESULT_CODE CTEBase::CoreRadioPower(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreRadioPower() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    bool bTurnRadioOn = false;
    
    if (0 == *(int *)pData)
    {
        RIL_LOG_INFO("CoreRadioPower() - Turn Radio OFF\r\n");
        bTurnRadioOn = false;
    }
    else
    {
        RIL_LOG_INFO("CoreRadioPower() - Turn Radio ON\r\n");
        bTurnRadioOn = true;
    }
    
    if (bTurnRadioOn)
    {
        RIL_LOG_INFO("CoreRadioPower() - Turning the Radio ON\r\n");

        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CFUN=1\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        RIL_LOG_INFO("CoreRadioPower() - Turning the Radio OFF\r\n");

        if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CFUN=4\r", sizeof(rReqData.szCmd1)))
        {
            res = RRIL_RESULT_OK;
        }
    }

    RIL_LOG_VERBOSE("CoreRadioPower() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseRadioPower(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseRadioPower() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseRadioPower() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF 24
//
RIL_RESULT_CODE CTEBase::CoreDtmf(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDtmf() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char tone;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreDtmf() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_CRITICAL("CoreDtmf() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    tone = ((char *)pData)[0];
    // tone should be one of 12 values: 0-9,*,#
    if (!(('#' == tone) || ('*' == tone) || ((tone >= '0') && (tone <= '9'))))
    {
        RIL_LOG_CRITICAL("CoreDtmf() - ERROR: DTMF tone [%c] out of range\r\n", tone);
        goto Error;
    }
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+VTS=%c,30\r"), tone)
    {
        RIL_LOG_CRITICAL("CoreDtmf() - ERROR: Unable to write command string to buffer\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDtmf(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDtmf() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDtmf() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS 25
//
RIL_RESULT_CODE CTEBase::CoreSendSms(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSendSms() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char **       pszCmdData   = NULL;
    char *        szSMSAddress = NULL;
    char *        szPDU        = NULL;

    int nPDULength = 0;    
    char szNoAddress[] = "00";

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSendSms() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != 2 * sizeof(char *))
    {
        RIL_LOG_CRITICAL("CoreSendSms() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    pszCmdData   = (char**)pData;

    szSMSAddress = pszCmdData[0];
    
    szPDU        = pszCmdData[1];
    
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CoreSendSms() - ERROR: Invalid input(s).\r\n");
        goto Error;
    }

    // 2 chars per byte.
    nPDULength = (strlen(szPDU) / 2);

    if (NULL == szSMSAddress)
    {
        szSMSAddress = szNoAddress;
    }

    (void)PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGS=%u\r", nPDULength);

    (void)PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s%s\x1a", szSMSAddress, szPDU);
    
    RIL_LOG_INFO("Payload: %s\r\n", CRLFExpandedString(rReqData.szCmd2, strlen(rReqData.szCmd2)).GetString());
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSendSms() - Exit\r\n");
    return res;
    
}

RIL_RESULT_CODE CTEBase::ParseSendSms(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSendSms() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    P_ND_SEND_MSG pSendMsg = NULL;
    uint          uiMsgRef;

    pSendMsg = (P_ND_SEND_MSG)malloc(sizeof(S_ND_SEND_MSG));
    if (NULL == pSendMsg)
    {
        RIL_LOG_CRITICAL("ParseSendSms() - ERROR: Could not allocate memory for a S_ND_SEND_MSG struct.\r\n");
        goto Error;
    }

    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendSms() - ERROR: Could not parse response prefix.\r\n");
        goto Error;
    }
    
    //  Sometimes modems add another rspStart here due to sending of a PDU.
    SkipRspStart(pszRsp, g_szNewLine, pszRsp);

    if (!SkipString(pszRsp, "+CMGS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendSms() - ERROR: Could not parse \"+CMGS: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, uiMsgRef, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendMsg() - ERROR: Could not parse <msgRef>.\r\n");
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
            RIL_LOG_CRITICAL("ParseSendSms() - ERROR: Could not parse <ackPdu>.\r\n");
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
        RIL_LOG_CRITICAL("ParseSendSms() - ERROR: Could not parse response postfix.\r\n");
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

    RIL_LOG_VERBOSE("ParseSendSms() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
RIL_RESULT_CODE CTEBase::CoreSendSmsExpectMore(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSendSmsExpectMore() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char **       pszCmdData   = NULL;
    char *        szSMSAddress = NULL;
    char *        szPDU        = NULL;
    int nPDULength = 0;    
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSendSmsExpectMore() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char **))
    {
        RIL_LOG_CRITICAL("CoreSendSmsExpectMore() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    pszCmdData   = (char**)pData;

    szSMSAddress = pszCmdData[0];
    szPDU        = pszCmdData[1];
    
    if (NULL == szPDU)
    {
        RIL_LOG_CRITICAL("CoreSendSmsExpectMore() - ERROR: Invalid input(s).\r\n");
        goto Error;
    }

    // 2 chars per byte.
    nPDULength = (strlen(szPDU) / 2);

    if (NULL == szSMSAddress)
    {
        strcpy(szSMSAddress, "00");
    }

    (void)PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMMS=1;+CMGS=%u\r", nPDULength);

    (void)PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s%s\x1a", szSMSAddress, szPDU);
    
    RIL_LOG_INFO("Payload: %s\r\n", CRLFExpandedString(rReqData.szCmd2, strlen(rReqData.szCmd2)).GetString());
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSendSmsExpectMore() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSendSmsExpectMore(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSendSmsExpectMore() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    P_ND_SEND_MSG pSendMsg = NULL;
    uint          uiMsgRef;

    pSendMsg = (P_ND_SEND_MSG)malloc(sizeof(S_ND_SEND_MSG));
    if (NULL == pSendMsg)
    {
        RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not allocate memory for a S_ND_SEND_MSG struct.\r\n");
        goto Error;
    }

    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not parse response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CMGS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not parse \"+CMGS: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, uiMsgRef, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not parse <msgRef>.\r\n");
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
            RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not parse <ackPdu>.\r\n");
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
        RIL_LOG_CRITICAL("ParseSendSmsExpectMore() - ERROR: Could not parse response postfix.\r\n");
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

    RIL_LOG_VERBOSE("ParseSendSmsExpectMore() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
//
RIL_RESULT_CODE CTEBase::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    PdpData stPdpData;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (6 * sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];  // not used
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];  // not used
    stPdpData.szPassword        = ((char **)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char **)pData)[5];  // not used

    (void)PrintStringNullTerminate(rReqData.szCmd1,
          sizeof(rReqData.szCmd1),
          "AT+CGDCONT=%d,\"IP\",\"%s\",,0,0;+CGQREQ=%d;+CGQMIN=%d;+CGACT=0,%d\r", uiCID,
          stPdpData.szApn, uiCID, uiCID, uiCID);
        
    (void)CopyStringNullTerminate(rReqData.szCmd2, "ATD*99***1#\r", sizeof(rReqData.szCmd2));
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetupDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTEBase::CoreSimIo(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSimIo() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO *   pSimIOArgs = NULL;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSimIo() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSimIo() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    // extract data
    pSimIOArgs = (RIL_SIM_IO *)pData;
    
    RIL_LOG_VERBOSE("CoreSimIo() - command=[0x%08x]  [%d]\r\n", pSimIOArgs->command, pSimIOArgs->command);
    RIL_LOG_VERBOSE("CoreSimIo() - fileid=[0x%08x]  [%d]\r\n", pSimIOArgs->fileid, pSimIOArgs->fileid);
    RIL_LOG_VERBOSE("CoreSimIo() - path=[%s]\r\n", (pSimIOArgs->path ? pSimIOArgs->path : "NULL") );
    RIL_LOG_VERBOSE("CoreSimIo() - p1=[0x%08x]  [%d]\r\n", pSimIOArgs->p1, pSimIOArgs->p1);
    RIL_LOG_VERBOSE("CoreSimIo() - p2=[0x%08x]  [%d]\r\n", pSimIOArgs->p2, pSimIOArgs->p2);
    RIL_LOG_VERBOSE("CoreSimIo() - p3=[0x%08x]  [%d]\r\n", pSimIOArgs->p3, pSimIOArgs->p3);
    RIL_LOG_VERBOSE("CoreSimIo() - data=[%s]\r\n", (pSimIOArgs->data ? pSimIOArgs->data : "NULL") );
    RIL_LOG_VERBOSE("CoreSimIo() - pin2=[%s]\r\n", (pSimIOArgs->pin2 ? pSimIOArgs->pin2 : "NULL") );
    
    //  If PIN2 is required, send out AT+CPIN2 request
    if (pSimIOArgs->pin2)
    {
        RIL_LOG_INFO("CoreSimIo() - PIN2 required\r\n");
        
        (void)PrintStringNullTerminate(rReqData.szCmd1,
                     sizeof(rReqData.szCmd1),
                     "AT+CPIN2=\"%s\"\r",
                     pSimIOArgs->pin2);
                     
        
        if (NULL == pSimIOArgs->data)
        {
            (void)PrintStringNullTerminate(rReqData.szCmd2,
                         sizeof(rReqData.szCmd2),
                         "AT+CRSM=%d,%d,%d,%d,%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3);
        }
        else
        {
            (void)PrintStringNullTerminate(rReqData.szCmd2,
                         sizeof(rReqData.szCmd2),
                         "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->data);
        }

        
        
    }
    else
    {
        //  No PIN2
            

        if (NULL == pSimIOArgs->data)
        {
            (void)PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3);
        }
        else
        {
            (void)PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->data);
        }
    }
    
    //  Set the context of this command to the SIM_IO command-type.
    rReqData.pContextData = (void*)pSimIOArgs->command;


    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSimIo() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSimIo() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint  uiSW1 = 0;
    uint  uiSW2 = 0;
    BYTE* szResponseString = NULL;
    UINT32  cbResponseString = 0;

    RIL_SIM_IO_Response* pResponse = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CRSM: <sw1>,<sw2>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CRSM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not skip over \"+CRSM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiSW2, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not extract SW2 value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("ParseSimIo() - Extracted SW1 = %u and SW2 = %u\r\n", uiSW1, uiSW2);

    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("ParseSimIo() - Extracted data string: \"%s\" (%u chars)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("ParseSimIo() : ERROR : String was not a multiple of 2.\r\n");
            goto Error;
        }
    }

    // Allocate memory for the response struct PLUS a buffer for the response string
    // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
    // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
    // Note: cbResponseString is the number of chars in the string, and does not include the NULL terminator.
    pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("ParseSimIo() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
        goto Error;
    }

    pResponse->sw1 = uiSW1;
    pResponse->sw2 = uiSW2;

    if (NULL == szResponseString)
    {
        pResponse->simResponse = NULL;
    }
    else
    {
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
        (void)CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString);

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

    RIL_LOG_VERBOSE("ParseSimIo() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTEBase::CoreSendUssd(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSendUssd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char *szUssdString = NULL;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSendUssd() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSendUssd() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    // extract data
    szUssdString = (char *)pData;

    (void)PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CUSD=1,\"%s\"\r", szUssdString);

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSendUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSendUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSendUssd() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSendUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
RIL_RESULT_CODE CTEBase::CoreCancelUssd(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreCancelUssd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CUSD=2\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreCancelUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCancelUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseCancelUssd() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseCancelUssd() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CLIR 31
//
RIL_RESULT_CODE CTEBase::CoreGetClir(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetClir() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CLIR?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetClir() - Enter\r\n");

    int * pCLIRBlob = NULL;

    UINT32 nValue;
    CRepository repository;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;
    
    pCLIRBlob = (int *)malloc(sizeof(int) * 2);

    if (NULL == pCLIRBlob)
    {
        RIL_LOG_CRITICAL("ParseGetClir() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }
    
    // Parse "<prefix>+CLIR: <status>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp)          ||
        !SkipString(szRsp, "+CLIR: ", szRsp) ||
        !ExtractUInt(szRsp, nValue, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetClir() - ERROR: Could not find status value\r\n");
        goto Error;
    }

    pCLIRBlob[0] = nValue;

    // cache this value because the radio does not persist it across reboots or radio resets
    g_dwLastCLIR = nValue;

    if (!repository.Write(g_szGroupLastValues, g_szLastCLIR, (int)g_dwLastCLIR))
    {
        RIL_LOG_WARNING("ParseGetClir() - WARN: Could not preserve Last CLIR value.\r\n");
    }
    else
    {
        RIL_LOG_VERBOSE("ParseGetClir() - Saved LastCLIR = 0x%X\r\n", g_dwLastCLIR);
    }

    // Parse ",<provisioning><postfix>"
    if (!SkipString(szRsp, ",", szRsp)     ||
        !ExtractUInt(szRsp, nValue, szRsp) ||
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

    RIL_LOG_VERBOSE("ParseGetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CLIR 32
//
RIL_RESULT_CODE CTEBase::CoreSetClir(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetClir() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnClir = NULL;
    
    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetClir() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetClir() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pnClir = (int*)pData;
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLIR=%u\r", pnClir[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetClir(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetClir() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetClir() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
RIL_RESULT_CODE CTEBase::CoreQueryCallForwardStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryCallForwardStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_CallForwardInfo* pCallFwdInfo = NULL;
    
    if (sizeof(RIL_CallForwardInfo) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreQueryCallForwardStatus() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreQueryCallForwardStatus() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pCallFwdInfo = (RIL_CallForwardInfo*)pData;

    if ((RRIL_INFO_CLASS_NONE == pCallFwdInfo->serviceClass) || 
        ((RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_DATA | RRIL_INFO_CLASS_FAX) == pCallFwdInfo->serviceClass))
    {
        if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                        sizeof(rReqData.szCmd1), 
                                        "AT+CCFC=%u,2\r", 
                                        pCallFwdInfo->reason))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                        sizeof(rReqData.szCmd1), 
                                        "AT+CCFC=%u,2,,,%u\r", 
                                        pCallFwdInfo->reason, 
                                        pCallFwdInfo->serviceClass))
        {
            res = RRIL_RESULT_OK;
        }
    }

Error:
    RIL_LOG_VERBOSE("CoreQueryCallForwardStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryCallForwardStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryCallForwardStatus() - Enter\r\n");

    P_ND_CALLFWD_DATA pCallFwdBlob = NULL;

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nEntries = 0;
    UINT32 nCur = 0;
    UINT32 nValue;
    char szAddress[MAX_BUFFER_SIZE];

    while (FindAndSkipString(szRsp, "+CCFC: ", szRsp))
    {
        nEntries++;
    }

    RIL_LOG_INFO("ParseQueryCallForwardStatus() - INFO: Found %d CCFC entries!\r\n", nEntries);

    if (RIL_MAX_CALLFWD_ENTRIES < nEntries)
    {
        RIL_LOG_CRITICAL("ParseQueryCallForwardStatus() - ERROR: Too many CCFC entries!\r\n");
        goto Error;
    }

    pCallFwdBlob = (P_ND_CALLFWD_DATA)malloc(sizeof(S_ND_CALLFWD_DATA));

    if (NULL == pCallFwdBlob)
    {
        RIL_LOG_CRITICAL("ParseQueryCallForwardStatus() - ERROR: Could not allocate memory for a S_ND_CALLFWD_DATA struct.\r\n");
        goto Error;
    }

    // Reset our buffer to the beginning of the response
    szRsp = rRspData.szResponse;

    // Parse "+CCFC: "
    while (FindAndSkipString(szRsp, "+CCFC: ", szRsp)) 
    {
        // Parse "<status>"
        if (!ExtractUInt(szRsp, nValue, szRsp)) 
        {
            RIL_LOG_WARNING("ParseQueryCallForwardStatus() - WARN: Could not find status value, skipping entry\r\n");
            goto Continue;
        }

        pCallFwdBlob->pCallFwdData[nCur].status = nValue;

        // Parse ",<serviceClass>"
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt(szRsp, nValue, szRsp)) 
        {
            RIL_LOG_WARNING("ParseQueryCallForwardStatus() - WARN: Could not find service class value, skipping entry\r\n");
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
                RIL_LOG_WARNING("ParseQueryCallForwardStatus() - WARN: Could not find address string, skipping entry\r\n");
                goto Continue;
            }

            //  Parse type if available.
            if (ExtractUpperBoundedUInt(szRsp, 0x100, nValue, szRsp))
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
                    RIL_LOG_WARNING("ParseQueryCallForwardStatus() - WARN: Couldn't find comma after subaddress, skipping entry\r\n");
                    goto Continue;
                }

                // Parse ","
                if (FindAndSkipString(szRsp, ",", szRsp)) 
                {
                    // Parse "<time>"
                    if (!ExtractUInt(szRsp, nValue, szRsp)) 
                    {
                        RIL_LOG_WARNING("ParseQueryCallForwardStatus() - WARN: Couldn't find comma after time, skipping entry\r\n");
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
            RIL_LOG_CRITICAL("ParseQueryCallForwardStatus() - ERROR: Could not find response end\r\n");
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

    RIL_LOG_VERBOSE("ParseQueryCallForwardStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
RIL_RESULT_CODE CTEBase::CoreSetCallForward(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetCallForward() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 nValue = 0;
    RIL_CallForwardInfo* pCallFwdInfo = NULL;
    
    if (sizeof(RIL_CallForwardInfo) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pCallFwdInfo = (RIL_CallForwardInfo*)pData;

    if (NULL != pCallFwdInfo->number)
    {
        RIL_LOG_INFO("CoreSetCallForward() - status: %d   info class: %d   reason: %d   number: %s   toa: %d   time: %d\r\n",
                        pCallFwdInfo->status,
                        pCallFwdInfo->serviceClass,
                        pCallFwdInfo->reason,
                        pCallFwdInfo->number,
                        pCallFwdInfo->toa,
                        pCallFwdInfo->timeSeconds);
    }
    else
    {
        RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Number cannot be NULL!\r\n");
        goto Error;
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
                                            pCallFwdInfo->number,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->serviceClass,
                                            pCallFwdInfo->timeSeconds))
            {
                RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
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
                                            pCallFwdInfo->number,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->serviceClass))
            {
                RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
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
                                            pCallFwdInfo->number,
                                            pCallFwdInfo->toa,
                                            pCallFwdInfo->timeSeconds))
            {
                RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
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
                                            pCallFwdInfo->number,
                                            pCallFwdInfo->toa))
            {
                RIL_LOG_CRITICAL("CoreSetCallForward() - ERROR: Unable to write command to buffer\r\n");
                goto Error;
            }            
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreSetCallForward() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetCallForward(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("Parse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("Parse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
RIL_RESULT_CODE CTEBase::CoreQueryCallWaiting(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryCallWaiting() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnInfoClasses = NULL;
    
    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreQueryCallWaiting() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreQueryCallWaiting() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pnInfoClasses = (int*)pData;
    
    if ((RRIL_INFO_CLASS_NONE == pnInfoClasses[0]) || 
        ((RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_DATA | RRIL_INFO_CLASS_FAX) == pnInfoClasses[0]))
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, 
                        sizeof(rReqData.szCmd1), 
                        "AT+CCWA=1,2\r"))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, 
                        sizeof(rReqData.szCmd1), 
                        "AT+CCWA=1,2,%u\r", 
                        pnInfoClasses[0]))
        {
            res = RRIL_RESULT_OK;
        }
    }
    
Error:
    RIL_LOG_VERBOSE("CoreQueryCallWaiting() - Exit\r\n");
    return res;
    
}

RIL_RESULT_CODE CTEBase::ParseQueryCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * prgnCallWaiting = NULL;
    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nStatus;
    UINT32 nClass;
    UINT32 dwServiceInfo = 0, dwStatus = 0;
    
    // Parse "+CCWA: "
    while (FindAndSkipString(szRsp, "+CCWA: ", szRsp)) 
    {
        // Parse "<status>,<class>"
        if (!ExtractUInt(szRsp, nStatus, szRsp) ||
            !SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt(szRsp, nClass, szRsp)) 
        {
            RIL_LOG_WARNING("ParseQueryCallWaiting() - WARN: Unable to extract UINTS, skip to next entry\r\n");
            goto Continue;
        }

        RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: Status= %d    Class=%d\r\n", nStatus, nClass);

        if (1 == nStatus)
        {
#if 0
            // Temporary fix for Android 1.5 - Only shows UI for voice infoclass. Don't say active if voice is not active.
            if (1 == nClass)
            {
                RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: Setting CW Active as voice service is set\r\n", nClass);
                dwStatus = 1;
            }
#else
            dwStatus = 1;
#endif
            dwServiceInfo |= nClass;

            RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: Recording service %d. Current mask: %d\r\n", nClass, dwServiceInfo);
        }

Continue:
        // Find "<postfix>"
        if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp)) 
        {
            RIL_LOG_CRITICAL("ParseQueryCallWaiting() - ERROR: Unable to find response end\r\n");
            goto Error;
        }
    }

    RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: No more +CCWA strings found, building result struct\r\n");

    // If we have an active infoclass, we return 2 ints.
    if (1 == dwStatus)
    {
        RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: Returning 2 ints\r\n");
        rRspData.uiDataSize = 2 * sizeof(int*);

        prgnCallWaiting = (int *)malloc(rRspData.uiDataSize);
        if (!prgnCallWaiting) 
        {
            RIL_LOG_CRITICAL("ParseQueryCallWaiting() - ERROR: Could not allocate memory for response.\r\n");
            goto Error;
        }

        prgnCallWaiting[0] = dwStatus;
        prgnCallWaiting[1] = dwServiceInfo;
    }
    else
    {
        RIL_LOG_INFO("ParseQueryCallWaiting() - INFO: Returning 1 int\r\n");
        rRspData.uiDataSize = sizeof(int);

        prgnCallWaiting = (int *)malloc(rRspData.uiDataSize);
        if (!prgnCallWaiting) 
        {
            RIL_LOG_CRITICAL("ParseQueryCallWaiting() - ERROR: Could not allocate memory for response.\r\n");
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

    RIL_LOG_VERBOSE("ParseQueryCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
RIL_RESULT_CODE CTEBase::CoreSetCallWaiting(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetCallWaiting() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nStatus;
    UINT32 nInfoClasses;
    
    if ((2 * sizeof(int *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetCallWaiting() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetCallWaiting() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    nStatus = ((int *)pData)[0];
    nInfoClasses = ((int *)pData)[1];
    
    if (RRIL_INFO_CLASS_NONE == nInfoClasses)
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCWA=1,%u\r", nInfoClasses))
        {
            res = RRIL_RESULT_OK;
        }
    }
    else
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CCWA=1,%u,%u\r", nStatus, nInfoClasses))
        {
            res = RRIL_RESULT_OK;
        }        
    }
    
Error:
    RIL_LOG_VERBOSE("CoreSetCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetCallWaiting(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetCallWaiting() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetCallWaiting() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE 37
//
RIL_RESULT_CODE CTEBase::CoreSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSmsAcknowledge() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    BOOL fSuccess = FALSE;
    int * pnAckData = NULL;

    if ((sizeof(int *) != uiDataSize) && ((2 * sizeof(int *)) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CoreSmsAcknowledge() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSmsAcknowledge() - ERROR: Passed data pointer was NULL\r\n");
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
    RIL_LOG_VERBOSE("CoreSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSmsAcknowledge() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSmsAcknowledge() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEI 38
//
RIL_RESULT_CODE CTEBase::CoreGetImei(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetImei() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGSN\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGetImei() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImei(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetImei() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    char * szSerialNumber = (char*)malloc(MAX_BUFFER_SIZE);

    if (NULL == szSerialNumber)
    {
        RIL_LOG_CRITICAL("ParseGetImei() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }

    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImei() - ERROR: Could not find response start\r\n");
        goto Error;
    }
    
    if (!ExtractUnquotedString(szRsp, g_cTerminator, szSerialNumber, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImei() - ERROR: Could not find unquoted string\r\n");
        goto Error;
    }
    
    if (!SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImei() - ERROR: Could not find response end\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)szSerialNumber;
    rRspData.uiDataSize  = sizeof(char*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szSerialNumber);
        szSerialNumber = NULL;
    }

    RIL_LOG_VERBOSE("ParseGetImei() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_IMEISV 39
//
RIL_RESULT_CODE CTEBase::CoreGetImeisv(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetImeisv() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGSN\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreGetImeisv() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetImeisv(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetImeisv() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    char * szSerialNumber = (char*)malloc(MAX_BUFFER_SIZE);

    if (NULL == szSerialNumber)
    {
        RIL_LOG_CRITICAL("ParseGetImeisv() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }

    if (!SkipRspStart(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImeisv() - ERROR: Could not find response start\r\n");
        goto Error;
    }
    
    if (!ExtractUnquotedString(szRsp, g_cTerminator, szSerialNumber, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImeisv() - ERROR: Could not find unquoted string\r\n");
        goto Error;
    }
    
    if (!SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetImeisv() - ERROR: Could not find response end\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)szSerialNumber;
    rRspData.uiDataSize  = sizeof(char*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szSerialNumber);
        szSerialNumber = NULL;
    }

    RIL_LOG_VERBOSE("ParseGetImeisv() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_ANSWER 40
//
RIL_RESULT_CODE CTEBase::CoreAnswer(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreAnswer() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "ATA\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreAnswer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseAnswer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseAnswer() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseAnswer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTEBase::CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszCid = 0;
    char chCid = '1';

    if (sizeof(char **) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pszCid = ((char**)pData)[0];    
    chCid = pszCid[0];
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%c;+CGDCONT=%c\r", chCid, chCid))
    {
        res = RRIL_RESULT_OK;
    }
    
    //  Set the context of this command to the CID (for multiple context support).
    rReqData.pContextData = (void*)((int)(chCid - '0'));  // Store this as an int.


Error:
    RIL_LOG_VERBOSE("CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
RIL_RESULT_CODE CTEBase::CoreQueryFacilityLock(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryFacilityLock() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszFacility = NULL;
    char * pszPassword = NULL;
    char * pszClass = NULL;
    
    if ((3 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreQueryFacilityLock() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreQueryFacilityLock() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pszFacility = ((char **)pData)[0];
    pszPassword = ((char **)pData)[1];
    pszClass    = ((char **)pData)[2];

    if (NULL == pszFacility)
    {
        RIL_LOG_CRITICAL("CoreQueryFacilityLock() - ERROR: Facility string pointer is NULL.\r\n");
        goto Error;
    }
    
    if (NULL == pszPassword)
    {
        RIL_LOG_CRITICAL("CoreQueryFacilityLock() - ERROR: Password string pointer is NULL.\r\n");
        goto Error;
    }
    
    if (NULL == pszClass)
    {
        RIL_LOG_CRITICAL("CoreQueryFacilityLock() - ERROR: Class string pointer is NULL.\r\n");
        goto Error;
    }

    if ('\0' == pszPassword[0])
    {
        if ('\0' == pszClass[0])
        {
            if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                            sizeof(rReqData.szCmd1), 
                                            "AT+CLCK=\"%s\",2\r", 
                                            pszFacility))
            {
                res = RRIL_RESULT_OK;
            }
        }
        else
        {
            if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                            sizeof(rReqData.szCmd1), 
                                            "AT+CLCK=\"%s\",2,,%s\r", 
                                            pszFacility, 
                                            pszClass))
            {
                res = RRIL_RESULT_OK;
            }
        }
    }
    else
    {
        if ('\0' == pszClass[0])
        {
            if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                            sizeof(rReqData.szCmd1), 
                                            "AT+CLCK=\"%s\",2,\"%s\"\r", 
                                            pszFacility, 
                                            pszPassword))
            {
                res = RRIL_RESULT_OK;
            }
        }
        else
        {
            if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                            sizeof(rReqData.szCmd1), 
                                            "AT+CLCK=\"%s\",2,\"%s\",%s\r", 
                                            pszFacility, 
                                            pszPassword, 
                                            pszClass))
            {
                res = RRIL_RESULT_OK;
            }
        }
    }

Error:
    RIL_LOG_VERBOSE("CoreQueryFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryFacilityLock() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnClass = NULL;
    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nClass;
    
    pnClass = (int*)malloc(sizeof(int));
    if (NULL == pnClass)
    {
        RIL_LOG_CRITICAL("ParseQueryFacilityLock() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CLCK: <status><postfix>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp)           ||
        !SkipString(szRsp, "+CLCK: ", szRsp)  ||
        !ExtractUInt(szRsp, nClass, szRsp)  ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryFacilityLock() - ERROR: Could not extract Locking Status.\r\n");
        goto Error;
    }

    *pnClass = nClass;
    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pnClass;
    rRspData.uiDataSize  = sizeof(int*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnClass);
        pnClass = NULL;
    }

    RIL_LOG_VERBOSE("ParseQueryFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
RIL_RESULT_CODE CTEBase::CoreSetFacilityLock(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetFacilityLock() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char ** ppszFacility = NULL;
    
    if ((4 * sizeof(char *)) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetFacilityLock() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetFacilityLock() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    ppszFacility = (char **)pData;

    if ((NULL == ppszFacility[0]) || (NULL == ppszFacility[1]))
    {
        RIL_LOG_CRITICAL("CoreSetFacilityLock() - ERROR: Facility or Mode strings were NULL\r\n");
        goto Error;
    }
    // Facility and Mode provided
    else if (NULL == ppszFacility[2])
    {
        if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                        sizeof(rReqData.szCmd1), 
                                        "AT+CLCK=\"%s\",%s\r", 
                                        ppszFacility[0], 
                                        ppszFacility[1]))
        {
            res = RRIL_RESULT_OK;
        }
    }
    // Password provided
    else if (NULL == ppszFacility[3])
    {
        if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                        sizeof(rReqData.szCmd1), 
                                        "AT+CLCK=\"%s\",%s,\"%s\"\r", 
                                        ppszFacility[0], 
                                        ppszFacility[1], 
                                        ppszFacility[2]))
        {
            res = RRIL_RESULT_OK;
        }
    }
    // Class provided
    else if (PrintStringNullTerminate(  rReqData.szCmd1, 
                                        sizeof(rReqData.szCmd1), 
                                        "AT+CLCK=\"%s\",%s,\"%s\",%s\r", 
                                        ppszFacility[0], 
                                        ppszFacility[1], 
                                        ppszFacility[2], 
                                        ppszFacility[3]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetFacilityLock(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetFacilityLock() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int *pnRetries = NULL;
    
    pnRetries = (int*)malloc(sizeof(int));
    if (NULL == pnRetries)
    {
        RIL_LOG_CRITICAL("ParseSetFacilityLock() - ERROR: Could not alloc int\r\n");
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

    RIL_LOG_VERBOSE("ParseSetFacilityLock() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
RIL_RESULT_CODE CTEBase::CoreChangeBarringPassword(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreChangeBarringPassword() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char * pszFacility;
    const char * pszOldPassword;
    const char * pszNewPassword;

    if ((3 * sizeof(char *) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CoreChangeBarringPassword() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreChangeBarringPassword() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
     pszFacility    = ((const char **)pData)[0];
     pszOldPassword = ((const char **)pData)[1];
     pszNewPassword = ((const char **)pData)[2];
    
    if (PrintStringNullTerminate(   rReqData.szCmd1, 
                                    sizeof(rReqData.szCmd1), 
                                    "AT+CPWD=\"%s\",\"%s\",\"%s\"\r"), 
                                    pszFacility, 
                                    pszOldPassword, 
                                    pszNewPassword)
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreChangeBarringPassword() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseChangeBarringPassword(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseChangeBarringPassword() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseChangeBarringPassword() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTEBase::CoreQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryNetworkSelectionMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryNetworkSelectionMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnMode = NULL;
    const BYTE* szRsp = rRspData.szResponse;

    pnMode = (int*)malloc(sizeof(int));
    if (NULL == pnMode)
    {
        RIL_LOG_CRITICAL("ParseQueryNetworkSelectionMode() - ERROR: Could not allocate memory for an int.\r\n");
        goto Error;
    }

    if (!FindAndSkipString(szRsp, "+COPS: ", szRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryNetworkSelectionMode() - ERROR: Could not find +COPS:\r\n");
        goto Error;
    }
    
    if (!ExtractUInt(szRsp, (UINT32&)*pnMode, szRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryNetworkSelectionMode() - ERROR: Could not extract the mode.\r\n");
        goto Error;
    }
    
    //  If we have a +COPS: 2, then just default to automatic.
    if (*pnMode >= 2)
    {
        *pnMode = 0; // automatic
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pnMode;
    rRspData.uiDataSize  = sizeof(int *);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnMode);
        pnMode = NULL;
    }

    RIL_LOG_VERBOSE("ParseQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
RIL_RESULT_CODE CTEBase::CoreSetNetworkSelectionAutomatic(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetNetworkSelectionAutomatic() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=0\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetNetworkSelectionAutomatic() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
RIL_RESULT_CODE CTEBase::CoreSetNetworkSelectionManual(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetNetworkSelectionManual() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* pszNumeric = NULL;

    if (sizeof(char *) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetNetworkSelectionManual() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetNetworkSelectionManual() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pszNumeric = (char *)pData;
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=1,2,\"%s\"\r", pszNumeric))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetNetworkSelectionManual() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTEBase::CoreQueryAvailableNetworks(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryAvailableNetworks() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+COPS=?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreQueryAvailableNetworks() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryAvailableNetworks(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryAvailableNetworks() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nValue;
    UINT32 nEntries = 0;
    UINT32 nCurrent = 0;
    //WCHAR tmp[MAX_BUFFER_SIZE];
    BYTE tmp[MAX_BUFFER_SIZE];

    P_ND_OPINFO_PTRS pOpInfoPtr = NULL;
    P_ND_OPINFO_DATA pOpInfoData = NULL;

    void * pOpInfoPtrBase = NULL;
    void * pOpInfoDataBase = NULL;

    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* szDummy = NULL;
    
    // Skip "<prefix>+COPS: "
    SkipRspStart(szRsp, g_szNewLine, szRsp);
    
    if (!FindAndSkipString(szRsp, "+COPS: ", szRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryAvailableNetworks() - ERROR: Unable to find +COPS: in response\r\n");
        goto Error;
    }

    szDummy = szRsp;

    while (FindAndSkipString(szDummy, "(", szDummy))
    {
        nEntries++;
    }

    RIL_LOG_VERBOSE("ParseQueryAvailableNetworks() - DEBUG: Found %d entries. Allocating memory...\r\n", nEntries);

    rRspData.pData = malloc(nEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
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
        if (!ExtractUInt(szRsp, nValue, szRsp))
        {
            RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Unable to extract status\r\n");
            goto Error;
        }

        switch(nValue)
        {
            case 0:
            {
                const BYTE* szTemp = "unknown";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 1:
            {
                const BYTE* szTemp = "available";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 2:
            {
                const BYTE* szTemp = "current";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            case 3:
            {
                const BYTE* szTemp = "forbidden";
                strcpy(pOpInfoData[nCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[nCurrent].pszOpInfoStatus = pOpInfoData[nCurrent].szOpInfoStatus;
                break;
            }

            default:
            {
                RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Invalid status found: %d\r\n", nValue);
                goto Error;
            }
        }

        // Extract ",<long_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           //(!ExtractQuotedUnicodeHexStringToUnicode(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Could not extract the Long Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            //int len = wcslen(tmp);
            int len = strlen(tmp);
            if (0 == len)
            {
                strcpy(pOpInfoData[nCurrent].szOpInfoLong, "<UNKNOWN>");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoLong[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoLong[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoLong = pOpInfoData[nCurrent].szOpInfoLong;
            RIL_LOG_INFO("ParseGetOperatorList() - Long oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoLong);
        }

        // Extract ",<short_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           //(!ExtractQuotedUnicodeHexStringToUnicode(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Could not extract the Short Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            //int len = wcslen(tmp);
            int len = strlen(tmp);
            if (0 == len)
            {
                strcpy(pOpInfoData[nCurrent].szOpInfoShort, "<UNKNOWN>");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoShort[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoShort[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoShort = pOpInfoData[nCurrent].szOpInfoShort;
            RIL_LOG_INFO("ParseGetOperatorList() - Short oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoShort);
       }

        // Extract ",<num_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           //(!ExtractQuotedUnicodeHexStringToUnicode(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
           (!ExtractQuotedString(szRsp, tmp, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Could not extract the Numeric Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            //int len = wcslen(tmp);
            int len = strlen(tmp);
            if (0 == len)
            {
                strcpy(pOpInfoData[nCurrent].szOpInfoNumeric, "<UNKNOWN>");
            }
            else
            {
                int i;
                for (i = 0; tmp[i]; pOpInfoData[nCurrent].szOpInfoNumeric[i] = (char) tmp[i], ++i);
                pOpInfoData[nCurrent].szOpInfoNumeric[i] = '\0';
            }

            pOpInfoPtr[nCurrent].pszOpInfoNumeric = pOpInfoData[nCurrent].szOpInfoNumeric;
            RIL_LOG_INFO("ParseGetOperatorList() - Numeric oper: %s\r\n", pOpInfoData[nCurrent].szOpInfoNumeric);
       }

        //Extract ")"
        if (!FindAndSkipString(szRsp, ")", szRsp))
        {
            RIL_LOG_CRITICAL("ParseGetOperatorList() - ERROR: Did not find closing bracket\r\n");
            goto Error;
        }

        // Increment the array index
        nCurrent++;

Continue:
        // Extract ","
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_INFO("ParseGetOperatorList() - INFO: Finished parsing entries\r\n");
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

    RIL_LOG_VERBOSE("ParseQueryAvailableNetworks() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_START 49
//
RIL_RESULT_CODE CTEBase::CoreDtmfStart(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDtmfStart() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char cTone;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreDtmfStart() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreDtmfStart() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    cTone = ((char*)pData)[0];
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+VTS=%c,5\r", cTone))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreDtmfStart() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDtmfStart(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDtmfStart() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDtmfStart() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DTMF_STOP 50
//
RIL_RESULT_CODE CTEBase::CoreDtmfStop(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTEBase::ParseDtmfStop(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDtmfStop() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDtmfStop() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTEBase::CoreBasebandVersion(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreBasebandVersion() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CGMR\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CoreBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseBasebandVersion(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    char* szModelId = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == szModelId)
    {
        RIL_LOG_CRITICAL("ParseBasebandVersion() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }
    memset(szModelId, 0x00, MAX_BUFFER_SIZE);

    // Parse "<prefix><serial_number><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !ExtractUnquotedString(pszRsp, g_cTerminator, szModelId, MAX_BUFFER_SIZE,pszRsp) ||
        !SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseBasebandVersion() - ERROR: Could not extract the IMSI string.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)szModelId;
    rRspData.uiDataSize  = sizeof(char*);
    
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(szModelId);
        szModelId = NULL;
    }

    RIL_LOG_VERBOSE("ParseBasebandVersion() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
RIL_RESULT_CODE CTEBase::CoreSeparateConnection(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSeparateConnection() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int callId = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSeparateConnection() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    callId = ((int *)pData)[0];
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CHLD=2%u\r", callId))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSeparateConnection() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSeparateConnection(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSeparateConnection() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSeparateConnection() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTEBase::CoreSetMute(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetMute() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nEnable = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetMute() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nEnable = ((int *)pData)[0];
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMUT=%d\r", (nEnable ? 1 : 0) ))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetMute() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_MUTE 54
//
RIL_RESULT_CODE CTEBase::CoreGetMute(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetMute() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CMUT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreGetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetMute() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    int * pMuteVal = NULL;
    UINT32 nValue = 0;

    pMuteVal = (int*)malloc(sizeof(int));
    if (!pMuteVal) 
    {
        RIL_LOG_CRITICAL("ParseGetMute() - ERROR : Can't allocate an int.\r\n");
        goto Error;
    }
    memset(pMuteVal, 0, sizeof(int));


    // Parse "<prefix>+CMUT: <enabled><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp)                         ||
        !SkipString(pszRsp, "+CMUT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetMute() - ERROR : Can't parse prefix.\r\n");
        goto Error;
    }
    
    if (!ExtractUpperBoundedUInt(pszRsp, 2, nValue, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseGetMute() - ERROR : Can't parse nValue.\r\n");
        goto Error;
    }
    *pMuteVal = nValue;
    
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))  
    {
        RIL_LOG_CRITICAL("ParseGetMute() - ERROR : Can't parse postfix.\r\n");
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

    RIL_LOG_VERBOSE("ParseGetMute() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
RIL_RESULT_CODE CTEBase::CoreQueryClip(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryClip() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CLIP?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreQueryClip() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryClip(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryClip() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    int * pClipVal = NULL;
    UINT32 nValue = 0;

    pClipVal = (int*)malloc(sizeof(int));
    if (!pClipVal) 
    {
        RIL_LOG_CRITICAL("ParseQueryClip() - ERROR : Can't allocate an int.\r\n");
        goto Error;
    }
    memset(pClipVal, 0, sizeof(int));


    // Parse "<prefix>+CLIP: <n>,<value><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp)                         ||
        !SkipString(pszRsp, "+CLIP: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryClip() - ERROR : Can't parse prefix.\r\n");
        goto Error;
    }
    
    if (!ExtractUInt(pszRsp, nValue, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseQueryClip() - ERROR : Can't parse nValue1.\r\n");
        goto Error;
    }
    
    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt(pszRsp, nValue, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseQueryClip() - ERROR : Can't parse nValue2.\r\n");
            goto Error;
        }
    }
    *pClipVal = nValue;
    
    // cache this value because the radio does not persist it across reboots or radio resets
    g_dwLastCLIP = nValue;
    // TODO: Store this in NV memory
    RIL_LOG_INFO("ParseQueryClip: Saved LastCLIP=0x%x\r\n", g_dwLastCLIP);
    
    
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))  
    {
        RIL_LOG_CRITICAL("ParseQueryClip() - ERROR : Can't parse postfix.\r\n");
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

    RIL_LOG_VERBOSE("ParseQueryClip() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
RIL_RESULT_CODE CTEBase::CoreLastDataCallFailCause(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreLastDataCallFailCause() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CEER\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreLastDataCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseLastDataCallFailCause(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseLastDataCallFailCause() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint      uiCause  = 0;
    int*      pCause   = NULL;
    char      szDummy[MAX_BUFFER_SIZE];

    pCause= (int*) malloc(sizeof(int));
    if (NULL == pCause)
    {
        RIL_LOG_CRITICAL("ParseLastDataCallFailCause() - ERROR: Could not allocate memory for an integer.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CEER: <string><value><string><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CEER: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseLastDataCallFailCause() - ERROR: Could not find AT response.\r\n");
        goto Error;
    }

    // skip first string
    // TODO: should check if this is indeed for a call?
    if (!ExtractQuotedString(pszRsp, szDummy, MAX_BUFFER_SIZE, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseLastDataCallFailCause() - ERROR: Could not find first string.\r\n");
        goto Error;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Get failure cause
        if (!ExtractUInt(pszRsp, uiCause, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseLastDataCallFailCause() - ERROR: Could not extract failure cause.\r\n");
            goto Error;
        }

        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, szDummy, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_WARNING("ParseLastDataCallFailCause() - WARNING: Could not extract verbose cause.\r\n");
        }
        else if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_WARNING("ParseLastDataCallFailCause() - WARNING: Could not extract RspEnd.\r\n");
        }
    }
    else if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_WARNING("ParseLastDataCallFailCause() - WARNING: Could not extract RspEnd.\r\n");
    }

    *pCause= (int) uiCause;
    
    rRspData.pData    = (void*) pCause;
    rRspData.uiDataSize   = sizeof(int*);
    
    RIL_LOG_INFO("ParseLastDataCallFailCause() - Last call fail cause [%d]\r\n", uiCause);
    
    res = RRIL_RESULT_OK;        

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCause);
        pCause = NULL;
    }        

    RIL_LOG_VERBOSE("ParseLastDataCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
RIL_RESULT_CODE CTEBase::CoreDataCallList(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDataCallList() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CGACT?;+CGDCONT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreDataCallList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDataCallList(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDataCallList() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    P_ND_PDP_CONTEXT_DATA pPDPListData = NULL;
    UINT32 nValue;
    UINT32 nCID;
    int  count;
    int active[MAX_PDP_CONTEXTS];

    pPDPListData = (P_ND_PDP_CONTEXT_DATA)malloc(sizeof(S_ND_PDP_CONTEXT_DATA));
    if (NULL == pPDPListData)
    {
        RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not allocate memory for a P_ND_PDP_CONTEXT_DATA struct.\r\n");
        goto Error;
    }
    memset(pPDPListData, 0, sizeof(S_ND_PDP_CONTEXT_DATA));
    memset(active, 0, sizeof(active));

    // Parse +CGACT response
    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Couldn't parse first prefix.\r\n");
        goto Error;
    }
    // Parse "+CGACT: "
    while (SkipString(pszRsp, "+CGACT: ", pszRsp))
    {
        // Parse <state>
        if (!ExtractUpperBoundedUInt(pszRsp, 2, nValue, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Invalid state.\r\n");
            goto Error;
        }

        // Parse <cid>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt(pszRsp, nCID, pszRsp) || (nCID > MAX_PDP_CONTEXTS))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Invalid CID.\r\n");
            goto Error;
        }
        active[nCID - 1] = nValue;
        RIL_LOG_INFO("ParseDataCallList() - Context %d %s.\r\n", nCID, (nValue ? "active" : "not active") );

        // Find "<postfix>"
        if (!FindAndSkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not find terminator.\r\n");
            goto Error;
        }
    }

    // Parse +CGDCONT response
    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Couldn't parse second prefix.\r\n");
        goto Error;
    }
    // Parse "+CGDCONT: "
    count = 0;
    while (SkipString(pszRsp, "+CGDCONT: ", pszRsp))
    {
        // Parse <cid>
        if (!ExtractUInt(pszRsp, nCID, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not extract CID.\r\n");
            goto Error;
        }
        pPDPListData->pPDPData[count].cid = nCID;

        // set active flag
        pPDPListData->pPDPData[count].active = active[nCID - 1];

        // Parse ,<PDP_type>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, pPDPListData->pTypeBuffers[count], MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not extract PDP type.\r\n");
            goto Error;
        }
        pPDPListData->pPDPData[count].type = pPDPListData->pTypeBuffers[count];

        // Parse ,<APN>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, pPDPListData->pApnBuffers[count], MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        pPDPListData->pPDPData[count].apn = pPDPListData->pApnBuffers[count];

        // Parse ,<PDP_addr>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractQuotedString(pszRsp, pPDPListData->pAddressBuffers[count], MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not extract APN.\r\n");
            goto Error;
        }
        pPDPListData->pPDPData[count].address = pPDPListData->pAddressBuffers[count];

        // Parse ,<data_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("ParseDataCallList() - WARNING: Could not extract data comp.\r\n");
            goto Continue;
        }

        // Parse ,<head_comp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUpperBoundedUInt(pszRsp, 0x2, nValue, pszRsp))
        {
            RIL_LOG_WARNING("ParseDataCallList() - WARNING: Could not extract header comp.\r\n");
            goto Continue;
        }

        // following we could have ,<pd1>[,...[,pdN]]
        // but Android does not care about extra parameters
        // so skip them all
Continue:
        // Find "<postfix>"
        if (!FindAndSkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("ParseDataCallList() - ERROR: Could not find terminator.\r\n");
            goto Error;
        }

        // entry complete, set-up pointers and increase counter
        pPDPListData->pPDPPointers[count] = &(pPDPListData->pPDPData[count]);
        ++count;
    }

    if (count > 0)
    {
        rRspData.pData  = (void*) pPDPListData;
        rRspData.uiDataSize = count * sizeof(RIL_Data_Call_Response);
    }
    else
    {
        free(pPDPListData);
        pPDPListData = NULL;
        rRspData.pData  = NULL;
        rRspData.uiDataSize = 0;
    }
    res = RRIL_RESULT_OK;
    RIL_LOG_INFO("ParseDataCallList() - Parse complete, found [%d] contexts.\r\n", count);
    
#if defined(DEBUG)
    for (int i = 0; i < count; ++i)
    {
        RIL_LOG_INFO("\t Context %d\r\n", i);
        RIL_LOG_INFO("\t\t CID:\t\t %d\r\n", pPDPListData->pPDPData[i].cid);
        RIL_LOG_INFO("\t\t Active:\t %d\r\n", pPDPListData->pPDPData[i].active);
        RIL_LOG_INFO("\t\t Type:\t\t %s\r\n", pPDPListData->pPDPData[i].type);
        RIL_LOG_INFO("\t\t APN:\t\t %s\r\n", pPDPListData->pPDPData[i].apn);
        RIL_LOG_INFO("\t\t Address:\t %s\r\n", pPDPListData->pPDPData[i].address);
    }
#endif  // DEBUG

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pPDPListData);
        pPDPListData = NULL;
    }        

    RIL_LOG_VERBOSE("ParseLastDataCallFailCause() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_RESET_RADIO 58
//
RIL_RESULT_CODE CTEBase::CoreResetRadio(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreResetRadio() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;
    
    RIL_LOG_VERBOSE("CoreResetRadio() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseResetRadio(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseResetRadio() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseResetRadio() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTEBase::CoreHookRaw(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreHookRaw() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;
    
    RIL_LOG_VERBOSE("CoreHookRaw() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHookRaw(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseHookRaw() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseHookRaw() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
RIL_RESULT_CODE CTEBase::CoreGetVersion(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetVersion() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    rReqData.fForceParse = TRUE;

    RIL_LOG_VERBOSE("CoreGetVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetVersion(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    P_ND_OEM_HOOK_GET_VERSION_STATUS pStatus = NULL;

    pStatus = (P_ND_OEM_HOOK_GET_VERSION_STATUS)malloc(sizeof(S_ND_OEM_HOOK_GET_VERSION_STATUS));
    if (NULL == pStatus)
    {
        RIL_LOG_CRITICAL("ParseGetVersion() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // fill data
    // TODO: get the right data here
    strcpy(pStatus->szRilVersion, "android rapid-ril 1.0");
    strcpy(pStatus->szModemAddr, "Modem address TBD");
    strcpy(pStatus->szHwSupport, "Hardware Support TBD");
    strcpy(pStatus->szReleasedTo, "Released to TBD");

    // set pointers
    pStatus->sStatusPointers.pszRilVersion = pStatus->szRilVersion;
    pStatus->sStatusPointers.pszModemAddr = pStatus->szModemAddr;
    pStatus->sStatusPointers.pszHwSupport= pStatus->szHwSupport;
    pStatus->sStatusPointers.pszReleasedTo= pStatus->szReleasedTo;

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pStatus;
    rRspData.uiDataSize  = sizeof(char*) * 4;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pStatus);
        pStatus = NULL;
    }

    RIL_LOG_VERBOSE("ParseGetVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::CoreGetRxGain(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("Core() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
   
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLVL?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("Core() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetRxGain(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetRxGain() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* szEnd = NULL;
    UINT32 uiValue = 0;

    P_ND_OEM_HOOK_GET_RXGAIN_DATA pRxGainData = (P_ND_OEM_HOOK_GET_RXGAIN_DATA)malloc(sizeof(S_ND_OEM_HOOK_GET_RXGAIN_DATA));
    if (NULL == pRxGainData)
    {
        RIL_LOG_CRITICAL("ParseGetRxGain() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Align the pointer with the actual array
    pRxGainData->sRxGainPointers.pszRxGain = pRxGainData->szRxGain;

    // Parse "<prefix>+CLVL: <value>"
    if (!FindAndSkipString(szRsp, "+CLVL: ", szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetRxGain() - ERROR: Could not find +CLVL.\r\n");
        goto Error;
    }

    if (!ExtractUInt(szRsp, uiValue, szEnd))
    {
        RIL_LOG_CRITICAL("ParseGetRxGain() - ERROR: Could not find value.\r\n");
        goto Error;    
    }

    strncat(pRxGainData->szRxGain, szRsp, (UINT32)szEnd - (UINT32)szRsp);

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pRxGainData;
    rRspData.uiDataSize  = sizeof(S_ND_OEM_HOOK_GET_RXGAIN_POINTER);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRxGainData);
        pRxGainData = NULL;
    }


    RIL_LOG_VERBOSE("ParseGetRxGain() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::CoreSetRxGain(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetRxGain() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (2 * sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetRxGain() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetRxGain() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CLVL=%s\r", ((char*)pData)[1]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetRxGain() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetRxGain(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetRxGain() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetRxGain() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::CoreHookStrings(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreHookStrings() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CoreHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseHookStrings(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseHookStrings() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseHookStrings() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTEBase::CoreScreenState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreScreenState() - Enter\r\n");
    
    // TODO : Mark flags for unsol updates for LAC, CID, CSQ to not send updates
    //        when screen is off. Resume updates when screen turns back on.
    //        will probably need to trigger immediate updates on resume to have correct
    //        info on screen. For now, just return OK and keep throwing updates.
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nEnable = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreScreenState() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nEnable = ((int *)pData)[0];
    
    //  0 is off
    //  1 is on
    switch(nEnable)
    {
        case 0:
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=0;+CGREG=0\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
            break;
        case 1:
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=2;+CGREG=2\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
            break;
        default:
            RIL_LOG_CRITICAL("CoreScreenState() - ERROR: unknown nEnable=[%d]\r\n", nEnable);
            break;
    }

Error:
    RIL_LOG_VERBOSE("CoreScreenState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseScreenState(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseScreenState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseScreenState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
RIL_RESULT_CODE CTEBase::CoreSetSuppSvcNotification(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetSuppSvcNotification() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetSuppSvcNotification() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetSuppSvcNotification() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSSN=%u,%u\r", ((int *)pData)[0], ((int *)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetSuppSvcNotification() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetSuppSvcNotification(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetSuppSvcNotification() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetSuppSvcNotification() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
RIL_RESULT_CODE CTEBase::CoreWriteSmsToSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreWriteSmsToSim() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SMS_WriteArgs* pSmsArgs;
    UINT32 nPDULength;
    
    if (sizeof(RIL_SMS_WriteArgs) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreWriteSmsToSim() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreWriteSmsToSim() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pSmsArgs = (RIL_SMS_WriteArgs*)pData;

    // 2 chars per byte.
    nPDULength = (strlen(pSmsArgs->pdu) / 2);

    if ((PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGW=%u,%u\r", nPDULength, pSmsArgs->status)) &&
        (PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "%s\x1a"), pSmsArgs->pdu))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreWriteSmsToSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseWriteSmsToSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseWriteSmsToSim() - Enter\r\n");

    int * pIndex = NULL;
    const BYTE* szRsp = rRspData.szResponse;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    pIndex = (int*)malloc(sizeof(int));
    if (NULL == pIndex)
    {
        RIL_LOG_CRITICAL("ParseWriteSmsToSim() - ERROR: Unable to allocate memory for int\r\n");
        goto Error;
    }

    if (!FindAndSkipString(szRsp, "+CMGW: ", szRsp) ||
        !ExtractUInt(szRsp, (UINT32&)*pIndex, szRsp)  ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseWriteMsg() - ERROR: Could not extract the Message Index.\r\n");
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

    RIL_LOG_VERBOSE("ParseWriteSmsToSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
RIL_RESULT_CODE CTEBase::CoreDeleteSmsOnSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreDeleteSmsOnSim() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreDeleteSmsOnSim() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreDeleteSmsOnSim() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CMGD=%u\r", ((int*)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreDeleteSmsOnSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseDeleteSmsOnSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseDeleteSmsOnSim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseDeleteSmsOnSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTEBase::CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetBandMode() - Enter\r\n");

    // this is modem dependent, to be implemented in te_modem_xxx.cpp
    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;

Error:
    RIL_LOG_VERBOSE("CoreSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTEBase::CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreQueryAvailableBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    RIL_LOG_VERBOSE("CoreQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseQueryAvailableBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    RIL_LOG_VERBOSE("ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTEBase::CoreStkGetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreStkGetProfile() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+cmd?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseStkGetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseStkGetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTEBase::CoreStkSetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreStkSetProfile() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreStkSetProfile() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreStkSetProfile() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+cmd=%s\r", (char*)pData))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseStkSetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseStkSetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTEBase::CoreStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreStkSendEnvelopeCommand() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreStkSendEnvelopeCommand() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreStkSendEnvelopeCommand() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+cmd=%s\r", (char*)pData))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseStkSendEnvelopeCommand() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTEBase::CoreStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreStkSendTerminalResponse() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char *szResponse = NULL;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreStkSendTerminalResponse() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_CRITICAL("CoreStkSendTerminalResponse() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    szResponse = (char *)pData;
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+=%s\r", szResponse))
    {
        RIL_LOG_CRITICAL("CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
        goto Error;
    }
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseStkSendTerminalResponse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseStkSendTerminalResponse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTEBase::CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreStkHandleCallSetupRequestedFromSim() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nCallSetup = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreStkHandleCallSetupRequestedFromSim() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CoreStkHandleCallSetupRequestedFromSim() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    nCallSetup = ((int *)pData)[0];
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+=%d\r", nCallSetup))
    {
        RIL_LOG_CRITICAL("CoreStkHandleCallSetupRequestedFromSim() - ERROR: Could not form string.\r\n");
        goto Error;
    }
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CoreStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
RIL_RESULT_CODE CTEBase::CoreExplicitCallTransfer(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreExplicitCallTransfer() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CHLD=4\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
    
    RIL_LOG_VERBOSE("CoreExplicitCallTransfer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseExplicitCallTransfer(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseExplicitCallTransfer() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseExplicitCallTransfer() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTEBase::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_LOG_VERBOSE("CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTEBase::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_LOG_VERBOSE("CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_LOG_VERBOSE("ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTEBase::CoreGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetNeighboringCellIDs() - Enter\r\n");

    // this is modem dependent, to be implemented in the modem
    // specific TE
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    RIL_LOG_VERBOSE("CoreGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    // this is modem dependent, to be implemented in the modem
    // specific TE
    RIL_LOG_VERBOSE("ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//
RIL_RESULT_CODE CTEBase::CoreSetLocationUpdates(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetLocationUpdates() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    int nUpdatesEnabled = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetLocationUpdates() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CoreSetLocationUpdates() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    nUpdatesEnabled = ((int *)pData)[0];
    
    if (1 == nUpdatesEnabled)     // Enable location updates
    {
        (void)CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=2\r", sizeof(rReqData.szCmd1));
    }
    else if (0 == nUpdatesEnabled)
    {
        (void)CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=1\r", sizeof(rReqData.szCmd1));
    }
    else
    {
        RIL_LOG_CRITICAL("CoreSetLocationUpdates() - ERROR: Undefined update setting: %d\r\n", nUpdatesEnabled);
        res = RIL_E_GENERIC_FAILURE;
        goto Error;
    }
    
    res = RRIL_RESULT_OK;
    
Error:
    RIL_LOG_VERBOSE("CoreSetLocationUpdates() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetLocationUpdates(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetLocationUpdates() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetLocationUpdates() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION 77
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetSubscription(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreCdmaSetSubscription() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;
    
    RIL_LOG_VERBOSE("CoreCdmaSetSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetSubscription(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseCdmaSetSubscription() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseCdmaSetSubscription() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetRoamingPreference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreCdmaSetRoamingPreference() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;
    
    RIL_LOG_VERBOSE("CoreCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseCdmaSetRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
RIL_RESULT_CODE CTEBase::CoreCdmaQueryRoamingPreference(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreCdmaQueryRoamingPreference() - Enter\r\n");
    RIL_RESULT_CODE res = RIL_E_REQUEST_NOT_SUPPORTED;
    
    RIL_LOG_VERBOSE("CoreCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseCdmaQueryRoamingPreference() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTEBase::CoreSetTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTEBase::CoreQueryTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
RIL_RESULT_CODE CTEBase::CoreCdmaQueryPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
RIL_RESULT_CODE CTEBase::CoreCdmaFlash(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaFlash(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
RIL_RESULT_CODE CTEBase::CoreCdmaBurstDtmf(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaBurstDtmf(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
RIL_RESULT_CODE CTEBase::CoreCdmaValidateAndWriteAkey(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
RIL_RESULT_CODE CTEBase::CoreCdmaSendSms(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSendSms(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
RIL_RESULT_CODE CTEBase::CoreCdmaSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
RIL_RESULT_CODE CTEBase::CoreGsmGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGsmGetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("Parse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    UINT32 nSelected = 0;
    char szChannels[MAX_BUFFER_SIZE] = {0};
    char szLangs[MAX_BUFFER_SIZE] = {0};
    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* pszChannels = NULL;
    const BYTE* pszLangs = NULL;
    P_ND_BROADCASTSMSCONFIGINFO_DATA pBroadcastSmsConfigInfoBlob = NULL;
    UINT32 nCount = 0;
    UINT32 nStructsChannels = 0, nStructsLangs = 0;

    pBroadcastSmsConfigInfoBlob = (P_ND_BROADCASTSMSCONFIGINFO_DATA)malloc(sizeof(S_ND_BROADCASTSMSCONFIGINFO_DATA));
    if (NULL == pBroadcastSmsConfigInfoBlob)
    {
        RIL_LOG_CRITICAL("ParseGsmGetBroadcastSmsConfig() - ERROR: Could not allocate memory for pBroadcastSmsConfigInfoBlob.\r\n");
        goto Error;
    }
    //  Set bytes to 0xFF because 0 values mean something in this structure.
    memset(pBroadcastSmsConfigInfoBlob, 0xFF, sizeof(S_ND_BROADCASTSMSCONFIGINFO_DATA));

    // Parse "<prefix>+CSCB: <mode>,<mids>,<dcss><postfix>"
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp) ||
        !SkipString(szRsp, "+CSCB: ", szRsp) ||
        !ExtractUInt(szRsp, nSelected, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedString(szRsp, szChannels, MAX_BUFFER_SIZE, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedString(szRsp, szLangs, MAX_BUFFER_SIZE, szRsp) ||
        !SkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGsmGetBroadcastSmsConfig() - ERROR: Could not extract data.\r\n");
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
        RIL_LOG_CRITICAL("ParseGsmGetBroadcastSmsConfig() - ERROR: Too many structs needed!  nCount=%d.\r\n", nCount);
        goto Error;
    }
    
    //  Parse through szChannels.
    pszChannels = szChannels;
    for (UINT32 i = 0; i < nStructsChannels; i++)
    {
        UINT32 nValue1, nValue2;
        if (!ExtractUInt(pszChannels, nValue1, pszChannels))
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
                if (!ExtractUInt(pszChannels, nValue2, pszChannels))
                {
                    //  Nothing after the "-" is an error.
                    RIL_LOG_CRITICAL("ParseGsmGetBroadcastSmsConfig() - ERROR: Parsing szChannels range. nStructsChannels=%d i=%d\r\n", nStructsChannels, i);
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
    for (UINT32 i = 0; i < nStructsLangs; i++)
    {
        UINT32 nValue1, nValue2;
        if (!ExtractUInt(pszLangs, nValue1, pszLangs))
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
                if (!ExtractUInt(pszLangs, nValue2, pszLangs))
                {
                    //  Nothing after the "-" is an error.
                    RIL_LOG_CRITICAL("ParseGsmGetBroadcastSmsConfig() - ERROR: Parsing szLangs range. nStructsLangs=%d i=%d\r\n", nStructsLangs, i);
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
    for (UINT32 i=0; i < nCount; i++)
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


    RIL_LOG_VERBOSE("Parse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
RIL_RESULT_CODE CTEBase::CoreGsmSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGsmSetBroadcastSmsConfig() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szChannels[MAX_BUFFER_SIZE] = {0};
    char szLangs[MAX_BUFFER_SIZE] = {0};
    UINT32 nSelected = 0;
    const RIL_GSM_BroadcastSmsConfigInfo **   ppBroadcastSmsConfigInfo;
    UINT32 nNumOfConfigInfos = 0;

    if (sizeof(RIL_GSM_BroadcastSmsConfigInfo) > uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    ppBroadcastSmsConfigInfo = (const RIL_GSM_BroadcastSmsConfigInfo **)pData;
    nNumOfConfigInfos = uiDataSize / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);

    //  For now, use the "selected" member of the first struct to choose active or not.
    {
        const RIL_GSM_BroadcastSmsConfigInfo *pConfigInfo = ppBroadcastSmsConfigInfo[0];
        if (NULL == pConfigInfo)
        {
            RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: ppBroadcastSmsConfigInfo[0] is NULL. (selected)\r\n");
            goto Error;
        }
        
        nSelected = (UINT32)(pConfigInfo->selected);
    }

    //  Loop through each RIL_GSM_BroadcastSmsConfigInfo structure.
    //  Make string szChannels our list of channels to add.
    for (UINT32 i = 0; i < nNumOfConfigInfos; i++)
    {
        const RIL_GSM_BroadcastSmsConfigInfo *pConfigInfo = ppBroadcastSmsConfigInfo[i];
        if (NULL == pConfigInfo)
        {
            RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: ppBroadcastSmsConfigInfo[%d] is NULL. (channels)\r\n", i);
            goto Error;
        }
        
        if (pConfigInfo->fromServiceId == pConfigInfo->toServiceId)
        {
            if (!PrintStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), "%u", pConfigInfo->fromServiceId))
            {
                RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Unable to print from service id of ppBroadcastSmsConfigInfo[%d]\r\n", i);
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), "%u-%u", pConfigInfo->fromServiceId, pConfigInfo->toServiceId))
            {
                RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Unable to print to service id of ppBroadcastSmsConfigInfo[%d]\r\n", i);
                goto Error;
            }
        }
        
        //  See if this is last range.  If not, add a ','.
        if (i+1 < nNumOfConfigInfos)
        {
            if (!PrintStringNullTerminate(szChannels, MAX_BUFFER_SIZE - strlen(szChannels), "%s,", szChannels))
            {
                RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Unable to print channels of ppBroadcastSmsConfigInfo[%d]\r\n", i);
                goto Error;
            }
        }
    }
    
    //  Loop through each RIL_GSM_BroadcastSmsConfigInfo structure.
    //  Make string szLangs our list of languages to add.
    for (UINT32 i = 0; i < nNumOfConfigInfos; i++)
    {
        const RIL_GSM_BroadcastSmsConfigInfo *pConfigInfo = ppBroadcastSmsConfigInfo[i];
        if (NULL == pConfigInfo)
        {
            RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: ppBroadcastSmsConfigInfo[%d] is NULL. (langs)\r\n", i);
            goto Error;
        }
        
        if (pConfigInfo->fromCodeScheme == pConfigInfo->toCodeScheme)
        {
            if (!PrintStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szLangs), "%u", pConfigInfo->fromCodeScheme))
            {
                RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Unable to print from service id of ppBroadcastSmsConfigInfo[%d]\r\n", i);
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szLangs), "%u-%u", pConfigInfo->fromCodeScheme, pConfigInfo->toCodeScheme))
            {
                RIL_LOG_CRITICAL("CoreGsmSetBroadcastSmsConfig() - ERROR: Unable to print from from-to code scheme of ppBroadcastSmsConfigInfo[%d]\r\n", i);
                goto Error;
            }
        }
        
        //  See if this is last range.  If not, add a ','.
        if (i+1 < nNumOfConfigInfos)
        {
            (void)PrintStringNullTerminate(szLangs, MAX_BUFFER_SIZE - strlen(szLangs), "%s,", szLangs);            
        }
    }

    //  Make the final string.
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB=%u,\"%s\",\"%s\"\r", nSelected, szChannels, szLangs))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGsmSetBroadcastSmsConfig() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//


RIL_RESULT_CODE CTEBase::CoreGsmSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGsmSmsBroadcastActivation() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreGsmSmsBroadcastActivation() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreGsmSmsBroadcastActivation() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCB=%u\r", ((int*)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("Parse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("Parse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
RIL_RESULT_CODE CTEBase::CoreCdmaGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
RIL_RESULT_CODE CTEBase::CoreCdmaSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
RIL_RESULT_CODE CTEBase::CoreCdmaSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
RIL_RESULT_CODE CTEBase::CoreCdmaSubscription(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaSubscription(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
RIL_RESULT_CODE CTEBase::CoreCdmaWriteSmsToRuim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
RIL_RESULT_CODE CTEBase::CoreCdmaDeleteSmsOnRuim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
RIL_RESULT_CODE CTEBase::CoreDeviceIdentity(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseDeviceIdentity(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
RIL_RESULT_CODE CTEBase::CoreExitEmergencyCallbackMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTEBase::ParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData)
{
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
RIL_RESULT_CODE CTEBase::CoreGetSmscAddress(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreGetSmscAddress() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCA?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreGetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseGetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseGetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;

    char* szSCAddr = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == szSCAddr)
    {
        RIL_LOG_CRITICAL("ParseGetSMSCAddress() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }

    // Parse "<prefix><sca>,<tosca><postfix>"
    //  We can ignore the , and <tosca>.
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp) ||
        !ExtractQuotedString(szRsp, szSCAddr, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("ParseGetSMSCAddress() - ERROR: Could not extract the SMS Service Center address string.\r\n");
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

    RIL_LOG_VERBOSE("ParseGetSMSCAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
RIL_RESULT_CODE CTEBase::CoreSetSmscAddress(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreSetSmscAddress() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CoreSetSmscAddress() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CoreSetSmscAddress() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CSCA=\"%s\"\r", ((char*)pData)[0]))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreSetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseSetSmscAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseSetSmscAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("ParseSetSmscAddress() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTEBase::CoreReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CoreReportSmsMemoryStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CPMS?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CoreReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTEBase::ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseReportSmsMemoryStatus() - Enter\r\n");

        RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    const int nMaxStorageStringLen = 5;
    char szMem1[nMaxStorageStringLen] = {0};
    char szMem2[nMaxStorageStringLen] = {0};
    char szMem3[nMaxStorageStringLen] = {0};
    unsigned int nUsed1 = 0, nUsed2 = 0, nUsed3 = 0;
    unsigned int nTotal1 = 0, nTotal2 = 0, nTotal3 = 0;

    int* pStorageIsAvailable = (int*)malloc(sizeof(int));
    if (NULL == pStorageIsAvailable)
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not allocate memory for an int.\r\n");
        goto Error;
    }
    memset(pStorageIsAvailable, 0x01, sizeof(int));

    // Parse "<prefix>+CPMS: <mem1>,<used1>,<total1>,<mem2>,<used2>,<total2>,<mem3>,<used3>,<total3><postfix>"

    //  Parse beginning
    if (!SkipRspStart(szRsp, g_szNewLine, szRsp) ||
        !SkipString(szRsp, "+CPMS: ", szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract +CPMS: string.\r\n");
        goto Error;
    }
    
    //  Parse <mem1>,<used1>,<total1>
    if (!ExtractQuotedString(szRsp, szMem1, nMaxStorageStringLen, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nUsed1, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nTotal1, szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract set 1 data.\r\n");
        goto Error;        
    }
    
    //  Parse ","
    if (!SkipString(szRsp, ",", szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract set 1 comma.\r\n");
        goto Error;        
    }
        
    //  Parse <mem2>,<used2>,<total2>
    if (!ExtractQuotedString(szRsp, szMem2, nMaxStorageStringLen, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nUsed2, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nTotal2, szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract set 2 data.\r\n");
        goto Error;        
    }
    
    //  Parse ","
    if (!SkipString(szRsp, ",", szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract set 2 comma.\r\n");
        goto Error;        
    }
    
    //  Parse <mem3>,<used3>,<total3>
    if (!ExtractQuotedString(szRsp, szMem3, nMaxStorageStringLen, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nUsed3, szRsp) ||
        !SkipString(szRsp, ",", szRsp) ||
        !ExtractUInt(szRsp, nTotal3, szRsp))
    {
        RIL_LOG_CRITICAL("ParseReportSmsMemoryStatus() - ERROR: Could not extract set 3 data.\r\n");
        goto Error;        
    }
    
    //  Determine if storage is available.
    //  Not sure if this is correct, but if any of the <usedX> = <totalX>, then full.
    if ( (nUsed1 >= nTotal1) || (nUsed2 >= nTotal2) || (nUsed3 >= nTotal3) )
    {
        ((int *)pStorageIsAvailable)[0] = 0;
    }
    else
    {
        ((int *)pStorageIsAvailable)[0] = 1;
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pStorageIsAvailable;
    rRspData.uiDataSize  = sizeof(int*);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pStorageIsAvailable);
        pStorageIsAvailable = NULL;
    }


    RIL_LOG_VERBOSE("ParseReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTEBase::CoreReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTEBase::CoreReportStkServiceRunning() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    //  [DP] Let's NO-OP this command.
    rReqData.szCmd1[0] = NULL;
    
    g_fReadyForSTKNotifications = TRUE;

    RIL_LOG_VERBOSE("CTEBase::CoreReportStkServiceRunning() - Exit\r\n");
    return res; 
}

RIL_RESULT_CODE CTEBase::ParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseReportStkServiceRunning() - Enter\r\n");
    RIL_LOG_VERBOSE("ParseReportStkServiceRunning() - Exit\r\n");
    return RRIL_RESULT_OK;
}

//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTEBase::ParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("ParseUnsolicitedSignalStrength() - Enter\r\n");
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint                uiRSSI  = 0;
    uint                uiBER   = 0;
    RIL_SignalStrength* pSigStrData;

    pSigStrData = (RIL_SignalStrength*)malloc(sizeof(RIL_SignalStrength));

    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("ParseUnsolicitedSignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength.\r\n");
        goto Error;
    }
    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength));

    // Parse "<prefix>+CSQ: <rssi>,<ber><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CSQ: ", pszRsp))
    {
        RIL_LOG_CRITICAL("ParseUnsolicitedSignalStrength() - ERROR: Could not find AT response.\r\n");
        goto Error;            
    }
    
    if (!ExtractUInt(pszRsp, uiRSSI, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseUnsolicitedSignalStrength() - ERROR: Could not extract uiRSSI.\r\n");
        goto Error;                        
    }
    
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiBER, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseUnsolicitedSignalStrength() - ERROR: Could not extract uiBER.\r\n");
        goto Error;            
    }
    
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("ParseUnsolicitedSignalStrength() - ERROR: Could not extract the response end.\r\n");
        goto Error;
    }

#if 0
    if (ACT_UMTS == g_uiAccessTechnology)
    {
        RIL_LOG_INFO("ParseUnsolicitedSignalStrength() - Compressing values in UMTS\r\n");
        // compress values - this needs to be reviewed as the mapping is not linear,
        // but this will do for now

        // for Infineon modem, the range is 0..91, so compress down to 0..31
        if (99 != uiRSSI)
            uiRSSI /= 3;

        // for Infineon modem, the range is 0..49, so compress down to 0..7
        if (99 != uiBER)
            uiBER /= 7;
    }
#endif
    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    res = RRIL_RESULT_OK;        

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, (void*)pSigStrData, sizeof(RIL_SignalStrength));

Error:
    free(pSigStrData);
    pSigStrData = NULL;

    RIL_LOG_VERBOSE("ParseUnsolicitedSignalStrength() - Exit\r\n");
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
