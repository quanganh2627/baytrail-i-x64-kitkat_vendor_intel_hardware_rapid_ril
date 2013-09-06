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

#include <stdio.h>
#include "types.h"
#include "command.h"
#include "rildmain.h"
#include "rillog.h"
#include "te.h"
#include "util.h"
#include "oemhookids.h"
#include "channel_data.h"

void notifyChangedCallState(void* param)
{
    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

void triggerRadioOffInd(void* param)
{
    if (RADIO_STATE_UNAVAILABLE == CTE::GetTE().GetRadioState())
    {
       CTE::GetTE().SetRadioStateAndNotify(RRIL_RADIO_STATE_OFF);
    }
}

void triggerDataResumedInd(void* param)
{
    const int DATA_RESUMED = 1;
    sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND data;

    CTE::GetTE().SetDataSuspended(FALSE);

    data.command = RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND;
    data.status = DATA_RESUMED;

    RIL_onUnsolicitedResponse(RIL_UNSOL_OEM_HOOK_RAW, (void*)&data,
            sizeof(sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND));
}

void triggerDataSuspendInd(void* param)
{
    if (!CTE::GetTE().IsDataSuspended() || (RADIO_STATE_ON != CTE::GetTE().GetRadioState()))
        return;

    const int DATA_SUSPENDED = 0;
    sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND data;

    data.command = RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND;
    data.status = DATA_SUSPENDED;

    RIL_onUnsolicitedResponse(RIL_UNSOL_OEM_HOOK_RAW, (void*)&data,
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

void triggerQueryDefaultPDNContextParams(void* param)
{
    REQUEST_DATA rReqData;
    memset(&rReqData, 0, sizeof(REQUEST_DATA));
    CChannel_Data* pChannelData = NULL;
    UINT32 uiCID = 0;

    if (param == NULL)
        return;

    pChannelData = (CChannel_Data*)param;

    uiCID = pChannelData->GetContextID();

    RIL_LOG_VERBOSE("triggerQueryDefaultPDNContextParams - uiCID: %u\r\n", uiCID);

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGCONTRDP=%u\r",
            uiCID))
    {
        CCommand* pCmd = new CCommand(pChannelData->GetRilChannel(), NULL, ND_REQ_ID_NONE,
                rReqData, &CTE::ParseReadDefaultPDNContextParams,
                &CTE::PostReadDefaultPDNContextParams);
        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("triggerQueryDefaultPDNContextParams - "
                        "Unable to queue AT+CGCONTRDP command!\r\n");
                delete pCmd;
            }
        }
    }
}

