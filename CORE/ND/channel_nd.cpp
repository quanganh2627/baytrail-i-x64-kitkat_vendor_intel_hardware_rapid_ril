////////////////////////////////////////////////////////////////////////////
// channel_nd.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements the CChannel class, which provides the
//    infrastructure for the various AT Command channels.
//
// Author:  Francesc Vilarino
// Created: 2009-06-18
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 18/06  FV       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "thread_ops.h"
#include "repository.h"
#include "command.h"
#include "response.h"
#include "cmdcontext.h"
#include "rildmain.h"
#include "channel_nd.h"

#undef __out_arg

namespace
{
const bool ATCMD_LOG_RESPONSE (true);
}


CChannel::CChannel(UINT32 uiChannel)
: CChannelBase(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel::CChannel() - Enter\r\n");

    CRepository repository;
    int iTemp = 0;

    if (repository.Read(g_szGroupRILSettings, g_szMaxCommandTimeouts, iTemp))
    {
        m_uiMaxTimeouts = (UINT32)iTemp;
    }
    else
    {
        m_uiMaxTimeouts = 2;
    }

    RIL_LOG_VERBOSE("CChannel::CChannel() - Exit\r\n");
}

CChannel::~CChannel()
{
    RIL_LOG_VERBOSE("CChannel::~CChannel() - Enter\r\n");

    delete m_pResponse;
    m_pResponse = NULL;

    RIL_LOG_VERBOSE("CChannel::~CChannel() - Exit\r\n");
}

//
// Send an RIL command
//
BOOL CChannel::SendCommand(CCommand*& rpCmd)
{
    RIL_LOG_VERBOSE("CChannel::SendCommand() - Enter\r\n");

    BYTE*           pATCommand = NULL;
    CResponse*      pResponse = NULL;
    RIL_RESULT_CODE resCode = RRIL_E_UNKNOWN_ERROR;
    BOOL            bResult = FALSE;
    CEvent*         pQuitEvent = NULL;

    if (NULL == rpCmd)
    {
        RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] NULL params [0x%08x] [0x%08x]\r\n", m_uiRilChannel, m_pSystemManager, rpCmd);
        goto Error;
    }

    RIL_LOG_VERBOSE("CChannel::SendCommand() - DEBUG: chnl=[%d] Executing command with ID=[0x%08X,%d]\r\n", m_uiRilChannel, rpCmd->GetRequestID(), (int)rpCmd->GetRequestID());

    // call Silo hook - PreSendCommandHook
    if (!PreSendCommandHook(rpCmd, pResponse))
    {
        RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] PreSendCommandHook returned FALSE\r\n", m_uiRilChannel);
        goto Error;
    }

    if (NULL == rpCmd->GetATCmd1())
    {
        // noop operation
        if (!ProcessNoop(pResponse))
            goto Error;
    }
    //  Feb 10/2011 - No need to check for this in this Infineon modem.  This modem is always
    //                responsive to AT commands.  However, if modem is being reset then
    //                possibly need to spoof responses.
    else if ( /*g_RadioState.IsRadioOff() && !rpCmd->IsHighPriority() */ FALSE )
    {
        // cannot send command as radio is off
        if (!RejectRadioOff(pResponse))
            goto Error;
    }
    else
    {
        // process command here
        int nNumRetries = (int)rpCmd->GetRetries();
        while (nNumRetries-- >= 0)
        {
            UINT32 uiBytesWritten = 0;

            pATCommand = (BYTE *) rpCmd->GetATCmd1();

            // set flag for response thread
            SetCmdThreadBlockedOnRxQueue();

            // empty the queue before sending the command
            g_pRxQueue[m_uiRilChannel]->MakeEmpty();
            if (NULL != m_pResponse)
                m_pResponse->FreeData();

            // write the command out to the com port
            if (!WriteToPort(pATCommand, strlen(pATCommand), uiBytesWritten))
            {
                // ignore the error and soldier on, something may have been written
                // to the modem so wait for a potential response
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] Error writing command: %s\r\n",
                                m_uiRilChannel,
                                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
            }

            if (strlen(pATCommand) != uiBytesWritten)
            {
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] Only wrote [%d] chars of command to port: %s\r\n",
                                m_uiRilChannel,
                                uiBytesWritten,
                                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
            }

            // retrieve response from modem
            resCode = GetResponse(rpCmd, pResponse);

            // response received, clear flag
            ClearCmdThreadBlockedOnRxQueue();

            if (!pResponse)
            {
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] No response received!\r\n", m_uiRilChannel);
                goto Error;
            }

            if (!pResponse->IsTimedOutFlag())
            {
                //  Our response is complete!
                break;
            }
            else
            {
                //  Our response timed out, retry
                RIL_LOG_INFO("CChannel::SendCommand() - Attempting retry  nNumRetries remaining=[%d]\r\n", nNumRetries+1);
            }
        }

        // call Silo hook - PostSendCommandHook
        if (!PostSendCommandHook(rpCmd, pResponse))
        {
            RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] SendRILCmdParseResponsePostSend returned FALSE\r\n", m_uiRilChannel);
            goto Error;
        }

    }

    /************** ALL THIS SHOULD BE IN A CONTEXT, IF REQUIRED *************/
