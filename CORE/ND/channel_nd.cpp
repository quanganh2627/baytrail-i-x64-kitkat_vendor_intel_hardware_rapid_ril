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
#include "command.h"
#include "response.h"
#include "cmdcontext.h"
#include "rildmain.h"
#include "reset.h"
#include "channel_nd.h"


CChannel::CChannel(UINT32 uiChannel)
: CChannelBase(uiChannel),
  m_pResponse(NULL)
{
    RIL_LOG_VERBOSE("CChannel::CChannel() - Enter/Exit\r\n");
}

CChannel::~CChannel()
{
    RIL_LOG_VERBOSE("CChannel::~CChannel() - Enter\r\n");

    delete m_pResponse;
    m_pResponse = NULL;

    RIL_LOG_VERBOSE("CChannel::~CChannel() - Exit\r\n");
}

//
// Function to get ABORT timeout (in ms) from RIL request ID
//
UINT32 GetAbortTimeout(UINT32 reqID)
{
    const UINT32 DEFAULT_ABORT_TIMEOUT = 500;  //  Default ABORT timeout
    UINT32 timeout = DEFAULT_ABORT_TIMEOUT;

    switch(reqID)
    {
        case ND_REQ_ID_QUERYNETWORKSELECTIONMODE:   // Test COPS?
        case ND_REQ_ID_QUERYAVAILABLENETWORKS:      // Test COPS=?
            timeout = 22000;
            break;

        case ND_REQ_ID_OPERATOR:                    // Set XCOPS=
        case ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC:// Set COPS=
        case ND_REQ_ID_SETNETWORKSELECTIONMANUAL:   // Set COPS=
            timeout = 42000;
            break;

        case ND_REQ_ID_QUERYCALLFORWARDSTATUS:      // Set CCFC=
        case ND_REQ_ID_SETCALLFORWARD:              // Set CCFC=
            timeout = 37000;
            break;

        case ND_REQ_ID_QUERYCLIP:                   // Read CLIP
            timeout = 37000;
            break;

        case ND_REQ_ID_GETCLIR:                     // Read CLIR
        case ND_REQ_ID_SETCLIR:                     // Read CLIR
            timeout = 37000;
            break;

        case ND_REQ_ID_PDPCONTEXTLIST:              // Read CGACT
            timeout = 2000;
            break;

        case ND_REQ_ID_DEACTIVATEDATACALL:          // Set CGACT
        case ND_REQ_ID_PDPCONTEXTLIST_UNSOL:        // Set CGACT
            timeout = 2000;
            break;

        default:
            timeout = DEFAULT_ABORT_TIMEOUT;
            break;
    }

    return timeout;
}

//
// Is request ID an abortable command?
//
BOOL IsReqIDAbortable(UINT32 reqID)
{
    BOOL bIsAbortable = FALSE;

    switch(reqID)
    {
        case ND_REQ_ID_QUERYNETWORKSELECTIONMODE:   // Test COPS?
        case ND_REQ_ID_QUERYAVAILABLENETWORKS:      // Test COPS=?
        case ND_REQ_ID_OPERATOR:                    // Set XCOPS=
        case ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC:// Set COPS=
        case ND_REQ_ID_SETNETWORKSELECTIONMANUAL:   // Set COPS=
        case ND_REQ_ID_QUERYCALLFORWARDSTATUS:      // Set CCFC=
        case ND_REQ_ID_SETCALLFORWARD:              // Set CCFC=
        case ND_REQ_ID_QUERYCLIP:                   // Read CLIP
        case ND_REQ_ID_GETCLIR:                     // Read CLIR
        case ND_REQ_ID_SETCLIR:                     // Read CLIR
        case ND_REQ_ID_PDPCONTEXTLIST:              // Read CGACT
        case ND_REQ_ID_DEACTIVATEDATACALL:          // Set CGACT
        case ND_REQ_ID_PDPCONTEXTLIST_UNSOL:        // Set CGACT
            bIsAbortable = TRUE;
            break;

        default:
            bIsAbortable = FALSE;
            break;
    }
    return bIsAbortable;
}