// [in] param = 1 for mobile release and 0 for network release
void triggerDropCallEvent(void* param)
{
    sOEM_HOOK_RAW_UNSOL_CRASHTOOL_EVENT_IND data;
    char szBuffer[CRASHTOOL_BUFFER_SIZE];

    BOOL bMobileRelease = (1 == (UINT32)param);

    data.command = RIL_OEM_HOOK_RAW_UNSOL_CRASHTOOL_EVENT_IND;
    data.type = CRASHTOOL_STATS;
    PrintStringNullTerminate(data.name, CRASHTOOL_NAME_SIZE, "TFT_STAT_CDROP");
    data.nameSize = strnlen(data.name, CRASHTOOL_NAME_SIZE);

    // Pre-initialize all data size to 0
    for (int i = 0; i < CRASHTOOL_NB_DATA; i++)
    {
        data.dataSize[i] = 0;
    }

    // See the definition of sOEM_HOOK_RAW_UNSOL_CRASHTOOL_EVENT_IND in
    // CORE/oemhookids.h for the raw unsol content.
    if (bMobileRelease)
    {
        PrintStringNullTerminate(data.data0, CRASHTOOL_BUFFER_SIZE, "MOBILE RELEASE");
        data.dataSize[0] = strnlen(data.data0, CRASHTOOL_BUFFER_SIZE);
    }
    else
    {
        data.dataSize[0] = snprintf(data.data0, CRASHTOOL_BUFFER_SIZE, "%s",
                CTE::GetTE().GetLastCEER());
    }

    if (strlen(CTE::GetTE().GetNetworkData(LAST_NETWORK_CREG)) != 0)
    {
        data.dataSize[1] = snprintf(data.data1, CRASHTOOL_BUFFER_SIZE, "+CREG: %s;",
                CTE::GetTE().GetNetworkData(LAST_NETWORK_CREG));
    }

    if (strlen(CTE::GetTE().GetNetworkData(LAST_NETWORK_XREG)) != 0)
    {
        data.dataSize[1] += snprintf(szBuffer, CRASHTOOL_BUFFER_SIZE - data.dataSize[1],
                "+XREG: %s;", CTE::GetTE().GetNetworkData(LAST_NETWORK_XREG));
        strncat(data.data1, szBuffer, CRASHTOOL_BUFFER_SIZE);
    }

    if (strlen(CTE::GetTE().GetNetworkData(LAST_NETWORK_XCSQ)) != 0)
    {
        data.dataSize[2] = snprintf(data.data2, CRASHTOOL_LARGE_BUFFER_SIZE, "+XCSQ: %s;",
                CTE::GetTE().GetNetworkData(LAST_NETWORK_XCSQ));
    }

    data.dataSize[3] = snprintf(data.data3, CRASHTOOL_BUFFER_SIZE, "%s,%s,%s",
            CTE::GetTE().GetNetworkData(LAST_NETWORK_OP_NAME_NUMERIC),
            CTE::GetTE().GetNetworkData(LAST_NETWORK_LAC),
            CTE::GetTE().GetNetworkData(LAST_NETWORK_CID));

    data.dataSize[4] = snprintf(data.data4, CRASHTOOL_LARGE_BUFFER_SIZE, "%s",
            CTE::GetTE().GetNetworkData(LAST_NETWORK_OP_NAME_SHORT));

    RIL_onUnsolicitedResponse (RIL_UNSOL_OEM_HOOK_RAW, (void*)&data,
            sizeof(sOEM_HOOK_RAW_UNSOL_CRASHTOOL_EVENT_IND));
}

void triggerCellInfoList(void* param)
{
    // Get the CellInfo rate and compare.
    // if the newly set rate is less or equal,continue reading cellinfo from modem
    // if it is more, then start a new timed call back with the difference in timeout
    RIL_LOG_VERBOSE("triggerCellInfoList- Enter\r\n");
    UINT32 uiTeRate = CTE::GetTE().GetCellInfoListRate();
    UINT32 uiRate = (UINT32)param;
    RIL_LOG_INFO("triggerCellInfoList- StoredRate %d Rate with callback %d\r\n", uiTeRate, uiRate);
    // the settings have changed to not to report CELLINFO
    // TODO: 0 to check for changed values and report
    if (0 == uiTeRate || INT_MAX == uiTeRate)
    {
        CTE::GetTE().SetCellInfoTimerRunning(FALSE);
        RIL_LOG_INFO("triggerCellInfoList- Unsol cell info disabled: %d\r\n", uiTeRate);
    }
    else if (uiTeRate <= uiRate)
    {
        CCommand* pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_GETCELLINFOLIST],
                NULL, REQ_ID_NONE, "AT+XCELLINFO?\r", &CTE::ParseUnsolCellInfoListRate,
                &CTE::PostUnsolCellInfoListRate);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("triggerCellInfoList() - Unable to queue command!\r\n");
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("triggerCellInfoList() - "
                    "Unable to allocate memory for new command!\r\n");
        }
        CTE::GetTE().SetCellInfoTimerRunning(FALSE);
    }
    else
    {
         if (uiTeRate > uiRate)
         {
             RIL_requestTimedCallback(triggerCellInfoList,
                   (void*)uiTeRate, (uiTeRate - uiRate), 0);
         }
    }
    RIL_LOG_VERBOSE("triggerCellInfoList- Exit\r\n");
}
