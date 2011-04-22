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
 *  Author: Francesc J. Vilarino Guell
 *  Created: 2009-08-17
 *
 */

#ifndef RRIL_REPOSITORY_H
#define RRIL_REPOSITORY_H

#include <pthread.h>


#define MAX_REQUEST_GROUP_LENGTH  256
#define MAX_REQUEST_ITEM_LENGTH    64


//////////////////////////////////////////////////////////////////////////
// String Constants used for NVM Access

extern const char   g_szGroupRequestTimeouts[];
extern const char   g_szGroupRequestNumRetries[];

extern char         g_szItemRequestTimeouts[MAX_REQUEST_ITEM_LENGTH];
extern char         g_szItemRequestNumRetries[MAX_REQUEST_ITEM_LENGTH];

extern const char * g_szRequestNames[];

/////////////////////////////////////////////////

extern const char   g_szGroupOtherTimeouts[];

extern const char   g_szTimeoutCmdInit[];
extern const char   g_szTimeoutCmdNoOp[];
extern const char   g_szTimeoutCmdOnline[];
extern const char   g_szTimeoutAPIDefault[];
extern const char   g_szTimeoutDTRDrop[];
extern const char   g_szTimeoutWaitForInit[];

/////////////////////////////////////////////////

extern const char   g_szGroupTTY[];

extern const char   g_szCommandPort[];
extern const char   g_szSimPort[];
extern const char   g_szData1Port[];
extern const char   g_szData2Port[];
extern const char   g_szData3Port[];
extern const char   g_szBaudRate[];
extern const char   g_szNumOpenAttempts[];
extern const char   g_szTimeBetweenOpenAttempts[];
extern const char   g_szDataModeInterruptionQuantum[];

/////////////////////////////////////////////////

extern const char   g_szGroupRILSettings[];

extern const char   g_szMaxCommandTimeouts[];
extern const char   g_szMaxInitRetries[];
extern const char   g_szDataModeCmdDelay[];
extern const char   g_szDataModeMaxTimeWithoutCmd[];
extern const char   g_szDefaultCmdRetries[];
extern const char   g_szRadioOnDelay[];
extern const char   g_szRadioOffDelay[];
extern const char   g_szUseUSIMAddress[];

/////////////////////////////////////////////////

extern const char   g_szGroupNetworkSettings[];
extern const char   g_szConnectRspDelay[];

/////////////////////////////////////////////////

extern const char   g_szGroupInitCmds[];

extern const char * g_szPreInitCmds[];
extern const char * g_szPostInitCmds[];

/////////////////////////////////////////////////

extern const char   g_szGroupEons[];

extern const char   g_szUseCphsPlmnName[];

/////////////////////////////////////////////////

extern const char   g_szGroupSecure[];

extern const char   g_szEonsEnabled[];

/////////////////////////////////////////////////

extern const char   g_szGroupLastValues[];

extern const char   g_szLastCLIP[];
extern const char   g_szLastCLIR[];
extern const char   g_szLastCOLP[];
extern const char   g_szLastCOLR[];
extern const char   g_szLastEquipmentState[];

/////////////////////////////////////////////////

extern const char   g_szGroupSIMTK[];

extern const char   g_szTerminalProfile[];

/////////////////////////////////////////////////

extern const char   g_szGroupOperatorNames[];

/////////////////////////////////////////////////

extern const char   g_szGroupSystemReady[];

extern const char   g_szQuerySimLock[];
extern const char   g_szInitializeSimSms[];
extern const char   g_szInitializeSimSTK[];
extern const char   g_szInitializeSimPhonebook[];
extern const char   g_szInitializeSimPhonebookTimeout[];

//////////////////////////////////////////////////////////////////////////

extern const char   g_szGroupModem[];

extern const char   g_szSupportedModem[];
extern const char   g_szNetworkInterfaceName[];
extern const char   g_szDisableModemReset[];
extern const char   g_szDisableCoreDump[];

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
    BOOL Write(const char *szGroup, const char* szKey, int iVal);
    BOOL Write(const char *szGroup, const char* szKey, const char* szVal);

private:
    // file access and manipulation
    BOOL  OpenRepositoryFile(void);
    void  CloseRepositoryFile(void);
    int   ReadLine(char *szBuf = NULL, bool bRemoveLF = true);
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
    int ExtractValue(char* szIn, char* szRes, unsigned int iMaxLen);
    int ReplaceKey(const char* szKey, const char* szVal);
    int InsertKey(const char* szKey, const char* szVal, int iGroupLine);
    int CreateGroup(const char* szGroup, const char* szKey, const char* szVal);

    // locking/unlocking access to the NVM file
    int ReaderLock();
    int ReaderUnlock();
    int WriterLock();
    int WriterUnlock();

private:
    struct CRepLock
    {
        pthread_mutex_t stLock;         // master lock
        pthread_cond_t  stRead;         // lock for reader threads
        pthread_cond_t  stWrite;        // lock for writer threads
        unsigned int    iReaders;       // # reader threads accessing the file
        unsigned int    iWriters;       // # writer threads accessing the file
        unsigned int    iReadWaiters;   // # reader threads waiting to access the file
        unsigned int    iWriteWaiters;  // # writer threads waiting to access the file
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
