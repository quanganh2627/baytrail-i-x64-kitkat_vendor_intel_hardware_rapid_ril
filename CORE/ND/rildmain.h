////////////////////////////////////////////////////////////////////////////
// rilmain.h
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for top-level RIL functions.
//
// Author:  Denis Londry
// Created: 2008-11-20
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 20/08  DL       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RILDMAIN_H
#define RRIL_RILDMAIN_H

#include "types.h"
#include <telephony/ril.h>

class CThread;

// The device path to use for the AT command channel
extern BYTE * g_szCmdPort;
// The device path to use for the data channel1
extern BYTE * g_szDataPort1;
// The device path to use for the data channel2
extern BYTE * g_szDataPort2;
// The device path to use for the data channel3
extern BYTE * g_szDataPort3;

extern BOOL  g_bIsSocket;

extern BOOL  g_bIsTriggerRadioError;

extern BOOL CreateWatchdogThread();

enum eRadioError
{
    eRadioError_ForceShutdown,
    eRadioError_LowMemory,
    eRadioError_ChannelDead,
    eRadioError_InitFailure,
    eRadioError_OpenPortFailure,
    eRadioError_ModemInitiatedCrash
};

void RIL_onRequestComplete(RIL_Token tRIL, RIL_Errno eErrNo, void *pResponse, size_t responseLen);

void RIL_onUnsolicitedResponse(int unsolResponseID, const void *pData, size_t dataSize);

void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const struct timeval * pRelativeTime);
void RIL_requestTimedCallback(RIL_TimedCallback callback, void * pParam, const unsigned long seconds, const unsigned long microSeconds);

void TriggerRadioError(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName);
void TriggerRadioErrorAsync(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName);

#endif // RRIL_RILDMAIN_H
