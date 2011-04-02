////////////////////////////////////////////////////////////////////////////
// channelbase.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements the CChannelBase class, which provides the
//    infrastructure for the various AT Command channels.
//
// Author:  Dennis Peter
// Created: 2007-07-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "util.h"
#include "thread_manager.h"
#include "rilqueue.h"
#include "rillog.h"
#include "systemmanager.h"
#include "cmdcontext.h"
#include "thread_ops.h"
#include "silo.h"
#include "rildmain.h"
#include "repository.h"
#include "channelbase.h"



#define ISEXITSET       (CSystemManager::GetInstance().IsExitRequestSignalled())
#define GETCANCELEVENT  (CSystemManager::GetCancelEvent())
#define GETINITEVENT    (CSystemManager::GetSystemInitCompleteEvent())


#undef __out_arg

extern BOOL PromptForSimAndRequeueCmd(CCommand* pCmd, UINT32 dwLockFacility);

CChannelBase::CChannelBase(UINT32 uiChannel)
  : m_uiRilChannel(uiChannel),
    m_fWaitingForRsp(FALSE),
    m_fLastCommandTimedOut(FALSE),
    m_fFinalInitOK(FALSE),
    m_pCmdThread(NULL),
    m_pReadThread(NULL),
    m_pBlockReadThreadEvent(NULL),
    m_uiNumTimeouts(0),
    m_uiMaxTimeouts(3),
    m_uiLockCommandQueue(0),
    m_uiLockCommandQueueTimeout(0),
    m_prisdModuleInit(NULL)
{
    RIL_LOG_VERBOSE("CChannelBase::CChannelBase() - Enter\r\n");

    memset(&m_SiloContainer, 0, sizeof(SILO_CONTAINER));

    RIL_LOG_VERBOSE("CChannelBase::CChannelBase() - Exit\r\n");
}

CChannelBase::~CChannelBase()
{
    RIL_LOG_VERBOSE("CChannelBase::~CChannelBase() - Enter\r\n");

    if (m_pCmdThread)
    {
        delete m_pCmdThread;
        m_pCmdThread = NULL;
    }

    if (m_pReadThread)
    {
        delete m_pReadThread;
        m_pReadThread = NULL;
    }

    if (m_pBlockReadThreadEvent)
    {
        delete m_pBlockReadThreadEvent;
        m_pBlockReadThreadEvent = NULL;
    }

    int count = m_SiloContainer.nSilos;
    for (int i = 0; i < count; ++i)
    {
        CSilo* pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];
        delete pSilo;
        pSilo = NULL;
    }

    m_SiloContainer.nSilos = 0;

    RIL_LOG_VERBOSE("CChannelBase::~CChannelBase() - Exit\r\n");
}

//
//  Init our channel.
//
BOOL CChannelBase::InitChannel()
{
    RIL_LOG_VERBOSE("CChannelBase::InitChannel() - Enter\r\n");
    BOOL bResult = FALSE;

    m_pBlockReadThreadEvent = new CEvent(NULL, TRUE, TRUE);
    if (!m_pBlockReadThreadEvent)
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] Failed to create block read thread event!\r\n", m_uiRilChannel);
        goto Done;
    }

    if (RIL_CHANNEL_MAX <= m_uiRilChannel)
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] Channel is invalid!\r\n", m_uiRilChannel);
        goto Done;
    }

    if (!FinishInit())
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] this->FinishInit() failed!\r\n", m_uiRilChannel);
        goto Done;
    }

    if (!AddSilos())
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] this->RegisterSilos() failed!\r\n", m_uiRilChannel);
        goto Done;
    }

    bResult = TRUE;
Done:
    if (!bResult)
    {
        delete m_pBlockReadThreadEvent;
        m_pBlockReadThreadEvent = NULL;
    }
    RIL_LOG_VERBOSE("CChannelBase::InitChannel() - Exit\r\n");
    return bResult;
}