#if 0
#ifdef GPRS_CONTEXT_CACHING
    if ((ND_REQ_ID_SETUPDEFAULTPDP == rpCmd->GetReqID()) &&
        (!rpCmd->FNoOp()) &&
        (RRIL_RESULT_OK == pResponse->GetNotifyCode()))
    {
        // We just set the GPRS context.  Don't set the same context again
        // unless the radio reboots.
        // NOTE: We assume pCmdData is a valid string here!
        UpdateGPRSContextCommandCache((BYTE*)pCmdData);
    }
#endif // GPRS_CONTEXT_CACHING

#endif  // 0

    // Handle the response
    // TODO: fix these dummies
    if (!ParseResponse(rpCmd, pResponse/*, dummy1, dummy2*/))
    {
        goto Error;
    }

    if ((NULL != rpCmd) || (NULL != pResponse))
    {
        RIL_LOG_CRITICAL("CChannel::SendCommand() : ERROR : rpCmd or pResponse was not NULL\r\n");
        goto Error;
    }

    bResult = TRUE;

Error:
    if (!bResult)
    {
        delete pResponse;
        pResponse = NULL;
    }

    if (pQuitEvent)
    {
        delete pQuitEvent;
        pQuitEvent = NULL;
    }

    RIL_LOG_VERBOSE("CChannel::SendCommand() - Exit\r\n");
    return bResult;
}

