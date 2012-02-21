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
/////////////////////////////////////////////////////////////////////////////

#include <telephony/ril.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "types.h"
#include "rillog.h"
#include "te.h"
#include "thread_ops.h"
#include "systemmanager.h"
#include "repository.h"
#include "rildmain.h"
#include "reset.h"



///////////////////////////////////////////////////////////
//  FUNCTION PROTOTYPES
//

static void onRequest(int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState onGetCurrentRadioState();
static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const char* getVersion();


///////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
//

char* g_szCmdPort  = NULL;
BOOL  g_bIsSocket = FALSE;
char* g_szDataPort1 = NULL;
char* g_szDataPort2 = NULL;
char* g_szDataPort3 = NULL;
char* g_szDataPort4 = NULL;
char* g_szDataPort5 = NULL;
char* g_szDLC2Port = NULL;
char* g_szDLC6Port = NULL;
char* g_szDLC8Port = NULL;
char* g_szURCPort = NULL;


static const RIL_RadioFunctions gs_callbacks =
{
    RIL_VERSION,
    onRequest,
    onGetCurrentRadioState,
    onSupports,
    onCancel,
    getVersion
};

static const struct RIL_Env * gs_pRilEnv;


///////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
//

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_onRequestComplete(RIL_Token tRIL, RIL_Errno eErrNo, void *pResponse, size_t responseLen)
{
    gs_pRilEnv->OnRequestComplete(tRIL, eErrNo, pResponse, responseLen);
    RIL_LOG_INFO("After OnRequestComplete(): token=0x%08x, eErrNo=%d, pResponse=[0x%08x], len=[%d]\r\n", tRIL, eErrNo, pResponse, responseLen);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_onUnsolicitedResponse(int unsolResponseID, const void *pData, size_t dataSize)
{
    bool bSendNotification = true;

    switch (unsolResponseID)
    {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: // 1000
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:  // 1001
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED\r\n");
            break;

        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:  // 1002
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED\r\n");
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
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - GW_signalStrength=%d\r\n", ((RIL_SignalStrength_v6 *)pData)->GW_SignalStrength.signalStrength);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - GW_bitErrorRate=%d\r\n", ((RIL_SignalStrength_v6 *)pData)->GW_SignalStrength.bitErrorRate);
            }
            break;

        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:  // 1010
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_DATA_CALL_LIST_CHANGED\r\n");
            if (pData && dataSize)
            {
                int nDataCallResponseNum = dataSize / sizeof(RIL_Data_Call_Response_v6);
                RIL_Data_Call_Response_v6 *pDCR = (RIL_Data_Call_Response_v6 *)pData;
                for (int i=0; i<nDataCallResponseNum; i++)
                {
                    RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_Data_Call_Response_v6[%d] status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\" addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
                        i,
                        pDCR[i].status,
                        pDCR[i].suggestedRetryTime,
                        pDCR[i].cid, pDCR[i].active,
                        pDCR[i].type, pDCR[i].ifname,
                        pDCR[i].addresses,
                        pDCR[i].dnses,
                        pDCR[i].gateways);
                }
            }
            break;

        case RIL_UNSOL_SUPP_SVC_NOTIFICATION:  // 1011
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_SUPP_SVC_NOTIFICATION\r\n");
            if (pData && dataSize)
            {
                RIL_SuppSvcNotification *pSSN = (RIL_SuppSvcNotification*)pData;
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - notification type=%d code=%d index=%d type=%d number=\"%s\"\r\n",
                    pSSN->notificationType,
                    pSSN->code,
                    pSSN->index,
                    pSSN->type,
                    pSSN->number);
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
            else
            {
                // If there is no data, this unsolicited command will generate a JAVA CRASH
                // So ignore it if we are in this case
                bSendNotification = false;
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

        case RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED:  // 1031 - not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_CDMA_PRL_CHANGED:  // 1032 - not supported
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CDMA_PRL_CHANGED\r\n");
            bSendNotification = false;
            break;

        case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE:  // 1033
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE\r\n");
            break;

        case RIL_UNSOL_RIL_CONNECTED:  // 1034
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RIL_CONNECTED\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_RIL_CONNECTED=[%d]\r\n", ((int *)pData)[0]);
            }
            break;


        //  ************************* END OF REGULAR NOTIFICATIONS *******************************

#if defined(M2_CALL_FAILED_CAUSE_FEATURE_ENABLED)

        case RIL_UNSOL_CALL_FAILED_CAUSE:  // 1035
            RIL_LOG_INFO("RIL_onUnsolicitedResponse() - RIL_UNSOL_CALL_FAILED_CAUSE\r\n");
            if (pData && dataSize)
            {
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - call id=[%d]\r\n", ((int *)pData)[0]);
                RIL_LOG_INFO("RIL_onUnsolicitedResponse() - failed cause=[%d]\r\n", ((int *)pData)[1]);
            }
            break;

#endif //  M2_CALL_FAILED_CAUSE_FEATURE_ENABLED


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
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const struct timeval * pRelativeTime)
{
    if (pRelativeTime)
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[%d]  usec=[%d]\r\n", pRelativeTime->tv_sec, pRelativeTime->tv_usec);
    }
    else
    {
        RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() timeval sec=[0]  usec=[0]\r\n");
    }
    gs_pRilEnv->RequestTimedCallback(callback, pParam, pRelativeTime);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const unsigned long seconds, const unsigned long microSeconds)
{
    RIL_LOG_INFO("Calling gs_pRilEnv->RequestTimedCallback() sec=[%d]  usec=[%d]\r\n", seconds, microSeconds);
    struct timeval myTimeval = {0,0};
    myTimeval.tv_sec = seconds;
    myTimeval.tv_usec = microSeconds;
    gs_pRilEnv->RequestTimedCallback(callback, pParam, &myTimeval);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
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

    //  If we're in the middle of TriggerRadioError(), spoof all commands.
    if (g_bSpoofCommands)
    {
        if (RIL_REQUEST_GET_CURRENT_CALLS == requestID)
        {
            RIL_LOG_INFO("****************** SPOOFED RIL_REQUEST_GET_CURRENT_CALLS *********************\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
            return;
        }
        else if (RIL_REQUEST_DEACTIVATE_DATA_CALL == requestID)
        {
            RIL_LOG_INFO("****************** SPOOFED RIL_REQUEST_DEACTIVATE_DATA_CALL *********************\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
            return;
        }
        else if (RIL_REQUEST_SETUP_DATA_CALL == requestID)
        {
            RIL_LOG_INFO("****************** SPOOFED RIL_REQUEST_SETUP_DATA_CALL *********************\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
            return;
        }
        else
        {
            RIL_LOG_INFO("****************** SPOOFED REQID=0x%08X, %d  token=0x%08x *********************\r\n", requestID, requestID, (int) hRilToken);
            RIL_onRequestComplete(hRilToken, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
            return;
        }
    }

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

        case RIL_REQUEST_VOICE_REGISTRATION_STATE:  // 20
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_VOICE_REGISTRATION_STATE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestRegistrationState(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DATA_REGISTRATION_STATE:  // 21
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DATA_REGISTRATION_STATE\r\n");
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

        case RIL_REQUEST_OEM_HOOK_RAW:  // 59
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

            CRepository repository;
            const int CELLINFO_EN_DEFAULT = 1;
            int nEnableCellInfo = CELLINFO_EN_DEFAULT;

            if (!repository.Read(g_szGroupModem, g_szEnableCellInfo, nEnableCellInfo))
            {
                nEnableCellInfo = CELLINFO_EN_DEFAULT;
            }

            if (nEnableCellInfo)
            {
                eRetVal = (RIL_Errno)CTE::GetTE().RequestGetNeighboringCellIDs(hRilToken, pData, datalen);
            }
            else
            {
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            }
        }
        break;

        case RIL_REQUEST_SET_LOCATION_UPDATES:  // 76
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_LOCATION_UPDATES\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
        }
        break;

        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE:  // 77 - CDMA, not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE\r\n");
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

        case RIL_REQUEST_SET_TTY_MODE:  // 80
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SET_TTY_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSetTtyMode(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_QUERY_TTY_MODE:  // 81
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_QUERY_TTY_MODE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestQueryTtyMode(hRilToken, pData, datalen);
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

        case RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE:  // 104 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_ISIM_AUTHENTICATION:  // 105 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ISIM_AUTHENTICATION\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        //  ************************* END OF REGULAR REQUESTS *******************************

        case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU:  // 106 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS:  // 107 - not supported
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS\r\n");
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        }
        break;

        case RIL_REQUEST_SIM_TRANSMIT_BASIC:  // 108
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIM_TRANSMIT_BASIC\r\n");
#if defined(M2_SEEK_FEATURE_ENABLED)
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSimTransmitBasic(hRilToken, pData, datalen);
#else
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
        }
        break;

        case RIL_REQUEST_SIM_OPEN_CHANNEL:  // 109
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIM_OPEN_CHANNEL\r\n");
#if defined(M2_SEEK_FEATURE_ENABLED)
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSimOpenChannel(hRilToken, pData, datalen);
#else
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
        }
        break;

        case RIL_REQUEST_SIM_CLOSE_CHANNEL:  // 110
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIM_CLOSE_CHANNEL\r\n");
#if defined(M2_SEEK_FEATURE_ENABLED)
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSimCloseChannel(hRilToken, pData, datalen);
#else
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
        }
        break;

        case RIL_REQUEST_SIM_TRANSMIT_CHANNEL:  // 111
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_SIM_TRANSMIT_CHANNEL\r\n");
#if defined(M2_SEEK_FEATURE_ENABLED)
            eRetVal = (RIL_Errno)CTE::GetTE().RequestSimTransmitChannel(hRilToken, pData, datalen);
#else
            RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
        }
        break;