//
// Add a silo to the channel.  Add CSilo to m_gpcSilos collection.
//
BOOL CChannelBase::AddSilo(CSilo *pSilo)
{
    RIL_LOG_VERBOSE("CChannelBase::AddSilo() - Enter\r\n");

    BOOL bResult = FALSE;

    if (!pSilo)
    {
        goto Done;
    }

    if (MAX_SILOS > m_SiloContainer.nSilos)
    {
        m_SiloContainer.rgpSilos[m_SiloContainer.nSilos++] = pSilo;
    }
    else
    {
        RIL_LOG_CRITICAL("CChannelBase::AddSilo() : ERROR : Unable to add silo as container is full\r\n");
        goto Done;
    }

    bResult = TRUE;

Done:
    RIL_LOG_VERBOSE("CChannelBase::AddSilo() - Exit\r\n");
    return bResult;
}


//
// Thread responsible for sending commands from the Command Queue to COM port
//
void * ChannelCommandThreadStart(void * pVoid)
{
    RIL_LOG_VERBOSE("ChannelCommandThreadStart() - Enter\r\n");

    if (NULL == pVoid)
    {
        RIL_LOG_CRITICAL("ChannelCommandThreadStart() : ERROR : lpParameter was NULL\r\n");
        return NULL;
    }

    CChannelBase* pChannel = (CChannelBase*)pVoid;

    if (pChannel)
    {
        RIL_LOG_INFO("ChannelCommandThreadStart : chnl=[%d] Entering, pChannel=0x%X\r\n", pChannel->GetRilChannel(), pChannel);
        pChannel->CommandThread();
        RIL_LOG_INFO("ChannelCommandThreadStart : chnl=[%d] THREAD IS EXITING\r\n", pChannel->GetRilChannel());
    }

    RIL_LOG_VERBOSE("ChannelCommandThreadStart() - Exit\r\n");
    return NULL;
}

//
// Thread responsible for reading responses from COM port into the Response Queue
//
void * ChannelResponseThreadStart(void * pVoid)
{
    RIL_LOG_VERBOSE("ChannelResponseThreadStart() - Enter\r\n");

    if (NULL == pVoid)
    {
        RIL_LOG_CRITICAL("ChannelResponseThreadStart() : ERROR : pVoid was NULL\r\n");
        return NULL;
    }

    CChannelBase* pChannel = (CChannelBase*)pVoid;

    if (pChannel)
    {
        RIL_LOG_INFO("ChannelResponseThreadStart : chnl=[%d] Entering, pChannel=0x%X\r\n", pChannel->GetRilChannel(), pChannel);
        pChannel->ResponseThread();
        RIL_LOG_INFO("ChannelResponseThreadStart : chnl=[%d] THREAD IS EXITING\r\n", pChannel->GetRilChannel());
    }

    RIL_LOG_VERBOSE("ChannelResponseThreadStart() - Exit\r\n");
    return NULL;
}

BOOL CChannelBase::StartChannelThreads()
{
    RIL_LOG_VERBOSE("CChannelBase::StartChannelThreads() - Enter\r\n");
    BOOL bResult = FALSE;

    //  Launch command thread.
    m_pCmdThread = new CThread(ChannelCommandThreadStart, (void*)this, THREAD_FLAGS_JOINABLE, 0);
    if (!m_pCmdThread)
    {
        RIL_LOG_CRITICAL("StartChannelThreads() - ERROR: Unable to launch command thread\r\n");
        goto Done;
    }

    //  Launch response thread.
    m_pReadThread = new CThread(ChannelResponseThreadStart, (void*)this, THREAD_FLAGS_JOINABLE, 0);
    if (!m_pReadThread)
    {
        RIL_LOG_CRITICAL("StartChannelThreads() - ERROR: Unable to launch response thread\r\n");
        goto Done;
    }

    // Switch the read thread into higher priority (to guarantee that the module's in buffer doesn't get overflown)
    if (!CThread::SetPriority(m_pReadThread, THREAD_PRIORITY_LEVEL_HIGH))
    {
        RIL_LOG_WARNING("LaunchThreads() : WARN : Unable to raise priority of read thread!!\r\n");
    }

    bResult = TRUE;

Done:
    if (!bResult)
    {
        if (m_pCmdThread)
        {
            delete m_pCmdThread;
            m_pCmdThread = NULL;
        }

        if (m_pReadThread)
        {
            delete m_pReadThread;
            m_pReadThread = NULL;
        }
    }

    RIL_LOG_VERBOSE("CChannelBase::StartChannelThreads() - Exit\r\n");
    return bResult;
}


