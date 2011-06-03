////////////////////////////////////////////////////////////////////////////
// globals.cpp
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines some global variables.
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
//  May  8/08  FP       1.00  Factored out from rilhand.cpp.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "globals.h"

///////////////////////////////////////////////////////////////////////////////
// Timeouts (in milliseconds)

const UINT32 TIMEOUT_INITIALIZATION_COMMAND (10000);
const UINT32 TIMEOUT_CMD_NOOP               ( 5000);
const UINT32 TIMEOUT_CMD_ONLINE             (10000);
const UINT32 TIMEOUT_API_DEFAULT            (10000);
const UINT32 TIMEOUT_DTRDROP                ( 5000);
const UINT32 TIMEOUT_WAITFORINIT            (20000);
const UINT32 DEFAULT_CMD_RETRIES            (    2);
const UINT32 TIMEOUT_CHNL_INIT              (10000);

///////////////////////////////////////////////////////////////////////////////
UINT32 g_TimeoutCmdNoOp(TIMEOUT_CMD_NOOP);
UINT32 g_TimeoutCmdOnline(TIMEOUT_CMD_ONLINE);
UINT32 g_TimeoutAPIDefault(TIMEOUT_API_DEFAULT);
UINT32 g_TimeoutDTRDrop(TIMEOUT_DTRDROP);
UINT32 g_TimeoutWaitForInit(TIMEOUT_WAITFORINIT);
UINT32 g_DefaultCmdRetries(DEFAULT_CMD_RETRIES);
UINT32 g_TimeoutCmdInit(TIMEOUT_INITIALIZATION_COMMAND);

///////////////////////////////////////////////////////////////////////////////
char g_cTerminator = '\r';
char g_szNewLine[3] = "\r\n";

///////////////////////////////////////////////////////////////////////////////
BOOL g_fReadyForSTKNotifications(0);

///////////////////////////////////////////////////////////////////////////////
ACCESS_TECHNOLOGY g_uiAccessTechnology = ACT_UNKNOWN;

///////////////////////////////////////////////////////////////////////////////
// This global class instance tracks the radio state and handles notifications
CRadioState g_RadioState;

///////////////////////////////////////////////////////////////////////////////
//  Event pointer used for getting registration status during SIMReadyThread().
CEvent * g_pSIMReadyRegEvent = NULL;
