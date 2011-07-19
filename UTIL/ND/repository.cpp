/*
 *
 *
 * Copyright (C) 2009 Intrinsyc Software International,
 * Inc.  All Rights Reserved
 *
 * Use of this code is subject to the terms of the
 * written agreement between you and Intrinsyc.
 *
 * UNLESS OTHERWISE AGREED IN WRITING, THIS WORK IS
 * DELIVERED ON AN AS IS BASIS WITHOUT WARRANTY,
 * REPRESENTATION OR CONDITION OF ANY KIND, ORAL OR
 * WRITTEN, EXPRESS OR IMPLIED, IN FACT OR IN LAW
 * INCLUDING WITHOUT LIMITATION, THE IMPLIED WARRANTIES
 * OR CONDITIONS OF MERCHANTABLE QUALITY
 * AND FITNESS FOR A PARTICULAR PURPOSE
 *
 * This work may be subject to patent protection in the
 *  United States and other jurisdictions
 *
 * Description:
 *    Implementation of Non-Volatile storage for Android
 *
 *  Author: Francesc J. Vilarino Guell
 *  Created: 2009-08-17
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <linux/stat.h>
#include <errno.h>

#include "types.h"
#include "rril.h"
#include "repository.h"
#include "rillog.h"


//////////////////////////////////////////////////////////////////////////
// Standardized Non-Volatile Memory Access Strings

const char   g_szGroupRequestTimeouts[]        = "RequestTimeouts";
const char   g_szGroupRequestNumRetries[]      = "RequestNumRetries";

char         g_szItemRequestTimeouts[MAX_REQUEST_ITEM_LENGTH];
char         g_szItemRequestNumRetries[MAX_REQUEST_ITEM_LENGTH];

//  These strings are used for Request ID Parameters in repository.txt
//  The strings must be in the same order as the Request IDs defined in request_id.h
//
const char * g_szRequestNames[] =
{
    "GetSimStatus",                         // ND_REQ_ID_GETSIMSTATUS
    "EnterSimPin",                          // ND_REQ_ID_ENTERSIMPIN
    "EnterSimPuk",                          // ND_REQ_ID_ENTERSIMPUK
    "EnterSimPin2",                         // ND_REQ_ID_ENTERSIMPIN2
    "EnterSimPuk2",                         // ND_REQ_ID_ENTERSIMPUK2
    "ChangeSimPin",                         // ND_REQ_ID_CHANGESIMPIN
    "ChangeSimPin2",                        // ND_REQ_ID_CHANGESIMPIN2
    "EnterNetworkDepersonalization",        // ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION
    "GetCurrentCalls",                      // ND_REQ_ID_GETCURRENTCALLS
    "Dial",                                 // ND_REQ_ID_DIAL
    "GetIMSI",                              // ND_REQ_ID_GETIMSI
    "Hangup",                               // ND_REQ_ID_HANGUP
    "HangupWaitingOrBackground",            // ND_REQ_ID_HANGUPWAITINGORBACKGROUND
    "HangupForegroundResumeBackground",     // ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND
    "SwitchHoldingAndActive",               // ND_REQ_ID_SWITCHHOLDINGANDACTIVE
    "Conference",                           // ND_REQ_ID_CONFERENCE
    "UDUB",                                 // ND_REQ_ID_UDUB
    "LastCallFailCause",                    // ND_REQ_ID_LASTCALLFAILCAUSE
    "SignalStrength",                       // ND_REQ_ID_SIGNALSTRENGTH
    "RegistrationState",                    // ND_REQ_ID_REGISTRATIONSTATE
    "GprsRegistrationState",                // ND_REQ_ID_GPRSREGISTRATIONSTATE
    "Operator",                             // ND_REQ_ID_OPERATOR
    "RadioPower",                           // ND_REQ_ID_RADIOPOWER
    "Dtmf",                                 // ND_REQ_ID_DTMF
    "SendSms",                              // ND_REQ_ID_SENDSMS
    "SendSmsExpectMore",                    // ND_REQ_ID_SENDSMSEXPECTMORE
    "SetupDefaultPDP",                      // ND_REQ_ID_SETUPDEFAULTPDP
    "SimIO",                                // ND_REQ_ID_SIMIO
    "SendUSSD",                             // ND_REQ_ID_SENDUSSD
    "CancelUSSD",                           // ND_REQ_ID_CANCELUSSD
    "GetCLIR",                              // ND_REQ_ID_GETCLIR
    "SetCLIR",                              // ND_REQ_ID_SETCLIR
    "QueryCallForwardStatus",               // ND_REQ_ID_QUERYCALLFORWARDSTATUS
    "SetCallForward",                       // ND_REQ_ID_SETCALLFORWARD
    "QueryCallWaiting",                     // ND_REQ_ID_QUERYCALLWAITING
    "SetCallWaiting",                       // ND_REQ_ID_SETCALLWAITING
    "SmsAcknowledge",                       // ND_REQ_ID_SMSACKNOWLEDGE
    "GetIMEI",                              // ND_REQ_ID_GETIMEI
    "GetIMEISV",                            // ND_REQ_ID_GETIMEISV
    "Answer",                               // ND_REQ_ID_ANSWER
    "DeactivateDataCall",                   // ND_REQ_ID_DEACTIVATEDATACALL
    "QueryFacilityLock",                    // ND_REQ_ID_QUERYFACILITYLOCK
    "SetFacilityLock",                      // ND_REQ_ID_SETFACILITYLOCK
    "ChangeBarringPassword",                // ND_REQ_ID_CHANGEBARRINGPASSWORD
    "QueryNetworkSelectionMode",            // ND_REQ_ID_QUERYNETWORKSELECTIONMODE
    "SetNetworkSelectionAutomatic",         // ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC
    "SetNetworkSelectionManual",            // ND_REQ_ID_SETNETWORKSELECTIONMANUAL
    "QueryAvailableNetworks",               // ND_REQ_ID_QUERYAVAILABLENETWORKS
    "RequestDtmfStart",                     // ND_REQ_ID_REQUESTDTMFSTART
    "RequestDtmfStop",                      // ND_REQ_ID_REQUESTDTMFSTOP
    "BasebandVersion",                      // ND_REQ_ID_BASEBANDVERSION
    "SeperateConnection",                   // ND_REQ_ID_SEPERATECONNECTION
    "SetMute",                              // ND_REQ_ID_SETMUTE
    "GetMute",                              // ND_REQ_ID_GETMUTE
    "QueryCLIP",                            // ND_REQ_ID_QUERYCLIP
    "LastPdpFailCause",                     // ND_REQ_ID_LASTPDPFAILCAUSE
    "PdpContextList",                       // ND_REQ_ID_PDPCONTEXTLIST
    "ResetRadio",                           // ND_REQ_ID_RESETRADIO
    "OemHookRaw",                           // ND_REQ_ID_OEMHOOKRAW
    "OemHookStrings",                       // ND_REQ_ID_OEMHOOKSTRINGS
    "ScreenState",                          // ND_REQ_ID_SCREENSTATE
    "SetSuppSvcNotification",               // ND_REQ_ID_SETSUPPSVCNOTIFICATION
    "WriteSmsToSim",                        // ND_REQ_ID_WRITESMSTOSIM
    "DeleteSmsOnSim",                       // ND_REQ_ID_DELETESMSONSIM
    "SetBandMode",                          // ND_REQ_ID_SETBANDMODE
    "QueryAvailableBandMode",               // ND_REQ_ID_QUERYAVAILABLEBANDMODE
    "StkGetProfile",                        // ND_REQ_ID_STKGETPROFILE
    "StkSetProfile",                        // ND_REQ_ID_STKSETPROFILE
    "StkSendEnvelopeCommand",               // ND_REQ_ID_STKSENDENVELOPECOMMAND
    "StkSendTerminalResponse",              // ND_REQ_ID_STKSENDTERMINALRESPONSE
    "StkHandleCallSetupRequestedFromSim",   // ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM
    "ExplicitCallTransfer",                 // ND_REQ_ID_EXPLICITCALLTRANSFER
    "SetPreferredNetworkType",              // ND_REQ_ID_SETPREFERREDNETWORKTYPE
    "GetPreferredNetworkType",              // ND_REQ_ID_GETPREFERREDNETWORKTYPE
    "GetNeighboringCellIDs",                // ND_REQ_ID_GETNEIGHBORINGCELLIDS
    "SetLocationUpdates",                   // ND_REQ_ID_SETLOCATIONUPDATES
    "CdmaSetSubscription",                  // ND_REQ_ID_CDMASETSUBSCRIPTION
    "CdmaSetRoamingPreference",             // ND_REQ_ID_CDMASETROAMINGPREFERENCE
    "CdmaQueryRoamingPreference",           // ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE
    "SetTtyMode",                           // ND_REQ_ID_SETTTYMODE
    "QueryTtyMode",                         // ND_REQ_ID_QUERYTTYMODE
    "CdmaSetPreferredVoicePrivacyMode",     // ND_REQ_ID_CDMASETPREFERREDVOICEPRIVACYMODE
    "CdmaQueryPreferredVoicePrivacyMode",   // ND_REQ_ID_CDMAQUERYPREFERREDVOICEPRIVACYMODE
    "CdmaFlash",                            // ND_REQ_ID_CDMAFLASH
    "CdmaBurstDtmf",                        // ND_REQ_ID_CDMABURSTDTMF
    "CdmaValidateKey",                      // ND_REQ_ID_CDMAVALIDATEAKEY
    "CdmaSendSms",                          // ND_REQ_ID_CDMASENDSMS
    "CdmaSmsAcknowledge",                   // ND_REQ_ID_CDMASMSACKNOWLEDGE
    "GetBroadcastSmsConfig",                // ND_REQ_ID_GETBROADCASTSMSCONFIG
    "SetBroadcastSmsConfig",                // ND_REQ_ID_SETBROADCASTSMSCONFIG
    "SmsBroadcastActivation",               // ND_REQ_ID_SMSBROADCASTACTIVATION
    "CdmaGetBroadcastSmsConfig",            // ND_REQ_ID_CDMAGETBROADCASTSMSCONFIG
    "CdmaSetBroadcastSmsConfig",            // ND_REQ_ID_CDMASETBROADCASTSMSCONFIG
    "CdmaSmsBroadcastActivation",           // ND_REQ_ID_CDMASMSBROADCASTACTIVATION
    "CdmaSubscription",                     // ND_REQ_ID_CDMASUBSCRIPTION
    "CdmaWriteSmsToRuim",                   // ND_REQ_ID_CDMAWRITESMSTORUIM
    "CdmaDeleteSmsOnRuim",                  // ND_REQ_ID_CDMADELETESMSONRUIM
    "DeviceIdentity",                       // ND_REQ_ID_DEVICEIDENTITY
    "ExitEmergencyCallBackMode",            // ND_REQ_ID_EXITEMERGENCYCALLBACKMODE
    "GetSmscAddress",                       // ND_REQ_ID_GETSMSCADDRESS
    "SetSmscAddress",                       // ND_REQ_ID_SETSMSCADDRESS
    "ReportSmsMemoryStatus",                // ND_REQ_ID_REPORTSMSMEMORYSTATUS
    "ReportStkServiceIsRunning",            // ND_REQ_ID_REPORTSTKSERVICEISRUNNING
    "GetIPAddress",                         // ND_REQ_ID_GETIPADDRESS
    "GetDNS",                               // ND_REQ_ID_GETDNS
    "PdpContextListUnsol",                  // ND_REQ_ID_PDPCONTEXTLIST_UNSOL
    "SimTransmitBasic",                     // ND_REQ_ID_SIMTRANSMITBASIC
    "SimOpenChannel",                       // ND_REQ_ID_SIMOPENCHANNEL
    "SimCloseChannel",                      // ND_REQ_ID_SIMCLOSECHANNEL
    "SimTransmitChannel"                    // ND_REQ_ID_SIMTRANSMITCHANNEL

};

/////////////////////////////////////////////////

const char   g_szGroupOtherTimeouts[]          = "OtherTimeouts";

const char   g_szTimeoutCmdInit[]              = "TimeoutCmdInit";
const char   g_szTimeoutCmdNoOp[]              = "TimeoutCmdNoOp";
const char   g_szTimeoutCmdOnline[]            = "TimeoutCmdOnline";
const char   g_szTimeoutAPIDefault[]           = "TimeoutAPIDefault";
const char   g_szTimeoutDTRDrop[]              = "TimeoutDTRDrop";
const char   g_szTimeoutWaitForInit[]          = "TimeoutWaitForInit";

/////////////////////////////////////////////////

const char   g_szGroupRILSettings[]            = "RILSettings";

const char   g_szMaxCommandTimeouts[]          = "MaxCommandTimeouts";
const char   g_szMaxInitRetries[]              = "MaxInitRetries";
const char   g_szDataModeCmdDelay[]            = "DataModeCmdDelay";
const char   g_szDataModeMaxTimeWithoutCmd[]   = "DataModeMaxTimeWithoutCmd";
const char   g_szDefaultCmdRetries[]           = "DefaultCmdRetries";
const char   g_szUseUSIMAddress[]              = "UseUSIMAddress";
const char   g_szOpenPortRetries[]             = "OpenPortRetries";
const char   g_szOpenPortInterval[]            = "OpenPortInterval";

/////////////////////////////////////////////////

const char   g_szGroupNetworkSettings[]        = "NetworkSettings";

const char   g_szConnectRspDelay[]             = "ConnectResponseDelay";

/////////////////////////////////////////////////

const char   g_szGroupInitCmds[]               = "InitCmds";

const char * g_szPreInitCmds[] =
{
    "PreInitCmds",      // COM_INIT_INDEX
    "PreReinitCmds",    // COM_REINIT_INDEX
    "PreUnlockCmds",    // COM_UNLOCK_INDEX
    "PreSmsInitCmds",   // COM_SMSINIT_INDEX
};

const char * g_szPostInitCmds[] =
{
    "PostInitCmds",     // COM_INIT_INDEX
    "PostReinitCmds",   // COM_REINIT_INDEX
    "PostUnlockCmds",   // COM_UNLOCK_INDEX
    "PostSmsInitCmds",  // COM_SMSINIT_INDEX
};

/////////////////////////////////////////////////

const char   g_szGroupSIMTK[]                  = "SIMTK";

const char   g_szTerminalProfile[]             = "TerminalProfile";

/////////////////////////////////////////////////

const char   g_szGroupOperatorNames[]          = "OperatorNames";

/////////////////////////////////////////////////

const char   g_szGroupSystemReady[]             = "SystemReadySettings";

/////////////////////////////////////////////////

const char   g_szGroupModem[]                   = "Modem";

const char   g_szSupportedModem[]               = "SupportedModem";
const char   g_szNetworkInterfaceName[]         = "NetworkInterfaceName";
const char   g_szDisableModemReset[]            = "DisableModemReset";
const char   g_szDisableCoreDump[]              = "DisableCoreDump";
const char   g_szCoreDumpTimeout[]              = "CoreDumpTimeout";
const char   g_szDisableWatchdogThread[]        = "DisableWatchdogThread";
const char   g_szRadioResetDelay[]              = "RadioResetDelay";
const char   g_szRadioResetStartStmdDelay[]     = "RadioResetStartStmdDelay";

/////////////////////////////////////////////////

const char   g_szGroupLogging[]                 = "Logging";

const char   g_szLogLevel[]                     = "LogLevel";


//////////////////////////////////////////////////////////////////////////
// Structs and Enums.

enum
{
    E_ERROR     = -1,
    E_OK        =  0,
    E_NOT_FOUND =  1,
    E_EOF       =  2
};


//////////////////////////////////////////////////////////////////////////
// Class-Specific Strings

static const char* REPO_FILE = "/system/etc/rril/repository.txt";
static const char* REPO_TMP_FILE = "/system/etc/rril/repository.tmp";
static const char* GROUP_MARKER = "Group";
static const int   GROUP_MARKER_LEN = 5;


//////////////////////////////////////////////////////////////////////////
// Variable Initialization

struct CRepository::CRepLock CRepository::m_stLock = {{0}, {0}, {0}, 0, 0, 0, 0};
bool CRepository::m_bInitialized = FALSE;


//////////////////////////////////////////////////////////////////////////
// CRepository Class Implementation

CRepository::CRepository()
{
}

CRepository::~CRepository()
{
    CloseRepositoryFile();
}

BOOL CRepository::Init()
{
    m_bInitialized = FALSE;

    CRepository::m_stLock.iReaders = 0;
    CRepository::m_stLock.iWriters = 0;
    CRepository::m_stLock.iReadWaiters = 0;
    CRepository::m_stLock.iWriteWaiters = 0;

    if (0 != pthread_mutex_init(&m_stLock.stLock, NULL))
    {
        goto Error;
    }

    if (0 != pthread_cond_init(&m_stLock.stRead, NULL))
    {
        goto Error;
    }

    if (0 != pthread_cond_init(&m_stLock.stWrite, NULL))
    {
        goto Error;
    }

    m_bInitialized = TRUE;

Error:
    return m_bInitialized;
}

BOOL CRepository::Close()
{
    if (m_bInitialized)
    {
        pthread_mutex_destroy(&m_stLock.stLock);
        pthread_cond_destroy(&m_stLock.stRead);
        pthread_cond_destroy(&m_stLock.stWrite);
        m_bInitialized = FALSE;
    }

    return TRUE;
}

BOOL CRepository::OpenRepositoryFile()
{
    BOOL fRetVal = FALSE;

    m_iLine = 1;
    m_iFd   = open(REPO_FILE, O_RDWR);

    if (m_iFd < 0)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::OpenRepositoryFile() - ERROR: Could not open file \"%s\" - %s\r\n", REPO_FILE, strerror(iErrCode));
        goto Error;
    }

    // seek to beginning of file
    ResetFile();

    fRetVal = TRUE;

Error:
    return fRetVal;
}

void CRepository::CloseRepositoryFile()
{
    if (m_iFd >= 0)
    {
        close(m_iFd);
        m_iFd = -1;
    }
}

BOOL CRepository::Read(const char *szGroup, const char* szKey, int& iRes)
{
    BOOL fRetVal = FALSE;
    char szBuf[MAX_INT_LEN];

    fRetVal = Read(szGroup, szKey, szBuf, MAX_INT_LEN);
    if (fRetVal)
    {
        char *remaining;

        iRes = strtol(szBuf, &remaining, 10);
        fRetVal = (szBuf != remaining);
    }

    return fRetVal;
}

BOOL CRepository::Read(const char *szGroup, const char* szKey, char* szRes, int iMaxLen)
{
    char  szBuf[MAX_LINE_LEN];
    char* pBuf = szBuf;
    int   iRetVal = E_ERROR;
    int   iRes;

    if (!m_bInitialized)
    {
        RIL_LOG_CRITICAL("CRepository::Read() - ERROR: Repository has not been initialized.\r\n");
        goto Error;
    }

    // grab lock before accessing the NVM file
    ReaderLock();

    if (!OpenRepositoryFile())
    {
        RIL_LOG_CRITICAL("CRepository::Read() - ERROR: Could not open the Repository file.\r\n");
        goto Error;
    }

    // look for group
    iRes = LocateGroup(szGroup);
    if (E_OK != iRes)
    {
        RIL_LOG_INFO("CRepository::Read() - Could not locate the \"%s\" group.\r\n", szGroup);
        goto Error;
    }

    // look for key in group
    iRes = LocateKey(szKey, pBuf);
    if (E_OK != iRes)
    {
        RIL_LOG_VERBOSE("CRepository::Read() - Could not locate the \"%s\" key.\r\n", szKey);
        goto Error;
    }

    // get value associated to key
    RemoveComment(pBuf);
    iRetVal = ExtractValue(pBuf, szRes, iMaxLen);

Error:
    CloseRepositoryFile();
    ReaderUnlock();

    return (E_OK == iRetVal);
}

BOOL CRepository::Write(const char *szGroup, const char* szKey, int iVal)
{
    char szTmp[MAX_INT_LEN];
    sprintf(szTmp, "%d", iVal);
    return Write(szGroup, szKey, szTmp);
}

BOOL CRepository::Write(const char *szGroup, const char* szKey, const char* szVal)
{
    int iRetVal = E_ERROR;
    int iRes;
    int iGroupLine;

    if (!m_bInitialized)
    {
        RIL_LOG_CRITICAL("CRepository::Write() - ERROR: Repository has not been initialized.\r\n");
        goto Error;
    }

    // grab lock before accessing file
    WriterLock();

    if (!OpenRepositoryFile())
    {
        RIL_LOG_CRITICAL("CRepository::Write() - ERROR: Could not open the Repository file.\r\n");
        goto Error;
    }

    // look for group
    iRes = LocateGroup(szGroup);
    switch(iRes)
    {
        case E_OK:
            iGroupLine = m_iLine;
            iRes = LocateKey(szKey);
            if (E_OK == iRes)
            {
                // the key exists, replace its value
                iRetVal = ReplaceKey(szKey, szVal);
                if (E_OK != iRetVal)
                {
                    RIL_LOG_CRITICAL("CRepository::Write() - ERROR: Could not update key value.\r\n");
                }
            }
            else if (E_NOT_FOUND == iRes)
            {
                // this is a new key, insert it in this group
                iRetVal = InsertKey(szKey, szVal, iGroupLine);
                if (E_OK != iRetVal)
                {
                    RIL_LOG_CRITICAL("CRepository::Write() - ERROR: Could not add new key to existing group.\r\n");
                }
            }
            break;

        case E_NOT_FOUND:
            // create a new group and key
            iRetVal = CreateGroup(szGroup, szKey, szVal);
            break;

        default:
            // We should never get here...
            RIL_LOG_CRITICAL("CRepository::Write() - ERROR: Something bad happened when trying to locate the specified group.\r\n");
            break;
    }

Error:
    CloseRepositoryFile();
    WriterUnlock();

    return (E_OK == iRetVal);
}

int CRepository::ReplaceKey(const char* szKey, const char* szVal)
{
    int  iRetVal = E_ERROR;
    int  iFd;
    int  iLine = m_iLine - 1;  // after we found the key, the line counter points to the next line
    int  iSize;
    char szBuf[MAX_LINE_LEN];

    // create temporary file
    iFd = open(REPO_TMP_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (iFd < 0)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not create temporary file (\"%s\") - %s\r\n", REPO_TMP_FILE, strerror(iErrCode));
        goto Error;
    }

    // write from the top of the file to the current line, excluding the current line
    if (E_OK != DumpLines(iFd, 1, iLine))
    {
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not write the lines before the modified entry.\r\n");
        goto Error;
    }

    // write key
    sprintf(szBuf, "\t%s\t%s\n", szKey, szVal);
    iSize = strlen(szBuf);
    if (write(iFd, szBuf, iSize) != iSize)
    {
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not write the modified entry.\r\n");
        goto Error;
    }

    // dump rest of lines
    if (E_OK != DumpLines(iFd, iLine + 1, END_OF_FILE))
    {
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not write the lines after the modified entry.\r\n");
        goto Error;
    }

    // REPLACE THE MAIN FILE WITH THE TEMP FILE

    // Close the temp file
    close(iFd);
    iFd = -1;

    // Close the main file
    CloseRepositoryFile();

    // Delete the main file
    remove(REPO_FILE);

    // Rename the temp file so that it becomes the main file
    iRetVal = rename(REPO_TMP_FILE, REPO_FILE) == 0 ? E_OK : E_ERROR;
    if (E_OK != iRetVal)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not replace \"%s\" with the updated version (\"%s\") - %s.\r\n", REPO_FILE, REPO_TMP_FILE, strerror(iErrCode));
        goto Error;
    }

    // Reopen the main file
    if (!OpenRepositoryFile())
    {
        RIL_LOG_CRITICAL("CRepository::ReplaceKey() - ERROR: Could not open the Repository file.\r\n");
        iRetVal = E_ERROR;
        goto Error;
    }

Error:
    if (iFd >= 0)
    {
        close(iFd);
        remove(REPO_TMP_FILE);
    }

    return iRetVal;
}

int CRepository::InsertKey(const char* szKey, const char* szVal, int iGroupLine)
{
    int  iRetVal = E_ERROR;
    int  iFd;
    int  iSize;
    char szBuf[MAX_LINE_LEN];

    // create temporary file
    iFd = open(REPO_TMP_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (iFd < 0)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not create temporary file (\"%s\") - %s\r\n", REPO_TMP_FILE, strerror(iErrCode));
        goto Error;
    }

    // write from the top of the file to the group line, included
    if (E_OK != DumpLines(iFd, 1, iGroupLine))
    {
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not write the lines before the new entry.\r\n");
        goto Error;
    }

    // write key
    sprintf(szBuf, "\t%s\t%s\n", szKey, szVal);
    iSize = strlen(szBuf);
    if (write(iFd, szBuf, iSize) != iSize)
    {
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not write the new entry.\r\n");
        goto Error;
    }

    // dump rest of lines
    if (E_OK != DumpLines(iFd, iGroupLine, END_OF_FILE))
    {
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not write the lines after the new entry.\r\n");
        goto Error;
    }

    // REPLACE THE MAIN FILE WITH THE TEMP FILE

    // Close the temp file
    close(iFd);
    iFd = -1;

    // Close the main file
    CloseRepositoryFile();

    // Delete the main file
    remove(REPO_FILE);

    // Rename the temp file so that it becomes the main file
    iRetVal = rename(REPO_TMP_FILE, REPO_FILE) == 0 ? E_OK : E_ERROR;
    if (E_OK != iRetVal)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not replace \"%s\" with the updated version (\"%s\") - %s.\r\n", REPO_FILE, REPO_TMP_FILE, strerror(iErrCode));
        goto Error;
    }

    // Reopen the main file
    if (!OpenRepositoryFile())
    {
        RIL_LOG_CRITICAL("CRepository::InsertKey() - ERROR: Could not open the Repository file.\r\n");
        iRetVal = E_ERROR;
        goto Error;
    }

Error:
    if (iFd >= 0)
    {
        close(iFd);
        remove(REPO_TMP_FILE);
    }

    return iRetVal;
}

int CRepository::CreateGroup(const char* szGroup, const char* szKey, const char* szVal)
{
    int  iRetVal = E_ERROR;
    int  iSize;
    char szBuf[MAX_LINE_LEN];

    // go to end of file
    lseek(m_iFd, 0, SEEK_END);

    // append group
    sprintf(szBuf, "\n%s %s\n", GROUP_MARKER, szGroup);
    iSize = strlen(szBuf);
    if (write(m_iFd, szBuf, iSize) != iSize)
    {
        RIL_LOG_CRITICAL("CRepository::CreateGroup() - ERROR: Could not create \"%s\" group.\r\n", szGroup);
        goto Error;
    }

    // append key and value
    sprintf(szBuf, "\t%s\t%s\n", szKey, szVal);
    iSize = strlen(szBuf);
    if (write(m_iFd, szBuf, iSize) != iSize)
    {
        RIL_LOG_CRITICAL("CRepository::CreateGroup() - ERROR: Could not create \"%s\" key.\r\n", szKey);
        goto Error;
    }

    iRetVal = E_OK;

Error:
    return iRetVal;
}

int CRepository::DumpLines(int iFd, int iFrom, int iTo)
{
    int iRetVal = E_ERROR;

    if (E_OK != GotoLine(iFrom))
        goto Error;

    while (m_iLine != iTo)
    {
        char szBuf[MAX_LINE_LEN];
        int  iSize;
        int  iRes;

        iRes = ReadLine(szBuf, FALSE);
        if (E_OK != iRes && E_EOF != iRes)
            goto Error;

        iSize = strlen(szBuf);
        if (write(iFd, szBuf, iSize) != iSize)
            goto Error;

        if (E_EOF == iRes)
            break;
    }

    iRetVal = E_OK;

Error:
    return iRetVal;
}

void CRepository::ResetFile()
{
    lseek(m_iFd, 0, SEEK_SET);
    m_iLine = 1;
}

int CRepository::GotoLine(int iLine)
{
    int iRetVal = E_ERROR;

    if (iLine < m_iLine)
        ResetFile();

    while (m_iLine != iLine)
    {
        if (E_OK != ReadLine())
            goto Error;
    }

    iRetVal = E_OK;

Error:
    return iRetVal;
}

int CRepository::ReadLine(char* szBuf, bool bRemoveCRAndLF)
{
    int  iRetVal = E_ERROR;
    int  iCount = 0;
    char cCh;

    // TODO: limit to 256 characters

    while(1)
    {
        switch(read(m_iFd, &cCh, 1))
        {
        case 1:
            if (cCh == '\n' || cCh == '\r')
            {
                if (szBuf)
                {
                    if (!bRemoveCRAndLF)
                        szBuf[iCount++] = cCh;

                    szBuf[iCount] = '\0';
                }
                iRetVal = E_OK;
                ++m_iLine;
                goto Done;
            }
            else if (szBuf)
            {
                szBuf[iCount++] = cCh;
            }
            break;

        case 0:
            // end of file
            if (szBuf)
                szBuf[iCount] = '\0';

            iRetVal = E_EOF;
            goto Done;
            break;  // unreachable

        default:
            // error
            if (szBuf)
                szBuf[iCount] = '\0';

            iRetVal = E_ERROR;
            goto Done;
            break;  // unreachable
        }
    }

Done:
    return iRetVal;
}


void CRepository::RemoveComment(char* szIn)
{
    // locate // marker
    const char *szMarker = "//";
    char *pSubStr = strstr(szIn, szMarker);
    if (pSubStr)
        *pSubStr= '\0';
}

char* CRepository::SkipSpace(char *szIn)
{
    if (szIn != NULL)
    {
        for (; *szIn != '\0' && isspace(*szIn); ++szIn);
    }
    return szIn;
}

void CRepository::RemoveTrailingSpaces(char * szIn)
{
    if (NULL != szIn)
    {
        int indexWalk = strlen(szIn);

        while (0 < indexWalk)
        {
            // If the current character is a space, replace it with NULL.
            // If the current character is NULL, skip it.
            // Otherwise, we are done.

            if (' ' == szIn[indexWalk])
            {
                szIn[indexWalk] = '\0';
            }
            else if ('\0' != szIn[indexWalk])
            {
                break;
            }

            indexWalk--;
        }
    }
}

char* CRepository::SkipAlphaNum(char *szIn)
{
    if (szIn != NULL)
    {
        for (; *szIn != '\0' && isalnum(*szIn); ++szIn);
    }
    return szIn;
}

int CRepository::LocateGroup(const char* szGroup)
{
    char  szBuf[MAX_LINE_LEN];
    char* pBuf;
    int   iCount = 0;
    int   iRetVal = E_ERROR;

    while (1)
    {
        int iRes = ReadLine(szBuf);
        switch(iRes)
        {
            case E_OK:
            case E_EOF:
                pBuf= SkipSpace(szBuf);
                if (strncmp(pBuf, GROUP_MARKER, GROUP_MARKER_LEN) == 0)
                {
                    // is this our group?
                    pBuf += GROUP_MARKER_LEN;
                    pBuf = SkipSpace(pBuf);
                    if (strncmp(pBuf, szGroup, strlen(szGroup)) == 0)
                    {
                        // group found
                        iRetVal = E_OK;
                        goto Done;
                    }
                }
                if (E_EOF == iRes)
                {
                    iRetVal = E_NOT_FOUND;
                    goto Done;
                }
                break;

            default:
                iRetVal = E_ERROR;
                goto Done;
                break;  // unreachable
        }
    }

Done:
    return iRetVal;
}


int CRepository::LocateKey(const char* szKey, char* szOut)
{
    char  szBuf[MAX_LINE_LEN];
    char* pBuf;
    int   iRetVal = E_ERROR;
    int   iKeyLen = strlen(szKey);

    while (1)
    {
        int iRes = ReadLine(szBuf);
        switch(iRes)
        {
            case E_OK:
            case E_EOF:
                pBuf= SkipSpace(szBuf);
                if (strncmp(pBuf, szKey, iKeyLen) == 0)
                {
                    if (szOut)
                    {
                        strcpy(szOut, szBuf);
                    }

                    iRetVal = E_OK;
                    goto Done;
                }
                else if (strncmp(pBuf, GROUP_MARKER, GROUP_MARKER_LEN) == 0)
                {
                    // into next group
                    iRetVal = E_NOT_FOUND;
                    goto Done;
                }

                if (E_EOF == iRes)
                {
                    iRetVal = E_NOT_FOUND;
                    goto Done;
                }
                break;

            default:
                iRetVal = E_ERROR;
                goto Done;
                break;  // unreachable
        }
    }

Done:
    return iRetVal;
}

int CRepository::ExtractValue(char* szIn, char* szRes, unsigned int iMaxLen)
{
    // skip key
    szIn = SkipSpace(szIn);
    szIn = SkipAlphaNum(szIn);
    szIn = SkipSpace(szIn);
    RemoveTrailingSpaces(szIn);

    // copy value
    if (strlen(szIn) >= iMaxLen)
    {
        strncpy(szRes, szIn, iMaxLen - 1);
        szRes[iMaxLen - 1] = '\0';
    }
    else
    {
        strcpy(szRes, szIn);
    }

    return E_OK;
}

int CRepository::ReaderLock()
{
    int iRetVal = E_ERROR;

    if (0 == pthread_mutex_lock(&m_stLock.stLock))
    {
        if (m_stLock.iWriters || m_stLock.iWriteWaiters)
        {
            // writers accessing or waiting to access, need to wait
            ++m_stLock.iReadWaiters;
            close(m_iFd);
            m_iFd = -1;
            do
            {
                if (0 != pthread_cond_wait(&m_stLock.stRead, &m_stLock.stLock))
                    goto Error;

            }
            while (m_stLock.iWriters || m_stLock.iWriteWaiters);

            m_iFd = open(REPO_FILE, O_RDWR);
            m_iLine = 1;
            --m_stLock.iReadWaiters;
        }
        ++m_stLock.iReaders;
        iRetVal = (pthread_mutex_unlock(&m_stLock.stLock) == 0) ? E_OK : E_ERROR;
    }

Error:
    return iRetVal;
}

int CRepository::ReaderUnlock()
{
    int iRetVal = E_ERROR;

    if (0 != pthread_mutex_lock(&m_stLock.stLock))
        goto Error;

    --m_stLock.iReaders;
    if (m_stLock.iWriteWaiters)
    {
        // signal writers to go ahead
        if (0 != pthread_cond_signal(&m_stLock.stWrite))
        {
            // ignore error
        }
    }

    iRetVal = (pthread_mutex_unlock(&m_stLock.stLock) == 0) ? E_OK : E_ERROR;

Error:
    return iRetVal;
}

int CRepository::WriterLock()
{
    int iRetVal = E_ERROR;

    if (0 != pthread_mutex_lock(&m_stLock.stLock))
        goto Error;

    if (m_stLock.iReaders || m_stLock.iWriters)
    {
        // file being accessed, have to wait
        ++m_stLock.iWriteWaiters;
        close(m_iFd);
        m_iFd = -1;
        do
        {
            if (0 != pthread_cond_wait(&m_stLock.stWrite, &m_stLock.stLock))
                goto Error;
        }
        while (m_stLock.iReaders || m_stLock.iWriters);

        m_iFd = open(REPO_FILE, O_RDWR);
        m_iLine = 1;
        --m_stLock.iWriteWaiters;
    }
    m_stLock.iWriters = 1;
    iRetVal = (pthread_mutex_unlock(&m_stLock.stLock) == 0) ? E_OK : E_ERROR;

Error:
    return iRetVal;
}

int CRepository::WriterUnlock()
{
    int iRetVal = E_ERROR;

    if (0 != pthread_mutex_lock(&m_stLock.stLock))
        goto Error;

    m_stLock.iWriters = 0;
    if (m_stLock.iWriteWaiters)
    {
        if (0 != pthread_cond_signal(&m_stLock.stWrite))
        {
            // ignore error
        }
    }
    else if (m_stLock.iReadWaiters)
    {
        if (0 != pthread_cond_broadcast(&m_stLock.stRead))
        {
            // ignore error
        }
    }

    iRetVal = (pthread_mutex_unlock(&m_stLock.stLock) == 0) ? E_OK : E_ERROR;

Error:
    return iRetVal;
}

