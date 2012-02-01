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
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "types.h"
#include "rril.h"
#include "repository.h"
#include "rillog.h"


//////////////////////////////////////////////////////////////////////////
// Standardized Non-Volatile Memory Access Strings

const char   g_szGroupRequestTimeouts[]        = "RequestTimeouts";

char         g_szItemRequestTimeouts[MAX_REQUEST_ITEM_LENGTH];

//  These strings are used for Request ID Parameters in repository.txt
//  The strings must be in the same order as the Request IDs defined in request_id.h
//
const char * g_szRequestNames[] =
{
    "GetSimStatus",                         // ND_REQ_ID_GETSIMSTATUS 0
    "EnterSimPin",                          // ND_REQ_ID_ENTERSIMPIN 1
    "EnterSimPuk",                          // ND_REQ_ID_ENTERSIMPUK 2
    "EnterSimPin2",                         // ND_REQ_ID_ENTERSIMPIN2 3
    "EnterSimPuk2",                         // ND_REQ_ID_ENTERSIMPUK2 4
    "ChangeSimPin",                         // ND_REQ_ID_CHANGESIMPIN 5
    "ChangeSimPin2",                        // ND_REQ_ID_CHANGESIMPIN2 6
    "EnterNetworkDepersonalization",        // ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION 7
    "GetCurrentCalls",                      // ND_REQ_ID_GETCURRENTCALLS 8
    "Dial",                                 // ND_REQ_ID_DIAL 9
    "GetIMSI",                              // ND_REQ_ID_GETIMSI 10
    "Hangup",                               // ND_REQ_ID_HANGUP 11
    "HangupWaitingOrBackground",            // ND_REQ_ID_HANGUPWAITINGORBACKGROUND 12
    "HangupForegroundResumeBackground",     // ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND 13
    "SwitchHoldingAndActive",               // ND_REQ_ID_SWITCHHOLDINGANDACTIVE 14
    "Conference",                           // ND_REQ_ID_CONFERENCE 15
    "UDUB",                                 // ND_REQ_ID_UDUB 16
    "LastCallFailCause",                    // ND_REQ_ID_LASTCALLFAILCAUSE 17
    "SignalStrength",                       // ND_REQ_ID_SIGNALSTRENGTH 18
    "RegistrationState",                    // ND_REQ_ID_REGISTRATIONSTATE 19
    "GprsRegistrationState",                // ND_REQ_ID_GPRSREGISTRATIONSTATE 20
    "Operator",                             // ND_REQ_ID_OPERATOR 21
    "RadioPower",                           // ND_REQ_ID_RADIOPOWER 22
    "Dtmf",                                 // ND_REQ_ID_DTMF 23
    "SendSms",                              // ND_REQ_ID_SENDSMS 24
    "SendSmsExpectMore",                    // ND_REQ_ID_SENDSMSEXPECTMORE 25
    "SetupDefaultPDP",                      // ND_REQ_ID_SETUPDEFAULTPDP 26
    "SimIO",                                // ND_REQ_ID_SIMIO 27
    "SendUSSD",                             // ND_REQ_ID_SENDUSSD 28
    "CancelUSSD",                           // ND_REQ_ID_CANCELUSSD 29
    "GetCLIR",                              // ND_REQ_ID_GETCLIR 30
    "SetCLIR",                              // ND_REQ_ID_SETCLIR 31
    "QueryCallForwardStatus",               // ND_REQ_ID_QUERYCALLFORWARDSTATUS 32
    "SetCallForward",                       // ND_REQ_ID_SETCALLFORWARD 33
    "QueryCallWaiting",                     // ND_REQ_ID_QUERYCALLWAITING 34
    "SetCallWaiting",                       // ND_REQ_ID_SETCALLWAITING 35
    "SmsAcknowledge",                       // ND_REQ_ID_SMSACKNOWLEDGE 36
    "GetIMEI",                              // ND_REQ_ID_GETIMEI 37
    "GetIMEISV",                            // ND_REQ_ID_GETIMEISV 38
    "Answer",                               // ND_REQ_ID_ANSWER 39
    "DeactivateDataCall",                   // ND_REQ_ID_DEACTIVATEDATACALL 40
    "QueryFacilityLock",                    // ND_REQ_ID_QUERYFACILITYLOCK 41
    "SetFacilityLock",                      // ND_REQ_ID_SETFACILITYLOCK 42
    "ChangeBarringPassword",                // ND_REQ_ID_CHANGEBARRINGPASSWORD 43
    "QueryNetworkSelectionMode",            // ND_REQ_ID_QUERYNETWORKSELECTIONMODE 44
    "SetNetworkSelectionAutomatic",         // ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC 45
    "SetNetworkSelectionManual",            // ND_REQ_ID_SETNETWORKSELECTIONMANUAL 46
    "QueryAvailableNetworks",               // ND_REQ_ID_QUERYAVAILABLENETWORKS 47
    "RequestDtmfStart",                     // ND_REQ_ID_REQUESTDTMFSTART 48
    "RequestDtmfStop",                      // ND_REQ_ID_REQUESTDTMFSTOP 49
    "BasebandVersion",                      // ND_REQ_ID_BASEBANDVERSION 50
    "SeperateConnection",                   // ND_REQ_ID_SEPERATECONNECTION 51
    "SetMute",                              // ND_REQ_ID_SETMUTE 52
    "GetMute",                              // ND_REQ_ID_GETMUTE 53
    "QueryCLIP",                            // ND_REQ_ID_QUERYCLIP 54
    "LastPdpFailCause",                     // ND_REQ_ID_LASTPDPFAILCAUSE 55
    "PdpContextList",                       // ND_REQ_ID_PDPCONTEXTLIST 56
    "ResetRadio",                           // ND_REQ_ID_RESETRADIO 57
    "OemHookRaw",                           // ND_REQ_ID_OEMHOOKRAW 58
    "OemHookStrings",                       // ND_REQ_ID_OEMHOOKSTRINGS 59
    "ScreenState",                          // ND_REQ_ID_SCREENSTATE 60
    "SetSuppSvcNotification",               // ND_REQ_ID_SETSUPPSVCNOTIFICATION 61
    "WriteSmsToSim",                        // ND_REQ_ID_WRITESMSTOSIM 62
    "DeleteSmsOnSim",                       // ND_REQ_ID_DELETESMSONSIM 63
    "SetBandMode",                          // ND_REQ_ID_SETBANDMODE 64
    "QueryAvailableBandMode",               // ND_REQ_ID_QUERYAVAILABLEBANDMODE 65
    "StkGetProfile",                        // ND_REQ_ID_STKGETPROFILE 66
    "StkSetProfile",                        // ND_REQ_ID_STKSETPROFILE 67
    "StkSendEnvelopeCommand",               // ND_REQ_ID_STKSENDENVELOPECOMMAND 68
    "StkSendTerminalResponse",              // ND_REQ_ID_STKSENDTERMINALRESPONSE 69
    "StkHandleCallSetupRequestedFromSim",   // ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM 70
    "ExplicitCallTransfer",                 // ND_REQ_ID_EXPLICITCALLTRANSFER 71
    "SetPreferredNetworkType",              // ND_REQ_ID_SETPREFERREDNETWORKTYPE 72
    "GetPreferredNetworkType",              // ND_REQ_ID_GETPREFERREDNETWORKTYPE 73
    "GetNeighboringCellIDs",                // ND_REQ_ID_GETNEIGHBORINGCELLIDS 74
    "SetLocationUpdates",                   // ND_REQ_ID_SETLOCATIONUPDATES 75
    "CdmaSetSubscription",                  // ND_REQ_ID_CDMASETSUBSCRIPTION 76
    "CdmaSetRoamingPreference",             // ND_REQ_ID_CDMASETROAMINGPREFERENCE 77
    "CdmaQueryRoamingPreference",           // ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE 78
    "SetTtyMode",                           // ND_REQ_ID_SETTTYMODE 79
    "QueryTtyMode",                         // ND_REQ_ID_QUERYTTYMODE 80
    "CdmaSetPreferredVoicePrivacyMode",     // ND_REQ_ID_CDMASETPREFERREDVOICEPRIVACYMODE 81
    "CdmaQueryPreferredVoicePrivacyMode",   // ND_REQ_ID_CDMAQUERYPREFERREDVOICEPRIVACYMODE 82
    "CdmaFlash",                            // ND_REQ_ID_CDMAFLASH 83
    "CdmaBurstDtmf",                        // ND_REQ_ID_CDMABURSTDTMF 84
    "CdmaValidateKey",                      // ND_REQ_ID_CDMAVALIDATEAKEY 85
    "CdmaSendSms",                          // ND_REQ_ID_CDMASENDSMS 86
    "CdmaSmsAcknowledge",                   // ND_REQ_ID_CDMASMSACKNOWLEDGE 87
    "GetBroadcastSmsConfig",                // ND_REQ_ID_GETBROADCASTSMSCONFIG 88
    "SetBroadcastSmsConfig",                // ND_REQ_ID_SETBROADCASTSMSCONFIG 89
    "SmsBroadcastActivation",               // ND_REQ_ID_SMSBROADCASTACTIVATION 90
    "CdmaGetBroadcastSmsConfig",            // ND_REQ_ID_CDMAGETBROADCASTSMSCONFIG 91
    "CdmaSetBroadcastSmsConfig",            // ND_REQ_ID_CDMASETBROADCASTSMSCONFIG 92
    "CdmaSmsBroadcastActivation",           // ND_REQ_ID_CDMASMSBROADCASTACTIVATION 93
    "CdmaSubscription",                     // ND_REQ_ID_CDMASUBSCRIPTION 94
    "CdmaWriteSmsToRuim",                   // ND_REQ_ID_CDMAWRITESMSTORUIM 95
    "CdmaDeleteSmsOnRuim",                  // ND_REQ_ID_CDMADELETESMSONRUIM 96
    "DeviceIdentity",                       // ND_REQ_ID_DEVICEIDENTITY 97
    "ExitEmergencyCallBackMode",            // ND_REQ_ID_EXITEMERGENCYCALLBACKMODE 98
    "GetSmscAddress",                       // ND_REQ_ID_GETSMSCADDRESS 99
    "SetSmscAddress",                       // ND_REQ_ID_SETSMSCADDRESS 100
    "ReportSmsMemoryStatus",                // ND_REQ_ID_REPORTSMSMEMORYSTATUS 101
    "ReportStkServiceIsRunning",            // ND_REQ_ID_REPORTSTKSERVICEISRUNNING 102
    "GetIPAddress",                         // ND_REQ_ID_GETIPADDRESS 103
    "GetDNS",                               // ND_REQ_ID_GETDNS 104
    "QueryPin2",                            // ND_REQ_ID_QUERYPIN2 105
    "PdpContextListUnsol",                  // ND_REQ_ID_PDPCONTEXTLIST_UNSOL 106
    "SimTransmitBasic",                     // ND_REQ_ID_SIMTRANSMITBASIC 107
    "SimOpenChannel",                       // ND_REQ_ID_SIMOPENCHANNEL 108
    "SimCloseChannel",                      // ND_REQ_ID_SIMCLOSECHANNEL 109
    "SimTransmitChannel",                   // ND_REQ_ID_SIMTRANSMITCHANNEL 110
    "QuerySimSmsStoreStatus",               // ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS 111
#if defined(M2_VT_FEATURE_ENABLED)
    "HangupVT",                             // ND_REQ_ID_HANGUPVT 112
    "DialVT",                               // ND_REQ_ID_DIALVT 113
#endif // M2_VT_FEATURE_ENABLED
};

