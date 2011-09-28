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

const UINT32 TIMEOUT_INITIALIZATION_COMMAND = 5000;
const UINT32 TIMEOUT_API_DEFAULT            = 10000;
const UINT32 TIMEOUT_WAITFORINIT            = 10000;
const UINT32 TIMEOUT_THRESHOLDFORRETRY      = 10000;


///////////////////////////////////////////////////////////////////////////////
UINT32 g_TimeoutCmdInit = TIMEOUT_INITIALIZATION_COMMAND;
UINT32 g_TimeoutAPIDefault = TIMEOUT_API_DEFAULT;
UINT32 g_TimeoutWaitForInit = TIMEOUT_WAITFORINIT;
UINT32 g_TimeoutThresholdForRetry = TIMEOUT_THRESHOLDFORRETRY;

///////////////////////////////////////////////////////////////////////////////
char g_cTerminator = '\r';
char g_szNewLine[3] = "\r\n";

///////////////////////////////////////////////////////////////////////////////
ACCESS_TECHNOLOGY g_uiAccessTechnology = ACT_UNKNOWN;

///////////////////////////////////////////////////////////////////////////////
// This global class instance tracks the radio state and handles notifications
CRadioState g_RadioState;

