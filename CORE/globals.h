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
// Author:  Dennis Peter
// Created: 2007-07-12
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//  May  9/08  FP       1.00  Factored out from rilhand.h.
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

extern const UINT32 TIMEOUT_CMD_NOOP;
extern const UINT32 TIMEOUT_CMD_ONLINE;
extern const UINT32 TIMEOUT_API_DEFAULT;
extern const UINT32 TIMEOUT_DTRDROP;
extern const UINT32 TIMEOUT_WAITFORINIT;
extern const UINT32 DEFAULT_CMD_RETRIES;
extern const UINT32 TIMEOUT_CHNL_INIT;

extern UINT32 g_TimeoutCmdInit;
extern UINT32 g_TimeoutCmdNoOp;
extern UINT32 g_TimeoutCmdOnline;
extern UINT32 g_TimeoutAPIDefault;
extern UINT32 g_TimeoutDTRDrop;
extern UINT32 g_TimeoutWaitForInit;
extern UINT32 g_DefaultCmdRetries;

///////////////////////////////////////////////////////////////////////////////
extern char g_cTerminator;
extern char g_szNewLine[3];

///////////////////////////////////////////////////////////////////////////////
extern BOOL g_fReadyForSTKNotifications;

///////////////////////////////////////////////////////////////////////////////
extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

///////////////////////////////////////////////////////////////////////////////
// This global class instance tracks the radio state and handles notifications
extern CRadioState g_RadioState;

///////////////////////////////////////////////////////////////////////////////
//  Event handle used for getting registration status during SIMReadyThread().
extern CEvent * g_pSIMReadyRegEvent;

#endif // RRIL_GLOBALS_H