//
// Get modem response from queue and send 2nd AT command if present
//
RIL_RESULT_CODE CChannel::GetResponse(CCommand*& rpCmd, CResponse*& rpResponse)
{
    RIL_RESULT_CODE resCode = RIL_E_GENERIC_FAILURE;
    BYTE*           pATCommand;

    RIL_LOG_VERBOSE("CChannel::GetResponse() - Enter\r\n");

    // Get the response out of the Response Queue
    resCode = ReadQueue(rpResponse, rpCmd->GetTimeout());

#if 0    //  This is for testing purposes only. (radio reboot)
    static int nCount = 0;
    if (ND_REQ_ID_SIGNALSTRENGTH == rpCmd->GetRequestID())
    {
        nCount++;
        RIL_LOG_INFO("COUNT = %d\r\n", nCount);
        if (nCount == 4)
        {
            TriggerRadioErrorAsync(eRadioError_ChannelDead, __LINE__, __FILE__);
            goto Error;
        }
    }
#endif // 0

#if 0
    static int nCount = 0;
    if (ND_REQ_ID_SCREENSTATE == rpCmd->GetRequestID())
    {
        nCount++;
        RIL_LOG_INFO("COUNT = %d\r\n", nCount);
        if (nCount == 2)
        {
            TriggerRadioErrorAsync(eRadioError_ChannelDead, __LINE__, __FILE__);
            goto Error;
        }
    }
#endif // 0

    if (rpResponse && rpResponse->IsTimedOutFlag())
    {
        RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: Command timed out!\r\n");
        HandleTimedOutError(TRUE);

        //  Send extra AT to possibly abort command in progress
        UINT32 uiBytesWritten = 0;
        BYTE szATCmd[] = "ATE0V1\r";
        WriteToPort(szATCmd, strlen(szATCmd), uiBytesWritten);

        CResponse *pRspTemp = NULL;
        RIL_RESULT_CODE resTmp = ReadQueue(pRspTemp, 500); //  wait 0.5 seconds for response
        delete pRspTemp;

        goto Error;
    }
    else if (RIL_E_SUCCESS != resCode)
    {
        RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: Failed read from queue\r\n");
        goto Error;
    }
    else
    {
        // Didn't time out and we got a response, clear the channel's timeout counter
        HandleTimedOutError(FALSE);
    }

    if ((NULL != rpCmd->GetATCmd2()) &&
        (NULL != rpResponse) &&
        (RIL_E_SUCCESS == rpResponse->GetResultCode()))
    {
        pATCommand = (BYTE *) rpCmd->GetATCmd2();
        UINT32 uiBytesWritten = 0;

        //  Temp fix (delay before sending 2nd part of XRAT command)
        if (ND_REQ_ID_SETPREFERREDNETWORKTYPE == rpCmd->GetRequestID())
        {
            const int dwSleep = 500;
            RIL_LOG_INFO("CChannel::GetResponse() - chnl=[%d]  BEGIN SLEEP=[%d]ms\r\n", m_uiRilChannel, dwSleep);
            Sleep(dwSleep);
            RIL_LOG_INFO("CChannel::GetResponse() - chnl=[%d]  END SLEEP=[%d]ms\r\n", m_uiRilChannel, dwSleep);
        }

        // send 2nd phase of command
        SetCmdThreadBlockedOnRxQueue();

        if (!WriteToPort(pATCommand, strlen(pATCommand), uiBytesWritten))
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: chnl=[%d] Error sending 2nd command: %s\r\n",
                          m_uiRilChannel,
                          CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
            // ignore error and wait for a modem response, or time out
        }

        if (strlen(pATCommand) != uiBytesWritten)
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: chnl=[%d] Could only write [%d] chars of 2nd command: %s\r\n",
                m_uiRilChannel,
                uiBytesWritten,
                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
        }

        // wait for the secondary response
        delete rpResponse;
        rpResponse = NULL;
        resCode = ReadQueue(rpResponse, rpCmd->GetTimeout());

        if (rpResponse && rpResponse->IsTimedOutFlag())
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: Command's second part timed out!\r\n");
            HandleTimedOutError(TRUE);


            //  Send extra AT to possibly abort command in progress
            if (ND_REQ_ID_SENDSMS == rpCmd->GetRequestID() ||
                ND_REQ_ID_SENDSMSEXPECTMORE == rpCmd->GetRequestID())
            {

                UINT32 uiBytesWritten = 0;
                BYTE szATCmd[] = "\x1b\r";
                WriteToPort(szATCmd, strlen(szATCmd), uiBytesWritten);
            }
            else
            {
                UINT32 uiBytesWritten = 0;
                BYTE szATCmd[] = "ATE0V1\r";
                WriteToPort(szATCmd, strlen(szATCmd), uiBytesWritten);
            }
            CResponse *pRspTemp = NULL;
            RIL_RESULT_CODE resTmp = ReadQueue(pRspTemp, 500); //  wait 0.5 seconds for response
            delete pRspTemp;


            goto Error;
        }
        else if (RIL_E_SUCCESS != resCode)
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: Failed read from queue for command's second response\r\n");
            goto Error;
        }
        else
        {
            // Didn't time out and we got a response, clear the channel's timeout counter
            HandleTimedOutError(FALSE);
        }
    }

Error:
    return resCode;
}

BOOL CChannel::ProcessNoop(CResponse*& rpResponse)
{
    // no-op command, nothing to send - processed here
    // allocate a new response
    rpResponse = new CResponse(this);
    if (NULL == rpResponse)
    {
        // signal critical error for low memory
        TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
        return FALSE;
    }

    // fake an OK response for the no-op command
    rpResponse->SetResultCode(RIL_E_SUCCESS);
    rpResponse->SetUnsolicitedFlag(FALSE);

    return TRUE;
}

BOOL CChannel::RejectRadioOff(CResponse*& rpResponse)
{
    // the radio is off and this command cannot be sent
    // in the current state

    // allocate a new response
    rpResponse = new CResponse(this);
    if (NULL == rpResponse)
    {
        // signal critical error for low memory
        TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
        return FALSE;
    }

    rpResponse->SetResultCode(RIL_E_RADIO_NOT_AVAILABLE);
    rpResponse->SetUnsolicitedFlag(FALSE);

    return TRUE;
}

