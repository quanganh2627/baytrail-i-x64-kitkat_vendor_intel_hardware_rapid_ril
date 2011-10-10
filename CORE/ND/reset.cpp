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


#if defined(RESET_MGMT)

#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#else  // RESET_MGMT

//  Core dump
#include <include/linux/hsi/hsi_ffl_tty.h>

#endif // RESET_MGMT


///////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
//



#if defined(RESET_MGMT)

//  Use this to "spoof" responses to commands while resetting modem.
//  Need separate variable for spoofing commands, since core dump app
//  may be running.
BOOL g_bSpoofCommands = TRUE;

#else // RESET_MGMT

//  Use this to "spoof" responses to commands while resetting modem.
//  Need separate variable for spoofing commands, since core dump app
//  may be running.
BOOL g_bSpoofCommands = FALSE;

CThread* g_pWatchdogThread = NULL;

//  Global variable to see if modem is dead.
BOOL g_bIsModemDead = FALSE;

//  Global variable to see if we are in TriggerRadioError() function.
BOOL g_bIsTriggerRadioError = FALSE;
#endif // RESET_MGMT


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


#if defined(RESET_MGMT)
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

        RIL_LOG_INFO("do_request_clean_up() - eError=[%d], SendRequestCleanup\r\n", eError);

        if (eRadioError_ForceShutdown == eError)
        {
            //  Send "REQUEST_SHUTDOWN" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestShutdown())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - ERROR: ***** CANNOT SEND SHUTDOWN REQUEST *****\r\n");
                //  Socket could have been closed by STMD.
            }
            return;
        }
        else
        {
            //  Send "REQUEST_CLEANUP" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestCleanup())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - ERROR: ***** CANNOT SEND CLEANUP REQUEST *****\r\n");
                //  Socket could have been closed by STMD.
                //  Restart RRIL, drop down to exit.
            }
        }

        RIL_LOG_INFO("do_request_clean_up() - *******************************\r\n");
        RIL_LOG_INFO("do_request_clean_up() - ******* Calling exit(0) *******\r\n");
        RIL_LOG_INFO("do_request_clean_up() - *******************************\r\n");
        exit(0);

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
        RIL_LOG_INFO("ModemWatchdogThreadProc() - starting polling for modem status......\r\n");

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

