////////////////////////////////////////////////////////////////////////////
// callbacks.cpp
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implementations for all functions provided to RIL_requestTimedCallback
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "command.h"
#include "rildmain.h"
#include "rillog.h"
#include "te.h"
#include "util.h"
#include "oemhookids.h"

void notifyChangedCallState(void* param)
{
    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

void triggerRadioOffInd(void* param)
{
    if (RADIO_STATE_UNAVAILABLE == CTE::GetTE().GetRadioState())
    {
       CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_OFF);
    }
}

void triggerDataResumedInd(void* param)
{
    unsigned char szData[10];
    int pos = 0;
    const int DATA_RESUMED = 1;

    CTE::GetTE().SetDataSuspended(FALSE);

    memset(szData, 0, sizeof(szData));

    convertIntToByteArrayAt(szData, RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND, pos);
    pos += sizeof(int);
    convertIntToByteArrayAt(szData, DATA_RESUMED, pos);

    RIL_onUnsolicitedResponse (RIL_UNSOL_OEM_HOOK_RAW, szData,
            sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND));
}

void triggerDataSuspendInd(void* param)
{
    if (!CTE::GetTE().IsDataSuspended() || (RADIO_STATE_ON != CTE::GetTE().GetRadioState()))
        return;

    unsigned char szData[10];
    int pos = 0;
    const int DATA_SUSPENDED = 0;

    memset(szData, 0, sizeof(szData));

    convertIntToByteArrayAt(szData, RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND, pos);
    pos += sizeof(int);
    convertIntToByteArrayAt(szData, DATA_SUSPENDED, pos);

    RIL_onUnsolicitedResponse (RIL_UNSOL_OEM_HOOK_RAW, szData,
            sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND));
}

void triggerHangup(UINT32 uiCallId)
{
    REQUEST_DATA rReqData;

    memset(&rReqData, 0, sizeof(REQUEST_DATA));
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
             "AT+XSETCAUSE=1,21;+CHLD=1%u\r", uiCallId))
    {
        RIL_LOG_CRITICAL("triggerHangup() - Unable to create hangup command!\r\n");
        return;
    }

    CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUP],
            NULL, REQ_ID_NONE, rReqData);
    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd, TRUE))
        {
            RIL_LOG_CRITICAL("triggerHangup() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerHangup() - Unable to allocate memory for new command!\r\n");
    }
}

void triggerSignalStrength(void* param)
{
    CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIGNALSTRENGTH], NULL, REQ_ID_NONE,
            "AT+CSQ\r", &CTE::ParseUnsolicitedSignalStrength);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerSignalStrength() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerSignalStrength() - Unable to allocate memory for new command!\r\n");
    }
}

void triggerSMSAck(void* param)
{
    CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSACKNOWLEDGE],
            NULL, REQ_ID_NONE, "AT+CNMA=1\r");

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerSMSAck() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerSMSAck() - Unable to allocate memory for new command!\r\n");
    }
}

void triggerQuerySimSmsStoreStatus(void* param)
{
    CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS],
                             NULL, REQ_ID_NONE, "AT+CPMS?\r", &CTE::ParseQuerySimSmsStoreStatus);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerQuerySimSmsStoreStatus() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerQuerySimSmsStoreStatus() - Unable to allocate memory for new command!\r\n");
    }
}

void triggerUSSDNotification(void* param)
{
    P_ND_USSD_STATUS pUssdStatus = (P_ND_USSD_STATUS)param;

    RIL_onUnsolicitedResponse (RIL_UNSOL_ON_USSD, pUssdStatus, sizeof(S_ND_USSD_POINTERS));

    free(pUssdStatus);
}

// [in] param = context id.
void triggerDeactivateDataCall(void* param)
{
    RIL_LOG_VERBOSE("triggerDeactivateDataCall() - Enter\r\n");

    UINT32 uiCID;
    REQUEST_DATA rReqData;
    BOOL bSuccess = FALSE;
    CCommand* pCmd = NULL;
    UINT32* pCID = NULL;

    if (param == NULL)
        return;

    uiCID = (UINT32)param;

    pCID = (UINT32*)malloc(sizeof(UINT32));
    if (NULL == pCID)
        return;

    *pCID = uiCID;
    memset(&rReqData, 0, sizeof(REQUEST_DATA));

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                                                "AT+CGACT=0,%d\r", uiCID))
    {
        RIL_LOG_CRITICAL("triggerDeactivateDataCall() - Unable to create CGACT command!\r\n");
        goto Error;
    }

    rReqData.pContextData = pCID;
    rReqData.cbContextData = sizeof(UINT32);

    pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DEACTIVATEDATACALL],
                            NULL, ND_REQ_ID_DEACTIVATEDATACALL, rReqData,
                            &CTE::ParseDeactivateDataCall,
                            &CTE::PostDeactivateDataCallCmdHandler);

    if (pCmd)
    {
        pCmd->SetHighPriority();
        if (!CCommand::AddCmdToQueue(pCmd, TRUE))
        {
            RIL_LOG_CRITICAL("triggerDeactivateDataCall() - Unable to queue command!\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerDeactivateDataCall() - Unable to allocate memory for new command!\r\n");
    }

    bSuccess = TRUE;
Error:
    if (!bSuccess)
    {
        free(pCID);
        delete pCmd;
    }

    RIL_LOG_VERBOSE("triggerDeactivateDataCall() - Exit\r\n");
}


// [in] param = RIL token
void triggerManualNetworkSearch(void* param)
{
    RIL_RESULT_CODE res = CTE::GetTE().RequestQueryAvailableNetworks((RIL_Token)param, NULL, 0);

    if (res != RRIL_RESULT_OK)
    {
        RIL_onRequestComplete((RIL_Token)param, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void triggerQueryCEER(void* param)
{
    CCommand* pCmd = new CCommand(RIL_CHANNEL_DLC8, NULL, REQ_ID_NONE,
                                    "AT+CEER\r",
                                    &CTE::ParseLastDataCallFailCause);

    if (pCmd)
    {
        pCmd->SetHighPriority();
        if (!CCommand::AddCmdToQueue(pCmd, TRUE))
        {
            RIL_LOG_CRITICAL("triggerQueryCEER() - Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerQueryCEER() - Unable to allocate memory for new command!\r\n");
    }
}


void requestEstablishedPDPList(void* param)
{
    REQUEST_DATA rReqData;
    memset(&rReqData, 0, sizeof(REQUEST_DATA));

    RIL_LOG_VERBOSE("requestEstablishedPDPList - uiCID:%d\r\n", (UINT32)param);

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGCONTRDP=%d\r",
            (UINT32)param))
    {
        CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SETUPDEFAULTPDP],
                NULL, ND_REQ_ID_SETUPDEFAULTPDP,
                rReqData, &CTE::ParseEstablishedPDPList);
        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("requestEstablishedPDPList - "
                        "Unable to queue AT+CGCONTRDP command!\r\n");
                delete pCmd;
            }
        }
    }

}
