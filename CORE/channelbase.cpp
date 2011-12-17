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
#include "reset.h"
#include "repository.h"
#include "channelbase.h"


CChannelBase::CChannelBase(UINT32 uiChannel)
  : m_uiRilChannel(uiChannel),
    m_fWaitingForRsp(FALSE),
    m_fLastCommandTimedOut(FALSE),
    m_fFinalInitOK(FALSE),
    m_pCmdThread(NULL),
    m_pReadThread(NULL),
    m_pBlockReadThreadEvent(NULL),
    m_uiLockCommandQueue(0),
    m_uiLockCommandQueueTimeout(0),
    m_prisdModuleInit(NULL),
    m_bPossibleInvalidFD(FALSE),
    m_pPossibleInvalidFDMutex(NULL)
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

    if (m_pPossibleInvalidFDMutex)
    {
        delete m_pPossibleInvalidFDMutex;
        m_pPossibleInvalidFDMutex = NULL;
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

    if (RIL_CHANNEL_MAX <= m_uiRilChannel)
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] Channel is invalid!\r\n", m_uiRilChannel);
        goto Done;
    }

    m_pBlockReadThreadEvent = new CEvent(NULL, TRUE, TRUE);
    if (!m_pBlockReadThreadEvent)
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] Failed to create block read thread event!\r\n", m_uiRilChannel);
        goto Done;
    }

    m_pPossibleInvalidFDMutex = new CMutex();
    if (!m_pPossibleInvalidFDMutex)
    {
        RIL_LOG_CRITICAL("CChannelBase::InitChannel() - ERROR: chnl=[%d] Failed to create m_pPossibleInvalidFDMutex!\r\n", m_uiRilChannel);
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

        delete m_pPossibleInvalidFDMutex;
        m_pPossibleInvalidFDMutex = NULL;
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
        RIL_LOG_INFO("ChannelCommandThreadStart() : chnl=[%d] Entering, pChannel=0x%X\r\n", pChannel->GetRilChannel(), pChannel);
        pChannel->CommandThread();
        RIL_LOG_INFO("ChannelCommandThreadStart() : chnl=[%d] THREAD IS EXITING\r\n", pChannel->GetRilChannel());
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
        RIL_LOG_INFO("ChannelResponseThreadStart() : chnl=[%d] Entering, pChannel=0x%X\r\n", pChannel->GetRilChannel(), pChannel);
        pChannel->ResponseThread();
        RIL_LOG_INFO("ChannelResponseThreadStart() : chnl=[%d] THREAD IS EXITING\r\n", pChannel->GetRilChannel());
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
        RIL_LOG_CRITICAL("CChannelBase::StartChannelThreads() - ERROR: Unable to launch command thread\r\n");
        goto Done;
    }

    //  Launch response thread.
    m_pReadThread = new CThread(ChannelResponseThreadStart, (void*)this, THREAD_FLAGS_JOINABLE, 0);
    if (!m_pReadThread)
    {
        RIL_LOG_CRITICAL("CChannelBase::StartChannelThreads() - ERROR: Unable to launch response thread\r\n");
        goto Done;
    }

    // Switch the read thread into higher priority (to guarantee that the module's in buffer doesn't get overflown)
    if (!CThread::SetPriority(m_pReadThread, THREAD_PRIORITY_LEVEL_HIGH))
    {
        //RIL_LOG_WARNING("CChannelBase::StartChannelThreads() : WARN : Unable to raise priority of read thread!!\r\n");
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
            //  Dequeue() returns false when the queue is empty.
            //  In this case, log the error, but continue processing other commands.
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] Dequeue TxQueue returned FALSE\r\n", m_uiRilChannel);
            continue;
        }
        //RIL_LOG_INFO("CChannelBase::CommandThread() : Getting command  DEQUEUE END\r\n");

        if (!CSystemManager::GetInstance().IsInitializationSuccessful())
        {
            RIL_LOG_CRITICAL("CChannelBase::CommandThread() : ERROR : chnl=[%d] Failed init, returning RIL_E_GENERIC_FAILURE\r\n", m_uiRilChannel);

            if ( (NULL != pCmd) && (0 != pCmd->GetToken()) )
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
    CEvent *rpEvents[] = {g_TxQueueEvent[m_uiRilChannel], CSystemManager::GetCancelEvent()};

    //RIL_LOG_INFO("CChannelBase::WaitForCommand() - WAITING FOR TxQueue EVENT...\r\n");
    CEvent::Reset(g_TxQueueEvent[m_uiRilChannel]);
    UINT32 uiRet = CEvent::WaitForAnyEvent(2, rpEvents, WAIT_FOREVER);

    //RIL_LOG_INFO("CChannelBase::WaitForCommand() - Got TxQueue event!\r\n");

    return  (WAIT_EVENT_0_SIGNALED == uiRet);
}

