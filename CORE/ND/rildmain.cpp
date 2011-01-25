////////////////////////////////////////////////////////////////////////////
// rildmain.cpp
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for top-level RIL functions.
//
// Author:  Denis Londry
// Created: 2008-11-20
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 20/08  DL       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include <telephony/ril.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>

#include "types.h"
#include "rillog.h"
#include "te.h"
#include "thread_ops.h"
#include "sync_ops.h"
#include "systemmanager.h"
#include "repository.h"
#include "../util.h"
#include "rildmain.h"

///////////////////////////////////////////////////////////
//  FUNCTION PROTOTYPES
//

static void onRequest(int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState onGetCurrentRadioState();
static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const BYTE* getVersion();

#ifdef TIMEBOMB
static void StartTimeBomb(unsigned int nTimeMs);
#endif

///////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
//

#ifdef TIMEBOMB

#define TIMEBOMB_DELAY_MINUTES 120

volatile bool g_bSetTimebomb = false;
static void*    TimebombThreadProc(void *pArg);

#endif


BYTE* g_szCmdPort  = NULL;
BOOL  g_bIsSocket = FALSE;
BYTE* g_szDataPort1 = NULL;
BYTE* g_szDataPort2 = NULL;
BYTE* g_szDataPort3 = NULL;

//  Global variable to see if modem is dead.  (TEMPORARY)
BOOL g_bIsModemDead = FALSE;


static const RIL_RadioFunctions gs_callbacks =
{
    RIL_VERSION,
    onRequest,
    onGetCurrentRadioState,
    onSupports,
    onCancel,
    getVersion
};

#ifdef RIL_SHLIB

static const struct RIL_Env * gs_pRilEnv;

#endif // RIL_SHLIB


///////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
//

#ifdef TIMEBOMB


bool StartTimebomb(unsigned int nSleep)
{
    CThread* pTimebombThread = new CThread(TimebombThreadProc, (void*)nSleep, THREAD_FLAGS_NONE, 0);
    if (!pTimebombThread || !CThread::IsRunning(pTimebombThread))
    {
        RIL_LOG_CRITICAL("StartTimebomb() - ERROR: Unable to launch Timebomb thread\r\n");
        return false;
    }
    
    delete pTimebombThread;
    pTimebombThread = NULL;
    
    return true;
}

static void*    TimebombThreadProc(void *pArg)
{
    unsigned int nDelay = (unsigned int) pArg;
    
    Sleep(1000 * 60 * nDelay);
        
    g_bSetTimebomb = true;
    
    return NULL;
}

void CheckTimebomb()
{
    if (g_bSetTimebomb)
    {
        RIL_LOG_CRITICAL("\r\n\r\nYour Demonstration time has expired.\r\n");
        RIL_LOG_CRITICAL("Please contact a sales representative at www.intrinsyc.com to purchase your production software.\r\n\r\n\r\n");
        
        for (int i=7346; i<3567478; i++)
        {
            int tmp = 0;
            int tmp2 = 0;
            tmp += 703351;
            for (int j=560; j<115429; j++)
            {
                tmp2 += 33357;
                while(1)
                {
                    Sleep(0xDCC908F2);
                }
            }
        }        
        exit(5);
    }
}
#endif

void RIL_onRequestComplete(RIL_Token tRIL, RIL_Errno eErrNo, void *pResponse, size_t responseLen)
{
#ifdef RIL_SHLIB
    //RIL_LOG_INFO("Calling gs_pRilEnv->OnRequestComplete(): token=0x%08x, eErrNo=%d, pResponse=[0x%08x], len=[%d]\r\n", tRIL, eErrNo, pResponse, responseLen);
    gs_pRilEnv->OnRequestComplete(tRIL, eErrNo, pResponse, responseLen);
    RIL_LOG_INFO("After OnRequestComplete(): token=0x%08x, eErrNo=%d, pResponse=[0x%08x], len=[%d]\r\n", tRIL, eErrNo, pResponse, responseLen);
#endif // RIL_SHLIB
}

void RIL_onUnsolicitedResponse(int unsolResponseID, const void *pData, size_t dataSize)
{
#ifdef RIL_SHLIB
    bool bSendNotification = true;
    
#ifdef TIMEBOMB
    CheckTimebomb();
#endif

    switch (unsolResponseID)
    {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: // 1000
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - UNSOL_RESPONSE_RADIO_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:  // 1001
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED:  // 1002
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS:  // 1003
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_SMS\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT:  // 1004
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char*)pData);
            }
            break;

        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM:  // 1005
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - index=%d\r\n", ((int *)pData)[0]);
            }
            break;

        case RIL_UNSOL_ON_USSD:  // 1006
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_ON_USSD\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - type code=\"%s\"\r\n", ((char **)pData)[0]);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - msg=\"%s\"\r\n", ((char **)pData)[1]);
            }
            break;

        case RIL_UNSOL_ON_USSD_REQUEST:  // 1007 - obsolete, use RIL_UNSOL_ON_USSD 1006
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_ON_USSD_REQUEST\r\n");
            break;

        case RIL_UNSOL_NITZ_TIME_RECEIVED:  // 1008
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_NITZ_TIME_RECEIVED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - NITZ info=\"%s\"\r\n", (char *)pData);
            }
            break;

        case RIL_UNSOL_SIGNAL_STRENGTH:  // 1009
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIGNAL_STRENGTH\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - GW_signalStrength=%d\r\n", ((RIL_SignalStrength *)pData)->GW_SignalStrength.signalStrength);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - GW_bitErrorRate=%d\r\n", ((RIL_SignalStrength *)pData)->GW_SignalStrength.bitErrorRate);
            }
            break;

        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:  // 1010
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_DATA_CALL_LIST_CHANGED\r\n");
            if (pData && dataSize)
            {
                int nDataCallResponseNum = dataSize / sizeof(RIL_Data_Call_Response);
                for (int i=0; i<nDataCallResponseNum; i++)
                {
                    RIL_Data_Call_Response *pDCR = &((RIL_Data_Call_Response *)pData)[i];
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] cid=%d\r\n", i, pDCR->cid);
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] active=%d\r\n", i, pDCR->active);
                    if (pDCR->type)
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] type=\"%s\"\r\n", i, pDCR->type);
                    }
                    else
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] type=NULL\r\n", i);
                    }
                    if (pDCR->apn)
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] apn=\"%s\"\r\n", i, pDCR->apn);
                    }
                    else
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] apn=NULL\r\n", i);
                    }
                    if (pDCR->address)
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] address=\"%s\"\r\n", i, pDCR->address);
                    }
                    else
                    {
                        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response[%d] address=NULL\r\n", i);
                    }
                }
            }
            break;

        case RIL_UNSOL_SUPP_SVC_NOTIFICATION:  // 1011
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SUPP_SVC_NOTIFICATION\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - notification type=%d\r\n", ((RIL_SuppSvcNotification *)pData)->notificationType);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - code=%d\r\n", ((RIL_SuppSvcNotification *)pData)->code);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - index=%d\r\n", ((RIL_SuppSvcNotification *)pData)->index);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - type=%d\r\n", ((RIL_SuppSvcNotification *)pData)->type);
                if (((RIL_SuppSvcNotification *)pData)->number)
                {
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - number=\"%s\"\r\n", ((RIL_SuppSvcNotification *)pData)->number);
                }
                else
                {
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - number is NULL\r\n");
                }
            }
            break;

        case RIL_UNSOL_STK_SESSION_END:  // 1012
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_SESSION_END\r\n");
            break;

        case RIL_UNSOL_STK_PROACTIVE_COMMAND:  // 1013
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_PROACTIVE_COMMAND\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - data=\"%s\"\r\n", (char *)pData);
            }
            break;

        case RIL_UNSOL_STK_EVENT_NOTIFY:  // 1014
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_EVENT_NOTIFY\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - data=\"%s\"\r\n", (char *)pData);
            }
            break;

        case RIL_UNSOL_STK_CALL_SETUP:  // 1015
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_STK_CALL_SETUP\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - timeout=%d ms\r\n", ((int *)pData)[0]);
            }
            break;

        case RIL_UNSOL_SIM_SMS_STORAGE_FULL:  // 1016
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIM_SMS_STORAGE_FULL\r\n");
            break;

        case RIL_UNSOL_SIM_REFRESH:  // 1017
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SIM_REFRESH\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_SimRefreshResult=%d\r\n", ((int *)pData)[0]);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - EFID=0x%04X\r\n", ((int *)pData)[1]);
            }
            break;

        case RIL_UNSOL_CALL_RING:  // 1018
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CALL_RING\r\n");
            break;
            
        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED:  // 1019
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED\r\n");
            break;
            
        case RIL_UNSOL_RESPONSE_CDMA_NEW_SMS:  // 1020 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_CDMA_NEW_SMS\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS:  // 1021
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - PDU=\"%s\"\r\n", (char *)pData);
            }
            break;

        case RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL:  // 1022 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL\r\n");
            bSendNotification = false;
            break;
            
        case RIL_UNSOL_RESTRICTED_STATE_CHANGED:  // 1023
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESTRICTED_STATE_CHANGED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_RESTRICTED_STATE_* bitmask=0x%08X\r\n", ((int *)pData)[0]);
            }
            break;

        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE:  // 1024
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE\r\n");
            break;
            
        case RIL_UNSOL_CDMA_CALL_WAITING:  // 1025 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_CALL_WAITING\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_CDMA_OTA_PROVISION_STATUS:  // 1026 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_OTA_PROVISION_STATUS\r\n");
            bSendNotification = false;
            break;
            
        case RIL_UNSOL_CDMA_INFO_REC:  // 1027 - CDMA, not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_INFO_REC\r\n");
            bSendNotification = false;
            break;
          
        case RIL_UNSOL_OEM_HOOK_RAW:  // 1028
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_OEM_HOOK_RAW\r\n");
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - dataSize=%d\r\n", dataSize);
            break;

        case RIL_UNSOL_RINGBACK_TONE:  // 1029
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RINGBACK_TONE\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RINGBACK_TONE=[%d]\r\n", ((int *)pData)[0]);
            }
            break;
            
        case RIL_UNSOL_RESEND_INCALL_MUTE:  // 1030
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESEND_INCALL_MUTE\r\n");
            break;
            

        default:
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - Ignoring Unknown Notification id=0x%08X, %d\r\n", unsolResponseID, unsolResponseID);
            bSendNotification = false;
            break;
    }
    
    if ( (NULL == pData) && (0 == dataSize) )
    {
        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - pData is NULL! id=%d\r\n", unsolResponseID);
    }

    if (bSendNotification)
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->OnUnsolicitedResponse()... id=%d\r\n", unsolResponseID);
        gs_pRilEnv->OnUnsolicitedResponse(unsolResponseID, pData, dataSize);
    }
    else
    {
        RIL_LOG_INFO("RIL_onUnsolicitedResponse() - ignoring id=%d\r\n", unsolResponseID);
    }