//
//  Terminate the threads and close the handles.
//
BOOL CChannelBase::StopChannelThreads()
{
    RIL_LOG_VERBOSE("CChannelBase::StopChannelThreads() - Enter\r\n");
    BOOL bResult = TRUE;
    const UINT32 uiThreadTime = 15000;

    UnblockReadThread();

    RIL_LOG_INFO("CChannelBase::StopChannelThreads() : INFO : Wait for Command Thread!\r\n");

    // Wait for auxiliary threads to terminate
    if (THREAD_WAIT_TIMEOUT == CThread::Wait(m_pCmdThread, uiThreadTime))
    {
        RIL_LOG_CRITICAL("CChannelBase::StopChannelThreads() : ERROR : We timed out waiting on command thread!\r\n");
        bResult = FALSE;
    }
    else
    {
        RIL_LOG_INFO("CChannelBase::StopChannelThreads() : INFO : Command Thread has successfully exited!\r\n");
    }

    RIL_LOG_INFO("CChannelBase::StopChannelThreads() : INFO : Wait for Response Thread!\r\n");

    if (THREAD_WAIT_TIMEOUT == CThread::Wait(m_pReadThread, uiThreadTime))
    {
        RIL_LOG_CRITICAL("CChannelBase::StopChannelThreads() : ERROR : We timed out waiting on response thread!\r\n");
        bResult = FALSE;
    }
    else
    {
        RIL_LOG_INFO("CChannelBase::StopChannelThreads() : INFO : Response Thread has successfully exited!\r\n");
    }

    if (m_pCmdThread)
    {
        delete m_pCmdThread;
        m_pCmdThread = NULL;
    }

    if (m_pReadThread)
    {
        delete m_pReadThread;
        m_pReadThread = NULL;
    }

    RIL_LOG_INFO("CChannelBase::StopChannelThreads() : INFO : Thread pointers were deleted!\r\n");

    RIL_LOG_VERBOSE("CChannelBase::StopChannelThreads() - Exit\r\n");

    return bResult;
}



UINT32 CChannelBase::CommandThread()
{
    RIL_LOG_VERBOSE("CChannelBase::CommandThread() - Enter\r\n");
    CCommand* pCmd = NULL;
    RIL_RESULT_CODE resCode;

    //  Double-check our channel is valid.
    if (m_uiRilChannel >= RIL_CHANNEL_MAX)
    {
        RIL_LOG_CRITICAL("CChannelBase::CommandThread() - ERROR: Invalid channel value: %d\r\n", m_uiRilChannel);
        return NULL;
    }

    CThreadManager::RegisterThread();
    while (TRUE)
    {
        if (g_pTxQueue[m_uiRilChannel]->IsEmpty())
        {
            //RIL_LOG_INFO("CChannelBase::CommandThread() : TxQueue queue empty, waiting for command\r\n");
            // wait for a command, or exit event
            if (!WaitForCommand())
            {
                RIL_LOG_CRITICAL("CChannelBase::CommandThread() : WaitForCommand returns False, exiting\r\n");
                break;
            }
        }

        //RIL_LOG_INFO("CChannelBase::CommandThread() : Getting command  DEQUEUE BEGIN\r\n");
        if (!g_pTxQueue[m_uiRilChannel]->Dequeue(pCmd))
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] Get returned RIL_E_CANCELLED\r\n", m_uiRilChannel);
            goto Done;
        }
        //RIL_LOG_INFO("CChannelBase::CommandThread() : Getting command  DEQUEUE END\r\n");

        if (!CSystemManager::GetInstance().IsInitializationSuccessful())
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] Failed init, returning RIL_E_GENERIC_FAILURE\r\n", m_uiRilChannel);

            if (0 != pCmd->GetToken())
            {
                RIL_LOG_VERBOSE("CChannelBase::CommandThread() - Complete for token 0x%08x, error: %d\r\n", pCmd->GetToken(), RIL_E_GENERIC_FAILURE);
                RIL_onRequestComplete(pCmd->GetToken(), RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            continue;
        }

        if (!SendCommand(pCmd))
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] Unable to send command!\r\n", m_uiRilChannel);

            // Need to return a failed response to the upper layers and deallocate the memory
            RIL_onRequestComplete(pCmd->GetToken(), RIL_E_GENERIC_FAILURE, NULL, 0);
            delete pCmd;
            pCmd = NULL;

            continue;
        }

        if (NULL != pCmd)
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] pCmd was not NULL following SendCommand()\r\n");
            goto Done;
        }

        /***************************************************/
        /*************** DO WE NEED THIS????? **************/
        /***************************************************/
        /*
        if (!SendDataInDataMode())
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() - ERROR: chnl=[%d] Error in sending packet!\r\n", m_uiRilChannel);
            goto Error;
        }
        */
    }