//
// Send an RIL command
//
BOOL CChannel::SendCommand(CCommand*& rpCmd)
{
    RIL_LOG_VERBOSE("CChannel::SendCommand() - Enter\r\n");

    BYTE*           pATCommand = NULL;
    BYTE*           pATCommand2 = NULL;
    CResponse*      pResponse = NULL;
    RIL_RESULT_CODE resCode = RRIL_E_UNKNOWN_ERROR;
    BOOL            bResult = FALSE;

    if (NULL == rpCmd)
    {
        RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] NULL params\r\n", m_uiRilChannel);
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
        int nNumRetries = 0;
        UINT32 nCommandTimeout = rpCmd->GetTimeout();

        //  Compare command timeout of threshold timeout
        if (0 == g_TimeoutThresholdForRetry)
        {
            //  A threshold value of 0 means no retries.
            nNumRetries = 0;
        }
        else if (nCommandTimeout <= g_TimeoutThresholdForRetry)
        {
            //  AT Command timeout is <= threshold time, retry ONCE
            nNumRetries = 1;
        }
        else
        {
            //  AT command timeout is > threshold time.  No retries.
            nNumRetries = 0;
        }

        pATCommand = (BYTE *) rpCmd->GetATCmd1();
        pATCommand2 = (BYTE *) rpCmd->GetATCmd2();

        if (NULL == pATCommand2)
        {
            RIL_LOG_INFO("CChannel::SendCommand() - chnl=[%d] RILReqID=[%d] retries=[%d] Timeout=[%d] cmd1=[%s]\r\n",
                m_uiRilChannel, rpCmd->GetRequestID(), nNumRetries, rpCmd->GetTimeout(),
                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString() );
        }
        else
        {
            RIL_LOG_INFO("CChannel::SendCommand() - chnl=[%d] RILReqID=[%d] retries=[%d] Timeout=[%d] cmd1=[%s] cmd2=[%s]\r\n",
                m_uiRilChannel, rpCmd->GetRequestID(), nNumRetries, rpCmd->GetTimeout(),
                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString(),
                CRLFExpandedString(pATCommand2, strlen(pATCommand2)).GetString() );
        }

        do
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
#if defined(RESET_MGMT)
                // write() = -1, error.
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: write() = -1, chnl=[%d] Error writing command: %s\r\n",
                                m_uiRilChannel,
                                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
                //  wait forever in here.
                do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
#else // RESET_MGMT
                // ignore the error and soldier on, something may have been written
                // to the modem so wait for a potential response
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] Error writing command: %s\r\n",
                                m_uiRilChannel,
                                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
#endif // RESET_MGMT
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
                RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] No response received to TX [%s]\r\n",
                                m_uiRilChannel,
                                CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
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
                if (nNumRetries > 0)
                {
                    RIL_LOG_INFO("CChannel::SendCommand() - ***** chnl=[%d] Attempting retry  nNumRetries remaining=[%d] *****\r\n", m_uiRilChannel, nNumRetries);
                }

                //  If this was last attempt and we timed out, and this was an init command,
                //  then reset modem.  Signal clean up to STMD.
                if ( (0 == nNumRetries) && (rpCmd->IsInitCommand()) )
                {
                    RIL_LOG_CRITICAL("CChannel::SendCommand() - ***** chnl=[%d] Init command timed-out. Reset modem! *****\r\n", m_uiRilChannel);

                    do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
                }
            }
        } while (--nNumRetries >= 0);

        // call Silo hook - PostSendCommandHook
        if (!PostSendCommandHook(rpCmd, pResponse))
        {
            RIL_LOG_CRITICAL("CChannel::SendCommand() - ERROR: chnl=[%d] SendRILCmdParseResponsePostSend returned FALSE\r\n", m_uiRilChannel);
            goto Error;
        }

    }


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

    RIL_LOG_VERBOSE("CChannel::SendCommand() - Exit\r\n");
    return bResult;
}

//
// Get modem response from queue and send 2nd AT command if present
//
RIL_RESULT_CODE CChannel::GetResponse(CCommand*& rpCmd, CResponse*& rpResponse)
{
    RIL_RESULT_CODE resCode = RIL_E_GENERIC_FAILURE;
    BYTE*           pATCommand = NULL;

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
#if defined(RESET_MGMT)
            do_request_clean_up(eRadioError_ChannelDead, __LINE__, __FILE__);
#else
            TriggerRadioErrorAsync(eRadioError_ChannelDead, __LINE__, __FILE__);
#endif
            goto Error;
        }
    }