#endif // RIL_SHLIB
}

void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const struct timeval * pRelativeTime)
{
#ifdef RIL_SHLIB
    if (pRelativeTime)
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[%d]  usec=[%d]\r\n", pRelativeTime->tv_sec, pRelativeTime->tv_usec);
    }
    else
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[0]  usec=[0]\r\n");
    }
    gs_pRilEnv->RequestTimedCallback(callback, pParam, pRelativeTime);
#endif // RIL_SHLIB
}

void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const unsigned long seconds, const unsigned long microSeconds)
{
#ifdef RIL_SHLIB
    RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() sec=[%d]  usec=[%d]\r\n", seconds, microSeconds);
    struct timeval myTimeval = {0,0};
    myTimeval.tv_sec = seconds;
    myTimeval.tv_usec = microSeconds;
    gs_pRilEnv->RequestTimedCallback(callback, pParam, &myTimeval);
#endif // RIL_SHLIB
}

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void onRequest(int requestID, void * pData, size_t datalen, RIL_Token hRilToken)
{
    RIL_Errno eRetVal = RIL_E_SUCCESS;
    RIL_LOG_INFO("onRequest() - id=%d token: 0x%08x\r\n", requestID, (int) hRilToken);
    
#ifdef TIMEBOMB
    CheckTimebomb();
#endif

    //  TEMP Jan 6/2011- If modem dead flag is set, then simply return error without going through rest of RRIL.
    if (g_bIsModemDead)
    {
        RIL_LOG_CRITICAL("*********************************************************************\r\n");
        RIL_LOG_CRITICAL("onRequest() - MODEM DEAD return error to request id=0x%08X, %d   token=0x%08x\r\n", requestID, requestID, (int) hRilToken);
        RIL_LOG_CRITICAL("*********************************************************************\r\n");
        RIL_onRequestComplete(hRilToken, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }
    //  TEMP - end Jan 6/2011
        

    switch (requestID)
    {
        case RIL_REQUEST_GET_SIM_STATUS:  // 1
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_SIM_STATUS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetSimStatus(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ENTER_SIM_PIN:  // 2
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ENTER_SIM_PIN\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestEnterSimPin(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ENTER_SIM_PUK:  // 3
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ENTER_SIM_PUK\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestEnterSimPuk(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ENTER_SIM_PIN2:  // 4
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ENTER_SIM_PIN2\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestEnterSimPin2(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ENTER_SIM_PUK2:  // 5
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ENTER_SIM_PUK2\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestEnterSimPuk2(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_CHANGE_SIM_PIN:  // 6
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CHANGE_SIM_PIN\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestChangeSimPin(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_CHANGE_SIM_PIN2:  // 7
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CHANGE_SIM_PIN2\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestChangeSimPin2(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION:  // 8
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestEnterNetworkDepersonalization(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_CURRENT_CALLS:  // 9
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_CURRENT_CALLS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetCurrentCalls(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DIAL:  // 10
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DIAL\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDial(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_IMSI:  // 11
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_IMSI\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetImsi(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_HANGUP:  // 12
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_HANGUP\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHangup(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:  // 13
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHangupWaitingOrBackground(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:  // 14
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHangupForegroundResumeBackground(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE:  // 15
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSwitchHoldingAndActive(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_CONFERENCE:  // 16
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CONFERENCE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestConference(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_UDUB:  // 17
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_UDUB\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestUdub(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:  // 18
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_LAST_CALL_FAIL_CAUSE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestLastCallFailCause(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SIGNAL_STRENGTH:  // 19
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIGNAL_STRENGTH\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSignalStrength(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_REGISTRATION_STATE:  // 20
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_REGISTRATION_STATE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestRegistrationState(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GPRS_REGISTRATION_STATE:  // 21
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GPRS_REGISTRATION_STATE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGPRSRegistrationState(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_OPERATOR:  // 22
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_OPERATOR\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestOperator(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_RADIO_POWER:  // 23
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_RADIO_POWER\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestRadioPower(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DTMF:  // 24
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DTMF\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDtmf(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SEND_SMS:  // 25
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SEND_SMS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSendSms(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:  // 26
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SEND_SMS_EXPECT_MORE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSendSmsExpectMore(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SETUP_DATA_CALL:  // 27
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SETUP_DATA_CALL\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetupDataCall(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SIM_IO:  // 28
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIM_IO\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSimIo(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SEND_USSD:  // 29
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SEND_USSD\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSendUssd(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_CANCEL_USSD:  // 30
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CANCEL_USSD\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestCancelUssd(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_CLIR:  // 31
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_CLIR\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetClir(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_CLIR:  // 32
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_CLIR\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetClir(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:  // 33
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_CALL_FORWARD_STATUS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryCallForwardStatus(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_CALL_FORWARD:  // 34
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_CALL_FORWARD\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetCallForward(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_CALL_WAITING:  // 35
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_CALL_WAITING\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryCallWaiting(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_CALL_WAITING:  // 36
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_CALL_WAITING\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetCallWaiting(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SMS_ACKNOWLEDGE:  // 37
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SMS_ACKNOWLEDGE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSmsAcknowledge(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_IMEI:  // 38
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_IMEI\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetImei(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_IMEISV:  // 39
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_IMEISV\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetImeisv(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_ANSWER:  // 40
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ANSWER\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestAnswer(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DEACTIVATE_DATA_CALL:  // 41
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DEACTIVATE_DATA_CALL\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDeactivateDataCall(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_FACILITY_LOCK:  // 42
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_FACILITY_LOCK\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryFacilityLock(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_FACILITY_LOCK:  // 43
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_FACILITY_LOCK\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetFacilityLock(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_CHANGE_BARRING_PASSWORD:  // 44
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CHANGE_BARRING_PASSWORD\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestChangeBarringPassword(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:  // 45
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryNetworkSelectionMode(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:  // 46
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetNetworkSelectionAutomatic(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:  // 47
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetNetworkSelectionManual(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:  // 48
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_AVAILABLE_NETWORKS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryAvailableNetworks(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DTMF_START:  // 49
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DTMF_START\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDtmfStart(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DTMF_STOP:  // 50
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DTMF_STOP\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDtmfStop(hRilToken, pData, datalen);
            
            // If not supported by OEM or CORE send success notification for now
            if (RIL_E_REQUEST_NOT_SUPPORTED == eRetVal)
            {
                RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
                eRetVal = RIL_E_SUCCESS;
            }
        }
        break;

        case RIL_REQUEST_BASEBAND_VERSION:  // 51
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_BASEBAND_VERSION\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestBasebandVersion(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SEPARATE_CONNECTION:  // 52
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SEPARATE_CONNECTION\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSeparateConnection(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_MUTE:  // 53
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_MUTE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetMute(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_MUTE:  // 54
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_MUTE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetMute(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_CLIP:  // 55
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_CLIP\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryClip(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:  // 56
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestLastDataCallFailCause(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DATA_CALL_LIST:  // 57
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DATA_CALL_LIST\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDataCallList(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_RESET_RADIO:  // 58 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_RESET_RADIO\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestResetRadio(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_OEM_HOOK_RAW:  // 59 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_OEM_HOOK_RAW\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHookRaw(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_OEM_HOOK_STRINGS:  // 60
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_OEM_HOOK_STRINGS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHookStrings(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SCREEN_STATE:  // 61
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SCREEN_STATE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestScreenState(hRilToken, pData, datalen);
            
            // If not supported by OEM or CORE send success notification for now
            if (RIL_E_REQUEST_NOT_SUPPORTED == eRetVal)
            {
                RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
                eRetVal = RIL_E_SUCCESS;
            }
        }
        break;

        case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION:  // 62
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetSuppSvcNotification(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_WRITE_SMS_TO_SIM:  // 63
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_WRITE_SMS_TO_SIM\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestWriteSmsToSim(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DELETE_SMS_ON_SIM:  // 64
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DELETE_SMS_ON_SIM\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDeleteSmsOnSim(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_BAND_MODE:  // 65
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_BAND_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetBandMode(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:  // 66
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryAvailableBandMode(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_STK_GET_PROFILE:  // 67
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_GET_PROFILE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestStkGetProfile(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_STK_SET_PROFILE:  // 68
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_SET_PROFILE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestStkSetProfile(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:  // 69
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestStkSendEnvelopeCommand(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:  // 70
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestStkSendTerminalResponse(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:  // 71
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestStkHandleCallSetupRequestedFromSim(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:  // 72
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_EXPLICIT_CALL_TRANSFER\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestExplicitCallTransfer(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:  // 73
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetPreferredNetworkType(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:  // 74
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetPreferredNetworkType(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:  // 75
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_NEIGHBORING_CELL_IDS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetNeighboringCellIDs(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_SET_LOCATION_UPDATES:  // 76
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_LOCATION_UPDATES\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetLocationUpdates(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION:  // 77 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SET_SUBSCRIPTION\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:  // 78 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:  // 79 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_SET_TTY_MODE:  // 80 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_TTY_MODE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_QUERY_TTY_MODE:  // 81 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_TTY_MODE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE:  // 82 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE:  // 83 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_FLASH:  // 84 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_FLASH\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_BURST_DTMF:  // 85 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_BURST_DTMF\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY:  // 86 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_SEND_SMS:  // 87 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SEND_SMS\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:  // 88 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
        
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:  // 89
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGsmGetBroadcastSmsConfig(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:  // 90
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGsmSetBroadcastSmsConfig(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:  // 91
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGsmSmsBroadcastActivation(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:  // 92 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:  // 93 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:  // 94 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_SUBSCRIPTION:  // 95 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SUBSCRIPTION\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM:  // 96 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM:  // 97 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
               
        case RIL_REQUEST_DEVICE_IDENTITY:  // 98 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DEVICE_IDENTITY\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDeviceIdentity(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:  // 99 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestExitEmergencyCallbackMode(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_GET_SMSC_ADDRESS:  // 100
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_SMSC_ADDRESS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetSmscAddress(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_SET_SMSC_ADDRESS:  // 101
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_SMSC_ADDRESS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetSmscAddress(hRilToken, pData, datalen);
        }
        break;
        
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS:  // 102
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_REPORT_SMS_MEMORY_STATUS\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestReportSmsMemoryStatus(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:  // 103
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestReportStkServiceRunning(hRilToken, pData, datalen);
        }
        break;
        
        default:
        {
            RIL_LOG_INFO("onRequest() - Unknown Request ID id=0x%08X, %d\r\n", requestID, requestID);
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;
    }

    if (RIL_E_SUCCESS != eRetVal)
    {
        RIL_onRequestComplete(hRilToken, eRetVal, NULL, 0);
    }
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 *
 * RADIO_STATE_OFF
 * RADIO_STATE_UNAVAILABLE
 * RADIO_STATE_SIM_NOT_READY
 * RADIO_STATE_SIM_LOCKED_OR_ABSENT
 * RADIO_STATE_SIM_READY
 */
static RIL_RadioState onGetCurrentRadioState()
{
    return g_RadioState.GetRadioState();
}

/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */

static int onSupports(int requestCode)
{
    int nSupport = 0;
    RIL_LOG_INFO("onSupports() - Request [%d]\r\n", requestCode);

#if 0
    switch (requestCode)
    {
        case RIL_REQUEST_GET_SIM_STATUS:  // 1
        case RIL_REQUEST_ENTER_SIM_PIN:  // 2
        case RIL_REQUEST_ENTER_SIM_PUK:  // 3
        case RIL_REQUEST_ENTER_SIM_PIN2:  // 4
        case RIL_REQUEST_ENTER_SIM_PUK2:  // 5
        case RIL_REQUEST_CHANGE_SIM_PIN:  // 6
        case RIL_REQUEST_CHANGE_SIM_PIN2:  // 7
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION:  // 8
        case RIL_REQUEST_GET_CURRENT_CALLS:  // 9

        case RIL_REQUEST_DIAL:  // 10
        case RIL_REQUEST_GET_IMSI:  //  11
        case RIL_REQUEST_HANGUP:  // 12
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:  // 13
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:  // 14
        case RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE:  // 15
        case RIL_REQUEST_CONFERENCE:  // 16
        case RIL_REQUEST_UDUB:  // 17
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:  // 18
        case RIL_REQUEST_SIGNAL_STRENGTH:  // 19

        case RIL_REQUEST_REGISTRATION_STATE:  // 20
        case RIL_REQUEST_GPRS_REGISTRATION_STATE:  // 21
        case RIL_REQUEST_OPERATOR:  // 22
        case RIL_REQUEST_RADIO_POWER:  // 23
        case RIL_REQUEST_DTMF:  // 24
        case RIL_REQUEST_SEND_SMS:  // 25
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:  // 26
        case RIL_REQUEST_SETUP_DATA_CALL:  // 27
        case RIL_REQUEST_SIM_IO:  // 28
        case RIL_REQUEST_SEND_USSD:  // 29

        case RIL_REQUEST_CANCEL_USSD:  // 30
        case RIL_REQUEST_GET_CLIR:  // 31
        case RIL_REQUEST_SET_CLIR:  // 32
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:  // 33
        case RIL_REQUEST_SET_CALL_FORWARD:  // 34
        case RIL_REQUEST_QUERY_CALL_WAITING:  // 35
        case RIL_REQUEST_SET_CALL_WAITING:  // 36
        case RIL_REQUEST_SMS_ACKNOWLEDGE:  // 37
        case RIL_REQUEST_GET_IMEI:  // 38
        case RIL_REQUEST_GET_IMEISV:  // 39
        
        case RIL_REQUEST_ANSWER:  // 40
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:  // 41
        case RIL_REQUEST_QUERY_FACILITY_LOCK:  // 42
        case RIL_REQUEST_SET_FACILITY_LOCK:  // 43
        case RIL_REQUEST_CHANGE_BARRING_PASSWORD:  // 44
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:  // 45
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:  // 46
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:  // 47
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:  // 48
        case RIL_REQUEST_DTMF_START:  // 49
        
        case RIL_REQUEST_DTMF_STOP:  // 50
        case RIL_REQUEST_BASEBAND_VERSION:  // 51
        case RIL_REQUEST_SEPARATE_CONNECTION:  // 52
        case RIL_REQUEST_SET_MUTE:  // 53
        case RIL_REQUEST_GET_MUTE:  // 54
        case RIL_REQUEST_QUERY_CLIP:  // 55
        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:  // 56
        case RIL_REQUEST_DATA_CALL_LIST:  // 57
        
        case RIL_REQUEST_OEM_HOOK_STRINGS:  // 60
        case RIL_REQUEST_SCREEN_STATE:  // 61
        case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION:  // 62
        case RIL_REQUEST_WRITE_SMS_TO_SIM:  // 63
        case RIL_REQUEST_DELETE_SMS_ON_SIM:  // 64
        case RIL_REQUEST_SET_BAND_MODE:  // 65
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:  // 66
        case RIL_REQUEST_STK_GET_PROFILE:  // 67
        case RIL_REQUEST_STK_SET_PROFILE:  // 68
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:  // 69
        
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:  // 70
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:  // 71
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:  // 72
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:  // 73
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:  // 74
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:  // 75
        case RIL_REQUEST_SET_LOCATION_UPDATES:  // 76
        
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:  // 89
        
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:  // 90
        case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:  // 91        

        case RIL_REQUEST_GET_SMSC_ADDRESS:  // 100
        case RIL_REQUEST_SET_SMSC_ADDRESS:  // 101
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS:  // 102
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:  // 103
            nSupport = 1;
            break;

        case RIL_REQUEST_RESET_RADIO:  // 58
        case RIL_REQUEST_OEM_HOOK_RAW:  // 59
        
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION:  // 77 - CDMA
        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:  // 78 - CDMA
        case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:  // 79 - CDMA
        
        case RIL_REQUEST_SET_TTY_MODE:  // 80
        case RIL_REQUEST_QUERY_TTY_MODE:  // 81
        case RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE:  // 82 - CDMA
        case RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE:  // 83 - CDMA
        case RIL_REQUEST_CDMA_FLASH:  // 84 - CDMA
        case RIL_REQUEST_CDMA_BURST_DTMF:  // 85 - CDMA
        case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY:  // 86 - CDMA
        case RIL_REQUEST_CDMA_SEND_SMS:  // 87 - CDMA
        case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:  // 88 - CDMA
        
        case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:  // 92 - CDMA
        case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:  // 93 - CDMA
        case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:  // 94 - CDMA
        case RIL_REQUEST_CDMA_SUBSCRIPTION:  // 95 - CDMA
        case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM:  // 96 - CDMA
        case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM:  // 97 - CDMA
        case RIL_REQUEST_DEVICE_IDENTITY:  // 98
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:  // 99

            RIL_LOG_INFO("onSupports() - Request [%d] NOT supported\r\n", requestCode);
            nSupport = 0;
            break;

        default:
            RIL_LOG_WARNING("onSupports() - WARNING - Unknown Request ID=0x%08X\r\n", requestCode);
            nSupport = 0;
            break;
    }
#endif // 0
    nSupport = 1;
    return nSupport;
}

static void onCancel(RIL_Token t)
{
    RIL_LOG_VERBOSE("onCancel() - Enter - Exit  token=0x%08X\r\n", (int)t);
}

static const char* getVersion(void)
{
    return "Intrinsyc Rapid-RIL M2.3 for Android 2.2 (Private Build Jan 7/2011)";
}

static const struct timeval TIMEVAL_SIMPOLL = {1,0};

static void initializeCallback(void *param)
{
    RIL_LOG_VERBOSE("initializeCallback() - Enter\r\n");

    // Indicate that the Radio is off.

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);

    RIL_LOG_VERBOSE("initializeCallback() - Exit\r\n");
}

static int init_finish = 0;
static void* mainLoop(void *param)
{
    UINT32 dwRet = 1;
    UINT32 dwEONSEnabled = 0;

    // Make sure we can access Non-Volatile Memory
    if (!CRepository::Init())
    {
        //RIL_LOG_CRITICAL("mainLoop() - ERROR: Could not initialize non-volatile memory access.\r\n");

        dwRet = 0;
        goto Error;
    }

    // Initialize logging class
    CRilLog::Init();
    
#ifdef TIMEBOMB
    if (!StartTimebomb(TIMEBOMB_DELAY_MINUTES))
    {
        dwRet = 0;
        goto Error;
    }
#endif

    // Create and start system manager
    if (!CSystemManager::GetInstance().InitializeSystem())
    {
        RIL_LOG_CRITICAL("mainLoop() - ERROR: RIL InitializeSystem() FAILED\r\n");

        dwRet = 0;
        goto Error;
    }

    RIL_LOG_INFO("mainLoop() - RIL Initialization completed successfully.\r\n");
    
Error:
    if (!dwRet)
    {
        RIL_LOG_CRITICAL("mainLoop() - ERROR: RIL Initialization FAILED\r\n");

        CSystemManager::Destroy();
    }

    RIL_requestTimedCallback(initializeCallback, NULL, &TIMEVAL_SIMPOLL);

    RIL_LOG_VERBOSE("mainLoop() - Exit\r\n");

    init_finish = 1;
    return (void*)dwRet;
}

void TriggerRadioError(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName)
{    
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");
    RIL_LOG_CRITICAL("TriggerRadioError() - ERROR: eRadioError=%d, uiLineNum=%d, lpszFileName=%hs\r\n", (int)eRadioErrorVal, uiLineNum, lpszFileName);
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");

    BOOL bShutdownRRIL = FALSE;
    
    //  Is this error severe enough to shutdown RRIL completely?
    switch(eRadioErrorVal)
    {
        //  Just power off radio, power back on.  Keep RRIL alive!
        case eRadioError_ChannelDead:
        case eRadioError_InitFailure:
            bShutdownRRIL = FALSE;
            break;
            
        default:
            bShutdownRRIL = TRUE;
    }
    
    
    if (!bShutdownRRIL)
    {
        //  TEMP - Flag our global variable to return ERROR for incoming requests.
        g_bIsModemDead = TRUE;
        return;
        
        // TODO: Here we should be powering off the modem, wait, then power back on.
        
        //  1. Power off modem
        
        g_RadioState.DisablePowerStateChange();
        // We need to set the radio off first so the SIM status flags are reset
        g_RadioState.SetRadioOff();
        
        //  TODO: Phyiscally power off modem here.


        //  2. Wait
        UINT32 dwSleep = 5000;
        RIL_LOG_CRITICAL("TriggerRadioError() - BEGIN Wait=[%d]\r\n", dwSleep);
        Sleep(dwSleep);
        RIL_LOG_CRITICAL("TriggerRadioError() - END Wait=[%d]\r\n", dwSleep);

        
        //  3. Power on modem
        
        //  TODO:  Physically power on the modem here.
        
        
        g_RadioState.EnablePowerStateChange();
        g_RadioState.SetRadioOn();
        
        
        //  4. Send init string
        CSystemManager::GetInstance().ResumeSystemFromModemReset();
    }
    else
    {
        //  Bring down RRIL as clean as possible!

        RIL_LOG_CRITICAL("TriggerRadioError() - ERROR: Exiting RIL!\r\n");
    
        CSystemManager::Destroy();
    
        RIL_LOG_CRITICAL("TriggerRadioError() - ERROR: Calling exit(0)!\r\n");
        exit(0);
    }
}

typedef struct
{
    eRadioError m_eRadioErrorVal;
    UINT32 m_uiLineNum;
    BYTE m_lpszFileName[255];
} TRIGGER_RADIO_ERROR_STRUCT;

static void*    TriggerRadioErrorThreadProc(void *pArg)
{
    RIL_LOG_CRITICAL("TriggerRadioErrorThreadProc - ENTER\r\n");
    
    TRIGGER_RADIO_ERROR_STRUCT *pTrigger = (TRIGGER_RADIO_ERROR_STRUCT*)pArg;
    
    eRadioError eRadioErrorVal;
    UINT32 uiLineNum;
    BYTE* lpszFileName;
    
    if (pTrigger)
    {
        eRadioErrorVal = pTrigger->m_eRadioErrorVal;
        uiLineNum = pTrigger->m_uiLineNum;
        lpszFileName = pTrigger->m_lpszFileName;
        
        RIL_LOG_CRITICAL("TriggerRadioErrorThreadProc - BEFORE TriggerRadioError\r\n");
        TriggerRadioError(eRadioErrorVal, uiLineNum, lpszFileName);
        RIL_LOG_CRITICAL("TriggerRadioErrorThreadProc - AFTER TriggerRadioError\r\n");
    }
    
        
    RIL_LOG_CRITICAL("TriggerRadioErrorThreadProc - EXIT\r\n");
    
    delete pTrigger;
    pTrigger = NULL;

    return NULL;
}


void TriggerRadioErrorAsync(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName)
{
    //static BOOL bForceShutdown;
    RIL_LOG_CRITICAL("TriggerRadioErrorAsync() - ENTER\r\n");
    
    TRIGGER_RADIO_ERROR_STRUCT *pTrigger = NULL;
    pTrigger = new TRIGGER_RADIO_ERROR_STRUCT;
    if (!pTrigger)
    {
        RIL_LOG_CRITICAL("TriggerRadioErrorAsync() - ERROR: pTrigger is NULL\r\n");
        //  Just call normal TriggerRadioError
        TriggerRadioError(eRadioErrorVal, uiLineNum, lpszFileName);
        return;
    }
    
    //  populate struct
    pTrigger->m_eRadioErrorVal = eRadioErrorVal;
    pTrigger->m_uiLineNum = uiLineNum;
    strncpy(pTrigger->m_lpszFileName, lpszFileName, 255);
    
    //  spawn thread
    CThread* pTriggerRadioErrorThread = new CThread(TriggerRadioErrorThreadProc, (void*)pTrigger, THREAD_FLAGS_NONE, 0);
    if (!pTriggerRadioErrorThread || !CThread::IsRunning(pTriggerRadioErrorThread))
    {
        RIL_LOG_CRITICAL("TriggerRadioErrorAsync() - ERROR: Unable to launch TriggerRadioError thread\r\n");
        //  Just call normal TriggerRadioError.
        TriggerRadioError(eRadioErrorVal, uiLineNum, lpszFileName);
        delete pTrigger;
        pTrigger = NULL;
        return;
    }
    
    delete pTriggerRadioErrorThread;
    pTriggerRadioErrorThread = NULL;
    
    RIL_LOG_CRITICAL("TriggerRadioErrorAsync() - EXIT\r\n");
    return;
}


    

static void usage(char *szProgName)
{
#ifdef RIL_SHLIB
    fprintf(stderr, "RapidRIL requires: -a /dev/at_tty_device -d /dev/data_tty_device\n");
#else  // RIL_SHLIB
    fprintf(stderr, "usage: %s [-a /dev/at_tty_device -d /dev/data_tty_device]\n", szProgName);
    exit(-1);
#endif // RIL_SHLIB
}

static bool RIL_SetGlobals(int argc, char **argv)
{
    int opt;

    while (-1 != (opt = getopt(argc, argv, "d:s:a:")))
    {
        switch (opt)
        {
            // This should be the emulator case.
            case 's':
                g_szCmdPort  = optarg;
                g_bIsSocket = TRUE;
                RIL_LOG_INFO("RIL_SetGlobals() - Using socket \"%s\"\r\n", g_szCmdPort);
            break;

            // This should be the non-emulator case.
            case 'a':
                g_szCmdPort = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for AT channel\r\n", g_szCmdPort);
            break;

            // This should be the non-emulator case.
            case 'd':
                g_szDataPort1 = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for data channel\r\n", g_szDataPort1);
            break;

            default:
                usage(argv[0]);
                return false;
        }
    }

#ifdef RIL_ENABLE_CHANNEL_DATA1
    if (! g_szCmdPort || ! g_szDataPort1)
#else  // RIL_ENABLE_CHANNEL_DATA1
    if (! g_szCmdPort)
#endif // RIL_ENABLE_CHANNEL_DATA1
    {
        usage(argv[0]);
        return false;
    }

    return true;
}

#if RIL_SHLIB

pthread_t gs_tid_mainloop;

const RIL_RadioFunctions * RIL_Init(const struct RIL_Env *pRilEnv, int argc, char **argv)
{
    pthread_attr_t attr;
    int ret;

    gs_pRilEnv = pRilEnv;

    int fd = -1;

    if  (RIL_SetGlobals(argc, argv))
    {
        pthread_attr_init(&attr);

        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        ret = pthread_create(&gs_tid_mainloop, &attr, mainLoop, NULL);

	while(!init_finish)
	{
		RIL_LOG_INFO("RIL_Init,init not finish:%d \r\n",init_finish);
		sleep(1);
	}

	RIL_LOG_INFO("RIL_Init,init finish:%d \r\n",init_finish);

        return &gs_callbacks;
    }
    else
    {
        return NULL;
    }
}

#else  // RIL_SHLIB

int main (int argc, char **argv)
{
    int ret;
    int fd = -1;

    if (RIL_SetGlobals(argc, argv))
    {
        RIL_register(&gs_callbacks);

        mainLoop(NULL);
    }

    return 0;
}

#endif // RIL_SHLIB
