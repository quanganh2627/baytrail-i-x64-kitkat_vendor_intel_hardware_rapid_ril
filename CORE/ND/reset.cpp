////////////////////////////////////////////////////////////////////////////
// reset.cpp
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implementation of modem reset.
//
// Author:  Dennis Peter
// Created: 2011-08-31
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Aug 31/11  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include "types.h"
#include "rillog.h"
#include "thread_ops.h"
#include "sync_ops.h"
#include "repository.h"
#include "../util.h"
#include "rildmain.h"
#include "reset.h"
#include "channel_data.h"
#include "te_inf_6260.h" // For DataConfigDown

#include <sys/ioctl.h>
#include <cutils/properties.h>
#include <sys/system_properties.h>



#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


///////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
//




//  Use this to "spoof" responses to commands while resetting modem.
//  Need separate variable for spoofing commands, since core dump app
//  may be running.
BOOL g_bSpoofCommands = TRUE;

///////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
//

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// This Function aggregates the actions of updating Android stack, when a reset is identified.
void ModemResetUpdate()
{

    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");
    RIL_LOG_CRITICAL("ModemResetUpdate() - \r\n");
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");

    RIL_LOG_INFO("ModemResetUpdate() - telling RRIL no more data\r\n");
    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        //  We are taking down all data connections here, so we are looping over each data channel.
        //  Don't call DataConfigDown with invalid CID.
        if (pChannelData && pChannelData->GetContextID() > 0)
        {
            RIL_LOG_INFO("ModemResetUpdate() - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", i, pChannelData->GetContextID());
            DataConfigDown(pChannelData->GetContextID());
        }
    }

    //  Tell Android no more data connection
    RIL_LOG_INFO("ModemResetUpdate() - telling Android no more data\r\n");
    RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);


    //  If there was a voice call active, it is disconnected.
    //  This will cause a RIL_REQUEST_GET_CURRENT_CALLS to be sent
    RIL_LOG_INFO("ModemResetUpdate() - telling Android no more voice calls\r\n");
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);

    RIL_LOG_INFO("ModemResetUpdate() - telling Android SIM removed - needed for removing stk app\r\n");
    g_RadioState.SetSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);

    RIL_LOG_INFO("ModemResetUpdate() - telling Android radio status changed\r\n");
    g_RadioState.SetSIMState(RADIO_STATE_UNAVAILABLE);

    //  Delay slightly so Java layer receives replies
    Sleep(10);

    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");
    RIL_LOG_CRITICAL("ModemResetUpdate() - COMPLETE\r\n");
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");

}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  RIL has detected something is wrong with the modem.
//  Alert STMD to attempt a clean-up.
void do_request_clean_up(eRadioError eError, UINT32 uiLineNum, const BYTE* lpszFileName)
{
    RIL_LOG_INFO("do_request_clean_up() - ENTER eError=[%d]\r\n", eError);
    RIL_LOG_INFO("do_request_clean_up() - file=[%s], line num=[%d]\r\n", lpszFileName, uiLineNum);

    if (g_bSpoofCommands)
    {
        RIL_LOG_CRITICAL("do_request_clean_up() - pending... g_bSpoofCommands=[%d]\r\n",
            g_bSpoofCommands);
    }
    else
    {
        RIL_LOG_CRITICAL("do_request_clean_up() - set g_bSpoofCommands to TRUE\r\n");
        g_bSpoofCommands = TRUE;

        //  Doesn't matter what the error is, we are notifying STMD that
        //  something is wrong.  Let the modem status socket watchdog get
        //  a MODEM_UP when things are OK again.

        //  Close ports
        RIL_LOG_INFO("do_request_clean_up() - Closing channel ports\r\n");
        CSystemManager::GetInstance().CloseChannelPorts();

        //  Inform Android of new state
        //  Voice calls disconnected, no more data connections
        ModemResetUpdate();


        if (eRadioError_ForceShutdown == eError)
        {
            RIL_LOG_INFO("do_request_clean_up() - eError=[%d], SendRequestShutdown\r\n", eError);

            //  Send "REQUEST_SHUTDOWN" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestShutdown())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - ERROR: ***** CANNOT SEND SHUTDOWN REQUEST *****\r\n");
                //  Socket could have been closed by STMD.
            }
            RIL_LOG_INFO("do_request_clean_up() - EXIT\r\n");
            return;
        }
        else
        {
            RIL_LOG_INFO("do_request_clean_up() - eError=[%d], SendRequestCleanup\r\n", eError);

            //  Send "REQUEST_CLEANUP" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestCleanup())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - ERROR: ***** CANNOT SEND CLEANUP REQUEST *****\r\n");
                //  Socket could have been closed by STMD.
                //  Restart RRIL, drop down to exit.
            }

            RIL_LOG_INFO("do_request_clean_up() - *******************************\r\n");
            RIL_LOG_INFO("do_request_clean_up() - ******* Calling exit(0) *******\r\n");
            RIL_LOG_INFO("do_request_clean_up() - *******************************\r\n");
            exit(0);
        }
    }

    RIL_LOG_INFO("do_request_clean_up() - EXIT\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void* ContinueInitThreadProc(void* pVoid)
{
    RIL_LOG_INFO("ContinueInitThreadProc() - ENTER\r\n");

    //  turn off spoof
    g_bSpoofCommands = FALSE;

    if (!CSystemManager::GetInstance().ContinueInit())
    {
        RIL_LOG_CRITICAL("ContinueInitThreadProc() - Continue init failed, try a restart\r\n");
        do_request_clean_up(eRadioError_OpenPortFailure, __LINE__, __FILE__);
    }
    else
    {
        RIL_LOG_INFO("ContinueInitThreadProc() - Open ports OK\r\n");

    }

    RIL_LOG_INFO("ContinueInitThreadProc() - EXIT\r\n");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This is the thread that monitors the "modem status" socket for notifications
//  from STMD that the modem has reset itself.
void* ModemWatchdogThreadProc(void* pVoid)
{
    RIL_LOG_INFO("ModemWatchdogThreadProc() - Enter\r\n");

    int fd_ModemStatusSocket = -1;
    int nPollRet = 0;
    int data_size = 0;
    unsigned int data;
    struct pollfd fds[1] = { {0,0,0} };
    CThread* pContinueInitThread = NULL;

    const int NUM_LOOPS = 10;
    const int SLEEP_MS = 1000;  // 1 sec between retries

    //  Store the previous modem's state.  Only handle the toggle of the modem state.
    //  Initialize to MODEM_DOWN.
    unsigned int nPreviousModemState = MODEM_DOWN;

    //  Let's connect to the modem status socket
    for (int i = 0; i < NUM_LOOPS; i++)
    {
        RIL_LOG_INFO("ModemWatchdogThreadProc() - Attempting open modem status socket try=[%d] out of %d\r\n", i+1, NUM_LOOPS);
        fd_ModemStatusSocket = socket_local_client(SOCKET_NAME_MODEM_STATUS,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_STREAM);

        if (fd_ModemStatusSocket < 0)
        {
            RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: Cannot open fd_ModemStatusSocket\r\n");
            Sleep(SLEEP_MS);
        }
        else
        {
            RIL_LOG_INFO("ModemWatchdogThreadProc() - *** CREATED socket fd=[%d] ***\r\n", fd_ModemStatusSocket);
            break;
        }
    }

    if (fd_ModemStatusSocket < 0)
    {
        RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: ***** CANNOT OPEN MODEM STATUS SOCKET *****\r\n");
        //  STMD or the socket is not present, therefore, the modem is not accessible.
        //  In this case, put RRIL in a state where it will spoof responses.
        //  Call do_request_clean_up() here to hopefully wake up STMD
        //  Upon RIL startup, spoof will be enabled.
        do_request_clean_up(eRadioError_RequestCleanup, __LINE__, __FILE__);
        return NULL;
    }


    for (;;)
    {
        fds[0].fd = fd_ModemStatusSocket;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        //  Now start polling for modem status...
        RIL_LOG_INFO("ModemWatchdogThreadProc() - polling for modem status......\r\n");

        nPollRet = poll(fds, 1, -1);
        if (nPollRet > 0)
        {
            if (fds[0].revents & POLLIN)
            {
                data_size = recv(fd_ModemStatusSocket, &data, sizeof(unsigned int), 0);
                if (data_size <= 0)
                {
                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - recv failed data_size=[%d]\r\n", data_size);
                    //  It is possible that socket was closed by STMD.
                    //  Restart this thread by cleaning up (by calling exit)
                    //  let's exit, init will restart us
                    RIL_LOG_CRITICAL("***********************************************************************\r\n");
                    RIL_LOG_CRITICAL("************ModemWatchdogThreadProc() - CALLING EXIT ******************\r\n");
                    RIL_LOG_CRITICAL("***********************************************************************\r\n");
                    exit(0);
                }
                else if (sizeof(unsigned int) != data_size)
                {
                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - recv size mismatch!  data_size=[%d]\r\n", data_size);
                    //  loop again
                }
                else
                {
                    //  Compare with previous modem status.  Only handle the toggle.
                    if (data == nPreviousModemState)
                    {
                        RIL_LOG_INFO("ModemWatchdogThreadProc() - *** RECEIVED SAME MODEM STATUS=[%d] ***\r\n", data);
                        RIL_LOG_INFO("ModemWatchdogThreadProc() - *** DO NOTHING ***\r\n");
                    }
                    else
                    {
                        switch(data)
                        {
                            case MODEM_UP:
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - poll() received MODEM_UP\r\n");

                                nPreviousModemState = data;

                                //  transition to up

                                //  Modem is alive, open ports since RIL has been waiting at this point.
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - Continue Init, open ports!\r\n");

                                //  launch system mananger continue init thread.
                                pContinueInitThread = new CThread(ContinueInitThreadProc, NULL, THREAD_FLAGS_JOINABLE, 0);
                                if (!pContinueInitThread)
                                {
                                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: Unable to continue init thread\r\n");
                                    //  let's exit, init will restart us
                                    RIL_LOG_CRITICAL("***********************************************************************\r\n");
                                    RIL_LOG_CRITICAL("************ModemWatchdogThreadProc() - CALLING EXIT ******************\r\n");
                                    RIL_LOG_CRITICAL("***********************************************************************\r\n");
                                    exit(0);
                                }

                                delete pContinueInitThread;
                                pContinueInitThread = NULL;

                                break;

                            case MODEM_DOWN:
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - poll() received MODEM_DOWN\r\n");

                                nPreviousModemState = data;

                                //  Spoof commands from now on
                                g_bSpoofCommands = TRUE;

                                //  Close ports
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - Closing channel ports\r\n");
                                CSystemManager::GetInstance().CloseChannelPorts();

                                //  Inform Android of new state
                                //  Voice calls disconnected, no more data connections
                                ModemResetUpdate();

                                //  let's exit, init will restart us
                                RIL_LOG_CRITICAL("********************************************************************\r\n");
                                RIL_LOG_CRITICAL("************ModemWatchdogThreadProc() - CALLING EXIT ******************\r\n");
                                RIL_LOG_CRITICAL("********************************************************************\r\n");

                                exit(0);

                                //  Rely on STMD to perform cleanup
                                //RIL_LOG_INFO("ModemWatchdogThreadProc() - Done closing channel ports\r\n");
                                //RIL_LOG_INFO("ModemWatchdogThreadProc() - Wait for MODEM_UP status...\r\n");

                                break;

                            case PLATFORM_SHUTDOWN:
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - poll() received PLATFORM_SHUTDOWN\r\n");
                                //  RIL should already be preparing for a platform shutdown through
                                //  RIL_REQUEST_RADIO_POWER.
                                //  Do nothing here.
                                break;

                            default:
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - poll() UNKNOWN [%d]\r\n", data);
                                break;
                        }
                    }
                }
            }
            else if (fds[0].revents & POLLHUP)
            {
                RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: POLLHUP received!\r\n");
                //  Reset RIL to recover to a good status
                //  let's exit, init will restart us
                RIL_LOG_CRITICAL("***********************************************************************\r\n");
                RIL_LOG_CRITICAL("************ModemWatchdogThreadProc() - CALLING EXIT ******************\r\n");
                RIL_LOG_CRITICAL("***********************************************************************\r\n");
                exit(0);
            }
            else
            {
                RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: UNKNOWN event received! [0x%08x]\r\n", fds[0].revents);
                //  continue polling
            }
        }
        else
        {
            RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - ERROR: poll() FAILED! nPollRet=[%d]\r\n", nPollRet);
        }

    }  // end of poll loop


    RIL_LOG_INFO("ModemWatchdogThreadProc() - Exit\r\n");
    return NULL;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Create modem watchdog thread to monitor modem status socket
//
BOOL CreateModemWatchdogThread()
{
    RIL_LOG_INFO("CreateModemWatchdogThread() - Enter\r\n");
    BOOL bResult = FALSE;

    //  launch watchdog thread.
    CThread* pModemWatchdogThread = new CThread(ModemWatchdogThreadProc, NULL, THREAD_FLAGS_JOINABLE, 0);
    if (!pModemWatchdogThread)
    {
        RIL_LOG_CRITICAL("CreateModemWatchdogThread() - ERROR: Unable to launch modem watchdog thread\r\n");
        goto Error;
    }

    bResult = TRUE;

Error:
    delete pModemWatchdogThread;
    pModemWatchdogThread = NULL;

    RIL_LOG_INFO("CreateModemWatchdogThread() - Exit\r\n");
    return bResult;
}