#endif // 0

    pATCommand = (BYTE *) rpCmd->GetATCmd1();

    if (rpResponse && rpResponse->IsTimedOutFlag())
    {
        if (NULL == pATCommand)
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ***** ERROR: Command timed out chnl=[%d]! timeout=[%d]ms No response to TX [(null)] *****\r\n",
                            m_uiRilChannel, rpCmd->GetTimeout());
        }
        else
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ***** ERROR: Command timed out chnl=[%d]! timeout=[%d]ms No response to TX [%s] *****\r\n",
                            m_uiRilChannel, rpCmd->GetTimeout(), CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
        }

        HandleTimeout(rpCmd, rpResponse);
        goto Error;
    }
    else if (RIL_E_SUCCESS != resCode)
    {
        RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: Failed read from queue\r\n");
        goto Error;
    }
    else
    {
        // We got a valid response.
    }


    //  Send 2nd phase of command
    //  Only send if response to first command was OK (RIL_E_SUCCESS) OR
    //  SEEK for Android request returned CME ERROR (need to get error code for custom SEEK response code)
    if ((NULL != rpCmd->GetATCmd2()) &&
        (NULL != rpResponse) &&
        //  Success and non-SEEK request OR
        ( (RIL_E_SUCCESS == rpResponse->GetResultCode() &&
            (ND_REQ_ID_SIMOPENCHANNEL != rpCmd->GetRequestID() &&
             ND_REQ_ID_SIMCLOSECHANNEL != rpCmd->GetRequestID() &&
             ND_REQ_ID_SIMTRANSMITCHANNEL != rpCmd->GetRequestID()) ) ||
        //  Error and SEEK request
          (RIL_E_SUCCESS != rpResponse->GetResultCode() &&
            (ND_REQ_ID_SIMOPENCHANNEL == rpCmd->GetRequestID() ||
             ND_REQ_ID_SIMCLOSECHANNEL == rpCmd->GetRequestID() ||
             ND_REQ_ID_SIMTRANSMITCHANNEL == rpCmd->GetRequestID()) ) ) )
    {
        pATCommand = (BYTE *) rpCmd->GetATCmd2();
        UINT32 uiBytesWritten = 0;

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
            if (NULL == pATCommand)
            {
                RIL_LOG_CRITICAL("CChannel::GetResponse() - ***** ERROR: Command2 timed out chnl=[%d] ! timeout=[%d]ms No response to TX [(null)] *****\r\n",
                                m_uiRilChannel, rpCmd->GetTimeout());
            }
            else
            {
                RIL_LOG_CRITICAL("CChannel::GetResponse() - ***** ERROR: Command2 timed out chnl=[%d] ! timeout=[%d]ms No response to TX [%s] *****\r\n",
                                m_uiRilChannel, rpCmd->GetTimeout(), CRLFExpandedString(pATCommand, strlen(pATCommand)).GetString());
            }

            HandleTimeout(rpCmd, rpResponse);
            goto Error;
        }
        else if (RIL_E_SUCCESS != resCode)
        {
            RIL_LOG_CRITICAL("CChannel::GetResponse() - ERROR: chnl=[%d] Failed read from queue for command's second response\r\n", m_uiRilChannel);
            goto Error;
        }
        else
        {
            // We got valid response.
        }
    }

Error:
    return resCode;
}

