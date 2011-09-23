////////////////////////////////////////////////////////////////////////////
// reset.h
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

#ifndef RRIL_RESET_H
#define RRIL_RESET_H


#if defined(RESET_MGMT)
#include "../../../IFX-modem/stmd.h"
#endif // RESET_MGMT

enum eRadioError
{
    eRadioError_ForceShutdown,      //  Critical error occured
    eRadioError_RequestCleanup,     //  General request clean up
    eRadioError_LowMemory,          //  Couldn't allocate memory
    eRadioError_ChannelDead,        //  Modem non-responsive
    eRadioError_InitFailure,        //  AT command init sequence failed
    eRadioError_OpenPortFailure,    //  Couldn't open the tty ports successfully

#if !defined(RESET_MGMT)
    eRadioError_ModemInitiatedCrash,    //  Detected modem initiated reset
#endif // RESET_MGMT
};

void ModemResetUpdate();
extern BOOL g_bSpoofCommands;


#if defined(RESET_MGMT)

void do_request_clean_up(eRadioError eError, UINT32 uiLineNum, const BYTE* lpszFileName);
extern BOOL CreateModemWatchdogThread();

#else // RESET_MGMT

extern BOOL g_bIsModemDead;
extern BOOL  g_bIsTriggerRadioError;
extern BOOL CreateWatchdogThread();

void TriggerRadioError(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName);
void TriggerRadioErrorAsync(eRadioError eRadioErrorVal, UINT32 uiLineNum, const BYTE* lpszFileName);

#endif // RESET_MGMT


#endif // RRIL_RESET_H

