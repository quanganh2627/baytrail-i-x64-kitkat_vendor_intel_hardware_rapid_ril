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

#pragma pack(1)

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_RAW API
 */

//  The first int (4 bytes) of the byte[] is the command ID.  The data follows.
//
// The command id allocation is the following:
//      0x00000001 -> 0x0000009F : Product Specific
//      0x000000A0 -> 0x000000CF : Platform Requests
//      0x000000D0 -> 0x000000FF : Platform Unsolicited
//

///////////////////////////////////////////////////////////////////////////////
typedef struct TAG_OEM_HOOK_RAW_GAN_RIL
{
    int nCommand; //  Command ID
} sOEM_HOOK_RAW_GAN_RIL;

//
//  RIL_OEM_HOOK_RAW_GAN_RIL
//  Command ID = 0x00000001
//
//  This is reserved for future implementation.
//
const int RIL_OEM_HOOK_RAW_GAN_RIL = 0x00000001;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
{
    int nCommandID;  //  Command ID
} sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY;

//
//  RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
//  Command ID = 0x000000A0
//
//  This command sends AT+XFDOR to the modem.
//
//  "data" = sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY = 0x000000A0;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
{
    int nCommandID;  //  Command ID
    int nTimerValue; // int from 0-120
} sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER;

//
//  RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
//  Command ID = 0x000000A1
//
//  This command sends AT+XFDORT to the modem.
//
//  "data" = sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER = 0x000000A1;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_THERMAL_GET_SENSOR
{
    int nCommand; //  Command ID
    int nSensorId; // sensor id
} sOEM_HOOK_RAW_THERMAL_GET_SENSOR;

//
//  TAG_OEM_HOOK_RAW_THERMAL_GET_SENSOR
//  Command ID = 0x000000A2
//
//  "data" = sOEM_HOOK_RAW_GET_SENSOR_VALUE
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_THERMAL_GET_SENSOR = 0x000000A2;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_SET_SENSOR_THRESHOLD
{
    int nCommand; //  Command ID
    int nEnable; // enable=1 or disable=0
    int nSensorId; // sensor id
    int nMinThreshold; // min Threshold
    int nMaxThreshold; // max Threshold
} sOEM_HOOK_RAW_THERMAL_SET_THRESHOLD;

//
//  TAG_OEM_HOOK_RAW_THERMAL_SET_THRESHOLD
//  Command ID = 0x000000A3
//
//  "data" = sOEM_HOOK_RAW_THERMAL_SET_THRESHOLD
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_THERMAL_SET_THRESHOLD = 0x000000A3;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
{
    int nCommand; //  Command ID
    int nEnable; // enable=1 or disable=0
    int nDelayTimer; // int from 0-60
} sOEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY;

//
//  TAG_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
//  Command ID = 0x000000A4
//
//  "data" = sOEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
//  "response" = NULL
//
const int OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY = 0x000000A4;

///////////////////////////////////////////////////////////////////////////////

#if defined(M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED)

typedef struct TAG_OEM_HOOK_RAW_SET_ACTIVE_SIM
{
    int nCommand; //  Command ID
    int sim_id;
} sOEM_HOOK_RAW_SET_ACTIVE_SIM;

//
//  RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM
//  Command ID = 0x000000B0
//
//  This command selects which SIM is active between two inserted cards.
//  To be mapped to:
//      AT@nvm:fix_uicc.ext_mux_misc_config=<sim_id>
//      AT@nvm:store_nvm_sync(fix_uicc)
//  In addition, this command should trigger a modem warm reset after
//  AT@nvm command has been sent.
//
//  Preconditions:
//      The modem has been previously put in flight mode by Android Framework
//      (Radio Off).
//  Post Conditions:
//      The modem is started in flight mode; Android framework will trigger
//      exit from Flight Mode.
//
//  "data" = sOEM_HOOK_RAW_SET_ACTIVE_SIM
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_SET_ACTIVE_SIM = 0x000000B0;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_GET_ACTIVE_SIM
{
    int nCommand; //  Command ID
} sOEM_HOOK_RAW_GET_ACTIVE_SIM;

//
//  RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM
//  Command ID = 0x000000B1
//
//  This command gets which SIM is active between two inserted cards.
//  To be mapped to:
//      AT@nvm:fix_uicc.ext_mux_misc_config?
//
//  Response:
//      <sim_id>
//      OK
//
//  "data" = sOEM_HOOK_RAW_GET_ACTIVE_SIM
//  "response" = int sim_id
//
const int RIL_OEM_HOOK_RAW_GET_ACTIVE_SIM = 0x000000B1;

#endif // M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
{
    int nCommand; //  Command ID
    int nResultCode; // result code
    int nSensorId; // sensor id
    int nTemp; // temperature
} sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND;

//
//  TAG_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  Command ID = 0x000000D0
//
//  "data" = sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND = 0x000000D0;


#pragma pack()

/***********************************************************************/