#define INIT_CMD_STRLEN  (MAX_BUFFER_SIZE)

//
// Send an intialization command string to the COM port
//
BOOL CChannelBase::SendModemConfigurationCommands(eComInitIndex eInitIndex)
{
    RIL_LOG_VERBOSE("CChannelBase::SendModemConfigurationCommands() - Enter\r\n");
    RIL_LOG_INFO("CChannelBase::SendModemConfigurationCommands() : chnl=[%d] index=[%d]\r\n", m_uiRilChannel, eInitIndex);

    char*        szInit;
    const UINT32 szInitLen = MAX_BUFFER_SIZE;
    BOOL         bRetVal  = FALSE;
    CCommand*    pCmd = NULL;
    BOOL         bLastCmd;
    CRepository  repository;
    char         szTemp[MAX_BUFFER_SIZE];

    szInit = new char[szInitLen];
    if (!szInit)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Out of memory\r\n");
        goto Done;
    }
    szInit[0] = NULL;

    if (eInitIndex >= COM_MAX_INDEX)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Invalid index [%d]\r\n", eInitIndex);
        goto Done;
    }

    if (NULL == m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : m_prisdModuleInit was NULL!\r\n");
        goto Done;
    }


    // Get any pre-init commands from non-volatile memory

    if (repository.Read(g_szGroupInitCmds, g_szPreInitCmds[eInitIndex], szTemp, MAX_BUFFER_SIZE))
    {
        if (!ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, szTemp))
        {
            RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Concat szTemp failed\r\n");
            goto Done;
        }
        if (!ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, "|"))
        {
            RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Concat szTemp | failed\r\n");
            goto Done;
        }
    }

    // Add the hard-coded Init Commands to the Init String

    if (NULL == m_prisdModuleInit[eInitIndex].szCmd)
    {
        RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : m_prisdModuleInit[%d].szCmd was NULL!\r\n", eInitIndex);
        goto Done;
    }

    if (m_prisdModuleInit[eInitIndex].szCmd[0])
    {
        if (!ConcatenateStringNullTerminate(szInit, szInitLen, m_prisdModuleInit[eInitIndex].szCmd))
        {
            RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Concat szCmd failed  eInitIndex=[%d]r\n", eInitIndex);
            goto Done;
        }
    }

    // Get any post-init commands from non-volatile memory

    if (repository.Read(g_szGroupInitCmds, g_szPostInitCmds[eInitIndex], szTemp, MAX_BUFFER_SIZE))
    {
        if (!ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, "|"))
        {
            RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Concat | failed  eInitIndex=[%d]\r\n", eInitIndex);
            goto Done;
        }
        if (!ConcatenateStringNullTerminate(szInit, INIT_CMD_STRLEN, szTemp))
        {
            RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() : Concat szTemp failed  eInitIndex=[%d]\r\n", eInitIndex);
            goto Done;
        }
    }

    RIL_LOG_INFO("CChannelBase::SendModemConfigurationCommands() : String [%s]\r\n", szInit);

    // Now go through the string and break it up into individual commands separated by a '|'
    char szCmd[MAX_BUFFER_SIZE];
    char *pszStart, *pszEnd;
    pszStart = szInit;
    for (;;)
    {
        // Look for the end of the current command
        pszEnd = strchr(pszStart, '|');
        if (pszEnd)
        {
            // If we found a termination char, terminate the command there
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
            if (!PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT%s\r", pszStart))
            {
                RIL_LOG_CRITICAL("CChannelBase::SendModemConfigurationCommands() - ERROR: Could not make command.\r\n");
                goto Done;
            }
            pCmd = new CCommand(m_uiRilChannel,
                                0,
                                REQ_ID_NONE,
                                szCmd);

            if (pCmd)
            {
                CContext* pContext = new CContextInitString(eInitIndex, m_uiRilChannel, bLastCmd);
                pCmd->SetContext(pContext);
                pCmd->SetTimeout(g_TimeoutCmdInit);
                pCmd->SetHighPriority();
                pCmd->SetInitCommand();
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

    bRetVal = TRUE;

Done:
    if (!bRetVal)
    {
        // Couldn't send an init string -- trigger radio error
        do_request_clean_up(eRadioError_LowMemory, __LINE__, __FILE__);
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
    char         szData[uiRespDataBufSize];
    UINT32       uiRead;
    UINT32       uiReadError = 0;
    const UINT32 uiMAX_READERROR = 3;

    CThreadManager::RegisterThread();

    Sleep(200);

    while (TRUE)
    {
        // If the error count is accumulated
        // to 3, we trigger a radio error and reboot the modem.
        if (uiReadError >= uiMAX_READERROR)
        {
            RIL_LOG_CRITICAL("CChannelBase::ResponseThread() - chnl=[%d] uiReadError > = %d! Trigger radio error!\r\n", m_uiRilChannel, uiMAX_READERROR);
            do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);


            // the modem is down and we're switching off, so no need to hang around
            // listening on the COM port, bail out now
            break;
        }


        // Wait for more data
        RIL_LOG_VERBOSE("CChannelBase::ResponseThread() chnl=[%d] - Waiting for data\r\n", m_uiRilChannel);
        if (!WaitForAvailableData(WAIT_FOREVER))
        {
            RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - Waiting for data failed!\r\n", m_uiRilChannel);
            CMutex::Lock(m_pPossibleInvalidFDMutex);
            BOOL bPossibleInvalidFD = m_bPossibleInvalidFD;
            CMutex::Unlock(m_pPossibleInvalidFDMutex);
            if (!bPossibleInvalidFD)
            {
                ++uiReadError;

                // if the port is not open, reboot
                if (!IsPortOpen())
                {
                    RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - Port closed, requesting cleanup\r\n", m_uiRilChannel);
                    do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
                    break;
                }
            }
            else
            {
                //  Wait small amount of time for port to be open
                Sleep(50);
            }
            continue;
        }
        RIL_LOG_VERBOSE("CChannelBase::ResponseThread() chnl=[%d] - Data received\r\n", m_uiRilChannel);

        BOOL bFirstRead = TRUE;
        do
        {
            if (!ReadFromPort(szData, uiRespDataBufSize, uiRead))
            {
                RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - Read failed\r\n", m_uiRilChannel);

                if (m_bPossibleInvalidFD)
                {
                    //  We could be closing and opening the DLC port. (For AT timeout case)
                    RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - m_bPossibleInvalidFD = TRUE\r\n");
                    Sleep(50);
                    break;
                }
                else
                {
                    // read() < 0, call do_request_clean_up()
                    do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
                    //  exit thread
                    return 0;
                }
            }

            if (!uiRead)
            {
                if (bFirstRead)
                {
                    RIL_LOG_CRITICAL("CChannelBase::ResponseThread() chnl=[%d] - Data available but uiRead is 0!\r\n", m_uiRilChannel);
                    //  ignore, watchdog thread handles this now.
                    Sleep(25);
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
BOOL CChannelBase::ParseUnsolicitedResponse(CResponse* const pResponse, const char*& rszPointer, BOOL &fGotoError)
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

BOOL CChannelBase::InitPort()
{
    return m_Port.Init();
}

BOOL CChannelBase::ClosePort()
{
    return m_Port.Close();
}

BOOL CChannelBase::WriteToPort(const char* pData, UINT32 uiBytesToWrite, UINT32& ruiBytesWritten)
{
    RIL_LOG_INFO("CChannelBase::WriteToPort() - INFO: chnl=[%d] TX [%s]\r\n",
                       m_uiRilChannel,
                       CRLFExpandedString(pData,uiBytesToWrite).GetString());

    return m_Port.Write(pData, uiBytesToWrite, ruiBytesWritten);
}

BOOL CChannelBase::ReadFromPort(char * pszReadBuf, UINT32 uiReadBufSize, UINT32& ruiBytesRead)
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