/////////////////////////////////////////////////

const char   g_szGroupOtherTimeouts[]          = "OtherTimeouts";

const char   g_szTimeoutCmdInit[]              = "TimeoutCmdInit";
const char   g_szTimeoutAPIDefault[]           = "TimeoutAPIDefault";
const char   g_szTimeoutWaitForInit[]          = "TimeoutWaitForInit";

/////////////////////////////////////////////////

const char   g_szGroupRILSettings[]            = "RILSettings";

const char   g_szTimeoutThresholdForRetry[]    = "TimeoutThresholdForRetry";
const char   g_szOpenPortRetries[]             = "OpenPortRetries";
const char   g_szOpenPortInterval[]            = "OpenPortInterval";

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

const char   g_szGroupModem[]                   = "Modem";

const char   g_szSupportedModem[]               = "SupportedModem";
const char   g_szNetworkInterfaceNamePrefix[]   = "NetworkInterfaceNamePrefix";
const char   g_szDisableModemReset[]            = "DisableModemReset";
const char   g_szDisableCoreDump[]              = "DisableCoreDump";
const char   g_szCoreDumpTimeout[]              = "CoreDumpTimeout";
const char   g_szDisableWatchdogThread[]        = "DisableWatchdogThread";
const char   g_szRadioResetDelay[]              = "RadioResetDelay";
const char   g_szRadioResetStartStmdDelay[]     = "RadioResetStartStmdDelay";
const char   g_szEnableCellInfo[]               = "EnableCellInfo";
const char   g_szRxDiversity3GEnable[]          = "RxDiversity3GEnable";
const char   g_szRxDiversity2GDARP[]            = "RxDiversity2GDARP";
const char   g_szFDDelayTimer[]                 = "FDDelayTimer";
const char   g_szSCRITimer[]                    = "SCRITimer";

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