Done:

    if (pCmd)
    {
        delete pCmd;
        pCmd = NULL;
    }

    RIL_LOG_VERBOSE("CChannelBase::CommandThread() - Exit\r\n");
    return NULL;
}

BOOL CChannelBase::WaitForCommand()
{
    CEvent *rpEvents[] = {g_TxQueueEvent[m_uiRilChannel], GETCANCELEVENT};

    //RIL_LOG_INFO("CChannelBase::WaitForCommand() - WAITING FOR TxQueue EVENT...\r\n");
    CEvent::Reset(g_TxQueueEvent[m_uiRilChannel]);
    UINT32 uiRet = CEvent::WaitForAnyEvent(2, rpEvents, WAIT_FOREVER);

    //RIL_LOG_INFO("CChannelBase::WaitForCommand() - Got TxQueue event!\r\n");

    return  (WAIT_EVENT_0_SIGNALED == uiRet);
}

#define INIT_CMD_STRLEN  (4 * MAX_BUFFER_SIZE)

//
// Send an intialization command string to the COM port
//
BOOL CChannelBase::SendModemConfigurationCommands(eComInitIndex eInitIndex)
{
    RIL_LOG_VERBOSE("CChannelBase::SendModemConfigurationCommands() - Enter\r\n");
    RIL_LOG_INFO("CChannelBase::SendModemConfigurationCommands : chnl=[%d] index=[%d]\r\n", m_uiRilChannel, eInitIndex);

    BYTE*        szInit;
    const UINT32 szInitLen = 1024;
    BOOL         bRetVal  = FALSE;
    CCommand*    pCmd = NULL;
    BOOL         bLastCmd;
    CRepository  repository;
    BYTE         szTemp[MAX_BUFFER_SIZE];

    szInit = new BYTE[szInitLen];
    if (!szInit)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands : Out of memory\r\n");
        goto Done;
    }
    szInit[0] = NULL;

    if (eInitIndex >= COM_MAX_INDEX)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands : Invalid index [%d]\r\n", eInitIndex);
        goto Done;
    }

    if (NULL == m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands : m_prisdModuleInit was NULL!\r\n");
        goto Done;
    }

#ifdef GPRS_CONTEXT_CACHING
    ClearGPRSContextCommandCache();
#endif // GPRS_CONTEXT_CACHING

    // Get any pre-init commands from non-volatile memory

    if (repository.Read(g_szGroupInitCmds, g_szPreInitCmds[eInitIndex], szTemp, MAX_BUFFER_SIZE))
    {
        ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, szTemp);
        ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, "|");
    }

    // Add the hard-coded Init Commands to the Init String

    if (NULL == m_prisdModuleInit[eInitIndex].szCmd)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands : m_prisdModuleInit[%d].szCmd was NULL!\r\n", eInitIndex);
        goto Done;
    }

    if (m_prisdModuleInit[eInitIndex].szCmd[0])
    {
        ConcatenateStringNullTerminate(szInit, szInitLen, m_prisdModuleInit[eInitIndex].szCmd);
    }

    // Get any post-init commands from non-volatile memory

    if (repository.Read(g_szGroupInitCmds, g_szPostInitCmds[eInitIndex], szTemp, MAX_BUFFER_SIZE))
    {
        ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, "|");
        ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, szTemp);
    }

    // TODO: REVIEW THIS
#if 0
    if ((iString == COM_UNLOCK_INDEX) && (RIL_CHANNEL_ATCMD == m_uiRilChannel))
    {
        // send these down after SIM is unlocked
        // Note that COLR is read-only for this modem.
        char szNextInitCmd[MAX_BUFFER_SIZE];
        szNextInitCmd[0] = NULL;

        // CLIP is not supported on Sierra Wireless MC8775
        (void)PrintStringNullTerminate(szNextInitCmd, MAX_BUFFER_SIZE, "|+CLIR=%u|+COLP=%u", /*g_dwLastCLIP,*/ g_dwLastCLIR, g_dwLastCOLP);
        ConcatenateStringNullTerminate(szInit,sizeof(szInit),szNextInitCmd);
    }