//
// Handle response to an AT command
//
BOOL CChannel::ParseResponse(CCommand*& rpCmd, CResponse*& rpRsp/*, BOOL& rfHungUp, BOOL& rfRadioOff*/)
{
    RIL_LOG_VERBOSE("CChannel::ParseResponse() - Enter\r\n");

    void* pData = NULL;
    UINT32 uiDataSize = 0;
    RIL_RESULT_CODE resCode;
    //CNotificationData* pnd = NULL;
    BOOL bResult = FALSE;

    if ((NULL == rpCmd) || (NULL == rpRsp))
    {
        RIL_LOG_CRITICAL("CChannel::ParseResponse() : ERROR : Invalid arguments\r\n");
        goto Error;
    }

    //rfHungUp = FALSE;

    //  Call our hook
    if (!PreParseResponseHook(rpCmd, rpRsp))
    {
        RIL_LOG_CRITICAL("CChannel::ParseResponse() - ERROR: chnl=[%d] PreParseResponseHook FAILED!\r\n", m_uiRilChannel);
        goto Error;
    }

    // parse the response if successful, or if client requires parse in all circumstances
    if ((RIL_E_SUCCESS == rpRsp->GetResultCode()) || rpCmd->IsAlwaysParse())
    {
        (void) rpRsp->ParseResponse(rpCmd);
    }

    // call the context
    if (rpCmd->GetContext())
    {
        rpCmd->GetContext()->Execute(RIL_E_SUCCESS == rpRsp->GetResultCode(), rpRsp->GetErrorCode());
    }

    /******** MOVE TO CMD CONTEXT *********/
#if 0


    // If we got a CONNECT response, it must be a response to successful ATD or ATA -- change it to OK
    if (RRIL_NOTIFY_CONNECT == rpRsp->GetNotifyCode())
    {
        CRepository repository;

        rpRsp->ConnectToOK();

        // Some networks seem to require a brief delay after a data connection is made
        // before data can be sent via the connection. We pause here before sending the
        // connect response to the client. The delay is configured using a registry value.
        // If the value is not present, then there is no delay.

        int iTemp = 0;

        if (repository.Read(g_szGroupNetworkSettings, g_szConnectRspDelay, iTemp))
        {
            RIL_LOG_INFO("CChannel::HandleRsp() : chnl=[%d] BEGIN ConnectResponseDelay Sleep(%d)\r\n", m_uiRilChannel, iTemp);
            Sleep(iTemp);
            RIL_LOG_INFO("CChannel::HandleRsp() : chnl=[%d] END ConnectResponseDelay Sleep(%d)\r\n", m_uiRilChannel, iTemp);
        }
    }
    else
    {
        // For non-CONNECT responses, if we have a notification to send upon success, do it now
        pnd = rpCmd->GiveUpNotificationData();
        if (pnd)
        {
            if (RRIL_RESULT_OK == rpRsp->GetNotifyCode() &&
               (!pnd->FDelayedInitFromRsp() || pnd->FinishInitFromRspBlob(*rpRsp)))
            {
                RIL_LOG_INFO("CChannel::HandleRsp() : chnl=[%d] Broadcasting Notification: 0x%08X\r\n", m_uiRilChannel, pnd->GetCode());
                RIL_onUnsolicitedResponse(pnd->GetCode(), pnd->GetBlob(), pnd->GetSize());
            }
            delete pnd;
            pnd = NULL;
        }
    }
#endif  // 0

    // Forward the response we got to the handle that sent the command
    //  Call our hook
    RIL_LOG_VERBOSE("CChannel::ParseResponse() - chnl=[%d] calling PostParseResponseHook\r\n", m_uiRilChannel);
    bResult = PostParseResponseHook(rpCmd, rpRsp);
    if (!bResult)
        goto Error;

    // Forward the response we got to the handle that sent the command
    rpRsp->GetData(pData, uiDataSize);
    if (0 != rpCmd->GetToken())
    {
        UINT32 uiReqID = rpCmd->GetRequestID();
        RIL_LOG_INFO("CChannel::ParseResponse() - Complete for token 0x%08x, resultcode: %d, errno: %d, reqID=[%d]\r\n", rpCmd->GetToken(), rpRsp->GetResultCode(), rpRsp->GetErrorCode(), uiReqID);
        RIL_onRequestComplete(rpCmd->GetToken(), (RIL_Errno) rpRsp->GetResultCode(), (void*)pData, uiDataSize);


        //  Check for identical request IDs.
        switch(uiReqID)
        {
            case ND_REQ_ID_REGISTRATIONSTATE:
            case ND_REQ_ID_GPRSREGISTRATIONSTATE:
            case ND_REQ_ID_OPERATOR:
            case ND_REQ_ID_QUERYNETWORKSELECTIONMODE:
                FindIdenticalRequestsAndSendResponses(uiReqID, (RIL_Errno) rpRsp->GetResultCode(), (void*)pData, uiDataSize);
                break;

            default:
                break;
        }
    }
    else
    {
        RIL_LOG_INFO("CChannel::ParseResponse() - Complete for token 0x%08x, resultcode: %d, errno: %d\r\n", rpCmd->GetToken(), rpRsp->GetResultCode(), rpRsp->GetErrorCode());
    }

    rpRsp->FreeData();
    pData  = NULL;
    uiDataSize = 0;

    bResult = TRUE;

Error:
    //delete pnd;
    delete rpCmd;
    delete rpRsp;
    rpCmd = NULL;
    rpRsp = NULL;

    RIL_LOG_VERBOSE("CChannel::ParseResponse() - Exit\r\n");
    return bResult;
}


