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
#include "../util.h"

void notifyChangedCallState(void *param)
{
    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

void triggerHangup(UINT32 uiCallId)
{
    REQUEST_DATA rReqData;

    memset(&rReqData, 0, sizeof(REQUEST_DATA));
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XSETCAUSE=1,21;+CHLD=1%u\r", uiCallId))
    {
        RIL_LOG_CRITICAL("triggerHangup() - ERROR: Unable to create hangup command!\r\n");
        return;
    }

    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_HANGUP], NULL, REQ_ID_NONE, rReqData);
    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd, TRUE))
        {
            RIL_LOG_CRITICAL("triggerHangup() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerHangup() - ERROR: Unable to allocate memory for new command!\r\n");
    }
}

void triggerSignalStrength(void *param)
{
    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SIGNALSTRENGTH], NULL, REQ_ID_NONE, "AT+CSQ\r", &CTE::ParseUnsolicitedSignalStrength);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerSignalStrength() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerSignalStrength() - ERROR: Unable to allocate memory for new command!\r\n");
    }
}

void triggerSMSAck(void *param)
{
    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SMSACKNOWLEDGE], NULL, REQ_ID_NONE, "AT+CNMA=1\r");

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerSMSAck() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerSMSAck() - ERROR: Unable to allocate memory for new command!\r\n");
    }
}

void triggerQuerySimSmsStoreStatus(void *param)
{
    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS],
                                   NULL, REQ_ID_NONE, "AT+CPMS?\r", &CTE::ParseQuerySimSmsStoreStatus);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerQuerySimSmsStoreStatus() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerQuerySimSmsStoreStatus() - ERROR: Unable to allocate memory for new command!\r\n");
    }
}

void triggerUSSDNotification(void *param)
{
    P_ND_USSD_STATUS pUssdStatus = (P_ND_USSD_STATUS)param;

    RIL_onUnsolicitedResponse (RIL_UNSOL_ON_USSD, pUssdStatus, sizeof(S_ND_USSD_POINTERS));

    free(pUssdStatus);
}

void triggerDataCallListChanged(void *param)
{
    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_PDPCONTEXTLIST_UNSOL], NULL, ND_REQ_ID_PDPCONTEXTLIST_UNSOL, "AT+CGACT?;+CGDCONT?\r", &CTE::ParseDataCallListChanged);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerDataCallListChanged() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerDataCallListChanged() - ERROR: Unable to allocate memory for new command!\r\n");
    }
}


//  [in] param = context id.
void triggerDeactivateDataCall(void *param)
{
    UINT32 nCID;
    REQUEST_DATA rReqData;

    if (param == NULL)
       return;

    nCID = (UINT32)param;

    memset(&rReqData, 0, sizeof(REQUEST_DATA));
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%d\r", nCID))
    {
        RIL_LOG_CRITICAL("triggerDeactivateDataCall() - ERROR: Unable to create CGACT command!\r\n");
        return;
    }
    rReqData.pContextData = param;
    CCommand * pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_DEACTIVATEDATACALL], NULL, ND_REQ_ID_DEACTIVATEDATACALL, rReqData, &CTE::ParseDeactivateDataCall);

    if (pCmd)
    {
        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("triggerDeactivateDataCall() - ERROR: Unable to queue command!\r\n");
            delete pCmd;
            pCmd = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("triggerDeactivateDataCall() - ERROR: Unable to allocate memory for new command!\r\n");
    }

}