#endif

    RIL_LOG_INFO("CChannelBase::SendModemConfigurationCommands : String [%s]\r\n", szInit);

    // Now go through the string and break it up into individual commands separated by a '|'
    BYTE szCmd[MAX_BUFFER_SIZE];
    BYTE *pszStart, *pszEnd;
    pszStart = szInit;
    for (;;)
    {
        // Look for the end of the current command
        pszEnd = strchr(pszStart, '|');
        if (pszEnd)
        {
            // If we found a termination BYTE, terminate the command there
            *pszEnd = '\0';
            bLastCmd = FALSE;
        }
        else
        {
            bLastCmd = TRUE;
        }

        if ('\0' != pszStart[0])
        {
            // Send the command
            (void)PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT%s\r", pszStart);
            pCmd = new CCommand(m_uiRilChannel,
                                0,
                                REQ_ID_NONE,
                                szCmd);

            if (pCmd)
            {
                CContext* pContext = new CContextInitString(eInitIndex, m_uiRilChannel, bLastCmd);
                pCmd->SetContext(pContext);
                pCmd->SetTimeout(TIMEOUT_INITIALIZATION_COMMAND);
                pCmd->SetHighPriority();
            }

            if (!pCmd || !CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() - ERROR: Could not queue command.\r\n");
                goto Done;
            }
        }

        // If this was the last command, get out now
        if (!pszEnd)
        {
            break;
        }

        // Get the next command
        pszStart = pszEnd+1;
    }

#if defined (RIL_CACHE_AUDIO_MUTING)

    // We know the mute is turned off here
    extern BOOL g_fAudioMutingOn;
    g_fAudioMutingOn = FALSE;
#endif

    bRetVal = TRUE;

Done:
    if (!bRetVal)
    {
        // Couldn't send an init string -- trigger radio error
        TriggerRadioErrorAsync(eRadioError_LowMemory, __LINE__, __FILE__);
    }

    delete[] szInit;
    szInit = NULL;
    delete pCmd;
    pCmd = NULL;
    RIL_LOG_VERBOSE("CChannelBase::SendModemConfigurationCommands() - Exit\r\n");

    return bRetVal;
}




