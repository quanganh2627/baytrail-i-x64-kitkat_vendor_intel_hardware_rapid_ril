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
#include "data_util.h" // For DataConfigDown
#include "te.h"
#include "te_base.h"
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
//  Helper function to print the error code for cleanup
const char* Print_eRadioError(eRadioError e)
{
    switch(e)
    {
        case eRadioError_ForceShutdown:
            return "eRadioError_ForceShutdown";
            break;
        case eRadioError_RequestCleanup:
            return "eRadioError_RequestCleanup";
            break;
        case eRadioError_LowMemory:
            return "eRadioError_LowMemory";
            break;
        case eRadioError_ChannelDead:
            return "eRadioError_ChannelDead";
            break;
        case eRadioError_InitFailure:
            return "eRadioError_InitFailure";
            break;
        case eRadioError_OpenPortFailure:
            return "eRadioError_OpenPortFailure";
            break;
        default:
            return "unknown eRadioError value";
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// This Function aggregates the actions of updating Android stack, when a reset is identified.
void ModemResetUpdate()
{

    RIL_LOG_VERBOSE("ModemResetUpdate() - Enter\r\n");

    CTE::GetTE().CleanupAllDataConnections();

    //  Tell Android no more data connection
    RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);


    //  If there was a voice call active, it is disconnected.
    //  This will cause a RIL_REQUEST_GET_CURRENT_CALLS to be sent
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);

    CTE::GetTE().SetSIMState(RRIL_SIM_STATE_NOT_AVAILABLE);

    CTE::GetTE().SetRadioState(RRIL_RADIO_STATE_UNAVAILABLE);

    //  Delay slightly so Java layer receives replies
    Sleep(10);

    RIL_LOG_VERBOSE("ModemResetUpdate() - Exit\r\n");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  RIL has detected something is wrong with the modem.
//  Alert STMD to attempt a clean-up.
void do_request_clean_up(eRadioError eError, UINT32 uiLineNum, const char* lpszFileName)
{
    RIL_LOG_INFO("[RIL STATE] REQUEST RESET (RIL -> STMD) eError=[%s], file=[%s], line num=[%d]\r\n",
            Print_eRadioError(eError), lpszFileName, uiLineNum);

    if (g_bSpoofCommands)
    {
        RIL_LOG_INFO("do_request_clean_up() - pending.\r\n");
    }
    else
    {
        g_bSpoofCommands = TRUE;

        //  Doesn't matter what the error is, we are notifying STMD that
        //  something is wrong.  Let the modem status socket watchdog get
        //  a MODEM_UP when things are OK again.

        //  Close ports
        CSystemManager::GetInstance().CloseChannelPorts();

        //  Inform Android of new state
        //  Voice calls disconnected, no more data connections
        ModemResetUpdate();


        if (eRadioError_ForceShutdown == eError)
        {
            RIL_LOG_INFO("do_request_clean_up() - SendRequestShutdown\r\n");

            //  Send "REQUEST_SHUTDOWN" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestShutdown())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - CANNOT SEND SHUTDOWN REQUEST\r\n");
                //  Socket could have been closed by STMD.
            }
        }
        else
        {
            RIL_LOG_INFO("do_request_clean_up() - SendRequestCleanup, eError=[%d]\r\n", eError);

            //  Send "REQUEST_CLEANUP" on CleanupRequest socket
            if (!CSystemManager::GetInstance().SendRequestCleanup())
            {
                RIL_LOG_CRITICAL("do_request_clean_up() - CANNOT SEND CLEANUP REQUEST\r\n");
                //  Socket could have been closed by STMD.
                //  Restart RRIL, drop down to exit.
            }

            RIL_LOG_INFO("do_request_clean_up() - Calling exit(0)\r\n");
            exit(0);
        }
    }

    RIL_LOG_VERBOSE("do_request_clean_up() - EXIT\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void* ContinueInitThreadProc(void* pVoid)
{
    RIL_LOG_VERBOSE("ContinueInitThreadProc() - Enter\r\n");

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

    RIL_LOG_VERBOSE("ContinueInitThreadProc() - EXIT\r\n");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This is the thread that monitors the "modem status" socket for notifications
//  from STMD that the modem has reset itself.
void* ModemWatchdogThreadProc(void* pVoid)
{
    RIL_LOG_VERBOSE("ModemWatchdogThreadProc() - Enter\r\n");

    int fd_ModemStatusSocket = -1;
    int nPollRet = 0;
    int data_size = 0;
    UINT32 data;
    struct pollfd fds[1] = { {0,0,0} };
    CThread* pContinueInitThread = NULL;

    const int NUM_LOOPS = 10;
    const int SLEEP_MS = 1000;  // 1 sec between retries

    //  Store the previous modem's state.  Only handle the toggle of the modem state.
    //  Initialize to MODEM_DOWN.
    UINT32 nPreviousModemState = MODEM_DOWN;

    //  Let's connect to the modem status socket
    for (int i = 0; i < NUM_LOOPS; i++)
    {
        RIL_LOG_INFO("ModemWatchdogThreadProc() - Attempting open modem status socket try=[%d] out of %d\r\n", i+1, NUM_LOOPS);
        fd_ModemStatusSocket = socket_local_client(SOCKET_NAME_MODEM_STATUS,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_STREAM);

        if (fd_ModemStatusSocket < 0)
        {
            RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - Cannot open fd_ModemStatusSocket\r\n");
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
        RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - CANNOT OPEN MODEM STATUS SOCKET\r\n");
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
                data_size = recv(fd_ModemStatusSocket, &data, sizeof(UINT32), 0);
                if (data_size <= 0)
                {
                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - recv failed data_size=[%d]\r\n", data_size);
                    //  It is possible that socket was closed by STMD.
                    //  Restart this thread by cleaning up (by calling exit)
                    //  let's exit, init will restart us
                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - CALLING EXIT\r\n");
                    exit(0);
                }
                else if (sizeof(UINT32) != data_size)
                {
                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - recv size mismatch!  data_size=[%d]\r\n", data_size);
                    //  loop again
                }
                else
                {
                    //  Compare with previous modem status.  Only handle the toggle.
                    if (data != nPreviousModemState)
                    {
                        switch(data)
                        {
                            case MODEM_UP:
                                RIL_LOG_INFO("[RIL STATE] (RIL <- STMD) MODEM_UP\r\n");

                                nPreviousModemState = data;

                                //  transition to up

                                //  Modem is alive, open ports since RIL has been waiting at this point.
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - Continue Init, open ports!\r\n");

                                //  launch system mananger continue init thread.
                                pContinueInitThread = new CThread(ContinueInitThreadProc, NULL, THREAD_FLAGS_JOINABLE, 0);
                                if (!pContinueInitThread)
                                {
                                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - Unable to continue init thread\r\n");
                                    //  let's exit, init will restart us
                                    RIL_LOG_INFO("ModemWatchdogThreadProc() - CALLING EXIT\r\n");
                                    exit(0);
                                }

                                delete pContinueInitThread;
                                pContinueInitThread = NULL;

                                break;

                            case MODEM_DOWN:
                                RIL_LOG_INFO("[RIL STATE] (RIL <- STMD) MODEM_DOWN\r\n");

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
                                RIL_LOG_INFO("ModemWatchdogThreadProc() - CALLING EXIT\r\n");

                                exit(0);

                                //  Rely on STMD to perform cleanup
                                //RIL_LOG_INFO("ModemWatchdogThreadProc() - Done closing channel ports\r\n");
                                //RIL_LOG_INFO("ModemWatchdogThreadProc() - Wait for MODEM_UP status...\r\n");

                                break;

                            case PLATFORM_SHUTDOWN:
                                RIL_LOG_INFO("[RIL STATE] (RIL <- STMD) PLATFORM_SHUTDOWN\r\n");
                                //  RIL should already be preparing for a platform shutdown through
                                //  RIL_REQUEST_RADIO_POWER.
                                //  Do nothing here.
                                break;

                            case MODEM_COLD_RESET:
                                RIL_LOG_INFO("[RIL STATE] (RIL <- STMD) MODEM_COLD_RESET\r\n");
                                //  RIL is already in modem off state because of previous modem warm reset
                                //  procedure.  It stores event in local variable (TODO: figure out where)
                                //  for later check, and send back ACK to STMD.

                                //  Set local flag to use cached PIN next time
                                PCache_SetUseCachedPIN(true);

                                //  Send MODEM_COLD_RESET_ACK on same socket
                                if (fd_ModemStatusSocket >= 0)
                                {
                                    UINT32 data;
                                    int data_size = 0;

                                    RIL_LOG_INFO("ModemWatchdogThreadProc() - Send MODEM_COLD_RESET_ACK\r\n");
                                    data = MODEM_COLD_RESET_ACK;
                                    data_size = send(fd_ModemStatusSocket, &data, sizeof(UINT32), 0);
                                    if (data_size < 0)
                                    {
                                        RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - Failed to send MODEM_COLD_RESET_ACK\r\n");
                                    }
                                    else
                                    {
                                        RIL_LOG_INFO("[RIL STATE] (RIL -> STMD) MODEM_COLD_RESET_ACK\r\n");
                                    }
                                }
                                else
                                {
                                    RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - invalid socket fd_ModemStatusSocket=[%d]\r\n", fd_ModemStatusSocket);
                                }

                                break;

                            default:
                                RIL_LOG_INFO("[RIL STATE] (RIL <- STMD) UNKNOWN [%d]\r\n", data);
                                break;
                        }
                    }
                }
            }
            else if (fds[0].revents & POLLHUP)
            {
                RIL_LOG_CRITICAL("[RIL STATE] (RIL <- STMD) POLLHUP\r\n");
                //  Reset RIL to recover to a good status
                //  let's exit, init will restart us
                RIL_LOG_INFO("ModemWatchdogThreadProc() - CALLING EXIT\r\n");
                exit(0);
            }
            else
            {
                RIL_LOG_CRITICAL("[RIL STATE] (RIL <- STMD) UNKNOWN event [0x%08x]\r\n", fds[0].revents);
                //  continue polling
            }
        }
        else
        {
            RIL_LOG_CRITICAL("ModemWatchdogThreadProc() - poll() FAILED! nPollRet=[%d]\r\n", nPollRet);
        }

    }  // end of poll loop


    RIL_LOG_VERBOSE("ModemWatchdogThreadProc() - Exit\r\n");
    return NULL;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Create modem watchdog thread to monitor modem status socket
//
BOOL CreateModemWatchdogThread()
{
    RIL_LOG_VERBOSE("CreateModemWatchdogThread() - Enter\r\n");
    BOOL bResult = FALSE;

    //  launch watchdog thread.
    CThread* pModemWatchdogThread = new CThread(ModemWatchdogThreadProc, NULL, THREAD_FLAGS_JOINABLE, 0);
    if (!pModemWatchdogThread)
    {
        RIL_LOG_CRITICAL("CreateModemWatchdogThread() - Unable to launch modem watchdog thread\r\n");
        goto Error;
    }

    bResult = TRUE;

Error:
    delete pModemWatchdogThread;
    pModemWatchdogThread = NULL;

    RIL_LOG_VERBOSE("CreateModemWatchdogThread() - Exit\r\n");
    return bResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  XXTEA encryption / decryption algorithm.
//  Code source is from http://en.wikipedia.org/wiki/XXTEA

#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

//  btea: Encrypt or decrypt int array v of length n with 4-integer array key
//
//  Parameters:
//  UINT32 array v [in/out] : UINT32 array to be encrypted or decrypted
//  int n [in] : Number of integers in array v.  If n is negative, then decrypt.
//  UINT32 array key [in] : UINT32 array of size 4 that contains key to encrypt or decrypt
//
//  Return values:
//  None
void btea(UINT32 *v, int n, UINT32 const key[4])
{
    UINT32 y, z, sum;
    UINT32 rounds, e;
    int p;

    if (n > 1)
    {
        // Coding Part
        rounds = 6 + 52/n;
        sum = 0;
        z = v[n-1];
        do
        {
            sum += DELTA;
            e = (sum >> 2) & 3;
            for (p=0; p<n-1; p++)
            {
                y = v[p+1];
                z = v[p] += MX;
            }
            y = v[0];
            z = v[n-1] += MX;
        } while (--rounds);
    }
    else if (n < -1)
    {
        // Decoding Part
        n = -n;
        rounds = 6 + 52/n;
        sum = rounds*DELTA;
        y = v[0];
        do
        {
            e = (sum >> 2) & 3;
            for (p=n-1; p>0; p--)
            {
                z = v[p-1];
                y = v[p] -= MX;
            }
            z = v[n-1];
            y = v[0] -= MX;
        } while ((sum -= DELTA) != 0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This function is a helper for the btea() function.  It converts a string to an 4-integer array
//  to be passed in as the key to the btea() function.
//
//  Parameters:
//  string szKey [in] : string to be converted to 4-integer array
//  UINT32 array pKey [out] : 4-integer array of szKey (to be passed into btea function)
//                                  Array must be allocated by caller.
//
//  Return values:
//  PIN_INVALID_UICC if error with szKey
//  PIN_NOK if any other error
//  PIN_OK if operation is successful
ePCache_Code ConvertKeyToInt4(const char *szKey, UINT32 *pKey)
{
    //  Check inputs
    if ( (NULL == szKey) || (strlen(szKey) > 32))
    {
        RIL_LOG_CRITICAL("ConvertKeyToInt4() - szKey invalid\r\n");
        return PIN_INVALID_UICC;
    }

    if (NULL == pKey)
    {
        RIL_LOG_CRITICAL("ConvertKeyToInt4() - pKey invalid\r\n");
        return PIN_NOK;
    }

    //  Algorithm:
    //  Take szKey, and prepend '0's to make 32 character string
    char szBuf[33] = {0};

    int len = (int)strlen(szKey);
    int diff = 32 - len;

    //  Front-fill buffer
    for (int i=0; i<diff; i++)
    {
        szBuf[i] = '0';
    }

    strncat(szBuf, szKey, 32-strlen(szBuf));
    szBuf[32] = '\0';  //  KW fix
    //RIL_LOG_INFO("**********%s\r\n", szBuf);

    //  Now we have szBuf in format "0000.... <UICC>" which is exactly 32 characters.
    //  Take chunks of 8 characters, use atoi on it.  Store atoi value in pKey.

    for (int i=0; i<4; i++)
    {
        char szChunk[9] = {0};
        strncpy(szChunk, &szBuf[i*8], 8);
        pKey[i] = atoi(szChunk);
    }

    //  Print key
    //RIL_LOG_INFO("key:\r\n");
    //for (int i=0; i<4; i++)
    //{
    //    RIL_LOG_INFO("    key[%d]=0x%08X\r\n", i, pKey[i]);
    //}

    return PIN_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This function is a wrapper for the btea() function.  Encrypt szInput string, with key szKey.
//  Store the encrypted string as HEX-ASCII in storage location.
//
//  Parameters:
//  string szInput [in] : string to be encrypted
//  int nInputLen [in] : number of characters to encrypt
//  string szKey [in] : key to encrypt szInput with
//
//  Return values:
//  PIN_NOK if error
//  PIN_OK if operation successful
ePCache_Code encrypt(const char *szInput, const int nInputLen, const char *szKey)
{
    //  Check inputs
    if ( (NULL == szInput) || ('\0' == szInput[0]) || (0 == nInputLen) || (NULL == szKey) )
    {
        RIL_LOG_CRITICAL("encrypt() - Inputs are invalid!\r\n");
        return PIN_NOK;
    }

    //RIL_LOG_INFO("encrypt() - szInput=[%s] nInputLen=[%d] szKey=[%s]\r\n", szInput, nInputLen, szKey);

    const int BUF_LEN = 9;
    UINT32 buf[BUF_LEN] = {0};

    //  generate random salt
    srand((UINT32) time(NULL));

    //  Front-fill the 9 int buffer with random salt (first 8 bits of int is FF so we can identify later)
    for (int i=0; i < BUF_LEN-nInputLen; i++)
    {
        int nRand = rand();
        nRand = (nRand << 16);
        nRand = (nRand + rand());
        nRand = (nRand | 0xFF000000);
        buf[i] = (UINT32)nRand;
    }

    //  Copy the ASCII values after the random salt in the buffer.
    for (int i=0; i < nInputLen; i++)
    {
        buf[i+(BUF_LEN-nInputLen)] = (UINT32)szInput[i];
    }

    //  Print what we have
    //RIL_LOG_INFO("Buffer before encryption:\r\n");
    //for (int i=0; i<BUF_LEN; i++)
    //{
    //    RIL_LOG_INFO("    buf[%d]=0x%08X\r\n", i, buf[i]);
    //}

    // Convert the UICC to format suitable for btea
    UINT32 key[4] = {0};
    if (PIN_OK != ConvertKeyToInt4(szKey, key))
    {
        RIL_LOG_CRITICAL("encrypt() - ConvertKeyToInt4() failed!\r\n");
        return PIN_NOK;
    }

    //  Actual encryption
    btea(buf, BUF_LEN, key);

    //RIL_LOG_INFO("after encrypt:\r\n");
    //for (int i = 0; i < BUF_LEN; i++)
    //{
    //    RIL_LOG_INFO("    buf[%d]=0x%08X\r\n", i, buf[i]);
    //}

    //  Now write pInput somewhere....
    char szEncryptedBuf[MAX_PROP_VALUE] = {0};
    for (int i = 0; i < BUF_LEN; i++)
    {
        char szPrint[9] = {0};
        snprintf(szPrint, 9, "%08X", buf[i]);  //  9 includes terminating NULL character
        szPrint[8] = '\0';  //  KW fix
        //RIL_LOG_INFO("szPrint[%d]=%s\r\n", i, szPrint);

        strncat(szEncryptedBuf, szPrint, (MAX_PROP_VALUE-1) - strlen(szEncryptedBuf));
        szEncryptedBuf[MAX_PROP_VALUE-1] = '\0';  //  KW fix
    }

    //RIL_LOG_INFO("szEncryptedBuf = %s\r\n", szEncryptedBuf);

    //  Store in property
    char szCachedPinProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.cachedpin" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szCachedPinProp, szRIL_cachedpin, MAX_PROP_VALUE-1);
        szCachedPinProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szCachedPinProp, MAX_PROP_VALUE, "%s%s", szRIL_cachedpin, g_szSIMID);
    }

    if (0 != property_set(szCachedPinProp, szEncryptedBuf))
    {
        property_set(szCachedPinProp, "");
        RIL_LOG_CRITICAL("encrypt() - Cannot store property %s szEncryptedBuf\r\n", szCachedPinProp);
        return PIN_NOK;
    }

    return PIN_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  This function is a wrapper for the btea() function.  Decrypt szInput string from storage location,
//  with string szKey.
//
//  Parameters:
//  string szOut [out] : the decrypted string.  It is up to caller to allocate szOut and make sure
//                       szOut is buffer size is large enough.
//  string szKey [in] : key to decrypt the encrypted string from storage location with.
//
//  Return values:
//  PIN_WRONG_INTEGRITY if decrypted text doesn't pass integrity checks
//  PIN_NOK if inputs are invalid
//  PIN_OK if operation successful
ePCache_Code decrypt(char *szOut, const char *szKey)
{
    if ( (NULL == szOut) || (NULL == szKey) )
    {
        RIL_LOG_CRITICAL("decrypt() - invalid inputs\r\n");
        return PIN_NOK;
    }

    //RIL_LOG_INFO("decrypt() - szKey=[%s]\r\n", szKey);

    // Convert the UICC to format suitable for btea
    UINT32 key[4] = {0};
    if (PIN_OK != ConvertKeyToInt4(szKey, key))
    {
        RIL_LOG_CRITICAL("decrypt() - ConvertKeyToInt4() error!\r\n");
        return PIN_NOK;
    }

    const int BUF_LEN = 9;
    UINT32 buf[BUF_LEN] = {0};

    //  Get encrypted string from property...
    char szEncryptedBuf[MAX_PROP_VALUE] = {0};

    char szCachedPinProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.cachedpin" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szCachedPinProp, szRIL_cachedpin, MAX_PROP_VALUE-1);
        szCachedPinProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szCachedPinProp, MAX_PROP_VALUE, "%s%s", szRIL_cachedpin, g_szSIMID);
    }
    if (!property_get(szCachedPinProp, szEncryptedBuf, ""))
    {
        RIL_LOG_CRITICAL("decrypt() - cannot retrieve cached uicc\r\n");
        return PIN_NO_PIN_AVAILABLE;
    }

    //RIL_LOG_INFO("decrypt() - szEncryptedBuf = %s\r\n", szEncryptedBuf);
    for (int i = 0; i < BUF_LEN; i++)
    {
        char szPrint[9] = {0};
        strncpy(szPrint, &szEncryptedBuf[i*8], 8);
        sscanf(szPrint, "%08X", &(buf[i]));
    }

    //RIL_LOG_INFO("Buffer before decryption:\r\n");
    //for (int i = 0; i < BUF_LEN; i++)
    //{
    //    RIL_LOG_INFO("    buf[%d]=0x%08X\r\n", i, buf[i]);
    //}

    //  Actual decryption
    btea(buf, (-1 * BUF_LEN), key);

    //  Print decrypted buffer
    //RIL_LOG_INFO("Buffer after decryption:\r\n");
    //for (int i=0; i<BUF_LEN; i++)
    //{
    //    RIL_LOG_INFO("    buf[%d]=0x%08X\r\n", i, buf[i]);
    //}

    char *pChar = &(szOut[0]);

    // We have our decrypted buffer.  Figure out if it was successful and
    //  throw away the random salt.
    for (int i=0; i<BUF_LEN; i++)
    {
        if (0xFF000000 == (buf[i] & 0xFF000000))
        {
            // it was random salt, discard
            continue;
        }
        else if (buf[i] >= '0' && buf[i] <= '9')
        {
            //  valid ASCII numeric character
            *pChar = (char)buf[i];
            pChar++;
        }
        else
        {
            //  bad decoding
            RIL_LOG_CRITICAL("decrypt() - integrity failed! i=%d\r\n", i);
            return PIN_WRONG_INTEGRITY;
        }
    }

    return PIN_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Ciphers {PIN code, UICC Id} pair and store ciphered object in a local location.
// Input: UICC Id, PIN code
// Output: {OK},{NOK}
//
ePCache_Code PCache_Store_PIN(const char *szUICC, const char *szPIN)
{
    //  TODO: Remove this log statement when complete
    RIL_LOG_INFO("PCache_Store_PIN() Enter - szUICC=[%s], szPIN=[%s]\r\n", szUICC, szPIN);

    char szCachedUiccProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.cacheduicc" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szCachedUiccProp, szRIL_cacheduicc, MAX_PROP_VALUE-1);
        szCachedUiccProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szCachedUiccProp, MAX_PROP_VALUE, "%s%s", szRIL_cacheduicc, g_szSIMID);
    }

    if (NULL == szUICC || '\0' == szUICC[0] || 0 != property_set(szCachedUiccProp, szUICC))
    {
        RIL_LOG_CRITICAL("PCache_Store_PIN() - Cannot store uicc\r\n");
        return PIN_NOK;
    }

    if (NULL == szPIN || '\0' == szPIN[0])
    {
        property_set(szCachedUiccProp, "");
        RIL_LOG_CRITICAL("PCache_Store_PIN() - Cannot store pin\r\n");
        return PIN_NOK;
    }

    //  Encrypt PIN, store in Android system property
    return encrypt(szPIN, strlen(szPIN), szUICC);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Returns PIN code previously cached and paired with the given UICC Id.
// Input: UICC Id
// Output: {NOK, invalid UICC},{NOK, wrong integrity},{NOK, No PIN available},{OK}
//
ePCache_Code PCache_Get_PIN(const char *szUICC, char *szPIN)
{
    char szUICCCached[MAX_PROP_VALUE];
    RIL_LOG_INFO("PCache_Get_PIN - Enter\r\n");

    if (NULL == szUICC || NULL == szPIN || '\0' == szUICC[0])
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - szUICC or szPIN are invalid\r\n");
        return PIN_INVALID_UICC;
    }

    char szCachedUiccProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.cacheduicc" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szCachedUiccProp, szRIL_cacheduicc, MAX_PROP_VALUE-1);
        szCachedUiccProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szCachedUiccProp, MAX_PROP_VALUE, "%s%s", szRIL_cacheduicc, g_szSIMID);
    }

    if (!property_get(szCachedUiccProp, szUICCCached, ""))
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - cannot retrieve cached uicc prop %s\r\n", szCachedUiccProp);
        return PIN_NO_PIN_AVAILABLE;
    }

    if ('\0' == szUICCCached[0])
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - szUICCCached is empty!\r\n");
        return PIN_NO_PIN_AVAILABLE;
    }

    if (0 != strcmp(szUICCCached, szUICC))
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - bad uicc\r\n");
        return PIN_INVALID_UICC;
    }

    //  Retrieve encrypted PIN from Android property, and decrypt it.
    ePCache_Code ret = decrypt(szPIN, szUICC);

    if ('\0' == szPIN[0])
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - szPIN is empty!\r\n");
        return PIN_NO_PIN_AVAILABLE;
    }
    else if (PIN_OK != ret)
    {
        RIL_LOG_CRITICAL("PCache_Get_PIN() - decrypt() error = %d\r\n", ret);
    }
    else
    {
        RIL_LOG_INFO("PCache_Get_PIN - Retrieved PIN=[%s]\r\n", szPIN);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Clear PIN code cache.
// Input: None
// Output: {OK},{NOK}
//
ePCache_Code PCache_Clear()
{
    char szCachedPinProp[MAX_PROP_VALUE] = {0};
    char szCachedUiccProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.cachedpin", "ril.cacheduicc" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szCachedPinProp, szRIL_cachedpin, MAX_PROP_VALUE-1);
        szCachedPinProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
        strncpy(szCachedUiccProp, szRIL_cacheduicc, MAX_PROP_VALUE-1);
        szCachedUiccProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szCachedPinProp, MAX_PROP_VALUE, "%s%s", szRIL_cachedpin, g_szSIMID);
        snprintf(szCachedUiccProp, MAX_PROP_VALUE, "%s%s", szRIL_cacheduicc, g_szSIMID);
    }
    if (0 != property_set(szCachedUiccProp, ""))
    {
        RIL_LOG_CRITICAL("PCache_Clear() - Cannot clear uicc cache\r\n");
        return PIN_NOK;
    }

    if (0 != property_set(szCachedPinProp, ""))
    {
        RIL_LOG_CRITICAL("PCache_Clear() - Cannot clear pin cache\r\n");
        return PIN_NOK;
    }

    RIL_LOG_INFO("PCache_Clear() - Cached cleared!\r\n");

    return PIN_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Set or clear local flag to use cached pin or not.