#else  // RESET_MGMT

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  When watchdog thread has detected a POLLHUP signal,
//  call this function to see if the reason is core dump.
//
//  If the repository value of "DisableCoreDump" is >= 1, then
//  skip this function.
//
//  return TRUE if reason is core dump
//  return FALSE if not
BOOL CheckForCoreDumpFlag(void)
{
    BOOL bRet = FALSE;
    int fdIfx = -1;
    int nRet = -1;
    int nReason = 0;
    const BYTE pszFileNameIFX[] = "/dev/ttyIFX0";

    //  Check repository first
    {
        CRepository repository;
        int nVal = 0;

        if (!repository.Read(g_szGroupModem, g_szDisableCoreDump, nVal))
        {
            nVal = 0;
        }
        if (nVal > 0)
        {
            //  Don't support core dump
            RIL_LOG_CRITICAL("CheckForCoreDumpFlag() - CORE DUMP NOT SUPPORTED!!  nVal=[%d]\r\n", nVal);
            return FALSE;
        }

    }

    RIL_LOG_INFO("CheckForCoreDumpFlag() - Enter\r\n");

    fdIfx = open(pszFileNameIFX, O_RDONLY);
    if (fdIfx < 0)
    {
        RIL_LOG_CRITICAL("CheckForCoreDumpFlag() - Cannot open port [%s]  errno=[%d],[%s]\r\n", pszFileNameIFX, errno, strerror(errno));
        bRet = FALSE;
    }
    else
    {
        //  Port open successful
        RIL_LOG_INFO("CheckForCoreDumpFlag() - Open port=[%s]\r\n", pszFileNameIFX);
        nRet = ioctl(fdIfx, FFL_TTY_GET_HANGUP_REASON, &nReason);
        if (nRet < 0)
        {
            // ioctl error occured
            RIL_LOG_CRITICAL("CheckForCoreDumpFlag() - IOCTL FAILED on port=[%s]\r\n", pszFileNameIFX);
            bRet = FALSE;
        }
        else
        {
            //  ioctl OK
            RIL_LOG_INFO("CheckForCoreDumpFlag() - IOCTL OK, nReason=[0x%08X]\r\n", nReason);

            if (HU_COREDUMP & nReason)
            {
                RIL_LOG_INFO("CheckForCoreDumpFlag() - COREDUMP flag set\r\n");

                bRet = TRUE;
            }
        }
    }

    if (fdIfx >= 0)
    {
        RIL_LOG_INFO("CheckForCoreDumpFlag() - Closing port=[%s]\r\n", pszFileNameIFX);
        close(fdIfx);
        fdIfx = -1;
    }

    RIL_LOG_INFO("CheckForCoreDumpFlag() - Exit  bRet=[%d]\r\n", (int)bRet);
    return bRet;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static void ResetModemAndRestart(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName)
{
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");
    RIL_LOG_CRITICAL("ResetModemAndRestart() - ERROR: eRadioError=%d, uiLineNum=%d, lpszFileName=%hs\r\n", (int)eRadioErrorVal, uiLineNum, lpszFileName);
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");

    int iTemp = 0;
    int iRadioResetDelay = 2000;            // Default value
    int iRadioResetStartStmdDelay = 4000;   // Default value
    char szCoreDumpStatus[PROPERTY_VALUE_MAX] = {0};
    CRepository repository;
    if (!repository.Read(g_szGroupModem, g_szDisableModemReset, iTemp))
    {
        iTemp = 0;
    }
    if (iTemp > 0)
    {
        //  Don't support modem reset.  Just set global flag to error out future requests.
        RIL_LOG_CRITICAL("MODEM RESET NOT SUPPORTED - ERROR OUT FUTURE REQUESTS\r\n");
        g_bIsModemDead = TRUE;
        return;
    }

    //  Read timeout values from repository.
    if (!repository.Read(g_szGroupModem, g_szRadioResetDelay, iRadioResetDelay))
    {
        iRadioResetDelay = 2000;
    }
    RIL_LOG_INFO("ResetModemAndRestart() - iRadioResetDelay=[%d]\r\n", iRadioResetDelay);

    if (!repository.Read(g_szGroupModem, g_szRadioResetStartStmdDelay, iRadioResetStartStmdDelay))
    {
        iRadioResetStartStmdDelay = 4000;
    }
    RIL_LOG_INFO("ResetModemAndRestart() - iRadioResetStartStmdDelay=[%d]\r\n", iRadioResetStartStmdDelay);

    //  Get FD to SPI.
    int ret = 0;

    UINT32 dwSleep = 200;

    RIL_LOG_INFO("ResetModemAndRestart() - BEGIN SLEEP %d\r\n", dwSleep);
    Sleep(dwSleep);
    RIL_LOG_INFO("ResetModemAndRestart() - END SLEEP %d\r\n", dwSleep);

    RIL_LOG_INFO("ResetModemAndRestart() - Closing channel ports\r\n");
    CSystemManager::GetInstance().CloseChannelPorts();

    dwSleep = 200;
    RIL_LOG_INFO("ResetModemAndRestart() - BEGIN SLEEP %d  check ports are gone\r\n", dwSleep);
    Sleep(dwSleep);
    RIL_LOG_INFO("ResetModemAndRestart() - END SLEEP %d\r\n", dwSleep);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////


    //  Try using ctrl.stop property
    RIL_LOG_INFO("CALLING prop ctl.stop on stmd\r\n");
    property_set("ctl.stop","stmd");
    RIL_LOG_INFO("Done prop ctl.stop on stmd\r\n");


    RIL_LOG_INFO("ResetModemAndRestart() - BEGIN SLEEP %d  check stmd is gone\r\n", dwSleep);
    Sleep(dwSleep);
    RIL_LOG_INFO("ResetModemAndRestart() - END SLEEP %d\r\n", dwSleep);

    //  NOTE that we do not want to call ifxreset when doing a modem initiated crash.
    if (eRadioErrorVal != eRadioError_ModemInitiatedCrash)
    {
        //  Try using ctrl.start property
        RIL_LOG_INFO("CALLING prop ctl.start on ifxreset\r\n");
        property_set("ctl.start","ifxreset_svc");
        RIL_LOG_INFO("Done prop ctl.start on ifxreset\r\n");

        //  Wait for ifxreset to complete
        RIL_LOG_INFO("ResetModemAndRestart() - BEGIN SLEEP %d\r\n", iRadioResetDelay);
        Sleep(iRadioResetDelay);
        RIL_LOG_INFO("ResetModemAndRestart() - END SLEEP %d\r\n", iRadioResetDelay);

    }


    //  Try using ctrl.start property
    RIL_LOG_INFO("CALLING prop ctl.start on stmd\r\n");
    property_set("ctl.start","stmd");
    RIL_LOG_INFO("Done prop ctl.start on stmd\r\n");

    RIL_LOG_INFO("ResetModemAndRestart() - BEGIN SLEEP %d  Wait for MUX to activate\r\n", iRadioResetStartStmdDelay);
    Sleep(iRadioResetStartStmdDelay);
    RIL_LOG_INFO("ResetModemAndRestart() - END SLEEP %d\r\n", iRadioResetStartStmdDelay);


Error:
    g_bIsTriggerRadioError = FALSE;
    g_bSpoofCommands = FALSE;

    RIL_LOG_CRITICAL("********************************************************************\r\n");
    RIL_LOG_CRITICAL("************ResetModemAndRestart() - CALLING EXIT ******************\r\n");
    RIL_LOG_CRITICAL("********************************************************************\r\n");

    exit(0);

    RIL_LOG_CRITICAL("ResetModemAndRestart() - EXIT\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This function is called to handle all core dump functionality
BOOL HandleCoreDump(int& fdGsm)
{
    RIL_LOG_INFO("HandleCoreDump() - Enter\r\n");

    // lunch intent to warn user about the core dump
    char cmd[1024] = {0};
    sprintf(cmd, "am broadcast -a com.intel.action.CORE_DUMP_WARNING");
    int status = system(cmd);

    BOOL bRet = TRUE;
    int timeout_msecs = 1000; //1 sec timeout
    const int COREDUMP_RETRIES = 120;
    int nRetries = COREDUMP_RETRIES, nCount = 0; // loop for 2 min. The typical core dump data size is 10Mb
                                         // UART speed 1Mbps, we timeout after >2*(total time to receive 10Mb)
    char szCoreDumpStatus[PROPERTY_VALUE_MAX] = {0};

    property_set("CoreDumpStatus", "None");
    RIL_LOG_INFO("HandleCoreDump() - Initialized the coredump status to None\r\n");

    //  Check repository for coredump retries
    CRepository repository;
    if (!repository.Read(g_szGroupModem, g_szCoreDumpTimeout, nRetries))
    {
        nRetries = COREDUMP_RETRIES;
    }
    RIL_LOG_INFO("HandleCoreDump() - nRetries=[%d] sec\r\n", nRetries);

    //  Launch Core Dump Download Manager
    RIL_LOG_INFO("HandleCoreDump() - Launch Core dump service\r\n");
    property_set("ctl.start", "coredump_svc");
    //some delay for the coredump reader to start and running
    Sleep(1000);

    for (nCount = 0; nCount < nRetries; nCount++)
    {
        RIL_LOG_INFO("HandleCoreDump() - sleep for %d msec before checking the core dump completion\r\n", timeout_msecs);
        Sleep(timeout_msecs);
        property_get("CoreDumpStatus", szCoreDumpStatus, "");
        RIL_LOG_INFO("HandleCoreDump() - count=%d, CoreDumpStatus is: %s\r\n", nCount, szCoreDumpStatus);
        if (strcmp(szCoreDumpStatus, "InProgress") != 0)
        {
            RIL_LOG_CRITICAL("HandleCoreDump() - Complete!\r\n");
            break;
        }
    }

    property_get("CoreDumpStatus", szCoreDumpStatus, "");

    if (strcmp(szCoreDumpStatus, "None") != 0)
    {
        RIL_LOG_CRITICAL("HandleCoreDump() - ERROR Core Dump Incomplete: %s\r\n", szCoreDumpStatus);
        RIL_LOG_CRITICAL("Reset the property and Stop coredump\r\n");
        property_set("ctl.stop", "coredump_svc");
        property_set("CoreDumpStatus", "None");
    }


    RIL_LOG_INFO("HandleCoreDump() - Exit  bRet=[%d]\r\n", (int)bRet);
    return bRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// This thread waits for a device hangup event before triggering
// a radio error and performing a re-init of the RIL.
//
void* WatchdogThreadProc(void* pVoid)
{
    RIL_LOG_INFO("WatchdogThreadProc() - Enter\r\n");
    struct pollfd fds[1] = { {0,0,0} };
    int numFds = sizeof(fds)/sizeof(fds[0]);
    int timeout_msecs = -1; // infinite timeout
    int fdGsm = -1;
    int ret = -1;
    int i = 0;
    //const BYTE pszFileNameGsm[] = "/dev/gsmtty1";
    const UINT32 uiInterval = 2000;
    UINT32 uiAttempts = 0;

    BOOL bIsPOLLHUP = FALSE;

    if ( (NULL == g_szCmdPort) || (0 == strlen(g_szCmdPort)) )
    {
        RIL_LOG_CRITICAL("WatchdogThreadProc() - g_szCmdPort = 0\r\n");
        return NULL;
    }

    Sleep(100);

    while (TRUE)
    {
        // open tty device
        RIL_LOG_INFO("WatchdogThreadProc() - opening port=[%s].....\r\n", g_szCmdPort);
        fdGsm = open(g_szCmdPort, O_RDONLY);
        if (fdGsm >= 0)
        {
            RIL_LOG_INFO("WatchdogThreadProc() - open %s successful\r\n", g_szCmdPort);
            break;
        }
        else
        {
            ++uiAttempts;
            RIL_LOG_CRITICAL("WatchdogThreadProc() : ERROR : open failed, attempt %d\r\n", uiAttempts);
            RIL_LOG_CRITICAL("WatchdogThreadProc() : ERROR : errno=[%d],[%s]\r\n", errno, strerror(errno));
            Sleep(uiInterval);
        }
    }


    //  Start polling loop here (until POLLHUP)
    while (!bIsPOLLHUP)
    {
        fds[0].fd = fdGsm;

        // block until event occurs
        ret = poll(fds, numFds, timeout_msecs);
        if (ret > 0)
        {
            // An event on one of the fds has occurred
            for (i = 0; i < numFds; i++)
            {

                if (fds[i].revents & POLLHUP)
                {
                    bIsPOLLHUP = TRUE;

                    //  Spoof commands from now on
                    g_bSpoofCommands = TRUE;

                    // A hangup has occurred on device number i
                    RIL_LOG_INFO("WatchdogThreadProc() - POLLHUP hangup event on fd[%d] and Update Android about Reset\r\n", i);
                    ModemResetUpdate();

                    //  Check for core-dump flag
                    if (CheckForCoreDumpFlag())
                    {
                        if (!HandleCoreDump(fdGsm))
                        {
                            RIL_LOG_CRITICAL("WatchdogThreadProc() - HandleCoreDump FAILED!!\r\n");
                        }
                        else
                        {
                            RIL_LOG_INFO("WatchdogThreadProc() - HandleCoreDump OK!\r\n");
                        }
                    }
                    else
                    {
                        //  No core dump flag
                        RIL_LOG_INFO("WatchdogThreadProc() - No core dump flag set or not supported\r\n");
                    }


                    //  Close port before triggering error
                    if (fdGsm >= 0)
                    {
                        RIL_LOG_INFO("WatchdogThreadProc() - Close port=[%s]\r\n", g_szCmdPort);
                        close(fdGsm);
                        fdGsm = -1;
                    }

                    // now restart everything
                    //ResetModemAndRestart(eRadioError_ModemInitiatedCrash, __LINE__, __FILE__);
                    TriggerRadioError(eRadioError_ModemInitiatedCrash, __LINE__, __FILE__);
                }
                else
                {
                    //  Not POLLHUP
                    RIL_LOG_CRITICAL("WatchdogThreadProc() - NO HANGUP!! Event is = [0x%08X] on fd[%d]\r\n", fds[i].revents, i);
                }
            }
        }
        else
        {
            RIL_LOG_CRITICAL("WatchdogThreadProc() - poll() returned error!  ret=%d, errno=%s\r\n", ret, strerror(errno));
            //  TODO: Figure out optimum sleep value here on poll error.
            Sleep(250);  // 250 ms is arbitrary.
        }

    } // end !bIsPOLLHUP loop


    if (fdGsm >= 0)
    {
        RIL_LOG_INFO("WatchdogThreadProc() - Close port=[%s]\r\n", g_szCmdPort);
        close(fdGsm);
        fdGsm = -1;
    }

    RIL_LOG_INFO("WatchdogThreadProc() - Exit\r\n");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Create watchdog thread to monitor disconnection of gsmtty device
//
BOOL CreateWatchdogThread()
{
    BOOL bResult = FALSE;

    //  launch watchdog thread.
    g_pWatchdogThread = new CThread(WatchdogThreadProc, NULL, THREAD_FLAGS_JOINABLE, 0);
    if (!g_pWatchdogThread)
    {
        RIL_LOG_CRITICAL("CreateWatchdogThread() - ERROR: Unable to launch watchdog thread\r\n");
        goto Error;
    }

    bResult = TRUE;

Error:
    if (!bResult)
    {
        if (g_pWatchdogThread)
        {
            delete g_pWatchdogThread;
            g_pWatchdogThread = NULL;
        }
    }

    RIL_LOG_INFO("CreateWatchdogThread() - Exit\r\n");
    return bResult;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
void TriggerRadioError(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName)
{
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");
    RIL_LOG_CRITICAL("TriggerRadioError() - ERROR: eRadioError=%d, uiLineNum=%d, lpszFileName=%hs\r\n", (int)eRadioErrorVal, uiLineNum, lpszFileName);
    RIL_LOG_CRITICAL("**********************************************************************************************\r\n");

    char szCoreDumpStatus[PROPERTY_VALUE_MAX] = {0};

    {
        //  Enter Mutex
        CMutex::Lock(CSystemManager::GetTriggerRadioErrorMutex());

        /*
         * If TriggerRadioError was already called do nothing.
         * If g_bSpoofCommands is set, this means that a modem reset is ongoing,
         * so do nothing except if the eRadioErrorVal is set to eRadioError_ModemInitiatedCrash.
         * In this case, we need to execute the code because this means that the
         * handling of the modem reset is terminated.
         */
        if (g_bIsTriggerRadioError || (g_bSpoofCommands && eRadioErrorVal != eRadioError_ModemInitiatedCrash) )
        {
            RIL_LOG_CRITICAL("TriggerRadioError() - Already taking place, return  eRadioError=%d\r\n", eRadioErrorVal);
            CMutex::Unlock(CSystemManager::GetTriggerRadioErrorMutex());
            return;
        }

        g_bIsTriggerRadioError = TRUE;

        property_get("CoreDumpStatus", szCoreDumpStatus, "");

        RIL_LOG_CRITICAL("TriggerRadioError() - CoreDumpStatus is: %s\r\n", szCoreDumpStatus);


        if (strcmp(szCoreDumpStatus, "InProgress") == 0)
        {
            RIL_LOG_CRITICAL("TriggerRadioError() - CoreDump In Progress.., return eRadioError=%d\r\n", eRadioErrorVal);

            //  If core dump is in progress and someone calls TriggerRadioError(), we don't want
            //  to interfere with the active core dump.  Also, when the core dump is complete
            //  that thread calls TriggerRadioError() which will just return unless this flag
            //  is set to FALSE.
            g_bIsTriggerRadioError = FALSE;
            CMutex::Unlock(CSystemManager::GetTriggerRadioErrorMutex());
            return;
        }

        //  Exit Mutex
        CMutex::Unlock(CSystemManager::GetTriggerRadioErrorMutex());
    }

    // There is no coredump running, and this is the case of unresponsive at command.

    RIL_LOG_CRITICAL("TriggerRadioError() - Update Android about reset\r\n");

    ModemResetUpdate();

    //  Now reset the modem
    ResetModemAndRestart(eRadioErrorVal, uiLineNum, lpszFileName);

    RIL_LOG_CRITICAL("TriggerRadioError() - EXIT\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
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
        delete pTriggerRadioErrorThread;
        pTriggerRadioErrorThread = NULL;
        return;
    }

    delete pTriggerRadioErrorThread;
    pTriggerRadioErrorThread = NULL;

    RIL_LOG_CRITICAL("TriggerRadioErrorAsync() - EXIT\r\n");
    return;
}
#endif // RESET_MGMT