//
//  This function handles the timeout mechanism.
//  If timeout, send ABORT.  Then send PING.
BOOL CChannel::HandleTimeout(CCommand*& rpCmd, CResponse*& rpResponse)
{
    RIL_LOG_VERBOSE("CChannel::HandleTimeout() - Enter\r\n");

    // New retry mechanism.
    //
    //-If there is AT command timeout, send ABORT command.
    //-Timeout of ABORT command dependent on RIL_REQUEST.
    //    -Abortable commands are: (COPS, CCFC, CLIP, CLIR, COLP, CGACT, CGATT)
    //    -Other non-abortable commands use 500ms.
    //-TODO: If ABORT timeout, close DLC.  Open DLC.
    //-Upon response from ABORT or ABORT timeout, send AT+CMEE=1\r to "ping" modem to see if still alive.
    //-If modem "ping" passes, RIL assumes modem is alive.
    //    -If timeout <= "threshold timeout for retry", DO NOT reset modem and resend command.
    //        -If retry attempt and init command, signal clean-up request to STMD.
    //    -If timeout > "threshold timeout for retry", then DO NOT reset modem and return
    //     RIL_E_GENERIC_FAILURE to Android.
    //-If modem "ping" fails, RIL assumes modem is dead.  Signal clean-up request to STMD.


    const UINT32 PING_TIMEOUT = 3000;  // PING timeout in ms.
    BYTE szABORTCmd[] = "AT\x1b\r";  //  AT<esc>\r
    BYTE szPINGCmd[] = "AT+CMEE=1\r";


    // Send ABORT command
    UINT32 uiBytesWritten = 0;
    UINT32 uiAbortTimeout = GetAbortTimeout(rpCmd->GetRequestID());
    RIL_LOG_INFO("CChannel::HandleTimeout() - Sending ABORT Command on chnl=[%d], timeout=[%d]ms\r\n", m_uiRilChannel, uiAbortTimeout);
    WriteToPort(szABORTCmd, strlen(szABORTCmd), uiBytesWritten);

    CResponse *pRspTemp = NULL;
    RIL_RESULT_CODE resTmp = ReadQueue(pRspTemp, uiAbortTimeout); //  wait for ABORTED response

    //  Did the ABORT command timeout?
    if (pRspTemp && pRspTemp->IsTimedOutFlag())
    {
        //  ABORTED timeout

        //  If command is non-abortable, this is OK.
        if (IsReqIDAbortable(rpCmd->GetRequestID()))
        {
            //  TODO: Close DLC and Open DLC here
            RIL_LOG_CRITICAL("CChannel::HandleTimeout() - ERROR: chnl=[%d] ABORT command timed out!!\r\n", m_uiRilChannel);
        }
        else
        {
            //  No response, but this is OK since command is non-abortable
            RIL_LOG_INFO("CChannel::HandleTimeout() - chnl=[%d] ABORT timed out, but non-abortable command.  This is OK.\r\n", m_uiRilChannel);
        }
    }
    else if (RIL_E_SUCCESS != resTmp)
    {
        RIL_LOG_CRITICAL("CChannel::HandleTimeout() - ERROR: chnl=[%d] Failed read from queue during ABORTED 1\r\n", m_uiRilChannel);
        return FALSE;
    }
    else
    {
        //  Received response to ABORTED
        RIL_LOG_INFO("CChannel::HandleTimeout() - chnl=[%d] Recevied response to ABORT command!!\r\n", m_uiRilChannel);
    }
    delete pRspTemp;
    pRspTemp = NULL;

    //  "ping" modem to see if still alive
    SetCmdThreadBlockedOnRxQueue();  //  Tell response thread that reponse is pending
    uiBytesWritten = 0;
    RIL_LOG_INFO("CChannel::HandleTimeout() - Sending PING Command on chnl=[%d], timeout=[%d]ms\r\n", m_uiRilChannel, PING_TIMEOUT);
    WriteToPort(szPINGCmd, strlen(szPINGCmd), uiBytesWritten);

    resTmp = ReadQueue(pRspTemp, PING_TIMEOUT);  // Wait for PING response

    //  Did the PING command timeout?
    if (pRspTemp && pRspTemp->IsTimedOutFlag())
    {
        //  PING timeout
        //  Assume modem is dead!  Signal STMD to request cleanup.
        RIL_LOG_CRITICAL("CChannel::HandleTimeout() - ERROR: chnl=[%d] PING attempt timed out!!  Assume MODEM IS DEAD!\r\n", m_uiRilChannel);
        RIL_LOG_CRITICAL("CChannel::HandleTimeout() - ERROR: chnl=[%d] request clean up\r\n", m_uiRilChannel);

        do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
    }
    else if (RIL_E_SUCCESS != resTmp)
    {
        RIL_LOG_CRITICAL("CChannel::HandleTimeout() - ERROR: chnl=[%d] Failed read from queue during PING 1\r\n", m_uiRilChannel);
        return FALSE;
    }
    else
    {
        //  Received response to PING
        RIL_LOG_INFO("CChannel::HandleTimeout() - chnl=[%d] Recevied response to PING command!!\r\n", m_uiRilChannel);

        //  Modem is alive.  Let calling function handle the retry attempt.
        delete pRspTemp;
        pRspTemp = NULL;
        return FALSE;
    }
    delete pRspTemp;
    pRspTemp = NULL;

    RIL_LOG_VERBOSE("CChannel::HandleTimeout() - Exit\r\n");
    return TRUE;
}


BOOL CChannel::ProcessNoop(CResponse*& rpResponse)
{
    // no-op command, nothing to send - processed here
    // allocate a new response
    rpResponse = new CResponse(this);
    if (NULL == rpResponse)
    {
        // signal critical error for low memory
#if defined(RESET_MGMT)
        do_request_clean_up(eRadioError_LowMemory, __LINE__, __FILE__);
#else // RESET_MGMT
        TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
#endif // RESET_MGMT
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
#if defined(RESET_MGMT)
        do_request_clean_up(eRadioError_LowMemory, __LINE__, __FILE__);
#else // RESET_MGMT
        TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
#endif // RESET_MGMT
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

    RIL_LOG_INFO("CChannel::ProcessModemData() - INFO: chnl=[%d] size=[%d] RX [%s]\r\n", m_uiRilChannel, uiRxBytesSize, CRLFExpandedString(szRxBytes,uiRxBytesSize).GetString());

    if (!m_pResponse)
    {
        m_pResponse = new CResponse(this);
        if (!m_pResponse)
        {
            // critically low on memory
#if defined(RESET_MGMT)
            do_request_clean_up(eRadioError_LowMemory, __LINE__, __FILE__);
#else // RESET_MGMT
            TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
#endif // RESET_MGMT
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
        CEvent *rgpEvents[] = {g_RxQueueEvent[m_uiRilChannel], CSystemManager::GetCancelEvent()};

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