#if defined(M2_VT_FEATURE_ENABLED)

        case RIL_REQUEST_HANGUP_VT:  // 112
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_HANGUP_VT\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestHangupVT(hRilToken, pData, datalen);
        }
        break;

        case RIL_REQUEST_DIAL_VT:  // 113
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_DIAL_VT\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestDialVT(hRilToken, pData, datalen);
        }
        break;

#endif // M2_VT_FEATURE_ENABLED

#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)

        case RIL_REQUEST_GET_SIM_SMS_STORAGE:  // 114
        {
            RIL_LOG_INFO("onRequest() - RIL_REQUEST_GET_SIM_SMS_STORAGE\r\n");
            eRetVal = (RIL_Errno)CTE::GetTE().RequestGetSimSmsStorage(hRilToken, pData, datalen);
        }
        break;

#endif // M2_GET_SIM_SMS_STORAGE_ENABLED

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

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
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

    nSupport = 1;
    return nSupport;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void onCancel(RIL_Token t)
{
    RIL_LOG_INFO("onCancel() - *******************************************************\r\n");
    RIL_LOG_INFO("onCancel() - Enter - Exit  token=0x%08X\r\n", (int)t);
    RIL_LOG_INFO("onCancel() - *******************************************************\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static const char* getVersion(void)
{
    return "Intrinsyc Rapid-RIL M6.09 for Android 4.0.3 (Build February 15/2012)";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void* mainLoop(void *param)
{
    RIL_LOG_INFO("mainLoop() - Enter\r\n");

    UINT32 dwRet = 1;

    // Make sure we can access Non-Volatile Memory
    if (!CRepository::Init())
    {
        //RIL_LOG_CRITICAL("mainLoop() - Could not initialize non-volatile memory access.\r\n");

        dwRet = 0;
        goto Error;
    }

    // Initialize logging class
    CRilLog::Init();


    // Create and start system manager
    if (!CSystemManager::GetInstance().InitializeSystem())
    {
        RIL_LOG_CRITICAL("mainLoop() - RIL InitializeSystem() FAILED\r\n");

        dwRet = 0;
        goto Error;
    }

    RIL_LOG_INFO("mainLoop() - RIL Initialization completed successfully.\r\n");

Error:
    if (!dwRet)
    {
        RIL_LOG_CRITICAL("mainLoop() - RIL Initialization FAILED\r\n");

        CSystemManager::Destroy();
    }

    RIL_LOG_INFO("mainLoop() - Exit\r\n");
    return (void*)dwRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void usage(char *szProgName)
{
    fprintf(stderr, "RapidRIL requires:\n");
    fprintf(stderr, "    -a <Call and Misc AT command port>\n");
    fprintf(stderr, "    -n <Network AT command port>\n");
    fprintf(stderr, "    -m <SMS, Supp Service, Call Setting AT command port>\n");
    fprintf(stderr, "    -c <SIM related, SIM Toolkit AT command port>\n");
    fprintf(stderr, "    -u <Notification channel>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 1>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 2>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 3>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 4>\n");
    fprintf(stderr, "    -d <PDP Primary Context - data channel 5>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example in init.rc file:\n");
    fprintf(stderr, "    service ril-daemon /system/bin/rild -l %s -- -a /dev/gsmtty12 -n /dev/gsmtty2 -m /dev/gsmtty6 -c /dev/gsmtty8 -u /dev/gsmtty1 -d /dev/gsmtty3 -d /dev/gsmtty4 -d /dev/gsmtty15 -d /dev/gsmtty16 -d /dev/gsmtty17\n", szProgName);
    fprintf(stderr, "\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static bool RIL_SetGlobals(int argc, char **argv)
{
    int opt;
    int nDataPortCount = 0;

    while (-1 != (opt = getopt(argc, argv, "d:s:a:n:m:c:u:")))
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
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for AT channel chnl=[%d] -a\r\n", g_szCmdPort, RIL_CHANNEL_ATCMD);
            break;

            // This should be the non-emulator case.
            case 'n':
                g_szDLC2Port = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Network channel chnl=[%d] -n\r\n", g_szDLC2Port, RIL_CHANNEL_DLC2);
            break;

            // This should be the non-emulator case.
            case 'm':
                g_szDLC6Port = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Messaging channel chnl=[%d] -m\r\n", g_szDLC6Port, RIL_CHANNEL_DLC6);
            break;

            // This should be the non-emulator case.
            case 'c':
                g_szDLC8Port = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for SIM/USIM Card channel chnl=[%d] -c\r\n", g_szDLC8Port, RIL_CHANNEL_DLC8);
            break;

            // This should be the non-emulator case.
            case 'u':
                g_szURCPort = optarg;
                RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for URC channel chnl=[%d] -u\r\n", g_szURCPort, RIL_CHANNEL_URC);
            break;

            // This should be the non-emulator case.
            //  For multiple PDP contexts (which is default in ICS), must choose 5 data ports.
            case 'd':
                switch(nDataPortCount)
                {
                    case 0:
                        g_szDataPort1 = optarg;
                        RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Data channel chnl=[%d] -d\r\n", g_szDataPort1, RIL_CHANNEL_DATA1);
                        break;

                    case 1:
                        g_szDataPort2 = optarg;
                        RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Data channel chnl=[%d] -d\r\n", g_szDataPort2, RIL_CHANNEL_DATA2);
                        break;

                    case 2:
                        g_szDataPort3 = optarg;
                        RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Data channel chnl=[%d] -d\r\n", g_szDataPort3, RIL_CHANNEL_DATA3);
                        break;

                    case 3:
                        g_szDataPort4 = optarg;
                        RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Data channel chnl=[%d] -d\r\n", g_szDataPort4, RIL_CHANNEL_DATA4);
                        break;

                    case 4:
                        g_szDataPort5 = optarg;
                        RIL_LOG_INFO("RIL_SetGlobals() - Using tty device \"%s\" for Data channel chnl=[%d] -d\r\n", g_szDataPort5, RIL_CHANNEL_DATA5);
                        break;

                    default:
                        usage(argv[0]);
                        return false;
                }
                nDataPortCount++;
            break;

            default:
                usage(argv[0]);
                return false;
        }
    }

    //  RIL will not function without all ports defined
    if (!g_szCmdPort || !g_szDLC2Port || !g_szDLC6Port || !g_szDLC8Port || !g_szURCPort || !g_szDataPort1 || !g_szDataPort2 || !g_szDataPort3 || !g_szDataPort4 || !g_szDataPort5)
    {
        usage(argv[0]);
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
const RIL_RadioFunctions * RIL_Init(const struct RIL_Env *pRilEnv, int argc, char **argv)
{
    RIL_LOG_INFO("RIL_Init() - Enter\r\n");

    gs_pRilEnv = pRilEnv;

    if  (RIL_SetGlobals(argc, argv))
    {
        //  Call mainLoop()
        //  This returns when init is complete.
        mainLoop(NULL);

        RIL_LOG_INFO("RIL_Init() - returning gs_callbacks\r\n");
        return &gs_callbacks;
    }
    else
    {
        RIL_LOG_CRITICAL("RIL_Init() - returning NULL\r\n");
        return NULL;
    }
}

