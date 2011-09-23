////////////////////////////////////////////////////////////////////////////
// oemhookids.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    List of enums for specific OEM requests via API
//    RIL_REQUEST_OEM_HOOK_STRINGS API.
//
// Author:  Francesc Vilarino Guell
// Created: 2009-05-29
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date           Who    Ver   Description
//  -----------  -------  ----  -----------------------------------------------
//  March 29/09    FV     1.00  Initial implementation.
//
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include "types.h"

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_RAW API
 */

//  The first byte of the byte[] is the command.  The data follows.

///////////////////////////////////////////////////////////////////////////////
typedef struct _TAG_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
{
    unsigned char bCommand;  //  Command ID
} sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY;

//
//  RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
//  Command ID = 0xBB
//
//  This command sends AT+XFDOR to the modem.
//
//  "data" = sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
//  "response" = NULL
//
const BYTE RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY = 0xBB;

///////////////////////////////////////////////////////////////////////////////

typedef struct _TAG_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
{
    unsigned char bCommand;  //  Command ID
    int nTimerValue; // int from 0-120
} sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER;

//
//  RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
//  Command ID = 0xCC
//
//  This command sends AT+XFDORT to the modem.
//
//  "data" = sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
//  "response" = NULL
//
const BYTE RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER = 0xCC;

#if defined(M2_FEATURE_ENABLED)
///////////////////////////////////////////////////////////////////////////////

typedef struct _TAG_OEM_HOOK_RAW_SET_DATACHANNEL
{
    unsigned char bCommand;  //  Command ID
} sOEM_HOOK_RAW_SET_DATACHANNEL;

//
//  RIL_OEM_HOOK_RAW_SET_DATACHANNEL
//  Command ID = 0xDD
//
//  This command sends AT+XDATACHANNEL=1,0,"/mux/2","/mux/5",0 to the modem.
//
//  "response" = NULL
//
const BYTE RIL_OEM_HOOK_RAW_SET_DATACHANNEL = 0xDD;
#endif //M2_FEATURE_ENABLED


/***********************************************************************/


