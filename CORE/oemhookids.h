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
 * via the RIL_REQUEST_OEM_HOOK_RAW, RIL_REQUEST_OEM_HOOK_STRINGS APIs.
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

//
//  RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR
//  Command ID = 0x000000A2
//
// "data" - Sensor Id
//              “0” = RF temperature sensor.
//              “1” = Baseband chip temperature sensor.
//              “3” = PCB temperature sensor.
// "response" - String containg the temperatures separated by space
//              "Filtered temperature Raw Temperature". Both temperature
//              are formated as a 4 digits number: "2300" = 23.00 celcius
//              degrees  (two digits after dot).
//
const int RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR = 0x000000A2;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD
//  Command ID = 0x000000A3
//
// "data" - String containing the following values separated by a space
//          boolean activate
//                    true - Activates the threshold notification
//                    false - Deactivates the threshold notification
//          int sensorId
//                    “0” = RF temperature sensor.
//                    “1” = Baseband chip temperature sensor.
//                    “3” = PCB temperature sensor.
//          int minTemperature
//                    Temperature formated as a 4 digit number.
//                    "2300" = 23.00 celcius degrees  (two digits after dot)
//          int maxTemperature: Same as minTemperature
//
//  "response" = NULL
//
const int RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD = 0x000000A3;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
{
    int nCommand; //  Command ID
    int nEnable; // enable=1 or disable=0
    int nDelayTimer; // int from 0-60
} sOEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY;

//
//  RIL_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
//  Command ID = 0x000000A4
//
//  "data" = sOEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_SET_MODEM_AUTO_FAST_DORMANCY = 0x000000A4;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_GET_ATR
//  Command ID = 0x000000A5
//
//  This command sends AT+XGATR to the modem.
//
//  "data" = NULL
//  "response" = Answer to Reset
//
const int RIL_OEM_HOOK_STRING_GET_ATR = 0x000000A5;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV
//  Command ID = 0x000000A6
//
//  This command sends AT+CGED to the modem.
//
//  "data" = NULL
//  "response" = string containing dump of GPRS cell environment
//
const int RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV = 0x000000A6;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND
//  Command ID = 0x000000A7
//
//  This command sends AT+XCGEDPAGE to the modem.
//
//  "data" = NULL
//  "response" = string containing dump of the cell environment debug screen
//
const int RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND = 0x000000A7;

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

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_SWAP_PS
//  Command ID = 0x000000B2
//
//  This command sends AT+XRAT=8 to the modem.
//
//  "data" = NULL
//  "response" = Error code returned by AT command
//
const int RIL_OEM_HOOK_STRING_SWAP_PS = 0x000000B2;

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
//  RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  Command ID = 0x000000D0
//
//  "data" = sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND = 0x000000D0;

///////////////////////////////////////////////////////////////////////////////

#if defined(M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED)

//
//  OEM_HOOK_RAW_UNSOL_FAST_OOS_IND
//  Command ID = 0x000000D1
//
//  "data" is a byte[] containing the command id
//  data[0] = command id
//
const int RIL_OEM_HOOK_RAW_UNSOL_FAST_OOS_IND = 0x000000D1;

//
//  OEM_HOOK_RAW_UNSOL_IN_SERVICE_IND
//  Command ID = 0x000000D2
//
//  "data" is a byte[] containing the command id
//  data[0] = command id
//
const int RIL_OEM_HOOK_RAW_UNSOL_IN_SERVICE_IND = 0x000000D2;

///////////////////////////////////////////////////////////////////////////////

#endif // M2_DUALSIM_1S1S_CMDS_FEATURE_ENABLED


#pragma pack()

/***********************************************************************/