// Input: boolean true if use cached pin, false if not.
// Output: {OK},{NOK}
//
ePCache_Code PCache_SetUseCachedPIN(bool bFlag)
{
    RIL_LOG_INFO("PCache_SetUseCachedPIN - Enter bFlag=[%d]\r\n", bFlag);

    char szUseCachedPinProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.usecachedpin" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szUseCachedPinProp, szRIL_usecachedpin, MAX_PROP_VALUE-1);
        szUseCachedPinProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szUseCachedPinProp, MAX_PROP_VALUE, "%s%s", szRIL_usecachedpin, g_szSIMID);
    }

    if (bFlag)
    {
        if (0 != property_set(szUseCachedPinProp, "1"))
        {
            RIL_LOG_CRITICAL("pCache_SetUseCachedPIN - cannot set %s  bFlag=[%d]\r\n", szUseCachedPinProp, bFlag);
            return PIN_NOK;
        }
    }
    else
    {
        if (0 != property_set(szUseCachedPinProp, ""))
        {
            RIL_LOG_CRITICAL("pCache_SetUseCachedPIN - cannot set %s  bFlag=[%d]\r\n", szUseCachedPinProp, bFlag);
            return PIN_NOK;
        }
    }

    return PIN_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Get local flag to use cached pin or not.