struct CRepository::CRepLock CRepository::m_stLock = {{0}, {0}, 0, 0};
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
    CRepository::m_stLock.iReadWaiters = 0;

    if (0 != pthread_mutex_init(&m_stLock.stLock, NULL))
    {
        goto Error;
    }

    if (0 != pthread_cond_init(&m_stLock.stRead, NULL))
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
        m_bInitialized = FALSE;
    }

    return TRUE;
}

BOOL CRepository::OpenRepositoryFile()
{
    BOOL fRetVal = FALSE;

    m_iLine = 1;
    m_iFd   = open(REPO_FILE, O_RDONLY);

    if (m_iFd < 0)
    {
        int iErrCode = errno;
        RIL_LOG_CRITICAL("CRepository::OpenRepositoryFile() - Could not open file \"%s\" - %s\r\n", REPO_FILE, strerror(iErrCode));
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
        RIL_LOG_CRITICAL("CRepository::Read() - Repository has not been initialized.\r\n");
        goto Error;
    }

    // grab lock before accessing the NVM file
    ReaderLock();

    if (!OpenRepositoryFile())
    {
        RIL_LOG_CRITICAL("CRepository::Read() - Could not open the Repository file.\r\n");
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
    iRetVal = (pthread_mutex_unlock(&m_stLock.stLock) == 0) ? E_OK : E_ERROR;

Error:
    return iRetVal;
}

