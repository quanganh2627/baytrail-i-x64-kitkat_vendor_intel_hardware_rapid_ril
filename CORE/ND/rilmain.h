////////////////////////////////////////////////////////////////////////////
// rilmain.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides function prototypes for top-level RIL functions.
//    Also includes top-level constant definitions
//    and global variable declarations.
//
// Author:  Dennis Peter
// Created: 2007-07-12
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RILMAIN_H
#define RRIL_RILMAIN_H

#pragma once


extern BOOL        g_cphschange;
extern const char *g_pcszCPHS;

#ifdef RIL_ENABLE_EONS
#include <eons.h>
extern EONS      g_eons;
extern EONSCache g_eonscache;
extern BOOL      g_eonschange;
#endif

//
#define MAX_DRIVERINIT_TIME    10000

// Globals
extern CEvent * g_pCriticalErrorEvent;

// Functions
#if !defined(__linux__)
HRESULT RRILND_GetNextNotification(UINT32 dwParam, RILDRVNOTIFICATION* lpRilDrvNotification);
CRilInstanceHandle* const ExtractHandle(const UINT32 dwParam, BOOL bOverrideEmergency = FALSE);
#endif // !defined(__linux__)

BOOL ClearCmdHandle(void* pItem, UINT32 dwDummy);

void SignalCriticalError(UINT32 dwEventId, UINT32 dwLineNum, const BYTE* lpszFileName);
void SignalCriticalErrorAsync(UINT32 dwEventId, UINT32 dwLineNum, const BYTE* lpszFileName);


// FIXME
// EVENT for critical error
#define RILLOG_EVENT_SENDINGCMD              0
#define RILLOG_EVENT_PARSERSPOK              1
#define RILLOG_EVENT_PARSERSPFAILED          2
#define RILLOG_EVENT_RADIOPRESENT            3
#define RILLOG_EVENT_RADIOREMOVED            4
#define RILLOG_EVENT_DRVINITFAILED           5
#define RILLOG_EVENT_LOWMEMORY               6
#define RILLOG_EVENT_TOOMANYTIMEOUTS         7
#define RILLOG_EVENT_COULDNTSENDINIT         8
#define RILLOG_EVENT_GENERALREINIT           9
#define RILLOG_EVENT_SMSREINIT              10
#define RILLOG_EVENT_COMMANDTHREADEXIT      11
#define RILLOG_EVENT_PARSEDGARBAGE          12
#define RILLOG_EVENT_PARSEDNOTIFICATION     13
#define RILLOG_EVENT_USSDRECEIVED           14
#define RILLOG_EVENT_CMDTIMEDOUT            15
#define RILLOG_EVENT_CMDTIMEDOUTSUPPRESSED  16
#define RILLOG_EVENT_NOONEWAITING           17
#define RILLOG_EVENT_UNRECOGNIZEDRSP        18
#define RILLOG_EVENT_CMDRSPOK               19
#define RILLOG_EVENT_CMDRSPERROR            20
#define RILLOG_EVENT_SENDINGCMDSUPPRESSED   21
#define RILLOG_EVENT_PRSRSPOKSUPPRESSED     22
#define RILLOG_EVENT_PRSRSPFAILEDSUPPRESSED 23
#define RILLOG_EVENT_GPRSFATALERROR         24
#define RILLOG_EVENT_RADIOFAILUREREASON     25

#endif // RRIL_RILMAIN_H