UINT32 CChannelBase::ResponseThread()
{
    RIL_LOG_VERBOSE("CChannelBase::ResponseThread() chnl=[%d] - Enter\r\n", m_uiRilChannel);
    const UINT32 uiRespDataBufSize = 1024;
    BYTE         szData[uiRespDataBufSize];
    UINT32       uiRead;
    UINT32       uiBlockRead = 0;
    UINT32       uiReadError = 0;

    CThreadManager::RegisterThread();

    while (TRUE)
    {
        // If the error count is accumulated
        // to 3, we trigger a radio error and reboot the modem.
        if (uiReadError >= 3)
        {
            RIL_LOG_CRITICAL("CChannel::ResponseThread() - ERROR: chnl=[%d] uiReadError > = 3! Trigger radio error!\r\n", m_uiRilChannel);
            Sleep(100);
            //TriggerRadioErrorAsync(eRadioError_ForceShutdown, __LINE__, __FILE__);
            TriggerRadioError(eRadioError_ForceShutdown, __LINE__, __FILE__);



            // the modem is down and we're switching off, so no need to hang around
            // listening on the COM port, bail out now
            break;
        }

        /*
        uiBlockRead = CEvent::Wait(m_pBlockReadThreadEvent, WAIT_FOREVER);
        RIL_LOG_VERBOSE("CChannel::ResponseThread() : DEBUG : chnl=[%d] Wait(m_hBlockReadThreadEvent) returns 0x%X", m_uiRilChannel, uiBlockRead);

        // See if the thread needs to terminate
        if (ISEXITSET)
        {
            RIL_LOG_CRITICAL("CChannelBase::ResponseThread() - ERROR: chnl=[%d] Cancel event was set\r\n", m_uiRilChannel);
            break;
        }
        */

        // Wait for more data
        RIL_LOG_VERBOSE("CChannel::ResponseThread() chnl=[%d] - Waiting for data\r\n", m_uiRilChannel);
        if (!WaitForAvailableData(WAIT_FOREVER))
        {
            RIL_LOG_CRITICAL("CChannel::ResponseThread() chnl=[%d] - ERROR - Waiting for data failed!\r\n", m_uiRilChannel);
            ++uiReadError;

            // if the port is not open, reboot
            if (!IsPortOpen())
            {
                RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - ERROR: Port closed, rebooting\r\n", m_uiRilChannel);
                Sleep(100);
                //TriggerRadioErrorAsync(eRadioError_ForceShutdown, __LINE__, __FILE__);
                TriggerRadioError(eRadioError_ForceShutdown, __LINE__, __FILE__);
                break;
            }
            continue;
        }
        RIL_LOG_VERBOSE("CChannel::ResponseThread() chnl=[%d] - Data received\r\n", m_uiRilChannel);

        BOOL bFirstRead = TRUE;
        do
        {
            if (!ReadFromPort(szData, uiRespDataBufSize, uiRead))
            {
                RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - ERROR: Read failed\r\n", m_uiRilChannel);
                break;
            }

            if (!uiRead)
            {
                if (bFirstRead)
                {
                    RIL_LOG_CRITICAL("CChannel::ResponseThread() chnl=[%d] - ERROR: Data available but uiRead is 0!\r\n", m_uiRilChannel);
                    ++uiReadError;
                }
                break;
            }
            else
            {
                uiReadError = 0;
            }

            if (!ProcessModemData(szData, uiRead))
            {
                RIL_LOG_CRITICAL("CChannelBase::ResponseThread() - ERROR: chnl=[%d] ProcessModemData failed?!\r\n", m_uiRilChannel);
                break;
            }

            bFirstRead = FALSE;
            // Loop until there is nothing left in the buffer to read
        } while (uiRead != 0);
    }

Done:
    RIL_LOG_INFO("CChannelBase::ResponseThread() chnl=[%d] - Exit\r\n", m_uiRilChannel);
    return 0;
}


// Causes a reboot if more than some number of commands in a row fail
void CChannelBase::HandleTimedOutError(BOOL fCmdTimedOut)
{
    RIL_LOG_VERBOSE("CChannelBase::HandleTimedOutError() - Enter\r\n");

    if (fCmdTimedOut)
    {
        m_bTimeoutWaitingForResponse = TRUE;
        ++m_uiNumTimeouts;

        if (m_uiNumTimeouts >= m_uiMaxTimeouts)
        {
            RIL_LOG_CRITICAL("CChannelBase::HandleTimedOutError() - ERROR: chnl=[%d] Modem has not responded to multiple commands, restart RIL\r\n", m_uiRilChannel);
            TriggerRadioErrorAsync(eRadioError_ChannelDead, __LINE__, __FILE__);
        }
    }
    else
    {
        m_uiNumTimeouts=0;
    }

    RIL_LOG_VERBOSE("CChannelBase::HandleTimedOutError() - Exit\r\n");
}

//
//  Set the flags to lock the command queue for uiTimeout ms.
//  returns FALSE if queue is already locked.
// TODO: CHECK IF WE NEED THIS
/*
BOOL CChannelBase::LockCommandQueue(UINT32 uiTimeout)
{
    if (m_uiLockCommandQueue)
    {
        //  Already being locked.
        return FALSE;
    }
    RIL_LOG_INFO("CChannelBase::LockCommandQueue() : chnl=[%d] Locking command queue uiTimeout=[%ld]\r\n", m_uiRilChannel, uiTimeout);
    m_uiLockCommandQueueTimeout = uiTimeout;
    m_uiLockCommandQueue = GetTickCount();

    return TRUE;
}
*/
//
//  Iterate through each silo in this channel to ParseNotification.
//
BOOL CChannelBase::ParseUnsolicitedResponse(CResponse* const pResponse, const BYTE*& rszPointer, BOOL &fGotoError)
{
    //RIL_LOG_VERBOSE("CChannelBase::ParseUnsolicitedResponse() - Enter\r\n");
    BOOL bResult = TRUE;
    int count = m_SiloContainer.nSilos;

    for (int i = 0; i < count; ++i)
    {
        CSilo *pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];

        if (pSilo)
        {
            if (pSilo->ParseUnsolicitedResponse(pResponse, rszPointer, fGotoError))
            {
                //  we're done.
                goto Done;
            }
            else if (fGotoError)
            {
                //  done, but need to goto error.
                break;
            }
        }
    }

    bResult = FALSE;