BOOL CChannel::FindIdenticalRequestsAndSendResponses(UINT32 uiReqID, RIL_Errno eErrNo, void *pResponse, size_t responseLen)
{
    RIL_LOG_VERBOSE("CChannel::FindIdenticalRequestsAndSendResponses() - Enter\r\n");

    CCommand **pCmdArray = NULL;
    int nNumOfCommands = 0;

    //  The following function returns us an array of CCommands, and the size of the returned array.
    g_pTxQueue[m_uiRilChannel]->GetAllQueuedObjects(pCmdArray, nNumOfCommands);

    for (int i=0; i<nNumOfCommands; i++)
    {
        RIL_LOG_VERBOSE("CChannel::FindIdenticalRequestsAndSendResponses() - nNumOfCommands=[%d] reqID to match=[%d]  i=[%d] reqID=[%d]\r\n", nNumOfCommands, uiReqID, i, pCmdArray[i]->GetRequestID());
        if (pCmdArray[i]->GetRequestID() == uiReqID)
        {
            //  Dequeue the object, send the response.  Then free the CCommand.
            g_pTxQueue[m_uiRilChannel]->DequeueByObj(pCmdArray[i]);

            RIL_LOG_INFO("CChannel::FindIdenticalRequestsAndSendResponses() - Found match for ReqID=[%d] at index=[%d]\r\n", uiReqID, i);
            RIL_LOG_INFO("CChannel::FindIdenticalRequestsAndSendResponses() - Complete for token 0x%08x, error: %d,  reqID=[%d]\r\n", pCmdArray[i]->GetToken(), eErrNo, uiReqID);
            RIL_onRequestComplete(pCmdArray[i]->GetToken(), eErrNo, pResponse, responseLen);

            delete pCmdArray[i];
            pCmdArray[i] = NULL;
        }
    }

    delete []pCmdArray;
    pCmdArray = NULL;

    RIL_LOG_VERBOSE("CChannel::FindIdenticalRequestsAndSendResponses() - Exit\r\n");
    return true;
}


