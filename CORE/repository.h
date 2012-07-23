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
 *    Class declaration for Non-Volatile storage
 *
 *
 */

#ifndef RRIL_REPOSITORY_H
#define RRIL_REPOSITORY_H

#include <pthread.h>
#include "types.h"


#define MAX_REQUEST_ITEM_LENGTH    64

// define max FD timers
#define MIN_FDDELAY_TIMER          1
#define MIN_SCRI_TIMER             1
#define MAX_FDDELAY_TIMER          60
#define MAX_SCRI_TIMER             120

//////////////////////////////////////////////////////////////////////////
// String Constants used for NVM Access

extern const char   g_szGroupRequestTimeouts[];

extern char         g_szItemRequestTimeouts[MAX_REQUEST_ITEM_LENGTH];

extern const char * g_szRequestNames[];

/////////////////////////////////////////////////

extern const char   g_szGroupOtherTimeouts[];

extern const char   g_szTimeoutCmdInit[];
extern const char   g_szTimeoutAPIDefault[];
extern const char   g_szTimeoutWaitForInit[];

/////////////////////////////////////////////////

extern const char   g_szGroupRILSettings[];

extern const char   g_szTimeoutThresholdForRetry[];
extern const char   g_szOpenPortRetries[];
extern const char   g_szOpenPortInterval[];

/////////////////////////////////////////////////

extern const char   g_szGroupInitCmds[];

extern const char * g_szPreInitCmds[];
extern const char * g_szPostInitCmds[];

//////////////////////////////////////////////////////////////////////////

extern const char   g_szGroupModem[];

extern const char   g_szSupportedModem[];
extern const char   g_szNetworkInterfaceNamePrefix[];
extern const char   g_szMTU[];
extern const char   g_szEnableCellInfo[];
extern const char   g_szRxDiversity3GEnable[];
extern const char   g_szRxDiversity2GDARP[];
extern const char   g_szFDDelayTimer[];
extern const char   g_szSCRITimer[];
extern const char   g_szFDMode[];
extern const char   g_szTempOoSNotificationEnable[];
#if defined(BOARD_HAVE_IFX7060)
extern const char   g_szHsiChannelsReservedForDataDirectlyoverHsi[];
extern const char   g_szApnTypeDefault[];
extern const char   g_szApnTypeThetered[];
extern const char   g_szApnTypeIMS[];
extern const char   g_szApnTypeMMS[];
extern const char   g_szApnTypeCBS[];
extern const char   g_szApnTypeFOTA[];
extern const char   g_szApnTypeSUPL[];
extern const char   g_szApnTypeHIPRI[];
#endif

//////////////////////////////////////////////////////////////////////////

extern const char   g_szGroupLogging[];

extern const char   g_szLogLevel[];


//////////////////////////////////////////////////////////////////////////

class CRepository
{
public:
    CRepository();
    ~CRepository();

public:
    // init and clean-up APIs; Init() must be called befor any other APIs
    // if successful, return 0
    // otherwise return -1
    static BOOL Init();
    static BOOL Close();

public:
    // APIs to read from and write to the NVM
    // if successful, return 0
    // otherwise return -1
    BOOL Read(const char *szGroup, const char* szKey, int& iRes);
    BOOL Read(const char *szGroup, const char* szKey, char* szRes, int iMaxLen);
    BOOL ReadFDParam(const char *szGroup, const char* szKey, char* szRes, int iMaxLen, int iMinVal, int iMaxVal);

private:
    // file access and manipulation
    BOOL  OpenRepositoryFile(void);
    void  CloseRepositoryFile(void);
    int   ReadLine(char *szBuf = NULL, bool bRemoveCRAndLF = true);
    void  RemoveComment(char* szIn);
    char* SkipSpace(char *szIn);
    void  RemoveTrailingSpaces(char * szIn);
    char* SkipAlphaNum(char *szIn);
    int   DumpLines(int iFd, int iFrom, int iTo);
    void  ResetFile(void);
    int   GotoLine(int iLine);

    // marker location utilities
    int LocateGroup(const char* szGroup);
    int LocateKey(const char* szKey, char* szOut = NULL);
    int ExtractValue(char* szIn, char* szRes, UINT32 iMaxLen);

    // locking/unlocking access to the NVM file
    int ReaderLock();
    int ReaderUnlock();

private:
    struct CRepLock
    {
        pthread_mutex_t stLock;         // master lock
        pthread_cond_t  stRead;         // lock for reader threads
        UINT32          iReaders;       // # reader threads accessing the file
        UINT32          iReadWaiters;   // # reader threads waiting to access the file
    };

    static struct CRepLock m_stLock;

private:
    // constants
    static const int END_OF_FILE  = -1;
    static const int MAX_INT_LEN  = 20;     // arbitrary number, long enough to hold a string-representation of a 32-bit int
    static const int MAX_LINE_LEN = 256;    // maximum length for a line in the NVM file

private:
    int m_iFd;                   // file descriptor
    int m_iLine;                 // current line in file
    static bool m_bInitialized;  // TRUE if repository initialized, FALSE otherwise
};

#endif // RRIL_REPOSITORY_H