// Input:
// Output: bool - true if flag is set to use cached pin, false if not (or error).
//
bool PCache_GetUseCachedPIN()
{
    RIL_LOG_INFO("PCache_GetUseCachedPIN - Enter\r\n");
    bool bRet = false;

    char szUseCachedPinProp[MAX_PROP_VALUE] = {0};
    //  If sim id == 0 or if sim id is not provided by RILD, then continue
    //  to use "ril.usecachedpin" property name.
    if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
    {
        strncpy(szUseCachedPinProp, szRIL_usecachedpin, MAX_PROP_VALUE-1);
        szUseCachedPinProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
    }
    else
    {
        snprintf(szUseCachedPinProp, MAX_PROP_VALUE, "%s%s", szRIL_usecachedpin, g_szSIMID);
    }

    char szProp[MAX_PROP_VALUE] = {0};

    if (!property_get(szUseCachedPinProp, szProp, ""))
    {
        RIL_LOG_CRITICAL("PCache_GetUseCachedPIN - cannot get %s\r\n", szUseCachedPinProp);
        return false;
    }

    bRet = (0 == strcmp(szProp, "1"));

    RIL_LOG_INFO("PCache_GetUseCachedPIN - Exit  bRet=[%d]\r\n", bRet);

    return bRet;
}