BOOL CChannel::ProcessModemData(BYTE *szRxBytes, UINT32 uiRxBytesSize)
{
    RIL_LOG_VERBOSE("CChannel::ProcessModemData() - Enter\r\n");
    BOOL bResult = FALSE;

    if (!szRxBytes || uiRxBytesSize == 0)
    {
        RIL_LOG_INFO("CChannel::ProcessModemData() - ERROR - Invalid params [0x%08x] [%d]\r\n", szRxBytes, uiRxBytesSize);
        goto Error;
    }

    RIL_LOG_INFO("CChannelBase::ProcessModemData() - INFO: chnl=[%d] size=[%d] RX [%s]\r\n", m_uiRilChannel, uiRxBytesSize, CRLFExpandedString(szRxBytes,uiRxBytesSize).GetString());

    if (!m_pResponse)
    {
        m_pResponse = new CResponse(this);
        if (!m_pResponse)
        {
            // critically low on memory
            TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
            goto Error;
        }
    }

    // append data to our response buffer
    if (!m_pResponse->Append(szRxBytes, uiRxBytesSize))
    {
        RIL_LOG_CRITICAL("CChannel::ProcessModemData() - ERROR: chnl=[%d] Append failed\r\n", m_uiRilChannel);
        goto Error;
    }

    if (m_bTimeoutWaitingForResponse)
    {
        m_pResponse->SetCorruptFlag(TRUE);
        m_bTimeoutWaitingForResponse = FALSE;
    }

    // process the data received thus far
    while (m_pResponse->IsCompleteResponse())
    {
        // yes; transfer data to a new buffer; this leaves data in the member variable
        //  buffer if we have more than a complete response
        CResponse* pResponse = NULL;
        if (!CResponse::TransferData(m_pResponse, pResponse))
        {
            RIL_LOG_CRITICAL("CChannel::ProcessModemData() - ERROR: chnl=[%d] TransferData failed\r\n", m_uiRilChannel);
            goto Error;
        }

        // process the response
        if (!ProcessResponse(pResponse))
        {
            RIL_LOG_CRITICAL("CChannel::ProcessModemData() - ERROR: chnl=[%d] ProcessResponse failed\r\n", m_uiRilChannel);
            goto Error;
        }
    }

    bResult = TRUE;

Error:
    RIL_LOG_VERBOSE("CChannel::ProcessModemData() - Exit [%d]\r\n", bResult);
    return bResult;
}

BOOL CChannel::ProcessResponse(CResponse*& rpResponse)
{
    BOOL bResult = FALSE;

    RIL_LOG_VERBOSE("CChannel::ProcessResponse : chnl=[%d] Enter\r\n", m_uiRilChannel);
    if (rpResponse->IsUnrecognizedFlag())
    {
        // garbage in buffer, discard
        RIL_LOG_INFO("CChannel::ProcessResponse : chnl=[%d] Unidentified response size [%d]  %s\r\n",
                       m_uiRilChannel,
                       rpResponse->Size(),
                       CRLFExpandedString(rpResponse->Data(), rpResponse->Size()).GetString());
    }
    else if (rpResponse->IsUnsolicitedFlag())
    {
        // unsolicited notification -> broadcast to all clients
        if ((0 != rpResponse->GetResultCode()))
        {
            void*   pData;
            UINT32  uiDataSize;

            rpResponse->GetData(pData, uiDataSize);
            RIL_LOG_INFO("CChannel::ProcessResponse : chnl=[%d] unsolicited notification resultcode=[%d]\r\n",
                m_uiRilChannel,
                rpResponse->GetResultCode());
            RIL_onUnsolicitedResponse(rpResponse->GetResultCode(), pData, uiDataSize);
            rpResponse->FreeData();
        }
    }
    else
    {
        // command result
        if (IsCmdThreadBlockedOnRxQueue())
        {
            // response expected, stop waiting for response and queue it
            ClearCmdThreadBlockedOnRxQueue();

            //  Make sure the command thread is listening for a response in proper state.
            //  Sometimes, the Rx Queue IsEmpty() is TRUE, but we enqueue and signal just before
            //  the RxQueueEvent Reset gets called in CChannel::ReadQueue().
            //const unsigned int dwSleep = 75;
            //RIL_LOG_INFO("CChannel::ProcessResponse() - BEGIN SLEEP %u\r\n", dwSleep);
            //Sleep(dwSleep);
            //RIL_LOG_INFO("CChannel::ProcessResponse() - END SLEEP %u\r\n", dwSleep);

            //RIL_LOG_INFO("CChannel::ProcessResponse() - Enqueue response  resultcode=[%d]\r\n", rpResponse->GetResultCode() );

            // Queue the command response
            if (!g_pRxQueue[m_uiRilChannel]->Enqueue(rpResponse))
            {
                RIL_LOG_CRITICAL("CChannel::ProcessResponse() - ERROR: Unable to Enqueue response\r\n");
                goto Error;
            }

            // Queue has ownership of this now
            rpResponse = NULL;

            // signal Tx thread
            //RIL_LOG_INFO("CChannel::ProcessResponse : Signal g_RxQueueEvent BEGIN\r\n");
            (void) CEvent::Signal(g_RxQueueEvent[m_uiRilChannel]);
            //(void) CEvent::Reset(g_RxQueueEvent[m_uiRilChannel]);
            //RIL_LOG_INFO("CChannel::ProcessResponse : Signal g_RxQueueEvent END\r\n");

        }
        else
        {
            RIL_LOG_INFO("CChannel::ProcessResponse : chnl=[%d] Non recognized response: [%d] %s\r\n",
                           m_uiRilChannel,
                           rpResponse->Size(),
                           CRLFExpandedString(rpResponse->Data(), rpResponse->Size()).GetString());
        }
    }

    bResult = TRUE;

Error:
    // we took ownership of the buffer, free it
    delete rpResponse;
    rpResponse = NULL;

    RIL_LOG_VERBOSE("CChannel::ProcessResponse : chnl=[%d] Exit\r\n", m_uiRilChannel);
    return bResult;
}


