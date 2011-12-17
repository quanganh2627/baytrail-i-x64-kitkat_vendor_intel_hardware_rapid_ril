////////////////////////////////////////////////////////////////////////////
// globals.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Also defines RIL framework constants and structures.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_GLOBALS_H
#define RRIL_GLOBALS_H

#include "rril.h"
#include "sync_ops.h"
#include "radio_state.h"
#include "nd_structs.h"

///////////////////////////////////////////////////////////////////////////////
// Timeouts (in milliseconds)

// Default API execution time (in msec)
extern const UINT32 TIMEOUT_INITIALIZATION_COMMAND;
extern const UINT32 TIMEOUT_API_DEFAULT;
extern const UINT32 TIMEOUT_WAITFORINIT;

extern UINT32 g_TimeoutCmdInit;
extern UINT32 g_TimeoutAPIDefault;
extern UINT32 g_TimeoutWaitForInit;
extern UINT32 g_TimeoutThresholdForRetry;

///////////////////////////////////////////////////////////////////////////////
extern char g_cTerminator;
extern char g_szNewLine[3];

///////////////////////////////////////////////////////////////////////////////
// This global class instance tracks the radio state and handles notifications
extern CRadioState g_RadioState;

///////////////////////////////////////////////////////////////////////////////
// This global flag is used to cancel the pending chld requests in ril when
// the call is disconnected.
extern bool g_clearPendingChlds;

#endif // RRIL_GLOBALS_H