Done:
    return bResult;
}


//  Hook to call all our silo's pre-send function.
BOOL CChannelBase::PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bResult = FALSE;
    int count = m_SiloContainer.nSilos;

    for (int i = 0; i < count; ++i)
    {
        CSilo *pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];

        if (pSilo)
        {
            if (!pSilo->PreSendCommandHook(rpCmd, rpRsp))
            {
                goto Done;
            }
        }
    }

    bResult = TRUE;
Done:
    return bResult;
}

//  Hook to call all our silo's pre-send function.
BOOL CChannelBase::PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bResult = FALSE;
    int count = m_SiloContainer.nSilos;

    for (int i = 0; i < count; ++i)
    {
        CSilo *pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];

        if (pSilo)
        {
            if (!pSilo->PostSendCommandHook(rpCmd, rpRsp))
            {
                goto Done;
            }
        }
    }

    bResult = TRUE;
Done:
    return bResult;
}


BOOL CChannelBase::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bResult = FALSE;
    int count = m_SiloContainer.nSilos;

    for (int i = 0; i < count; ++i)
    {
        CSilo *pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];

        if (pSilo)
        {
            if (!pSilo->PreParseResponseHook(rpCmd, rpRsp))
            {
                goto Done;
            }
        }
    }

    bResult = TRUE;
Done:
    return bResult;

}


BOOL CChannelBase::PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bResult = TRUE;
    int count = m_SiloContainer.nSilos;

    for (int i = 0; i < count; ++i)
    {
        CSilo *pSilo = NULL;
        pSilo = m_SiloContainer.rgpSilos[i];

        if (pSilo)
        {
            bResult = pSilo->PostParseResponseHook(rpCmd, rpRsp);
            if (!bResult)
            {
                goto Done;
            }
        }
    }

Done:
    return bResult;

}

BOOL CChannelBase::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannelBase::OpenPort() - Opening COM Port=[%s]  g_bIsSocket=[%d]\r\n", (g_szCmdPort ? g_szCmdPort : "NULL") , g_bIsSocket);

    // TODO: Grab this from repository
    bRetVal = m_Port.Open(g_szCmdPort, g_bIsSocket);

    RIL_LOG_INFO("CChannelBase::OpenPort() - Opening COM Port: %s\r\n", (bRetVal ? "SUCCESS" : "FAILED!") );

    return bRetVal;
}

BOOL CChannelBase::InitPort()
{
    return m_Port.Init();
}

BOOL CChannelBase::ClosePort()
{
    return m_Port.Close();
}

BOOL CChannelBase::WriteToPort(const BYTE* pData, UINT32 uiBytesToWrite, UINT32& ruiBytesWritten)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannelBase::WriteToPort() - INFO: chnl=[%d] TX [%s]\r\n",
                       m_uiRilChannel,
                       CRLFExpandedString(pData,uiBytesToWrite).GetString());

    bRetVal = m_Port.Write(pData, uiBytesToWrite, ruiBytesWritten);

    if (!bRetVal)
    {
        RIL_LOG_CRITICAL("CChannelBase::WriteToPort() - ERROR: chnl=[%d] Failed to write command: [%s]\r\n",
                        m_uiRilChannel,
                        CRLFExpandedString(pData,uiBytesToWrite).GetString());
    }

    return bRetVal;
}

BOOL CChannelBase::ReadFromPort(BYTE * pszReadBuf, UINT32 uiReadBufSize, UINT32& ruiBytesRead)
{
    return m_Port.Read(pszReadBuf, uiReadBufSize, ruiBytesRead);
}

BOOL CChannelBase::IsPortOpen()
{
    return m_Port.IsOpen();
}

BOOL CChannelBase::WaitForAvailableData(UINT32 uiTimeout)
{
    return m_Port.WaitForAvailableData(uiTimeout);
}