RIL_RESULT_CODE CChannel::ReadQueue(CResponse*& rpResponse, UINT32 uiTimeout)
{
    RIL_RESULT_CODE resCode = RIL_E_GENERIC_FAILURE;

    if (g_pRxQueue[m_uiRilChannel]->IsEmpty())
    {
        CEvent *rgpEvents[] = {g_RxQueueEvent[m_uiRilChannel], m_pSystemManager->GetCancelEvent()};

        // wait for response
        //RIL_LOG_INFO("CChannel::ReadQueue() - QUEUE EMPTY, WAITING FOR RxQueueEvent...\r\n");
        //CEvent::Reset(g_RxQueueEvent[m_uiRilChannel]);
        UINT32 uiRet = CEvent::WaitForAnyEvent(2, rgpEvents, uiTimeout);
        //CEvent::Reset(g_RxQueueEvent[m_uiRilChannel]);
        switch(uiRet)
        {
            case WAIT_EVENT_0_SIGNALED:
                //RIL_LOG_INFO("CChannel::ReadQueue() - chnl=[%d] RxQueueEvent WAIT_EVENT_0_SIGNALED\r\n", m_uiRilChannel);
                // object pushed to queue
                break;

            case WAIT_EVENT_0_SIGNALED + 1:
                // cancel event signalled
                RIL_LOG_CRITICAL("CChannel::ReadQueue() : chnl=[%d] Cancel signalled\r\n", m_uiRilChannel);
                goto Error;
                break;  // unreachable

            case WAIT_TIMEDOUT:
                // command timed-out
                RIL_LOG_VERBOSE("CChannel::ReadQueue() : chnl=[%d] Setting timed out flag!\r\n", m_uiRilChannel);
                rpResponse = new CResponse(this);
                if (!rpResponse)
                {
                    RIL_LOG_CRITICAL("CChannel::ReadQueue() - chnl=[%d] ERROR: Failed to allocate memory for response!\r\n", m_uiRilChannel);
                    goto Error;
                }

                rpResponse->SetResultCode(resCode);
                rpResponse->SetUnsolicitedFlag(FALSE);
                rpResponse->SetTimedOutFlag(TRUE);

                goto Error;
                break;  // unreachable

            default:
                RIL_LOG_INFO("CChannel::ReadQueue() : chnl=[%d] unknown error\r\n", m_uiRilChannel);
                goto Error;
                break;  // unreachable
        }
    }

    //RIL_LOG_INFO("CChannel::ReadQueue() : chnl=[%d] g_RxQueue Dequeue BEGIN\r\n", m_uiRilChannel);
    // if we get here, there is something in the queue; get it
    //CEvent::Reset(g_RxQueueEvent[m_uiRilChannel]);
    if (!g_pRxQueue[m_uiRilChannel]->Dequeue(rpResponse))
    {
        RIL_LOG_CRITICAL("CChannel::ReadQueue() : ERROR : Dequeue failed\r\n", m_uiRilChannel);
        goto Error;
    }
    else
    {
        CEvent::Reset(g_RxQueueEvent[m_uiRilChannel]);
    }
    //RIL_LOG_INFO("CChannel::ReadQueue() : chnl=[%d] g_RxQueue Dequeue END\r\n", m_uiRilChannel);

    resCode = RIL_E_SUCCESS;

Error:
    RIL_LOG_VERBOSE("CChannel::ReadQueue() - Exit [%d]\r\n", resCode);
    return resCode;
}


