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
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include "types.h"

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_RAW API
 */

//  The first byte of the byte[] is the command.  The data follows.

///////////////////////////////////////////////////////////////////////////////
typedef struct TAG_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
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

typedef struct TAG_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
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

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_GAN_RIL
{
    unsigned char bCommand; //  Command ID
} sOEM_HOOK_RAW_GAN_RIL;

//
//  RIL_OEM_HOOK_RAW_GAN_RIL
//  Command ID = 0x01
//
//  This is reserved for future implementation.
//
const BYTE RIL_OEM_HOOK_RAW_GAN_RIL = 0x01;

///////////////////////////////////////////////////////////////////////////////

#if defined(M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED)

typedef struct TAG_OEM_HOOK_RAW_SET_ACTIVE_SIM
{
    unsigned char bCommand; //  Command ID
    int sim_id;
} sOEM_HOOK_RAW_SET_ACTIVE_SIM;

//
//  RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM
//  Command ID = 0xD0
//
//  This command selects which SIM is active between two inserted cards.
//  To be mapped to:
//      AT+XSIM=<sim_id>
//
//  "data" = sOEM_HOOK_RAW_SET_ACTIVE_SIM
//  "response" = NULL
//
const BYTE RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM = 0xD0;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_GET_ACTIVE_SIM
{
    unsigned char bCommand; //  Command ID
} sOEM_HOOK_RAW_GET_ACTIVE_SIM;

//
//  RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM
//  Command ID = 0xD1
//
//  This command gets which SIM is active between two inserted cards.
//  To be mapped to:
//      AT+XSIM=?
//
//  "data" = sOEM_HOOK_RAW_GET_ACTIVE_SIM
//  "response" = int sim_id
//
const BYTE RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM = 0xD1;

#endif // M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED

/***********************************************************************/


